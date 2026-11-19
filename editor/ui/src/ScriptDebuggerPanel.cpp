// Script debugger panel: breakpoints on the code view's line numbers, pause, and the
// break view (current line, call stack, variables, watches, continue / step / stop).
// The break view is drawn by EditorApp's nested loop while the script waits.
#include <algorithm>
#include <sstream>

#include <imgui.h>
#include <misc/cpp/imgui_stdlib.h>

#include "agr/editor/ui/Panels.hpp"
#include "agr/script/ScriptComponent.hpp"
#include "agr/script/ScriptPlugin.hpp"

namespace agr::editor {

namespace {

ScriptEngine* scriptsOf(EditorShared& s) {
    auto* plugin = s.engine.service<ScriptPlugin>();
    return plugin ? plugin->scripts() : nullptr;
}

const char* reasonText(ScriptBreak::Reason r) {
    switch (r) {
    case ScriptBreak::Reason::Breakpoint: return "point d'arrêt";
    case ScriptBreak::Reason::Step: return "pas à pas";
    case ScriptBreak::Reason::Pause: return "pause";
    case ScriptBreak::Reason::Error: return "erreur";
    }
    return "";
}

} // namespace

void ScriptDebuggerPanel::toggleBreakpoint(const std::string& uri, int line) {
    const auto key = std::make_pair(uri, line);
    if (!m_breakpoints.erase(key)) m_breakpoints.insert(key);
    m_dirty = true;
}

void ScriptDebuggerPanel::sync(ScriptEngine* scripts) {
    if (!scripts || (!m_dirty && scripts == m_synced)) return;
    scripts->clearBreakpoints();
    for (const auto& [uri, line] : m_breakpoints) scripts->setBreakpoint(uri, line);
    scripts->setBreakOnError(breakOnError);
    m_synced = scripts;
    m_dirty = false;
}

void ScriptDebuggerPanel::openFile(EditorShared& s, const std::string& uri) {
    const i64 time = s.engine.vfs().modifiedTime(uri);
    if (uri == m_file && time == m_fileTime && !m_lines.empty()) return;
    m_file = uri;
    m_fileTime = time;
    m_lines.clear();
    auto text = s.engine.vfs().readText(uri);
    if (!text) return;
    std::istringstream in(text.value());
    for (std::string line; std::getline(in, line);) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        std::replace(line.begin(), line.end(), '\t', ' ');
        m_lines.push_back(std::move(line));
    }
}

void ScriptDebuggerPanel::drawSource(EditorShared&, int current, float height) {
    if (!ImGui::BeginChild("##source", ImVec2(0, height), ImGuiChildFlags_Borders, ImGuiWindowFlags_HorizontalScrollbar)) {
        ImGui::EndChild();
        return;
    }
    if (m_lines.empty()) ImGui::TextDisabled(m_file.empty() ? "Choisissez un script." : "Fichier illisible.");
    const float gutter = ImGui::CalcTextSize("00000").x;
    ImGuiListClipper clipper;
    clipper.Begin(static_cast<int>(m_lines.size()));
    if (m_scroll && current > 0) clipper.IncludeItemByIndex(current - 1);
    while (clipper.Step()) {
        for (int i = clipper.DisplayStart; i < clipper.DisplayEnd; ++i) {
            const int line = i + 1;
            ImGui::PushID(line);
            const bool bp = hasBreakpoint(m_file, line);
            const ImVec2 pos = ImGui::GetCursorScreenPos();
            const float h = ImGui::GetTextLineHeight();
            ImDrawList* dl = ImGui::GetWindowDrawList();
            if (line == current) {
                dl->AddRectFilled(pos, ImVec2(pos.x + ImGui::GetContentRegionAvail().x + 4000.0f, pos.y + h), IM_COL32(230, 180, 40, 70));
                if (m_scroll) {
                    ImGui::SetScrollHereY(0.4f);
                    m_scroll = false;
                }
            }
            // Gutter: a click toggles the breakpoint.
            if (ImGui::InvisibleButton("##bp", ImVec2(gutter, h))) toggleBreakpoint(m_file, line);
            if (bp) dl->AddCircleFilled(ImVec2(pos.x + h * 0.5f, pos.y + h * 0.5f), h * 0.32f, IM_COL32(220, 60, 60, 255));
            dl->AddText(ImVec2(pos.x + h, pos.y), line == current ? IM_COL32(255, 220, 120, 255) : IM_COL32(130, 130, 140, 255),
                        std::to_string(line).c_str());
            ImGui::SameLine();
            ImGui::TextUnformatted(m_lines[i].c_str());
            ImGui::PopID();
        }
    }
    ImGui::EndChild();
}

