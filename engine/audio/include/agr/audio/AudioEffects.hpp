#pragma once
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "agr/audio/AudioComponents.hpp"
#include "agr/core/Result.hpp"

namespace agr {

enum class AudioEffectType : u8 {
    LowPass = 0, HighPass = 1, Echo = 2, Reverb = 3, Compressor = 4, Distortion = 5, Chorus = 6,
    Equalizer = 7,   ///< 3 bands: low shelf, middle peak, high shelf (gains in dB).
    Phaser = 8,      ///< Sweeping notches (4 all-pass stages moved by an LFO).
    PitchShift = 9,  ///< Changes the pitch without changing the speed (`pitch`: 2 = an octave up).
    Analyzer = 10,   ///< Leaves the sound unchanged; measures its spectrum (AudioSystem::spectrum).
};
inline constexpr u32 kAudioEffectTypeCount = 11;
const char* audioEffectName(AudioEffectType type);  ///< "LowPass"...
bool audioEffectFromName(std::string_view name, AudioEffectType& out);

/// One effect of a bus. Each type reads its own fields:
///   LowPass / HighPass: cutoff (Hz), resonance (Q, 0.707 = flat)
///   Echo: delay (s), feedback (0..0.95), mix
///   Reverb: roomSize (0..1), damping (0..1), mix
///   Compressor: threshold (dB), ratio, attack / release (ms), gain (dB, make-up)
///   Distortion: drive (1..50), mix
///   Chorus: rate (Hz), depth (ms), mix
/// `mix` is the wet amount (0 = dry only, 1 = wet only).
struct AudioEffect {
    AudioEffectType type = AudioEffectType::Reverb;
    bool enabled = true;
    float cutoff = 1000.0f, resonance = 0.707f;
    float delay = 0.3f, feedback = 0.4f;
    float roomSize = 0.6f, damping = 0.5f;
    float threshold = -18.0f, ratio = 4.0f, attack = 10.0f, release = 150.0f, gain = 0.0f;
    float drive = 6.0f;
    float rate = 0.8f, depth = 3.0f;
    float mix = 0.35f;
    float lowGain = 0.0f, midGain = 0.0f, highGain = 0.0f;  ///< Equalizer, dB.
    float lowFreq = 200.0f, midFreq = 1000.0f, highFreq = 5000.0f;
    float pitch = 1.0f;  ///< PitchShift ratio (0.5 .. 2).
};

/// Stereo effect chain processed in place on interleaved float frames (the audio thread of a
/// bus; also usable offline). Changing the list keeps the state of effects whose type did not
/// change (no click when a parameter moves).
class AudioEffectChain {
public:
    AudioEffectChain();
    ~AudioEffectChain();
    void configure(const std::vector<AudioEffect>& effects, u32 sampleRate);
    void process(float* frames, u32 count);  ///< Stereo, interleaved.
    /// Forgets delays, reverb tails and envelopes.
    void reset();
    /// Spectrum measured by the chain's first Analyzer: `bands` levels (0..1, log-spaced from
    /// 40 Hz to 16 kHz). Empty without an Analyzer. Safe from any thread.
    std::vector<float> spectrum(u32 bands) const;

private:
    struct Impl;
    std::unique_ptr<Impl> m;
};

/// Volumes, mutes and effects of every bus (assets://audio_buses.json), loaded by the audio
/// plugin at start and edited in the mixer.
struct AudioBusLayout {
    AudioBusLayout() {
        for (float& v : volume) v = 1.0f;
    }
    float volume[kAudioBusCount];
    bool muted[kAudioBusCount]{};
    std::vector<AudioEffect> effects[kAudioBusCount];
    /// Custom slots: name (empty = unused) and the bus they feed (Master by default).
    std::string names[kAudioBusCount];
    i32 parent[kAudioBusCount]{};
    static constexpr const char* kUri = "assets://audio_buses.json";
    std::string toJson() const;
    static Result<AudioBusLayout> fromJson(std::string_view json);
};

} // namespace agr
