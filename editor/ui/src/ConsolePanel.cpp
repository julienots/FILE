#include <algorithm>
#include <cctype>

#include <imgui.h>
#include <imgui_stdlib.h>

#include "agr/core/Memory.hpp"
#include "agr/editor/ui/Panels.hpp"

namespace agr::editor {

void ConsolePanel::draw(EditorShared& s) {
    if (!ImGui::Begin("Console")) {
        ImGui::End();
        return;
    }
    const auto& sink = s.engine.consoleLog();
    if (sink && sink->totalWritten() != m_seen) {  // copy only when something new was logged
        m_seen = sink->totalWritten();
        m_entries = sink->snapshot();
    }
    usize counts[4] = {};
    for (const auto& e : m_entries) {
        if (e.level == LogLevel::Debug || e.level == LogLevel::Trace) ++counts[0];
        else if (e.level == LogLevel::Info) ++counts[1];
        else if (e.level == LogLevel::Warning) ++counts[2];
        else ++counts[3];
    }
    if (ImGui::Button("Effacer") && sink) sink->clear();
    ImGui::SameLine();
    auto toggle = [](const char* label, bool& v, usize n, ImU32 color) {
        ImGui::PushStyleColor(ImGuiCol_Text, v ? palette::toVec4(color) : palette::toVec4(palette::kTextMuted));
        char text[64];
        std::snprintf(text, sizeof text, "%s %zu", label, n);
        if (ImGui::Button(text)) v = !v;
        ImGui::PopStyleColor();
        ImGui::SameLine();
    };
    toggle("Debug", m_showDebug, counts[0], palette::kTextMuted);
    toggle("Info", m_showInfo, counts[1], palette::kText);
    toggle("Avertissements", m_showWarn, counts[2], palette::kWarning);
    toggle("Erreurs", m_showError, counts[3], palette::kError);
    ImGui::Checkbox("Défilement auto", &m_autoScroll);
    ImGui::SameLine();
    ImGui::SetNextItemWidth(-FLT_MIN);
    ImGui::InputTextWithHint("##filter", "Filtrer...", &m_filter);

    ImGui::BeginChild("##log", {0, 0}, ImGuiChildFlags_Borders, ImGuiWindowFlags_HorizontalScrollbar);
    ImGui::PushFont(s.fonts.mono, 0.0f);
    std::string needle = m_filter;
    std::transform(needle.begin(), needle.end(), needle.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    for (usize index = 0; index < m_entries.size(); ++index) {
        const auto& e = m_entries[index];
        const bool debug = e.level == LogLevel::Debug || e.level == LogLevel::Trace;
        if ((debug && !m_showDebug) || (e.level == LogLevel::Info && !m_showInfo) || (e.level == LogLevel::Warning && !m_showWarn) ||
            (e.level >= LogLevel::Error && !m_showError))
            continue;
        if (!needle.empty()) {
            std::string hay = e.channel + " " + e.text;
            std::transform(hay.begin(), hay.end(), hay.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
            if (hay.find(needle) == std::string::npos) continue;
        }
        const ImU32 color = e.level >= LogLevel::Error ? palette::kError
                            : e.level == LogLevel::Warning ? palette::kWarning
                            : debug                         ? palette::kTextMuted
                                                            : palette::kText;
        ImGui::PushStyleColor(ImGuiCol_Text, palette::toVec4(color));
        ImGui::Text("%8.2f  %-8s %s", e.timeSeconds, e.channel.c_str(), e.text.c_str());
        ImGui::PopStyleColor();
        ImGui::PushID(static_cast<int>(index));
        if (ImGui::BeginPopupContextItem("##line")) {  // right click a line to copy it
            if (ImGui::MenuItem("Copier")) ImGui::SetClipboardText((e.channel + ": " + e.text).c_str());
            ImGui::EndPopup();
        }
        ImGui::PopID();
    }
    ImGui::PopFont();
    if (m_autoScroll && ImGui::GetScrollY() >= ImGui::GetScrollMaxY() - 4.0f) ImGui::SetScrollHereY(1.0f);
    ImGui::EndChild();
    ImGui::End();
}

void StatsPanel::draw(EditorShared& s, const FrameTime& time) {
    m_frameMs[m_head] = static_cast<float>(time.unscaledDeltaSeconds * 1000.0);
    m_head = (m_head + 1) % m_frameMs.size();
    if (!ImGui::Begin("Statistiques")) {
        ImGui::End();
        return;
    }
    const FrameStats& fs = s.engine.stats();
    ImGui::Text("%.0f FPS   image %.2f ms   CPU moteur %.2f ms", fs.fps, fs.frameMs, fs.cpuMs);
    std::vector<float> ordered(m_frameMs.size());
    for (usize i = 0; i < ordered.size(); ++i) ordered[i] = m_frameMs[(m_head + i) % m_frameMs.size()];
    ImGui::PlotLines("##frames", ordered.data(), static_cast<int>(ordered.size()), 0, "temps d'image (ms)", 0.0f, 40.0f,
                     {-FLT_MIN, 60});
    if (rhi::Device* d = s.device()) {
        const auto& info = d->info();
        ImGui::SeparatorText("GPU");
        ImGui::Text("%s  —  %s", rhi::enumName(info.backend), info.renderer.c_str());
        ImGui::TextDisabled("%s", info.apiVersion.c_str());
        if (Renderer* r = s.renderer()) {
            const auto& st = r->stats();
            ImGui::Text("Draw calls %u   triangles %llu", st.drawCalls, static_cast<unsigned long long>(st.triangles));
            ImGui::Text("Maillages %u (+%u éliminés)   sprites %u en %u lot(s)", st.meshesVisible, st.meshesCulled, st.spritesVisible,
                        st.spriteBatches);
            ImGui::Text("Rendu %ux%u  MSAA x%u   profil « %s »", st.renderWidth, st.renderHeight, st.msaaSamples, r->profile().name);
            if (st.gpuTimeMs >= 0.0) ImGui::Text("Temps GPU %.2f ms   attente GPU %.2f ms", st.gpuTimeMs, st.gpuWaitMs);
            else ImGui::TextDisabled("Temps GPU indisponible sur ce pilote");
        }
        const auto mem = d->memoryStats();
        ImGui::Text("Mémoire GPU : buffers %.1f Mo, textures %.1f Mo", static_cast<double>(mem.bufferBytes) / 1048576.0,
                    static_cast<double>(mem.textureBytes) / 1048576.0);
    }
    if (AssetManager* am = s.assets()) {
        const auto st = am->stats();
        ImGui::SeparatorText("Ressources");
        ImGui::Text("Textures %u (en chargement %u, en erreur %u)", st.textures, st.loading, st.failed);
    }
    ImGui::SeparatorText("Mémoire CPU suivie");
    for (int t = 0; t < static_cast<int>(MemoryTag::Count); ++t) {
        const auto tag = static_cast<MemoryTag>(t);
        if (MemoryStats::currentBytes(tag) == 0) continue;
        ImGui::Text("%-10s %8.1f Ko (pic %.1f Ko)", enumName(tag), static_cast<double>(MemoryStats::currentBytes(tag)) / 1024.0,
                    static_cast<double>(MemoryStats::peakBytes(tag)) / 1024.0);
    }
    ImGui::SeparatorText("Scène");
    ImGui::Text("%zu entités", s.ctx.scene().entityCount());
    ImGui::End();
}

} // namespace agr::editor
