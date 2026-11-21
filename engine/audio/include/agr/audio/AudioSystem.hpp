#pragma once
#include <memory>
#include <string>
#include <string_view>

#include "agr/audio/AudioComponents.hpp"
#include "agr/audio/AudioEffects.hpp"
#include "agr/core/FileSystem.hpp"
#include "agr/core/Result.hpp"

namespace agr {

enum class AudioOutput : u8 {
    Device,   ///< The system's audio output (falls back to Virtual when none is available).
    Virtual,  ///< No output; update() advances the mix in real time (sounds still end on time).
    Manual,   ///< No output; the caller pulls samples with mix() (tests, offline rendering).
};

struct AudioConfig {
    AudioOutput output = AudioOutput::Device;
    u32 sampleRate = 48000;
    u32 maxVoices = 64;  ///< Beyond this, the oldest non-looping voice is stopped.
};

struct PlayParams {
    AudioBus bus = AudioBus::Effects;
    float volume = 1.0f;
    float pitch = 1.0f;
    float pan = 0.0f;      ///< -1 left .. 1 right (non-spatial voices).
    bool loop = false;
    bool stream = false;
    bool spatial = false;
    math::Vec3 position{0.0f};
    float minDistance = 1.0f;
    float maxDistance = 30.0f;
    float fadeInSeconds = 0.0f;
    AudioAttenuation attenuation = AudioAttenuation::Inverse;
    float rolloff = 1.0f;
    float doppler = 0.0f;
    float coneInner = 6.2832f, coneOuter = 6.2832f, coneOuterVolume = 0.0f;  ///< Radians.
    math::Vec3 direction{0.0f, 0.0f, -1.0f};  ///< Cone axis (spatial).
};

/// 0 = no voice.
using VoiceId = u64;

struct AudioStats {
    bool deviceActive = false;
    std::string backend;      ///< "PulseAudio", "AAudio"... or "virtual" / "manual".
    u32 sampleRate = 0;
    u32 voices = 0;
    u32 voicesStolen = 0;     ///< Since start.
    u32 clips = 0;            ///< Decoded clips in memory.
    u64 clipBytes = 0;
    u64 framesMixed = 0;
};

/// Peak levels of the master output since the previous call (0..1+).
struct AudioMeter {
    float left = 0.0f, right = 0.0f;
};

/// Audio mixer built on miniaudio: decoded clips shared between voices, buses
/// (Master <- Music, Effects, Ambient, Interface, Voice) with volume and mute,
/// spatial voices (distance attenuation, panning) relative to a listener, and
/// AudioSource components of a scene. Main thread only; mixing runs on the
/// audio device thread.
class AudioSystem {
public:
    explicit AudioSystem(VirtualFileSystem& vfs);
    ~AudioSystem();
    AudioSystem(const AudioSystem&) = delete;
    AudioSystem& operator=(const AudioSystem&) = delete;

    /// Opens the output. Never fails because of a missing audio device: falls back to Virtual.
    Status initialize(const AudioConfig& config = {});
    void shutdown();
    bool initialized() const;

    // ------------------------------------------------------------------ voices
    Result<VoiceId> play(std::string_view uri, const PlayParams& params = {});
    void stop(VoiceId voice, float fadeOutSeconds = 0.0f);
    void setPaused(VoiceId voice, bool paused);
    bool isPlaying(VoiceId voice) const;  ///< true while playing or paused
    void setVolume(VoiceId voice, float volume);
    void setPitch(VoiceId voice, float pitch);
    void setPosition(VoiceId voice, math::Vec3 position);
    void stopAll();
    /// Procedural sound (synthesizers, computed effects): a voice playing the mono samples pushed
    /// with pushGenerator (at `sampleRate`), silence when its queue is empty. stop() ends it.
    Result<VoiceId> playGenerator(const PlayParams& params = {}, u32 sampleRate = 44100);
    /// Appends samples (-1..1); returns how many fit (the queue holds 2 s at most).
    u32 pushGenerator(VoiceId voice, const float* samples, u32 count);
    /// Samples waiting to be played (keep it above a frame's worth to avoid gaps).
    u32 generatorQueued(VoiceId voice) const;

