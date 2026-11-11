#include "Audio.h"
#include "../core/Platform.h"
#include <cstring>
#include <algorithm>

#define STB_VORBIS_NO_STDIO
#define STB_VORBIS_NO_PUSHDATA_API
#include "../third_party/stb_vorbis.c"
#undef L
#undef C
#undef R

namespace agr {

bool decodeOgg(const std::vector<uint8_t>& bytes, SoundBuffer& out) {
    int ch = 0, rate = 0;
    short* data = nullptr;
    int n = stb_vorbis_decode_memory(bytes.data(), (int)bytes.size(), &ch, &rate, &data);
    if (n <= 0 || !data) return false;
    out.channels = ch; out.rate = rate;
    out.data.assign(data, data + (size_t)n * ch);
    free(data);
    return true;
}

bool OggStream::open(std::vector<uint8_t>&& bytes) {
    close();
    file = std::move(bytes);
    int err = 0;
    vb = stb_vorbis_open_memory(file.data(), (int)file.size(), &err, nullptr);
    if (!vb) return false;
    stb_vorbis_info inf = stb_vorbis_get_info(vb);
    channels = inf.channels; rate = inf.sample_rate;
    return true;
}

int OggStream::read(float* out, int frames) {
    if (!vb) { memset(out, 0, frames * 2 * sizeof(float)); return frames; }
    int got = 0;
    while (got < frames) {
        int n = stb_vorbis_get_samples_float_interleaved(vb, 2, out + got * 2, (frames - got) * 2);
        if (n <= 0) { stb_vorbis_seek_start(vb); n = stb_vorbis_get_samples_float_interleaved(vb, 2, out + got * 2, (frames - got) * 2); if (n <= 0) break; }
        got += n;
    }
    if (got < frames) memset(out + got * 2, 0, (frames - got) * 2 * sizeof(float));
    return frames;
}

void OggStream::close() {
    if (vb) stb_vorbis_close(vb);
    vb = nullptr;
}

bool AudioEngine::loadSound(const std::string& name, const std::vector<uint8_t>& ogg) {
    SoundBuffer b;
    if (!decodeOgg(ogg, b)) { AGR_WARN("audio: cannot decode %s", name.c_str()); return false; }
    soundNames.push_back(name);
    sounds.push_back(std::move(b));
    return true;
}

SoundId AudioEngine::id(const std::string& name) const {
    for (size_t i = 0; i < soundNames.size(); i++) if (soundNames[i] == name) return (SoundId)i;
    return -1;
}

void AudioEngine::addMusic(const std::string& name, std::vector<uint8_t>&& ogg) { musicData.push_back({name, std::move(ogg)}); }
void AudioEngine::addAmbience(const std::string& name, std::vector<uint8_t>&& ogg) { ambData.push_back({name, std::move(ogg)}); }

std::shared_ptr<OggStream> AudioEngine::openStream(const std::vector<std::pair<std::string, std::vector<uint8_t>>>& lib, const std::string& name) {
    for (auto& kv : lib)
        if (kv.first == name) {
            auto s = std::make_shared<OggStream>();
            std::vector<uint8_t> copy = kv.second;
            if (s->open(std::move(copy))) return s;
        }
    return nullptr;
}

void AudioEngine::play(SoundId s, float vol, float pitch, float pan) {
    if (s < 0 || s >= (int)sounds.size()) return;
    std::lock_guard<std::mutex> l(mtx);
    if (voices.size() >= 48) {  // voice stealing: drop the oldest
        voices.erase(voices.begin());
    }
    pan = clampf(pan, -1, 1);
    float a = (pan + 1) * PI / 4;  // equal-power panning
    voices.push_back({&sounds[s], 0.0, pitch, vol * std::cos(a) * 1.4142f, vol * std::sin(a) * 1.4142f, true});
}

void AudioEngine::play3D(SoundId s, vec3 pos, float vol, float pitch, float range) {
    vec3 d;
    float right;
    {
        std::lock_guard<std::mutex> l(mtx);
        d = pos - listener;
        right = dot(d, listenerRight);
    }
    float dist = length(d);
    if (dist > range) return;
    // 2.5D spatialisation: inverse-ish rolloff + stereo pan from screen-space horizontal offset
    float att = 1.f / (1.f + std::pow(std::max(0.f, dist - 3.f) / (range * 0.25f), 2.f));
    float pan = clampf(right / 14.f, -0.85f, 0.85f);
    play(s, vol * att, pitch, pan);
}

void AudioEngine::playMusic(const std::string& name, float fade) {
    std::lock_guard<std::mutex> l(mtx);
    if (name == musicName) return;
    musicName = name;
    Deck& old = decks[curDeck];
    old.target = 0; old.fadeRate = 1.f / std::max(0.1f, fade);
    curDeck ^= 1;
    Deck& nd = decks[curDeck];
    nd.stream = name.empty() ? nullptr : openStream(musicData, name);
    nd.name = name;
    nd.vol = 0; nd.target = nd.stream ? 1.f : 0.f; nd.fadeRate = 1.f / std::max(0.1f, fade);
}

void AudioEngine::setAmbience(const std::string& name, float volume) {
    std::lock_guard<std::mutex> l(mtx);
    for (auto& a : ambs) if (a.name == name) { a.target = volume; return; }
    if (volume <= 0) return;
    Amb a; a.name = name; a.stream = openStream(ambData, name); a.vol = 0; a.target = volume;
    if (a.stream) ambs.push_back(a);
}

void AudioEngine::setVolumes(float m, float mu, float s, float a) {
    std::lock_guard<std::mutex> l(mtx);
    master = m; musicVol = mu; sfxVol = s; ambVol = a;
}

void AudioEngine::render(float* out, int frames, int rate) {
    memset(out, 0, sizeof(float) * frames * 2);
    if (paused) return;
    std::lock_guard<std::mutex> l(mtx);
    float dtBlock = (float)frames / rate;
    if ((int)scratch.size() < frames * 2) scratch.resize(frames * 2);
    if ((int)sfxBus.size() < frames * 2) sfxBus.resize(frames * 2);
    // Music decks (crossfade)
    for (auto& d : decks) {
        if (d.target > d.vol) d.vol = std::min(d.target, d.vol + d.fadeRate * dtBlock);
        else d.vol = std::max(d.target, d.vol - d.fadeRate * dtBlock);
        if (!d.stream || d.vol <= 0.0001f) { if (d.target <= 0) d.stream.reset(); continue; }
        d.stream->read(scratch.data(), frames);  // assets are authored at the output rate class (44.1/48k): minor pitch drift acceptable
        float g = d.vol * musicVol * musicDuck * master;
        for (int i = 0; i < frames * 2; i++) out[i] += scratch[i] * g;
    }
    // Ambience layers
    for (auto& a : ambs) {
        a.vol += (a.target - a.vol) * std::min(1.f, dtBlock * 0.8f);
        if (a.vol < 0.002f) continue;
        a.stream->read(scratch.data(), frames);
        float g = a.vol * ambVol * master;
        for (int i = 0; i < frames * 2; i++) out[i] += scratch[i] * g;
    }
    ambs.erase(std::remove_if(ambs.begin(), ambs.end(), [](const Amb& a) { return a.target <= 0 && a.vol < 0.002f; }), ambs.end());
    // SFX voices into a bus (for reverb send)
    memset(sfxBus.data(), 0, sizeof(float) * frames * 2);
    int active = 0;
    for (auto& v : voices) {
        if (!v.active) continue;
        const SoundBuffer& b = *v.buf;
        double step = v.pitch * (double)b.rate / rate;
        size_t nf = b.frames();
        for (int i = 0; i < frames; i++) {
            size_t i0 = (size_t)v.pos;
            if (i0 + 1 >= nf) { v.active = false; break; }
            float f = (float)(v.pos - i0);
            float sl, sr;
            if (b.channels == 1) {
                float s = (b.data[i0] * (1 - f) + b.data[i0 + 1] * f) / 32768.f;
                sl = sr = s;
            } else {
                sl = (b.data[i0 * 2] * (1 - f) + b.data[i0 * 2 + 2] * f) / 32768.f;
                sr = (b.data[i0 * 2 + 1] * (1 - f) + b.data[i0 * 2 + 3] * f) / 32768.f;
            }
            sfxBus[i * 2] += sl * v.volL;
            sfxBus[i * 2 + 1] += sr * v.volR;
            v.pos += step;
        }
        if (v.active) active++;
    }
    voices.erase(std::remove_if(voices.begin(), voices.end(), [](const Voice& v) { return !v.active; }), voices.end());
    voiceCount = active;
    // Reverb on the SFX bus (dungeons, caves)
    reverb += (reverbTarget - reverb) * std::min(1.f, dtBlock * 2.f);
    if (reverbRate != rate) {
        reverbRate = rate;
        const int combMs[4] = {29, 37, 41, 43};
        for (int k = 0; k < 4; k++) { comb[k].assign((size_t)(rate * combMs[k] / 1000), 0.f); combPos[k] = 0; }
        ap[0].assign((size_t)(rate * 5 / 1000), 0.f); ap[1].assign((size_t)(rate * 17 / 10000), 0.f);
        apPos[0] = apPos[1] = 0;
    }
    float g = sfxVol * master;
    for (int i = 0; i < frames; i++) {
        float l = sfxBus[i * 2], r = sfxBus[i * 2 + 1];
        if (reverb > 0.01f) {
            float in = (l + r) * 0.5f, acc = 0;
            for (int k = 0; k < 4; k++) {
                float y = comb[k][combPos[k]];
                combFilt[k] = y * 0.7f + combFilt[k] * 0.3f;  // damping
                comb[k][combPos[k]] = in + combFilt[k] * 0.78f;
                combPos[k] = (combPos[k] + 1) % comb[k].size();
                acc += y;
            }
            for (int k = 0; k < 2; k++) {
                float buf = ap[k][apPos[k]];
                float y = -acc + buf;
                ap[k][apPos[k]] = acc + buf * 0.5f;
                apPos[k] = (apPos[k] + 1) % ap[k].size();
                acc = y;
            }
            l += acc * reverb * 0.35f; r += acc * reverb * 0.35f;
        }
        out[i * 2] += l * g;
        out[i * 2 + 1] += r * g;
    }
    // soft limiter
    for (int i = 0; i < frames * 2; i++) { float x = out[i]; out[i] = x / (1.f + std::fabs(x) * 0.25f) * 1.05f; }
}

}  // namespace agr
