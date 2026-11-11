#include "agr/runtime/InputActions.hpp"

#include <algorithm>
#include <cmath>

#include <nlohmann/json.hpp>

#include "agr/core/Log.hpp"

namespace agr {

using json = nlohmann::ordered_json;

namespace {

constexpr const char* kTypeNames[] = {"key", "mouse", "gamepadButton", "gamepadAxis", "touchZone", "touchStick"};

const char* mouseName(MouseButton b) {
    switch (b) {
        case MouseButton::Left: return "Left";
        case MouseButton::Middle: return "Middle";
        case MouseButton::Right: return "Right";
        case MouseButton::X1: return "X1";
        case MouseButton::X2: return "X2";
        default: return "";
    }
}

MouseButton parseMouse(std::string_view n) {
    for (u8 i = 0; i < static_cast<u8>(MouseButton::Count); ++i)
        if (n == mouseName(static_cast<MouseButton>(i))) return static_cast<MouseButton>(i);
    return MouseButton::Count;
}

InputBinding keyBinding(Key k, float scale = 1.0f) {
    InputBinding b;
    b.type = InputBinding::Type::Key;
    b.key = k;
    b.scale = scale;
    return b;
}

bool insideZone(math::Vec2 p, math::Vec2 screen, math::Vec4 z) {
    if (screen.x <= 0.0f || screen.y <= 0.0f) return false;
    const float x = p.x / screen.x, y = p.y / screen.y;
    return x >= z.x && x <= z.x + z.z && y >= z.y && y <= z.y + z.w;
}

} // namespace

// ------------------------------------------------------------------ map
Result<InputMap> InputMap::fromJson(const json& j) {
    if (!j.is_object() || j.value("format", "") != "agr.input" || !j.contains("actions") || !j["actions"].is_array())
        return makeError(Error::Code::ParseError, "not an AGR input map (format agr.input)");
    InputMap map;
    for (const json& a : j["actions"]) {
        if (!a.is_object() || !a.contains("name") || !a["name"].is_string()) return makeError(Error::Code::ParseError, "action without a name");
        InputAction action;
        action.name = a["name"].get<std::string>();
        if (action.name.empty() || map.find(action.name)) return makeError(Error::Code::ParseError, "empty or duplicate action name '" + action.name + "'");
        action.kind = a.value("kind", "button") == "axis" ? InputAction::Kind::Axis : InputAction::Kind::Button;
        action.deadzone = std::clamp(a.value("deadzone", 0.2f), 0.0f, 0.95f);
        if (a.contains("bindings") && a["bindings"].is_array())
            for (const json& bj : a["bindings"]) {
                InputBinding b;
                const std::string type = bj.value("type", "");
                const auto* it = std::find_if(std::begin(kTypeNames), std::end(kTypeNames), [&](const char* n) { return type == n; });
                if (it == std::end(kTypeNames)) {
                    AGR_LOG_WARN("Input", "action '{}': unknown binding type '{}' skipped", action.name, type);
                    continue;
                }
                b.type = static_cast<InputBinding::Type>(it - std::begin(kTypeNames));
                b.scale = bj.value("scale", 1.0f);
                b.pad = bj.value("pad", -1);
                const std::string input = bj.value("input", "");
                bool ok = true;
                switch (b.type) {
                    case InputBinding::Type::Key: ok = (b.key = parseKey(input)) != Key::Unknown; break;
                    case InputBinding::Type::MouseButton: ok = (b.mouse = parseMouse(input)) != MouseButton::Count; break;
                    case InputBinding::Type::GamepadButton: ok = (b.button = parseGamepadButton(input)) != GamepadButton::Count; break;
                    case InputBinding::Type::GamepadAxis: ok = (b.axis = parseGamepadAxis(input)) != GamepadAxis::Count; break;
                    case InputBinding::Type::TouchZone:
                    case InputBinding::Type::TouchStick:
                        if (bj.contains("zone") && bj["zone"].is_array() && bj["zone"].size() == 4)
                            b.zone = {bj["zone"][0].get<float>(), bj["zone"][1].get<float>(), bj["zone"][2].get<float>(), bj["zone"][3].get<float>()};
                        b.stickAxis = input == "y" ? 1 : 0;
                        b.radius = std::max(bj.value("radius", 0.08f), 0.01f);
                        break;
                }
                if (!ok) {
                    AGR_LOG_WARN("Input", "action '{}': unknown input '{}' skipped", action.name, input);
                    continue;
                }
                action.bindings.push_back(b);
            }
        map.actions.push_back(std::move(action));
    }
    return map;
}

json InputMap::toJson() const {
    json out = {{"format", "agr.input"}, {"version", 1}, {"actions", json::array()}};
    for (const InputAction& a : actions) {
        json aj = {{"name", a.name}, {"kind", a.kind == InputAction::Kind::Axis ? "axis" : "button"}, {"deadzone", a.deadzone}, {"bindings", json::array()}};
        for (const InputBinding& b : a.bindings) {
            json bj = {{"type", kTypeNames[static_cast<usize>(b.type)]}};
            switch (b.type) {
                case InputBinding::Type::Key: bj["input"] = keyName(b.key); break;
                case InputBinding::Type::MouseButton: bj["input"] = mouseName(b.mouse); break;
                case InputBinding::Type::GamepadButton: bj["input"] = enumName(b.button); break;
                case InputBinding::Type::GamepadAxis: bj["input"] = enumName(b.axis); break;
                case InputBinding::Type::TouchZone:
                case InputBinding::Type::TouchStick:
                    bj["input"] = b.stickAxis ? "y" : "x";
                    bj["zone"] = {b.zone.x, b.zone.y, b.zone.z, b.zone.w};
                    if (b.type == InputBinding::Type::TouchStick) bj["radius"] = b.radius;
                    break;
            }
            if (b.scale != 1.0f) bj["scale"] = b.scale;
            if (b.pad >= 0) bj["pad"] = b.pad;
            aj["bindings"].push_back(std::move(bj));
        }
        out["actions"].push_back(std::move(aj));
    }
    return out;
}

const InputAction* InputMap::find(std::string_view name) const {
    for (const InputAction& a : actions)
        if (a.name == name) return &a;
    return nullptr;
}

InputMap InputMap::defaults() {
    InputMap m;
    auto axis = [](const char* name, std::initializer_list<InputBinding> bindings) {
        InputAction a;
        a.name = name;
        a.kind = InputAction::Kind::Axis;
        a.bindings = bindings;
        return a;
    };
    InputBinding padX;
    padX.type = InputBinding::Type::GamepadAxis;
    padX.axis = GamepadAxis::LeftX;
    InputBinding padY = padX;
    padY.axis = GamepadAxis::LeftY;
    padY.scale = -1.0f;  // controllers report down as positive; actions use up positive
    InputBinding stickX;
    stickX.type = InputBinding::Type::TouchStick;
    stickX.zone = {0.0f, 0.3f, 0.45f, 0.7f};  // lower left of the screen
    InputBinding stickY = stickX;
    stickY.stickAxis = 1;
    m.actions.push_back(axis("move_x", {keyBinding(Key::Right), keyBinding(Key::D), keyBinding(Key::Left, -1.0f), keyBinding(Key::A, -1.0f),
                                        keyBinding(Key::Q, -1.0f), padX, stickX}));
    m.actions.push_back(axis("move_y", {keyBinding(Key::Up), keyBinding(Key::W), keyBinding(Key::Z), keyBinding(Key::Down, -1.0f),
                                        keyBinding(Key::S, -1.0f), padY, stickY}));
    auto button = [](const char* name, std::initializer_list<InputBinding> bindings) {
        InputAction a;
        a.name = name;
        a.bindings = bindings;
        return a;
    };
    InputBinding south;
    south.type = InputBinding::Type::GamepadButton;
    south.button = GamepadButton::South;
    InputBinding west = south;
    west.button = GamepadButton::West;
    InputBinding start = south;
    start.button = GamepadButton::Start;
    InputBinding rightTouch;
    rightTouch.type = InputBinding::Type::TouchZone;
    rightTouch.zone = {0.55f, 0.3f, 0.45f, 0.7f};  // lower right of the screen
    InputBinding click;
    click.type = InputBinding::Type::MouseButton;
    click.mouse = MouseButton::Left;
    m.actions.push_back(button("jump", {keyBinding(Key::Space), south, rightTouch}));
    m.actions.push_back(button("fire", {click, keyBinding(Key::LeftCtrl), west}));
    m.actions.push_back(button("pause", {keyBinding(Key::Escape), keyBinding(Key::AndroidBack), start}));
    return m;
}

std::string describe(const InputBinding& b) {
    const std::string scale = b.scale == 1.0f ? "" : (b.scale == -1.0f ? " (-)" : " (x" + std::to_string(b.scale).substr(0, 4) + ")");
    const std::string pad = b.pad >= 0 ? " [manette " + std::to_string(b.pad + 1) + "]" : "";
    switch (b.type) {
        case InputBinding::Type::Key: return std::string("Touche ") + keyName(b.key) + scale;
        case InputBinding::Type::MouseButton: return std::string("Souris ") + mouseName(b.mouse) + scale;
        case InputBinding::Type::GamepadButton: return std::string("Manette ") + enumName(b.button) + pad + scale;
        case InputBinding::Type::GamepadAxis: return std::string("Manette ") + enumName(b.axis) + pad + scale;
        case InputBinding::Type::TouchZone: return "Zone tactile" + scale;
        case InputBinding::Type::TouchStick: return std::string("Joystick tactile (") + (b.stickAxis ? "y" : "x") + ")" + scale;
    }
    return "";
}

// ------------------------------------------------------------------ evaluation
void InputActions::setMap(InputMap map) {
    m_map = std::move(map);
    m_states.assign(m_map.actions.size(), ActionState{});
    m_sticks.clear();
    m_index.clear();
    for (usize i = 0; i < m_map.actions.size(); ++i) {
        m_index[m_map.actions[i].name] = i;
        m_sticks.emplace_back(m_map.actions[i].bindings.size());
    }
}

Status InputActions::load(VirtualFileSystem& vfs, const std::string& uri) {
    if (!vfs.exists(uri)) {
        setMap(InputMap::defaults());
        return {};
    }
    auto text = vfs.readText(uri);
    if (!text) return text.error();
    json j = json::parse(text.value(), nullptr, false);
    if (j.is_discarded()) return makeError(Error::Code::ParseError, uri + ": invalid JSON");
    auto map = InputMap::fromJson(j);
    if (!map) return makeError(map.error().code, uri + ": " + map.error().message);
    setMap(std::move(map.value()));
    return {};
}

Status InputActions::save(VirtualFileSystem& vfs, const std::string& uri) const { return vfs.writeText(uri, m_map.toJson().dump(2)); }

const ActionState& InputActions::state(std::string_view name) const {
    static const ActionState none;
    auto it = m_index.find(std::string(name));
    return it == m_index.end() ? none : m_states[it->second];
}

void InputActions::update(const Input& input, math::Vec2 screen) {
    for (usize ai = 0; ai < m_map.actions.size(); ++ai) {
        const InputAction& a = m_map.actions[ai];
        auto& sticks = m_sticks[ai];
        float sum = 0.0f;
        float strongest = 0.0f;  // buttons: largest analog contribution
        bool digital = false;
        auto analog = [&](float v) {
            const float m = std::fabs(v);
            if (m <= a.deadzone) return 0.0f;
            return std::copysign((m - a.deadzone) / (1.0f - a.deadzone), v);  // rescaled to 0..1 beyond the dead zone
        };
        for (usize bi = 0; bi < a.bindings.size(); ++bi) {
            const InputBinding& b = a.bindings[bi];
            float v = 0.0f;
            switch (b.type) {
                case InputBinding::Type::Key: v = input.keyDown(b.key) ? 1.0f : 0.0f; break;
                case InputBinding::Type::MouseButton: v = input.mouseDown(b.mouse) ? 1.0f : 0.0f; break;
                case InputBinding::Type::GamepadButton:
                    v = (b.pad < 0 ? input.anyGamepadDown(b.button) : input.gamepadDown(static_cast<u32>(b.pad), b.button)) ? 1.0f : 0.0f;
                    break;
                case InputBinding::Type::GamepadAxis: {
                    float raw = 0.0f;
                    if (b.pad >= 0) raw = input.gamepadAxis(static_cast<u32>(b.pad), b.axis);
                    else
                        for (u32 p = 0; p < kMaxGamepads; ++p)
                            if (std::fabs(input.gamepadAxis(p, b.axis)) > std::fabs(raw)) raw = input.gamepadAxis(p, b.axis);
                    v = analog(raw);
                    break;
                }
                case InputBinding::Type::TouchZone:  // touches that started in the zone (a finger sliding in does not count)
                    for (const Touch& t : input.touches())
                        if (t.phase != TouchPhase::Ended && insideZone(t.start, screen, b.zone)) v = 1.0f;
                    break;
                case InputBinding::Type::TouchStick: {
                    Stick& st = sticks[bi];
                    const Touch* t = st.active ? input.findTouch(st.touch) : nullptr;
                    if (!t || t->phase == TouchPhase::Ended) {
                        st.active = false;
                        for (const Touch& c : input.touches())  // a new touch starting in the zone takes the stick
                            if (c.phase == TouchPhase::Began && insideZone(c.position, screen, b.zone)) {
                                st = Stick{c.id, c.position, true};
                                t = &c;
                                break;
                            }
                    }
                    if (st.active && t && screen.y > 0.0f) {
                        const math::Vec2 d = (t->position - st.origin) / (b.radius * screen.y);
                        v = analog(std::clamp(b.stickAxis ? -d.y : d.x, -1.0f, 1.0f));  // up positive
                    }
                    break;
                }
            }
            v *= b.scale;
            sum += v;
            if (std::fabs(v) > std::fabs(strongest)) strongest = v;
            digital |= (b.type == InputBinding::Type::Key || b.type == InputBinding::Type::MouseButton ||
                        b.type == InputBinding::Type::GamepadButton || b.type == InputBinding::Type::TouchZone) && v != 0.0f;
        }
        ActionState& s = m_states[ai];
        const bool wasDown = s.down;
        if (a.kind == InputAction::Kind::Axis) {
            s.value = std::clamp(sum, -1.0f, 1.0f);
            s.down = std::fabs(s.value) >= 0.5f;
        } else {
            s.value = digital ? 1.0f : std::clamp(std::fabs(strongest), 0.0f, 1.0f);
            s.down = digital || s.value >= 0.5f;
        }
        s.pressed = s.down && !wasDown;
        s.released = !s.down && wasDown;
    }
}

} // namespace agr
