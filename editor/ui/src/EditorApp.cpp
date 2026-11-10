#include "agr/editor/ui/EditorApp.hpp"

#include <SDL3/SDL.h>
#include <imgui_impl_sdl3.h>
#include <imgui_internal.h>
#include <imgui.h>
#include <imgui_stdlib.h>
#include <ImGuizmo.h>

#include <cmath>
#include <filesystem>
#include <fstream>
#include <sstream>

#include "agr/editor/SceneTemplates.hpp"
#include "agr/runtime/Engine.hpp"
#include "agr/runtime/Events.hpp"
#include "agr/render/ModelLibrary.hpp"
#include "agr/physics3d/Physics3DPlugin.hpp"
#include "agr/script/ScriptPlugin.hpp"
#include "agr/animation/AnimationPlugin.hpp"
#include "agr/audio/AudioPlugin.hpp"
#include "agr/ui/UIPlugin.hpp"
#include "agr/ui/UIComponents.hpp"
#include "agr/twod/SpriteAnimator.hpp"

namespace agr::editor {

namespace fs = std::filesystem;

namespace {

std::string readText(const fs::path& p) {
    std::ifstream in(p, std::ios::binary);
    std::ostringstream ss;
    ss << in.rdbuf();
    return ss.str();
}

void writeText(const fs::path& p, const std::string& text) {
    std::error_code ec;
    fs::create_directories(p.parent_path(), ec);
    std::ofstream(p, std::ios::binary) << text;
}

std::string defaultProjectsFolder() {
    const char* docs = SDL_GetUserFolder(SDL_FOLDER_DOCUMENTS);
    const fs::path base = docs ? fs::path(docs) : fs::current_path();
    return (base / "AGR Projects").string();
}

const char* modeLabel(ProjectMode m) {
    switch (m) {
        case ProjectMode::Mode2D: return "2D";
        case ProjectMode::Mode25D: return "2.5D";
        case ProjectMode::Mode3D: return "3D";
    }
    return "";
}

const char* modeDescription(ProjectMode m) {
    switch (m) {
        case ProjectMode::Mode2D: return "Sprites, caméra orthographique.\nPlateforme, puzzle, cartes, idle...";
        case ProjectMode::Mode25D: return "Monde 3D vu de haut, sprites.\nStratégie, tower defense, RPG...";
        case ProjectMode::Mode3D: return "Maillages, lumières, caméra libre.\nAction, course, simulation...";
    }
    return "";
}

} // namespace

EditorApp::EditorApp(SdlPlatform* platform, EditorOptions options) : m_platform(platform), m_options(std::move(options)) {}
EditorApp::~EditorApp() = default;

// ================================================================== startup
Status EditorApp::onStart(Engine& engine) {
    m_engine = &engine;
    auto* render = engine.service<RenderPlugin>();
    if (!m_platform || !render || !render->device())
        return makeError(Error::Code::Unsupported, "the editor needs a window and a GPU device");
    render->setAutoRender(false);  // the editor drives frames: scene view target, then UI
    render->setHotReload(true);

    m_ctx = std::make_unique<EditorContext>(engine);
    m_shared = std::make_unique<EditorShared>(EditorShared{engine, *m_ctx});
    m_shared->platform = m_platform;
    m_shared->render = render;
    m_shared->twod = engine.service<TwoDPlugin>();
    m_shared->audio = engine.service<AudioPlugin>();
    m_shared->ui = engine.service<UIPlugin>();
    m_shared->animation = engine.service<AnimationPlugin>();
    if (m_shared->audio) m_shared->audio->setRunning(false);  // sources play in play mode only
    if (m_shared->ui) m_shared->ui->setRunning(false);
    if (m_shared->animation) m_shared->animation->setRunning(false);
    if (m_shared->twod) m_shared->twod->setPreviewWhilePaused(true);  // particles animate in edit mode
    if (auto* scripts = engine.service<ScriptPlugin>()) {
        scripts->setRunning(false);  // scripts run in play mode only
        scripts->setHotReload(true);
    }

    // Settings live in the per-user preference folder.
    if (char* pref = SDL_GetPrefPath("Superessence Studio", "AGR Editor")) {
        m_settingsDir = pref;
        SDL_free(pref);
    } else {
        m_settingsDir = (fs::temp_directory_path() / "agr_editor").string();
    }
    if (!m_options.smokeTestDir.empty()) m_settingsDir = (fs::path(m_options.smokeTestDir) / "settings").string() + "/";
    m_iniPath = m_settingsDir + "imgui.ini";
    loadSettings();
    m_wizard.location = defaultProjectsFolder();

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable | ImGuiConfigFlags_NavEnableKeyboard;
    io.IniFilename = m_iniPath.c_str();
    m_layoutBuilt = fs::exists(m_iniPath);
    ImGui_ImplSDL3_InitForOther(m_platform->window());
    m_platform->setRawEventHook([](const void* e) { ImGui_ImplSDL3_ProcessEvent(static_cast<const SDL_Event*>(e)); });
    const float dpi = SDL_GetWindowDisplayScale(m_platform->window());
    m_shared->fonts = applyAgrTheme(m_options.resourcesDir + "/fonts", dpi > 0.0f ? dpi : 1.0f);

    m_imgui = std::make_unique<ImGuiRenderer>(*render->device());
    if (Status s = m_imgui->initialize(); !s) return s;

    engine.events().subscribe<FileDroppedEvent>([this](const FileDroppedEvent& e) {
        if (m_ctx->hasProject()) m_assets.importFiles(*m_shared, {e.path});
    });
    engine.setCloseRequestHandler([this] {
        if (!m_ctx->hasProject() || !m_ctx->dirty()) return true;
        m_shared->runGuarded([this] { m_engine->requestQuit(); });
        return false;  // the "unsaved changes" dialog decides
    });

    if (!m_options.smokeTestDir.empty()) {
        ProjectSettings settings;
        settings.name = "Smoke Test";
        auto project = Project::create(fs::path(m_options.smokeTestDir) / "SmokeProject", settings, SceneTemplate::Empty3D,
                                       m_options.resourcesDir + "/templates/assets");
        if (!project) return project.error();
        openProject(project.value().root().string());
    } else if (!m_options.projectPath.empty()) {
        openProject(m_options.projectPath);
    }
    AGR_LOG_INFO("Editor", "AGR Editor ready ({} UI on {})", ImGui::GetVersion(), rhi::enumName(render->device()->info().backend));
    return {};
}

void EditorApp::loadSettings() {
    m_recent = RecentProjects::fromJson(readText(fs::path(m_settingsDir) / "editor_settings.json"));
}

void EditorApp::saveSettings() { writeText(fs::path(m_settingsDir) / "editor_settings.json", m_recent.toJson()); }

void EditorApp::openProject(const std::string& path) {
    auto project = Project::open(path);
    if (!project) {
        m_shared->notify("Projet introuvable : " + project.error().message);
        m_recent.remove(path);
        saveSettings();
        return;
    }
    m_shared->releaseThumbnails();
    m_shared->assetFolder.clear();
    if (Status s = m_ctx->openProject(std::move(project.value())); !s) {
        m_shared->notify("Ouverture impossible : " + s.error().message);
        return;
    }
    m_recent.touch(fs::absolute(path).string());
    saveSettings();
    m_shared->notify("Projet ouvert : " + m_ctx->project().settings().name);
}

void EditorApp::createProject() {
    Wizard& w = m_wizard;
    w.error.clear();
    if (w.name.empty() || w.name.find_first_of("/\\:*?\"<>|") != std::string::npos) {
        w.error = "Nom invalide (caractères interdits : / \\ : * ? \" < > |).";
        return;
    }
    if (!w.desktop && !w.android) {
        w.error = "Choisissez au moins une plateforme cible.";
        return;
    }
    ProjectSettings settings;
    settings.name = w.name;
    settings.mode = w.mode;
    settings.targetDesktop = w.desktop;
    settings.targetAndroid = w.android;
    const fs::path folder = fs::path(w.location) / w.name;
    auto project = Project::create(folder, settings, w.tmpl, m_options.resourcesDir + "/templates/assets");
    if (!project) {
        w.error = project.error().message;
        return;
    }
    openProject(folder.string());
}

void EditorApp::onShutdown(Engine&) {
    if (!ImGui::GetCurrentContext()) return;
    m_viewport.release(*m_shared);
    m_materialPreview.release(*m_shared);
    m_shared->releaseThumbnails();
    m_imgui->shutdown();
    m_imgui.reset();  // GPU objects must go while the device still exists (plugins unload after onShutdown)
    ImGui_ImplSDL3_Shutdown();
    if (m_platform) m_platform->setRawEventHook(nullptr);
    ImGui::DestroyContext();
    saveSettings();
}

// =================================================================== frame
void EditorApp::onUpdate(Engine&, const FrameTime& time) {
    ++m_frame;
    if (auto* scripts = m_engine->service<ScriptPlugin>()) {  // scripts follow the play state
        const PlayState ps = m_ctx->playState();
        if (scripts->running() != (ps != PlayState::Edit)) scripts->setRunning(ps != PlayState::Edit);
        scripts->setPaused(ps == PlayState::Paused);
    }
    {  // audio, animation and UI input follow the play state too
        const PlayState ps = m_ctx->playState();
        const bool playing = ps != PlayState::Edit;
        if (auto* a = m_engine->service<AudioPlugin>()) {
            a->setRunning(playing);
            a->setPaused(ps == PlayState::Paused);
        }
        if (auto* an = m_engine->service<AnimationPlugin>()) an->setRunning(playing);  // paused: time scale 0 freezes it
        if (auto* ui = m_engine->service<UIPlugin>()) ui->setRunning(ps == PlayState::Playing && m_viewport.gameView);
    }
    if (m_closeProjectRequested) {
        m_closeProjectRequested = false;
        m_viewport.release(*m_shared);
        m_materialPreview.release(*m_shared);
        m_shared->releaseThumbnails();
        RenderPlugin* render = m_shared->render;
        const EditorFonts fonts = m_shared->fonts;
        m_ctx = std::make_unique<EditorContext>(*m_engine);
        m_shared = std::make_unique<EditorShared>(EditorShared{*m_engine, *m_ctx});
        m_shared->platform = m_platform;
        m_shared->render = render;
        m_shared->twod = m_engine->service<TwoDPlugin>();
        m_shared->audio = m_engine->service<AudioPlugin>();
        m_shared->ui = m_engine->service<UIPlugin>();
        m_shared->animation = m_engine->service<AnimationPlugin>();
        m_shared->fonts = fonts;
    }
    m_viewport.beginFrame(*m_shared);
    ImGui_ImplSDL3_NewFrame();
    ImGui::NewFrame();
    ImGuizmo::BeginFrame();
    {  // native folder dialog results (may arrive from another thread)
        std::lock_guard lock(m_dialogMutex);
        if (!m_dialogFolder.empty()) {
            if (m_dialogTarget == 1) m_wizard.location = m_dialogFolder;
            else if (m_dialogTarget == 2) {
                const std::string path = m_dialogFolder;
                m_shared->runGuarded([this, path] { openProject(path); });
            }
            m_dialogFolder.clear();
        }
    }
    if (m_ctx->hasProject()) {
        m_ctx->update();
        drawEditor(time);
    } else {
        drawHub();
    }
    drawModals();
    if (m_showDemo) ImGui::ShowDemoWindow(&m_showDemo);
    if (!m_options.smokeTestDir.empty()) smokeTest(time);
    if (!m_options.hubScreenshot.empty() && m_frame == 8) captureNextFrame(m_options.hubScreenshot);
    if (!m_options.hubScreenshot.empty() && m_frame == 10) m_engine->requestQuit();
    ImGui::Render();
    updateWindowTitle();
}

void EditorApp::onRender(Engine& engine, const FrameTime&) {
    rhi::Device* device = m_shared->device();
    if (!device) return;
    const math::Vec2 fb = engine.platform().framebufferSize();
    const u32 w = static_cast<u32>(fb.x), h = static_cast<u32>(fb.y);
    if (w == 0 || h == 0) return;
    device->beginFrame(w, h);
    if (m_ctx->hasProject()) {
        m_materialPreview.render(*m_shared);  // before the scene view: the stats panel shows the scene's frame
        m_viewport.renderScene(*m_shared);
    }
    m_imgui->render(ImGui::GetDrawData(), true);
    device->endFrame();
    if (!m_capturePath.empty()) {
        std::vector<u8> pixels;
        if (device->readBackbuffer(0, 0, w, h, pixels)) {
            if (auto png = encodePng(w, h, pixels)) {
                writeText(m_capturePath, std::string(png.value().begin(), png.value().end()));
                AGR_LOG_INFO("Editor", "screenshot saved to {}", m_capturePath);
            }
        }
        m_capturePath.clear();
    }
}

void EditorApp::updateWindowTitle() {
    std::string title = "AGR ENGINE";
    if (m_ctx->hasProject()) {
        title += " — " + m_ctx->project().settings().name + " — ";
        title += m_ctx->scenePath().empty() ? std::string("(scène non enregistrée)") : fs::path(m_ctx->scenePath()).stem().string();
        if (m_ctx->dirty()) title += " *";
    }
    if (title != m_title && m_platform) {
        m_title = title;
        m_platform->setTitle(title);
    }
}

// ============================================================= project hub
void EditorApp::drawHub() {
    const ImGuiViewport* vp = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(vp->WorkPos);
    ImGui::SetNextWindowSize(vp->WorkSize);
    ImGui::PushStyleColor(ImGuiCol_WindowBg, palette::toVec4(palette::kGraphite950));
    ImGui::Begin("##hub", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoBringToFrontOnFocus);
    ImGui::PopStyleColor();
    const float pad = 40.0f;
    ImGui::SetCursorPos({pad, pad});
    ImDrawList* dl = ImGui::GetWindowDrawList();
    const ImVec2 logoPos = ImGui::GetCursorScreenPos();
    icons::drawAgrMark(dl, logoPos, 64.0f);
    ImGui::SetCursorPos({pad + 80.0f, pad + 4.0f});
    ImGui::PushFont(m_shared->fonts.semibold, 30.0f);
    ImGui::TextUnformatted("AGR ENGINE");
    ImGui::PopFont();
    ImGui::SetCursorPos({pad + 82.0f, pad + 42.0f});
    ImGui::TextColored(palette::toVec4(palette::kAmber), "BUILD BEYOND LIMITS.");
    ImGui::SameLine();
    ImGui::TextDisabled("  Superessence Studio — version %d.%d.%d", AGR_VERSION_MAJOR, AGR_VERSION_MINOR, AGR_VERSION_PATCH);

    ImGui::SetCursorPos({pad, pad + 100.0f});
    const float columnH = ImGui::GetContentRegionAvail().y - pad;
    // ---- recent projects
    ImGui::BeginChild("##recent", {std::max(320.0f, ImGui::GetContentRegionAvail().x * 0.36f), columnH}, ImGuiChildFlags_Borders);
    ImGui::PushFont(m_shared->fonts.semibold, 0.0f);
    ImGui::TextUnformatted("Projets récents");
    ImGui::PopFont();
    ImGui::Separator();
    if (m_recent.paths.empty()) ImGui::TextDisabled("Aucun projet récent.");
    std::string toOpen, toForget;
    for (const std::string& p : m_recent.paths) {
        ImGui::PushID(p.c_str());
        const std::string name = fs::path(p).filename().string();
        if (ImGui::Selectable("##p", false, ImGuiSelectableFlags_AllowDoubleClick, {0, 40})) toOpen = p;
        const ImVec2 a = ImGui::GetItemRectMin();
        icons::draw(ImGui::GetWindowDrawList(), icons::Kind::Folder, {a.x + 18, a.y + 20}, 18, palette::kAmber);
        ImGui::GetWindowDrawList()->AddText({a.x + 38, a.y + 3}, palette::kText, name.c_str());
        ImGui::GetWindowDrawList()->AddText({a.x + 38, a.y + 21}, palette::kTextMuted, p.c_str());
        if (ImGui::BeginPopupContextItem()) {
            if (ImGui::MenuItem("Retirer de la liste")) toForget = p;
            ImGui::EndPopup();
        }
        ImGui::PopID();
    }
    if (!toOpen.empty()) openProject(toOpen);
    if (!toForget.empty()) {
        m_recent.remove(toForget);
        saveSettings();
    }
    ImGui::Spacing();
    if (ImGui::Button("Ouvrir un projet...", {-FLT_MIN, 0})) {
        m_dialogTarget = 2;
        SDL_ShowOpenFolderDialog(
            [](void* user, const char* const* files, int) {
                auto* app = static_cast<EditorApp*>(user);
                if (files && files[0]) {
                    std::lock_guard lock(app->m_dialogMutex);
                    app->m_dialogFolder = files[0];
                }
            },
            this, m_platform->window(), nullptr, false);
    }
    ImGui::EndChild();

    // ---- new project wizard
    ImGui::SameLine(0, 24.0f);
    ImGui::BeginChild("##wizard", {0, columnH}, ImGuiChildFlags_Borders);
    ImGui::PushFont(m_shared->fonts.semibold, 0.0f);
    ImGui::TextUnformatted("Nouveau projet");
    ImGui::PopFont();
    ImGui::Separator();
    Wizard& w = m_wizard;
    ImGui::TextDisabled("1. Type de jeu");
    const float cardW = (ImGui::GetContentRegionAvail().x - 2 * ImGui::GetStyle().ItemSpacing.x) / 3.0f;
    for (ProjectMode m : {ProjectMode::Mode2D, ProjectMode::Mode25D, ProjectMode::Mode3D}) {
        ImGui::PushID(static_cast<int>(m));
        const bool sel = w.mode == m;
        const ImVec2 p = ImGui::GetCursorScreenPos();
        if (ImGui::InvisibleButton("##card", {cardW, 96})) {
            w.mode = m;
            w.tmpl = defaultTemplateFor(m);
        }
        ImDrawList* cdl = ImGui::GetWindowDrawList();
        cdl->AddRectFilled(p, {p.x + cardW, p.y + 96}, ImGui::IsItemHovered() ? palette::kGraphite700 : palette::kGraphite800, 6.0f);
        if (sel) cdl->AddRect(p, {p.x + cardW, p.y + 96}, palette::kAmber, 6.0f, 2.0f);
        cdl->AddText(m_shared->fonts.semibold, 26.0f, {p.x + 14, p.y + 10}, sel ? palette::kAmber : palette::kText, modeLabel(m));
        cdl->AddText({p.x + 14, p.y + 48}, palette::kTextMuted, modeDescription(m));
        ImGui::PopID();
        ImGui::SameLine();
    }
    ImGui::NewLine();
    ImGui::Spacing();
    ImGui::TextDisabled("2. Modèle");
    for (SceneTemplate t : templatesFor(w.mode))
        if (ImGui::Selectable(templateName(t), w.tmpl == t)) w.tmpl = t;
    ImGui::TextWrapped("%s", templateDescription(w.tmpl));
    ImGui::TextDisabled("Les modèles de jeux complets (plateforme, RPG, tower defense, course...) arrivent en phase 9.");
    ImGui::Spacing();
    ImGui::TextDisabled("3. Nom et emplacement");
    ImGui::SetNextItemWidth(-FLT_MIN);
    ImGui::InputText("##name", &w.name);
    ImGui::SetNextItemWidth(-110.0f);
    ImGui::InputText("##location", &w.location);
    ImGui::SameLine();
    if (ImGui::Button("Parcourir...", {-FLT_MIN, 0})) {
        m_dialogTarget = 1;
        SDL_ShowOpenFolderDialog(
            [](void* user, const char* const* files, int) {
                auto* app = static_cast<EditorApp*>(user);
                if (files && files[0]) {
                    std::lock_guard lock(app->m_dialogMutex);
                    app->m_dialogFolder = files[0];
                }
            },
            this, m_platform->window(), w.location.c_str(), false);
    }
    ImGui::TextDisabled("Dossier du projet : %s", (fs::path(w.location) / w.name).string().c_str());
    ImGui::Spacing();
    ImGui::TextDisabled("4. Plateformes cibles");
    ImGui::Checkbox("Windows / Linux", &w.desktop);
    ImGui::SameLine();
    ImGui::Checkbox("Android", &w.android);
    if (w.android) ImGui::TextColored(palette::toVec4(palette::kWarning), "Enregistré dans le projet ; l'export APK / AAB arrive en phase 8.");
    ImGui::Spacing();
    if (!w.error.empty()) ImGui::TextColored(palette::toVec4(palette::kError), "%s", w.error.c_str());
    ImGui::PushStyleColor(ImGuiCol_Button, palette::toVec4(palette::kAmber));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(1.0f, 0.72f, 0.25f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_Text, palette::toVec4(palette::kGraphite950));
    if (ImGui::Button("Créer et ouvrir", {220, 38})) createProject();
    ImGui::PopStyleColor(3);
    ImGui::EndChild();
    ImGui::End();
}

// ================================================================== editor
void EditorApp::buildDefaultLayout(unsigned int dockspace) {
    ImGui::DockBuilderRemoveNode(dockspace);
    ImGui::DockBuilderAddNode(dockspace, ImGuiDockNodeFlags_DockSpace);
    ImGui::DockBuilderSetNodeSize(dockspace, ImGui::GetMainViewport()->WorkSize);
    ImGuiID center = dockspace;
    const ImGuiID left = ImGui::DockBuilderSplitNode(center, ImGuiDir_Left, 0.18f, nullptr, &center);
    const ImGuiID right = ImGui::DockBuilderSplitNode(center, ImGuiDir_Right, 0.26f, nullptr, &center);
    const ImGuiID bottom = ImGui::DockBuilderSplitNode(center, ImGuiDir_Down, 0.30f, nullptr, &center);
    ImGui::DockBuilderDockWindow("Hiérarchie", left);
    ImGui::DockBuilderDockWindow("Inspecteur", right);
    ImGui::DockBuilderDockWindow("Statistiques", right);
    ImGui::DockBuilderDockWindow("Assets", bottom);
    ImGui::DockBuilderDockWindow("Console", bottom);
    ImGui::DockBuilderDockWindow("Animation", bottom);
    ImGui::DockBuilderDockWindow("Machine à états", bottom);
    ImGui::DockBuilderDockWindow("Mixeur audio", bottom);
    ImGui::DockBuilderDockWindow("Scène", center);
    ImGui::DockBuilderFinish(dockspace);
}

void EditorApp::drawEditor(const FrameTime& time) {
    drawMenuBar();
    const ImGuiViewport* vp = ImGui::GetMainViewport();
    const float toolbarH = ImGui::GetFrameHeight() + 14.0f, statusH = ImGui::GetFrameHeight() + 4.0f;
    drawToolbar(toolbarH);

    ImGui::SetNextWindowPos({vp->WorkPos.x, vp->WorkPos.y + toolbarH});
    ImGui::SetNextWindowSize({vp->WorkSize.x, vp->WorkSize.y - toolbarH - statusH});
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, {0, 0});
    ImGui::Begin("##dockhost", nullptr,
                 ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoBringToFrontOnFocus |
                     ImGuiWindowFlags_NoNavFocus | ImGuiWindowFlags_NoDocking);
    ImGui::PopStyleVar();
    const ImGuiID dockspace = ImGui::GetID("AgrDockspace");
    if (!m_layoutBuilt || m_resetLayout) {
        buildDefaultLayout(dockspace);
        m_layoutBuilt = true;
        m_resetLayout = false;
    }
    ImGui::DockSpace(dockspace);
    ImGui::End();

