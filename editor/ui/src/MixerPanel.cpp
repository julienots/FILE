#include <algorithm>
#include <cmath>

#include <imgui.h>

#include "agr/editor/ui/Panels.hpp"

namespace agr::editor {

void MixerPanel::draw(EditorShared& s, const FrameTime& time) {
    if (!s.showMixer) return;
    if (!ImGui::Begin("Mixeur audio", &s.showMixer)) {
        ImGui::End();
        return;
    }
    if (!s.audio || !s.audio->audio().initialized()) {
        ImGui::TextDisabled("Audio indisponible.");
        ImGui::End();
        return;
    }
    AudioSystem& audio = s.audio->audio();
    const AudioStats& st = audio.stats();
    if (st.deviceActive) ImGui::Text("Sortie : %s, %u Hz", st.backend.c_str(), st.sampleRate);
    else ImGui::TextColored(palette::toVec4(palette::kWarning), "Aucune sortie audio : mixage silencieux (%s)", st.backend.c_str());
    ImGui::TextDisabled("%u voix, %u clips en mémoire (%.1f Mo), %u voix coupées (limite)", st.voices, st.clips,
                        static_cast<double>(st.clipBytes) / (1024.0 * 1024.0), st.voicesStolen);

    // Master meter with a falling peak.
    const AudioMeter m = audio.takeMeter();
    const float fall = static_cast<float>(time.unscaledDeltaSeconds) * 1.5f;
    m_level[0] = std::max(std::min(m.left, 1.5f), m_level[0] - fall);
    m_level[1] = std::max(std::min(m.right, 1.5f), m_level[1] - fall);
    for (int c = 0; c < 2; ++c) {
        const float v = m_level[c];
        ImGui::PushStyleColor(ImGuiCol_PlotHistogram, palette::toVec4(v > 1.0f ? palette::kError : v > 0.7f ? palette::kWarning : palette::kSuccess));
        char label[32];
        std::snprintf(label, sizeof label, "%s %.0f dB", c == 0 ? "G" : "D", v > 1e-4f ? 20.0f * std::log10(v) : -80.0f);
        ImGui::ProgressBar(std::min(v, 1.0f), {-FLT_MIN, 0}, label);
        ImGui::PopStyleColor();
    }
    ImGui::Separator();

    // One column per bus: volume (vertical), mute.
    const float h = std::max(80.0f, ImGui::GetContentRegionAvail().y - ImGui::GetFrameHeightWithSpacing() * 2.5f);
    for (i32 i = 0; i < kAudioBusCount; ++i) {
        const auto bus = static_cast<AudioBus>(i);
        if (i) ImGui::SameLine(0, 12);
        ImGui::BeginGroup();
        ImGui::PushID(i);
        float v = audio.busVolume(bus);
        if (ImGui::VSliderFloat("##v", {44, h}, &v, 0.0f, 1.5f, "%.2f")) audio.setBusVolume(bus, v);
        bool muted = audio.busMuted(bus);
        if (ImGui::Checkbox("##m", &muted)) audio.setBusMuted(bus, muted);
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Couper");
        ImGui::TextUnformatted(audioBusName(bus));
        ImGui::PopID();
        ImGui::EndGroup();
    }
    ImGui::TextDisabled("Réglages de la session ; un jeu règle ses volumes par script (agr.audio.setBusVolume).");
    ImGui::End();
}

} // namespace agr::editor
