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
        luaL_error(L, "unknown audio bus '%s' (Master, Music, Effects, Ambient, Interface, Voice)", name);
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
                                     {"busMuted", au_busMuted},   {nullptr, nullptr}};
    addAgrLibrary(L, "audio", audio);
    static const luaL_Reg methods[] = {
        {"playSound", e_playSound}, {"stopSound", e_stopSound}, {"isSoundPlaying", e_isSoundPlaying}, {nullptr, nullptr}};
    addEntityMethods(L, methods);
}

} // namespace agr