void ScriptDebuggerPanel::draw(EditorShared& s) {
    ScriptEngine* scripts = scriptsOf(s);
    sync(scripts);
    if (!s.showScriptDebugger) return;
    if (!ImGui::Begin("Débogueur de scripts", &s.showScriptDebugger)) {
        ImGui::End();
        return;
    }
    // Scripts of the scene, then any file typed in.
    std::set<std::string> files;
    if (s.ctx.hasProject())
        s.ctx.scene().registry().view<ScriptComponent>().each([&](ecs::Entity, ScriptComponent& sc) {
            if (sc.script.size() > 4 && sc.script.ends_with(".lua")) files.insert(sc.script);
        });
    for (const auto& [uri, line] : m_breakpoints) files.insert(uri);
    if (m_file.empty() && !files.empty()) openFile(s, *files.begin());
    ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x * 0.45f);
    if (ImGui::BeginCombo("##file", m_file.empty() ? "(script)" : m_file.c_str())) {
        for (const std::string& f : files)
            if (ImGui::Selectable(f.c_str(), f == m_file)) openFile(s, f);
        ImGui::EndCombo();
    }
    ImGui::SameLine();
    ImGui::SetNextItemWidth(160.0f);
    if (ImGui::InputTextWithHint("##open", "assets://...lua", &m_openInput, ImGuiInputTextFlags_EnterReturnsTrue) && !m_openInput.empty())
        openFile(s, m_openInput);
    ImGui::SameLine();
    const bool playing = s.ctx.playState() != PlayState::Edit;
    ImGui::BeginDisabled(!playing || !scripts);
    if (ImGui::Button("Pause (F6)") && scripts) scripts->requestPause();
    ImGui::EndDisabled();
    ImGui::SameLine();
    if (ImGui::Checkbox("Arrêt sur erreur", &breakOnError)) m_dirty = true;
    if (!m_file.empty()) openFile(s, m_file);  // reloads the view when the file changes

    const float listH = std::min(140.0f, 24.0f + ImGui::GetTextLineHeightWithSpacing() * static_cast<float>(m_breakpoints.size() + 1));
    drawSource(s, 0, std::max(80.0f, ImGui::GetContentRegionAvail().y - listH));
    ImGui::Text("Points d'arrêt (%zu) — cliquez un numéro de ligne", m_breakpoints.size());
    ImGui::SameLine();
    if (ImGui::SmallButton("Tout supprimer")) {
        m_breakpoints.clear();
        m_dirty = true;
    }
    std::pair<std::string, int> remove{"", 0}, go{"", 0};
    for (const auto& bp : m_breakpoints) {
        ImGui::PushID((bp.first + std::to_string(bp.second)).c_str());
        if (ImGui::SmallButton("x")) remove = bp;
        ImGui::SameLine();
        if (ImGui::Selectable((bp.first + ":" + std::to_string(bp.second)).c_str())) go = bp;
        ImGui::PopID();
    }
    if (remove.second) toggleBreakpoint(remove.first, remove.second);
    if (go.second) openFile(s, go.first);
    if (playing && scripts && ImGui::IsKeyPressed(ImGuiKey_F6, false)) scripts->requestPause();
    ImGui::End();
}

void ScriptDebuggerPanel::beginBreak(const ScriptBreak& b) {
    m_frame = 0;
    m_scroll = true;
}

