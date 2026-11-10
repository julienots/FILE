#include "agr/audio/AudioSystem.hpp"

#include <algorithm>
#include <atomic>
#include <bit>
#include <cmath>
#include <cstring>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include <miniaudio.h>

#include "agr/core/Log.hpp"
#include "agr/render/RenderComponents.hpp"
#include "agr/scene/Components.hpp"

namespace agr {

namespace {

constexpr u32 kChannels = 2;

struct Clip {
    std::vector<float> pcm;  ///< Interleaved f32 (decoded clips).
    u32 channels = 0;
    u32 sampleRate = 0;
    u64 frames = 0;
    std::shared_ptr<const ByteBuffer> encoded;  ///< Kept for streamed voices.
    i64 modifiedTime = 0;
};

struct Voice {
    VoiceId id = 0;
    ma_sound sound{};
    ma_audio_buffer_ref buffer{};
    ma_decoder decoder{};
    bool soundInit = false, bufferInit = false, decoderInit = false;
    std::shared_ptr<Clip> clip;
    std::shared_ptr<const ByteBuffer> encoded;
    std::string uri;
    AudioBus bus = AudioBus::Effects;
    bool loop = false;
    bool paused = false;
    bool stopping = false;
    bool spatial = false;
    bool streamed = false;
    u64 order = 0;
    u64 entity = 0;  ///< Packed entity of a scene source, 0 for other voices.

    ~Voice() {
        if (soundInit) ma_sound_uninit(&sound);
        if (decoderInit) ma_decoder_uninit(&decoder);
        if (bufferInit) ma_audio_buffer_ref_uninit(&buffer);
    }
};

u32 floatBits(float f) { return std::bit_cast<u32>(f); }
float bitsFloat(u32 u) { return std::bit_cast<float>(u); }

void atomicMax(std::atomic<u32>& a, float value) {
    u32 cur = a.load(std::memory_order_relaxed);
    while (bitsFloat(cur) < value && !a.compare_exchange_weak(cur, floatBits(value), std::memory_order_relaxed)) {}
}

const char* resultText(ma_result r) { return ma_result_description(r); }

} // namespace

struct AudioSystem::Impl {
    VirtualFileSystem& vfs;
    AudioConfig config;
    ma_engine engine{};
    ma_device device{};
    ma_sound_group groups[kAudioBusCount]{};
    bool engineInit = false, deviceInit = false, deviceRunning = false, suspended = false;
    bool groupInit[kAudioBusCount]{};
    float busVolumes[kAudioBusCount]{1, 1, 1, 1, 1, 1};
    bool busMutedFlags[kAudioBusCount]{};
    AudioOutput output = AudioOutput::Manual;

    std::unordered_map<std::string, std::shared_ptr<Clip>> clips;
    std::unordered_map<VoiceId, std::unique_ptr<Voice>> voices;
    VoiceId nextId = 1;
    u64 nextOrder = 0;

    std::unordered_map<u64, VoiceId> sources;  // packed entity -> voice
    std::unordered_set<u64> started;           // sources already started (playOnStart)
    u64 sceneGeneration = 0;
    bool sourcesPaused = false;

    std::atomic<u32> peakL{0}, peakR{0};
    std::atomic<u64> framesMixed{0};
    f64 virtualAccumulator = 0.0;
    std::vector<float> scratch;
    AudioStats stats;

    explicit Impl(VirtualFileSystem& v) : vfs(v) {}

    static void dataCallback(ma_device* d, void* out, const void*, ma_uint32 frames) {
        auto* self = static_cast<Impl*>(d->pUserData);
        self->render(static_cast<float*>(out), frames);
    }

    void render(float* out, u32 frames) {
        ma_engine_read_pcm_frames(&engine, out, frames, nullptr);
        float l = 0.0f, r = 0.0f;
        for (u32 i = 0; i < frames; ++i) {
            l = std::max(l, std::fabs(out[i * kChannels]));
            r = std::max(r, std::fabs(out[i * kChannels + 1]));
        }
        atomicMax(peakL, l);
        atomicMax(peakR, r);
        framesMixed.fetch_add(frames, std::memory_order_relaxed);
    }

