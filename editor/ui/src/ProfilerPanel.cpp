// Profiler panel: the engine's CPU scopes (frame phases, plugins, scripts, renderer stages,
// agr.profile sections) as a tree with last / average / peak times, share of the frame, calls,
// a bar per line, a frame-time history, pause, filter and CSV export.
#include <algorithm>
#include <cstdio>
#include <filesystem>
#include <fstream>

#include <imgui.h>
#include <misc/cpp/imgui_stdlib.h>

#include "agr/core/Profiler.hpp"
#include "agr/editor/ui/Panels.hpp"
#include "agr/editor/ui/Theme.hpp"

namespace agr::editor {

void ProfilerPanel::draw(EditorShared& s) {
    Profiler& prof = Profiler::instance();
    if (!m_paused) {
        m_rows = prof.snapshot();
        m_history[m_head] = static_cast<float>(prof.lastFrameMs());
        m_head = (m_head + 1) % m_history.size();
    }
    if (!s.showProfiler) return;
    if (!ImGui::Begin("Profileur", &s.showProfiler)) {
        ImGui::End();
        return;
    }
    bool enabled = prof.enabled();
    if (ImGui::Checkbox("Mesurer", &enabled)) prof.setEnabled(enabled);
    ImGui::SameLine();
    ImGui::Checkbox("Pause", &m_paused);
    ImGui::SameLine();
    if (ImGui::Button("Réinitialiser")) {
        prof.reset();
        m_rows.clear();
    }
    ImGui::SameLine();
    ImGui::BeginDisabled(!s.ctx.hasProject());
    if (ImGui::Button("Exporter CSV")) {
        const std::filesystem::path out = s.ctx.project().root() / "profiler.csv";
        std::ofstream f(out, std::ios::binary);
        f << prof.toCsv();
        s.notify(f ? "Profil exporté : " + out.string() : "Export impossible : " + out.string());
    }
    ImGui::EndDisabled();
    ImGui::SameLine();
    ImGui::SetNextItemWidth(-FLT_MIN);
    ImGui::InputTextWithHint("##profFilter", "Filtrer (script, plugin...)", &m_filter);

    std::vector<float> ordered(m_history.size());
    float worst = 1.0f;
    for (usize i = 0; i < ordered.size(); ++i) {
        ordered[i] = m_history[(m_head + i) % m_history.size()];
        worst = std::max(worst, ordered[i]);
    }
    char overlay[64];
    std::snprintf(overlay, sizeof overlay, "image %.2f ms (pire %.1f)", ordered.back(), worst);
    ImGui::PlotHistogram("##hist", ordered.data(), static_cast<int>(ordered.size()), 0, overlay, 0.0f, std::max(33.4f, worst), {-FLT_MIN, 70});
    ImGui::TextDisabled("Repères : 16,7 ms = 60 images/s, 33,3 ms = 30 images/s. Temps CPU du fil principal.");

    f64 frame = 0.0;
    for (const Profiler::ScopeStats& r : m_rows)
        if (r.depth == 0) frame += r.avgMs;
    std::string filter = m_filter;
    std::transform(filter.begin(), filter.end(), filter.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    if (ImGui::BeginTable("##prof", 6, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY | ImGuiTableFlags_SizingStretchProp)) {
        ImGui::TableSetupScrollFreeze(0, 1);
        ImGui::TableSetupColumn("Section", ImGuiTableColumnFlags_WidthStretch, 3.0f);
        ImGui::TableSetupColumn("Image", ImGuiTableColumnFlags_WidthStretch, 0.8f);
        ImGui::TableSetupColumn("Moyenne", ImGuiTableColumnFlags_WidthStretch, 0.8f);
        ImGui::TableSetupColumn("Pic (2 s)", ImGuiTableColumnFlags_WidthStretch, 0.8f);
        ImGui::TableSetupColumn("Part", ImGuiTableColumnFlags_WidthStretch, 1.4f);
        ImGui::TableSetupColumn("Appels", ImGuiTableColumnFlags_WidthStretch, 0.6f);
        ImGui::TableHeadersRow();
        for (const Profiler::ScopeStats& r : m_rows) {
            if (!filter.empty()) {
                std::string p = r.path;
                std::transform(p.begin(), p.end(), p.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
                if (p.find(filter) == std::string::npos) continue;
            }
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            const float indent = static_cast<float>(r.depth) * 16.0f;  // Indent(0) would use the default spacing
            if (indent > 0.0f) ImGui::Indent(indent);
            const bool heavy = r.avgMs > 4.0;
            if (heavy) ImGui::TextColored(palette::toVec4(palette::kWarning), "%s", r.name.c_str());
            else ImGui::TextUnformatted(r.name.c_str());
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", r.path.c_str());
            if (indent > 0.0f) ImGui::Unindent(indent);
            ImGui::TableNextColumn();
            ImGui::Text("%.2f", r.lastMs);
            ImGui::TableNextColumn();
            ImGui::Text("%.2f", r.avgMs);
            ImGui::TableNextColumn();
            ImGui::Text("%.2f", r.maxMs);
            ImGui::TableNextColumn();
            const float share = frame > 0.0 ? static_cast<float>(r.avgMs / frame) : 0.0f;
            char label[16];
            std::snprintf(label, sizeof label, "%.0f %%", share * 100.0f);
            ImGui::ProgressBar(std::clamp(share, 0.0f, 1.0f), {-FLT_MIN, 0}, label);
            ImGui::TableNextColumn();
            ImGui::Text("%u", r.calls);
        }
        ImGui::EndTable();
    }
    ImGui::End();
}

} // namespace agr::editor