std::optional<ScriptDebugAction> ScriptDebuggerPanel::drawBreak(EditorShared& s, const ScriptBreak& b) {
    std::optional<ScriptDebugAction> action;
    ScriptEngine* scripts = scriptsOf(s);
    const ImGuiViewport* vp = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(ImVec2(vp->WorkPos.x + vp->WorkSize.x * 0.5f, vp->WorkPos.y + vp->WorkSize.y * 0.5f), ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(ImVec2(vp->WorkSize.x * 0.8f, vp->WorkSize.y * 0.85f), ImGuiCond_Appearing);
    ImGui::Begin("Script arrêté###ScriptBreak", nullptr, ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoCollapse);
    if (m_frame >= b.stack.size()) m_frame = 0;
    const ScriptStackFrame* frame = b.stack.empty() ? nullptr : &b.stack[m_frame];

    // Toolbar (+ the usual shortcuts).
    const bool shift = ImGui::GetIO().KeyShift;
    if (ImGui::Button("Continuer (F5)") || ImGui::IsKeyPressed(ImGuiKey_F5, false)) action = ScriptDebugAction::Continue;
    ImGui::SameLine();
    if (ImGui::Button("Pas à pas principal (F10)") || ImGui::IsKeyPressed(ImGuiKey_F10, false)) action = ScriptDebugAction::StepOver;
    ImGui::SameLine();
    if (ImGui::Button("Pas à pas détaillé (F11)") || (!shift && ImGui::IsKeyPressed(ImGuiKey_F11, false))) action = ScriptDebugAction::StepInto;
    ImGui::SameLine();
    if (ImGui::Button("Pas à pas sortant (Maj+F11)") || (shift && ImGui::IsKeyPressed(ImGuiKey_F11, false))) action = ScriptDebugAction::StepOut;
    ImGui::SameLine();
    if (ImGui::Button("Arrêter le jeu")) action = ScriptDebugAction::Stop;
    ImGui::Text("%s — %s:%d", reasonText(b.reason), b.source.c_str(), b.line);
    if (!b.message.empty()) {
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.45f, 0.4f, 1.0f));
        ImGui::TextWrapped("%s", b.message.c_str());
        ImGui::PopStyleColor();
    }

    // Left: code of the selected frame. Right: stack, variables, watches.
    const float rightW = std::max(260.0f, ImGui::GetContentRegionAvail().x * 0.38f);
    ImGui::BeginChild("##code", ImVec2(-rightW, 0));
    if (frame) openFile(s, frame->source);
    drawSource(s, frame ? frame->line : 0, 0.0f);
    ImGui::EndChild();
    ImGui::SameLine();
    ImGui::BeginChild("##info", ImVec2(0, 0));
    ImGui::SeparatorText("Pile d'appels");
    for (usize i = 0; i < b.stack.size(); ++i) {
        const auto& f = b.stack[i];
        const std::string label = f.function + "  " + f.source + ":" + std::to_string(f.line) + "##frame" + std::to_string(i);
        if (ImGui::Selectable(label.c_str(), i == m_frame)) {
            m_frame = i;
            m_scroll = true;
        }
    }
    ImGui::SeparatorText("Variables");
    if (frame && ImGui::BeginTable("##vars", 2, ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable | ImGuiTableFlags_BordersInnerV)) {
        ImGui::TableSetupColumn("nom", ImGuiTableColumnFlags_WidthFixed, 110.0f);
        ImGui::TableSetupColumn("valeur", ImGuiTableColumnFlags_WidthStretch);
        for (const auto& v : frame->variables) {
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            if (v.upvalue) {
                ImGui::TextDisabled("%s", v.name.c_str());
                if (ImGui::IsItemHovered()) ImGui::SetTooltip("variable capturée (upvalue)");
            } else {
                ImGui::TextUnformatted(v.name.c_str());
            }
            ImGui::TableNextColumn();
            ImGui::TextWrapped("%s", v.value.c_str());
        }
        ImGui::EndTable();
    }
    ImGui::SeparatorText("Expressions");
    usize removeWatch = m_watches.size();
    for (usize i = 0; i < m_watches.size(); ++i) {
        ImGui::PushID(static_cast<int>(i));
        if (ImGui::SmallButton("x")) removeWatch = i;
        ImGui::SameLine();
        std::string value;
        if (scripts) {
            auto r = scripts->evaluate(m_watches[i], m_frame);
            value = r ? r.value() : "erreur : " + r.error().message;
        }
        ImGui::TextWrapped("%s = %s", m_watches[i].c_str(), value.c_str());
        ImGui::PopID();
    }
    if (removeWatch < m_watches.size()) m_watches.erase(m_watches.begin() + static_cast<std::ptrdiff_t>(removeWatch));
    ImGui::SetNextItemWidth(-1.0f);
    if (ImGui::InputTextWithHint("##watch", "expression Lua (ex. self.health * 2), Entrée", &m_watchInput, ImGuiInputTextFlags_EnterReturnsTrue) &&
        !m_watchInput.empty()) {
        m_watches.push_back(m_watchInput);
        m_watchInput.clear();
    }
    ImGui::EndChild();
    ImGui::End();
    sync(scripts);  // breakpoints toggled while stopped apply at once
    return action;
}

} // namespace agr::editor