    void applyBus(i32 i) {
        if (groupInit[i]) ma_sound_group_set_volume(&groups[i], busMutedFlags[i] ? 0.0f : busVolumes[i]);
    }

    Voice* find(VoiceId id) const {
        auto it = voices.find(id);
        return it == voices.end() ? nullptr : it->second.get();
    }

    // ---------------------------------------------------------------- clips
    Result<std::shared_ptr<Clip>> loadClip(std::string_view uri, bool stream) {
        const std::string key(uri);
        const i64 modified = vfs.modifiedTime(uri);
        std::shared_ptr<Clip>& slot = clips[key];
        // A modified file gets a new entry (voices still playing keep the old data alive).
        if (!slot || slot->modifiedTime != modified) {
            slot = std::make_shared<Clip>();
            slot->modifiedTime = modified;
        }
        if (stream ? static_cast<bool>(slot->encoded) : !slot->pcm.empty()) return slot;
        std::shared_ptr<Clip> clip = slot;
        auto fail = [&](Error e) -> Result<std::shared_ptr<Clip>> {
            if (clip->pcm.empty() && !clip->encoded) clips.erase(key);
            return e;
        };

        auto bytes = vfs.readBytes(uri);
        if (!bytes) return fail(bytes.error());
        auto encoded = std::make_shared<const ByteBuffer>(std::move(bytes.value()));
        ma_decoder_config dc = ma_decoder_config_init(ma_format_f32, 0, 0);
        ma_decoder decoder;
        const ma_result r = ma_decoder_init_memory(encoded->data(), encoded->size(), &dc, &decoder);
        if (r != MA_SUCCESS) return fail(makeError(Error::Code::ParseError, key + ": unsupported or corrupt audio file (" + resultText(r) + ")"));
        const u32 channels = decoder.outputChannels, sampleRate = decoder.outputSampleRate;
        if (channels == 0 || channels > 2) {
            ma_decoder_uninit(&decoder);
            return fail(makeError(Error::Code::Unsupported, key + ": only mono and stereo audio are supported"));
        }
        if (stream) {
            ma_uint64 length = 0;
            if (ma_decoder_get_length_in_pcm_frames(&decoder, &length) == MA_SUCCESS && clip->pcm.empty()) clip->frames = length;
            clip->encoded = encoded;
        } else {
            // Decode everything (the length is not always known in advance: read in chunks).
            std::vector<float> pcm;
            constexpr ma_uint64 kChunk = 16384;
            for (;;) {
                const usize at = pcm.size();
                pcm.resize(at + kChunk * channels);
                ma_uint64 read = 0;
                const ma_result rr = ma_decoder_read_pcm_frames(&decoder, pcm.data() + at, kChunk, &read);
                pcm.resize(at + read * channels);
                if (read < kChunk || rr != MA_SUCCESS) break;
            }
            if (pcm.empty()) {
                ma_decoder_uninit(&decoder);
                return fail(makeError(Error::Code::ParseError, key + ": the file contains no audio"));
            }
            pcm.shrink_to_fit();
            clip->frames = pcm.size() / channels;
            clip->pcm = std::move(pcm);
        }
        clip->channels = channels;
        clip->sampleRate = sampleRate;
        ma_decoder_uninit(&decoder);
        return clip;
    }

    // --------------------------------------------------------------- voices
    void enforceVoiceLimit() {
        while (voices.size() >= std::max<u32>(1, config.maxVoices)) {
            Voice* oldest = nullptr;
            for (auto& [id, v] : voices)
                if (!v->loop && (!oldest || v->order < oldest->order)) oldest = v.get();
            if (!oldest) return;  // only loops: the caller fails
            forget(oldest->id);
            ++stats.voicesStolen;
        }
    }

