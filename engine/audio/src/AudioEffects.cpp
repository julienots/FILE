#include "agr/audio/AudioEffects.hpp"

#include <algorithm>
#include <array>
#include <vector>
#include <memory>
#include <atomic>
#include <cmath>

#include <nlohmann/json.hpp>

namespace agr {

namespace {

constexpr float kPi = 3.14159265358979f;
constexpr const char* kNames[kAudioEffectTypeCount] = {"LowPass", "HighPass", "Echo",      "Reverb",     "Compressor", "Distortion",
                                                       "Chorus",  "Equalizer", "Phaser", "PitchShift", "Analyzer"};
constexpr usize kFftSize = 2048;

float dbToGain(float db) { return std::pow(10.0f, db / 20.0f); }

/// RBJ biquad (direct form I), one per channel.
struct Biquad {
    float b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0;
    float x1[2]{}, x2[2]{}, y1[2]{}, y2[2]{};
    void design(bool high, float cutoff, float q, float rate) {
        const float w = 2.0f * kPi * std::clamp(cutoff, 10.0f, rate * 0.45f) / rate;
        const float alpha = std::sin(w) / (2.0f * std::max(q, 0.05f)), c = std::cos(w);
        const float a0 = 1.0f + alpha;
        if (high) {
            b0 = (1.0f + c) * 0.5f / a0, b1 = -(1.0f + c) / a0, b2 = b0;
        } else {
            b0 = (1.0f - c) * 0.5f / a0, b1 = (1.0f - c) / a0, b2 = b0;
        }
        a1 = -2.0f * c / a0, a2 = (1.0f - alpha) / a0;
    }
    /// RBJ shelves (kind 0 low, 2 high) and peak (kind 1); gain in dB.
    void designEq(int kind, float freq, float gainDb, float rate) {
        const float A = std::pow(10.0f, gainDb / 40.0f);
        const float w = 2.0f * kPi * std::clamp(freq, 10.0f, rate * 0.45f) / rate, c = std::cos(w), sn = std::sin(w);
        if (kind == 1) {
            const float alpha = sn / (2.0f * 0.9f);
            const float a0 = 1.0f + alpha / A;
            b0 = (1.0f + alpha * A) / a0, b1 = -2.0f * c / a0, b2 = (1.0f - alpha * A) / a0;
            a1 = -2.0f * c / a0, a2 = (1.0f - alpha / A) / a0;
            return;
        }
        const float alpha = sn / 2.0f * std::sqrt(2.0f), sq = 2.0f * std::sqrt(A) * alpha;
        if (kind == 0) {
            const float a0 = (A + 1) + (A - 1) * c + sq;
            b0 = A * ((A + 1) - (A - 1) * c + sq) / a0, b1 = 2 * A * ((A - 1) - (A + 1) * c) / a0, b2 = A * ((A + 1) - (A - 1) * c - sq) / a0;
            a1 = -2 * ((A - 1) + (A + 1) * c) / a0, a2 = ((A + 1) + (A - 1) * c - sq) / a0;
        } else {
            const float a0 = (A + 1) - (A - 1) * c + sq;
            b0 = A * ((A + 1) + (A - 1) * c + sq) / a0, b1 = -2 * A * ((A - 1) + (A + 1) * c) / a0, b2 = A * ((A + 1) + (A - 1) * c - sq) / a0;
            a1 = 2 * ((A - 1) - (A + 1) * c) / a0, a2 = ((A + 1) - (A - 1) * c - sq) / a0;
        }
    }
    float run(int ch, float x) {
        const float y = b0 * x + b1 * x1[ch] + b2 * x2[ch] - a1 * y1[ch] - a2 * y2[ch];
        x2[ch] = x1[ch], x1[ch] = x, y2[ch] = y1[ch], y1[ch] = y;
        return y;
    }
};

/// Freeverb-style comb with a damping low-pass in the feedback.
struct Comb {
    std::vector<float> buf;
    usize pos = 0;
    float store = 0.0f;
    float run(float x, float feedback, float damp) {
        const float out = buf[pos];
        store = out * (1.0f - damp) + store * damp;
        buf[pos] = x + store * feedback;
        pos = (pos + 1) % buf.size();
        return out;
    }
};
struct Allpass {
    std::vector<float> buf;
    usize pos = 0;
    float run(float x) {
        const float b = buf[pos];
        const float out = b - x;
        buf[pos] = x + b * 0.5f;
        pos = (pos + 1) % buf.size();
        return out;
    }
};

/// In-place radix-2 FFT (real input in re, im = 0).
void fft(std::vector<float>& re, std::vector<float>& im) {
    const usize n = re.size();
    for (usize i = 1, j = 0; i < n; ++i) {
        usize bit = n >> 1;
        for (; j & bit; bit >>= 1) j ^= bit;
        j ^= bit;
        if (i < j) std::swap(re[i], re[j]), std::swap(im[i], im[j]);
    }
    for (usize len = 2; len <= n; len <<= 1) {
        const float ang = -2.0f * kPi / static_cast<float>(len);
        for (usize i = 0; i < n; i += len)
            for (usize k = 0; k < len / 2; ++k) {
                const float c = std::cos(ang * static_cast<float>(k)), s = std::sin(ang * static_cast<float>(k));
                const usize a = i + k, b = i + k + len / 2;
                const float tr = re[b] * c - im[b] * s, ti = re[b] * s + im[b] * c;
                re[b] = re[a] - tr, im[b] = im[a] - ti;
                re[a] += tr, im[a] += ti;
            }
    }
}

struct EffectState {
    AudioEffect fx;
    Biquad eq[3];
    float apState[2][4]{};       // phaser all-pass stages (one sample of memory each)
    float phaserFeedback[2]{};
    std::vector<float> window;   // analyzer input (mono)
    usize windowPos = 0;
    std::shared_ptr<std::vector<std::atomic<float>>> magnitudes;  // analyzer output, kFftSize / 2 bins
    AudioEffectType builtFor = AudioEffectType::LowPass;
    bool built = false;
    Biquad biquad;
    std::vector<float> line[2];  // echo / chorus delay lines
    usize writePos = 0;
    std::array<Comb, 8> combs[2];
    std::array<Allpass, 4> allpasses[2];
    float envelope = 0.0f;  // compressor, dB above silence
    float phase = 0.0f;     // chorus LFO

