#include <filesystem>

#include <SDL3/SDL.h>
#include <imgui.h>
#include <misc/cpp/imgui_stdlib.h>

#include "agr/editor/AndroidExport.hpp"
#include "agr/editor/ui/Panels.hpp"

namespace agr::editor {

namespace fs = std::filesystem;

namespace {
constexpr const char* kTargets[] = {"PC (dossier)", "Android : APK de test (debug)", "Android : APK signé (release)", "Android : App Bundle (Google Play)"};
constexpr const char* kTargetDirs[] = {"desktop", "android", "android", "android"};
}

ExportPanel::~ExportPanel() {
    if (m_thread.joinable()) m_thread.join();
}

void ExportPanel::start(EditorShared& s, const std::string& editorDirectory) {
    if (m_thread.joinable()) m_thread.join();
    {
        std::lock_guard lock(m_mutex);
        m_log.clear();
        m_result.clear();
        m_resultPath.clear();
    }
    m_running = true;
    // The thread works on copies: the editor keeps running meanwhile.
    Project project = s.ctx.project();
    const fs::path output = m_output.empty() ? project.root() / "build" / kTargetDirs[m_target] : fs::path(m_output);
    const int target = m_target;
    AndroidBuildOptions options;
    options.kind = target == 1 ? AndroidBuildKind::DebugApk : target == 2 ? AndroidBuildKind::ReleaseApk : AndroidBuildKind::ReleaseBundle;
    options.storePassword = m_storePassword;
    options.keyPassword = m_keyPassword;
    const fs::path exeDir = editorDirectory;
    m_thread = std::thread([this, project, output, target, options, exeDir] {
        auto log = [this](const std::string& line) {
            std::lock_guard lock(m_mutex);
            m_log.push_back(line);
            if (m_log.size() > 4000) m_log.erase(m_log.begin(), m_log.begin() + 1000);
        };
        Result<ExportReport> report = makeError(Error::Code::Unknown, "not run");
        if (target == 0) {
#if defined(_WIN32)
            report = exportDesktop(project, output, exeDir / "agr_player.exe");
#else
            report = exportDesktop(project, output, exeDir / "agr_player");
#endif
        } else {
            log("Gradle compile l'application Android ; la première fois, il télécharge ses outils (quelques minutes).");
            report = buildAndroid(project, findAndroidToolchain(exeDir), output, options, log);
        }
        std::lock_guard lock(m_mutex);
        m_resultOk = report.ok();
        if (report) {
            for (const std::string& w : report.value().warnings) m_log.push_back("Avertissement : " + w);
            m_resultPath = report.value().output.string();
            m_result = "Export terminé : " + m_resultPath;
        } else {
            m_result = "Échec : " + report.error().message;
        }
        m_running = false;
    });
}

void ExportPanel::draw(EditorShared& s, const std::string& editorDirectory) {
    if (!open) return;
    ImGui::SetNextWindowSize({720, 640}, ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("Exporter le jeu", &open)) {
        ImGui::End();
        return;
    }
    if (!s.ctx.hasProject()) {
        ImGui::End();
        return;
    }
    Project& project = s.ctx.project();
    ProjectSettings& ps = project.settings();
    ExportSettings& x = ps.exportSettings;
    const bool running = m_running.load();
    bool changed = false;
    ImGui::BeginDisabled(running);

    ImGui::SetNextItemWidth(320);
    ImGui::Combo("Cible", &m_target, kTargets, 4);
    const bool android = m_target > 0, release = m_target >= 2;

    ImGui::SeparatorText("Jeu");
    changed |= ImGui::InputText("Nom", &ps.name);
    changed |= ImGui::InputText("Éditeur (organisation)", &x.organization);
    changed |= ImGui::InputText("Version", &x.version);
    int versionCode = static_cast<int>(x.versionCode);
    if (ImGui::InputInt("Code de version", &versionCode)) {
        x.versionCode = static_cast<u32>(std::max(1, versionCode));
        changed = true;
    }
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Entier qui doit augmenter à chaque envoi sur Google Play");
    static const char* orientations[] = {"Paysage", "Portrait", "Les deux (capteur)"};
    int orientation = static_cast<int>(x.orientation);
    if (ImGui::Combo("Orientation", &orientation, orientations, 3)) {
        x.orientation = static_cast<ScreenOrientation>(orientation);
        changed = true;
    }
    int fps = static_cast<int>(x.targetFps);
    static const char* fpsModes[] = {"Écran (vsync)", "30 images/s (économie de batterie)", "60 images/s"};
    int fpsIndex = fps == 30 ? 1 : fps == 60 ? 2 : 0;
    if (ImGui::Combo("Cadence", &fpsIndex, fpsModes, 3)) {
        x.targetFps = fpsIndex == 1 ? 30u : fpsIndex == 2 ? 60u : 0u;
        changed = true;
    }
    changed |= ImGui::Checkbox("Garder l'écran allumé", &x.keepScreenOn);
    ImGui::SameLine();
    changed |= ImGui::Checkbox("Bouton Retour : quitter", &x.backQuits);

    if (android) {
        ImGui::SeparatorText("Android");
        changed |= ImGui::InputTextWithHint("Identifiant", defaultPackageId(ps).c_str(), &x.packageId);
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Identifiant unique de l'application (com.studio.jeu) ; vide = généré");
        changed |= ImGui::Checkbox("Vibration (permission VIBRATE)", &x.vibration);
        changed |= ImGui::Checkbox("Réseau / multijoueur (permission INTERNET)", &x.network);
        int minSdk = static_cast<int>(x.minSdk), targetSdk = static_cast<int>(x.targetSdk);
        ImGui::SetNextItemWidth(120);
        if (ImGui::InputInt("API minimale", &minSdk)) x.minSdk = static_cast<u32>(std::clamp(minSdk, 26, 40)), changed = true;
        ImGui::SameLine();
        ImGui::SetNextItemWidth(120);
        if (ImGui::InputInt("API cible", &targetSdk)) x.targetSdk = static_cast<u32>(std::clamp(targetSdk, 26, 40)), changed = true;
        ImGui::TextUnformatted("Processeurs :");
        for (const char* abi : {"arm64-v8a", "x86_64"}) {
            ImGui::SameLine();
            bool on = std::find(x.abis.begin(), x.abis.end(), abi) != x.abis.end();
            if (ImGui::Checkbox(abi, &on)) {
                if (on) x.abis.push_back(abi);
                else std::erase(x.abis, std::string(abi));
                changed = true;
            }
        }
        ImGui::SameLine();
        ImGui::TextDisabled("(?)");
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("arm64-v8a : téléphones et tablettes ; x86_64 : émulateurs et Chromebooks");
    }
    if (release) {
        ImGui::SeparatorText("Signature");
        changed |= ImGui::InputText("Keystore", &x.keystorePath);
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Fichier de clé (relatif au projet ou absolu). Gardez-en une copie : il est indispensable pour publier les mises à jour.");
        changed |= ImGui::InputText("Alias de la clé", &x.keyAlias);
        ImGui::InputText("Mot de passe du keystore", &m_storePassword, ImGuiInputTextFlags_Password);
        ImGui::InputText("Mot de passe de la clé", &m_keyPassword, ImGuiInputTextFlags_Password);
        ImGui::TextDisabled("Les mots de passe ne sont jamais enregistrés (vide = celui du keystore).");
        if (ImGui::TreeNode("Créer un keystore")) {
            ImGui::InputText("Alias", &m_newAlias);
            ImGui::InputText("Propriétaire (nom ou studio)", &m_newOwner);
            ImGui::InputText("Mot de passe (6 caractères minimum)", &m_newPassword, ImGuiInputTextFlags_Password);
            if (ImGui::Button("Créer keys/release.p12")) {
                const fs::path path = project.root() / "keys" / "release.p12";
                if (Status st = createKeystore(findAndroidToolchain(editorDirectory), path, m_newAlias, m_newPassword, m_newOwner); !st) {
                    s.notify(st.error().message);
                } else {
                    x.keystorePath = "keys/release.p12";
                    x.keyAlias = m_newAlias;
                    m_storePassword = m_newPassword;
                    m_newPassword.clear();
                    changed = true;
                    s.notify("Keystore créé : " + path.string() + " (sauvegardez-le en lieu sûr)");
                }
            }
            ImGui::TreePop();
        }
    }
    if (changed)
        if (Status st = project.save(); !st) s.notify("Réglages non enregistrés : " + st.error().message);

    ImGui::SeparatorText("Sortie");
    ImGui::InputTextWithHint("Dossier", (project.root() / "build" / kTargetDirs[m_target]).string().c_str(), &m_output);

    // Checks.
    std::vector<std::string> problems;
    if (android) {
        const AndroidToolchain tools = findAndroidToolchain(editorDirectory);
        problems = checkAndroidExport(project, tools);
        ImGui::TextDisabled("SDK : %s", tools.sdk.empty() ? "introuvable (ANDROID_HOME)" : tools.sdk.string().c_str());
        ImGui::TextDisabled("Gradle : %s", tools.gradle.empty() ? "introuvable (PATH ou AGR_GRADLE)" : tools.gradle.string().c_str());
        ImGui::TextDisabled("Modèles d'export : %s", tools.templates.empty() ? "introuvables" : tools.templates.string().c_str());
        if (release && x.keystorePath.empty()) problems.push_back("choisissez ou créez un keystore");
        if (release && m_storePassword.empty()) problems.push_back("saisissez le mot de passe du keystore");
    }
    if (s.ctx.dirty()) problems.push_back("la scène a des modifications non enregistrées (le jeu exporte les fichiers enregistrés)");
    for (const std::string& p : problems) ImGui::TextColored(palette::toVec4(palette::kWarning), "%s", p.c_str());
    ImGui::EndDisabled();

    ImGui::BeginDisabled(running || !problems.empty() || s.ctx.playState() != PlayState::Edit);
    if (ImGui::Button(running ? "Export en cours..." : "Exporter", {160, 0})) start(s, editorDirectory);
    ImGui::EndDisabled();
    if (s.ctx.dirty()) {
        ImGui::SameLine();
        if (ImGui::Button("Enregistrer la scène")) {
            if (Status st = s.ctx.saveScene(); !st) s.notify(st.error().message);
        }
    }

    std::lock_guard lock(m_mutex);
    if (!m_result.empty()) {
        ImGui::TextColored(palette::toVec4(m_resultOk ? palette::kSuccess : palette::kError), "%s", m_result.c_str());
        if (m_resultOk && ImGui::SmallButton("Ouvrir le dossier")) {
            const fs::path p = m_resultPath;
            const std::string url = "file://" + (fs::is_directory(p) ? p : p.parent_path()).generic_string();
            if (!SDL_OpenURL(url.c_str())) s.notify(std::string("Impossible d'ouvrir : ") + SDL_GetError());
        }
    }
    ImGui::BeginChild("log", {0, 0}, ImGuiChildFlags_Borders);
    if (s.fonts.mono) ImGui::PushFont(s.fonts.mono, 0.0f);
    ImGuiListClipper clipper;
    clipper.Begin(static_cast<int>(m_log.size()));
    while (clipper.Step())
        for (int i = clipper.DisplayStart; i < clipper.DisplayEnd; ++i) ImGui::TextUnformatted(m_log[static_cast<usize>(i)].c_str());
    if (s.fonts.mono) ImGui::PopFont();
    if (running && ImGui::GetScrollY() >= ImGui::GetScrollMaxY() - 20) ImGui::SetScrollHereY(1.0f);
    ImGui::EndChild();
    ImGui::End();
}

} // namespace agr::editor
