// agr.device.*, agr.save.*, agr.input.accelerometer() / gyroscope().
#include <cstring>
#include <cstdio>
#include <cmath>

#include "ScriptInternal.hpp"
#include "agr/core/Crypto.hpp"
#include "agr/core/Platform.hpp"

namespace agr {

// ---------------------------------------------------------- Lua <-> JSON
// (shared with agr.net: declared in ScriptInternal.hpp)
nlohmann::json toJson(lua_State* L, int idx, int depth) {
    idx = lua_absindex(L, idx);
    if (depth > 32) luaL_error(L, "value nested too deeply (saved / sent tables)");
    switch (lua_type(L, idx)) {
        case LUA_TNIL: return nullptr;
        case LUA_TBOOLEAN: return lua_toboolean(L, idx) != 0;
        case LUA_TNUMBER:
            if (lua_isinteger(L, idx)) return static_cast<i64>(lua_tointeger(L, idx));
            if (!std::isfinite(lua_tonumber(L, idx))) luaL_error(L, "saved / sent numbers must be finite");
            return lua_tonumber(L, idx);
        case LUA_TSTRING: return std::string(lua_tostring(L, idx));
        case LUA_TTABLE: {
            // Array if the keys are exactly 1..n, else an object with string keys.
            const lua_Integer n = static_cast<lua_Integer>(lua_rawlen(L, idx));
            lua_Integer count = 0;
            lua_pushnil(L);
            while (lua_next(L, idx)) {
                ++count;
                lua_pop(L, 1);
            }
            if (count == n && n > 0) {
                nlohmann::json arr = nlohmann::json::array();
                for (lua_Integer i = 1; i <= n; ++i) {
                    lua_rawgeti(L, idx, i);
                    arr.push_back(toJson(L, -1, depth + 1));
                    lua_pop(L, 1);
                }
                return arr;
            }
            nlohmann::json obj = nlohmann::json::object();
            lua_pushnil(L);
            while (lua_next(L, idx)) {
                std::string key;
                if (lua_type(L, -2) == LUA_TSTRING) key = lua_tostring(L, -2);
                else if (lua_isinteger(L, -2)) key = std::to_string(lua_tointeger(L, -2));
                else luaL_error(L, "table keys must be strings or integers (saved / sent tables)");
                obj[key] = toJson(L, -1, depth + 1);
                lua_pop(L, 1);
            }
            return obj;
        }
        default: luaL_error(L, "cannot save or send a %s", luaL_typename(L, idx));
    }
    return nullptr;
}

void pushJson(lua_State* L, const nlohmann::json& j) {
    luaL_checkstack(L, 4, "pushJson");
    if (j.is_null()) lua_pushnil(L);
    else if (j.is_boolean()) lua_pushboolean(L, j.get<bool>());
    else if (j.is_number_integer()) lua_pushinteger(L, static_cast<lua_Integer>(j.get<i64>()));
    else if (j.is_number()) lua_pushnumber(L, j.get<double>());
    else if (j.is_string()) lua_pushstring(L, j.get_ref<const std::string&>().c_str());
    else if (j.is_array()) {
        lua_createtable(L, static_cast<int>(j.size()), 0);
        lua_Integer i = 1;
        for (const auto& v : j) {
            pushJson(L, v);
            lua_rawseti(L, -2, i++);
        }
    } else {
        lua_createtable(L, 0, static_cast<int>(j.size()));
        for (auto it = j.begin(); it != j.end(); ++it) {
            pushJson(L, it.value());
            lua_setfield(L, -2, it.key().c_str());
        }
    }
}

namespace {

SaveStore& saveOf(lua_State* L) {
    auto& m = implOf(L);
    if (!m.save) luaL_error(L, "agr.save is not available here");
    return *m.save;
}

int sv_set(lua_State* L) {
    const std::string key = luaL_checkstring(L, 1);
    luaL_checkany(L, 2);
    nlohmann::json value = toJson(L, 2, 0);
    saveOf(L).set(key, std::move(value));
    return 0;
}
int sv_get(lua_State* L) {
    const std::string key = luaL_checkstring(L, 1);
    SaveStore& s = saveOf(L);
    if (!s.has(key)) {
        lua_settop(L, 2);  // the default, or nil
        return 1;
    }
    pushJson(L, s.get(key));
    return 1;
}
int sv_has(lua_State* L) {
    const std::string key = luaL_checkstring(L, 1);
    lua_pushboolean(L, saveOf(L).has(key));
    return 1;
}
int sv_remove(lua_State* L) {
    const std::string key = luaL_checkstring(L, 1);
    saveOf(L).remove(key);
    return 0;
}
int sv_clear(lua_State* L) {
    saveOf(L).clear();
    return 0;
}
int sv_flush(lua_State* L) {
    Status s = saveOf(L).flush();
    if (s) {
        lua_pushboolean(L, 1);
        return 1;
    }
    lua_pushnil(L);
    lua_pushstring(L, s.error().message.c_str());
    return 2;
}

// ---------------------------------------------------------------- device
int dv_platform(lua_State* L) {
    lua_pushstring(L, kPlatformName);
    return 1;
}
int dv_isMobile(lua_State* L) {
#if defined(AGR_PLATFORM_MOBILE)
    lua_pushboolean(L, 1);
#else
    lua_pushboolean(L, 0);
#endif
    return 1;
}
int dv_vibrate(lua_State* L) {
    const float seconds = static_cast<float>(luaL_checknumber(L, 1));
    const float strength = static_cast<float>(luaL_optnumber(L, 2, 1.0));
    if (PlatformLayer* p = implOf(L).platform) p->vibrate(std::clamp(seconds, 0.0f, 5.0f), strength);
    return 0;
}
int dv_canVibrate(lua_State* L) {
    PlatformLayer* p = implOf(L).platform;
    lua_pushboolean(L, p && p->canVibrate());
    return 1;
}
int dv_safeArea(lua_State* L) {
    PlatformLayer* p = implOf(L).platform;
    const math::Vec4 in = p ? p->safeAreaInsets() : math::Vec4{0, 0, 0, 0};
    for (float v : {in.x, in.y, in.z, in.w}) lua_pushnumber(L, v);
    return 4;
}

// ----------------------------------------------------------------- input
int in_accelerometer(lua_State* L) {
    const Input* in = implOf(L).input;
    if (!in || !in->hasAccelerometer()) {
        lua_pushnil(L);
        return 1;
    }
    const math::Vec3 a = in->accelerometer();
    lua_pushnumber(L, a.x);
    lua_pushnumber(L, a.y);
    lua_pushnumber(L, a.z);
    return 3;
}
int in_gyroscope(lua_State* L) {
    const Input* in = implOf(L).input;
    if (!in || !in->hasGyroscope()) {
        lua_pushnil(L);
        return 1;
    }
    const math::Vec3 g = in->gyroscope();
    lua_pushnumber(L, g.x);
    lua_pushnumber(L, g.y);
    lua_pushnumber(L, g.z);
    return 3;
}

// ---------------------------------------------------------------- data: json, files, base64, sha256
int js_encode(lua_State* L) {
    const nlohmann::json j = toJson(L, 1, 0);
    const std::string text = lua_toboolean(L, 2) ? j.dump(2) : j.dump();
    lua_pushlstring(L, text.data(), text.size());
    return 1;
}
int js_decode(lua_State* L) {
    size_t n = 0;
    const char* text = luaL_checklstring(L, 1, &n);
    const nlohmann::json j = nlohmann::json::parse(std::string_view(text, n), nullptr, false);
    if (j.is_discarded()) {
        lua_pushnil(L);
        lua_pushstring(L, "invalid JSON");
        return 2;
    }
    pushJson(L, j);
    return 1;
}
bool readableUri(const std::string& uri) { return uri.rfind("user://", 0) == 0 || uri.rfind("assets://", 0) == 0; }
int fl_read(lua_State* L) {
    const std::string uri = luaL_checkstring(L, 1);
    if (!readableUri(uri)) return luaL_error(L, "agr.file.read: only user:// and assets:// files");
    auto text = implOf(L).vfs.readText(uri);
    if (!text) {
        lua_pushnil(L);
        lua_pushstring(L, text.error().message.c_str());
        return 2;
    }
    lua_pushlstring(L, text.value().data(), text.value().size());
    return 1;
}
int fl_write(lua_State* L) {
    const std::string uri = luaL_checkstring(L, 1);
    size_t n = 0;
    const char* text = luaL_checklstring(L, 2, &n);
    if (uri.rfind("user://", 0) != 0) return luaL_error(L, "agr.file.write: only user:// files (the game's own folder)");
    const Status st = implOf(L).vfs.writeText(uri, std::string(text, n));
    lua_pushboolean(L, st.ok());
    if (!st) {
        lua_pushstring(L, st.error().message.c_str());
        return 2;
    }
    return 1;
}
int fl_exists(lua_State* L) {
    const std::string uri = luaL_checkstring(L, 1);
    lua_pushboolean(L, readableUri(uri) && implOf(L).vfs.exists(uri));
    return 1;
}
const char* kB64 = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
int b64_encode(lua_State* L) {
    size_t n = 0;
    const auto* d = reinterpret_cast<const unsigned char*>(luaL_checklstring(L, 1, &n));
    std::string out;
    for (size_t i = 0; i < n; i += 3) {
        const u32 v = (u32(d[i]) << 16) | (i + 1 < n ? u32(d[i + 1]) << 8 : 0) | (i + 2 < n ? u32(d[i + 2]) : 0);
        out += kB64[(v >> 18) & 63];
        out += kB64[(v >> 12) & 63];
        out += i + 1 < n ? kB64[(v >> 6) & 63] : '=';
        out += i + 2 < n ? kB64[v & 63] : '=';
    }
    lua_pushlstring(L, out.data(), out.size());
    return 1;
}
int b64_decode(lua_State* L) {
    size_t n = 0;
    const char* s = luaL_checklstring(L, 1, &n);
    std::string out;
    u32 acc = 0;
    int bits = 0;
    for (size_t i = 0; i < n; ++i) {
        const char* p = std::strchr(kB64, s[i]);
        if (s[i] == '=' || !p || !s[i]) continue;
        acc = (acc << 6) | static_cast<u32>(p - kB64);
        bits += 6;
        if (bits >= 8) {
            bits -= 8;
            out += static_cast<char>((acc >> bits) & 0xFF);
        }
    }
    lua_pushlstring(L, out.data(), out.size());
    return 1;
}
std::string sha256(const std::string& msg) { return crypto::sha256Hex(msg); }
int a_sha256(lua_State* L) {
    size_t n = 0;
    const char* s = luaL_checklstring(L, 1, &n);
    const std::string hex = sha256(std::string(s, n));
    lua_pushlstring(L, hex.data(), hex.size());
    return 1;
}

// ---------------------------------------------------------------- player remapping
InputActions& remapActions(lua_State* L) {
    auto& m = implOf(L);
    if (!m.actions) luaL_error(L, "input actions are not available");
    return *m.actions;
}
const InputAction& remapAction(lua_State* L, int idx) {
    const char* name = luaL_checkstring(L, idx);
    const InputAction* a = remapActions(L).map().find(name);
    if (!a) luaL_error(L, "unknown action '%s'", name);
    return *a;
}
void pushBinding(lua_State* L, const InputBinding& b) {
    pushJson(L, nlohmann::json::parse(bindingToJson(b).dump()));
    lua_pushstring(L, describe(b).c_str());
    lua_setfield(L, -2, "text");
}
/// agr.input.bindings("jump") -> { {type = "key", input = "Space", text = "Key Space"}, ... }
int rm_bindings(lua_State* L) {
    const InputAction& a = remapAction(L, 1);
    lua_createtable(L, static_cast<int>(a.bindings.size()), 0);
    for (usize i = 0; i < a.bindings.size(); ++i) {
        pushBinding(L, a.bindings[i]);
        lua_rawseti(L, -2, static_cast<lua_Integer>(i + 1));
    }
    return 1;
}
/// agr.input.setBindings("jump", { {type = "key", input = "W"}, captured, ... })
int rm_setBindings(lua_State* L) {
    const std::string name = remapAction(L, 1).name;
    luaL_checktype(L, 2, LUA_TTABLE);
    std::vector<InputBinding> bindings;
    const lua_Integer n = luaL_len(L, 2);
    for (lua_Integer i = 1; i <= n; ++i) {
        lua_rawgeti(L, 2, i);
        nlohmann::json j = toJson(L, -1, 0);
        lua_pop(L, 1);
        if (j.is_object()) j.erase("text");
        InputBinding b;
        if (!bindingFromJson(nlohmann::ordered_json::parse(j.dump()), b)) luaL_error(L, "binding %d of '%s' is not valid", static_cast<int>(i), name.c_str());
        bindings.push_back(b);
    }
    remapActions(L).setBindings(name, std::move(bindings));
    return 0;
}
/// agr.input.capture() -> binding table of what was pressed this frame (key, mouse, pad), or nil.
int rm_capture(lua_State* L) {
    const Input* input = implOf(L).input;
    const auto b = input ? capturePressed(*input) : std::nullopt;
    if (!b) {
        lua_pushnil(L);
        return 1;
    }
    pushBinding(L, *b);
    return 1;
}
/// agr.input.saveBindings(): keeps the player's remapping in the save (applied at every start).
int rm_save(lua_State* L) {
    auto& m = implOf(L);
    if (!m.save) luaL_error(L, "no save store");
    m.save->set("agr.input", nlohmann::json::parse(remapActions(L).overrides().dump()));
    (void)m.save->flush();
    return 0;
}
/// agr.input.resetBindings(): back to the game's bindings (call saveBindings to keep it).
int rm_reset(lua_State* L) {
    remapActions(L).resetBindings();
    return 0;
}
/// agr.input.vibrate([pad = 1], strength, seconds[, light]): controller rumble -> true if it has one.
int rm_vibrate(lua_State* L) {
    const lua_Integer pad = luaL_optinteger(L, 1, 1);
    const float low = static_cast<float>(luaL_optnumber(L, 2, 1.0));
    const float seconds = static_cast<float>(luaL_optnumber(L, 3, 0.2));
    const float high = static_cast<float>(luaL_optnumber(L, 4, low));
    PlatformLayer* p = implOf(L).platform;
    lua_pushboolean(L, p && pad >= 1 && p->rumbleGamepad(static_cast<u32>(pad - 1), low, high, seconds));
    return 1;
}

// ---------------------------------------------------------------- agr.rewind
int rw_start(lua_State* L) {
    implOf(L).rewind.start();
    return 0;
}
int rw_stop(lua_State* L) {
    auto& m = implOf(L);
    if (m.scene) m.rewind.stop(*m.scene);
    return 0;
}
int rw_isRewinding(lua_State* L) {
    lua_pushboolean(L, implOf(L).rewind.rewinding());
    return 1;
}
int rw_setSpeed(lua_State* L) {
    implOf(L).rewind.setSpeed(static_cast<float>(luaL_checknumber(L, 1)));
    return 0;
}
int rw_available(lua_State* L) {
    lua_pushnumber(L, implOf(L).rewind.available(checkEntity(L, 1)));
    return 1;
}
int rw_capture(lua_State* L) {
    const std::string text = implOf(L).rewind.capture(checkEntity(L, 1));
    lua_pushlstring(L, text.data(), text.size());
    return 1;
}
int rw_playGhost(lua_State* L) {
    const ecs::Entity e = checkEntity(L, 1);
    lua_pushboolean(L, implOf(L).rewind.playGhost(e, luaL_checkstring(L, 2), lua_toboolean(L, 3)));
    return 1;
}
int rw_ghostPlaying(lua_State* L) {
    lua_pushboolean(L, implOf(L).rewind.ghostPlaying(checkEntity(L, 1)));
    return 1;
}

} // namespace

void openMobileBindings(lua_State* L) {
    static const luaL_Reg json[] = {{"encode", js_encode}, {"decode", js_decode}, {nullptr, nullptr}};
    addAgrLibrary(L, "json", json);
    static const luaL_Reg file[] = {{"read", fl_read}, {"write", fl_write}, {"exists", fl_exists}, {nullptr, nullptr}};
    addAgrLibrary(L, "file", file);
    static const luaL_Reg b64[] = {{"encode", b64_encode}, {"decode", b64_decode}, {nullptr, nullptr}};
    addAgrLibrary(L, "base64", b64);
    lua_getglobal(L, "agr");
    lua_pushcfunction(L, a_sha256);
    lua_setfield(L, -2, "sha256");
    lua_pop(L, 1);
    static const luaL_Reg save[] = {{"set", sv_set},     {"get", sv_get},     {"has", sv_has}, {"remove", sv_remove},
                                    {"clear", sv_clear}, {"flush", sv_flush}, {nullptr, nullptr}};
    addAgrLibrary(L, "save", save);
    static const luaL_Reg device[] = {{"platform", dv_platform},       {"isMobile", dv_isMobile}, {"vibrate", dv_vibrate},
                                      {"canVibrate", dv_canVibrate},   {"safeArea", dv_safeArea}, {nullptr, nullptr}};
    addAgrLibrary(L, "device", device);
    static const luaL_Reg rewind[] = {{"start", rw_start},         {"stop", rw_stop},           {"isRewinding", rw_isRewinding},
                                      {"setSpeed", rw_setSpeed},   {"available", rw_available}, {"capture", rw_capture},
                                      {"playGhost", rw_playGhost}, {"ghostPlaying", rw_ghostPlaying}, {nullptr, nullptr}};
    addAgrLibrary(L, "rewind", rewind);
    static const luaL_Reg input[] = {{"accelerometer", in_accelerometer}, {"gyroscope", in_gyroscope}, {"bindings", rm_bindings},
                                    {"setBindings", rm_setBindings},    {"capture", rm_capture},     {"saveBindings", rm_save},
                                    {"resetBindings", rm_reset},        {"vibrate", rm_vibrate},     {nullptr, nullptr}};
    extendAgrLibrary(L, "input", input);
}

} // namespace agr