    void build(u32 rate) {
        builtFor = fx.type;
        built = true;
        const float scale = static_cast<float>(rate) / 44100.0f;
        switch (fx.type) {
            case AudioEffectType::Echo:
                for (auto& l : line) l.assign(static_cast<usize>(rate * 2.05f), 0.0f);  // up to 2 s
                break;
            case AudioEffectType::Chorus:
                for (auto& l : line) l.assign(static_cast<usize>(rate * 0.06f), 0.0f);
                break;
            case AudioEffectType::PitchShift:
                for (auto& l : line) l.assign(static_cast<usize>(rate * 0.1f), 0.0f);
                break;
            case AudioEffectType::Analyzer:
                window.assign(kFftSize, 0.0f);
                magnitudes = std::make_shared<std::vector<std::atomic<float>>>(kFftSize / 2);
                break;
            case AudioEffectType::Reverb: {
                static constexpr int kComb[8] = {1116, 1188, 1277, 1356, 1422, 1491, 1557, 1617};
                static constexpr int kAll[4] = {556, 441, 341, 225};
                for (int ch = 0; ch < 2; ++ch) {
                    const int spread = ch * 23;  // decorrelated left / right
                    for (int i = 0; i < 8; ++i) combs[ch][static_cast<usize>(i)].buf.assign(static_cast<usize>((kComb[i] + spread) * scale), 0.0f);
                    for (int i = 0; i < 4; ++i) allpasses[ch][static_cast<usize>(i)].buf.assign(static_cast<usize>((kAll[i] + spread) * scale), 0.0f);
                }
                break;
            }
            default: break;
        }
    }

