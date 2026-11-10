#include "agr/audio/AudioComponents.hpp"

#include <cctype>

#include "agr/scene/Reflection.hpp"

namespace agr {

namespace {
constexpr const char* kBusNames[kAudioBusCount] = {"Master", "Music", "Effects", "Ambient", "Interface", "Voice"};
}

const char* audioBusName(AudioBus bus) {
    const i32 i = static_cast<i32>(bus);
    return i >= 0 && i < kAudioBusCount ? kBusNames[i] : "?";
}

bool audioBusFromName(std::string_view name, AudioBus& out) {
    for (i32 i = 0; i < kAudioBusCount; ++i) {
        const std::string_view candidate = kBusNames[i];
        if (candidate.size() != name.size()) continue;
        bool same = true;
        for (usize c = 0; c < name.size() && same; ++c)
            same = std::tolower(static_cast<unsigned char>(name[c])) == std::tolower(static_cast<unsigned char>(candidate[c]));
        if (same) {
            out = static_cast<AudioBus>(i);
            return true;
        }
    }
    return false;
}

void registerAudioComponents() {
    auto& reg = ComponentRegistry::instance();
    if (reg.find<AudioSourceComponent>()) return;
    std::vector<EnumEntry> buses;
    for (i32 i = 0; i < kAudioBusCount; ++i) buses.push_back({kBusNames[i], i});
    reg.add<AudioSourceComponent>("AudioSource")
        .category("Audio")
        .description("Plays a sound or music (.wav, .ogg, .mp3, .flac), optionally positioned in space.")
        .field("clip", &AudioSourceComponent::clip, {.tooltip = "Audio file, e.g. assets://audio/jump.wav", .flags = FieldFlag_AssetPath})
        .field("bus", &AudioSourceComponent::bus, {.tooltip = "Mixer bus (volume groups)", .enumEntries = buses})
        .field("volume", &AudioSourceComponent::volume, {.rangeMin = 0.0f, .rangeMax = 2.0f, .speed = 0.01f})
        .field("pitch", &AudioSourceComponent::pitch, {.tooltip = "Playback speed", .rangeMin = 0.1f, .rangeMax = 4.0f, .speed = 0.01f})
        .field("loop", &AudioSourceComponent::loop)
        .field("playOnStart", &AudioSourceComponent::playOnStart, {.tooltip = "Starts with the game"})
        .field("spatial", &AudioSourceComponent::spatial, {.tooltip = "Attenuated and panned from its position"})
        .field("minDistance", &AudioSourceComponent::minDistance, {.rangeMin = 0.01f, .rangeMax = 1000.0f, .speed = 0.05f})
        .field("maxDistance", &AudioSourceComponent::maxDistance, {.rangeMin = 0.01f, .rangeMax = 10000.0f, .speed = 0.1f})
        .field("stream", &AudioSourceComponent::stream, {.tooltip = "Decode while playing (long music): less memory"});
    reg.add<AudioListenerComponent>("AudioListener")
        .category("Audio")
        .description("Position and orientation from which spatial sounds are heard (default: the primary camera).")
        .field("active", &AudioListenerComponent::active);
}

} // namespace agr
