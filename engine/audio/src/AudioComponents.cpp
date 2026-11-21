#include "agr/audio/AudioComponents.hpp"

#include <cctype>
#include <string>
#include <vector>

#include "agr/scene/Reflection.hpp"

namespace agr {

namespace {
constexpr const char* kBuiltinNames[kAudioBuiltinBusCount] = {"Master", "Music", "Effects", "Ambient", "Interface", "Voice"};
std::string g_customNames[kAudioBusCount - kAudioBuiltinBusCount];
std::string g_slotNames[kAudioBusCount - kAudioBuiltinBusCount];  // "Bus7".."Bus14"

const std::string& slotName(i32 i) {
    std::string& s = g_slotNames[i - kAudioBuiltinBusCount];
    if (s.empty()) s = "Bus" + std::to_string(i + 1);
    return s;
}

std::vector<EnumEntry> busEntries() {
    std::vector<EnumEntry> buses;
    for (i32 i = 0; i < kAudioBusCount; ++i)
        if (audioBusActive(static_cast<AudioBus>(i))) buses.push_back({audioBusName(static_cast<AudioBus>(i)), i});
    return buses;
}
}  // namespace

const char* audioBusName(AudioBus bus) {
    const i32 i = static_cast<i32>(bus);
    if (i < 0 || i >= kAudioBusCount) return "?";
    if (i < kAudioBuiltinBusCount) return kBuiltinNames[i];
    const std::string& custom = g_customNames[i - kAudioBuiltinBusCount];
    return custom.empty() ? slotName(i).c_str() : custom.c_str();
}

bool audioBusActive(AudioBus bus) {
    const i32 i = static_cast<i32>(bus);
    if (i < 0 || i >= kAudioBusCount) return false;
    return i < kAudioBuiltinBusCount || !g_customNames[i - kAudioBuiltinBusCount].empty();
}

void setAudioBusName(AudioBus bus, std::string_view name) {
    const i32 i = static_cast<i32>(bus);
    if (i < kAudioBuiltinBusCount || i >= kAudioBusCount) return;
    std::string clean;
    for (char c : name)
        if (std::isalnum(static_cast<unsigned char>(c)) || c == '_' || c == '-' || c == ' ') clean += c;
    AudioBus existing;
    if (!clean.empty() && audioBusFromName(clean, existing) && existing != bus) clean += "_" + std::to_string(i + 1);  // keep names unique
    g_customNames[i - kAudioBuiltinBusCount] = clean;
    // The AudioSource "bus" list in the inspector follows the names.
    if (const ComponentInfo* info = ComponentRegistry::instance().find<AudioSourceComponent>())
        for (const FieldInfo& f : info->fields)
            if (f.name == "bus") const_cast<FieldInfo&>(f).enumEntries = busEntries();
}

bool audioBusFromName(std::string_view name, AudioBus& out) {
    for (i32 i = 0; i < kAudioBusCount; ++i) {
        const std::string_view candidate = audioBusName(static_cast<AudioBus>(i));
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
    std::vector<EnumEntry> buses = busEntries();
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
        .field("stream", &AudioSourceComponent::stream, {.tooltip = "Decode while playing (long music): less memory"})
        .field("attenuation", &AudioSourceComponent::attenuation,
               {.tooltip = "How the volume falls with distance (spatial)", .enumEntries = {{"None", 0}, {"Inverse", 1}, {"Linear", 2}, {"Exponential", 3}}})
        .field("rolloff", &AudioSourceComponent::rolloff, {.rangeMin = 0.0f, .rangeMax = 20.0f, .speed = 0.01f})
        .field("doppler", &AudioSourceComponent::doppler, {.tooltip = "Pitch shift of moving sources (0 = off, 1 = real)", .rangeMin = 0.0f, .rangeMax = 10.0f, .speed = 0.01f})
        .field("coneInnerAngle", &AudioSourceComponent::coneInnerAngle, {.tooltip = "Full volume inside (forward = -Z)", .flags = FieldFlag_Angle, .rangeMin = 0.0f, .rangeMax = 6.2832f})
        .field("coneOuterAngle", &AudioSourceComponent::coneOuterAngle, {.flags = FieldFlag_Angle, .rangeMin = 0.0f, .rangeMax = 6.2832f})
        .field("coneOuterVolume", &AudioSourceComponent::coneOuterVolume, {.tooltip = "Volume outside the outer angle", .rangeMin = 0.0f, .rangeMax = 1.0f, .speed = 0.01f})
        .field("alternateClips", &AudioSourceComponent::alternateClips, {.tooltip = "Other clips picked at random (';'-separated)"})
        .field("randomPitch", &AudioSourceComponent::randomPitch, {.tooltip = "Pitch varies by up to this (0.1 = 0.9x .. 1.1x)", .rangeMin = 0.0f, .rangeMax = 1.0f, .speed = 0.005f})
        .field("randomVolume", &AudioSourceComponent::randomVolume, {.tooltip = "Volume lowered by up to this (0.2 = 80% .. 100%)", .rangeMin = 0.0f, .rangeMax = 1.0f, .speed = 0.005f});
    reg.add<AudioListenerComponent>("AudioListener")
        .category("Audio")
        .description("Position and orientation from which spatial sounds are heard (default: the primary camera).")
        .field("active", &AudioListenerComponent::active);
}

} // namespace agr
