#pragma once
// Input actions: named buttons and axes ("jump", "move_x") bound to keys, mouse buttons,
// gamepads and touch zones / virtual sticks. Games read actions instead of devices, so one
// script works with a keyboard, a controller and a phone screen, and players' bindings can
// change without touching code. Stored in assets://input.agrinput (JSON).
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include <nlohmann/json_fwd.hpp>

#include "agr/core/FileSystem.hpp"
#include "agr/core/Result.hpp"
#include "agr/runtime/Input.hpp"

namespace agr {

struct InputBinding {
    enum class Type : u8 { Key, MouseButton, GamepadButton, GamepadAxis, TouchZone, TouchStick };
    Type type = Type::Key;
    Key key = Key::Unknown;
    MouseButton mouse = MouseButton::Left;
    GamepadButton button = GamepadButton::South;
    GamepadAxis axis = GamepadAxis::LeftX;
    /// Contribution to an axis: +1 / -1 for keys and buttons (right / left), a multiplier for
    /// analog inputs (-1 inverts a stick).
    float scale = 1.0f;
    i32 pad = -1;  ///< Gamepad slot, -1 = any.
    /// Touch zones and sticks: screen rectangle in fractions of the screen (x, y, width,
    /// height; top-left origin). Only touches that start inside it drive the binding (a
    /// finger sliding in from elsewhere does not).
    math::Vec4 zone{0.0f, 0.0f, 1.0f, 1.0f};
    u8 stickAxis = 0;      ///< TouchStick: 0 = horizontal, 1 = vertical (up positive).
    float radius = 0.08f;  ///< TouchStick: drag for full deflection, fraction of the screen height.
};

struct InputAction {
    enum class Kind : u8 { Button, Axis };
    std::string name;
    Kind kind = Kind::Button;
    float deadzone = 0.2f;  ///< Analog values below it count as 0.
    std::vector<InputBinding> bindings;
};

/// The set of actions of a game.
struct InputMap {
    std::vector<InputAction> actions;

    static Result<InputMap> fromJson(const nlohmann::ordered_json& j);
    nlohmann::ordered_json toJson() const;
    /// move_x, move_y (axes: keyboard arrows / WASD / ZQSD, left stick, touch stick on the
    /// left of the screen), jump, fire, pause (buttons).
    static InputMap defaults();
    const InputAction* find(std::string_view name) const;
};

std::string describe(const InputBinding& b);  ///< "Key Space", "Pad South", "Touch stick (x)"...

struct ActionState {
    float value = 0.0f;  ///< Button: 0 or 1; axis: -1..1.
    bool down = false;   ///< Button held, or axis beyond half its range.
    bool pressed = false;
    bool released = false;
};

/// Evaluates an InputMap from the raw Input once per frame (the engine does it).
class InputActions {
public:
    static constexpr const char* kDefaultUri = "assets://input.agrinput";

    void setMap(InputMap map);
    const InputMap& map() const { return m_map; }
    /// Loads `uri`; when the file does not exist, the defaults are used (no error).
    Status load(VirtualFileSystem& vfs, const std::string& uri = kDefaultUri);
    Status save(VirtualFileSystem& vfs, const std::string& uri = kDefaultUri) const;

    /// `screen`: window size in the units of touch / mouse positions.
    void update(const Input& input, math::Vec2 screen);
    bool has(std::string_view name) const { return m_index.contains(std::string(name)); }
    const ActionState& state(std::string_view name) const;
    float value(std::string_view name) const { return state(name).value; }
    bool down(std::string_view name) const { return state(name).down; }
    bool pressed(std::string_view name) const { return state(name).pressed; }
    bool released(std::string_view name) const { return state(name).released; }

private:
    struct Stick {
        u64 touch = 0;
        math::Vec2 origin;
        bool active = false;
    };
    InputMap m_map;
    std::vector<ActionState> m_states;
    std::vector<std::vector<Stick>> m_sticks;  // per action, per binding
    std::unordered_map<std::string, usize> m_index;
};

} // namespace agr
