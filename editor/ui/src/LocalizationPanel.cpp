#include <algorithm>

#include <imgui.h>
#include <misc/cpp/imgui_stdlib.h>

#include "agr/editor/ui/Panels.hpp"
#include "agr/runtime/Localization.hpp"

namespace agr::editor {

namespace {
void save(EditorShared& s, Localization& l) {
    if (Status st = l.save(s.ctx.engine().vfs()); !st) s.notify("Enregistrement des traductions impossible : " + st.error().message);
}
bool containsCaseless(const std::string& text, const std::string& needle) {
    auto low = [](std::string v) {
        std::transform(v.begin(), v.end(), v.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        return v;
    };
    return low(text).find(low(needle)) != std::string::npos;
}
} // namespace

void LocalizationPanel::draw(EditorShared& s) {
    if (!s.showLocalization) return;
    if (!ImGui::Begin("Localisation", &s.showLocalization)) {
        ImGui::End();
        return;
    }
    Localization& l = s.ctx.engine().localization();
    ImGui::TextDisabled("assets://localization.csv : une ligne par clé, une colonne par langue (modifiable aussi dans un tableur).");

    // Toolbar: preview language, new language, new key, filter.
    ImGui::SetNextItemWidth(120);
    if (ImGui::BeginCombo("Aperçu", l.language().empty() ? "-" : l.language().c_str())) {
        for (const auto& code : l.languages())
            if (ImGui::Selectable(code.c_str(), code == l.language())) l.setLanguage(code);
        ImGui::EndCombo();
    }
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Langue des textes de la scène dans l'éditeur (le jeu choisit celle du système)");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(70);
    const bool addLang = ImGui::InputTextWithHint("##lang", "en, es...", &m_newLanguage, ImGuiInputTextFlags_EnterReturnsTrue);
    ImGui::SameLine();
    if ((addLang || ImGui::Button("+ Langue")) && !m_newLanguage.empty()) {
        if (l.addLanguage(m_newLanguage)) save(s, l);
        else s.notify("Langue déjà présente : " + m_newLanguage);
        m_newLanguage.clear();
    }
    ImGui::SameLine();
    ImGui::SetNextItemWidth(160);
    const bool addKey = ImGui::InputTextWithHint("##key", "menu.play", &m_newKey, ImGuiInputTextFlags_EnterReturnsTrue);
    ImGui::SameLine();
    if ((addKey || ImGui::Button("+ Clé")) && !m_newKey.empty()) {
        if (l.languages().empty()) l.addLanguage("fr");
        if (l.addKey(m_newKey)) save(s, l);
        else s.notify("Clé déjà présente : " + m_newKey);
        m_newKey.clear();
    }
    ImGui::SameLine();
    ImGui::SetNextItemWidth(-FLT_MIN);
    ImGui::InputTextWithHint("##filter", "Rechercher", &m_filter);

    if (l.languages().empty()) {
        ImGui::TextDisabled("Aucune langue : ajoutez-en une (par exemple « fr »), puis des clés.");
        ImGui::End();
        return;
    }
    const int columns = 2 + static_cast<int>(l.languages().size());
    if (ImGui::BeginTable("##loc", columns, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY | ImGuiTableFlags_Resizable)) {
        ImGui::TableSetupScrollFreeze(1, 1);
        ImGui::TableSetupColumn("Clé", ImGuiTableColumnFlags_WidthFixed, 160);
        for (const auto& code : l.languages()) ImGui::TableSetupColumn(code.c_str());
        ImGui::TableSetupColumn("##actions", ImGuiTableColumnFlags_WidthFixed, 24);
        ImGui::TableNextRow(ImGuiTableRowFlags_Headers);
        ImGui::TableSetColumnIndex(0);
        ImGui::TableHeader("Clé");
        for (usize c = 0; c < l.languages().size(); ++c) {
            ImGui::TableSetColumnIndex(static_cast<int>(c + 1));
            ImGui::PushID(static_cast<int>(c));
            ImGui::TableHeader(l.languages()[c].c_str());
            if (ImGui::BeginPopupContextItem("##langCtx")) {
                if (ImGui::MenuItem(("Supprimer la langue « " + l.languages()[c] + " »").c_str())) {
                    l.removeLanguage(c);
                    save(s, l);
                    ImGui::EndPopup();
                    ImGui::PopID();
                    break;
                }
                ImGui::EndPopup();
            }
            ImGui::PopID();
        }
        for (usize r = 0; r < l.keys().size(); ++r) {
            bool show = m_filter.empty() || containsCaseless(l.keys()[r], m_filter);
            for (usize c = 0; !show && c < l.languages().size(); ++c) show = containsCaseless(l.cell(r, c), m_filter);
            if (!show) continue;
            ImGui::PushID(static_cast<int>(r));
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            std::string key = l.keys()[r];
            ImGui::SetNextItemWidth(-FLT_MIN);
            if (ImGui::InputText("##k", &key, ImGuiInputTextFlags_EnterReturnsTrue)) {
                if (l.renameKey(r, key)) save(s, l);
                else s.notify("Clé vide ou déjà utilisée");
            }
            for (usize c = 0; c < l.languages().size(); ++c) {
                ImGui::TableSetColumnIndex(static_cast<int>(c + 1));
                ImGui::PushID(static_cast<int>(c));
                std::string value = l.cell(r, c);
                ImGui::SetNextItemWidth(-FLT_MIN);
                if (value.empty() && c > 0) ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.35f, 0.22f, 0.10f, 0.6f));  // missing translation
                if (ImGui::InputText("##v", &value)) l.setCell(r, c, value);
                if (value.empty() && c > 0) ImGui::PopStyleColor();
                if (ImGui::IsItemDeactivatedAfterEdit()) save(s, l);
                ImGui::PopID();
            }
            ImGui::TableSetColumnIndex(columns - 1);
            if (ImGui::SmallButton("x")) {
                l.removeKey(r);
                save(s, l);
                ImGui::PopID();
                break;
            }
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("Supprimer la clé");
            ImGui::PopID();
        }
        ImGui::EndTable();
    }
    ImGui::End();
}

} // namespace agr::editor
