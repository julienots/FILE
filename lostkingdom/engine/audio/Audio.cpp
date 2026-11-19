#include "Audio.h"
#include "../core/Platform.h"
#include <cstring>
#include <algorithm>
#include <chrono>

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

bool OggStream::open(std::shared_ptr<const std::vector<uint8_t>> bytes) {
    close();
    file = std::move(bytes);
    if (!file || file->empty()) return false;
    int err = 0;
    vb = stb_vorbis_open_memory(file->data(), (int)file->size(), &err, nullptr);
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

void OggStream::readAt(float* out, int frames, int outRate) {
    if (!vb || outRate <= 0 || rate == outRate) { read(out, frames); return; }
    // FIFO of decoded stereo frames + fractional read position (no seeking: stb_vorbis seeks are expensive)
    double step = (double)rate / outRate;
    int have = (int)rbuf.size() / 2;
    int need = (int)std::ceil(rpos + frames * step) + 2;
    if (need > have) {
        rbuf.resize((size_t)need * 2);
        read(rbuf.data() + have * 2, need - have);
        have = need;
    }
    for (int i = 0; i < frames; i++) {
        double x = rpos + i * step; int i0 = (int)x; float f = (float)(x - i0);
        out[i * 2] = rbuf[i0 * 2] * (1 - f) + rbuf[i0 * 2 + 2] * f;
        out[i * 2 + 1] = rbuf[i0 * 2 + 1] * (1 - f) + rbuf[i0 * 2 + 3] * f;
    }
    double end = rpos + frames * step;
    int used = std::min((int)end, have);
    rbuf.erase(rbuf.begin(), rbuf.begin() + (size_t)used * 2);
    rpos = end - used;
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

void AudioEngine::addMusic(const std::string& name, std::vector<uint8_t>&& ogg) { musicData.push_back({name, std::make_shared<const std::vector<uint8_t>>(std::move(ogg))}); }
void AudioEngine::addAmbience(const std::string& name, std::vector<uint8_t>&& ogg) { ambData.push_back({name, std::make_shared<const std::vector<uint8_t>>(std::move(ogg))}); }

// Called WITHOUT the mixer lock: opening a Vorbis stream parses headers (and used to copy the whole file)
std::shared_ptr<OggStream> AudioEngine::openStream(const Lib& lib, const std::string& name) {
    for (auto& kv : lib)
        if (kv.first == name) {
            auto s = std::make_shared<OggStream>();
            if (s->open(kv.second)) return s;
        }
    return nullptr;
}

static double nowSec() { return std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count(); }

void AudioEngine::play(SoundId s, float vol, float pitch, float pan) { playVoice(s, vol, pitch, pan, 1.f); }

void AudioEngine::playVoice(SoundId s, float vol, float pitch, float pan, float lp) {
    if (s < 0 || s >= (int)sounds.size() || vol <= 0.001f) return;
    // the same sound fired twice within 35 ms (several hits in one frame, two systems reacting to one event) is
    // merged into the voice already started: no doubled volume, no flanging
    if ((int)lastStart.size() < (int)sounds.size()) lastStart.resize(sounds.size(), -1.0);
    double t = nowSec();
    bool dup = t - lastStart[s] < 0.035;
    std::lock_guard<std::mutex> l(mtx);
    pan = clampf(pan, -1, 1);
    float a = (pan + 1) * PI / 4;  // equal-power panning
    float vl = vol * std::cos(a) * 1.4142f, vr = vol * std::sin(a) * 1.4142f;
    if (dup) {
        for (auto it = voices.rbegin(); it != voices.rend(); ++it)
            if (it->id == s && it->active && it->fadeOut < 0) { it->volL = std::max(it->volL, vl); it->volR = std::max(it->volR, vr); it->lp = std::max(it->lp, lp); return; }
    }
    lastStart[s] = t;
    // voice stealing: the quietest old voice fades out over ~8 ms instead of being cut (clicks)
    int live = 0; for (auto& v : voices) if (v.fadeOut < 0) live++;
    if (live >= 40) {
        Voice* q = nullptr;
        for (auto& v : voices) if (v.fadeOut < 0 && (!q || v.volL + v.volR < q->volL + q->volR)) q = &v;
        if (q) q->fadeOut = 384;
    }
    if (voices.size() >= 64) voices.erase(voices.begin());
    Voice v{&sounds[s], 0.0, pitch, vl, vr, true};
    v.id = s; v.lp = clampf(lp, 0.02f, 1.f);
    voices.push_back(v);
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
    // 2.3.4 air absorption: far sounds lose their highs (one-pole low-pass, ~18 kHz near -> ~2 kHz at the range limit)
    float far = saturate((dist - 4.f) / std::max(1.f, range - 4.f));
    float lp = 1.f - 0.85f * std::sqrt(far);
    playVoice(s, vol * att, pitch, pan, lp);
}

void AudioEngine::playMusic(const std::string& name, float fade) {
    { std::lock_guard<std::mutex> l(mtx); if (name == musicName) return; }
    std::shared_ptr<OggStream> st = name.empty() ? nullptr : openStream(musicData, name);   // outside the mixer lock
    std::lock_guard<std::mutex> l(mtx);
    if (name == musicName) return;
    musicName = name;
    Deck& old = decks[curDeck];
    old.target = 0; old.fadeRate = 1.f / std::max(0.1f, fade);
    curDeck ^= 1;
    Deck& nd = decks[curDeck];
    nd.stream = st;
    nd.name = name;
    nd.vol = 0; nd.target = nd.stream ? 1.f : 0.f; nd.fadeRate = 1.f / std::max(0.1f, fade);
}

void AudioEngine::setAmbience(const std::string& name, float volume) {
    {
        std::lock_guard<std::mutex> l(mtx);
        for (auto& a : ambs) if (a.name == name) { a.target = volume; return; }
    }
    if (volume <= 0.005f) return;
    std::shared_ptr<OggStream> st = openStream(ambData, name);   // outside the mixer lock
    if (!st) return;
    std::lock_guard<std::mutex> l(mtx);
    for (auto& a : ambs) if (a.name == name) { a.target = volume; return; }
    Amb a; a.name = name; a.stream = st; a.vol = 0; a.target = volume;
    ambs.push_back(a);
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
        d.stream->readAt(scratch.data(), frames, rate);
        float g = d.vol * musicVol * musicDuck * master;
        for (int i = 0; i < frames * 2; i++) out[i] += scratch[i] * g;
    }
    // Ambience layers
    for (auto& a : ambs) {
        a.vol += (a.target - a.vol) * std::min(1.f, dtBlock * 0.8f);
        if (a.vol < 0.002f) continue;
        a.stream->readAt(scratch.data(), frames, rate);
        float g = a.vol * ambVol * master;
        for (int i = 0; i < frames * 2; i++) out[i] += scratch[i] * g;
    }
    ambs.erase(std::remove_if(ambs.begin(), ambs.end(), [](const Amb& a) { return a.target <= 0 && a.vol < 0.002f; }), ambs.end());
    // SFX voices into a bus (for reverb send)
    memset(sfxBus.data(), 0, sizeof(float) * frames * 2);
    int active = 0;
    // vehicle engine: one looping voice, volume and pitch glide towards the game's targets
    if (engineSound >= 0 && engineSound < (int)sounds.size() && (engineVol > 0.0005f || engineTarget > 0)) {
        const SoundBuffer& b = sounds[engineSound];
        size_t nf = b.frames();
        if (nf > 2) {
            for (int i = 0; i < frames; i++) {
                engineVol += (engineTarget - engineVol) * 0.0008f;
                enginePitchCur += (enginePitch - enginePitchCur) * 0.0006f;
                size_t i0 = (size_t)enginePos % (nf - 1);
                float f = (float)(enginePos - std::floor(enginePos));
                float s = b.channels == 1 ? (b.data[i0] * (1 - f) + b.data[i0 + 1] * f) / 32768.f : b.data[i0 * 2] / 32768.f;
                sfxBus[i * 2] += s * engineVol; sfxBus[i * 2 + 1] += s * engineVol;
                enginePos += enginePitchCur * (double)b.rate / rate;
                if (enginePos >= nf - 1) enginePos -= (nf - 1);
            }
        }
    }
    for (auto& v : voices) {
        if (!v.active) continue;
        const SoundBuffer& b = *v.buf;
        double step = v.pitch * (double)b.rate / rate;
        size_t nf = b.frames();
        const double fadeIn = 0.0015 * b.rate, fadeEnd = 0.004 * b.rate;   // ~1.5 ms in, ~4 ms out (no clicks)
        // one-pole low-pass coefficient (lp = 1: open)
        float k = v.lp >= 0.999f ? 1.f : clampf(v.lp * v.lp * 1.1f, 0.03f, 1.f);
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
            float e = 1.f;
            if (v.pos < fadeIn) e = (float)(v.pos / fadeIn);
            double left = (double)(nf - 1) - v.pos;
            if (left < fadeEnd) e = std::min(e, (float)(left / fadeEnd));
            if (v.fadeOut >= 0) { e *= v.fadeOut / 384.f; if (--v.fadeOut <= 0) { v.active = false; break; } }
            if (k < 1.f) { v.zl += (sl - v.zl) * k; v.zr += (sr - v.zr) * k; sl = v.zl; sr = v.zr; }
            sfxBus[i * 2] += sl * v.volL * e;
            sfxBus[i * 2 + 1] += sr * v.volR * e;
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
