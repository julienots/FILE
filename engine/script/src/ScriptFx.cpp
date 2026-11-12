// Game feel for scripts (Godot / Unity style helpers):
//   agr.tween(entity, {position = {x, y, z}, scale = 1.2, alpha = 0, ...}, seconds [, options]) -> id
//   agr.stopTween(id), agr.stopTweens(entity [, finish]), agr.isTweening(entity)
//   agr.fx.burst(x, y, z [, options])         one-shot 3D particle burst (impacts, pickups)
//   agr.fx.shake(strength [, seconds])        camera shake (trauma, decays)
//   agr.fx.popup(x, y, z, text [, options])   floating text over a world point (damage, +1)
//   agr.fx.flash(r, g, b [, a, seconds])      full-screen color flash
//   agr.fx.fade(alpha [, seconds, r, g, b])   full-screen fade (scene transitions)
//   agr.fx.hitStop(seconds [, scale])         brief slow motion on impacts
//   agr.time.setScale(s), agr.time.scale()
#include <algorithm>
#include <cmath>

#include "ScriptInternal.hpp"
#include "agr/core/Log.hpp"
#include "agr/render/RenderComponents.hpp"
#include "agr/render/Renderer.hpp"
#include "agr/scene/Reflection.hpp"
#include "agr/scene/Scene.hpp"
#include "agr/ui/UIComponents.hpp"

