// entity:navigateTo / stopNavigation / navigationStatus / navigationPath, agr.nav.path / rebuild.
#include "ScriptInternal.hpp"
#include "agr/navigation/NavigationSystem.hpp"

namespace agr {

namespace {

NavigationSystem& navOf(lua_State* L) {
    auto& m = implOf(L);
    if (!m.navigation) luaL_error(L, "navigation is not enabled (NavigationPlugin)");
    return *m.navigation;
}

math::Vec3 point(lua_State* L, int idx, ecs::Entity e) {
    // x, y [, z]: z defaults to the entity's current depth (2D games).
    const float x = static_cast<float>(luaL_checknumber(L, idx)), y = static_cast<float>(luaL_checknumber(L, idx + 1));
    float z = 0.0f;
    if (!lua_isnoneornil(L, idx + 2)) z = static_cast<float>(luaL_checknumber(L, idx + 2));
    else if (!e.isNull()) z = sceneOf(L).worldMatrix(e).c[3].z;
    return {x, y, z};
}

/// entity:navigateTo(x, y [, z]) -> true when the destination is reachable.
int e_navigateTo(lua_State* L) {
    const ecs::Entity e = checkEntity(L, 1);
    Scene& s = sceneOf(L);
    if (!s.registry().has<NavAgentComponent>(e)) luaL_error(L, "the entity has no NavAgent component");
    lua_pushboolean(L, navOf(L).navigateTo(s, e, point(L, 2, e)));
    return 1;
}
int e_stopNavigation(lua_State* L) {
    navOf(L).stop(checkEntity(L, 1));
    return 0;
}
int e_navigationStatus(lua_State* L) {
    lua_pushstring(L, enumName(navOf(L).status(checkEntity(L, 1))));
    return 1;
}
void pushPoints(lua_State* L, const std::vector<math::Vec3>& pts) {
    lua_createtable(L, static_cast<int>(pts.size()), 0);
    for (usize i = 0; i < pts.size(); ++i) {
        lua_createtable(L, 0, 3);
        lua_pushnumber(L, pts[i].x);
        lua_setfield(L, -2, "x");
        lua_pushnumber(L, pts[i].y);
        lua_setfield(L, -2, "y");
        lua_pushnumber(L, pts[i].z);
        lua_setfield(L, -2, "z");
        lua_rawseti(L, -2, static_cast<lua_Integer>(i + 1));
    }
}
int e_navigationPath(lua_State* L) {
    pushPoints(L, navOf(L).path(checkEntity(L, 1)));
    return 1;
}
/// agr.nav.path(x1, y1, z1, x2, y2, z2 [, "3d" | "2d"]) -> points ({x, y, z}) or nil.
int n_path(lua_State* L) {
    NavigationSystem& nav = navOf(L);
    Scene& s = sceneOf(L);
    const math::Vec3 a = point(L, 1, {}), b = point(L, 4, {});
    bool use3D = !nav.mesh().empty();
    if (lua_isstring(L, 7)) use3D = std::string_view(lua_tostring(L, 7)) == "3d";
    std::vector<math::Vec3> pts;
    if (!nav.findPath(s, a, b, use3D, pts)) {
        lua_pushnil(L);
        return 1;
    }
    pushPoints(L, pts);
    return 1;
}
int n_rebuild(lua_State* L) {
    navOf(L).rebuild(sceneOf(L));
    return 0;
}

} // namespace

void openNavigationBindings(lua_State* L) {
    static const luaL_Reg methods[] = {{"navigateTo", e_navigateTo},
                                       {"stopNavigation", e_stopNavigation},
                                       {"navigationStatus", e_navigationStatus},
                                       {"navigationPath", e_navigationPath},
                                       {nullptr, nullptr}};
    addEntityMethods(L, methods);
    static const luaL_Reg nav[] = {{"path", n_path}, {"rebuild", n_rebuild}, {nullptr, nullptr}};
    addAgrLibrary(L, "nav", nav);
}

} // namespace agr
