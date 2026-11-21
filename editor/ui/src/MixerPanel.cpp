#include <algorithm>
#include <cmath>
#include <cstdio>
#include <string>

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

    // One column per bus: volume (vertical), mute; a click on the name shows its effects.
    const float h = std::clamp(ImGui::GetContentRegionAvail().y * 0.35f, 80.0f, 220.0f);
    for (i32 i = 0; i < kAudioBusCount; ++i) {
        const auto bus = static_cast<AudioBus>(i);
        if (!audioBusActive(bus)) continue;  // unused custom slot
        if (i) ImGui::SameLine(0, 12);
        ImGui::BeginGroup();
        ImGui::PushID(i);
        float v = audio.busVolume(bus);
        if (ImGui::VSliderFloat("##v", {44, h}, &v, 0.0f, 1.5f, "%.2f")) {
            audio.setBusVolume(bus, v);
            m_dirty = true;
        }
        bool muted = audio.busMuted(bus);
        if (ImGui::Checkbox("##m", &muted)) {
            audio.setBusMuted(bus, muted);
            m_dirty = true;
        }
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Couper");
        const usize fxCount = audio.busEffects(bus).size();
        const std::string name = std::string(audioBusName(bus)) + (fxCount ? " (" + std::to_string(fxCount) + ")" : "");
        if (ImGui::Selectable(name.c_str(), m_bus == i, 0, {60, 0})) m_bus = i;
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Effets de ce bus");
        if (audioBusIsCustom(bus)) ImGui::TextDisabled("> %s", audioBusName(audio.busParent(bus)));
        ImGui::PopID();
        ImGui::EndGroup();
    }
    // Custom buses: add one in a free slot.
    ImGui::SameLine(0, 12);
    i32 freeSlot = -1;
    for (i32 i = kAudioBuiltinBusCount; i < kAudioBusCount && freeSlot < 0; ++i)
        if (!audioBusActive(static_cast<AudioBus>(i))) freeSlot = i;
    ImGui::BeginDisabled(freeSlot < 0);
    if (ImGui::Button("+ Bus")) {
        setAudioBusName(static_cast<AudioBus>(freeSlot), "Bus" + std::to_string(freeSlot + 1));
        audio.setBusParent(static_cast<AudioBus>(freeSlot), AudioBus::Effects);
        m_bus = freeSlot;
        m_dirty = true;
    }
    ImGui::EndDisabled();
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) ImGui::SetTooltip("Nouveau bus (8 au plus), routé vers Effects");
    if (m_bus >= kAudioBuiltinBusCount && audioBusActive(static_cast<AudioBus>(m_bus))) {
        const auto bus = static_cast<AudioBus>(m_bus);
        ImGui::SeparatorText("Bus personnalisé");
        char name[64];
        std::snprintf(name, sizeof name, "%s", audioBusName(bus));
        ImGui::SetNextItemWidth(180);
        if (ImGui::InputText("Nom", name, sizeof name, ImGuiInputTextFlags_EnterReturnsTrue) && name[0]) {
            setAudioBusName(bus, name);
            m_dirty = true;
        }
        ImGui::SameLine();
        ImGui::SetNextItemWidth(160);
        if (ImGui::BeginCombo("Sortie vers", audioBusName(audio.busParent(bus)))) {
            for (i32 p = 0; p < kAudioBusCount; ++p) {
                const auto pb = static_cast<AudioBus>(p);
                if (p == m_bus || !audioBusActive(pb)) continue;
                if (ImGui::Selectable(audioBusName(pb), audio.busParent(bus) == pb)) m_dirty |= audio.setBusParent(bus, pb);
            }
            ImGui::EndCombo();
        }
        ImGui::SameLine();
        if (ImGui::Button("Supprimer le bus")) {
            for (i32 c = kAudioBuiltinBusCount; c < kAudioBusCount; ++c)  // buses routed into it go to Master
                if (audio.busParent(static_cast<AudioBus>(c)) == bus) audio.setBusParent(static_cast<AudioBus>(c), AudioBus::Master);
            audio.setBusEffects(bus, {});
            audio.setBusVolume(bus, 1.0f);
            audio.setBusMuted(bus, false);
            audio.setBusParent(bus, AudioBus::Master);
            setAudioBusName(bus, "");
            m_bus = 0;
            m_dirty = true;
        }
    }
    drawEffects(s, audio);
    ImGui::End();
}

