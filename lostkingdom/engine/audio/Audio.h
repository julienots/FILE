// AGR Engine — audio runtime: software mixer driven by the platform audio callback (AAudio on Android).
#pragma once
#include "../core/Math.h"
#include <vector>
#include <string>
#include <mutex>
#include <memory>
#include <atomic>

struct stb_vorbis;

namespace agr {

struct SoundBuffer {
    std::vector<int16_t> data;  // interleaved
    int channels = 1, rate = 44100;
    size_t frames() const { return data.size() / channels; }
};

// Streaming OGG source (music, long ambiences)
struct OggStream {
    std::vector<uint8_t> file;
    stb_vorbis* vb = nullptr;
    int channels = 2, rate = 44100;
    bool open(std::vector<uint8_t>&& bytes);
    int read(float* out, int frames);  // interleaved stereo float, loops forever
    void close();
    ~OggStream() { close(); }
};

bool decodeOgg(const std::vector<uint8_t>& bytes, SoundBuffer& out);

using SoundId = int;

class AudioEngine {
public:
    bool loadSound(const std::string& name, const std::vector<uint8_t>& ogg);
    SoundId id(const std::string& name) const;
    void addMusic(const std::string& name, std::vector<uint8_t>&& ogg);
    void addAmbience(const std::string& name, std::vector<uint8_t>&& ogg);

    // Game-thread API ----------------------------------------------------------------------
    void setListener(vec3 pos, vec3 right) { std::lock_guard<std::mutex> l(mtx); listener = pos; listenerRight = right; }
    void play(SoundId s, float vol = 1.f, float pitch = 1.f, float pan = 0.f);
    void play3D(SoundId s, vec3 pos, float vol = 1.f, float pitch = 1.f, float range = 30.f);
    void playMusic(const std::string& name, float fade = 3.f);  // "" = silence
    void setAmbience(const std::string& name, float volume);   // target volume, crossfades smoothly
    void setVolumes(float master, float music, float sfx, float ambience);
    void setReverb(float wet) { reverbTarget = clampf(wet, 0, 0.7f); }
    void setPaused(bool p) { paused = p; }
    void setMusicDuck(float d) { musicDuck = d; }
    std::string currentMusic() const { return musicName; }

    // Audio-thread API ---------------------------------------------------------------------
    void render(float* out, int frames, int rate);

    int activeVoices() const { return voiceCount.load(); }

private:
    struct Voice { const SoundBuffer* buf; double pos; float pitch, volL, volR; bool active; };
    struct Deck { std::shared_ptr<OggStream> stream; float vol = 0, target = 0, fadeRate = 0.3f; std::string name; };
    struct Amb { std::string name; std::shared_ptr<OggStream> stream; float vol = 0, target = 0; };
    std::vector<std::string> soundNames;
    std::vector<SoundBuffer> sounds;
    std::vector<std::pair<std::string, std::vector<uint8_t>>> musicData, ambData;
    std::mutex mtx;
    std::vector<Voice> voices;
    Deck decks[2];
    int curDeck = 0;
    std::vector<Amb> ambs;
    vec3 listener, listenerRight{1, 0, 0};
    float master = 1, musicVol = 0.6f, sfxVol = 1, ambVol = 0.7f, musicDuck = 1.f;
    std::string musicName;
    std::atomic<bool> paused{false};
    std::atomic<int> voiceCount{0};
    // reverb (Schroeder: 4 combs + 2 allpasses)
    std::vector<float> comb[4], ap[2];
    size_t combPos[4] = {0}, apPos[2] = {0};
    float combFilt[4] = {0};
    float reverb = 0, reverbTarget = 0;
    int reverbRate = 0;
    std::vector<float> scratch, sfxBus;
    std::shared_ptr<OggStream> openStream(const std::vector<std::pair<std::string, std::vector<uint8_t>>>& lib, const std::string& name);
};

}  // namespace agr
