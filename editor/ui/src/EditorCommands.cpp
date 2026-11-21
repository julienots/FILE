// History panel (every undo state, click to go back or forward to it) and the command palette
// (Ctrl+Shift+P / Ctrl+K): every action, panel, creation, scene, script and entity by fuzzy search.
#include <algorithm>
#include <cctype>
#include <filesystem>

#include <imgui.h>
#include <imgui_stdlib.h>

#include "agr/editor/ui/EditorApp.hpp"

namespace agr::editor {

namespace {

/// Fuzzy match: every character of the query in order (case-insensitive). Higher is better;
/// -1 = no match. Rewards a match at the start, at word starts and consecutive characters.
int fuzzyScore(std::string_view text, std::string_view query) {
    if (query.empty()) return 0;
    auto lower = [](char c) { return static_cast<char>(std::tolower(static_cast<unsigned char>(c))); };
    int score = 0, run = 0;
    usize qi = 0;
    for (usize i = 0; i < text.size() && qi < query.size(); ++i) {
        if (lower(text[i]) == lower(query[qi])) {
            const bool wordStart = i == 0 || text[i - 1] == ' ' || text[i - 1] == '/' || text[i - 1] == ':' || text[i - 1] == '_';
            score += 1 + (wordStart ? 6 : 0) + run * 3 + (i == qi ? 4 : 0);
            ++run;
            ++qi;
        } else {
            run = 0;
        }
    }
    return qi == query.size() ? score - static_cast<int>(text.size()) / 16 : -1;
}

} // namespace

std::vector<EditorApp::Command> EditorApp::buildCommands() {
    std::vector<Command> out;
    EditorContext& ctx = *m_ctx;
    EditorShared& s = *m_shared;
    auto add = [&](std::string label, std::function<void()> run) { out.push_back({std::move(label), std::move(run)}); };
    add("Enregistrer la scène", [this] { saveOrSaveAs(); });
    add("Jouer / arrêter", [this] {
        if (m_ctx->playState() == PlayState::Edit) startPlay();
        else stopPlay();
    });
    add("Annuler", [&ctx] { ctx.undo(); });
    add("Rétablir", [&ctx] { ctx.redo(); });
    add("Dupliquer la sélection", [&ctx] { ctx.duplicateSelection(); });
    add("Grouper la sélection", [&ctx] { ctx.groupSelection(); });
    add("Supprimer la sélection", [&ctx] { ctx.deleteSelection(); });
    add("Tout sélectionner", [&ctx] { ctx.selectAll(); });
    add("Cadrer la sélection", [&ctx] { ctx.focusSelection(); });
    add("Réinitialiser la disposition", [this] { m_resetLayout = true; });
    add("Exporter le jeu...", [this] { m_export.open = true; });
    add("Documentation", [&s] { s.showDocs = true; });
    for (const auto& [label, flag] : std::initializer_list<std::pair<const char*, bool*>>{
             {"Animation (timeline)", &s.showAnimation}, {"Machine à états", &s.showStateMachine}, {"Mixeur audio", &s.showMixer},
             {"Entrées (actions)", &s.showInput},         {"Localisation", &s.showLocalization},      {"Éditeur de shaders", &s.showShaderEditor},
             {"Éditeur de scripts", &s.showScriptEditor}, {"Logique visuelle", &s.showLogic},          {"Plugins et paquets", &s.showPackages},
             {"Profileur", &s.showProfiler},              {"Débogueur de scripts", &s.showScriptDebugger}, {"Effets de particules", &s.showParticles}, {"Feuilles de sprites", &s.showSheetEditor},
             {"Couches de collision", &s.showPhysicsLayers}, {"Historique (annuler)", &m_showHistory}, {"Vue jeu", &m_viewport.gameView},
             {"Grille", &m_viewport.showGrid}})
        add(std::string(*flag ? "Masquer : " : "Afficher : ") + label, [flag] { *flag = !*flag; });
    for (const auto& [label, kind] : createMenuItems())
        if (label) add(std::string("Créer : ") + label, [&ctx, kind = kind] { ctx.create(kind); });
    for (const std::string& scene : ctx.project().listScenes())
        add("Ouvrir la scène : " + scene, [this, scene] { m_shared->runGuarded([this, scene] { m_ctx->loadScene(scene); }); });
    std::error_code ec;
    const auto assets = ctx.project().assetsDirectory();
    usize files = 0;
    for (auto it = std::filesystem::recursive_directory_iterator(assets, ec); !ec && it != std::filesystem::recursive_directory_iterator() && files < 2000;
         it.increment(ec), ++files) {
        if (!it->is_regular_file(ec)) continue;
        const std::string ext = it->path().extension().string();
        const std::string uri = "assets://" + std::filesystem::relative(it->path(), assets, ec).generic_string();
        if (ext == ".lua") add("Ouvrir le script : " + uri, [&s, uri] { s.openScriptRequest = uri; });
        else if (ext == ".agrshader") add("Ouvrir le shader : " + uri, [&s, uri] { s.openShaderRequest = uri; });
        else if (ext == ".agrlogic") add("Ouvrir la logique : " + uri, [&s, uri] { s.openLogicRequest = uri; });
        else if (ext == ".agrsheet") add("Éditer la feuille : " + uri, [&s, uri] { s.openSheetRequest = uri; });
    }
    Scene& scene = ctx.scene();
    usize entities = 0;
    scene.traverse([&](ecs::Entity e, u32) {
        if (entities++ >= 500) return;
        const Uuid id = scene.uuidOf(e);
        add("Sélectionner : " + scene.nameOf(e), [&ctx, id] {
            ctx.selection().assign({id});
            ctx.focusSelection();
        });
    });
    return out;
}

void EditorApp::drawCommandPalette() {
    if (!m_paletteOpen) return;
    const ImGuiViewport* vp = ImGui::GetMainViewport();
    const float w = std::min(640.0f, vp->WorkSize.x * 0.9f);
    ImGui::SetNextWindowPos({vp->WorkPos.x + (vp->WorkSize.x - w) * 0.5f, vp->WorkPos.y + 60.0f});
    ImGui::SetNextWindowSize({w, 0});
    ImGui::SetNextWindowFocus();
    if (!ImGui::Begin("##palette", nullptr,
                      ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::End();
        return;
    }
    if (m_paletteFocus) {
        ImGui::SetKeyboardFocusHere();
        m_paletteFocus = false;
    }
    ImGui::SetNextItemWidth(-FLT_MIN);
    if (ImGui::InputTextWithHint("##q", "Commande, panneau, création, scène, script, entité...", &m_paletteQuery)) m_paletteSelected = 0;
    // Ranked matches.
    std::vector<std::pair<int, usize>> ranked;
    for (usize i = 0; i < m_paletteCommands.size(); ++i) {
        const int sc = fuzzyScore(m_paletteCommands[i].label, m_paletteQuery);
        if (sc >= 0) ranked.push_back({sc, i});
    }
    std::stable_sort(ranked.begin(), ranked.end(), [](const auto& a, const auto& b) { return a.first > b.first; });
    const int shown = std::min<int>(14, static_cast<int>(ranked.size()));
    if (ImGui::IsKeyPressed(ImGuiKey_DownArrow)) m_paletteSelected = std::min(m_paletteSelected + 1, std::max(shown - 1, 0));
    if (ImGui::IsKeyPressed(ImGuiKey_UpArrow)) m_paletteSelected = std::max(m_paletteSelected - 1, 0);
    int run = -1;
    for (int i = 0; i < shown; ++i) {
        const auto& cmd = m_paletteCommands[ranked[static_cast<usize>(i)].second];
        if (ImGui::Selectable(cmd.label.c_str(), i == m_paletteSelected)) run = i;
    }
    if (ranked.empty()) ImGui::TextDisabled("Aucun résultat");
    ImGui::TextDisabled("Entrée : exécuter · flèches : choisir · Échap : fermer");
    if (ImGui::IsKeyPressed(ImGuiKey_Enter) || ImGui::IsKeyPressed(ImGuiKey_KeypadEnter)) run = m_paletteSelected;
    const bool close = ImGui::IsKeyPressed(ImGuiKey_Escape) || (!ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows) && !m_paletteFocus);
    ImGui::End();
    if (run >= 0 && run < shown) {
        m_paletteOpen = false;
        const auto action = m_paletteCommands[ranked[static_cast<usize>(run)].second].run;
        action();
    } else if (close) {
        m_paletteOpen = false;
    }
}

void EditorApp::drawHistory() {
    if (!m_showHistory) return;
    if (!ImGui::Begin("Historique", &m_showHistory)) {
        ImGui::End();
        return;
    }
    UndoStack& undo = m_ctx->undoStack();
    const bool editing = m_ctx->playState() == PlayState::Edit;
    if (!editing) ImGui::TextDisabled("En mode Jeu : l'historique reprend à l'arrêt.");
    ImGui::TextDisabled("Clic : revenir à cet état (les suivants restent disponibles).");
    ImGui::Separator();
    usize jump = undo.size();
    for (usize i = 0; i < undo.size(); ++i) {
        const bool current = i == undo.position();
        const bool future = i > undo.position();
        const std::string& label = undo.label(i);
        std::string text = (i == 0 && label.empty() ? std::string("Ouverture") : (label.empty() ? std::string("(sans nom)") : label));
        if (future) ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
        ImGui::PushID(static_cast<int>(i));
        if (ImGui::Selectable(((current ? "> " : "  ") + text).c_str(), current) && editing) jump = i;
        ImGui::PopID();
        if (future) ImGui::PopStyleColor();
        if (current && ImGui::IsWindowAppearing()) ImGui::SetScrollHereY();
    }
    ImGui::End();
    if (jump < undo.size()) m_ctx->jumpToHistory(jump);
}

} // namespace agr::editor
