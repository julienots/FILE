// agr.device.*, agr.save.*, agr.input.accelerometer() / gyroscope().
#include <cmath>

#include "ScriptInternal.hpp"
#include "agr/core/Platform.hpp"

namespace agr {

namespace {

// ---------------------------------------------------------- Lua <-> JSON
nlohmann::json toJson(lua_State* L, int idx, int depth) {
    idx = lua_absindex(L, idx);
    if (depth > 32) luaL_error(L, "agr.save: value nested too deeply");
    switch (lua_type(L, idx)) {
        case LUA_TNIL: return nullptr;
        case LUA_TBOOLEAN: return lua_toboolean(L, idx) != 0;
        case LUA_TNUMBER:
            if (lua_isinteger(L, idx)) return static_cast<i64>(lua_tointeger(L, idx));
            if (!std::isfinite(lua_tonumber(L, idx))) luaL_error(L, "agr.save: numbers must be finite");
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
                else luaL_error(L, "agr.save: table keys must be strings or integers");
                obj[key] = toJson(L, -1, depth + 1);
                lua_pop(L, 1);
            }
            return obj;
        }
        default: luaL_error(L, "agr.save: cannot save a %s", luaL_typename(L, idx));
    }
    return nullptr;
}

void pushJson(lua_State* L, const nlohmann::json& j) {
    luaL_checkstack(L, 4, "agr.save");
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

} // namespace

void openMobileBindings(lua_State* L) {
    static const luaL_Reg save[] = {{"set", sv_set},     {"get", sv_get},     {"has", sv_has}, {"remove", sv_remove},
                                    {"clear", sv_clear}, {"flush", sv_flush}, {nullptr, nullptr}};
    addAgrLibrary(L, "save", save);
    static const luaL_Reg device[] = {{"platform", dv_platform},       {"isMobile", dv_isMobile}, {"vibrate", dv_vibrate},
                                      {"canVibrate", dv_canVibrate},   {"safeArea", dv_safeArea}, {nullptr, nullptr}};
    addAgrLibrary(L, "device", device);
    static const luaL_Reg input[] = {{"accelerometer", in_accelerometer}, {"gyroscope", in_gyroscope}, {nullptr, nullptr}};
    extendAgrLibrary(L, "input", input);
}

} // namespace agr
