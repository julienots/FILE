#pragma once
#include <string>

#include "agr/math/Math.hpp"
#include "agr/scene/Scene.hpp"

namespace agr {

/// Mixer buses. Every bus except Master is routed into Master.
enum class AudioBus : i32 { Master = 0, Music = 1, Effects = 2, Ambient = 3, Interface = 4, Voice = 5 };
enum class AudioAttenuation : i32 { None = 0, Inverse = 1, Linear = 2, Exponential = 3 };
inline constexpr i32 kAudioBuiltinBusCount = 6;
/// Built-in buses then 8 custom slots (named by the project: "Footsteps", "Weapons"...).
inline constexpr i32 kAudioBusCount = kAudioBuiltinBusCount + 8;
inline constexpr bool audioBusIsCustom(AudioBus bus) { return static_cast<i32>(bus) >= kAudioBuiltinBusCount; }
/// Name of a bus: the built-in name, a custom slot's name, or "Bus7".."Bus14" for an unnamed slot.
const char* audioBusName(AudioBus bus);
/// Names a custom slot (empty = unused); ignored for built-in buses. Updates the inspector list.
void setAudioBusName(AudioBus bus, std::string_view name);
/// Built-in, or a custom slot that has a name.
bool audioBusActive(AudioBus bus);
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
    bool occlusion = false;          ///< Spatial: muffled and quieter when a 3D collider is between it and the listener.
    float occlusionStrength = 0.8f;  ///< 0..1: how much a wall removes (highs and volume).
    float randomVolume = 0.0f;       ///< e.g. 0.2 = between 80% and 100%.
};

/// The "ears" of the scene: position and orientation used by spatial sources.
/// Without one, the primary camera is used.
struct AudioListenerComponent {
    bool active = true;
};

enum class AudioZoneShape : i32 { Box = 0, Sphere = 1 };
enum class AudioZoneMode : i32 { Sources = 0, Listener = 1 };
/// A volume of space that re-routes spatial sources to another bus (a "Cave" bus with a reverb,
/// an "Underwater" bus with a low-pass...). Mode `Sources`: the sources inside the zone play on
/// `bus`. Mode `Listener`: while the listener is inside, every spatial source plays on `bus` (the
/// whole world sounds like a cathedral once you enter it). The zone follows the entity's world
/// transform (position, rotation, scale); the highest `priority` wins where zones overlap.
/// Sources on the Interface or Music bus are never re-routed.
struct AudioZoneComponent {
    AudioZoneShape shape = AudioZoneShape::Box;
    math::Vec3 size{10.0f, 4.0f, 10.0f};  ///< Box: full size (before scale).
    float radius = 5.0f;                  ///< Sphere (before scale).
    AudioBus bus = AudioBus::Effects;
    AudioZoneMode mode = AudioZoneMode::Sources;
    i32 priority = 0;
};

/// Registers the reflection data (idempotent).
void registerAudioComponents();

} // namespace agr
