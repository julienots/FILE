// Gameplay helpers: agr.clone (copies of prototype entities) and agr.camera (screen <-> world
// through the primary camera: clicks and taps on the game world).
#include <cmath>

#include "ScriptInternal.hpp"
#include "agr/render/Renderer.hpp"

namespace agr {

namespace {

/// agr.clone(entity [, parent]) -> copy of the entity and its children (components and
/// scripts), active, placed under `parent` (default: the original's parent).
int a_clone(lua_State* L) {
    Scene& s = sceneOf(L);
    const ecs::Entity src = checkEntity(L, 1);
    const ecs::Entity parent = lua_isnoneornil(L, 2) ? s.parent(src) : checkEntity(L, 2);
    const ecs::Entity copy = s.duplicate(src);
    if (copy.isNull()) return luaL_error(L, "agr.clone: the entity cannot be copied");
    if (parent != s.parent(copy)) s.setParent(copy, parent, {}, false);
    if (auto* st = s.registry().tryGet<StateComponent>(copy)) st->active = true;  // prototypes are usually hidden
    s.updateTransforms();
    pushEntity(L, copy);
    return 1;
}

struct CameraRay {
    math::Vec3 origin, direction;
    bool orthographic = false;
};

/// Ray through a screen point (pixels, top-left origin) of the primary camera.
bool screenRay(lua_State* L, float sx, float sy, CameraRay& out) {
    Scene& s = sceneOf(L);
    const auto rv = RenderView::fromPrimaryCamera(s);
    const math::Vec2 size = implOf(L).screenSize;
    if (!rv || size.x <= 0.0f || size.y <= 0.0f) return false;
    const math::Mat4 vp = rv->projectionMatrix(size.x / size.y, true) * rv->view;
    const math::Mat4 inv = math::inverse(vp);
    const float nx = sx / size.x * 2.0f - 1.0f, ny = 1.0f - sy / size.y * 2.0f;
    const math::Vec4 a = inv * math::Vec4(nx, ny, -1.0f, 1.0f), b = inv * math::Vec4(nx, ny, 1.0f, 1.0f);
    const math::Vec3 p0 = math::Vec3{a.x, a.y, a.z} / a.w, p1 = math::Vec3{b.x, b.y, b.z} / b.w;
    out.origin = p0;
    out.direction = math::normalize(p1 - p0);
    out.orthographic = rv->projection == Projection::Orthographic;
    return true;
}

/// agr.camera.ray(sx, sy) -> ox, oy, oz, dx, dy, dz (nil without a camera).
int c_ray(lua_State* L) {
    CameraRay r;
    if (!screenRay(L, static_cast<float>(luaL_checknumber(L, 1)), static_cast<float>(luaL_checknumber(L, 2)), r)) {
        lua_pushnil(L);
        return 1;
    }
    for (float v : {r.origin.x, r.origin.y, r.origin.z, r.direction.x, r.direction.y, r.direction.z}) lua_pushnumber(L, v);
    return 6;
}

/// agr.camera.screenToWorld(sx, sy [, height]) -> x, y, z where the screen point meets the
/// game's ground: the plane z = 0 for an orthographic (2D) camera, else the horizontal plane
/// y = height (default 0). nil when it misses (looking at the sky).
int c_screenToWorld(lua_State* L) {
    CameraRay r;
    if (!screenRay(L, static_cast<float>(luaL_checknumber(L, 1)), static_cast<float>(luaL_checknumber(L, 2)), r)) {
        lua_pushnil(L);
        return 1;
    }
    math::Vec3 n{0.0f, 1.0f, 0.0f};
    float d = static_cast<float>(luaL_optnumber(L, 3, 0.0));
    if (r.orthographic && lua_isnoneornil(L, 3) && std::abs(r.direction.z) > 0.5f) {
        n = {0.0f, 0.0f, 1.0f};
        d = 0.0f;
    }
    const float denom = math::dot(n, r.direction);
    if (std::abs(denom) < 1e-6f) {
        lua_pushnil(L);
        return 1;
    }
    const float t = (d - math::dot(n, r.origin)) / denom;
    if (t < 0.0f) {
        lua_pushnil(L);
        return 1;
    }
    const math::Vec3 p = r.origin + r.direction * t;
    lua_pushnumber(L, p.x);
    lua_pushnumber(L, p.y);
    lua_pushnumber(L, p.z);
    return 3;
}

/// agr.camera.worldToScreen(x, y, z) -> sx, sy, visible (in front of the camera).
int c_worldToScreen(lua_State* L) {
    Scene& s = sceneOf(L);
    const auto rv = RenderView::fromPrimaryCamera(s);
    const math::Vec2 size = implOf(L).screenSize;
    if (!rv || size.x <= 0.0f || size.y <= 0.0f) {
        lua_pushnil(L);
        return 1;
    }
    const math::Vec4 c = rv->projectionMatrix(size.x / size.y, true) * rv->view *
                         math::Vec4(static_cast<float>(luaL_checknumber(L, 1)), static_cast<float>(luaL_checknumber(L, 2)),
                                    static_cast<float>(luaL_optnumber(L, 3, 0.0)), 1.0f);
    const float w = std::abs(c.w) > 1e-6f ? c.w : 1e-6f;
    lua_pushnumber(L, (c.x / w * 0.5f + 0.5f) * size.x);
    lua_pushnumber(L, (0.5f - c.y / w * 0.5f) * size.y);
    lua_pushboolean(L, c.w > 0.0f);
    return 3;
}

} // namespace

void openGameBindings(lua_State* L) {
    lua_getglobal(L, "agr");
    lua_pushcfunction(L, a_clone);
    lua_setfield(L, -2, "clone");
    lua_pop(L, 1);
    static const luaL_Reg camera[] = {{"ray", c_ray}, {"screenToWorld", c_screenToWorld}, {"worldToScreen", c_worldToScreen}, {nullptr, nullptr}};
    addAgrLibrary(L, "camera", camera);
}

} // namespace agr
