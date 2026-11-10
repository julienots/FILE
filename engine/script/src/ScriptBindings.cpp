// The Lua API: `agr.*` functions and the Entity type. Every function checks its
// arguments (luaL_check*) and raises Lua errors (caught by the engine, reported
// with file:line) instead of crashing the game.
#include <cstring>
#include <string_view>

#include "ScriptInternal.hpp"
#include "agr/core/Log.hpp"
#include "agr/scene/Reflection.hpp"
#include "agr/script/ScriptComponent.hpp"

namespace agr {

namespace {

constexpr const char* kEntityMeta = "agr.Entity";

Scene& sceneOf(lua_State* L) {
    auto& m = implOf(L);
    if (!m.scene) luaL_error(L, "no scene is running");
    return *m.scene;
}

// --------------------------------------------------------------- values
void pushVec(lua_State* L, const float* v, int n) {
    static const char* keys[] = {"x", "y", "z", "w"};
    lua_createtable(L, 0, n);
    for (int i = 0; i < n; ++i) {
        lua_pushnumber(L, v[i]);
        lua_setfield(L, -2, keys[i]);
    }
}

/// Reads {x=, y=, ...}, {r=, g=, ...} or {1, 2, ...}; missing components keep their value.
void readVec(lua_State* L, int idx, float* out, int n) {
    idx = lua_absindex(L, idx);
    luaL_checktype(L, idx, LUA_TTABLE);
    static const char* xyzw[] = {"x", "y", "z", "w"};
    static const char* rgba[] = {"r", "g", "b", "a"};
    for (int i = 0; i < n; ++i) {
        int t = lua_getfield(L, idx, xyzw[i]);
        if (t != LUA_TNUMBER) {
            lua_pop(L, 1);
            t = lua_getfield(L, idx, rgba[i]);
        }
        if (t != LUA_TNUMBER) {
            lua_pop(L, 1);
            t = lua_rawgeti(L, idx, i + 1);
        }
        if (t == LUA_TNUMBER) out[i] = static_cast<float>(lua_tonumber(L, -1));
        lua_pop(L, 1);
    }
}

Key keyFromName(std::string_view n) {
    if (n.size() == 1) {
        const char c = static_cast<char>(n[0] >= 'a' && n[0] <= 'z' ? n[0] - 32 : n[0]);
        if (c >= 'A' && c <= 'Z') return static_cast<Key>(static_cast<int>(Key::A) + (c - 'A'));
        if (c >= '1' && c <= '9') return static_cast<Key>(static_cast<int>(Key::Num1) + (c - '1'));
        if (c == '0') return Key::Num0;
    }
    struct Named {
        std::string_view name;
        Key key;
    };
    static const Named table[] = {{"Space", Key::Space},     {"Return", Key::Return},     {"Enter", Key::Return},
                                  {"Escape", Key::Escape},   {"Backspace", Key::Backspace}, {"Tab", Key::Tab},
                                  {"Left", Key::Left},       {"Right", Key::Right},       {"Up", Key::Up},
                                  {"Down", Key::Down},       {"LeftShift", Key::LeftShift}, {"RightShift", Key::RightShift},
                                  {"LeftCtrl", Key::LeftCtrl}, {"RightCtrl", Key::RightCtrl}, {"LeftAlt", Key::LeftAlt},
                                  {"Back", Key::AndroidBack}};
    for (const Named& k : table)
        if (k.name == n) return k.key;
    if (n.size() >= 2 && (n[0] == 'F') && n.size() <= 3) {
        const int f = std::atoi(std::string(n.substr(1)).c_str());
        if (f >= 1 && f <= 12) return static_cast<Key>(static_cast<int>(Key::F1) + f - 1);
    }
    return Key::Unknown;
}

Key checkKey(lua_State* L, int idx) {
    const char* name = luaL_checkstring(L, idx);
    const Key k = keyFromName(name);
    if (k == Key::Unknown) luaL_error(L, "unknown key '%s' (A-Z, 0-9, Space, Return, Escape, Left, Right, Up, Down, LeftShift...)", name);
    return k;
}

const Input& inputOf(lua_State* L) {
    auto& m = implOf(L);
    if (!m.input) luaL_error(L, "input is not available");
    return *m.input;
}

// --------------------------------------------------------------- reflection
struct ComponentRef {
    const ComponentInfo* info;
    void* data;
};

ComponentRef component(lua_State* L, ecs::Entity e, const char* name, bool mustExist) {
    const ComponentInfo* info = ComponentRegistry::instance().find(name);
    if (!info) luaL_error(L, "unknown component '%s'", name);
    ecs::PoolBase* pool = sceneOf(L).registry().findPool(info->id);
    if (!pool || !pool->contains(e)) {
        if (mustExist) luaL_error(L, "the entity has no '%s' component", name);
        return {info, nullptr};
    }
    return {info, pool->getRaw(e)};
}

void pushField(lua_State* L, const FieldInfo& f, void* comp) {
    switch (f.type) {
        case FieldType::Bool: lua_pushboolean(L, f.ref<bool>(comp)); break;
        case FieldType::Int32: lua_pushinteger(L, f.ref<i32>(comp)); break;
        case FieldType::UInt32: lua_pushinteger(L, f.ref<u32>(comp)); break;
        case FieldType::Float: lua_pushnumber(L, f.ref<float>(comp)); break;
        case FieldType::Vec2: pushVec(L, &f.ref<math::Vec2>(comp).x, 2); break;
        case FieldType::Vec3: pushVec(L, &f.ref<math::Vec3>(comp).x, 3); break;
        case FieldType::Vec4: pushVec(L, &f.ref<math::Vec4>(comp).x, 4); break;
        case FieldType::Quat: {  // Euler angles in degrees, like the inspector
            const math::Vec3 e = f.ref<math::Quat>(comp).toEuler();
            const float deg[3] = {math::degrees(e.x), math::degrees(e.y), math::degrees(e.z)};
            pushVec(L, deg, 3);
            break;
        }
        case FieldType::String: lua_pushstring(L, f.ref<std::string>(comp).c_str()); break;
        case FieldType::Entity: {
            const ecs::Entity t = f.ref<ecs::Entity>(comp);
            if (t.isNull() || !sceneOf(L).valid(t)) lua_pushnil(L);
            else pushEntity(L, t);
            break;
        }
        case FieldType::Enum: {
            const i32 v = f.ref<i32>(comp);
            for (const auto& en : f.enumEntries)
                if (en.value == v) {
                    lua_pushstring(L, en.name.c_str());
                    return;
                }
            lua_pushinteger(L, v);
            break;
        }
        case FieldType::IntArray: {
            const auto& arr = f.ref<std::vector<i32>>(comp);
            lua_createtable(L, static_cast<int>(arr.size()), 0);
            for (usize i = 0; i < arr.size(); ++i) {
                lua_pushinteger(L, arr[i]);
                lua_rawseti(L, -2, static_cast<lua_Integer>(i + 1));
            }
            break;
        }
    }
}

void readField(lua_State* L, int idx, const FieldInfo& f, void* comp) {
    switch (f.type) {
        case FieldType::Bool: f.ref<bool>(comp) = lua_toboolean(L, idx); break;
        case FieldType::Int32: f.ref<i32>(comp) = static_cast<i32>(luaL_checkinteger(L, idx)); break;
        case FieldType::UInt32: f.ref<u32>(comp) = static_cast<u32>(std::max<lua_Integer>(0, luaL_checkinteger(L, idx))); break;
        case FieldType::Float: f.ref<float>(comp) = static_cast<float>(luaL_checknumber(L, idx)); break;
        case FieldType::Vec2: readVec(L, idx, &f.ref<math::Vec2>(comp).x, 2); break;
        case FieldType::Vec3: readVec(L, idx, &f.ref<math::Vec3>(comp).x, 3); break;
        case FieldType::Vec4: readVec(L, idx, &f.ref<math::Vec4>(comp).x, 4); break;
        case FieldType::Quat: {
            float deg[3] = {0, 0, 0};
            readVec(L, idx, deg, 3);
            f.ref<math::Quat>(comp) = math::Quat::fromEuler({math::radians(deg[0]), math::radians(deg[1]), math::radians(deg[2])});
            break;
        }
        case FieldType::String: f.ref<std::string>(comp) = luaL_checkstring(L, idx); break;
        case FieldType::Entity: f.ref<ecs::Entity>(comp) = lua_isnil(L, idx) ? ecs::Entity{} : checkEntity(L, idx); break;
        case FieldType::Enum: {
            if (lua_type(L, idx) == LUA_TNUMBER) {
                f.ref<i32>(comp) = static_cast<i32>(lua_tointeger(L, idx));
                break;
            }
            const char* name = luaL_checkstring(L, idx);
            for (const auto& en : f.enumEntries)
                if (en.name == name) {
                    f.ref<i32>(comp) = en.value;
                    return;
                }
            luaL_error(L, "'%s' is not a value of %s", name, f.name.c_str());
            break;
        }
        case FieldType::IntArray: {
            luaL_checktype(L, idx, LUA_TTABLE);
            auto& arr = f.ref<std::vector<i32>>(comp);
            arr.clear();
            const lua_Integer n = luaL_len(L, idx);
            for (lua_Integer i = 1; i <= n; ++i) {
                lua_rawgeti(L, idx, i);
                arr.push_back(static_cast<i32>(lua_tointeger(L, -1)));
                lua_pop(L, 1);
            }
            break;
        }
    }
}

// --------------------------------------------------------------- Entity methods
TransformComponent& transformOf(lua_State* L, ecs::Entity e) { return sceneOf(L).registry().get<TransformComponent>(e); }

float optNum(lua_State* L, int idx, float def) { return static_cast<float>(luaL_optnumber(L, idx, def)); }

int e_isValid(lua_State* L) {
    auto* ud = static_cast<LuaEntity*>(luaL_checkudata(L, 1, kEntityMeta));
    auto& m = implOf(L);
    const ecs::Entity e{static_cast<u32>(ud->packed), static_cast<u32>(ud->packed >> 32)};
    lua_pushboolean(L, m.scene && ud->generation == m.sceneGeneration && m.scene->valid(e));
    return 1;
}
int e_name(lua_State* L) {
    lua_pushstring(L, sceneOf(L).nameOf(checkEntity(L, 1)).c_str());
    return 1;
}
int e_setName(lua_State* L) {
    const ecs::Entity e = checkEntity(L, 1);
    sceneOf(L).registry().get<NameComponent>(e).name = luaL_checkstring(L, 2);
    return 0;
}
int e_position(lua_State* L) {
    const auto& p = transformOf(L, checkEntity(L, 1)).position;
    lua_pushnumber(L, p.x);
    lua_pushnumber(L, p.y);
    lua_pushnumber(L, p.z);
    return 3;
}
int e_setPosition(lua_State* L) {
    auto& t = transformOf(L, checkEntity(L, 1));
    t.position = {static_cast<float>(luaL_checknumber(L, 2)), static_cast<float>(luaL_checknumber(L, 3)), optNum(L, 4, t.position.z)};
    return 0;
}
int e_translate(lua_State* L) {
    auto& t = transformOf(L, checkEntity(L, 1));
    t.position = t.position + math::Vec3{static_cast<float>(luaL_checknumber(L, 2)), static_cast<float>(luaL_checknumber(L, 3)), optNum(L, 4, 0.0f)};
    return 0;
}
int e_worldPosition(lua_State* L) {
    const math::Vec3 p = sceneOf(L).worldMatrix(checkEntity(L, 1)).translation();
    lua_pushnumber(L, p.x);
    lua_pushnumber(L, p.y);
    lua_pushnumber(L, p.z);
    return 3;
}
int e_rotation(lua_State* L) {
    const math::Vec3 r = transformOf(L, checkEntity(L, 1)).rotation.toEuler();
    lua_pushnumber(L, math::degrees(r.x));
    lua_pushnumber(L, math::degrees(r.y));
    lua_pushnumber(L, math::degrees(r.z));
    return 3;
}
int e_setRotation(lua_State* L) {
    auto& t = transformOf(L, checkEntity(L, 1));
    t.rotation = math::Quat::fromEuler({math::radians(optNum(L, 2, 0)), math::radians(optNum(L, 3, 0)), math::radians(optNum(L, 4, 0))});
    return 0;
}
int e_rotate(lua_State* L) {  // degrees about the local axes
    auto& t = transformOf(L, checkEntity(L, 1));
    t.rotation = math::normalize(t.rotation * math::Quat::fromEuler({math::radians(optNum(L, 2, 0)), math::radians(optNum(L, 3, 0)),
                                                                     math::radians(optNum(L, 4, 0))}));
    return 0;
}
int e_scale(lua_State* L) {
    const auto& s = transformOf(L, checkEntity(L, 1)).scale;
    lua_pushnumber(L, s.x);
    lua_pushnumber(L, s.y);
    lua_pushnumber(L, s.z);
    return 3;
}
int e_setScale(lua_State* L) {
    auto& t = transformOf(L, checkEntity(L, 1));
    const float x = static_cast<float>(luaL_checknumber(L, 2));
    t.scale = {x, optNum(L, 3, x), optNum(L, 4, lua_isnoneornil(L, 3) ? x : t.scale.z)};
    return 0;
}
int e_get(lua_State* L) {
    const ecs::Entity e = checkEntity(L, 1);
    const char* comp = luaL_checkstring(L, 2);
    const char* field = luaL_checkstring(L, 3);
    const ComponentRef c = component(L, e, comp, true);
    const FieldInfo* f = c.info->field(field);
    if (!f) luaL_error(L, "component '%s' has no field '%s'", comp, field);
    pushField(L, *f, c.data);
    return 1;
}
int e_set(lua_State* L) {
    const ecs::Entity e = checkEntity(L, 1);
    const char* comp = luaL_checkstring(L, 2);
    const char* field = luaL_checkstring(L, 3);
    luaL_checkany(L, 4);
    const ComponentRef c = component(L, e, comp, true);
    const FieldInfo* f = c.info->field(field);
    if (!f) luaL_error(L, "component '%s' has no field '%s'", comp, field);
    readField(L, 4, *f, c.data);
    return 0;
}
int e_has(lua_State* L) {
    const ecs::Entity e = checkEntity(L, 1);
    lua_pushboolean(L, component(L, e, luaL_checkstring(L, 2), false).data != nullptr);
    return 1;
}
int e_add(lua_State* L) {
    const ecs::Entity e = checkEntity(L, 1);
    const char* name = luaL_checkstring(L, 2);
    const ComponentInfo* info = ComponentRegistry::instance().find(name);
    if (!info) luaL_error(L, "unknown component '%s'", name);
    auto& reg = sceneOf(L).registry();
    auto& pool = reg.ensurePool(info->id, info->makePool);
    if (!pool.contains(e)) pool.emplaceDefault(reg, e);
    return 0;
}
int e_remove(lua_State* L) {
    const ecs::Entity e = checkEntity(L, 1);
    const ComponentRef c = component(L, e, luaL_checkstring(L, 2), false);
    if (c.data) sceneOf(L).registry().findPool(c.info->id)->remove(sceneOf(L).registry(), e);
    return 0;
}
int e_parent(lua_State* L) {
    const ecs::Entity p = sceneOf(L).parent(checkEntity(L, 1));
    if (p.isNull()) lua_pushnil(L);
    else pushEntity(L, p);
    return 1;
}
int e_children(lua_State* L) {
    const auto kids = sceneOf(L).children(checkEntity(L, 1));
    lua_createtable(L, static_cast<int>(kids.size()), 0);
    for (usize i = 0; i < kids.size(); ++i) {
        pushEntity(L, kids[i]);
        lua_rawseti(L, -2, static_cast<lua_Integer>(i + 1));
    }
    return 1;
}
int e_find(lua_State* L) {  // descendant by name (depth first)
    Scene& s = sceneOf(L);
    const ecs::Entity root = checkEntity(L, 1);
    const std::string_view name = luaL_checkstring(L, 2);
    std::vector<ecs::Entity> stack = s.children(root);
    while (!stack.empty()) {
        const ecs::Entity e = stack.front();
        stack.erase(stack.begin());
        if (s.nameOf(e) == name) {
            pushEntity(L, e);
            return 1;
        }
        const auto kids = s.children(e);
        stack.insert(stack.end(), kids.begin(), kids.end());
    }
    lua_pushnil(L);
    return 1;
}
int e_destroy(lua_State* L) {
    sceneOf(L).destroyDeferred(checkEntity(L, 1));  // at the end of the frame: safe during callbacks
    return 0;
}
int e_script(lua_State* L) {
    const ecs::Entity e = checkEntity(L, 1);
    auto& m = implOf(L);
    auto it = m.instances.find(e.packed());
    if (it == m.instances.end() || it->second.ref == LUA_NOREF) lua_pushnil(L);
    else lua_rawgeti(L, LUA_REGISTRYINDEX, it->second.ref);
    return 1;
}
Physics2DWorld& physics2d(lua_State* L) {
    auto& m = implOf(L);
    if (!m.physics2d) luaL_error(L, "2D physics is not enabled (Physics2DPlugin)");
    return *m.physics2d;
}
int e_velocity(lua_State* L) {
    const auto v = physics2d(L).velocity(checkEntity(L, 1));
    lua_pushnumber(L, v ? v->x : 0.0);
    lua_pushnumber(L, v ? v->y : 0.0);
    return 2;
}
int e_setVelocity(lua_State* L) {
    const ecs::Entity e = checkEntity(L, 1);
    lua_pushboolean(L, physics2d(L).setVelocity(e, {static_cast<float>(luaL_checknumber(L, 2)), static_cast<float>(luaL_checknumber(L, 3))}));
    return 1;
}
int e_applyForce(lua_State* L) {
    const ecs::Entity e = checkEntity(L, 1);
    lua_pushboolean(L, physics2d(L).applyForce(e, {static_cast<float>(luaL_checknumber(L, 2)), static_cast<float>(luaL_checknumber(L, 3))}));
    return 1;
}
int e_applyImpulse(lua_State* L) {
    const ecs::Entity e = checkEntity(L, 1);
    lua_pushboolean(L, physics2d(L).applyImpulse(e, {static_cast<float>(luaL_checknumber(L, 2)), static_cast<float>(luaL_checknumber(L, 3))}));
    return 1;
}
int e_setAngularVelocity(lua_State* L) {
    const ecs::Entity e = checkEntity(L, 1);
    lua_pushboolean(L, physics2d(L).setAngularVelocity(e, static_cast<float>(luaL_checknumber(L, 2))));
    return 1;
}
int e_eq(lua_State* L) {
    const auto* a = static_cast<LuaEntity*>(luaL_testudata(L, 1, kEntityMeta));
    const auto* b = static_cast<LuaEntity*>(luaL_testudata(L, 2, kEntityMeta));
    lua_pushboolean(L, a && b && a->packed == b->packed && a->generation == b->generation);
    return 1;
}
int e_tostring(lua_State* L) {
    auto* ud = static_cast<LuaEntity*>(luaL_checkudata(L, 1, kEntityMeta));
    auto& m = implOf(L);
    const ecs::Entity e{static_cast<u32>(ud->packed), static_cast<u32>(ud->packed >> 32)};
    if (m.scene && ud->generation == m.sceneGeneration && m.scene->valid(e))
        lua_pushfstring(L, "Entity(%s)", m.scene->nameOf(e).c_str());
    else
        lua_pushliteral(L, "Entity(destroyed)");
    return 1;
}

// --------------------------------------------------------------- agr.*
std::string joinArgs(lua_State* L) {
    std::string out;
    for (int i = 1; i <= lua_gettop(L); ++i) {
        if (i > 1) out += ' ';
        out += luaL_tolstring(L, i, nullptr);
        lua_pop(L, 1);
    }
    return out;
}
int a_log(lua_State* L) {
    AGR_LOG_INFO("Script", "{}", joinArgs(L));
    return 0;
}
int a_warn(lua_State* L) {
    AGR_LOG_WARN("Script", "{}", joinArgs(L));
    return 0;
}
int a_error(lua_State* L) {
    AGR_LOG_ERROR("Script", "{}", joinArgs(L));
    return 0;
}
int a_find(lua_State* L) {
    const ecs::Entity e = sceneOf(L).findByName(luaL_checkstring(L, 1));
    if (e.isNull()) lua_pushnil(L);
    else pushEntity(L, e);
    return 1;
}
int a_spawn(lua_State* L) {
    Scene& s = sceneOf(L);
    const ecs::Entity parent = lua_isnoneornil(L, 2) ? ecs::Entity{} : checkEntity(L, 2);
    pushEntity(L, s.createEntity(luaL_optstring(L, 1, "Entity"), parent));
    return 1;
}
int i_keyDown(lua_State* L) {
    lua_pushboolean(L, inputOf(L).keyDown(checkKey(L, 1)));
    return 1;
}
int i_keyPressed(lua_State* L) {
    lua_pushboolean(L, inputOf(L).keyPressed(checkKey(L, 1)));
    return 1;
}
int i_keyReleased(lua_State* L) {
    lua_pushboolean(L, inputOf(L).keyReleased(checkKey(L, 1)));
    return 1;
}
MouseButton checkButton(lua_State* L, int idx) {
    const lua_Integer b = luaL_optinteger(L, idx, 0);
    if (b < 0 || b >= static_cast<lua_Integer>(MouseButton::Count)) luaL_error(L, "mouse button must be 0 (left), 1 (middle) or 2 (right)");
    return static_cast<MouseButton>(b);
}
int i_mouseDown(lua_State* L) {
    lua_pushboolean(L, inputOf(L).mouseDown(checkButton(L, 1)));
    return 1;
}
int i_mousePressed(lua_State* L) {
    lua_pushboolean(L, inputOf(L).mousePressed(checkButton(L, 1)));
    return 1;
}
int i_mousePosition(lua_State* L) {
    const math::Vec2 p = inputOf(L).mousePosition();
    lua_pushnumber(L, p.x);
    lua_pushnumber(L, p.y);
    return 2;
}
int i_touchCount(lua_State* L) {
    lua_pushinteger(L, static_cast<lua_Integer>(inputOf(L).touches().size()));
    return 1;
}
int i_touch(lua_State* L) {
    const auto touches = inputOf(L).touches();
    const lua_Integer i = luaL_checkinteger(L, 1);
    if (i < 1 || i > static_cast<lua_Integer>(touches.size())) return 0;
    const Touch& t = touches[static_cast<usize>(i - 1)];
    static const char* phases[] = {"began", "moved", "stationary", "ended"};
    lua_pushnumber(L, t.position.x);
    lua_pushnumber(L, t.position.y);
    lua_pushstring(L, phases[static_cast<int>(t.phase)]);
    return 3;
}
int i_axis(lua_State* L) {  // "horizontal" / "vertical": arrows and WASD / ZQSD, -1..1
    const Input& in = inputOf(L);
    const std::string_view axis = luaL_checkstring(L, 1);
    float v = 0.0f;
    if (axis == "horizontal") {
        v += (in.keyDown(Key::Right) || in.keyDown(Key::D)) ? 1.0f : 0.0f;
        v -= (in.keyDown(Key::Left) || in.keyDown(Key::A) || in.keyDown(Key::Q)) ? 1.0f : 0.0f;
    } else if (axis == "vertical") {
        v += (in.keyDown(Key::Up) || in.keyDown(Key::W) || in.keyDown(Key::Z)) ? 1.0f : 0.0f;
        v -= (in.keyDown(Key::Down) || in.keyDown(Key::S)) ? 1.0f : 0.0f;
    } else {
        luaL_error(L, "unknown axis '%s' (horizontal, vertical)", axis.data());
    }
    lua_pushnumber(L, v);
    return 1;
}
int t_delta(lua_State* L) {
    lua_pushnumber(L, implOf(L).time.deltaSeconds);
    return 1;
}
int t_fixedDelta(lua_State* L) {
    lua_pushnumber(L, implOf(L).time.fixedDeltaSeconds);
    return 1;
}
int t_total(lua_State* L) {
    lua_pushnumber(L, implOf(L).time.totalSeconds);
    return 1;
}
int t_frame(lua_State* L) {
    lua_pushinteger(L, static_cast<lua_Integer>(implOf(L).time.frameIndex));
    return 1;
}
int p_raycast(lua_State* L) {
    const math::Vec2 o{static_cast<float>(luaL_checknumber(L, 1)), static_cast<float>(luaL_checknumber(L, 2))};
    const math::Vec2 d{static_cast<float>(luaL_checknumber(L, 3)), static_cast<float>(luaL_checknumber(L, 4))};
    const auto hit = physics2d(L).raycast(o, d, static_cast<float>(luaL_optnumber(L, 5, 1000.0)));
    if (!hit) {
        lua_pushnil(L);
        return 1;
    }
    lua_createtable(L, 0, 6);
    if (sceneOf(L).valid(hit->entity)) {
        pushEntity(L, hit->entity);
        lua_setfield(L, -2, "entity");
    }
    for (auto [name, value] : {std::pair{"x", hit->point.x}, {"y", hit->point.y}, {"nx", hit->normal.x}, {"ny", hit->normal.y},
                               {"distance", hit->distance}}) {
        lua_pushnumber(L, value);
        lua_setfield(L, -2, name);
    }
    return 1;
}
int p_setGravity(lua_State* L) {
    physics2d(L).setGravity({static_cast<float>(luaL_checknumber(L, 1)), static_cast<float>(luaL_checknumber(L, 2))});
    return 0;
}
int p_gravity(lua_State* L) {
    const math::Vec2 g = physics2d(L).gravity();
    lua_pushnumber(L, g.x);
    lua_pushnumber(L, g.y);
    return 2;
}

/// require("assets://scripts/util.lua"): runs a module once through the VFS and caches its result.
int a_require(lua_State* L) {
    auto& m = implOf(L);
    const std::string uri = luaL_checkstring(L, 1);
    if (auto it = m.modules.find(uri); it != m.modules.end()) {
        lua_rawgeti(L, LUA_REGISTRYINDEX, it->second);
        return 1;
    }
    auto text = m.vfs.readText(uri);
    if (!text) return luaL_error(L, "require '%s': %s", uri.c_str(), text.error().message.c_str());
    const std::string chunk = "@" + uri;
    if (luaL_loadbufferx(L, text.value().data(), text.value().size(), chunk.c_str(), "t") != LUA_OK) return lua_error(L);
    lua_call(L, 0, 1);
    if (lua_isnil(L, -1)) {
        lua_pop(L, 1);
        lua_pushboolean(L, 1);
    }
    lua_pushvalue(L, -1);
    m.modules[uri] = luaL_ref(L, LUA_REGISTRYINDEX);
    return 1;
}

void setFuncs(lua_State* L, const luaL_Reg* funcs) { luaL_setfuncs(L, funcs, 0); }

} // namespace

void pushEntity(lua_State* L, ecs::Entity e) {
    auto* ud = static_cast<LuaEntity*>(lua_newuserdatauv(L, sizeof(LuaEntity), 0));
    ud->packed = e.packed();
    ud->generation = implOf(L).sceneGeneration;
    luaL_setmetatable(L, kEntityMeta);
}

ecs::Entity checkEntity(lua_State* L, int idx) {
    auto* ud = static_cast<LuaEntity*>(luaL_checkudata(L, idx, kEntityMeta));
    auto& m = implOf(L);
    const ecs::Entity e{static_cast<u32>(ud->packed), static_cast<u32>(ud->packed >> 32)};
    if (!m.scene || ud->generation != m.sceneGeneration || !m.scene->valid(e)) luaL_error(L, "the entity no longer exists");
    return e;
}

void openAgrLibrary(lua_State* L) {
    // Safe standard libraries only: no io, os, debug, package (no file or process access).
    static const luaL_Reg libs[] = {{LUA_GNAME, luaopen_base},       {LUA_TABLIBNAME, luaopen_table}, {LUA_STRLIBNAME, luaopen_string},
                                    {LUA_MATHLIBNAME, luaopen_math}, {LUA_UTF8LIBNAME, luaopen_utf8}, {LUA_COLIBNAME, luaopen_coroutine}};
    for (const luaL_Reg& lib : libs) {
        luaL_requiref(L, lib.name, lib.func, 1);
        lua_pop(L, 1);
    }
    for (const char* unsafe : {"dofile", "loadfile", "load", "collectgarbage"}) {
        lua_pushnil(L);
        lua_setglobal(L, unsafe);
    }
    lua_getglobal(L, "string");  // string.dump produces bytecode
    lua_pushnil(L);
    lua_setfield(L, -2, "dump");
    lua_pop(L, 1);

    // Entity type.
    luaL_newmetatable(L, kEntityMeta);
    static const luaL_Reg methods[] = {
        {"isValid", e_isValid}, {"name", e_name}, {"setName", e_setName}, {"position", e_position}, {"setPosition", e_setPosition},
        {"translate", e_translate}, {"worldPosition", e_worldPosition}, {"rotation", e_rotation}, {"setRotation", e_setRotation},
        {"rotate", e_rotate}, {"scale", e_scale}, {"setScale", e_setScale}, {"get", e_get}, {"set", e_set}, {"has", e_has},
        {"add", e_add}, {"remove", e_remove}, {"parent", e_parent}, {"children", e_children}, {"find", e_find},
        {"destroy", e_destroy}, {"script", e_script}, {"velocity", e_velocity}, {"setVelocity", e_setVelocity},
        {"applyForce", e_applyForce}, {"applyImpulse", e_applyImpulse}, {"setAngularVelocity", e_setAngularVelocity},
        {nullptr, nullptr}};
    lua_newtable(L);
    setFuncs(L, methods);
    lua_setfield(L, -2, "__index");
    lua_pushcfunction(L, e_eq);
    lua_setfield(L, -2, "__eq");
    lua_pushcfunction(L, e_tostring);
    lua_setfield(L, -2, "__tostring");
    lua_pushliteral(L, "Entity");
    lua_setfield(L, -2, "__name");
    lua_pop(L, 1);

    // agr.*
    static const luaL_Reg agr[] = {{"log", a_log}, {"warn", a_warn}, {"error", a_error}, {"find", a_find}, {"spawn", a_spawn}, {nullptr, nullptr}};
    static const luaL_Reg input[] = {{"keyDown", i_keyDown},         {"keyPressed", i_keyPressed}, {"keyReleased", i_keyReleased},
                                     {"mouseDown", i_mouseDown},     {"mousePressed", i_mousePressed}, {"mousePosition", i_mousePosition},
                                     {"touchCount", i_touchCount},   {"touch", i_touch},           {"axis", i_axis},
                                     {nullptr, nullptr}};
    static const luaL_Reg time[] = {{"delta", t_delta}, {"fixedDelta", t_fixedDelta}, {"total", t_total}, {"frame", t_frame}, {nullptr, nullptr}};
    static const luaL_Reg physics[] = {{"raycast", p_raycast}, {"setGravity", p_setGravity}, {"gravity", p_gravity}, {nullptr, nullptr}};
    lua_newtable(L);
    setFuncs(L, agr);
    lua_newtable(L);
    setFuncs(L, input);
    lua_setfield(L, -2, "input");
    lua_newtable(L);
    setFuncs(L, time);
    lua_setfield(L, -2, "time");
    lua_newtable(L);
    setFuncs(L, physics);
    lua_setfield(L, -2, "physics2d");
    lua_setglobal(L, "agr");
    lua_pushcfunction(L, a_log);
    lua_setglobal(L, "print");
    lua_pushcfunction(L, a_require);
    lua_setglobal(L, "require");
}

} // namespace agr
