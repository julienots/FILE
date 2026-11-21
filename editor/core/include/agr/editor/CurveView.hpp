#pragma once
// Geometry of the animation curve editor: time / value <-> pixels and Bézier tangent handles.
#include <algorithm>
#include <cmath>

#include "agr/animation/AnimationClip.hpp"
#include "agr/math/Math.hpp"

namespace agr::editor {

struct CurveView {
    math::Vec2 origin{};  ///< Top-left corner (pixels).
    math::Vec2 size{1, 1};
    float duration = 1.0f;         ///< Time shown across the width.
    float lo = 0.0f, hi = 1.0f;    ///< Values at the bottom / top.

    math::Vec2 toScreen(float t, float v) const {
        return {origin.x + t / std::max(duration, 1e-3f) * size.x, origin.y + size.y - (v - lo) / std::max(hi - lo, 1e-6f) * size.y};
    }
    float timeAt(float x) const { return (x - origin.x) / std::max(size.x, 1.0f) * duration; }
    float valueAt(float y) const { return lo + (origin.y + size.y - y) / std::max(size.y, 1.0f) * (hi - lo); }
    /// Pixels per second / per value unit.
    float sx() const { return size.x / std::max(duration, 1e-3f); }
    float sy() const { return size.y / std::max(hi - lo, 1e-6f); }

    /// Handle of a key's tangent, `length` pixels from the key (side -1 = in, +1 = out).
    math::Vec2 handle(const Keyframe& k, float slope, int side, float length = 40.0f) const {
        const math::Vec2 key = toScreen(k.time, k.value);
        float dx = sx(), dy = -slope * sy();  // screen direction of one second along the tangent
        const float n = std::sqrt(dx * dx + dy * dy);
        if (n < 1e-6f) return key;
        dx /= n, dy /= n;
        return {key.x + side * dx * length, key.y + side * dy * length};
    }
    /// Slope (value per second) of the tangent pointing from the key to a dragged handle. A
    /// handle dragged past the key's time keeps a steep slope instead of flipping.
    float slopeFromHandle(const Keyframe& k, math::Vec2 handlePos, int side) const {
        const math::Vec2 key = toScreen(k.time, k.value);
        float dt = (handlePos.x - key.x) / sx() * static_cast<float>(side);
        const float dv = -(handlePos.y - key.y) / sy() * static_cast<float>(side);
        dt = std::max(dt, 1e-3f);
        return std::clamp(dv / dt, -1e5f, 1e5f);
    }
};

} // namespace agr::editor