    m_viewport.draw(*m_shared, time);
    m_hierarchy.draw(*m_shared);
    m_shared->previewMaterial.clear();  // the inspector sets it again while it shows a material
    m_inspector.draw(*m_shared, m_materialPreview);
    m_assets.draw(*m_shared);
    m_console.draw(*m_shared);
    m_stats.draw(*m_shared, time);
    m_mixer.draw(*m_shared, time);
    m_animation.draw(*m_shared, time);
    m_stateMachine.draw(*m_shared);
    drawStatusBar(statusH);
    handleShortcuts();
}

void EditorApp::startPlay() {
    m_animation.endPreview(*m_shared);  // the game starts from the edited values
    if (m_ctx->playState() == PlayState::Edit) {
        m_gameViewBeforePlay = m_viewport.gameView;
        m_viewport.gameView = true;
    }
    m_ctx->play();
}

void EditorApp::stopPlay() {
    if (m_ctx->playState() == PlayState::Edit) return;
    // Scripts get onDestroy while the played scene still exists, before it is restored.
    if (auto* scripts = m_engine->service<ScriptPlugin>()) scripts->setRunning(false);
    if (auto* a = m_engine->service<AudioPlugin>()) a->setRunning(false);
    if (auto* an = m_engine->service<AnimationPlugin>()) an->setRunning(false);
    if (auto* ui = m_engine->service<UIPlugin>()) ui->setRunning(false);
    m_ctx->stop();
    m_viewport.gameView = m_gameViewBeforePlay;
}