    // -------------------------------------------------------------------- mixer
    void setBusVolume(AudioBus bus, float volume);
    float busVolume(AudioBus bus) const;
    void setBusMuted(AudioBus bus, bool muted);
    bool busMuted(AudioBus bus) const;
    void setListener(math::Vec3 position, math::Vec3 forward, math::Vec3 up = {0.0f, 1.0f, 0.0f});
    /// Effects of a bus, applied in order to everything it mixes (reverb on Effects, low-pass
    /// on Music when paused, compressor on Master...). Safe while sounds play.
    void setBusEffects(AudioBus bus, std::vector<AudioEffect> effects);
    const std::vector<AudioEffect>& busEffects(AudioBus bus) const;
    /// Spectrum of what the bus mixes, measured by an Analyzer effect on it (`bands` levels 0..1,
    /// log-spaced 40 Hz .. 16 kHz); empty without one. For visualizers and rhythm games.
    std::vector<float> spectrum(AudioBus bus, u32 bands) const;
    /// Volumes, mutes and effects of every bus at once (assets://audio_buses.json).
    void applyLayout(const AudioBusLayout& layout);
    AudioBusLayout layout() const;
    /// Suspends the output device (application in background) or resumes it.
    void suspend(bool suspended);

    // ------------------------------------------------------------------- frame
    /// Releases finished voices; in Virtual mode advances the mix by `deltaSeconds`.
    void update(f64 deltaSeconds);
    /// Manual / Virtual mode: renders `frames` stereo float frames (interleaved) into `out`.
    void mix(float* out, u32 frames);

    // ------------------------------------------------------------------- clips
    Status preload(std::string_view uri);
    /// Frees decoded clips no voice uses.
    void unloadUnused();
    /// Duration of a clip in seconds (decodes it if needed).
    Result<f64> clipDuration(std::string_view uri);

    // ------------------------------------------------------------------- scene
    /// Drives the scene's AudioSource components: starts playOnStart sources while
    /// `running`, follows positions and parameter changes, stops the voices of removed
    /// or inactive sources, and places the listener. With running = false every
    /// source voice stops (editor edit mode). Detects scene reloads.
    void updateScene(Scene& scene, bool running);
    VoiceId playSource(Scene& scene, ecs::Entity entity);
    void stopSource(ecs::Entity entity, float fadeOutSeconds = 0.0f);
    /// Game pause: pauses (or resumes) every voice except those of the Interface bus.
    void pauseSources(bool paused);
    bool sourcePlaying(ecs::Entity entity) const;
    /// Stops the voices of the scene sources (game stopped, scene changed).
    void resetScene();

    // ---------------------------------------------------------------- recording
    /// Starts recording the microphone (mono, the mixer's rate). Without a capture device (or in
    /// Virtual / Manual output), samples come from feedRecording (tests, custom sources). False if
    /// already recording.
    bool startRecording();
    bool isRecording() const;
    /// Level of the last ~50 ms recorded (RMS, 0..1): voice meters, "blow to play".
    float recordingLevel() const;
    /// Stops and writes the recording as a 16-bit WAV to `uri` (e.g. "user://voice.wav"), playable
    /// with play(uri). Returns the duration in seconds.
    Result<f64> stopRecording(const std::string& uri);
    /// Appends samples to the recording (mono floats), when recording.
    void feedRecording(const float* samples, u32 count);
    /// Whether a real microphone is being captured (false: fed samples only).
    bool recordingFromDevice() const;

    const AudioStats& stats() const;
    AudioMeter takeMeter();

private:
    struct Impl;
    std::unique_ptr<Impl> m;
};

} // namespace agr
