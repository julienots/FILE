#include "agr/runtime/Input.hpp"

#include <algorithm>
#include <cctype>
#include <iterator>
#include <string>

namespace agr {

const Touch* Input::findTouch(u64 id) const {
    for (const auto& t : m_touches)
        if (t.id == id) return &t;
    return nullptr;
}

void Input::beginFrame() {
    m_prevKeys = m_keys;
    m_prevMouse = m_mouse;
    for (Pad& p : m_pads) p.prev = p.buttons;
    m_mouseDelta = {};
    m_wheel = {};
    m_text.clear();
    std::erase_if(m_touches, [](const Touch& t) { return t.phase == TouchPhase::Ended; });
    for (auto& t : m_touches) {
        t.phase = TouchPhase::Stationary;
        t.delta = {};
    }
}

void Input::setKey(Key k, bool down) { m_keys[idx(k)] = down; }
void Input::setMouseButton(MouseButton b, bool down) {
    if (b < MouseButton::Count) m_mouse[static_cast<usize>(b)] = down;
}

void Input::setMousePosition(math::Vec2 p) {
    if (m_hasMousePos) m_mouseDelta += p - m_mousePos;
    m_mousePos = p;
    m_hasMousePos = true;
}

void Input::touchDown(u64 id, math::Vec2 position, float pressure) {
    std::erase_if(m_touches, [id](const Touch& t) { return t.id == id; });
    m_touches.push_back(Touch{id, position, {}, pressure, TouchPhase::Began, position});
}

void Input::touchMove(u64 id, math::Vec2 position, float pressure) {
    for (auto& t : m_touches) {
        if (t.id != id) continue;
        t.delta += position - t.position;
        t.position = position;
        t.pressure = pressure;
        if (t.phase != TouchPhase::Began) t.phase = TouchPhase::Moved;
        return;
    }
    touchDown(id, position, pressure);
}

void Input::touchUp(u64 id, math::Vec2 position) {
    for (auto& t : m_touches) {
        if (t.id != id) continue;
        t.delta += position - t.position;
        t.position = position;
        t.phase = TouchPhase::Ended;
    }
}

void Input::reset() {
    m_keys.fill(false);
    m_mouse.fill(false);
    for (Pad& p : m_pads) {
        p.buttons.fill(false);
        p.axes.fill(0.0f);
    }
    m_touches.clear();
    m_text.clear();
}

// ------------------------------------------------------------------ gamepads
Input::Pad* Input::padFor(u64 deviceId) {
    for (Pad& p : m_pads)
        if (p.connected && p.device == deviceId) return &p;
    return nullptr;
}

u32 Input::gamepadConnect(u64 deviceId, std::string name) {
    if (Pad* p = padFor(deviceId)) return static_cast<u32>(p - m_pads.data());
    for (u32 i = 0; i < kMaxGamepads; ++i)
        if (!m_pads[i].connected) {
            m_pads[i] = Pad{};
            m_pads[i].connected = true;
            m_pads[i].device = deviceId;
            m_pads[i].name = std::move(name);
            return i;
        }
    return kMaxGamepads;
}

void Input::gamepadDisconnect(u64 deviceId) {
    if (Pad* p = padFor(deviceId)) *p = Pad{};
}

void Input::setGamepadButton(u64 deviceId, GamepadButton b, bool down) {
    if (Pad* p = padFor(deviceId); p && b < GamepadButton::Count) p->buttons[static_cast<usize>(b)] = down;
}

void Input::setGamepadAxis(u64 deviceId, GamepadAxis a, float value) {
    if (Pad* p = padFor(deviceId); p && a < GamepadAxis::Count) p->axes[static_cast<usize>(a)] = std::clamp(value, -1.0f, 1.0f);
}

u32 Input::gamepadCount() const {
    u32 n = 0;
    for (const Pad& p : m_pads) n += p.connected ? 1u : 0u;
    return n;
}

bool Input::gamepadDown(u32 pad, GamepadButton b) const {
    return gamepadConnected(pad) && b < GamepadButton::Count && m_pads[pad].buttons[static_cast<usize>(b)];
}
bool Input::gamepadPressed(u32 pad, GamepadButton b) const {
    return gamepadDown(pad, b) && !m_pads[pad].prev[static_cast<usize>(b)];
}
bool Input::gamepadReleased(u32 pad, GamepadButton b) const {
    return gamepadConnected(pad) && b < GamepadButton::Count && !m_pads[pad].buttons[static_cast<usize>(b)] && m_pads[pad].prev[static_cast<usize>(b)];
}
float Input::gamepadAxis(u32 pad, GamepadAxis a) const {
    return gamepadConnected(pad) && a < GamepadAxis::Count ? m_pads[pad].axes[static_cast<usize>(a)] : 0.0f;
}
bool Input::anyGamepadDown(GamepadButton b) const {
    for (u32 i = 0; i < kMaxGamepads; ++i)
        if (gamepadDown(i, b)) return true;
    return false;
}
bool Input::anyGamepadPressed(GamepadButton b) const {
    for (u32 i = 0; i < kMaxGamepads; ++i)
        if (gamepadPressed(i, b)) return true;
    return false;
}
const std::string& Input::gamepadName(u32 pad) const {
    static const std::string none;
    return gamepadConnected(pad) ? m_pads[pad].name : none;
}

// ------------------------------------------------------------------ names
namespace {
constexpr const char* kButtonNames[] = {"South", "East", "West", "North", "Back", "Guide", "Start", "LeftStick", "RightStick",
                                        "LeftShoulder", "RightShoulder", "DpadUp", "DpadDown", "DpadLeft", "DpadRight"};
constexpr const char* kAxisNames[] = {"LeftX", "LeftY", "RightX", "RightY", "LeftTrigger", "RightTrigger"};
struct NamedKey {
    const char* name;
    Key key;
};
constexpr NamedKey kNamedKeys[] = {
    {"Space", Key::Space}, {"Return", Key::Return}, {"Enter", Key::Return}, {"Escape", Key::Escape}, {"Backspace", Key::Backspace},
    {"Tab", Key::Tab}, {"Left", Key::Left}, {"Right", Key::Right}, {"Up", Key::Up}, {"Down", Key::Down},
    {"LeftShift", Key::LeftShift}, {"RightShift", Key::RightShift}, {"LeftCtrl", Key::LeftCtrl}, {"RightCtrl", Key::RightCtrl},
    {"LeftAlt", Key::LeftAlt}, {"RightAlt", Key::RightAlt}, {"Minus", Key::Minus}, {"Equals", Key::Equals}, {"Insert", Key::Insert},
    {"Delete", Key::Delete}, {"Home", Key::Home}, {"End", Key::End}, {"PageUp", Key::PageUp}, {"PageDown", Key::PageDown},
    {"Back", Key::AndroidBack}};
bool iequals(std::string_view a, std::string_view b) {
    if (a.size() != b.size()) return false;
    for (usize i = 0; i < a.size(); ++i)
        if (std::tolower(static_cast<unsigned char>(a[i])) != std::tolower(static_cast<unsigned char>(b[i]))) return false;
    return true;
}
} // namespace

const char* enumName(GamepadButton b) { return b < GamepadButton::Count ? kButtonNames[static_cast<usize>(b)] : ""; }
const char* enumName(GamepadAxis a) { return a < GamepadAxis::Count ? kAxisNames[static_cast<usize>(a)] : ""; }
GamepadButton parseGamepadButton(std::string_view name) {
    for (usize i = 0; i < std::size(kButtonNames); ++i)
        if (iequals(name, kButtonNames[i])) return static_cast<GamepadButton>(i);
    return GamepadButton::Count;
}
GamepadAxis parseGamepadAxis(std::string_view name) {
    for (usize i = 0; i < std::size(kAxisNames); ++i)
        if (iequals(name, kAxisNames[i])) return static_cast<GamepadAxis>(i);
    return GamepadAxis::Count;
}

const char* keyName(Key k) {
    static const auto letters = [] {
        std::array<std::string, 128> names{};
        for (int i = 0; i < 26; ++i) names[static_cast<usize>(Key::A) + static_cast<usize>(i)] = std::string(1, static_cast<char>('A' + i));
        for (int i = 0; i < 9; ++i) names[static_cast<usize>(Key::Num1) + static_cast<usize>(i)] = std::string(1, static_cast<char>('1' + i));
        names[static_cast<usize>(Key::Num0)] = "0";
        for (int i = 0; i < 12; ++i) names[static_cast<usize>(Key::F1) + static_cast<usize>(i)] = "F" + std::to_string(i + 1);
        return names;
    }();
    const usize i = static_cast<usize>(k);
    if (i < letters.size() && !letters[i].empty()) return letters[i].c_str();
    for (const NamedKey& n : kNamedKeys)
        if (n.key == k && std::string_view(n.name) != "Enter") return n.name;
    return "";
}

Key parseKey(std::string_view n) {
    if (n.size() == 1) {
        const char c = static_cast<char>(std::toupper(static_cast<unsigned char>(n[0])));
        if (c >= 'A' && c <= 'Z') return static_cast<Key>(static_cast<int>(Key::A) + (c - 'A'));
        if (c >= '1' && c <= '9') return static_cast<Key>(static_cast<int>(Key::Num1) + (c - '1'));
        if (c == '0') return Key::Num0;
    }
    for (const NamedKey& k : kNamedKeys)
        if (iequals(n, k.name)) return k.key;
    if (n.size() >= 2 && (n[0] == 'F' || n[0] == 'f')) {
        int f = 0;
        for (usize i = 1; i < n.size(); ++i) {
            if (n[i] < '0' || n[i] > '9') return Key::Unknown;
            f = f * 10 + (n[i] - '0');
        }
        if (f >= 1 && f <= 12) return static_cast<Key>(static_cast<int>(Key::F1) + f - 1);
    }
    return Key::Unknown;
}

} // namespace agr