void EditorApp::saveOrSaveAs() {
    m_animation.endPreview(*m_shared);  // previewed values are not part of the scene
    if (m_ctx->playState() != PlayState::Edit) {
        m_shared->notify("Arrêtez le jeu avant d'enregistrer.");
        return;
    }
    if (m_ctx->scenePath().empty()) {
        m_saveAsName = m_ctx->scene().name();
        m_shared->openSaveAsPopup = true;
        return;
    }
    if (Status s = m_ctx->saveScene(); !s) m_shared->notify("Enregistrement impossible : " + s.error().message);
    else m_shared->notify("Scène enregistrée : " + m_ctx->scenePath());
}

void EditorApp::drawMenuBar() {
    if (!ImGui::BeginMainMenuBar()) return;
    EditorContext& ctx = *m_ctx;
    if (ImGui::BeginMenu("Fichier")) {
        if (ImGui::BeginMenu("Nouvelle scène")) {
            for (SceneTemplate t : allTemplates())
                if (ImGui::MenuItem(templateName(t))) m_shared->runGuarded([this, t] { m_ctx->newScene(t); });
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Ouvrir une scène")) {
            for (const std::string& scene : ctx.project().listScenes())
                if (ImGui::MenuItem(scene.c_str(), nullptr, scene == ctx.scenePath()))
                    m_shared->runGuarded([this, scene] {
                        if (Status s = m_ctx->loadScene(scene); !s) m_shared->notify("Ouverture impossible : " + s.error().message);
                    });
            ImGui::EndMenu();
        }
        if (ImGui::MenuItem("Enregistrer", "Ctrl+S")) saveOrSaveAs();
        if (ImGui::MenuItem("Enregistrer sous...", "Ctrl+Maj+S")) {
            m_saveAsName = ctx.scene().name();
            m_shared->openSaveAsPopup = true;
        }
        ImGui::Separator();
        // Applied at the start of the next frame: panels still use the context this frame.
        if (ImGui::MenuItem("Fermer le projet")) m_shared->runGuarded([this] { m_closeProjectRequested = true; });
        if (ImGui::MenuItem("Quitter")) m_shared->runGuarded([this] { m_engine->requestQuit(); });
        ImGui::EndMenu();
    }
    if (ImGui::BeginMenu("Édition")) {
        const std::string undoLabel = "Annuler " + ctx.undoStack().undoLabel();
        const std::string redoLabel = "Rétablir " + ctx.undoStack().redoLabel();
        if (ImGui::MenuItem(undoLabel.c_str(), "Ctrl+Z", false, ctx.undoStack().canUndo())) ctx.undo();
        if (ImGui::MenuItem(redoLabel.c_str(), "Ctrl+Y", false, ctx.undoStack().canRedo())) ctx.redo();
        ImGui::Separator();
        const bool any = !ctx.selection().empty();
        if (ImGui::MenuItem("Dupliquer", "Ctrl+D", false, any)) ctx.duplicateSelection();
        if (ImGui::MenuItem("Grouper", "Ctrl+G", false, any)) ctx.groupSelection();
        if (ImGui::MenuItem("Supprimer", "Suppr", false, any)) ctx.deleteSelection();
        if (ImGui::MenuItem("Renommer", "F2", false, any)) m_shared->renameSelection = true;
        if (ImGui::MenuItem("Tout sélectionner", "Ctrl+A")) ctx.selectAll();
        if (ImGui::MenuItem("Cadrer la sélection", "F", false, any)) ctx.focusSelection();
        ImGui::EndMenu();
    }
    if (ImGui::BeginMenu("Créer")) {
        // nullptr label = separator
        static const std::pair<const char*, CreateKind> items[] = {
            {"Entité vide", CreateKind::Empty}, {nullptr, {}}, {"Cube", CreateKind::Cube}, {"Sphère", CreateKind::Sphere},
            {"Plan", CreateKind::Plane}, {"Quad", CreateKind::Quad}, {nullptr, {}}, {"Sprite", CreateKind::Sprite},
            {"Tilemap", CreateKind::Tilemap}, {"Particules 2D", CreateKind::Particles2D}, {"Lumière 2D", CreateKind::Light2D},
            {"Couche de parallaxe", CreateKind::ParallaxLayer}, {nullptr, {}}, {"Caméra", CreateKind::Camera},
            {"Lumière directionnelle", CreateKind::DirectionalLight}, {"Lumière ponctuelle", CreateKind::PointLight},
            {"Projecteur (spot)", CreateKind::SpotLight}, {"Environnement", CreateKind::Environment}, {nullptr, {}},
            {"UI : Canvas", CreateKind::UICanvas}, {"UI : Panneau", CreateKind::UIPanel}, {"UI : Texte", CreateKind::UIText},
            {"UI : Bouton", CreateKind::UIButton}, {"UI : Curseur", CreateKind::UISlider}, {"UI : Colonne", CreateKind::UIColumn},
            {nullptr, {}}, {"Source audio", CreateKind::AudioSource}};
        for (const auto& [label, kind] : items) {
            if (!label) ImGui::Separator();
            else if (ImGui::MenuItem(label)) ctx.create(kind);
        }
        ImGui::EndMenu();
    }
    if (ImGui::BeginMenu("Affichage")) {
        if (ImGui::MenuItem("Réinitialiser la disposition")) m_resetLayout = true;
        ImGui::MenuItem("Vue jeu", nullptr, &m_viewport.gameView);
        ImGui::Separator();
        ImGui::MenuItem("Animation (timeline)", nullptr, &m_shared->showAnimation);
        ImGui::MenuItem("Machine à états", nullptr, &m_shared->showStateMachine);
        ImGui::MenuItem("Mixeur audio", nullptr, &m_shared->showMixer);
        ImGui::Separator();
        ImGui::MenuItem("Grille", nullptr, &m_viewport.showGrid);
        ImGui::MenuItem("Démo Dear ImGui (référence des widgets)", nullptr, &m_showDemo);
        ImGui::EndMenu();
    }
    if (ImGui::BeginMenu("Graphismes")) {
        if (auto* r = m_shared->renderer()) {
            for (int t = 4; t >= 0; --t) {
                const auto tier = static_cast<GraphicsTier>(t);
                const GraphicsProfile p = graphicsProfile(tier);
                if (ImGui::MenuItem(p.name, nullptr, r->profile().tier == tier)) m_shared->render->applyProfile(*m_engine, tier);
            }
            ImGui::Separator();
            if (const rhi::Device* d = m_shared->device()) ImGui::TextDisabled("Backend : %s", rhi::enumName(d->info().backend));
        }
        ImGui::EndMenu();
    }
    if (ImGui::BeginMenu("Aide")) {
        ImGui::TextDisabled("Raccourcis");
        ImGui::BulletText("Clic droit + WASD / ZQSD, E / C : survol de la vue");
        ImGui::BulletText("Alt + clic gauche : orbite   Molette : zoom   Clic molette : panoramique");
        ImGui::BulletText("W / E / R : déplacer / pivoter / échelle   F : cadrer");
        ImGui::BulletText("Ctrl+Z / Ctrl+Y : annuler / rétablir   Ctrl+D : dupliquer   Ctrl+G : grouper   Suppr : supprimer");
        ImGui::BulletText("Ctrl+P : jouer / arrêter   Ctrl+S : enregistrer   F12 : capture de l'éditeur");
        ImGui::Separator();
        ImGui::TextDisabled("Documentation : dossier docs/ du moteur (EDITOR.md, RENDERING.md...)");
        ImGui::EndMenu();
    }
    ImGui::EndMainMenuBar();
}

