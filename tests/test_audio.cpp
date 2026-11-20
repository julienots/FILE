#include <doctest/doctest.h>

#include <cmath>
#include <filesystem>
#include <numbers>
#include <vector>

#include "agr/audio/AudioSystem.hpp"
#include "agr/audio/Wav.hpp"
#include "agr/core/Uuid.hpp"
#include "agr/render/RenderComponents.hpp"

using namespace agr;
namespace fs = std::filesystem;

namespace {

constexpr u32 kRate = 48000;

struct AudioFixture {
    fs::path root = fs::temp_directory_path() / ("agr_audio_" + Uuid::generate().toString());
    VirtualFileSystem vfs;
    AudioSystem audio{vfs};
    std::vector<float> buffer;

    explicit AudioFixture(u32 maxVoices = 64) {
        fs::create_directories(root);
        vfs.mount("assets", std::make_unique<NativeFileProvider>(root.string(), false));
        registerRenderComponents();
        registerAudioComponents();
        AudioConfig c;
        c.output = AudioOutput::Manual;
        c.sampleRate = kRate;
        c.maxVoices = maxVoices;
        REQUIRE(audio.initialize(c));
    }
    ~AudioFixture() {
        audio.shutdown();
        std::error_code ec;
        fs::remove_all(root, ec);
    }
    /// Writes a mono sine wave of `seconds` at `amplitude`.
    void sine(const std::string& uri, f64 seconds, float amplitude = 0.5f, u32 rate = kRate, u32 channels = 1) {
        const u32 frames = static_cast<u32>(seconds * rate);
        std::vector<float> s(static_cast<usize>(frames) * channels);
        for (u32 i = 0; i < frames; ++i)
            for (u32 c = 0; c < channels; ++c)
                s[i * channels + c] = amplitude * static_cast<float>(std::sin(2.0 * std::numbers::pi * 440.0 * i / rate));
        const ByteBuffer wav = encodeWav16(s.data(), frames, channels, rate);
        REQUIRE(vfs.writeBytes(uri, wav.data(), wav.size()));
    }
    struct Level {
        float left = 0, right = 0;
        float peak() const { return std::max(left, right); }
    };
    /// Mixes `seconds` and returns the peak level per channel, then releases finished voices.
    Level run(f64 seconds) {
        const u32 frames = static_cast<u32>(seconds * kRate);
        buffer.assign(static_cast<usize>(frames) * 2, 0.0f);
        Level l;
        u32 done = 0;
        while (done < frames) {
            const u32 n = std::min<u32>(1024, frames - done);
            audio.mix(buffer.data() + static_cast<usize>(done) * 2, n);
            done += n;
        }
        for (u32 i = 0; i < frames; ++i) {
            l.left = std::max(l.left, std::fabs(buffer[i * 2]));
            l.right = std::max(l.right, std::fabs(buffer[i * 2 + 1]));
        }
        audio.update(0.0);
        return l;
    }
};

} // namespace

TEST_CASE("audio: a clip plays at its volume and its voice ends with it") {
    AudioFixture f;
    f.sine("assets://beep.wav", 0.1);
    auto d = f.audio.clipDuration("assets://beep.wav");
    REQUIRE(d);
    CHECK(d.value() == doctest::Approx(0.1).epsilon(0.01));
    CHECK(f.run(0.05).peak() == doctest::Approx(0.0f));  // silence before anything plays

    auto v = f.audio.play("assets://beep.wav");
    REQUIRE(v);
    CHECK(f.audio.isPlaying(v.value()));
    const auto level = f.run(0.05);
    CHECK(level.left > 0.3f);
    CHECK(level.left < 0.7f);
    CHECK(level.right == doctest::Approx(level.left).epsilon(0.05));  // centered
    CHECK(f.audio.stats().voices == 1);
    f.run(0.1);  // past the end
    CHECK_FALSE(f.audio.isPlaying(v.value()));
    CHECK(f.audio.stats().voices == 0);
    CHECK(f.audio.stats().clips == 1);
    f.audio.unloadUnused();
    CHECK(f.audio.stats().clips == 0);

    // Volume and pitch: a pitch of 2 ends in half the time.
    auto quiet = f.audio.play("assets://beep.wav", {.volume = 0.25f, .pitch = 2.0f});
    REQUIRE(quiet);
    CHECK(f.run(0.03).peak() < 0.2f);
    f.run(0.025);
    CHECK_FALSE(f.audio.isPlaying(quiet.value()));
    CHECK(f.audio.takeMeter().left > 0.0f);  // the master meter saw the output
}

