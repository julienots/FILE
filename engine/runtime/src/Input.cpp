#include "agr/runtime/Input.hpp"

#include <algorithm>

namespace agr {

const Touch* Input::findTouch(u64 id) const {
    for (const auto& t : m_touches)
        if (t.id == id) return &t;
    return nullptr;
}

void Input::beginFrame() {
    m_prevKeys = m_keys;
    m_prevMouse = m_mouse;
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
    m_touches.push_back(Touch{id, position, {}, pressure, TouchPhase::Began});
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
    m_touches.clear();
    m_text.clear();
}

} // namespace agr