void EditorApp::drawToolbar(float height) {
    const ImGuiViewport* vp = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(vp->WorkPos);
    ImGui::SetNextWindowSize({vp->WorkSize.x, height});
    ImGui::PushStyleColor(ImGuiCol_WindowBg, palette::toVec4(palette::kGraphite950));
    ImGui::Begin("##toolbar", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoDocking |
                                           ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoSavedSettings);
    ImGui::PopStyleColor();
    icons::drawAgrMark(ImGui::GetWindowDrawList(), {ImGui::GetCursorScreenPos().x, ImGui::GetCursorScreenPos().y - 2}, height - 12.0f);
    ImGui::SetCursorPosX(ImGui::GetCursorPosX() + height);
    ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted(m_ctx->project().settings().name.c_str());
    ImGui::SameLine();
    ImGui::TextDisabled("/ %s%s", m_ctx->scenePath().empty() ? "scène non enregistrée" : m_ctx->scenePath().c_str(),
                        m_ctx->dirty() ? " *" : "");

    const float btn = ImGui::GetFrameHeight();
    ImGui::SameLine(vp->WorkSize.x * 0.5f - btn * 1.5f);
    const PlayState ps = m_ctx->playState();
    if (icons::button("##play", icons::Kind::Play, btn, ps == PlayState::Playing ? palette::kAmber : palette::kText,
                      "Jouer (Ctrl+P)", ps == PlayState::Playing))
        startPlay();
    ImGui::SameLine(0, 2);
    if (icons::button("##pause", icons::Kind::Pause, btn, ps == PlayState::Paused ? palette::kAmber : palette::kText, "Pause",
                      ps == PlayState::Paused))
        m_ctx->pause();
    ImGui::SameLine(0, 2);
    ImGui::BeginDisabled(ps == PlayState::Edit);
    if (icons::button("##stop", icons::Kind::Stop, btn, palette::kText, "Arrêter et restaurer la scène")) stopPlay();
    ImGui::EndDisabled();
    ImGui::End();
}