TEST_CASE("audio: buses scale and mute their voices") {
    AudioFixture f;
    f.sine("assets://tone.wav", 1.0);
    REQUIRE(f.audio.play("assets://tone.wav", {.bus = AudioBus::Music, .loop = true}));
    const float full = f.run(0.05).peak();
    CHECK(full > 0.3f);
    f.audio.setBusVolume(AudioBus::Music, 0.5f);
    CHECK(f.audio.busVolume(AudioBus::Music) == doctest::Approx(0.5f));
    f.run(0.02);  // volume smoothing
    CHECK(f.run(0.05).peak() == doctest::Approx(full * 0.5f).epsilon(0.1));
    f.audio.setBusMuted(AudioBus::Music, true);
    f.run(0.02);
    CHECK(f.run(0.05).peak() < 0.01f);
    f.audio.setBusMuted(AudioBus::Music, false);
    f.audio.setBusVolume(AudioBus::Master, 0.0f);  // master silences every bus
    f.run(0.02);
    CHECK(f.run(0.05).peak() < 0.01f);
    f.audio.setBusVolume(AudioBus::Master, 1.0f);
    f.run(0.02);
    CHECK(f.run(0.05).peak() == doctest::Approx(full * 0.5f).epsilon(0.1));
}

TEST_CASE("audio: looping, stopping, pausing and the voice limit") {
    AudioFixture f(4);
    f.sine("assets://short.wav", 0.05);
    auto loop = f.audio.play("assets://short.wav", {.loop = true});
    REQUIRE(loop);
    f.run(0.2);  // four times its length
    CHECK(f.audio.isPlaying(loop.value()));
    CHECK(f.run(0.02).peak() > 0.3f);
    f.audio.setPaused(loop.value(), true);
    CHECK(f.audio.isPlaying(loop.value()));  // paused voices are kept
    f.run(0.02);  // the mixer has up to ~10 ms of audio in flight
    CHECK(f.run(0.05).peak() < 0.01f);
    f.audio.setPaused(loop.value(), false);
    CHECK(f.run(0.05).peak() > 0.3f);
    f.audio.stop(loop.value());
    f.run(0.01);
    CHECK_FALSE(f.audio.isPlaying(loop.value()));
    CHECK(f.audio.stats().voices == 0);

    // 6 one-shots with 4 voices: the 2 oldest are stolen.
    std::vector<VoiceId> ids;
    for (int i = 0; i < 6; ++i) {
        auto v = f.audio.play("assets://short.wav");
        REQUIRE(v);
        ids.push_back(v.value());
    }
    CHECK(f.audio.stats().voices == 4);
    CHECK(f.audio.stats().voicesStolen == 2);
    CHECK_FALSE(f.audio.isPlaying(ids[0]));
    CHECK(f.audio.isPlaying(ids[5]));

    // Game pause leaves the Interface bus playing.
    f.audio.stopAll();
    REQUIRE(f.audio.play("assets://short.wav", {.loop = true}));
    REQUIRE(f.audio.play("assets://short.wav", {.bus = AudioBus::Interface, .volume = 0.2f, .loop = true}));
    f.audio.pauseSources(true);
    f.run(0.02);
    const float paused = f.run(0.05).peak();
    CHECK(paused > 0.05f);
    CHECK(paused < 0.15f);
    f.audio.pauseSources(false);
    CHECK(f.run(0.05).peak() > 0.4f);
}

TEST_CASE("audio: spatial voices are panned and attenuated from the listener") {
    AudioFixture f;
    f.sine("assets://src.wav", 1.0);
    f.audio.setListener({0, 0, 0}, {0, 0, -1});
    auto right = f.audio.play("assets://src.wav", {.loop = true, .spatial = true, .position = {3, 0, 0}, .minDistance = 1, .maxDistance = 100});
    REQUIRE(right);
    f.run(0.05);
    const auto r = f.run(0.05);
    CHECK(r.right > r.left * 1.5f);

    // Same source on the left.
    f.audio.setPosition(right.value(), {-3, 0, 0});
    f.run(0.05);
    const auto l = f.run(0.05);
    CHECK(l.left > l.right * 1.5f);

    // Farther = quieter; within minDistance = full volume.
    f.audio.setPosition(right.value(), {0, 0, -20});
    f.run(0.05);
    const float far = f.run(0.05).peak();
    f.audio.setPosition(right.value(), {0, 0, -0.5f});
    f.run(0.05);
    const float near = f.run(0.05).peak();
    CHECK(far < near * 0.25f);

    // Non-spatial pan.
    f.audio.stopAll();
    REQUIRE(f.audio.play("assets://src.wav", {.pan = -1.0f, .loop = true}));
    const auto panned = f.run(0.05);
    CHECK(panned.left > 0.3f);
    CHECK(panned.right < 0.05f);
}

