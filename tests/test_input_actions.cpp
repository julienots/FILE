#include <doctest/doctest.h>

#include <nlohmann/json.hpp>

#include "agr/runtime/InputActions.hpp"

using namespace agr;
using math::Vec2;

namespace {
constexpr Vec2 kScreen{1000.0f, 500.0f};
struct Frame {
    Input input;
    InputActions actions;
    Frame() { actions.setMap(InputMap::defaults()); }
    void step() {
        actions.update(input, kScreen);
        input.beginFrame();
    }
};
}  // namespace

TEST_SUITE("input") {

TEST_CASE("key names and gamepad names round trip") {
    for (Key k : {Key::A, Key::Z, Key::Num0, Key::Num7, Key::Space, Key::Left, Key::F1, Key::F12, Key::LeftShift, Key::AndroidBack})
        CHECK(parseKey(keyName(k)) == k);
    CHECK(parseKey("enter") == Key::Return);
    CHECK(parseKey("f13") == Key::Unknown);
    CHECK(parseKey("Nope") == Key::Unknown);
    for (u8 i = 0; i < static_cast<u8>(GamepadButton::Count); ++i)
        CHECK(parseGamepadButton(enumName(static_cast<GamepadButton>(i))) == static_cast<GamepadButton>(i));
    for (u8 i = 0; i < static_cast<u8>(GamepadAxis::Count); ++i)
        CHECK(parseGamepadAxis(enumName(static_cast<GamepadAxis>(i))) == static_cast<GamepadAxis>(i));
}

TEST_CASE("gamepads take free slots; buttons and axes per pad") {
    Input in;
    CHECK(in.gamepadConnect(101, "Pad A") == 0);
    CHECK(in.gamepadConnect(202, "Pad B") == 1);
    CHECK(in.gamepadCount() == 2);
    in.setGamepadButton(202, GamepadButton::South, true);
    in.setGamepadAxis(101, GamepadAxis::LeftX, 2.0f);  // clamped
    CHECK(in.gamepadDown(1, GamepadButton::South));
    CHECK(in.gamepadPressed(1, GamepadButton::South));
    CHECK_FALSE(in.gamepadDown(0, GamepadButton::South));
    CHECK(in.anyGamepadDown(GamepadButton::South));
    CHECK(in.gamepadAxis(0, GamepadAxis::LeftX) == doctest::Approx(1.0f));
    in.beginFrame();
    CHECK_FALSE(in.gamepadPressed(1, GamepadButton::South));
    in.gamepadDisconnect(101);
    CHECK_FALSE(in.gamepadConnected(0));
    CHECK(in.gamepadAxis(0, GamepadAxis::LeftX) == 0.0f);
    CHECK(in.gamepadConnect(303, "Pad C") == 0);  // slot reused
    CHECK(in.gamepadName(0) == "Pad C");
}

TEST_CASE("keyboard drives axes and buttons with pressed / released edges") {
    Frame f;
    f.input.setKey(Key::Right, true);
    f.input.setKey(Key::Space, true);
    f.actions.update(f.input, kScreen);
    CHECK(f.actions.value("move_x") == doctest::Approx(1.0f));
    CHECK(f.actions.down("jump"));
    CHECK(f.actions.pressed("jump"));
    f.input.beginFrame();
    f.input.setKey(Key::Left, true);  // both: cancel out
    f.actions.update(f.input, kScreen);
    CHECK(f.actions.value("move_x") == doctest::Approx(0.0f));
    CHECK_FALSE(f.actions.pressed("jump"));
    CHECK(f.actions.down("jump"));
    f.input.beginFrame();
    f.input.setKey(Key::Space, false);
    f.input.setKey(Key::Right, false);
    f.actions.update(f.input, kScreen);
    CHECK(f.actions.released("jump"));
    CHECK(f.actions.value("move_x") == doctest::Approx(-1.0f));
    CHECK_FALSE(f.actions.has("nope"));
    CHECK(f.actions.value("nope") == 0.0f);
}

TEST_CASE("gamepad: stick with dead zone, up positive; buttons") {
    Frame f;
    f.input.gamepadConnect(7, "Pad");
    f.input.setGamepadAxis(7, GamepadAxis::LeftX, 0.1f);  // inside the dead zone
    f.input.setGamepadAxis(7, GamepadAxis::LeftY, -1.0f);  // pushed up
    f.input.setGamepadButton(7, GamepadButton::South, true);
    f.actions.update(f.input, kScreen);
    CHECK(f.actions.value("move_x") == 0.0f);
    CHECK(f.actions.value("move_y") == doctest::Approx(1.0f));
    CHECK(f.actions.pressed("jump"));
    f.input.setGamepadAxis(7, GamepadAxis::LeftX, 0.6f);
    f.actions.update(f.input, kScreen);
    CHECK(f.actions.value("move_x") == doctest::Approx((0.6f - 0.2f) / 0.8f));  // rescaled beyond the dead zone
}

TEST_CASE("touch: a zone acts as a button, a virtual stick follows the finger that started in it") {
    Frame f;
    // Right-side zone = jump.
    f.input.touchDown(1, {800.0f, 400.0f}, 1.0f);
    f.actions.update(f.input, kScreen);
    CHECK(f.actions.pressed("jump"));
    f.input.beginFrame();
    f.input.touchUp(1, {800.0f, 400.0f});
    f.actions.update(f.input, kScreen);
    f.input.beginFrame();
    f.actions.update(f.input, kScreen);
    CHECK_FALSE(f.actions.down("jump"));

    // Stick: starts lower left, drags right and up by one radius (0.08 x 500 = 40 px).
    f.input.touchDown(2, {100.0f, 400.0f}, 1.0f);
    f.actions.update(f.input, kScreen);
    CHECK(f.actions.value("move_x") == 0.0f);
    f.input.beginFrame();
    f.input.touchMove(2, {140.0f, 360.0f}, 1.0f);
    f.actions.update(f.input, kScreen);
    CHECK(f.actions.value("move_x") == doctest::Approx(1.0f));
    CHECK(f.actions.value("move_y") == doctest::Approx(1.0f));
    // Moving into the jump zone keeps controlling the stick, it does not press jump.
    f.input.beginFrame();
    f.input.touchMove(2, {800.0f, 360.0f}, 1.0f);
    f.actions.update(f.input, kScreen);
    CHECK(f.actions.value("move_x") == doctest::Approx(1.0f));
    CHECK_FALSE(f.actions.down("jump"));
    // A touch starting outside the stick zone does not take it.
    f.input.beginFrame();
    f.input.touchUp(2, {800.0f, 360.0f});
    f.actions.update(f.input, kScreen);
    f.input.beginFrame();
    f.input.touchDown(3, {500.0f, 50.0f}, 1.0f);
    f.actions.update(f.input, kScreen);
    f.input.beginFrame();
    f.input.touchMove(3, {600.0f, 50.0f}, 1.0f);
    f.actions.update(f.input, kScreen);
    CHECK(f.actions.value("move_x") == 0.0f);
}

TEST_CASE("input maps: JSON round trip, invalid entries skipped, duplicates rejected") {
    const InputMap d = InputMap::defaults();
    auto back = InputMap::fromJson(d.toJson());
    REQUIRE(back);
    REQUIRE(back.value().actions.size() == d.actions.size());
    for (usize i = 0; i < d.actions.size(); ++i) {
        CHECK(back.value().actions[i].name == d.actions[i].name);
        CHECK(back.value().actions[i].bindings.size() == d.actions[i].bindings.size());
    }
    CHECK(back.value().toJson() == d.toJson());

    nlohmann::ordered_json j = {{"format", "agr.input"},
                                {"actions",
                                 {{{"name", "shoot"},
                                   {"bindings", {{{"type", "key"}, {"input", "Nope"}}, {{"type", "laser"}}, {{"type", "gamepadButton"}, {"input", "East"}, {"pad", 1}}}}}}}};
    auto m = InputMap::fromJson(j);
    REQUIRE(m);
    REQUIRE(m.value().actions[0].bindings.size() == 1);
    CHECK(m.value().actions[0].bindings[0].pad == 1);
    CHECK(describe(m.value().actions[0].bindings[0]) == "Manette East [manette 2]");
    j["actions"].push_back(j["actions"][0]);
    CHECK_FALSE(InputMap::fromJson(j));
    CHECK_FALSE(InputMap::fromJson(nlohmann::ordered_json::object()));
}

}