namespace agr {

namespace {

// ---- easing
const char* const kEaseNames[] = {"linear", "inQuad", "outQuad", "inOutQuad", "inCubic", "outCubic", "inOutCubic",
                                  "inBack", "outBack", "outElastic", "outBounce", "inOutSine"};

float easeOutBounce(float t) {
    const float n = 7.5625f, d = 2.75f;
    if (t < 1.0f / d) return n * t * t;
    if (t < 2.0f / d) {
        t -= 1.5f / d;
        return n * t * t + 0.75f;
    }
    if (t < 2.5f / d) {
        t -= 2.25f / d;
        return n * t * t + 0.9375f;
    }
    t -= 2.625f / d;
    return n * t * t + 0.984375f;
}

float ease(int kind, float t) {
    constexpr float c1 = 1.70158f, c3 = c1 + 1.0f, pi = 3.14159265f;
    switch (kind) {
        case 1: return t * t;
        case 2: return 1.0f - (1.0f - t) * (1.0f - t);
        case 3: return t < 0.5f ? 2.0f * t * t : 1.0f - std::pow(-2.0f * t + 2.0f, 2.0f) / 2.0f;
        case 4: return t * t * t;
        case 5: return 1.0f - std::pow(1.0f - t, 3.0f);
        case 6: return t < 0.5f ? 4.0f * t * t * t : 1.0f - std::pow(-2.0f * t + 2.0f, 3.0f) / 2.0f;
        case 7: return c3 * t * t * t - c1 * t * t;
        case 8: return 1.0f + c3 * std::pow(t - 1.0f, 3.0f) + c1 * std::pow(t - 1.0f, 2.0f);
        case 9:
            if (t <= 0.0f || t >= 1.0f) return t;
            return std::pow(2.0f, -10.0f * t) * std::sin((t * 10.0f - 0.75f) * (2.0f * pi / 3.0f)) + 1.0f;
        case 10: return easeOutBounce(t);
        case 11: return -(std::cos(pi * t) - 1.0f) / 2.0f;
        default: return t;
    }
}

int easeIndex(lua_State* L, const char* name) {
    for (int i = 0; i < static_cast<int>(std::size(kEaseNames)); ++i)
        if (std::string_view(name) == kEaseNames[i]) return i;
    return luaL_error(L, "unknown easing '%s' (linear, inQuad, outQuad, inOutQuad, inCubic, outCubic, inOutCubic, inBack, outBack, outElastic, outBounce, inOutSine)", name);
}

// ---- reflected fields
struct FieldRef {
    const FieldInfo* field = nullptr;
    void* data = nullptr;
};

FieldRef findField(Scene& scene, ecs::Entity e, const std::string& component, const std::string& field) {
    const ComponentInfo* info = ComponentRegistry::instance().find(component);
    if (!info) return {};
    ecs::PoolBase* pool = scene.registry().findPool(info->id);
    if (!pool || !pool->contains(e)) return {};
    const FieldInfo* f = info->field(field);
    if (!f) return {};
    return {f, f->access(pool->getRaw(e))};
}

int fieldSize(FieldType t) {
    switch (t) {
        case FieldType::Float: return 1;
        case FieldType::Vec2: return 2;
        case FieldType::Vec3: return 3;
        case FieldType::Vec4: return 4;
        default: return 0;
    }
}

math::Vec4 readValue(const FieldRef& r) {
    const float* p = static_cast<const float*>(r.data);
    math::Vec4 v{0.0f, 0.0f, 0.0f, 0.0f};
    for (int i = 0; i < fieldSize(r.field->type); ++i) v[i] = p[i];
    return v;
}

void writeValue(const FieldRef& r, const math::Vec4& v, u32 mask) {
    float* p = static_cast<float*>(r.data);
    for (int i = 0; i < fieldSize(r.field->type); ++i)
        if (mask & (1u << i)) p[i] = v[i];
}

/// The component carrying the entity's color (sprites, UI, meshes).
const char* colorComponent(Scene& scene, ecs::Entity e) {
    for (const char* c : {"SpriteRenderer", "UIImage", "UIText", "MeshRenderer"})
        if (findField(scene, e, c, "color").data) return c;
    return nullptr;
}

/// Reads a number or an array of numbers {x, y, z, w}; a single number fills `n` components.
math::Vec4 readTarget(lua_State* L, int idx, int n) {
    math::Vec4 v{0.0f, 0.0f, 0.0f, 0.0f};
    if (lua_isnumber(L, idx)) {
        const float x = static_cast<float>(lua_tonumber(L, idx));
        for (int i = 0; i < n; ++i) v[i] = x;
        return v;
    }
    luaL_checktype(L, idx, LUA_TTABLE);
    for (int i = 0; i < 4; ++i) {
        lua_rawgeti(L, idx, i + 1);
        if (lua_isnumber(L, -1)) v[i] = static_cast<float>(lua_tonumber(L, -1));
        lua_pop(L, 1);
    }
    return v;
}

// ---- tweens
/// agr.tween(entity, properties, seconds [, options]) -> id.
/// properties: position, scale, rotation (Euler degrees), color, alpha, uiPosition, uiSize, or
/// "Component.field" (float / vector field). options: ease, delay, loop ("yoyo" | "repeat"),
/// times (cycles, default 1, or forever with loop), from (start value), unscaled, onDone.
int a_tween(lua_State* L) {
    auto& m = implOf(L);
    Scene& scene = sceneOf(L);
    const ecs::Entity e = checkEntity(L, 1);
    luaL_checktype(L, 2, LUA_TTABLE);
    const float duration = std::max(0.0f, static_cast<float>(luaL_checknumber(L, 3)));
    FxState::Tween base;
    base.entity = e;
    base.duration = duration;
    base.id = m.fx.nextTween++;
    bool hasFrom = false;
    if (lua_istable(L, 4)) {
        if (lua_getfield(L, 4, "ease") == LUA_TSTRING) base.ease = easeIndex(L, lua_tostring(L, -1));
        lua_pop(L, 1);
        if (lua_getfield(L, 4, "delay") == LUA_TNUMBER) base.delay = static_cast<float>(lua_tonumber(L, -1));
        lua_pop(L, 1);
        if (lua_getfield(L, 4, "loop") == LUA_TSTRING) {
            const std::string loop = lua_tostring(L, -1);
            if (loop != "yoyo" && loop != "repeat") luaL_error(L, "agr.tween: loop is \"yoyo\" or \"repeat\"");
            base.yoyo = loop == "yoyo";
            base.cycles = -1;
        }
        lua_pop(L, 1);
        if (lua_getfield(L, 4, "times") == LUA_TNUMBER) base.cycles = std::max(1, static_cast<int>(lua_tointeger(L, -1)));
        lua_pop(L, 1);
        if (lua_getfield(L, 4, "unscaled") == LUA_TBOOLEAN) base.unscaled = lua_toboolean(L, -1);
        lua_pop(L, 1);
        hasFrom = lua_getfield(L, 4, "from") != LUA_TNIL;
        lua_pop(L, 1);
    }
    bool first = true;
    lua_pushnil(L);
    while (lua_next(L, 2)) {  // key -2, value -1
        if (lua_type(L, -2) != LUA_TSTRING) luaL_error(L, "agr.tween: property names are strings");
        const std::string key = lua_tostring(L, -2);
        FxState::Tween t = base;
        if (key == "position" || key == "scale") {
            t.component = "Transform";
            t.field = key;
        } else if (key == "rotation") {
            t.component = "Transform";
            t.field = "rotation";
            t.euler = true;
        } else if (key == "color" || key == "alpha") {
            const char* c = colorComponent(scene, e);
            if (!c) luaL_error(L, "agr.tween: '%s' needs a SpriteRenderer, UIImage, UIText or MeshRenderer", key.c_str());
            t.component = c;
            t.field = "color";
            if (key == "alpha") t.mask = 0x8;
        } else if (key == "uiPosition" || key == "uiSize") {
            t.component = "UIRect";
            t.field = key == "uiPosition" ? "position" : "size";
        } else if (const auto dot = key.find('.'); dot != std::string::npos) {
            t.component = key.substr(0, dot);
            t.field = key.substr(dot + 1);
        } else {
            luaL_error(L, "agr.tween: unknown property '%s'", key.c_str());
        }
        int n = 3;
        if (!t.euler) {
            const FieldRef r = findField(scene, e, t.component, t.field);
            if (!r.data) luaL_error(L, "agr.tween: the entity has no %s.%s", t.component.c_str(), t.field.c_str());
            n = fieldSize(r.field->type);
            if (n == 0) luaL_error(L, "agr.tween: %s.%s is not a number or a vector", t.component.c_str(), t.field.c_str());
        }
        t.to = readTarget(L, -1, n);
        if (t.mask == 0x8) t.to = {0.0f, 0.0f, 0.0f, static_cast<float>(luaL_checknumber(L, -1))};
        if (hasFrom) {
            lua_getfield(L, 4, "from");
            t.from = readTarget(L, -1, n);
            if (t.mask == 0x8) t.from = {0.0f, 0.0f, 0.0f, static_cast<float>(luaL_checknumber(L, -1))};
            lua_pop(L, 1);
            t.hasFrom = true;
        }
        if (first && lua_istable(L, 4)) {
            if (lua_getfield(L, 4, "onDone") == LUA_TFUNCTION) t.onDone = luaL_ref(L, LUA_REGISTRYINDEX);
            else lua_pop(L, 1);
        }
        first = false;
        m.fx.tweens.push_back(std::move(t));
        lua_pop(L, 1);
    }
    lua_pushinteger(L, static_cast<lua_Integer>(base.id));
    return 1;
}

void releaseTween(lua_State* L, FxState::Tween& t) {
    if (t.onDone != LUA_NOREF) luaL_unref(L, LUA_REGISTRYINDEX, t.onDone);
    t.onDone = LUA_NOREF;
}

void applyTween(Scene& scene, FxState::Tween& t, float k) {
    if (!t.started) {
        if (!t.hasFrom) {
            if (t.euler) {
                const math::Vec3 r = scene.registry().get<TransformComponent>(t.entity).rotation.toEuler();
                t.from = {math::degrees(r.x), math::degrees(r.y), math::degrees(r.z), 0.0f};
            } else if (const FieldRef r = findField(scene, t.entity, t.component, t.field); r.data) {
                t.from = readValue(r);
            }
        }
        t.started = true;
    }
    const float e = ease(t.ease, t.backward ? 1.0f - k : k);
    const math::Vec4 v = t.from + (t.to - t.from) * e;
    if (t.euler) {
        if (auto* tr = scene.registry().tryGet<TransformComponent>(t.entity))
            tr->rotation = math::Quat::fromEuler({math::radians(v.x), math::radians(v.y), math::radians(v.z)});
    } else if (const FieldRef r = findField(scene, t.entity, t.component, t.field); r.data) {
        writeValue(r, v, t.mask);
    }
}

int a_stopTween(lua_State* L) {
    auto& m = implOf(L);
    const u64 id = static_cast<u64>(luaL_checkinteger(L, 1));
    for (auto& t : m.fx.tweens)
        if (t.id == id) {
            releaseTween(L, t);
            t.cycles = 0;
        }
    return 0;
}

/// agr.stopTweens(entity [, finish]): stops the entity's tweens (finish: jump to the end values).
int a_stopTweens(lua_State* L) {
    auto& m = implOf(L);
    Scene& scene = sceneOf(L);
    const ecs::Entity e = checkEntity(L, 1);
    const bool finish = lua_toboolean(L, 2);
    for (auto& t : m.fx.tweens)
        if (t.entity == e && t.cycles != 0) {
            if (finish) applyTween(scene, t, t.backward ? 0.0f : 1.0f);
            releaseTween(L, t);
            t.cycles = 0;
        }
    return 0;
}

int a_isTweening(lua_State* L) {
    auto& m = implOf(L);
    const ecs::Entity e = checkEntity(L, 1);
    bool any = false;
    for (const auto& t : m.fx.tweens) any = any || (t.entity == e && t.cycles != 0);
    lua_pushboolean(L, any);
    return 1;
}

// ---- overlay canvas (popups, flash, fade)
ecs::Entity fxCanvas(ScriptEngine::Impl& m, Scene& scene) {
    if (!m.fx.canvas.isNull() && scene.valid(m.fx.canvas)) return m.fx.canvas;
    const ecs::Entity c = scene.createEntity("__fx");
    auto& cv = scene.registry().emplace<UICanvasComponent>(c);
    cv.referenceResolution = {1920.0f, 1080.0f};
    cv.match = 0.5f;
    cv.sortOrder = 10000;  // over the game's interface
    cv.respectSafeArea = false;
    m.fx.canvas = c;
    m.fx.overlay = {};
    return c;
}

ecs::Entity fxOverlay(ScriptEngine::Impl& m, Scene& scene) {
    if (!m.fx.overlay.isNull() && scene.valid(m.fx.overlay)) return m.fx.overlay;
    const ecs::Entity o = scene.createEntity("__fx overlay", fxCanvas(m, scene));
    auto& r = scene.registry().emplace<UIRectComponent>(o);
    r.anchorMin = {0.0f, 0.0f};
    r.anchorMax = {1.0f, 1.0f};
    r.size = {0.0f, 0.0f};
    auto& img = scene.registry().emplace<UIImageComponent>(o);
    img.color = {0.0f, 0.0f, 0.0f, 0.0f};
    img.blocksPointer = false;  // the game stays playable under a flash or a fade
    m.fx.overlay = o;
    return o;
}

math::Vec4 optColor(lua_State* L, int idx, const char* field, math::Vec4 def) {
    if (lua_getfield(L, idx, field) == LUA_TTABLE) def = readTarget(L, -1, 4);
    lua_pop(L, 1);
    if (def.w == 0.0f && std::string_view(field) != "endColor") def.w = 1.0f;  // {r, g, b} = opaque
    return def;
}
float optNumber(lua_State* L, int idx, const char* field, float def) {
    if (!lua_istable(L, idx)) return def;
    if (lua_getfield(L, idx, field) == LUA_TNUMBER) def = static_cast<float>(lua_tonumber(L, -1));
    lua_pop(L, 1);
    return def;
}

/// agr.fx.burst(x, y, z [, {count, color, endColor, speed, life, size, endSize, gravity, spread
/// ("sphere" | "up" | "ring"), additive, texture}]): particles thrown once from a point, cleaned up
/// automatically.
int fx_burst(lua_State* L) {
    auto& m = implOf(L);
    Scene& scene = sceneOf(L);
    const math::Vec3 p{static_cast<float>(luaL_checknumber(L, 1)), static_cast<float>(luaL_checknumber(L, 2)),
                       static_cast<float>(luaL_checknumber(L, 3))};
    const bool opts = lua_istable(L, 4);
    const ecs::Entity e = scene.createEntity("__fx burst");
    scene.registry().get<TransformComponent>(e).position = p;
    auto& em = scene.registry().emplace<ParticleEmitter3DComponent>(e);
    em.loop = false;
    em.rate = 0.0f;
    em.duration = 0.05f;
    em.burst = static_cast<u32>(std::clamp(optNumber(L, 4, "count", 16.0f), 1.0f, 400.0f));
    em.maxParticles = em.burst;
    const float speed = optNumber(L, 4, "speed", 4.0f), life = std::max(0.05f, optNumber(L, 4, "life", 0.6f));
    em.speedMin = speed * 0.4f;
    em.speedMax = speed;
    em.lifetimeMin = life * 0.6f;
    em.lifetimeMax = life;
    em.startSize = optNumber(L, 4, "size", 0.22f);
    em.endSize = optNumber(L, 4, "endSize", 0.0f);
    em.gravity = {0.0f, optNumber(L, 4, "gravity", -6.0f), 0.0f};
    em.drag = optNumber(L, 4, "drag", 1.5f);
    em.angularVelocity = 4.0f;
    em.startColor = opts ? optColor(L, 4, "color", {1.0f, 0.85f, 0.4f, 1.0f}) : math::Color{1.0f, 0.85f, 0.4f, 1.0f};
    math::Vec4 end = em.startColor;
    end.w = 0.0f;
    em.endColor = opts ? optColor(L, 4, "endColor", end) : end;
    em.shape = EmitterShape3D::Sphere;
    em.shapeRadius = 0.1f;
    em.additive = true;
    if (opts) {
        if (lua_getfield(L, 4, "spread") == LUA_TSTRING) {
            const std::string s = lua_tostring(L, -1);
            if (s == "up") {
                em.shape = EmitterShape3D::Cone;
                em.coneAngle = math::radians(35.0f);
            } else if (s == "ring") {
                em.shape = EmitterShape3D::Cone;
                em.coneAngle = math::radians(88.0f);
            }
        }
        lua_pop(L, 1);
        if (lua_getfield(L, 4, "additive") == LUA_TBOOLEAN) em.additive = lua_toboolean(L, -1);
        lua_pop(L, 1);
        if (lua_getfield(L, 4, "texture") == LUA_TSTRING) em.texture = lua_tostring(L, -1);
        lua_pop(L, 1);
    }
    m.fx.temporaries.push_back({e, life + 0.3f});
    pushEntity(L, e);
    return 1;
}

/// agr.fx.shake(strength [, seconds]): adds camera shake ("trauma", 0..1, fades out over `seconds`).
int fx_shake(lua_State* L) {
    auto& m = implOf(L);
    const float s = std::clamp(static_cast<float>(luaL_checknumber(L, 1)), 0.0f, 1.0f);
    const float seconds = std::max(0.05f, static_cast<float>(luaL_optnumber(L, 2, 0.4)));
    m.fx.trauma = std::min(1.0f, m.fx.trauma + s);
    m.fx.traumaDecay = std::max(m.fx.trauma / seconds, 0.5f);
    return 0;
}

/// agr.fx.popup(x, y, z, text [, {color, size, duration, rise}]): text floating up from a world point.
int fx_popup(lua_State* L) {
    auto& m = implOf(L);
    Scene& scene = sceneOf(L);
    FxState::Popup p;
    p.world = {static_cast<float>(luaL_checknumber(L, 1)), static_cast<float>(luaL_checknumber(L, 2)), static_cast<float>(luaL_checknumber(L, 3))};
    const char* text = luaL_checkstring(L, 4);
    p.color = lua_istable(L, 5) ? optColor(L, 5, "color", {1.0f, 1.0f, 1.0f, 1.0f}) : math::Vec4{1.0f, 1.0f, 1.0f, 1.0f};
    p.duration = std::max(0.1f, optNumber(L, 5, "duration", 0.9f));
    p.rise = optNumber(L, 5, "rise", 1.2f);
    const ecs::Entity e = scene.createEntity("__fx popup", fxCanvas(m, scene));
    auto& r = scene.registry().emplace<UIRectComponent>(e);
    r.size = {600.0f, 120.0f};
    auto& t = scene.registry().emplace<UITextComponent>(e);
    t.text = text;
    t.fontSize = p.size = optNumber(L, 5, "size", 52.0f);
    t.color = p.color;
    t.wrap = false;
    p.entity = e;
    m.fx.popups.push_back(p);
    return 0;
}

void startFade(ScriptEngine::Impl& m, Scene& scene, float to, float seconds) {
    fxOverlay(m, scene);
    m.fx.fadeFrom = m.fx.fadeDuration > 0.0f ? m.fx.fadeFrom + (m.fx.fadeTo - m.fx.fadeFrom) * std::min(1.0f, m.fx.fadeTime / m.fx.fadeDuration) : m.fx.fadeTo;
    m.fx.fadeTo = to;
    m.fx.fadeTime = 0.0f;
    m.fx.fadeDuration = seconds;
}

/// agr.fx.flash(r, g, b [, a, seconds]): the screen flashes a color (damage, lightning).
int fx_flash(lua_State* L) {
    auto& m = implOf(L);
    Scene& scene = sceneOf(L);
    fxOverlay(m, scene);
    m.fx.flashColor = {static_cast<float>(luaL_checknumber(L, 1)), static_cast<float>(luaL_checknumber(L, 2)), static_cast<float>(luaL_checknumber(L, 3)),
                       static_cast<float>(luaL_optnumber(L, 4, 0.5))};
    m.fx.flashDuration = std::max(0.02f, static_cast<float>(luaL_optnumber(L, 5, 0.25)));
    m.fx.flash = m.fx.flashDuration;
    return 0;
}

/// agr.fx.fade(alpha [, seconds, r, g, b]): fades the screen to a color (1 = covered, 0 = clear).
int fx_fade(lua_State* L) {
    auto& m = implOf(L);
    Scene& scene = sceneOf(L);
    const float to = std::clamp(static_cast<float>(luaL_checknumber(L, 1)), 0.0f, 1.0f);
    const float seconds = std::max(0.0f, static_cast<float>(luaL_optnumber(L, 2, 0.5)));
    if (!lua_isnoneornil(L, 3))
        m.fx.overlayColor = {static_cast<float>(luaL_checknumber(L, 3)), static_cast<float>(luaL_checknumber(L, 4)), static_cast<float>(luaL_checknumber(L, 5)), 0.0f};
    startFade(m, scene, to, seconds);
    return 0;
}

/// agr.fx.hitStop(seconds [, scale]): the game runs at `scale` (default 0.05) for a moment.
int fx_hitStop(lua_State* L) {
    auto& m = implOf(L);
    if (!m.setTimeScale || !m.timeScale) return 0;
    const float seconds = std::clamp(static_cast<float>(luaL_checknumber(L, 1)), 0.0f, 1.0f);
    if (m.fx.hitStop <= 0.0f) m.fx.resumeScale = m.timeScale();
    m.fx.hitStop = std::max(m.fx.hitStop, seconds);
    m.setTimeScale(std::clamp(luaL_optnumber(L, 2, 0.05), 0.0, 1.0));
    m.fx.scaleTouched = true;
    return 0;
}

int t_setScale(lua_State* L) {
    auto& m = implOf(L);
    if (!m.setTimeScale) return luaL_error(L, "agr.time.setScale: not available here");
    const double s = std::clamp(luaL_checknumber(L, 1), 0.0, 10.0);
    if (m.fx.hitStop > 0.0f) m.fx.resumeScale = s;  // applied when the hit stop ends
    else m.setTimeScale(s);
    m.fx.scaleTouched = true;
    return 0;
}

int t_scale(lua_State* L) {
    auto& m = implOf(L);
    lua_pushnumber(L, m.fx.hitStop > 0.0f ? m.fx.resumeScale : (m.timeScale ? m.timeScale() : 1.0));
    return 1;
}

/// Primary camera entity (the one RenderView::fromPrimaryCamera uses).
ecs::Entity primaryCamera(Scene& scene) {
    ecs::Entity best;
    scene.registry().view<CameraComponent>().each([&](ecs::Entity e, CameraComponent& c) {
        if (!c.primary || !scene.isActiveInHierarchy(e)) return;
        if (best.isNull() || e.index < best.index) best = e;
    });
    return best;
}

} // namespace

void fxReset(ScriptEngine::Impl& m) {
    for (auto& t : m.fx.tweens)
        if (t.onDone != LUA_NOREF && m.L) luaL_unref(m.L, LUA_REGISTRYINDEX, t.onDone);
    const bool restore = m.fx.scaleTouched;
    m.fx = FxState{};
    if (restore && m.setTimeScale) m.setTimeScale(1.0);
}

void fxBeginFrame(ScriptEngine::Impl& m, Scene& scene) {
    // Remove last frame's shake so scripts see (and move) the real camera position.
    if (!m.fx.shakeCamera.isNull() && scene.valid(m.fx.shakeCamera))
        if (auto* t = scene.registry().tryGet<TransformComponent>(m.fx.shakeCamera)) t->position -= m.fx.shakeOffset;
    m.fx.shakeCamera = {};
    m.fx.shakeOffset = {0.0f, 0.0f, 0.0f};
}

void fxEndFrame(ScriptEngine::Impl& m, Scene& scene, const FrameTime& time) {
    FxState& fx = m.fx;
    const float dt = static_cast<float>(time.deltaSeconds), real = static_cast<float>(time.unscaledDeltaSeconds);
    lua_State* L = m.L;
    // Hit stop runs on real time.
    if (fx.hitStop > 0.0f) {
        fx.hitStop -= real;
        if (fx.hitStop <= 0.0f && m.setTimeScale) m.setTimeScale(fx.resumeScale);
    }
    // Tweens (callbacks run after the loop: they may start new tweens).
    std::vector<int> callbacks;
    for (auto& t : fx.tweens) {
        if (t.cycles == 0) continue;
        if (!scene.valid(t.entity)) {
            releaseTween(L, t);
            t.cycles = 0;
            continue;
        }
        if (t.fresh) {  // starts on the next frame; its start value shows now
            t.fresh = false;
            if (t.delay <= 0.0f) applyTween(scene, t, 0.0f);
            continue;
        }
        float step = t.unscaled ? real : dt;
        if (t.delay > 0.0f) {
            t.delay -= step;
            if (t.delay > 0.0f) continue;
            step = -t.delay;
            t.delay = 0.0f;
        }
        t.t += step;
        const float k = t.duration > 0.0f ? std::min(1.0f, t.t / t.duration) : 1.0f;
        applyTween(scene, t, k);
        if (k < 1.0f) continue;
        if (t.cycles > 0) --t.cycles;
        if (t.cycles == 0) {
            if (t.onDone != LUA_NOREF) callbacks.push_back(t.onDone);
            t.onDone = LUA_NOREF;
            continue;
        }
        t.t = 0.0f;
        if (t.yoyo) t.backward = !t.backward;
    }
    fx.tweens.erase(std::remove_if(fx.tweens.begin(), fx.tweens.end(), [](const FxState::Tween& t) { return t.cycles == 0; }), fx.tweens.end());
    for (int ref : callbacks) {
        lua_rawgeti(L, LUA_REGISTRYINDEX, ref);
        luaL_unref(L, LUA_REGISTRYINDEX, ref);
        m.inCall = true;
        m.callClock.restart();
        if (lua_pcall(L, 0, 0, 0) != LUA_OK) {
            m.reportError("agr.tween onDone", lua_tostring(L, -1));
            lua_pop(L, 1);
        }
        m.inCall = false;
    }
    // Temporary entities (bursts).
    for (auto& t : fx.temporaries) {
        t.remaining -= dt;
        if (t.remaining <= 0.0f && scene.valid(t.entity)) scene.destroyEntity(t.entity);
    }
    fx.temporaries.erase(std::remove_if(fx.temporaries.begin(), fx.temporaries.end(), [&](const auto& t) { return t.remaining <= 0.0f || !scene.valid(t.entity); }),
                         fx.temporaries.end());
    // Popups follow their world point, rise and fade (real time: they keep moving during a hit stop).
    const auto view = RenderView::fromPrimaryCamera(scene);
    const math::Vec2 screen = m.screenSize;
    for (auto& p : fx.popups) {
        p.age += real;
        if (!scene.valid(p.entity)) continue;
        if (p.age >= p.duration) {
            scene.destroyEntity(p.entity);
            continue;
        }
        const float k = p.age / p.duration;
        auto* r = scene.registry().tryGet<UIRectComponent>(p.entity);
        auto* t = scene.registry().tryGet<UITextComponent>(p.entity);
        if (!r || !t) continue;
        math::Vec2 anchor{-1.0f, -1.0f};
        if (view && screen.x > 0.0f && screen.y > 0.0f) {
            const math::Vec3 w = p.world + math::Vec3{0.0f, p.rise * ease(5, k), 0.0f};
            const math::Vec4 c = view->projectionMatrix(screen.x / screen.y, true) * view->view * math::Vec4(w.x, w.y, w.z, 1.0f);
            if (c.w > 1e-4f) anchor = {c.x / c.w * 0.5f + 0.5f, 0.5f - c.y / c.w * 0.5f};
        }
        r->anchorMin = r->anchorMax = anchor;
        const float pop = k < 0.15f ? 0.6f + 0.4f * ease(8, k / 0.15f) : 1.0f;  // pops in
        t->fontSize = p.size * pop;
        t->color = p.color;
        t->color.w = p.color.w * (k < 0.6f ? 1.0f : 1.0f - (k - 0.6f) / 0.4f);
    }
    fx.popups.erase(std::remove_if(fx.popups.begin(), fx.popups.end(), [&](const auto& p) { return p.age >= p.duration || !scene.valid(p.entity); }),
                    fx.popups.end());
    // Flash and fade: one overlay image.
    if (!fx.overlay.isNull() && scene.valid(fx.overlay)) {
        float fade = fx.fadeTo;
        if (fx.fadeDuration > 0.0f && fx.fadeTime < fx.fadeDuration) {
            fx.fadeTime += real;
            fade = fx.fadeFrom + (fx.fadeTo - fx.fadeFrom) * ease(3, std::min(1.0f, fx.fadeTime / fx.fadeDuration));
        }
        math::Vec4 color = fx.overlayColor;
        color.w = fade;
        if (fx.flash > 0.0f) {
            fx.flash = std::max(0.0f, fx.flash - real);
            const float a = fx.flashColor.w * fx.flash / fx.flashDuration;
            color = {color.x * (1.0f - a) + fx.flashColor.x * a, color.y * (1.0f - a) + fx.flashColor.y * a, color.z * (1.0f - a) + fx.flashColor.z * a,
                     std::max(color.w, a)};
        }
        if (auto* img = scene.registry().tryGet<UIImageComponent>(fx.overlay)) img->color = color;
    }
    // Camera shake: random offset proportional to trauma squared, applied until the next frame.
    if (fx.trauma > 0.0f) {
        fx.shakeTime += real;
        const ecs::Entity cam = primaryCamera(scene);
        if (!cam.isNull()) {
            const float amount = fx.trauma * fx.trauma;
            const bool ortho = scene.registry().get<CameraComponent>(cam).projection == Projection::Orthographic;
            const float scale = (ortho ? scene.registry().get<CameraComponent>(cam).orthographicSize * 0.06f : 0.35f) * amount;
            const float s = fx.shakeTime * 40.0f;
            const math::Vec3 local{std::sin(s * 1.13f) + 0.5f * std::sin(s * 2.71f), std::cos(s * 1.37f) + 0.5f * std::sin(s * 3.11f), 0.0f};
            const math::Mat4 world = scene.worldMatrix(cam);
            math::Vec3 offset = world.transformVector(local) * scale;
            if (const ecs::Entity parent = scene.parent(cam); !parent.isNull())
                offset = math::inverse(scene.worldMatrix(parent)).transformVector(offset);
            scene.registry().get<TransformComponent>(cam).position += offset;
            fx.shakeCamera = cam;
            fx.shakeOffset = offset;
        }
        fx.trauma = std::max(0.0f, fx.trauma - fx.traumaDecay * real);
        if (fx.trauma <= 0.0f) fx.shakeTime = 0.0f;
    }
}

void openFxBindings(lua_State* L) {
    lua_getglobal(L, "agr");
    for (auto [name, fn] : {std::pair<const char*, lua_CFunction>{"tween", a_tween}, {"stopTween", a_stopTween}, {"stopTweens", a_stopTweens}, {"isTweening", a_isTweening}}) {
        lua_pushcfunction(L, fn);
        lua_setfield(L, -2, name);
    }
    lua_pop(L, 1);
    static const luaL_Reg fx[] = {{"burst", fx_burst}, {"shake", fx_shake}, {"popup", fx_popup}, {"flash", fx_flash},
                                  {"fade", fx_fade},   {"hitStop", fx_hitStop}, {nullptr, nullptr}};
    addAgrLibrary(L, "fx", fx);
    static const luaL_Reg time[] = {{"setScale", t_setScale}, {"scale", t_scale}, {nullptr, nullptr}};
    extendAgrLibrary(L, "time", time);
}

} // namespace agr
