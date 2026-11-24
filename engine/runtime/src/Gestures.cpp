#include "agr/runtime/Gestures.hpp"

#include <algorithm>
#include <cmath>

namespace agr {

const char* enumName(GestureType type) {
    switch (type) {
        case GestureType::Tap: return "tap";
        case GestureType::DoubleTap: return "doubleTap";
        case GestureType::LongPress: return "longPress";
        case GestureType::DragBegin: return "dragBegin";
        case GestureType::Drag: return "drag";
        case GestureType::DragEnd: return "dragEnd";
        case GestureType::Swipe: return "swipe";
        case GestureType::Pinch: return "pinch";
    }
    return "tap";
}

void GestureRecognizer::emit(GestureType t, math::Vec2 p, math::Vec2 start, math::Vec2 delta, float scale, math::Vec2 velocity) {
    m_events.push_back({t, p, start, delta, scale, 0.0f, velocity});
}

void GestureRecognizer::reset() {
    m_events.clear();
    m_pointers.clear();
    m_order.clear();
    m_mode = Mode::None;
    m_longPressed = false;
}

void GestureRecognizer::update(const Input& input, f64 dt) {
    m_events.clear();
    m_time += dt;
    // Current pointers: fingers, or the mouse's left button as one finger.
    struct Now {
        u64 id;
        math::Vec2 pos;
        bool down;
    };
    std::vector<Now> now;
    if (!input.touches().empty()) {
        for (const Touch& t : input.touches()) now.push_back({t.id + 1, t.position, t.phase != TouchPhase::Ended});
    } else if (input.mouseDown(MouseButton::Left) || input.mouseReleased(MouseButton::Left)) {
        now.push_back({0, input.mousePosition(), input.mouseDown(MouseButton::Left)});
    }
    // Desktop zoom: the wheel is a pinch around the mouse.
    if (input.wheel().y != 0.0f && input.touches().empty())
        emit(GestureType::Pinch, input.mousePosition(), input.mousePosition(), {0.0f, 0.0f}, std::pow(settings.wheelZoomStep, input.wheel().y));

    // Ups and downs.
    for (const Now& n : now) {
        auto it = m_pointers.find(n.id);
        if (n.down && it == m_pointers.end()) {
            m_pointers[n.id] = {n.pos, n.pos, m_time};
            m_order.push_back(n.id);
            if (m_order.size() == 1) {
                m_mode = Mode::Press;
                m_longPressed = false;
                m_velocity = {0.0f, 0.0f};
            }
        }
    }
    // Two fingers: pinch (a drag in progress ends).
    if (m_order.size() >= 2 && m_mode != Mode::Pinch) {
        if (m_mode == Mode::Drag) {
            const Pointer& p = m_pointers[m_order[0]];
            emit(GestureType::DragEnd, p.position, p.start);
        }
        m_mode = Mode::Pinch;
        const math::Vec2 a = m_pointers[m_order[0]].position, b = m_pointers[m_order[1]].position;
        m_pinchCenter = (a + b) * 0.5f;
        m_pinchDistance = std::max(1.0f, math::length(b - a));
        m_pinchAngle = std::atan2(b.y - a.y, b.x - a.x);
    }
    // Moves.
    for (const Now& n : now) {
        auto it = m_pointers.find(n.id);
        if (it == m_pointers.end()) continue;
        const math::Vec2 previous = it->second.position;
        it->second.position = n.pos;
        if (m_mode == Mode::Press || m_mode == Mode::Drag) {
            if (m_order.empty() || n.id != m_order[0]) continue;
            const math::Vec2 d = n.pos - previous;
            if (dt > 0.0) m_velocity = m_velocity * 0.5f + (d / static_cast<float>(dt)) * 0.5f;
            if (m_mode == Mode::Press && math::length(n.pos - it->second.start) > settings.tapMaxMove) {
                m_mode = Mode::Drag;
                emit(GestureType::DragBegin, n.pos, it->second.start, n.pos - it->second.start);
            } else if (m_mode == Mode::Drag && (d.x != 0.0f || d.y != 0.0f)) {
                emit(GestureType::Drag, n.pos, it->second.start, d);
            }
        }
    }
    if (m_mode == Mode::Pinch && m_order.size() >= 2) {
        const math::Vec2 a = m_pointers[m_order[0]].position, b = m_pointers[m_order[1]].position;
        const math::Vec2 center = (a + b) * 0.5f;
        const float dist = std::max(1.0f, math::length(b - a));
        const float scale = dist / m_pinchDistance;
        const math::Vec2 pan = center - m_pinchCenter;
        const float angle = std::atan2(b.y - a.y, b.x - a.x);
        float twist = angle - m_pinchAngle;
        constexpr float kPi = 3.14159265358979f;
        if (twist > kPi) twist -= 2.0f * kPi;
        if (twist < -kPi) twist += 2.0f * kPi;
        if (std::abs(scale - 1.0f) > 1e-4f || pan.x != 0.0f || pan.y != 0.0f || std::abs(twist) > 1e-4f) {
            emit(GestureType::Pinch, center, center - pan, pan, scale);
            m_events.back().rotation = twist;
        }
        m_pinchCenter = center;
        m_pinchDistance = dist;
        m_pinchAngle = angle;
    }
    // Long press: still and held.
    if (m_mode == Mode::Press && !m_longPressed && m_order.size() == 1) {
        const Pointer& p = m_pointers[m_order[0]];
        if (m_time - p.time >= settings.longPressTime) {
            m_longPressed = true;
            emit(GestureType::LongPress, p.position, p.start);
        }
    }
    // Releases (and pointers that vanished).
    std::vector<u64> released;
    for (u64 id : m_order) {
        const auto n = std::find_if(now.begin(), now.end(), [&](const Now& x) { return x.id == id; });
        if (n == now.end() || !n->down) released.push_back(id);
    }
    for (u64 id : released) {
        const Pointer p = m_pointers[id];
        const bool primary = !m_order.empty() && m_order[0] == id;
        if (primary && m_mode == Mode::Press && !m_longPressed && m_time - p.time <= settings.tapMaxTime) {
            const bool doubleTap = m_time - m_lastTapTime <= settings.doubleTapTime && math::length(p.position - m_lastTapPos) <= settings.doubleTapDistance;
            emit(doubleTap ? GestureType::DoubleTap : GestureType::Tap, p.position, p.start);
            m_lastTapTime = doubleTap ? -10.0 : m_time;
            m_lastTapPos = p.position;
        } else if (primary && m_mode == Mode::Drag) {
            emit(GestureType::DragEnd, p.position, p.start, {0.0f, 0.0f}, 1.0f, m_velocity);
            if (math::length(m_velocity) >= settings.swipeMinSpeed) emit(GestureType::Swipe, p.position, p.start, {0.0f, 0.0f}, 1.0f, m_velocity);
        }
        m_pointers.erase(id);
        m_order.erase(std::remove(m_order.begin(), m_order.end(), id), m_order.end());
    }
    if (m_order.empty()) m_mode = Mode::None;
    else if (m_mode == Mode::Pinch && m_order.size() < 2) m_mode = Mode::None;  // lifting one finger ends the pinch (no tap, no drag)
}

math::Vec2 GestureRecognizer::dragDelta() const {
    math::Vec2 d{0.0f, 0.0f};
    for (const GestureEvent& e : m_events)
        if (e.type == GestureType::Drag || e.type == GestureType::DragBegin) d += e.delta;
    return d;
}

float GestureRecognizer::pinchScale() const {
    float s = 1.0f;
    for (const GestureEvent& e : m_events)
        if (e.type == GestureType::Pinch) s *= e.scale;
    return s;
}

float GestureRecognizer::pinchRotation() const {
    float r = 0.0f;
    for (const GestureEvent& e : m_events)
        if (e.type == GestureType::Pinch) r += e.rotation;
    return r;
}

math::Vec2 GestureRecognizer::pinchPan() const {
    math::Vec2 d{0.0f, 0.0f};
    for (const GestureEvent& e : m_events)
        if (e.type == GestureType::Pinch) d += e.delta;
    return d;
}

} // namespace agr
