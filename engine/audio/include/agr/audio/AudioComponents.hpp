#pragma once
#include <string>

#include "agr/math/Math.hpp"
#include "agr/scene/Scene.hpp"

namespace agr {

/// Mixer buses. Every bus except Master is routed into Master.
enum class AudioBus : i32 { Master = 0, Music = 1, Effects = 2, Ambient = 3, Interface = 4, Voice = 5 };
inline constexpr i32 kAudioBusCount = 6;
const char* audioBusName(AudioBus bus);
/// Bus from its name ("Music"...), case-insensitive; false if unknown.
bool audioBusFromName(std::string_view name, AudioBus& out);

/// Plays an audio clip (.wav, .ogg, .mp3, .flac) from the entity. Spatial sources are
/// attenuated and panned from the entity's world position relative to the listener
/// (AudioListener, else the primary camera).
struct AudioSourceComponent {
    std::string clip;                ///< Asset URI.
    AudioBus bus = AudioBus::Effects;
    float volume = 1.0f;
    float pitch = 1.0f;              ///< Playback speed (1 = normal).
    bool loop = false;
    bool playOnStart = true;         ///< Starts when the game starts (or the entity appears).
    bool spatial = false;            ///< 3D position: distance attenuation and panning.
    float minDistance = 1.0f;        ///< Full volume up to this distance (meters).
    float maxDistance = 30.0f;       ///< No more attenuation beyond this distance.
    bool stream = false;             ///< Decodes while playing (long music) instead of fully in memory.
};

/// The "ears" of the scene: position and orientation used by spatial sources.
/// Without one, the primary camera is used.
struct AudioListenerComponent {
    bool active = true;
};

/// Registers the reflection data (idempotent).
void registerAudioComponents();

} // namespace agr
