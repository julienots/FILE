#pragma once
// Touch gestures for mobile games: tap, double tap, long press, one-finger drag (with its start,
// end and a swipe when released fast), two-finger pinch (scale) and pan. The mouse acts as one
// finger on desktop (left button), the wheel as a pinch. Updated once per frame by the Engine from
// Input; read by gameplay (C++ or agr.input.gestures / drag / pinch / tapped in Lua).
#include <span>
#include <unordered_map>
#include <vector>

#include "agr/math/Math.hpp"
#include "agr/runtime/Input.hpp"

namespace agr {

enum class GestureType : u8 { Tap, DoubleTap, LongPress, DragBegin, Drag, DragEnd, Swipe, Pinch };
const char* enumName(GestureType type);  ///< "tap", "doubleTap", "longPress", "dragBegin", "drag", "dragEnd", "swipe", "pinch"

struct GestureEvent {
    GestureType type = GestureType::Tap;
    math::Vec2 position;  ///< Window pixels (pinch: center between the fingers).
    math::Vec2 start;     ///< Where the gesture began.
    math::Vec2 delta;     ///< Drag: movement this frame; pinch: center movement this frame.
    float scale = 1.0f;   ///< Pinch: distance ratio this frame (> 1 = fingers apart = zoom in).
    math::Vec2 velocity;  ///< Swipe / DragEnd: pixels per second.
};

class GestureRecognizer {
public:
    struct Settings {
        float tapMaxMove = 14.0f;       ///< Pixels a finger may move and still tap.
        float tapMaxTime = 0.35f;       ///< Seconds.
        float doubleTapTime = 0.35f;    ///< Between two taps.
        float doubleTapDistance = 40.0f;
        float longPressTime = 0.5f;
        float swipeMinSpeed = 900.0f;   ///< Pixels per second at release.
        float wheelZoomStep = 1.1f;     ///< Pinch scale per wheel notch (desktop).
    };
    Settings settings;

    void update(const Input& input, f64 dt);
    std::span<const GestureEvent> events() const { return m_events; }
    /// Shortcuts over this frame's events.
    math::Vec2 dragDelta() const;           ///< One-finger drag movement this frame.
    float pinchScale() const;               ///< Product of this frame's pinch scales (1 = none).
    math::Vec2 pinchPan() const;            ///< Two-finger pan this frame.
    bool dragging() const { return m_mode == Mode::Drag; }
    bool pinching() const { return m_mode == Mode::Pinch; }
    void reset();

private:
    enum class Mode : u8 { None, Press, Drag, Pinch };
    struct Pointer {
        math::Vec2 position, start;
        f64 time = 0.0;
    };
    void emit(GestureType t, math::Vec2 p, math::Vec2 start, math::Vec2 delta = {0.0f, 0.0f}, float scale = 1.0f, math::Vec2 velocity = {0.0f, 0.0f});

    std::vector<GestureEvent> m_events;
    std::unordered_map<u64, Pointer> m_pointers;
    std::vector<u64> m_order;  // touch-down order
    Mode m_mode = Mode::None;
    f64 m_time = 0.0, m_lastTapTime = -10.0;
    math::Vec2 m_lastTapPos{-1e6f, -1e6f}, m_velocity{0.0f, 0.0f}, m_pinchCenter{0.0f, 0.0f};
    float m_pinchDistance = 0.0f;
    bool m_longPressed = false;
};

} // namespace agr