void MixerPanel::drawEffects(EditorShared& s, AudioSystem& audio) {
    const auto bus = static_cast<AudioBus>(std::clamp(m_bus, 0, kAudioBusCount - 1));
    ImGui::SeparatorText((std::string("Effets du bus ") + audioBusName(bus)).c_str());
    std::vector<AudioEffect> list = audio.busEffects(bus);
    bool changed = false;
    int remove = -1, up = -1;
    for (usize i = 0; i < list.size(); ++i) {
        AudioEffect& e = list[i];
        ImGui::PushID(static_cast<int>(i));
        changed |= ImGui::Checkbox("##on", &e.enabled);
        ImGui::SameLine();
        const bool open = ImGui::TreeNodeEx(audioEffectName(e.type), ImGuiTreeNodeFlags_DefaultOpen);
        ImGui::SameLine(ImGui::GetContentRegionAvail().x - 60.0f);
        if (i > 0 && ImGui::SmallButton("^")) up = static_cast<int>(i);
        ImGui::SameLine();
        if (ImGui::SmallButton("x")) remove = static_cast<int>(i);
        if (open) {
            switch (e.type) {
                case AudioEffectType::LowPass:
                case AudioEffectType::HighPass:
                    changed |= ImGui::SliderFloat("Coupure (Hz)", &e.cutoff, 20.0f, 20000.0f, "%.0f", ImGuiSliderFlags_Logarithmic);
                    changed |= ImGui::SliderFloat("Résonance", &e.resonance, 0.3f, 8.0f);
                    break;
                case AudioEffectType::Echo:
                    changed |= ImGui::SliderFloat("Délai (s)", &e.delay, 0.01f, 2.0f);
                    changed |= ImGui::SliderFloat("Réinjection", &e.feedback, 0.0f, 0.95f);
                    changed |= ImGui::SliderFloat("Mélange", &e.mix, 0.0f, 1.0f);
                    break;
                case AudioEffectType::Reverb:
                    changed |= ImGui::SliderFloat("Taille de la salle", &e.roomSize, 0.0f, 1.0f);
                    changed |= ImGui::SliderFloat("Amortissement", &e.damping, 0.0f, 1.0f);
                    changed |= ImGui::SliderFloat("Mélange", &e.mix, 0.0f, 1.0f);
                    break;
                case AudioEffectType::Compressor:
                    changed |= ImGui::SliderFloat("Seuil (dB)", &e.threshold, -60.0f, 0.0f);
                    changed |= ImGui::SliderFloat("Ratio", &e.ratio, 1.0f, 30.0f);
                    changed |= ImGui::SliderFloat("Attaque (ms)", &e.attack, 0.1f, 200.0f);
                    changed |= ImGui::SliderFloat("Relâchement (ms)", &e.release, 5.0f, 2000.0f);
                    changed |= ImGui::SliderFloat("Gain (dB)", &e.gain, -12.0f, 24.0f);
                    break;
                case AudioEffectType::Distortion:
                    changed |= ImGui::SliderFloat("Attaque du signal", &e.drive, 1.0f, 50.0f);
                    changed |= ImGui::SliderFloat("Mélange", &e.mix, 0.0f, 1.0f);
                    break;
                case AudioEffectType::Chorus:
                    changed |= ImGui::SliderFloat("Vitesse (Hz)", &e.rate, 0.05f, 10.0f);
                    changed |= ImGui::SliderFloat("Profondeur (ms)", &e.depth, 0.0f, 20.0f);
                    changed |= ImGui::SliderFloat("Mélange", &e.mix, 0.0f, 1.0f);
                    break;
                case AudioEffectType::Equalizer:
                    changed |= ImGui::SliderFloat("Graves (dB)", &e.lowGain, -24.0f, 24.0f);
                    changed |= ImGui::SliderFloat("Médiums (dB)", &e.midGain, -24.0f, 24.0f);
                    changed |= ImGui::SliderFloat("Aigus (dB)", &e.highGain, -24.0f, 24.0f);
                    changed |= ImGui::SliderFloat("Fréq. graves", &e.lowFreq, 30.0f, 1000.0f, "%.0f Hz", ImGuiSliderFlags_Logarithmic);
                    changed |= ImGui::SliderFloat("Fréq. médiums", &e.midFreq, 200.0f, 8000.0f, "%.0f Hz", ImGuiSliderFlags_Logarithmic);
                    changed |= ImGui::SliderFloat("Fréq. aigus", &e.highFreq, 1000.0f, 16000.0f, "%.0f Hz", ImGuiSliderFlags_Logarithmic);
                    break;
                case AudioEffectType::Phaser:
                    changed |= ImGui::SliderFloat("Vitesse (Hz)", &e.rate, 0.05f, 10.0f);
                    changed |= ImGui::SliderFloat("Étendue (octaves)", &e.depth, 0.1f, 6.0f);
                    changed |= ImGui::SliderFloat("Réinjection", &e.feedback, 0.0f, 0.9f);
                    changed |= ImGui::SliderFloat("Mélange", &e.mix, 0.0f, 1.0f);
                    break;
                case AudioEffectType::PitchShift:
                    changed |= ImGui::SliderFloat("Hauteur (×)", &e.pitch, 0.25f, 4.0f, "%.2f", ImGuiSliderFlags_Logarithmic);
                    changed |= ImGui::SliderFloat("Mélange", &e.mix, 0.0f, 1.0f);
                    break;
                case AudioEffectType::Analyzer: {
                    const auto bands = audio.spectrum(bus, 32);
                    if (!bands.empty()) ImGui::PlotHistogram("##spectrum", bands.data(), static_cast<int>(bands.size()), 0, "spectre", 0.0f, 1.0f, {0, 60});
                    break;
                }
            }
            ImGui::TreePop();
        }
        ImGui::PopID();
    }
    if (remove >= 0) {
        list.erase(list.begin() + remove);
        changed = true;
    }
    if (up > 0) {
        std::swap(list[static_cast<usize>(up)], list[static_cast<usize>(up - 1)]);
        changed = true;
    }
    static const char* kLabels[kAudioEffectTypeCount] = {"Passe-bas", "Passe-haut", "Écho",   "Réverbération",         "Compresseur", "Distorsion",
                                                         "Chorus",    "Égaliseur",  "Phaser", "Décalage de hauteur", "Analyseur de spectre"};
    if (ImGui::BeginCombo("##add", "+ Ajouter un effet")) {
        for (u32 t = 0; t < kAudioEffectTypeCount; ++t)
            if (ImGui::Selectable(kLabels[t])) {
                AudioEffect e;
                e.type = static_cast<AudioEffectType>(t);
                if (e.type == AudioEffectType::HighPass) e.cutoff = 200.0f;
                if (e.type == AudioEffectType::Distortion) e.mix = 0.6f;
                if (e.type == AudioEffectType::Chorus) e.mix = 0.5f;
                if (e.type == AudioEffectType::Phaser) e.mix = 0.5f, e.feedback = 0.5f, e.rate = 0.4f, e.depth = 3.0f;
                if (e.type == AudioEffectType::PitchShift) e.mix = 1.0f, e.pitch = 1.5f;
                list.push_back(e);
                changed = true;
            }
        ImGui::EndCombo();
    }
    if (changed) {
        audio.setBusEffects(bus, list);  // heard at once
        m_dirty = true;
    }
    ImGui::BeginDisabled(!s.ctx.hasProject());
    if (ImGui::Button(m_dirty ? "Enregistrer le mixage*" : "Enregistrer le mixage")) {
        if (Status st = s.engine.vfs().writeText(AudioBusLayout::kUri, audio.layout().toJson()); !st) s.notify(st.error().message);
        else m_dirty = false;
    }
    ImGui::EndDisabled();
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
        ImGui::SetTooltip("Volumes, coupures et effets dans assets/audio_buses.json, chargés au lancement du jeu");
}

} // namespace agr::editor