void EditorApp::drawStatusBar(float height) {
    const ImGuiViewport* vp = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos({vp->WorkPos.x, vp->WorkPos.y + vp->WorkSize.y - height});
    ImGui::SetNextWindowSize({vp->WorkSize.x, height});
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, {10, 2});
    ImGui::PushStyleColor(ImGuiCol_WindowBg, palette::toVec4(palette::kGraphite950));
    ImGui::Begin("##status", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoDocking |
                                          ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoScrollbar);
    ImGui::PopStyleColor();
    ImGui::PopStyleVar();
    ImGui::AlignTextToFramePadding();
    if (!m_shared->status.empty() && ImGui::GetTime() - m_shared->statusTime < 6.0) ImGui::TextUnformatted(m_shared->status.c_str());
    else ImGui::TextDisabled("Prêt");
    char right[200];
    const auto* device = m_shared->device();
    std::snprintf(right, sizeof right, "%s  ·  %s  ·  %.0f FPS  ·  %zu entités", device ? rhi::enumName(device->info().backend) : "-",
                  device ? device->info().renderer.c_str() : "-", m_engine->stats().fps, m_ctx->scene().entityCount());
    ImGui::SameLine(ImGui::GetWindowWidth() - ImGui::CalcTextSize(right).x - 14.0f);
    ImGui::TextDisabled("%s", right);
    ImGui::End();
}

void EditorApp::handleShortcuts() {
    EditorContext& ctx = *m_ctx;
    const ImGuiInputFlags global = ImGuiInputFlags_RouteGlobal;
    if (ImGui::Shortcut(ImGuiMod_Ctrl | ImGuiKey_S, global)) saveOrSaveAs();
    if (ImGui::Shortcut(ImGuiMod_Ctrl | ImGuiMod_Shift | ImGuiKey_S, global)) {
        m_saveAsName = ctx.scene().name();
        m_shared->openSaveAsPopup = true;
    }
    if (ImGui::Shortcut(ImGuiMod_Ctrl | ImGuiKey_Z, global)) ctx.undo();
    if (ImGui::Shortcut(ImGuiMod_Ctrl | ImGuiKey_Y, global) || ImGui::Shortcut(ImGuiMod_Ctrl | ImGuiMod_Shift | ImGuiKey_Z, global)) ctx.redo();
    if (ImGui::Shortcut(ImGuiMod_Ctrl | ImGuiKey_P, global)) {
        if (ctx.playState() == PlayState::Edit) startPlay();
        else stopPlay();
    }
    if (ImGui::Shortcut(ImGuiKey_F12, global)) {
        const auto stamp = static_cast<long long>(SDL_GetTicks());
        captureNextFrame((ctx.project().root() / "screenshots" / ("editor-" + std::to_string(stamp) + ".png")).string());
    }
    if (ImGui::GetIO().WantTextInput) return;  // the keys below are text while typing
    if (ImGui::Shortcut(ImGuiMod_Ctrl | ImGuiKey_D, global)) ctx.duplicateSelection();
    if (ImGui::Shortcut(ImGuiMod_Ctrl | ImGuiKey_G, global)) ctx.groupSelection();
    if (ImGui::Shortcut(ImGuiKey_Delete, global)) ctx.deleteSelection();
    if (ImGui::Shortcut(ImGuiMod_Ctrl | ImGuiKey_A, global)) ctx.selectAll();
    if (ImGui::Shortcut(ImGuiKey_F2, global)) m_shared->renameSelection = true;
}