    void process(float* f, u32 n, u32 rate) {
        const float wet = std::clamp(fx.mix, 0.0f, 1.0f), dry = 1.0f - wet;
        switch (fx.type) {
            case AudioEffectType::LowPass:
            case AudioEffectType::HighPass:
                biquad.design(fx.type == AudioEffectType::HighPass, fx.cutoff, fx.resonance, static_cast<float>(rate));
                for (u32 i = 0; i < n; ++i)
                    for (int ch = 0; ch < 2; ++ch) f[i * 2 + ch] = biquad.run(ch, f[i * 2 + ch]);
                break;
            case AudioEffectType::Echo: {
                const usize size = line[0].size();
                const usize d = std::clamp<usize>(static_cast<usize>(std::clamp(fx.delay, 0.001f, 2.0f) * rate), 1, size - 1);
                const float fb = std::clamp(fx.feedback, 0.0f, 0.95f);
                for (u32 i = 0; i < n; ++i) {
                    const usize read = (writePos + size - d) % size;
                    for (int ch = 0; ch < 2; ++ch) {
                        const float x = f[i * 2 + ch], echo = line[ch][read];
                        line[ch][writePos] = x + echo * fb;
                        f[i * 2 + ch] = x + echo * wet;  // the dry signal stays at full level
                    }
                    writePos = (writePos + 1) % size;
                }
                break;
            }
            case AudioEffectType::Reverb: {
                const float feedback = 0.7f + 0.28f * std::clamp(fx.roomSize, 0.0f, 1.0f), damp = 0.4f * std::clamp(fx.damping, 0.0f, 1.0f);
                for (u32 i = 0; i < n; ++i) {
                    const float input = (f[i * 2] + f[i * 2 + 1]) * 0.015f;  // freeverb's fixed input gain
                    for (int ch = 0; ch < 2; ++ch) {
                        float out = 0.0f;
                        for (Comb& c : combs[ch]) out += c.run(input, feedback, damp);
                        for (Allpass& a : allpasses[ch]) out = a.run(out);
                        f[i * 2 + ch] = f[i * 2 + ch] * dry + out * wet * 3.0f;
                    }
                }
                break;
            }
            case AudioEffectType::Compressor: {
                const float att = std::exp(-1.0f / (std::max(fx.attack, 0.01f) * 0.001f * rate));
                const float rel = std::exp(-1.0f / (std::max(fx.release, 0.01f) * 0.001f * rate));
                const float slope = 1.0f - 1.0f / std::max(fx.ratio, 1.0f), makeup = dbToGain(fx.gain);
                for (u32 i = 0; i < n; ++i) {
                    const float peak = std::max(std::fabs(f[i * 2]), std::fabs(f[i * 2 + 1]));
                    const float level = 20.0f * std::log10(std::max(peak, 1e-6f));
                    const float k = level > envelope ? att : rel;
                    envelope = level + (envelope - level) * k;
                    const float over = envelope - fx.threshold;
                    const float g = (over > 0.0f ? dbToGain(-over * slope) : 1.0f) * makeup;
                    f[i * 2] *= g;
                    f[i * 2 + 1] *= g;
                }
                break;
            }
            case AudioEffectType::Distortion: {
                const float drive = std::clamp(fx.drive, 1.0f, 50.0f), norm = 1.0f / std::tanh(drive);
                for (u32 i = 0; i < 2 * n; ++i) f[i] = f[i] * dry + std::tanh(f[i] * drive) * norm * wet;
                break;
            }
            case AudioEffectType::Equalizer: {
                eq[0].designEq(0, fx.lowFreq, fx.lowGain, static_cast<float>(rate));
                eq[1].designEq(1, fx.midFreq, fx.midGain, static_cast<float>(rate));
                eq[2].designEq(2, fx.highFreq, fx.highGain, static_cast<float>(rate));
                for (u32 i = 0; i < n; ++i)
                    for (int ch = 0; ch < 2; ++ch) {
                        float x = f[i * 2 + ch];
                        for (Biquad& b : eq) x = b.run(ch, x);
                        f[i * 2 + ch] = x;
                    }
                break;
            }
            case AudioEffectType::Phaser: {
                // 4 first-order all-pass stages whose corner sweeps 200 Hz .. 200 Hz * 2^(depth) with an LFO.
                const float step = 2.0f * kPi * std::clamp(fx.rate, 0.01f, 20.0f) / rate;
                const float octaves = std::clamp(fx.depth, 0.1f, 6.0f), fb = std::clamp(fx.feedback, 0.0f, 0.9f);
                for (u32 i = 0; i < n; ++i) {
                    const float corner = 200.0f * std::exp2(octaves * (0.5f + 0.5f * std::sin(phase)));
                    const float t = std::tan(kPi * std::min(corner, rate * 0.45f) / rate), a = (t - 1.0f) / (t + 1.0f);
                    for (int ch = 0; ch < 2; ++ch) {
                        float x = f[i * 2 + ch] + phaserFeedback[ch] * fb;
                        for (float& z : apState[ch]) {  // y = a*x + z; z = x - a*y
                            const float y = a * x + z;
                            z = x - a * y;
                            x = y;
                        }
                        phaserFeedback[ch] = x;
                        f[i * 2 + ch] = f[i * 2 + ch] * dry + x * wet;
                    }
                    phase += step;
                    if (phase > 2.0f * kPi) phase -= 2.0f * kPi;
                }
                break;
            }
            case AudioEffectType::PitchShift: {
                // Two read heads move through a short delay line at (1 - pitch) samples per sample,
                // half a window apart, crossfaded with triangle windows (no speed change).
                const usize size = line[0].size();
                const float win = static_cast<float>(size) * 0.45f, ratio = std::clamp(fx.pitch, 0.25f, 4.0f);
                for (u32 i = 0; i < n; ++i) {
                    float out[2] = {0.0f, 0.0f};
                    for (int head = 0; head < 2; ++head) {
                        float d = std::fmod(phase + static_cast<float>(head) * win * 0.5f, win);
                        if (d < 0.0f) d += win;
                        const float gain = 1.0f - std::fabs(d / win * 2.0f - 1.0f);  // triangle
                        float readPos = static_cast<float>(writePos) - d - 1.0f;
                        while (readPos < 0.0f) readPos += static_cast<float>(size);
                        const usize r0 = static_cast<usize>(readPos) % size, r1 = (r0 + 1) % size;
                        const float frac = readPos - std::floor(readPos);
                        for (int ch = 0; ch < 2; ++ch) out[ch] += (line[ch][r0] + (line[ch][r1] - line[ch][r0]) * frac) * gain;
                    }
                    for (int ch = 0; ch < 2; ++ch) {
                        line[ch][writePos] = f[i * 2 + ch];
                        f[i * 2 + ch] = f[i * 2 + ch] * dry + out[ch] * wet;
                    }
                    writePos = (writePos + 1) % size;
                    phase -= ratio - 1.0f;  // the delay shrinks (pitch up) or grows (pitch down)
                }
                break;
            }
            case AudioEffectType::Analyzer: {
                for (u32 i = 0; i < n; ++i) {
                    window[windowPos++] = (f[i * 2] + f[i * 2 + 1]) * 0.5f;
                    if (windowPos < kFftSize) continue;
                    windowPos = 0;
                    std::vector<float> re(kFftSize), im(kFftSize, 0.0f);
                    for (usize k = 0; k < kFftSize; ++k)  // Hann window
                        re[k] = window[k] * (0.5f - 0.5f * std::cos(2.0f * kPi * static_cast<float>(k) / static_cast<float>(kFftSize - 1)));
                    fft(re, im);
                    for (usize k = 0; k < kFftSize / 2; ++k) (*magnitudes)[k].store(std::sqrt(re[k] * re[k] + im[k] * im[k]) * 4.0f / kFftSize, std::memory_order_relaxed);
                }
                break;
            }
            case AudioEffectType::Chorus: {
                const usize size = line[0].size();
                const float base = 0.012f * rate, depth = std::clamp(fx.depth, 0.0f, 20.0f) * 0.001f * rate;
                const float step = 2.0f * kPi * std::clamp(fx.rate, 0.01f, 20.0f) / rate;
                for (u32 i = 0; i < n; ++i) {
                    for (int ch = 0; ch < 2; ++ch) {
                        line[ch][writePos] = f[i * 2 + ch];
                        const float d = base + depth * (0.5f + 0.5f * std::sin(phase + (ch ? kPi * 0.5f : 0.0f)));
                        float readPos = static_cast<float>(writePos) - d;
                        while (readPos < 0.0f) readPos += static_cast<float>(size);
                        const usize r0 = static_cast<usize>(readPos) % size, r1 = (r0 + 1) % size;
                        const float frac = readPos - std::floor(readPos);
                        const float delayed = line[ch][r0] + (line[ch][r1] - line[ch][r0]) * frac;
                        f[i * 2 + ch] = f[i * 2 + ch] * dry + delayed * wet;
                    }
                    writePos = (writePos + 1) % size;
                    phase += step;
                    if (phase > 2.0f * kPi) phase -= 2.0f * kPi;
                }
                break;
            }
        }
    }
};

} // namespace

const char* audioEffectName(AudioEffectType type) { return kNames[std::min<usize>(static_cast<usize>(type), kAudioEffectTypeCount - 1)]; }

bool audioEffectFromName(std::string_view name, AudioEffectType& out) {
    for (u32 i = 0; i < kAudioEffectTypeCount; ++i)
        if (name == kNames[i]) {
            out = static_cast<AudioEffectType>(i);
            return true;
        }
    return false;
}

struct AudioEffectChain::Impl {
    std::vector<EffectState> states;
    u32 rate = 48000;
};

AudioEffectChain::AudioEffectChain() : m(std::make_unique<Impl>()) {}
AudioEffectChain::~AudioEffectChain() = default;

void AudioEffectChain::configure(const std::vector<AudioEffect>& effects, u32 sampleRate) {
    if (sampleRate != m->rate) {
        m->states.clear();
        m->rate = sampleRate;
    }
    m->states.resize(effects.size());
    for (usize i = 0; i < effects.size(); ++i) {
        EffectState& s = m->states[i];
        s.fx = effects[i];
        if (!s.built || s.builtFor != s.fx.type) {
            s = EffectState{};
            s.fx = effects[i];
            s.build(m->rate);
        }
    }
}

void AudioEffectChain::process(float* frames, u32 count) {
    for (EffectState& s : m->states)
        if (s.fx.enabled) s.process(frames, count, m->rate);
}

std::vector<float> AudioEffectChain::spectrum(u32 bands) const {
    std::vector<float> out;
    for (const EffectState& s : m->states) {
        if (s.fx.type != AudioEffectType::Analyzer || !s.magnitudes || bands == 0) continue;
        const auto mags = s.magnitudes;
        out.assign(bands, 0.0f);
        const float binHz = static_cast<float>(m->rate) / static_cast<float>(kFftSize);
        for (u32 b = 0; b < bands; ++b) {  // log-spaced bands, peak of their bins
            const float f0 = 40.0f * std::pow(400.0f, static_cast<float>(b) / bands), f1 = 40.0f * std::pow(400.0f, static_cast<float>(b + 1) / bands);
            const usize k0 = std::clamp<usize>(static_cast<usize>(f0 / binHz), 1, kFftSize / 2 - 1);
            const usize k1 = std::clamp<usize>(static_cast<usize>(f1 / binHz), k0 + 1, kFftSize / 2);
            float peak = 0.0f;
            for (usize k = k0; k < k1; ++k) peak = std::max(peak, (*mags)[k].load(std::memory_order_relaxed));
            out[b] = std::min(peak, 1.0f);
        }
        break;
    }
    return out;
}

void AudioEffectChain::reset() {
    for (EffectState& s : m->states) {
        const AudioEffect fx = s.fx;
        s = EffectState{};
        s.fx = fx;
        s.build(m->rate);
    }
}

// ------------------------------------------------------------------- layout
using json = nlohmann::json;

std::string AudioBusLayout::toJson() const {
    json j;
    j["version"] = 1;
    json buses = json::object();
    for (i32 b = 0; b < kAudioBusCount; ++b) {
        json fx = json::array();
        for (const AudioEffect& e : effects[b])
            fx.push_back({{"type", audioEffectName(e.type)}, {"enabled", e.enabled}, {"cutoff", e.cutoff}, {"resonance", e.resonance}, {"delay", e.delay},
                          {"feedback", e.feedback}, {"roomSize", e.roomSize}, {"damping", e.damping}, {"threshold", e.threshold}, {"ratio", e.ratio},
                          {"attack", e.attack}, {"release", e.release}, {"gain", e.gain}, {"drive", e.drive}, {"rate", e.rate}, {"depth", e.depth},
                          {"mix", e.mix}, {"lowGain", e.lowGain}, {"midGain", e.midGain}, {"highGain", e.highGain}, {"lowFreq", e.lowFreq},
                          {"midFreq", e.midFreq}, {"highFreq", e.highFreq}, {"pitch", e.pitch}});
        buses[audioBusName(static_cast<AudioBus>(b))] = {{"volume", volume[b]}, {"muted", muted[b]}, {"effects", std::move(fx)}};
    }
    j["buses"] = std::move(buses);
    return j.dump(2);
}

Result<AudioBusLayout> AudioBusLayout::fromJson(std::string_view text) {
    json j = json::parse(text, nullptr, false);
    if (j.is_discarded() || !j.is_object()) return makeError(Error::Code::ParseError, "audio buses: invalid JSON");
    if (j.value("version", 1) > 1) return makeError(Error::Code::VersionMismatch, "audio buses: made by a newer version of AGR");
    AudioBusLayout l;
    try {
        const json buses = j.value("buses", json::object());
        for (const auto& [name, bj] : buses.items()) {
            AudioBus bus;
            if (!audioBusFromName(name, bus)) continue;
            const i32 b = static_cast<i32>(bus);
            l.volume[b] = std::clamp(bj.value("volume", 1.0f), 0.0f, 4.0f);
            l.muted[b] = bj.value("muted", false);
            for (const json& e : bj.value("effects", json::array())) {
                AudioEffect fx;
                if (!audioEffectFromName(e.value("type", ""), fx.type)) continue;
                fx.enabled = e.value("enabled", true);
                fx.cutoff = e.value("cutoff", fx.cutoff), fx.resonance = e.value("resonance", fx.resonance);
                fx.delay = e.value("delay", fx.delay), fx.feedback = e.value("feedback", fx.feedback);
                fx.roomSize = e.value("roomSize", fx.roomSize), fx.damping = e.value("damping", fx.damping);
                fx.threshold = e.value("threshold", fx.threshold), fx.ratio = e.value("ratio", fx.ratio);
                fx.attack = e.value("attack", fx.attack), fx.release = e.value("release", fx.release), fx.gain = e.value("gain", fx.gain);
                fx.drive = e.value("drive", fx.drive), fx.rate = e.value("rate", fx.rate), fx.depth = e.value("depth", fx.depth);
                fx.mix = e.value("mix", fx.mix);
                fx.lowGain = e.value("lowGain", fx.lowGain), fx.midGain = e.value("midGain", fx.midGain), fx.highGain = e.value("highGain", fx.highGain);
                fx.lowFreq = e.value("lowFreq", fx.lowFreq), fx.midFreq = e.value("midFreq", fx.midFreq), fx.highFreq = e.value("highFreq", fx.highFreq);
                fx.pitch = e.value("pitch", fx.pitch);
                l.effects[b].push_back(fx);
            }
        }
    } catch (const json::exception& e) {
        return makeError(Error::Code::ParseError, std::string("audio buses: ") + e.what());
    }
    return l;
}

} // namespace agr