    void forget(VoiceId id) {
        auto it = voices.find(id);
        if (it == voices.end()) return;
        if (it->second->entity) {
            auto s = sources.find(it->second->entity);
            if (s != sources.end() && s->second == id) sources.erase(s);
        }
        voices.erase(it);
    }

    Result<VoiceId> play(std::string_view uri, const PlayParams& p, u64 entity) {
        if (!engineInit) return makeError(Error::Code::InvalidArgument, "audio is not initialized");
        auto clip = loadClip(uri, p.stream);
        if (!clip) return clip.error();
        enforceVoiceLimit();
        if (voices.size() >= std::max<u32>(1, config.maxVoices))
            return makeError(Error::Code::Unsupported, "too many looping voices (" + std::to_string(voices.size()) + ")");

        auto v = std::make_unique<Voice>();
        v->id = nextId++;
        v->order = nextOrder++;
        v->uri = std::string(uri);
        v->clip = clip.value();
        v->bus = p.bus;
        v->loop = p.loop;
        v->entity = entity;
        v->spatial = p.spatial;
        ma_data_source* source = nullptr;
        if (p.stream) {
            v->streamed = true;
            v->encoded = v->clip->encoded;
            ma_decoder_config dc = ma_decoder_config_init(ma_format_f32, 0, 0);
            if (ma_result r = ma_decoder_init_memory(v->encoded->data(), v->encoded->size(), &dc, &v->decoder); r != MA_SUCCESS)
                return makeError(Error::Code::ParseError, v->uri + ": " + resultText(r));
            v->decoderInit = true;
            source = &v->decoder;
        } else {
            if (ma_result r = ma_audio_buffer_ref_init(ma_format_f32, v->clip->channels, v->clip->pcm.data(), v->clip->frames, &v->buffer);
                r != MA_SUCCESS)
                return makeError(Error::Code::Unknown, v->uri + ": " + resultText(r));
            v->buffer.sampleRate = v->clip->sampleRate;
            v->bufferInit = true;
            source = &v->buffer;
        }
        const i32 bus = std::clamp(static_cast<i32>(p.bus), 0, kAudioBusCount - 1);
        if (ma_result r = ma_sound_init_from_data_source(&engine, source, 0, &groups[bus], &v->sound); r != MA_SUCCESS)
            return makeError(Error::Code::Unknown, v->uri + ": " + resultText(r));
        v->soundInit = true;
        ma_sound_set_looping(&v->sound, p.loop ? MA_TRUE : MA_FALSE);
        ma_sound_set_volume(&v->sound, std::max(0.0f, p.volume));
        ma_sound_set_pitch(&v->sound, std::clamp(p.pitch, 0.01f, 16.0f));
        ma_sound_set_doppler_factor(&v->sound, 0.0f);
        applySpatial(*v, p.spatial, p.position, p.minDistance, p.maxDistance);
        if (!p.spatial) ma_sound_set_pan(&v->sound, std::clamp(p.pan, -1.0f, 1.0f));
        if (p.fadeInSeconds > 0.0f)
            ma_sound_set_fade_in_milliseconds(&v->sound, 0.0f, 1.0f, static_cast<ma_uint64>(p.fadeInSeconds * 1000.0f));
        if (ma_result r = ma_sound_start(&v->sound); r != MA_SUCCESS) return makeError(Error::Code::Unknown, v->uri + ": " + resultText(r));
        const VoiceId id = v->id;
        voices.emplace(id, std::move(v));
        return id;
    }

    static void applySpatial(Voice& v, bool spatial, math::Vec3 position, float minDistance, float maxDistance) {
        v.spatial = spatial;
        ma_sound_set_spatialization_enabled(&v.sound, spatial ? MA_TRUE : MA_FALSE);
        if (!spatial) return;
        ma_sound_set_position(&v.sound, position.x, position.y, position.z);
        const float mn = std::max(0.01f, minDistance);
        ma_sound_set_min_distance(&v.sound, mn);
        ma_sound_set_max_distance(&v.sound, std::max(mn, maxDistance));
    }

