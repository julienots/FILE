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
