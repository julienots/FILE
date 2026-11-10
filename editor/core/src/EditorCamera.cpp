#include "agr/editor/EditorCamera.hpp"

#include <algorithm>
#include <cmath>

namespace agr::editor {

using math::Quat;
using math::Vec3;

namespace {
Quat orientation(float yaw, float pitch) {
    return Quat::fromAxisAngle({0, 1, 0}, yaw) * Quat::fromAxisAngle({1, 0, 0}, pitch);
}
constexpr float kMaxPitch = math::radians(89.0f);
constexpr float kZoomStep = 0.85f;
} // namespace

Vec3 EditorCamera::forward() const { return orientation(yaw, pitch) * Vec3{0, 0, -1}; }
Vec3 EditorCamera::right() const { return orientation(yaw, pitch) * Vec3{1, 0, 0}; }
Vec3 EditorCamera::up() const { return orientation(yaw, pitch) * Vec3{0, 1, 0}; }

Vec3 EditorCamera::position() const {
    if (mode2D) return {position2D.x, position2D.y, 50.0f};
    return target - forward() * distance;
}

void EditorCamera::orbit(float deltaYaw, float deltaPitch) {
    yaw += deltaYaw;
    pitch = std::clamp(pitch + deltaPitch, -kMaxPitch, kMaxPitch);
}

void EditorCamera::look(float deltaYaw, float deltaPitch) {
    const Vec3 eye = position();
    orbit(deltaYaw, deltaPitch);
    target = eye + forward() * distance;  // rotate in place
}

void EditorCamera::move(Vec3 local, float units) {
    if (mode2D) {
        position2D.x += local.x * units;
        position2D.y += local.y * units;
        return;
    }
    target += (right() * local.x + up() * local.y + forward() * local.z) * units;
}

void EditorCamera::pan(math::Vec2 px, float viewportHeight) {
    viewportHeight = std::max(viewportHeight, 1.0f);
    if (mode2D) {
        const float k = 2.0f * orthoSize / viewportHeight;
        position2D.x -= px.x * k;
        position2D.y += px.y * k;
        return;
    }
    const float k = 2.0f * distance * std::tan(fieldOfView * 0.5f) / viewportHeight;
    target += right() * (-px.x * k) + up() * (px.y * k);
}

void EditorCamera::zoom(float steps) {
    const float f = std::pow(kZoomStep, steps);
    if (mode2D) orthoSize = std::clamp(orthoSize * f, 0.05f, 10000.0f);
    else distance = std::clamp(distance * f, 0.1f, 5000.0f);
}

void EditorCamera::focus(const math::Aabb& box) {
    if (!box.valid()) return;
    const Vec3 c = box.center();
    const float radius = std::max(math::length(box.extents()), 0.5f);
    if (mode2D) {
        position2D = {c.x, c.y};
        orthoSize = std::max(box.extents().x, box.extents().y) * 1.5f + 0.5f;
        return;
    }
    target = c;
    distance = radius / std::sin(fieldOfView * 0.5f) * 1.2f;
}

RenderView EditorCamera::renderView() const {
    RenderView v;
    v.editorView = true;
    v.position = position();
    if (mode2D) {
        v.view = math::Mat4::lookAt(v.position, {position2D.x, position2D.y, 0.0f}, Vec3::up());
        v.projection = Projection::Orthographic;
        v.orthographicSize = orthoSize;
        v.nearPlane = 0.1f;
        v.farPlane = 1000.0f;
    } else {
        v.view = math::Mat4::lookAt(v.position, target, Vec3::up());
        v.fieldOfView = fieldOfView;
        v.nearPlane = std::max(0.01f, distance * 0.002f);
        v.farPlane = std::max(2000.0f, distance * 20.0f);
    }
    return v;
}

math::Ray EditorCamera::ray(math::Vec2 uv, float aspect) const {
    const float nx = uv.x * 2.0f - 1.0f, ny = 1.0f - uv.y * 2.0f;
    if (mode2D) {
        return {{position2D.x + nx * orthoSize * aspect, position2D.y + ny * orthoSize, 50.0f}, {0.0f, 0.0f, -1.0f}};
    }
    const float t = std::tan(fieldOfView * 0.5f);
    const Vec3 dir = math::normalize(forward() + right() * (nx * t * aspect) + up() * (ny * t));
    return {position(), dir};
}

} // namespace agr::editor