TEST_CASE("audio: stereo, other sample rates, streaming and invalid files") {
    AudioFixture f;
    f.sine("assets://stereo22k.wav", 0.2, 0.5f, 22050, 2);
    auto v = f.audio.play("assets://stereo22k.wav");
    REQUIRE(v);
    CHECK(f.run(0.05).peak() > 0.3f);
    f.run(0.2);
    CHECK_FALSE(f.audio.isPlaying(v.value()));  // resampled: ends at 0.2 s, not 0.2 * 48000 / 22050

    f.sine("assets://music.wav", 0.3);
    auto s = f.audio.play("assets://music.wav", {.bus = AudioBus::Music, .stream = true});
    REQUIRE(s);
    CHECK(f.run(0.1).peak() > 0.3f);
    f.run(0.25);
    CHECK_FALSE(f.audio.isPlaying(s.value()));

    CHECK_FALSE(f.audio.play("assets://missing.wav"));
    const std::string junk = "this is not audio data at all, just some text";
    REQUIRE(f.vfs.writeText("assets://junk.wav", junk));
    auto bad = f.audio.play("assets://junk.wav");
    REQUIRE_FALSE(bad);
    CHECK(bad.error().message.find("junk.wav") != std::string::npos);
    CHECK(f.audio.stats().voices == 0);
}

TEST_CASE("audio: AudioSource components follow the scene") {
    AudioFixture f;
    f.sine("assets://loop.wav", 0.2);
    f.sine("assets://other.wav", 0.2);
    Scene scene;
    const auto cam = scene.createEntity("Camera");
    scene.registry().emplace<CameraComponent>(cam);
    const auto e = scene.createEntity("Speaker");
    auto& src = scene.registry().emplace<AudioSourceComponent>(e);
    src.clip = "assets://loop.wav";
    src.loop = true;
    const auto silent = scene.createEntity("Manual");
    auto& manual = scene.registry().emplace<AudioSourceComponent>(silent);
    manual.clip = "assets://other.wav";
    manual.playOnStart = false;
    scene.updateTransforms();

    f.audio.updateScene(scene, false);  // edit mode: nothing plays
    CHECK_FALSE(f.audio.sourcePlaying(e));
    f.audio.updateScene(scene, true);
    CHECK(f.audio.sourcePlaying(e));
    CHECK_FALSE(f.audio.sourcePlaying(silent));
    CHECK(f.run(0.05).peak() > 0.3f);

    // Played on demand, then finishes by itself (not looping).
    CHECK(f.audio.playSource(scene, silent) != 0);
    CHECK(f.audio.sourcePlaying(silent));
    f.run(0.25);
    f.audio.updateScene(scene, true);
    CHECK_FALSE(f.audio.sourcePlaying(silent));
    CHECK(f.audio.sourcePlaying(e));

    // Live parameter changes: volume 0, then a new clip restarts the voice.
    scene.registry().get<AudioSourceComponent>(e).volume = 0.0f;
    f.audio.updateScene(scene, true);
    f.run(0.02);
    CHECK(f.run(0.05).peak() < 0.01f);
    scene.registry().get<AudioSourceComponent>(e).volume = 1.0f;
    scene.registry().get<AudioSourceComponent>(e).clip = "assets://other.wav";
    f.audio.updateScene(scene, true);
    CHECK(f.audio.sourcePlaying(e));

    // Inactive entity: stops; active again: plays again.
    scene.registry().get<StateComponent>(e).active = false;
    scene.updateTransforms();
    f.audio.updateScene(scene, true);
    CHECK_FALSE(f.audio.sourcePlaying(e));
    scene.registry().get<StateComponent>(e).active = true;
    scene.updateTransforms();
    f.audio.updateScene(scene, true);
    CHECK(f.audio.sourcePlaying(e));

    // Removed component and game stop.
    scene.registry().remove<AudioSourceComponent>(e);
    f.audio.updateScene(scene, true);
    CHECK_FALSE(f.audio.sourcePlaying(e));
    f.audio.updateScene(scene, false);
    f.run(0.01);
    CHECK(f.audio.stats().voices == 0);

    // A reloaded scene (new generation) starts its sources from scratch.
    auto& again = scene.registry().emplace<AudioSourceComponent>(e);
    again.clip = "assets://loop.wav";
    again.loop = true;
    f.audio.updateScene(scene, true);
    CHECK(f.audio.sourcePlaying(e));
    scene.clear();
    f.audio.updateScene(scene, true);
    CHECK(f.audio.stats().voices == 0);
}