    void stop(VoiceId id, float fade) {
        Voice* v = find(id);
        if (!v) return;
        if (fade > 0.0f && !v->paused && output != AudioOutput::Manual) {
            ma_sound_stop_with_fade_in_milliseconds(&v->sound, static_cast<ma_uint64>(fade * 1000.0f));
            v->stopping = true;
            v->loop = false;
        } else {
            forget(id);
        }
    }

    void reap() {
        for (auto it = voices.begin(); it != voices.end();) {
            Voice& v = *it->second;
            const bool finished = !v.paused && !ma_sound_is_playing(&v.sound) && (v.stopping || ma_sound_at_end(&v.sound));
            if (finished) {
                if (v.entity) {
                    auto s = sources.find(v.entity);
                    if (s != sources.end() && s->second == v.id) sources.erase(s);
                }
                it = voices.erase(it);
            } else {
                ++it;
            }
        }
    }

    void stopSourceVoices() {
        std::vector<VoiceId> ids;
        for (auto& [entity, id] : sources) ids.push_back(id);
        for (VoiceId id : ids) forget(id);
        sources.clear();
        started.clear();
    }
};

AudioSystem::AudioSystem(VirtualFileSystem& vfs) : m(std::make_unique<Impl>(vfs)) {}
AudioSystem::~AudioSystem() { shutdown(); }

bool AudioSystem::initialized() const { return m->engineInit; }

Status AudioSystem::initialize(const AudioConfig& config) {
    shutdown();
    m->config = config;
    const u32 rate = std::clamp<u32>(config.sampleRate, 8000, 192000);

    ma_engine_config ec = ma_engine_config_init();
    ec.noDevice = MA_TRUE;  // AGR owns the device (master meter, suspend / resume)
    ec.channels = kChannels;
    ec.sampleRate = rate;
    if (ma_result r = ma_engine_init(&ec, &m->engine); r != MA_SUCCESS)
        return makeError(Error::Code::Unknown, std::string("audio engine: ") + resultText(r));
    m->engineInit = true;

    for (i32 i = 0; i < kAudioBusCount; ++i) {
        ma_sound_group* parent = i == 0 ? nullptr : &m->groups[0];
        if (ma_result r = ma_sound_group_init(&m->engine, 0, parent, &m->groups[i]); r != MA_SUCCESS) {
            shutdown();
            return makeError(Error::Code::Unknown, std::string("audio bus: ") + resultText(r));
        }
        m->groupInit[i] = true;
        m->applyBus(i);
    }
    m->stats.sampleRate = rate;

    m->output = config.output;
    if (config.output == AudioOutput::Device) {
        ma_device_config dc = ma_device_config_init(ma_device_type_playback);
        dc.playback.format = ma_format_f32;
        dc.playback.channels = kChannels;
        dc.sampleRate = rate;
        dc.dataCallback = &Impl::dataCallback;
        dc.pUserData = m.get();
        ma_result r = ma_device_init(nullptr, &dc, &m->device);
        if (r == MA_SUCCESS) {
            m->deviceInit = true;
            r = ma_device_start(&m->device);
            m->deviceRunning = r == MA_SUCCESS;
        }
        if (m->deviceRunning) {
            m->stats.deviceActive = true;
            m->stats.backend = ma_get_backend_name(m->device.pContext->backend);
            AGR_LOG_INFO("Audio", "output: {} ({} Hz, stereo)", m->stats.backend, rate);
        } else {
            if (m->deviceInit) ma_device_uninit(&m->device);
            m->deviceInit = false;
            m->output = AudioOutput::Virtual;
            AGR_LOG_WARN("Audio", "no audio output available ({}): sounds are mixed silently", resultText(r));
        }
    }
    if (m->output != AudioOutput::Device) m->stats.backend = m->output == AudioOutput::Virtual ? "virtual" : "manual";
    return {};
}

void AudioSystem::shutdown() {
    if (m->deviceInit) {
        ma_device_uninit(&m->device);  // stops the audio thread before anything is destroyed
        m->deviceInit = m->deviceRunning = false;
    }
    m->voices.clear();
    m->sources.clear();
    m->started.clear();
    m->clips.clear();
    for (i32 i = kAudioBusCount - 1; i >= 0; --i)
        if (m->groupInit[i]) {
            ma_sound_group_uninit(&m->groups[i]);
            m->groupInit[i] = false;
        }
    if (m->engineInit) {
        ma_engine_uninit(&m->engine);
        m->engineInit = false;
    }
    m->stats = {};
}

// ------------------------------------------------------------------ voices
Result<VoiceId> AudioSystem::play(std::string_view uri, const PlayParams& params) { return m->play(uri, params, 0); }
void AudioSystem::stop(VoiceId voice, float fadeOutSeconds) { m->stop(voice, fadeOutSeconds); }

void AudioSystem::setPaused(VoiceId voice, bool paused) {
    Voice* v = m->find(voice);
    if (!v || v->paused == paused || v->stopping) return;
    v->paused = paused;
    if (paused) ma_sound_stop(&v->sound);
    else ma_sound_start(&v->sound);
}

bool AudioSystem::isPlaying(VoiceId voice) const {
    const Voice* v = m->find(voice);
    if (!v || v->stopping) return false;
    return v->paused || ma_sound_is_playing(&v->sound) || !ma_sound_at_end(&v->sound);
}

void AudioSystem::setVolume(VoiceId voice, float volume) {
    if (Voice* v = m->find(voice)) ma_sound_set_volume(&v->sound, std::max(0.0f, volume));
}
void AudioSystem::setPitch(VoiceId voice, float pitch) {
    if (Voice* v = m->find(voice)) ma_sound_set_pitch(&v->sound, std::clamp(pitch, 0.01f, 16.0f));
}
void AudioSystem::setPosition(VoiceId voice, math::Vec3 p) {
    if (Voice* v = m->find(voice); v && v->spatial) ma_sound_set_position(&v->sound, p.x, p.y, p.z);
}
void AudioSystem::stopAll() {
    m->voices.clear();
    m->sources.clear();
}

// ------------------------------------------------------------------- mixer
void AudioSystem::setBusVolume(AudioBus bus, float volume) {
    const i32 i = static_cast<i32>(bus);
    if (i < 0 || i >= kAudioBusCount) return;
    m->busVolumes[i] = std::clamp(volume, 0.0f, 4.0f);
    m->applyBus(i);
}
float AudioSystem::busVolume(AudioBus bus) const {
    const i32 i = static_cast<i32>(bus);
    return i >= 0 && i < kAudioBusCount ? m->busVolumes[i] : 0.0f;
}
void AudioSystem::setBusMuted(AudioBus bus, bool muted) {
    const i32 i = static_cast<i32>(bus);
    if (i < 0 || i >= kAudioBusCount) return;
    m->busMutedFlags[i] = muted;
    m->applyBus(i);
}
bool AudioSystem::busMuted(AudioBus bus) const {
    const i32 i = static_cast<i32>(bus);
    return i >= 0 && i < kAudioBusCount && m->busMutedFlags[i];
}

void AudioSystem::setListener(math::Vec3 p, math::Vec3 forward, math::Vec3 up) {
    if (!m->engineInit) return;
    ma_engine_listener_set_position(&m->engine, 0, p.x, p.y, p.z);
    ma_engine_listener_set_direction(&m->engine, 0, forward.x, forward.y, forward.z);
    ma_engine_listener_set_world_up(&m->engine, 0, up.x, up.y, up.z);
}

void AudioSystem::suspend(bool suspended) {
    if (m->suspended == suspended) return;
    m->suspended = suspended;
    if (!m->deviceInit) return;
    if (suspended && m->deviceRunning) {
        ma_device_stop(&m->device);
        m->deviceRunning = false;
    } else if (!suspended && !m->deviceRunning) {
        m->deviceRunning = ma_device_start(&m->device) == MA_SUCCESS;
        if (!m->deviceRunning) AGR_LOG_WARN("Audio", "the audio output could not be resumed");
    }
}

// ------------------------------------------------------------------- frame
void AudioSystem::update(f64 deltaSeconds) {
    if (!m->engineInit) return;
    if (m->output == AudioOutput::Virtual && !m->suspended) {
        m->virtualAccumulator += std::clamp(deltaSeconds, 0.0, 0.25) * m->stats.sampleRate;
        const u32 frames = static_cast<u32>(m->virtualAccumulator);
        m->virtualAccumulator -= frames;
        if (frames > 0) {
            m->scratch.resize(static_cast<usize>(frames) * kChannels);
            m->render(m->scratch.data(), frames);
        }
    }
    m->reap();
}

void AudioSystem::mix(float* out, u32 frames) {
    if (!m->engineInit || m->output == AudioOutput::Device) {
        std::memset(out, 0, sizeof(float) * frames * kChannels);
        return;
    }
    m->render(out, frames);
}

// ------------------------------------------------------------------- clips
Status AudioSystem::preload(std::string_view uri) {
    auto c = m->loadClip(uri, false);
    if (!c) return c.error();
    return {};
}

void AudioSystem::unloadUnused() {
    for (auto it = m->clips.begin(); it != m->clips.end();) {
        if (it->second.use_count() == 1) it = m->clips.erase(it);
        else ++it;
    }
}

Result<f64> AudioSystem::clipDuration(std::string_view uri) {
    auto c = m->loadClip(uri, false);
    if (!c) return c.error();
    return static_cast<f64>(c.value()->frames) / std::max<u32>(1, c.value()->sampleRate);
}

// ------------------------------------------------------------------- scene
void AudioSystem::updateScene(Scene& scene, bool running) {
    if (!m->engineInit) return;
    if (scene.generation() != m->sceneGeneration) {
        m->stopSourceVoices();
        m->sceneGeneration = scene.generation();
    }
    if (!running) {
        if (!m->sources.empty() || !m->started.empty()) m->stopSourceVoices();
        return;
    }
    auto& reg = scene.registry();

    // Listener: first active AudioListener, else the primary camera.
    bool listenerSet = false;
    reg.view<AudioListenerComponent, WorldTransformComponent>().each([&](ecs::Entity e, AudioListenerComponent& l, WorldTransformComponent& w) {
        if (listenerSet || !l.active || !scene.isActiveInHierarchy(e)) return;
        listenerSet = true;
        setListener(w.matrix.translation(), math::normalize(w.matrix.transformVector({0, 0, -1})), math::normalize(w.matrix.transformVector({0, 1, 0})));
    });
    if (!listenerSet) {
        reg.view<CameraComponent, WorldTransformComponent>().each([&](ecs::Entity e, CameraComponent& c, WorldTransformComponent& w) {
            if (listenerSet || !c.primary || !scene.isActiveInHierarchy(e)) return;
            listenerSet = true;
            setListener(w.matrix.translation(), math::normalize(w.matrix.transformVector({0, 0, -1})),
                        math::normalize(w.matrix.transformVector({0, 1, 0})));
        });
    }

    // Sources: start, follow, stop.
    std::unordered_set<u64> alive;
    reg.view<AudioSourceComponent, WorldTransformComponent>().each([&](ecs::Entity e, AudioSourceComponent& s, WorldTransformComponent& w) {
        const u64 key = e.packed();
        if (!scene.isActiveInHierarchy(e)) return;  // inactive: its voice stops below, restarts when active again
        alive.insert(key);
        auto it = m->sources.find(key);
        if (it == m->sources.end()) {
            if (s.playOnStart && !s.clip.empty() && !m->started.count(key)) {
                m->started.insert(key);
                if (playSource(scene, e) == 0) return;
            }
            return;
        }
        Voice* v = m->find(it->second);
        if (!v) return;
        ma_sound_set_volume(&v->sound, std::max(0.0f, s.volume));
        ma_sound_set_pitch(&v->sound, std::clamp(s.pitch, 0.01f, 16.0f));
        if (v->loop != s.loop && !v->stopping) {
            v->loop = s.loop;
            ma_sound_set_looping(&v->sound, s.loop ? MA_TRUE : MA_FALSE);
        }
        if (v->spatial != s.spatial || s.spatial) Impl::applySpatial(*v, s.spatial, w.matrix.translation(), s.minDistance, s.maxDistance);
        if (v->uri != s.clip || v->bus != s.bus || v->streamed != s.stream) {
            // Clip, bus or streaming changed while playing: restart with the new settings.
            m->forget(it->second);
            playSource(scene, e);
        }
    });
    std::vector<u64> gone;
    for (auto& [key, id] : m->sources)
        if (!alive.count(key)) gone.push_back(key);
    for (u64 key : gone) {
        m->forget(m->sources[key]);
        m->sources.erase(key);
    }
    for (auto it = m->started.begin(); it != m->started.end();) {
        if (!alive.count(*it)) it = m->started.erase(it);  // re-enabled or re-created: plays again
        else ++it;
    }
}

VoiceId AudioSystem::playSource(Scene& scene, ecs::Entity entity) {
    auto& reg = scene.registry();
    if (!reg.valid(entity)) return 0;
    auto* s = reg.tryGet<AudioSourceComponent>(entity);
    if (!s || s->clip.empty()) return 0;
    if (auto it = m->sources.find(entity.packed()); it != m->sources.end()) m->forget(it->second);
    PlayParams p;
    p.bus = s->bus;
    p.volume = s->volume;
    p.pitch = s->pitch;
    p.loop = s->loop;
    p.stream = s->stream;
    p.spatial = s->spatial;
    p.minDistance = s->minDistance;
    p.maxDistance = s->maxDistance;
    if (auto* w = reg.tryGet<WorldTransformComponent>(entity)) p.position = w->matrix.translation();
    auto voice = m->play(s->clip, p, entity.packed());
    if (!voice) {
        AGR_LOG_WARN("Audio", "{}", voice.error().message);
        return 0;
    }
    m->sources[entity.packed()] = voice.value();
    m->started.insert(entity.packed());
    if (m->sourcesPaused) setPaused(voice.value(), true);
    return voice.value();
}

void AudioSystem::stopSource(ecs::Entity entity, float fadeOutSeconds) {
    auto it = m->sources.find(entity.packed());
    if (it != m->sources.end()) m->stop(it->second, fadeOutSeconds);
}

void AudioSystem::pauseSources(bool paused) {
    m->sourcesPaused = paused;
    // Game pause: every voice except the Interface bus (menus keep their sounds).
    std::vector<VoiceId> ids;
    for (auto& [id, v] : m->voices)
        if (v->bus != AudioBus::Interface) ids.push_back(id);
    for (VoiceId id : ids) setPaused(id, paused);
}

bool AudioSystem::sourcePlaying(ecs::Entity entity) const {
    auto it = m->sources.find(entity.packed());
    return it != m->sources.end() && isPlaying(it->second);
}

void AudioSystem::resetScene() { m->stopSourceVoices(); }

const AudioStats& AudioSystem::stats() const {
    m->stats.voices = static_cast<u32>(m->voices.size());
    m->stats.clips = static_cast<u32>(m->clips.size());
    u64 bytes = 0;
    for (auto& [uri, c] : m->clips) bytes += c->pcm.size() * sizeof(float) + (c->encoded ? c->encoded->size() : 0);
    m->stats.clipBytes = bytes;
    m->stats.framesMixed = m->framesMixed.load(std::memory_order_relaxed);
    return m->stats;
}

AudioMeter AudioSystem::takeMeter() {
    return {bitsFloat(m->peakL.exchange(0, std::memory_order_relaxed)), bitsFloat(m->peakR.exchange(0, std::memory_order_relaxed))};
}

} // namespace agr
