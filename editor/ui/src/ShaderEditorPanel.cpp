#include <algorithm>
#include <cfloat>
#include <cstdio>

#include <imgui.h>
#include <imgui_stdlib.h>

#include "agr/editor/ui/ImGuiRenderer.hpp"
#include "agr/editor/ui/Panels.hpp"

namespace agr::editor {

namespace {

constexpr f64 kCompileDelay = 0.35;  // seconds after the last keystroke

int lineOf(const char* text, int offset) {
    int line = 1;
    for (int i = 0; i < offset && text[i]; ++i)
        if (text[i] == '\n') ++line;
    return line;
}

int offsetOfLine(const char* text, int length, int line) {
    int current = 1;
    for (int i = 0; i < length; ++i) {
        if (current == line) return i;
        if (text[i] == '\n') ++current;
    }
    return length;
}

const char* kHelp =
    "Fonctions (GLSL) :\n"
    "void surface(inout Surface s)\n"
    "  modifiable : baseColor, alpha, metallic,\n"
    "  roughness, normal, emissive, occlusion\n"
    "  lecture : worldPos, viewDir, uv, color\n"
    "void vertex(inout Vertex v)  (facultatif)\n"
    "  position, normal (espace monde), uv,\n"
    "  color ; lecture : localPosition\n"
    "TIME : temps écoulé (secondes)\n\n"
    "Paramètres (réglables par matériau) :\n"
    "param float vitesse = 1 range(0, 5);\n"
    "param color teinte = (1, 0.5, 0.2, 1);\n"
    "param vec2 / vec3 / vec4 v = (...);\n"
    "param texture motif = \"assets://...\";\n"
    "  (ajouter srgb pour une image en couleur)\n\n"
    "Guide complet : docs/SHADERS.md";

} // namespace

void ShaderEditorPanel::open(EditorShared& s, const std::string& uri) {
    ShaderLibrary* lib = s.shaders();
    if (!lib) return;
    const ShaderAsset* asset = lib->get(uri);
    if (!asset) {
        s.notify("Shader illisible : " + uri + " (voir la console)");
        return;
    }
    m_uri = uri;
    m_text = m_saved = asset->source;
    m_pendingCompile = false;
    m_gotoLine = 0;
    m_cursorLine = 1;
    s.shaderDirty = false;
    if (MaterialLibrary* materials = s.materials()) {  // preview sphere: a material using only this shader
        MaterialData m;
        m.name = "Aperçu du shader";
        m.shader = uri;
        materials->preview(kPreviewMaterial, m);
    }
    compile(s);
}

void ShaderEditorPanel::compile(EditorShared& s) {
    m_pendingCompile = false;
    if (m_uri.empty()) return;
    if (ShaderLibrary* lib = s.shaders()) lib->preview(m_uri, m_text);  // the scene uses the new code at once
    if (Renderer* r = s.renderer()) m_status = r->shaderStatus(m_uri);
    else m_status = {false, {{0, "pas de rendu (mode sans affichage)"}}};
}

void ShaderEditorPanel::edit(EditorShared& s, std::string text, bool compileNow) {
    m_text = std::move(text);
    s.shaderDirty = m_text != m_saved;
    m_editTime = ImGui::GetTime();
    m_pendingCompile = true;
    if (compileNow) compile(s);
}

void ShaderEditorPanel::save(EditorShared& s) {
    ShaderLibrary* lib = s.shaders();
    if (!lib || m_uri.empty()) return;
    if (Status st = lib->save(m_uri, m_text); !st) {
        s.notify("Enregistrement impossible : " + st.error().message);
        return;
    }
    m_saved = m_text;
    s.shaderDirty = false;
    compile(s);
    s.notify(m_status.usable ? "Shader enregistré : " + m_uri
                             : "Shader enregistré avec des erreurs (ses matériaux utilisent le rendu standard)");
}

void ShaderEditorPanel::revert(EditorShared& s) {
    ShaderLibrary* lib = s.shaders();
    if (!lib || m_uri.empty()) return;
    lib->invalidate(m_uri);  // drop the live preview: read the file again
    const std::string uri = m_uri;
    m_uri.clear();
    open(s, uri);
}

void ShaderEditorPanel::drawCode(EditorShared& s, float height) {
    const float lineH = ImGui::GetTextLineHeight();
    const ImGuiStyle& style = ImGui::GetStyle();
    ImGui::BeginChild("##codeScroll", {0, height}, ImGuiChildFlags_Borders, ImGuiWindowFlags_HorizontalScrollbar);
    const int lines = static_cast<int>(std::count(m_text.begin(), m_text.end(), '\n')) + 1;
    char buf[16];
    std::snprintf(buf, sizeof buf, "%d", std::max(lines, 999));
    const float gutter = ImGui::CalcTextSize(buf).x + style.ItemSpacing.x * 2.0f;
    // Gutter: line numbers, errors in red.
    const ImVec2 origin = ImGui::GetCursorScreenPos();
    ImDrawList* dl = ImGui::GetWindowDrawList();
    const float scroll = ImGui::GetScrollY();
    const int first = std::max(0, static_cast<int>(scroll / lineH) - 1);
    const int last = std::min(lines, first + static_cast<int>(ImGui::GetWindowHeight() / lineH) + 3);
    for (int i = first; i < last; ++i) {
        const bool error = std::any_of(m_status.diagnostics.begin(), m_status.diagnostics.end(),
                                       [&](const ShaderDiagnostic& d) { return d.line == static_cast<u32>(i + 1); });
        std::snprintf(buf, sizeof buf, "%d", i + 1);
        const float y = origin.y + style.FramePadding.y + static_cast<float>(i) * lineH;
        if (error) dl->AddRectFilled({origin.x, y}, {origin.x + gutter - style.ItemSpacing.x * 0.5f, y + lineH}, IM_COL32(200, 60, 60, 90));
        dl->AddText({origin.x + gutter - style.ItemSpacing.x - ImGui::CalcTextSize(buf).x, y},
                    error ? palette::kError : (i + 1 == m_cursorLine ? palette::kText : palette::kTextMuted), buf);
    }
    ImGui::SetCursorScreenPos({origin.x + gutter, origin.y});
    // The text field is as tall as the code: this child scrolls numbers and text together.
    float width = ImGui::GetContentRegionAvail().x;
    {
        usize start = 0;
        while (start <= m_text.size()) {
            usize end = m_text.find('\n', start);
            if (end == std::string::npos) end = m_text.size();
            if (end - start > 60) width = std::max(width, ImGui::CalcTextSize(m_text.data() + start, m_text.data() + end).x + 40.0f);
            start = end + 1;
        }
    }
    if (m_gotoLine > 0) ImGui::SetKeyboardFocusHere();
    auto callback = [](ImGuiInputTextCallbackData* data) -> int {
        auto* self = static_cast<ShaderEditorPanel*>(data->UserData);
        if (self->m_gotoLine > 0) {
            const int at = offsetOfLine(data->Buf, data->BufTextLen, self->m_gotoLine);
            data->CursorPos = data->SelectionStart = data->SelectionEnd = at;
            self->m_gotoLine = 0;
            self->m_followCursor = true;
        }
        const int line = lineOf(data->Buf, data->CursorPos);
        if (line != self->m_cursorLine) {
            self->m_cursorLine = line;
            self->m_followCursor = true;
        }
        return 0;
    };
    const float textH = static_cast<float>(lines + 1) * lineH + style.FramePadding.y * 2.0f;
    ImGui::PushStyleColor(ImGuiCol_FrameBg, IM_COL32(0, 0, 0, 0));
    const bool edited = ImGui::InputTextMultiline("##code", &m_text, {width, std::max(textH, ImGui::GetContentRegionAvail().y)},
                                                  ImGuiInputTextFlags_AllowTabInput | ImGuiInputTextFlags_CallbackAlways, callback, this);
    ImGui::PopStyleColor();
    if (edited) {
        m_pendingCompile = true;
        m_editTime = ImGui::GetTime();
        s.shaderDirty = m_text != m_saved;
    }
    if (m_followCursor) {  // keep the cursor's line visible
        m_followCursor = false;
        const float y = style.FramePadding.y + static_cast<float>(m_cursorLine - 1) * lineH;
        const float view = ImGui::GetWindowHeight() - style.ScrollbarSize;
        if (y < ImGui::GetScrollY()) ImGui::SetScrollY(y);
        else if (y + lineH * 2.0f > ImGui::GetScrollY() + view) ImGui::SetScrollY(y + lineH * 2.0f - view);
    }
    ImGui::EndChild();
}

void ShaderEditorPanel::drawUnsavedPopup(EditorShared& s) {
    if (s.shaderPendingAction && m_after == After::None) {
        if (s.shaderDirty) {
            m_after = After::Action;
            m_openPopup = true;
        } else {  // nothing to ask
            auto action = std::move(s.shaderPendingAction);
            s.shaderPendingAction = nullptr;
            s.runGuarded(std::move(action));
        }
    }
    if (m_openPopup) {
        ImGui::OpenPopup("Shader non enregistré");
        m_openPopup = false;
    }
    if (!ImGui::BeginPopupModal("Shader non enregistré", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) return;
    ImGui::Text("Le shader %s a des modifications non enregistrées.", m_uri.c_str());
    ImGui::Spacing();
    int choice = 0;
    if (ImGui::Button("Enregistrer")) choice = 1;
    ImGui::SameLine();
    if (ImGui::Button("Ignorer les modifications")) choice = 2;
    ImGui::SameLine();
    if (ImGui::Button("Annuler") || ImGui::IsKeyPressed(ImGuiKey_Escape)) choice = 3;
    if (choice != 0) {
        if (choice == 1) save(s);
        if (choice == 2) revert(s);
        const bool proceed = choice != 3 && !s.shaderDirty;
        const After after = m_after;
        m_after = After::None;
        auto action = std::move(s.shaderPendingAction);
        s.shaderPendingAction = nullptr;
        if (proceed) {
            if (after == After::Open) open(s, m_afterUri);
            else if (after == After::Close) s.showShaderEditor = false;
            else if (after == After::Action && action) s.runGuarded(std::move(action));
        }
        ImGui::CloseCurrentPopup();
    }
    ImGui::EndPopup();
}

void ShaderEditorPanel::draw(EditorShared& s, const MaterialPreview& preview) {
    if (!s.openShaderRequest.empty()) {
        const std::string uri = std::move(s.openShaderRequest);
        s.openShaderRequest.clear();
        s.showShaderEditor = true;
        if (uri != m_uri) {
            if (s.shaderDirty) {
                m_after = After::Open;
                m_afterUri = uri;
                m_openPopup = true;
            } else {
                open(s, uri);
            }
        }
    }
    drawUnsavedPopup(s);
    if (!s.showShaderEditor) return;
    if (!s.shaderDirty && !m_uri.empty() && s.shaders()) {  // file changed outside the editor (hot reload)
        const ShaderAsset* asset = s.shaders()->get(m_uri);
        if (asset && asset->source != m_saved) {
            m_text = m_saved = asset->source;
            compile(s);
        }
    }
    if (m_pendingCompile && ImGui::GetTime() - m_editTime >= kCompileDelay) compile(s);

    bool visible = true;
    const bool shown = ImGui::Begin("Shader", &visible);
    if (!visible && s.shaderDirty) {  // closing with unsaved changes: ask first
        m_after = After::Close;
        m_openPopup = true;
    } else if (!visible) {
        s.showShaderEditor = false;
    }
    if (!shown) {
        ImGui::End();
        return;
    }
    if (m_uri.empty() || !s.shaders()) {
        ImGui::TextDisabled("Aucun shader ouvert.");
        ImGui::TextDisabled("Cliquez sur un fichier .agrshader dans les assets, ou créez-en un avec « Nouveau shader ».");
        ImGui::End();
        return;
    }
    s.shaderPreviewMaterial = kPreviewMaterial;

    // ---- toolbar
    ImGui::PushFont(s.fonts.semibold, 0.0f);
    ImGui::TextUnformatted((m_uri.substr(m_uri.find_last_of('/') + 1) + (s.shaderDirty ? " *" : "")).c_str());
    ImGui::PopFont();
    ImGui::SameLine();
    ImGui::BeginDisabled(!s.shaderDirty);
    if (ImGui::Button("Enregistrer")) save(s);
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort | ImGuiHoveredFlags_AllowWhenDisabled)) ImGui::SetTooltip("Ctrl+S quand l'éditeur de shader a le focus");
    ImGui::SameLine();
    if (ImGui::Button("Annuler les modifications")) revert(s);
    ImGui::EndDisabled();
    ImGui::SameLine();
    if (m_pendingCompile) {
        ImGui::TextDisabled("Compilation...");
    } else if (m_status.usable) {
        ImGui::TextColored(palette::toVec4(palette::kSuccess), "Compilé : aperçu en direct dans la scène");
    } else {
        ImGui::TextColored(palette::toVec4(palette::kError), "%zu erreur(s) : les matériaux utilisent le rendu standard",
                           m_status.diagnostics.size());
    }
    if (ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows) && ImGui::GetIO().KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_S, false) &&
        s.shaderDirty)
        save(s);

    // ---- code | preview + help
    if (ImGui::BeginTable("##shaderLayout", 2, ImGuiTableFlags_Resizable | ImGuiTableFlags_BordersInnerV)) {
        ImGui::TableSetupColumn("code", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupColumn("side", ImGuiTableColumnFlags_WidthFixed, 280.0f);
        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        const float errorsH = m_status.diagnostics.empty() ? 0.0f
                                                           : std::min<float>(static_cast<float>(m_status.diagnostics.size()), 5.0f) *
                                                                     ImGui::GetTextLineHeightWithSpacing() +
                                                                 ImGui::GetStyle().WindowPadding.y * 2.0f;
        const float codeH = std::max(120.0f, ImGui::GetContentRegionAvail().y - errorsH - ImGui::GetStyle().ItemSpacing.y);
        if (s.fonts.mono) ImGui::PushFont(s.fonts.mono, 0.0f);
        drawCode(s, codeH);
        if (s.fonts.mono) ImGui::PopFont();
        if (!m_status.diagnostics.empty()) {
            ImGui::BeginChild("##errors", {0, errorsH}, ImGuiChildFlags_Borders);
            for (usize i = 0; i < m_status.diagnostics.size(); ++i) {
                const ShaderDiagnostic& d = m_status.diagnostics[i];
                ImGui::PushID(static_cast<int>(i));
                ImGui::PushStyleColor(ImGuiCol_Text, palette::toVec4(palette::kError));
                const std::string label = d.line ? "Ligne " + std::to_string(d.line) + " : " + d.message : d.message;
                if (ImGui::Selectable(label.c_str()) && d.line) m_gotoLine = static_cast<int>(d.line);
                ImGui::PopStyleColor();
                if (d.line && ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort)) ImGui::SetTooltip("Aller à la ligne %u", d.line);
                ImGui::PopID();
            }
            ImGui::EndChild();
        }
        ImGui::TableNextColumn();
        if (const rhi::TextureHandle tex = preview.texture(s)) {
            const float w = std::min(ImGui::GetContentRegionAvail().x, 256.0f);
            const bool flip = s.device() && s.device()->info().renderTargetsFlipY;
            ImGui::Image(ImTextureRef(ImGuiRenderer::toImTexture(tex)), {w, w}, {0, flip ? 1.0f : 0.0f}, {1, flip ? 0.0f : 1.0f});
        }
        ImGui::TextDisabled("Aperçu : paramètres par défaut");
        ImGui::Separator();
        ImGui::TextUnformatted(kHelp);
        ImGui::EndTable();
    }
    ImGui::End();
}

} // namespace agr::editor
