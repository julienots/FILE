#pragma once
#include <array>
#include <span>
#include <string>
#include <vector>

#include "agr/core/Types.hpp"
#include "agr/math/Math.hpp"

namespace agr {

/// Physical key codes (USB HID usage ids, layout independent).
enum class Key : u16 {
    Unknown = 0,
    A = 4, B, C, D, E, F, G, H, I, J, K, L, M, N, O, P, Q, R, S, T, U, V, W, X, Y, Z,
    Num1 = 30, Num2, Num3, Num4, Num5, Num6, Num7, Num8, Num9, Num0,
    Return = 40, Escape = 41, Backspace = 42, Tab = 43, Space = 44, Minus = 45, Equals = 46,
    F1 = 58, F2, F3, F4, F5, F6, F7, F8, F9, F10, F11, F12,
    Insert = 73, Home = 74, PageUp = 75, Delete = 76, End = 77, PageDown = 78,
    Right = 79, Left = 80, Down = 81, Up = 82,
    LeftCtrl = 224, LeftShift = 225, LeftAlt = 226, LeftSuper = 227,
    RightCtrl = 228, RightShift = 229, RightAlt = 230, RightSuper = 231,
    AndroidBack = 282,  // SDL_SCANCODE_AC_BACK (Android back button)
    Count = 512
};

enum class MouseButton : u8 { Left = 0, Middle, Right, X1, X2, Count };

enum class TouchPhase : u8 { Began, Moved, Stationary, Ended };

struct Touch {
    u64 id = 0;
    math::Vec2 position;  ///< Window pixels.
    math::Vec2 delta;     ///< Movement since last frame, pixels.
    float pressure = 1.0f;
    TouchPhase phase = TouchPhase::Began;
};

/// Frame-coherent input state. Filled by the platform layer, read by gameplay.
class Input {
public:
    // --------------------------------------------------------- queries
    bool keyDown(Key k) const { return m_keys[idx(k)]; }
    bool keyPressed(Key k) const { return m_keys[idx(k)] && !m_prevKeys[idx(k)]; }
    bool keyReleased(Key k) const { return !m_keys[idx(k)] && m_prevKeys[idx(k)]; }

    bool mouseDown(MouseButton b) const { return m_mouse[static_cast<usize>(b)]; }
    bool mousePressed(MouseButton b) const { return m_mouse[static_cast<usize>(b)] && !m_prevMouse[static_cast<usize>(b)]; }
    bool mouseReleased(MouseButton b) const { return !m_mouse[static_cast<usize>(b)] && m_prevMouse[static_cast<usize>(b)]; }
    math::Vec2 mousePosition() const { return m_mousePos; }
    math::Vec2 mouseDelta() const { return m_mouseDelta; }
    math::Vec2 wheel() const { return m_wheel; }

    std::span<const Touch> touches() const { return m_touches; }
    const Touch* findTouch(u64 id) const;
    const std::string& textInput() const { return m_text; }

    // ----------------------------------------------- platform interface
    /// Starts a new frame: snapshots previous state, clears deltas, drops ended touches.
    void beginFrame();
    void setKey(Key k, bool down);
    void setMouseButton(MouseButton b, bool down);
    void setMousePosition(math::Vec2 p);
    void addWheel(math::Vec2 w) { m_wheel += w; }
    void touchDown(u64 id, math::Vec2 position, float pressure);
    void touchMove(u64 id, math::Vec2 position, float pressure);
    void touchUp(u64 id, math::Vec2 position);
    void appendText(std::string_view utf8) { m_text += utf8; }
    /// Releases everything (focus lost, app backgrounded).
    void reset();

private:
    static usize idx(Key k) { return static_cast<usize>(k) < static_cast<usize>(Key::Count) ? static_cast<usize>(k) : 0; }

    std::array<bool, static_cast<usize>(Key::Count)> m_keys{};
    std::array<bool, static_cast<usize>(Key::Count)> m_prevKeys{};
    std::array<bool, static_cast<usize>(MouseButton::Count)> m_mouse{};
    std::array<bool, static_cast<usize>(MouseButton::Count)> m_prevMouse{};
    math::Vec2 m_mousePos, m_mouseDelta, m_wheel;
    bool m_hasMousePos = false;
    std::vector<Touch> m_touches;
    std::string m_text;
};

} // namespace agr
