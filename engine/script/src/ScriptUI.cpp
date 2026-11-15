// agr.ui.* and Entity:setText() / text().
#include "ScriptInternal.hpp"

namespace agr {

namespace {

UISystem& uiOf(lua_State* L) {
    auto& m = implOf(L);
    if (!m.ui) luaL_error(L, "the UI is not enabled (UIPlugin)");
    return *m.ui;
}

int ui_pointerOverUI(lua_State* L) {
    lua_pushboolean(L, uiOf(L).pointerOverUI());
    return 1;
}
/// agr.ui.isTyping(): a text field has the keyboard (gameplay should ignore keys).
int ui_isTyping(lua_State* L) {
    lua_pushboolean(L, implOf(L).ui && implOf(L).ui->wantsText());
    return 1;
}
/// agr.ui.focus(entity): a text field takes the keyboard; another control gets the keyboard /
/// gamepad highlight; nil clears both.
int ui_focus(lua_State* L) {
    auto& m = implOf(L);
    if (!m.ui) return 0;
    if (lua_isnoneornil(L, 1)) {
        m.ui->focusField({});
        m.ui->setNavFocus({});
        return 0;
    }
    const ecs::Entity e = checkEntity(L, 1);
    if (sceneOf(L).registry().has<UIInputFieldComponent>(e)) m.ui->focusField(e);
    else m.ui->setNavFocus(e);
    return 0;
}
int ui_isPressed(lua_State* L) {
    const ecs::Entity e = checkEntity(L, 1);
    lua_pushboolean(L, uiOf(L).isPressed(e));
    return 1;
}

UITextComponent& textOf(lua_State* L, ecs::Entity e) {
    auto* t = sceneOf(L).registry().tryGet<UITextComponent>(e);
    if (!t) luaL_error(L, "the entity has no UIText");
    return *t;
}
int e_setText(lua_State* L) {
    const ecs::Entity e = checkEntity(L, 1);
    // Any value: numbers are formatted by Lua (score counters).
    luaL_tolstring(L, 2, nullptr);
    textOf(L, e).text = lua_tostring(L, -1);
    return 0;
}
int e_text(lua_State* L) {
    const ecs::Entity e = checkEntity(L, 1);
    lua_pushstring(L, textOf(L, e).text.c_str());
    return 1;
}

} // namespace

void openUIBindings(lua_State* L) {
    static const luaL_Reg ui[] = {{"pointerOverUI", ui_pointerOverUI}, {"isPressed", ui_isPressed}, {"isTyping", ui_isTyping},
                                  {"focus", ui_focus},             {nullptr, nullptr}};
    addAgrLibrary(L, "ui", ui);
    static const luaL_Reg methods[] = {{"setText", e_setText}, {"text", e_text}, {nullptr, nullptr}};
    addEntityMethods(L, methods);
}

} // namespace agr