void EditorApp::drawModals() {
    EditorShared& s = *m_shared;
    if (s.openUnsavedPopup) {
        ImGui::OpenPopup("Modifications non enregistrées");
        s.openUnsavedPopup = false;
    }
    if (ImGui::BeginPopupModal("Modifications non enregistrées", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("La scène « %s » contient des modifications non enregistrées.", m_ctx->scene().name().c_str());
        ImGui::Spacing();
        if (ImGui::Button("Enregistrer", {150, 0})) {
            ImGui::CloseCurrentPopup();
            if (m_ctx->scenePath().empty()) {
                m_runPendingAfterSaveAs = true;
                m_saveAsName = m_ctx->scene().name();
                s.openSaveAsPopup = true;
            } else if (Status st = m_ctx->saveScene(); st) {
                if (s.pendingAction) s.pendingAction();
                s.pendingAction = nullptr;
            } else {
                s.notify("Enregistrement impossible : " + st.error().message);
            }
        }
        ImGui::SameLine();
        if (ImGui::Button("Ne pas enregistrer", {150, 0})) {
            ImGui::CloseCurrentPopup();
            if (s.pendingAction) s.pendingAction();
            s.pendingAction = nullptr;
        }
        ImGui::SameLine();
        if (ImGui::Button("Annuler", {150, 0}) || ImGui::IsKeyPressed(ImGuiKey_Escape)) {
            ImGui::CloseCurrentPopup();
            s.pendingAction = nullptr;
        }
        ImGui::EndPopup();
    }

    if (s.openSaveAsPopup) {
        ImGui::OpenPopup("Enregistrer la scène sous");
        s.openSaveAsPopup = false;
    }
    if (ImGui::BeginPopupModal("Enregistrer la scène sous", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        if (ImGui::IsWindowAppearing()) ImGui::SetKeyboardFocusHere();
        ImGui::SetNextItemWidth(320);
        const bool enter = ImGui::InputText("##name", &m_saveAsName, ImGuiInputTextFlags_EnterReturnsTrue);
        const std::string path = "scenes/" + m_saveAsName + ".agrscene";
        ImGui::TextDisabled("%s", path.c_str());
        const bool valid = !m_saveAsName.empty() && m_saveAsName.find_first_of("/\\:*?\"<>|") == std::string::npos;
        std::error_code ec;
        if (valid && m_ctx->hasProject() && fs::exists(m_ctx->project().root() / path, ec) && path != m_ctx->scenePath())
            ImGui::TextColored(palette::toVec4(palette::kWarning), "Ce fichier existe déjà et sera remplacé.");
        ImGui::BeginDisabled(!valid);
        if (ImGui::Button("Enregistrer", {150, 0}) || (enter && valid)) {
            if (Status st = m_ctx->saveSceneAs(path); !st) {
                s.notify("Enregistrement impossible : " + st.error().message);
            } else {
                s.notify("Scène enregistrée : " + path);
                if (m_runPendingAfterSaveAs && s.pendingAction) s.pendingAction();
                s.pendingAction = nullptr;
            }
            m_runPendingAfterSaveAs = false;
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndDisabled();
        ImGui::SameLine();
        if (ImGui::Button("Annuler", {150, 0}) || ImGui::IsKeyPressed(ImGuiKey_Escape)) {
            m_runPendingAfterSaveAs = false;
            s.pendingAction = nullptr;
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
}

// ============================================================== smoke test
void EditorApp::pushMouse(u32 type, ImVec2 pos) {
    SDL_Event e{};
    e.type = type;
    const SDL_WindowID window = SDL_GetWindowID(m_platform->window());
    if (type == SDL_EVENT_MOUSE_MOTION) {
        e.motion.windowID = window;
        e.motion.x = pos.x;
        e.motion.y = pos.y;
    } else {
        e.button.windowID = window;
        e.button.button = SDL_BUTTON_LEFT;
        e.button.down = type == SDL_EVENT_MOUSE_BUTTON_DOWN;
        e.button.clicks = 1;
        e.button.x = pos.x;
        e.button.y = pos.y;
    }
    SDL_PushEvent(&e);
}

void EditorApp::pushKey(u32 key, u32 scancode, u16 mod, bool down) {
    SDL_Event e{};
    e.type = down ? SDL_EVENT_KEY_DOWN : SDL_EVENT_KEY_UP;
    e.key.windowID = SDL_GetWindowID(m_platform->window());
    e.key.key = key;
    e.key.scancode = static_cast<SDL_Scancode>(scancode);
    e.key.mod = mod;
    e.key.down = down;
    SDL_PushEvent(&e);
}

void EditorApp::check(bool condition, const char* what) {
    if (!condition) {
        m_failures.emplace_back(what);
        AGR_LOG_ERROR("SmokeTest", "FAILED: {}", what);
    }
}

/// Scripted end-to-end session driven through the same calls as the UI, with a
/// screenshot of the real editor window. Used by CI on every graphics backend.
void EditorApp::smokeTest(const FrameTime&) {
    EditorContext& ctx = *m_ctx;
    Scene& scene = ctx.scene();
    static Uuid cubeId, spriteId;
    switch (m_frame) {
        case 3: {
            check(ctx.hasProject(), "project opened");
            const auto cube = ctx.create(CreateKind::Cube);
            scene.registry().get<TransformComponent>(cube).position = {2.0f, 0.5f, 0.0f};
            ctx.rename(cube, "Smoke Cube");
            cubeId = scene.uuidOf(cube);
            check(ctx.addComponent(cube, "Spin") || !ComponentRegistry::instance().find("Spin"), "Spin plugin component added");
            const auto sprite = ctx.createSprite("assets://textures/agr_mark.png", {-2.0f, 1.0f, 0.0f});
            spriteId = scene.uuidOf(sprite);
            break;
        }
        case 6:
            check(ctx.undo(), "undo create sprite");
            check(scene.findByUuid(spriteId).isNull(), "sprite removed by undo");
            check(ctx.redo(), "redo create sprite");
            check(!scene.findByUuid(spriteId).isNull(), "sprite restored by redo");
            ctx.selection().assign({spriteId, cubeId});
            ctx.focusSelection();
            break;
        case 10: startPlay(); break;
        case 40: {
            check(ctx.playState() == PlayState::Playing, "playing");
            const auto cube = scene.findByUuid(cubeId);
            if (ComponentRegistry::instance().find("Spin") && !cube.isNull())
                check(!math::nearlyEqual(scene.registry().get<TransformComponent>(cube).rotation, math::Quat::identity()),
                      "the Spin system ran during play");
            stopPlay();
            check(ctx.playState() == PlayState::Edit, "stopped");
            const auto restored = scene.findByUuid(cubeId);
            check(!restored.isNull() && math::nearlyEqual(scene.registry().get<TransformComponent>(restored).rotation, math::Quat::identity()),
                  "stop restored the edited scene");
            break;
        }
        case 44: check(ctx.saveScene().ok(), "scene saved"); break;
        // ---- Real input path: SDL events -> ImGui -> viewport picking / shortcuts.
        case 46: {
            ctx.selection().clear();
            ImVec2 p;
            const auto cube = scene.findByName("Cube");  // template cube at (0, 0.5, 0)
            check(!cube.isNull() && m_viewport.worldToScreen(*m_shared, {0.0f, 0.5f, 0.0f}, p), "template cube visible in the viewport");
            m_smokeClick = p;
            pushMouse(SDL_EVENT_MOUSE_MOTION, p);
            break;
        }
        case 47: pushMouse(SDL_EVENT_MOUSE_BUTTON_DOWN, m_smokeClick); break;
        case 48: pushMouse(SDL_EVENT_MOUSE_BUTTON_UP, m_smokeClick); break;
        case 50: {
            const auto cube = scene.findByName("Cube");
            check(!cube.isNull() && ctx.selection().primary() == scene.uuidOf(cube), "a click in the viewport selects the cube (picking)");
            m_smokeDeleted = ctx.selection().primary();
            pushKey(SDLK_DELETE, SDL_SCANCODE_DELETE, SDL_KMOD_NONE, true);
            break;
        }
        case 51: pushKey(SDLK_DELETE, SDL_SCANCODE_DELETE, SDL_KMOD_NONE, false); break;
        case 53:
            check(scene.findByUuid(m_smokeDeleted).isNull(), "the Delete key deletes the selection");
            pushKey(SDLK_Z, SDL_SCANCODE_Z, SDL_KMOD_LCTRL, true);
            break;
        case 54: pushKey(SDLK_Z, SDL_SCANCODE_Z, SDL_KMOD_LCTRL, false); break;
        case 56:
            check(!scene.findByUuid(m_smokeDeleted).isNull(), "Ctrl+Z restores the deleted entity");
            captureNextFrame((fs::path(m_options.smokeTestDir) / "editor.png").string());
            break;
        case 60: {
            std::error_code ec;
            check(fs::exists(fs::path(m_options.smokeTestDir) / "editor.png", ec), "editor screenshot written");
            check(!ctx.dirty(), "no unsaved changes after save");
            // ---- 2D: showcase template, tile painting with real mouse events, animation in play mode.
            ctx.newScene(SceneTemplate::Showcase2D);  // replaces the scene: `scene` is stale from here
            Scene& showcase = ctx.scene();
            ctx.camera().mode2D = true;
            ctx.camera().position2D = {0.0f, 0.0f};
            ctx.camera().orthoSize = 6.0f;
            const auto ground = showcase.findByName("Ground");
            check(!ground.isNull() && showcase.registry().has<TilemapComponent>(ground), "showcase tilemap created");
            ctx.selection().set(showcase.uuidOf(ground));
            m_shared->brush.active = true;
            m_shared->brush.tool = TilemapBrush::Tool::Paint;
            m_shared->brush.tile = 18;  // crate
            // Ground at (-20, -5): cell (x, y) is centered on (x - 19.5, y - 4.5).
            check(m_viewport.worldToScreen(*m_shared, {0.5f, 1.5f, 0.0f}, m_smokeClick), "tile cell visible in the 2D view");
            pushMouse(SDL_EVENT_MOUSE_MOTION, m_smokeClick);
            break;
        }
        case 61: pushMouse(SDL_EVENT_MOUSE_BUTTON_DOWN, m_smokeClick); break;
        case 62: {
            ImVec2 p;
            check(m_viewport.worldToScreen(*m_shared, {2.5f, 1.5f, 0.0f}, p), "second tile cell visible");
            pushMouse(SDL_EVENT_MOUSE_MOTION, p);  // drag: Bresenham fills the cell in between
            m_smokeClick = p;
            break;
        }
        case 63: pushMouse(SDL_EVENT_MOUSE_BUTTON_UP, m_smokeClick); break;
        case 65: {
            const auto& tm = scene.registry().get<TilemapComponent>(scene.findByName("Ground"));
            check(tm.tileAt(20, 6) == 18 && tm.tileAt(21, 6) == 18 && tm.tileAt(22, 6) == 18, "a mouse drag paints a line of tiles");
            check(ctx.undoStack().undoLabel() == "Paint Tiles", "one undo entry per paint stroke");
            check(ctx.selection().primary() == scene.uuidOf(scene.findByName("Ground")), "painting does not change the selection");
            const Renderer* renderer = m_shared->renderer();
            check(renderer && renderer->stats().tilesVisible > 100 && renderer->stats().lights2D >= 1 && renderer->stats().particlesVisible > 0,
                  "tilemap, 2D light and particles rendered");
            check(m_shared->twod && m_shared->twod->particleStats().particles > 0, "particles preview in edit mode");
            captureNextFrame((fs::path(m_options.smokeTestDir) / "editor_2d.png").string());
            break;
        }
        case 67:
            check(ctx.undo(), "undo the paint stroke");
            check(scene.registry().get<TilemapComponent>(scene.findByName("Ground")).tileAt(21, 6) == -1, "undo erases the painted tiles");
            m_shared->brush.active = false;
            startPlay();
            break;
        // Play mode: real key events drive the Lua hero through 2D physics.
        case 72: {
            m_smokeHeroX = scene.registry().get<TransformComponent>(scene.findByName("Hero")).position.x;
            pushKey(SDLK_RIGHT, SDL_SCANCODE_RIGHT, SDL_KMOD_NONE, true);
            // The animation tools draw while the game runs (state machine highlights the current state).
            ctx.selection().set(scene.uuidOf(scene.findByName("Hero")));
            m_shared->showAnimation = m_shared->showStateMachine = m_shared->showMixer = true;
            break;
        }
        case 86: {
            const auto hero = scene.findByName("Hero");
            check(!hero.isNull() && scene.registry().get<SpriteAnimatorComponent>(hero).clip == "run", "the state machine plays the run clip");
            check(m_shared->animation && m_shared->animation->animation().currentState(hero) == "Run", "the hero's Animator is in the Run state");
            const i32 frame = hero.isNull() ? -1 : scene.registry().get<SpriteRendererComponent>(hero).frame;
            check(frame >= 4 && frame <= 9, "the sprite animator shows run frames");
            pushKey(SDLK_RIGHT, SDL_SCANCODE_RIGHT, SDL_KMOD_NONE, false);
            break;
        }
        case 90: {
            const auto hero = scene.findByName("Hero");
            check(!hero.isNull() && scene.registry().get<TransformComponent>(hero).position.x > m_smokeHeroX + 0.5f,
                  "the Right key moves the scripted hero (input -> Lua -> physics)");
            auto* scripts = m_engine->service<ScriptPlugin>();
            check(scripts && scripts->scripts()->stats().instances >= 5 && scripts->scripts()->stats().errors == 0, "scripts run without errors");
            check(m_shared->audio && m_shared->audio->audio().sourcePlaying(scene.findByName("Music")), "the music plays in play mode");
            const Renderer* renderer = m_shared->renderer();
            check(renderer && renderer->stats().overlayQuads > 10, "the game UI (HUD) is drawn in the game view");
            // Real mouse on the on-screen "Jump" button of the HUD.
            const auto jump = scene.findByName("Jump");
            check(!jump.isNull() && m_viewport.uiToScreen({scene.registry().get<UIRectComponent>(jump).screenRect.x + 20.0f,
                                                            scene.registry().get<UIRectComponent>(jump).screenRect.y + 20.0f},
                                                           m_smokeClick),
                  "the Jump button is laid out in the game view");
            m_smokeHeroY = hero.isNull() ? 0.0f : scene.registry().get<TransformComponent>(hero).position.y;
            pushMouse(SDL_EVENT_MOUSE_MOTION, m_smokeClick);
            break;
        }
        case 92: pushMouse(SDL_EVENT_MOUSE_BUTTON_DOWN, m_smokeClick); break;
        case 96: {
            check(m_shared->ui && m_shared->ui->ui().isPressed(scene.findByName("Jump")), "a mouse press holds the UI button");
            check(m_shared->animation && m_shared->animation->animation().currentState(scene.findByName("Hero")) == "Air",
                  "the hero jumps (button -> Lua -> physics) and the state machine switches to Air");
            captureNextFrame((fs::path(m_options.smokeTestDir) / "editor_play_2d.png").string());
            break;
        }
        case 98: pushMouse(SDL_EVENT_MOUSE_BUTTON_UP, m_smokeClick); break;
        case 100: {
            const auto hero = scene.findByName("Hero");
            check(!hero.isNull() && scene.registry().get<TransformComponent>(hero).position.y > m_smokeHeroY + 0.5f, "the hero rose");
            check(m_shared->audio && m_shared->audio->audio().stats().clips >= 2, "music and jump sound decoded");
            stopPlay();
            const auto restored = scene.findByName("Hero");
            check(!restored.isNull() && scene.registry().get<TransformComponent>(restored).position.x == -3.0f,
                  "stop restores the scene (hero back at its start)");
            check(m_shared->audio && !m_shared->audio->audio().sourcePlaying(scene.findByName("Music")), "stop silences the scene's sounds");
            // Animation timeline of a collectible (single clip), shown for the documentation screenshot.
            ctx.selection().set(scene.uuidOf(scene.findByName("Mushroom 2")));
            ImGui::SetWindowFocus("Animation");
            break;
        }
        case 103: captureNextFrame((fs::path(m_options.smokeTestDir) / "editor_animation.png").string()); break;
        case 105: m_shared->showAnimation = m_shared->showStateMachine = m_shared->showMixer = false; break;
        case 112: {
            std::error_code ec;
            check(fs::exists(fs::path(m_options.smokeTestDir) / "editor_2d.png", ec), "2D editor screenshot written");
            check(fs::exists(fs::path(m_options.smokeTestDir) / "editor_play_2d.png", ec), "2D play screenshot written");
            // ---- 3D: showcase template (glTF models, .agrmat materials, lights, shadows, sky).
            ctx.newScene(SceneTemplate::Showcase3D);
            ctx.camera().mode2D = false;
            m_viewport.gameView = true;  // frame it like the game camera
            break;
        }
        case 140: {
            Renderer* renderer = m_shared->renderer();
            check(renderer && renderer->stats().modelsLoading == 0, "glTF models of the showcase loaded");
            check(renderer && renderer->stats().lights >= 2 && renderer->stats().shadowCasters > 10, "point / spot lights and shadow casters");
            check(renderer && renderer->stats().meshBatches > 0 && renderer->stats().meshBatches < renderer->stats().meshesVisible,
                  "identical meshes are instanced");
            const ModelAsset* rock = m_shared->models() ? m_shared->models()->load("assets://3d/rock.glb") : nullptr;
            check(rock && rock->state == AssetState::Ready, "model loads through the editor (drag and drop path)");
            if (rock && rock->state == AssetState::Ready) {
                const auto root = ctx.instantiateModel(*rock, {-2.0f, 0.0f, 3.2f});
                check(!root.isNull() && ctx.undoStack().undoLabel() == "Add Model", "model instantiated as one undo step");
            }
            ctx.selection().clear();
            m_shared->inspectedAsset = "assets://3d/gold.agrmat";  // material editor + preview sphere
            break;
        }
        case 142: captureNextFrame((fs::path(m_options.smokeTestDir) / "editor_3d.png").string()); break;
        case 144: {
            std::error_code ec;
            check(fs::exists(fs::path(m_options.smokeTestDir) / "editor_3d.png", ec), "3D editor screenshot written");
            check(static_cast<bool>(m_materialPreview.texture(*m_shared)), "material preview rendered");
            check(ctx.undo(), "undo the added model");
            break;
        }
        case 146: {
            // UI builder: drag the HUD's counter text in the game view (edit mode); a click picks the
            // topmost element (the text over its panel).
            const auto panel = scene.findByName("Counter");
            const math::Vec4 r = panel.isNull() ? math::Vec4{} : scene.registry().get<UIRectComponent>(panel).screenRect;
            check(r.z > 0.0f && m_viewport.uiToScreen({r.x + r.z * 0.5f, r.y + r.w * 0.5f}, m_smokeClick), "HUD laid out in the edit-mode game view");
            m_smokeUiPos = panel.isNull() ? math::Vec2{} : scene.registry().get<UIRectComponent>(panel).position;
            pushMouse(SDL_EVENT_MOUSE_MOTION, m_smokeClick);
            break;
        }
        case 147: pushMouse(SDL_EVENT_MOUSE_BUTTON_DOWN, m_smokeClick); break;
        case 149: pushMouse(SDL_EVENT_MOUSE_MOTION, {m_smokeClick.x + 60.0f, m_smokeClick.y + 40.0f}); break;
        case 151: pushMouse(SDL_EVENT_MOUSE_BUTTON_UP, {m_smokeClick.x + 60.0f, m_smokeClick.y + 40.0f}); break;
        case 153: {
            const auto panel = scene.findByName("Counter");
            check(!panel.isNull() && ctx.selection().primary() == scene.uuidOf(panel), "a click in the game view selects the UI element");
            const math::Vec2 moved = panel.isNull() ? math::Vec2{} : scene.registry().get<UIRectComponent>(panel).position;
            check(moved.x > m_smokeUiPos.x + 20.0f && moved.y > m_smokeUiPos.y + 10.0f, "dragging moves the UI element (UIRect position)");
            check(ctx.undoStack().undoLabel() == "Move UI Element", "one undo entry per UI drag");
            check(ctx.undo(), "undo the UI move");
            ctx.selection().clear();
            startPlay();
            break;
        }
        case 157: pushKey(SDLK_SPACE, SDL_SCANCODE_SPACE, SDL_KMOD_NONE, true); break;
        case 159: pushKey(SDLK_SPACE, SDL_SCANCODE_SPACE, SDL_KMOD_NONE, false); break;
        case 169: {
            check(!scene.findByName("Ball 1").isNull(), "Space throws a ball (Lua spawns a physics entity)");
            auto* p3 = m_engine->service<Physics3DPlugin>();
            check(p3 && p3->world().stats().bodies > 15, "3D physics bodies are simulated in play mode");
            const auto counter = scene.findByName("Counter");
            check(!counter.isNull() && scene.registry().get<UITextComponent>(counter).text.rfind("Balles : 1", 0) == 0,
                  "the HUD counter is updated by the script");
            const auto cube = scene.findByName("copper");
            check(!cube.isNull() && !math::nearlyEqual(scene.registry().get<TransformComponent>(cube).rotation, math::Quat::identity()),
                  "the spin animation turns the kinematic cube");
            stopPlay();
            check(scene.findByName("Ball 1").isNull(), "stop removes what the game spawned");
            break;
        }
        case 171: {
            m_viewport.gameView = false;
            const rhi::Device* device = m_shared->device();
            check(device && device->validationErrorCount() == 0, "no graphics API validation error");
            m_exitCode = m_failures.empty() ? 0 : 1;
            AGR_LOG_INFO("SmokeTest", "{} ({} failure(s))", m_failures.empty() ? "SMOKE TEST PASSED" : "SMOKE TEST FAILED", m_failures.size());
            m_engine->requestQuit();
            break;
        }
        default: break;
    }
}

} // namespace agr::editor
