// agr.audio.* and Entity:playSound() / stopSound() / isSoundPlaying().
#include "ScriptInternal.hpp"
#include "agr/core/Log.hpp"

namespace agr {

namespace {

/// Without AudioPlugin (dedicated servers, tests) the audio functions do nothing.
AudioSystem* audioOf(lua_State* L) { return implOf(L).audio; }

AudioBus checkBus(lua_State* L, int idx) {
    AudioBus bus{};
    const char* name = luaL_checkstring(L, idx);
    if (!audioBusFromName(name, bus))
        luaL_error(L, "unknown audio bus '%s' (Master, Music, Effects, Ambient, Interface, Voice or a custom bus)", name);
    return bus;
}

float numberField(lua_State* L, int t, const char* key, float fallback) {
    lua_getfield(L, t, key);
    const float v = lua_isnumber(L, -1) ? static_cast<float>(lua_tonumber(L, -1)) : fallback;
    lua_pop(L, 1);
    return v;
}
bool boolField(lua_State* L, int t, const char* key, bool fallback) {
    lua_getfield(L, t, key);
    const bool v = lua_isnil(L, -1) ? fallback : lua_toboolean(L, -1) != 0;
    lua_pop(L, 1);
    return v;
}

VoiceId checkVoice(lua_State* L, int idx) { return static_cast<VoiceId>(luaL_checkinteger(L, idx)); }

// agr.audio.play(uri [, {volume, pitch, pan, loop, bus, x, y, z, minDistance, maxDistance, fadeIn, stream}])
// -> voice id, or nil and a message (missing or invalid file).
int au_play(lua_State* L) {
    const char* uri = luaL_checkstring(L, 1);
    PlayParams p;
    if (lua_istable(L, 2)) {
        p.volume = numberField(L, 2, "volume", 1.0f);
        p.pitch = numberField(L, 2, "pitch", 1.0f);
        p.pan = numberField(L, 2, "pan", 0.0f);
        p.loop = boolField(L, 2, "loop", false);
        p.stream = boolField(L, 2, "stream", false);
        p.fadeInSeconds = numberField(L, 2, "fadeIn", 0.0f);
        p.minDistance = numberField(L, 2, "minDistance", 1.0f);
        p.maxDistance = numberField(L, 2, "maxDistance", 30.0f);
        lua_getfield(L, 2, "bus");
        if (!lua_isnil(L, -1)) p.bus = checkBus(L, -1);
        lua_pop(L, 1);
        lua_getfield(L, 2, "x");
        p.spatial = !lua_isnil(L, -1);  // a position makes the voice spatial
        lua_pop(L, 1);
        p.position = {numberField(L, 2, "x", 0.0f), numberField(L, 2, "y", 0.0f), numberField(L, 2, "z", 0.0f)};
    } else if (!lua_isnoneornil(L, 2)) {
        luaL_checktype(L, 2, LUA_TTABLE);
    }
    AudioSystem* audio = audioOf(L);
    if (!audio) {
        lua_pushnil(L);
        lua_pushliteral(L, "audio is not enabled");
        return 2;
    }
    auto voice = audio->play(uri, p);
    if (!voice) {
        lua_pushnil(L);
        lua_pushstring(L, voice.error().message.c_str());
        return 2;
    }
    lua_pushinteger(L, static_cast<lua_Integer>(voice.value()));
    return 1;
}
int au_stop(lua_State* L) {
    const VoiceId v = checkVoice(L, 1);
    if (AudioSystem* a = audioOf(L)) a->stop(v, static_cast<float>(luaL_optnumber(L, 2, 0.0)));
    return 0;
}
int au_isPlaying(lua_State* L) {
    const VoiceId v = checkVoice(L, 1);
    AudioSystem* a = audioOf(L);
    lua_pushboolean(L, a && a->isPlaying(v));
    return 1;
}
int au_setVolume(lua_State* L) {
    const VoiceId v = checkVoice(L, 1);
    const float x = static_cast<float>(luaL_checknumber(L, 2));
    if (AudioSystem* a = audioOf(L)) a->setVolume(v, x);
    return 0;
}
int au_setPitch(lua_State* L) {
    const VoiceId v = checkVoice(L, 1);
    const float x = static_cast<float>(luaL_checknumber(L, 2));
    if (AudioSystem* a = audioOf(L)) a->setPitch(v, x);
    return 0;
}
/// agr.audio.addBusEffect(bus, type [, params]) -> index (1-based). type: "LowPass", "HighPass",
/// "Echo", "Reverb", "Compressor", "Distortion", "Chorus"; params: a table of AudioEffect fields.
float* effectField(AudioEffect& e, std::string_view name) {
    static const std::pair<const char*, float AudioEffect::*> fields[] = {
        {"cutoff", &AudioEffect::cutoff},       {"resonance", &AudioEffect::resonance}, {"delay", &AudioEffect::delay},
        {"feedback", &AudioEffect::feedback},   {"roomSize", &AudioEffect::roomSize},   {"damping", &AudioEffect::damping},
        {"threshold", &AudioEffect::threshold}, {"ratio", &AudioEffect::ratio},         {"attack", &AudioEffect::attack},
        {"release", &AudioEffect::release},     {"gain", &AudioEffect::gain},           {"drive", &AudioEffect::drive},
        {"rate", &AudioEffect::rate},           {"depth", &AudioEffect::depth},         {"mix", &AudioEffect::mix},
        {"lowGain", &AudioEffect::lowGain},     {"midGain", &AudioEffect::midGain},     {"highGain", &AudioEffect::highGain},
        {"lowFreq", &AudioEffect::lowFreq},     {"midFreq", &AudioEffect::midFreq},     {"highFreq", &AudioEffect::highFreq},
        {"pitch", &AudioEffect::pitch}};
    for (const auto& [n, member] : fields)
        if (name == n) return &(e.*member);
    return nullptr;
}
int au_addBusEffect(lua_State* L) {
    const AudioBus bus = checkBus(L, 1);
    AudioEffect fx;
    if (!audioEffectFromName(luaL_checkstring(L, 2), fx.type)) return luaL_error(L, "unknown audio effect '%s'", lua_tostring(L, 2));
    if (lua_istable(L, 3)) {
        lua_pushnil(L);
        while (lua_next(L, 3) != 0) {
            if (lua_type(L, -2) == LUA_TSTRING) {
                const std::string key = lua_tostring(L, -2);
                if (key == "enabled") fx.enabled = lua_toboolean(L, -1) != 0;
                else if (float* f = effectField(fx, key)) *f = static_cast<float>(luaL_checknumber(L, -1));
                else return luaL_error(L, "unknown effect parameter '%s'", key.c_str());
            }
            lua_pop(L, 1);
        }
    }
    AudioSystem* a = audioOf(L);
    if (!a) return 0;
    auto list = a->busEffects(bus);
    list.push_back(fx);
    a->setBusEffects(bus, list);
    lua_pushinteger(L, static_cast<lua_Integer>(list.size()));
    return 1;
}
AudioEffect* busEffect(lua_State* L, std::vector<AudioEffect>& list) {
    const lua_Integer i = luaL_checkinteger(L, 2);
    if (i < 1 || i > static_cast<lua_Integer>(list.size())) luaL_error(L, "no effect %d on this bus (it has %d)", static_cast<int>(i), static_cast<int>(list.size()));
    return &list[static_cast<usize>(i - 1)];
}
/// agr.audio.setBusEffect(bus, index, param, value) — param: "enabled" or an AudioEffect field.
int au_setBusEffect(lua_State* L) {
    const AudioBus bus = checkBus(L, 1);
    AudioSystem* a = audioOf(L);
    if (!a) return 0;
    auto list = a->busEffects(bus);
    AudioEffect* fx = busEffect(L, list);
    const std::string key = luaL_checkstring(L, 3);
    if (key == "enabled") fx->enabled = lua_toboolean(L, 4) != 0;
    else if (float* f = effectField(*fx, key)) *f = static_cast<float>(luaL_checknumber(L, 4));
    else return luaL_error(L, "unknown effect parameter '%s'", key.c_str());
    a->setBusEffects(bus, list);
    return 0;
}
int au_clearBusEffects(lua_State* L) {
    const AudioBus bus = checkBus(L, 1);
    if (AudioSystem* a = audioOf(L)) a->setBusEffects(bus, {});
    return 0;
}
int au_busEffectCount(lua_State* L) {
    const AudioBus bus = checkBus(L, 1);
    AudioSystem* a = audioOf(L);
    lua_pushinteger(L, a ? static_cast<lua_Integer>(a->busEffects(bus).size()) : 0);
    return 1;
}

/// agr.audio.spectrum(bus [, bands]) -> { level, ... } (0..1) from an Analyzer effect on the bus.
int au_spectrum(lua_State* L) {
    const AudioBus bus = checkBus(L, 1);
    const u32 bands = static_cast<u32>(std::clamp<lua_Integer>(luaL_optinteger(L, 2, 16), 1, 256));
    AudioSystem* a = audioOf(L);
    const auto levels = a ? a->spectrum(bus, bands) : std::vector<float>{};
    lua_createtable(L, static_cast<int>(levels.size()), 0);
    for (usize i = 0; i < levels.size(); ++i) {
        lua_pushnumber(L, levels[i]);
        lua_rawseti(L, -2, static_cast<lua_Integer>(i + 1));
    }
    return 1;
}

int au_generator(lua_State* L) {
    AudioSystem* a = audioOf(L);
    if (!a) return 0;
    PlayParams p;
    p.volume = static_cast<float>(luaL_optnumber(L, 1, 1.0));
    auto v = a->playGenerator(p, static_cast<u32>(luaL_optinteger(L, 2, 44100)));
    if (!v) return 0;
    lua_pushinteger(L, static_cast<lua_Integer>(v.value()));
    return 1;
}
int au_push(lua_State* L) {
    AudioSystem* a = audioOf(L);
    const VoiceId id = static_cast<VoiceId>(luaL_checkinteger(L, 1));
    luaL_checktype(L, 2, LUA_TTABLE);
    std::vector<float> samples(static_cast<usize>(lua_rawlen(L, 2)));
    for (usize i = 0; i < samples.size(); ++i) {
        lua_rawgeti(L, 2, static_cast<lua_Integer>(i + 1));
        samples[i] = static_cast<float>(lua_tonumber(L, -1));
        lua_pop(L, 1);
    }
    lua_pushinteger(L, a ? a->pushGenerator(id, samples.data(), static_cast<u32>(samples.size())) : 0);
    return 1;
}
int au_queued(lua_State* L) {
    AudioSystem* a = audioOf(L);
    lua_pushinteger(L, a ? a->generatorQueued(static_cast<VoiceId>(luaL_checkinteger(L, 1))) : 0);
    return 1;
}

/// agr.audio.startMicrophone([bus [, volume]]) -> voice: the microphone heard live on a bus.
int au_startMicrophone(lua_State* L) {
    AudioSystem* a = audioOf(L);
    const AudioBus bus = lua_isnoneornil(L, 1) ? AudioBus::Voice : checkBus(L, 1);
    const float volume = static_cast<float>(luaL_optnumber(L, 2, 1.0));
    lua_pushinteger(L, a ? static_cast<lua_Integer>(a->startMicrophone(bus, volume)) : 0);
    return 1;
}
int au_stopMicrophone(lua_State* L) {
    if (AudioSystem* a = audioOf(L)) a->stopMicrophone();
    return 0;
}

int au_startRecording(lua_State* L) {
    AudioSystem* a = audioOf(L);
    lua_pushboolean(L, a && a->startRecording());
    return 1;
}
int au_stopRecording(lua_State* L) {
    const std::string uri = luaL_optstring(L, 1, "user://recording.wav");
    AudioSystem* a = audioOf(L);
    if (!a) return 0;
    auto r = a->stopRecording(uri);
    if (!r) {
        lua_pushnil(L);
        lua_pushstring(L, r.error().message.c_str());
        return 2;
    }
    lua_pushstring(L, uri.c_str());
    lua_pushnumber(L, r.value());
    return 2;
}
int au_recordingLevel(lua_State* L) {
    AudioSystem* a = audioOf(L);
    lua_pushnumber(L, a ? a->recordingLevel() : 0.0f);
    return 1;
}
int au_isRecording(lua_State* L) {
    AudioSystem* a = audioOf(L);
    lua_pushboolean(L, a && a->isRecording());
    return 1;
}

/// agr.audio.createBus(name [, parent = "Master"]) -> true if a free custom slot took it
/// (an existing bus of that name is just re-routed).
int au_createBus(lua_State* L) {
    const char* name = luaL_checkstring(L, 1);
    AudioBus bus{};
    if (!audioBusFromName(name, bus) || !audioBusIsCustom(bus)) {
        bool found = false;
        for (i32 i = kAudioBuiltinBusCount; i < kAudioBusCount && !found; ++i)
            if (!audioBusActive(static_cast<AudioBus>(i))) bus = static_cast<AudioBus>(i), found = true;
        AudioBus clash{};
        if (!found || audioBusFromName(name, clash)) {
            lua_pushboolean(L, 0);
            return 1;
        }
        setAudioBusName(bus, name);
    }
    const AudioBus parent = lua_isnoneornil(L, 2) ? AudioBus::Master : checkBus(L, 2);
    if (AudioSystem* a = audioOf(L)) a->setBusParent(bus, parent);
    lua_pushboolean(L, 1);
    return 1;
}
/// agr.audio.setBusParent(bus, parent) -> false when the route is refused (built-in bus, loop).
int au_setBusParent(lua_State* L) {
    const AudioBus bus = checkBus(L, 1), parent = checkBus(L, 2);
    AudioSystem* a = audioOf(L);
    lua_pushboolean(L, a && a->setBusParent(bus, parent));
    return 1;
}
int au_busParent(lua_State* L) {
    const AudioBus bus = checkBus(L, 1);
    AudioSystem* a = audioOf(L);
    lua_pushstring(L, audioBusName(a ? a->busParent(bus) : AudioBus::Master));
    return 1;
}

int au_setBusVolume(lua_State* L) {
    const AudioBus bus = checkBus(L, 1);
    const float x = static_cast<float>(luaL_checknumber(L, 2));
    if (AudioSystem* a = audioOf(L)) a->setBusVolume(bus, x);
    return 0;
}
int au_busVolume(lua_State* L) {
    const AudioBus bus = checkBus(L, 1);
    AudioSystem* a = audioOf(L);
    lua_pushnumber(L, a ? a->busVolume(bus) : 1.0);
    return 1;
}
int au_setBusMuted(lua_State* L) {
    const AudioBus bus = checkBus(L, 1);
    if (AudioSystem* a = audioOf(L)) a->setBusMuted(bus, lua_toboolean(L, 2) != 0);
    return 0;
}
int au_busMuted(lua_State* L) {
    const AudioBus bus = checkBus(L, 1);
    AudioSystem* a = audioOf(L);
    lua_pushboolean(L, a && a->busMuted(bus));
    return 1;
}

int e_playSound(lua_State* L) {
    const ecs::Entity e = checkEntity(L, 1);
    Scene& scene = sceneOf(L);
    if (!scene.registry().has<AudioSourceComponent>(e)) luaL_error(L, "the entity has no AudioSource");
    AudioSystem* a = audioOf(L);
    lua_pushboolean(L, a && a->playSource(scene, e) != 0);
    return 1;
}
int e_stopSound(lua_State* L) {
    const ecs::Entity e = checkEntity(L, 1);
    const float fade = static_cast<float>(luaL_optnumber(L, 2, 0.0));
    if (AudioSystem* a = audioOf(L)) a->stopSource(e, fade);
    return 0;
}
int e_isSoundPlaying(lua_State* L) {
    const ecs::Entity e = checkEntity(L, 1);
    AudioSystem* a = audioOf(L);
    lua_pushboolean(L, a && a->sourcePlaying(e));
    return 1;
}

} // namespace

void openAudioBindings(lua_State* L) {
    static const luaL_Reg audio[] = {{"play", au_play},           {"stop", au_stop},
                                     {"isPlaying", au_isPlaying}, {"setVolume", au_setVolume},
                                     {"setPitch", au_setPitch},   {"setBusVolume", au_setBusVolume},
                                     {"busVolume", au_busVolume}, {"setBusMuted", au_setBusMuted},
                                     {"busMuted", au_busMuted},   {"addBusEffect", au_addBusEffect},
                                     {"setBusEffect", au_setBusEffect}, {"clearBusEffects", au_clearBusEffects}, {"spectrum", au_spectrum},
                                     {"generator", au_generator}, {"push", au_push}, {"queued", au_queued},
                                     {"startRecording", au_startRecording}, {"startMicrophone", au_startMicrophone}, {"stopMicrophone", au_stopMicrophone}, {"stopRecording", au_stopRecording},
                                     {"recordingLevel", au_recordingLevel}, {"isRecording", au_isRecording},
                                     {"busEffectCount", au_busEffectCount}, {"createBus", au_createBus},
                                     {"setBusParent", au_setBusParent}, {"busParent", au_busParent}, {nullptr, nullptr}};
    addAgrLibrary(L, "audio", audio);
    static const luaL_Reg methods[] = {
        {"playSound", e_playSound}, {"stopSound", e_stopSound}, {"isSoundPlaying", e_isSoundPlaying}, {nullptr, nullptr}};
    addEntityMethods(L, methods);
}

} // namespace agr
