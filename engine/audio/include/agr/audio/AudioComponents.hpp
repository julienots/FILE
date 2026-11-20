#pragma once
#include <string>

#include "agr/math/Math.hpp"
#include "agr/scene/Scene.hpp"

namespace agr {

/// Mixer buses. Every bus except Master is routed into Master.
enum class AudioBus : i32 { Master = 0, Music = 1, Effects = 2, Ambient = 3, Interface = 4, Voice = 5 };
enum class AudioAttenuation : i32 { None = 0, Inverse = 1, Linear = 2, Exponential = 3 };
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
    /// Spatial sources: how the volume falls with distance, the pitch shift of moving sources
    /// (Doppler, 0 = off) and a directivity cone (its forward is the entity's -Z: speakers, a
    /// character's voice); outside `coneOuterAngle` the volume is `coneOuterVolume`.
    AudioAttenuation attenuation = AudioAttenuation::Inverse;
    float rolloff = 1.0f;
    float doppler = 0.0f;
    float coneInnerAngle = 6.2832f;  ///< Radians; full circle = no cone.
    float coneOuterAngle = 6.2832f;
    float coneOuterVolume = 0.0f;
    /// Variations at each start (footsteps, impacts): another clip picked among `alternateClips`
    /// (';'-separated, with `clip`), pitch and volume randomly varied by up to these amounts.
    std::string alternateClips;
    float randomPitch = 0.0f;        ///< e.g. 0.1 = between 0.9x and 1.1x.
    float randomVolume = 0.0f;       ///< e.g. 0.2 = between 80% and 100%.
};

/// The "ears" of the scene: position and orientation used by spatial sources.
/// Without one, the primary camera is used.
struct AudioListenerComponent {
    bool active = true;
};

/// Registers the reflection data (idempotent).
void registerAudioComponents();

} // namespace agr