TEST_CASE("audio: virtual output advances in real time") {
    VirtualFileSystem vfs;
    const fs::path root = fs::temp_directory_path() / ("agr_audio_v_" + Uuid::generate().toString());
    fs::create_directories(root);
    vfs.mount("assets", std::make_unique<NativeFileProvider>(root.string(), false));
    std::vector<float> s(4800, 0.5f);
    const ByteBuffer wav = encodeWav16(s.data(), 4800, 1, kRate);
    REQUIRE(vfs.writeBytes("assets://a.wav", wav.data(), wav.size()));
    {
        AudioSystem audio(vfs);
        REQUIRE(audio.initialize({.output = AudioOutput::Virtual, .sampleRate = kRate}));
        CHECK_FALSE(audio.stats().deviceActive);
        auto v = audio.play("assets://a.wav");
        REQUIRE(v);
        audio.update(0.05);
        CHECK(audio.isPlaying(v.value()));
        audio.update(0.08);  // includes the ~10 ms in flight in the mixer
        CHECK_FALSE(audio.isPlaying(v.value()));
        CHECK(audio.stats().framesMixed >= static_cast<u64>(0.1 * kRate));
    }
    std::error_code ec;
    fs::remove_all(root, ec);
}

namespace {

std::vector<float> stereoSine(float freq, float amplitude, u32 frames, u32 rate) {
    std::vector<float> s(static_cast<usize>(frames) * 2);
    for (u32 i = 0; i < frames; ++i) s[i * 2] = s[i * 2 + 1] = amplitude * static_cast<float>(std::sin(2.0 * std::numbers::pi * freq * i / rate));
    return s;
}
float peakFrom(const std::vector<float>& s, usize firstFrame) {
    float p = 0.0f;
    for (usize i = firstFrame * 2; i < s.size(); ++i) p = std::max(p, std::fabs(s[i]));
    return p;
}
std::vector<float> runChain(const std::vector<AudioEffect>& fx, std::vector<float> s, u32 rate = 48000) {
    AudioEffectChain chain;
    chain.configure(fx, rate);
    for (usize at = 0; at < s.size() / 2; at += 512) chain.process(s.data() + at * 2, static_cast<u32>(std::min<usize>(512, s.size() / 2 - at)));
    return s;
}

} // namespace

