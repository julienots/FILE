// Entity:setParameter / setTrigger / getParameter / playState / currentState / setLayerWeight / layerWeight (Animator).
#include "ScriptInternal.hpp"

namespace agr {

namespace {

AnimationSystem& animOf(lua_State* L) {
    auto& m = implOf(L);
    if (!m.animation) luaL_error(L, "animation is not enabled (AnimationPlugin)");
    return *m.animation;
}

ecs::Entity animated(lua_State* L) {
    const ecs::Entity e = checkEntity(L, 1);
    if (!sceneOf(L).registry().has<AnimatorComponent>(e)) luaL_error(L, "the entity has no Animator");
    return e;
}

int e_setParameter(lua_State* L) {
    const ecs::Entity e = animated(L);
    const char* name = luaL_checkstring(L, 2);
    bool ok = false;
    if (lua_isboolean(L, 3)) ok = animOf(L).setBool(sceneOf(L), e, name, lua_toboolean(L, 3) != 0);
    else ok = animOf(L).setFloat(sceneOf(L), e, name, static_cast<float>(luaL_checknumber(L, 3)));
    if (!ok) luaL_error(L, "the Animator has no parameter '%s'", name);
    return 0;
}
int e_setTrigger(lua_State* L) {
    const ecs::Entity e = animated(L);
    const char* name = luaL_checkstring(L, 2);
    if (!animOf(L).setTrigger(sceneOf(L), e, name)) luaL_error(L, "the Animator has no parameter '%s'", name);
    return 0;
}
int e_getParameter(lua_State* L) {
    const ecs::Entity e = animated(L);
    const auto v = animOf(L).parameter(sceneOf(L), e, luaL_checkstring(L, 2));
    if (v) lua_pushnumber(L, *v);
    else lua_pushnil(L);
    return 1;
}
int e_playState(lua_State* L) {
    const ecs::Entity e = animated(L);
    const char* name = luaL_checkstring(L, 2);
    const char* layer = luaL_optstring(L, 4, "");
    if (!animOf(L).play(sceneOf(L), e, name, static_cast<float>(luaL_optnumber(L, 3, 0.0)), layer)) {
        if (layer[0]) luaL_error(L, "the Animator has no layer '%s' with a state '%s'", layer, name);
        luaL_error(L, "the Animator has no state '%s'", name);
    }
    return 0;
}
int e_currentState(lua_State* L) {
    const ecs::Entity e = animated(L);
    const std::string s = animOf(L).currentState(e, luaL_optstring(L, 2, ""));
    lua_pushstring(L, s.c_str());
    return 1;
}
int e_setLayerWeight(lua_State* L) {
    const ecs::Entity e = animated(L);
    const char* layer = luaL_checkstring(L, 2);
    if (!animOf(L).setLayerWeight(sceneOf(L), e, layer, static_cast<float>(luaL_checknumber(L, 3))))
        luaL_error(L, "the Animator has no layer '%s'", layer);
    return 0;
}
int e_layerWeight(lua_State* L) {
    const ecs::Entity e = animated(L);
    const auto w = animOf(L).layerWeight(sceneOf(L), e, luaL_checkstring(L, 2));
    if (w) lua_pushnumber(L, *w);
    else lua_pushnil(L);
    return 1;
}

} // namespace

void openAnimationBindings(lua_State* L) {
    static const luaL_Reg methods[] = {{"setParameter", e_setParameter}, {"setTrigger", e_setTrigger}, {"getParameter", e_getParameter},
                                       {"playState", e_playState},       {"currentState", e_currentState}, {"setLayerWeight", e_setLayerWeight},
                                       {"layerWeight", e_layerWeight},   {nullptr, nullptr}};
    addEntityMethods(L, methods);
}

} // namespace agr