TEST_CASE("audio effects: filters, echo, reverb, compressor, distortion, chorus") {
    const u32 rate = 48000;
    AudioEffect lp{.type = AudioEffectType::LowPass, .cutoff = 500.0f};
    CHECK(peakFrom(runChain({lp}, stereoSine(8000, 0.5f, rate / 4, rate)), rate / 8) < 0.05f);   // highs removed
    CHECK(peakFrom(runChain({lp}, stereoSine(100, 0.5f, rate / 4, rate)), rate / 8) > 0.45f);    // lows kept
    AudioEffect hp{.type = AudioEffectType::HighPass, .cutoff = 2000.0f};
    CHECK(peakFrom(runChain({hp}, stereoSine(100, 0.5f, rate / 4, rate)), rate / 8) < 0.05f);
    CHECK(peakFrom(runChain({hp}, stereoSine(8000, 0.5f, rate / 4, rate)), rate / 8) > 0.45f);

    // Echo: an impulse comes back after `delay` at `mix`, then again at mix x feedback.
    std::vector<float> impulse(static_cast<usize>(rate) * 2, 0.0f);
    impulse[0] = impulse[1] = 1.0f;
    AudioEffect echo{.type = AudioEffectType::Echo, .delay = 0.1f, .feedback = 0.5f, .mix = 0.6f};
    const auto e = runChain({echo}, impulse);
    CHECK(e[0] == doctest::Approx(1.0f));
    CHECK(e[4800 * 2] == doctest::Approx(0.6f));
    CHECK(e[9600 * 2] == doctest::Approx(0.3f));
    CHECK(std::fabs(e[2400 * 2]) < 1e-6f);

    // Reverb: a tail after the impulse; mix 0 leaves the sound untouched.
    AudioEffect rev{.type = AudioEffectType::Reverb, .roomSize = 0.8f, .mix = 0.5f};
    const auto r = runChain({rev}, impulse);
    CHECK(peakFrom(r, rate / 5) > 1e-3f);
    rev.mix = 0.0f;
    const auto dry = runChain({rev}, impulse);
    CHECK(dry[0] == doctest::Approx(1.0f));
    CHECK(peakFrom(dry, 1) < 1e-6f);

    // Compressor: a loud tone (0 dBFS) above -20 dB at 10:1 comes out around -18 dB.
    AudioEffect comp{.type = AudioEffectType::Compressor, .threshold = -20.0f, .ratio = 10.0f, .attack = 1.0f, .release = 50.0f};
    const float squashed = peakFrom(runChain({comp}, stereoSine(440, 1.0f, rate / 2, rate)), rate / 4);
    CHECK(squashed < 0.2f);
    CHECK(squashed > 0.08f);
    CHECK(peakFrom(runChain({comp}, stereoSine(440, 0.05f, rate / 2, rate)), rate / 4) == doctest::Approx(0.05f).epsilon(0.05));  // quiet: untouched

    // Distortion: a soft clip that drives a 0.5 tone near full scale; disabled = untouched.
    AudioEffect dist{.type = AudioEffectType::Distortion, .drive = 10.0f, .mix = 1.0f};
    CHECK(peakFrom(runChain({dist}, stereoSine(440, 0.5f, rate / 10, rate)), 0) > 0.95f);
    dist.enabled = false;
    CHECK(peakFrom(runChain({dist}, stereoSine(440, 0.5f, rate / 10, rate)), 0) == doctest::Approx(0.5f).epsilon(0.01));

    // Chorus: mixes a moving delayed copy (the output differs, stays bounded).
    AudioEffect ch{.type = AudioEffectType::Chorus, .mix = 0.5f};
    const auto src = stereoSine(440, 0.5f, rate / 4, rate);
    const auto c = runChain({ch}, src);
    float diff = 0.0f;
    for (usize i = 0; i < c.size(); ++i) diff = std::max(diff, std::fabs(c[i] - src[i]));
    CHECK(diff > 0.05f);
    CHECK(peakFrom(c, 0) <= 0.51f);
    // A chain runs effects in order.
    CHECK(peakFrom(runChain({lp, dist}, stereoSine(8000, 0.5f, rate / 4, rate)), rate / 8) < 0.1f);
}

TEST_CASE("audio: bus effects apply to the mix, layouts round-trip") {
    AudioFixture f;
    f.sine("assets://tone.wav", 1.0);
    REQUIRE(f.audio.play("assets://tone.wav", {.bus = AudioBus::Music, .loop = true}));
    const float full = f.run(0.05).peak();
    f.audio.setBusEffects(AudioBus::Music, {AudioEffect{.type = AudioEffectType::LowPass, .cutoff = 120.0f}});
    REQUIRE(f.audio.busEffects(AudioBus::Music).size() == 1);
    f.run(0.05);
    CHECK(f.run(0.05).peak() < full * 0.25f);  // a 440 Hz tone through a 120 Hz low-pass
    f.audio.setBusEffects(AudioBus::Music, {});
    f.run(0.05);
    CHECK(f.run(0.05).peak() == doctest::Approx(full).epsilon(0.05));
    // On Master: every bus goes through it.
    f.audio.setBusEffects(AudioBus::Master, {AudioEffect{.type = AudioEffectType::Distortion, .drive = 20.0f, .mix = 1.0f}});
    f.run(0.05);
    CHECK(f.run(0.05).peak() > 0.9f);

    AudioBusLayout l = f.audio.layout();
    l.volume[static_cast<int>(AudioBus::Effects)] = 0.4f;
    l.effects[static_cast<int>(AudioBus::Effects)].push_back(AudioEffect{.type = AudioEffectType::Reverb, .roomSize = 0.9f});
    auto parsed = AudioBusLayout::fromJson(l.toJson());
    REQUIRE(parsed);
    CHECK(parsed.value().volume[static_cast<int>(AudioBus::Effects)] == doctest::Approx(0.4f));
    REQUIRE(parsed.value().effects[static_cast<int>(AudioBus::Master)].size() == 1);
    CHECK(parsed.value().effects[static_cast<int>(AudioBus::Master)][0].type == AudioEffectType::Distortion);
    CHECK(parsed.value().effects[static_cast<int>(AudioBus::Effects)][0].roomSize == doctest::Approx(0.9f));
    f.audio.applyLayout(parsed.value());
    CHECK(f.audio.busVolume(AudioBus::Effects) == doctest::Approx(0.4f));
    CHECK(f.audio.busEffects(AudioBus::Effects).size() == 1);
    CHECK_FALSE(AudioBusLayout::fromJson("{"));
}
