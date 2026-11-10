#pragma once
#include "agr/math/Geometry.hpp"
#include "agr/render/Renderer.hpp"

namespace agr::editor {

/// Free camera of the scene viewport (never saved in the scene).
///  3D: orbit around a target, pan, dolly, fly (yaw / pitch + move).
///  2D: orthographic view looking down -Z, pan and zoom.
class EditorCamera {
public:
    bool mode2D = false;
    math::Vec3 target{0.0f, 0.5f, 0.0f};
    float distance = 10.0f;
    float yaw = math::radians(30.0f);     ///< Around +Y.
    float pitch = math::radians(-25.0f);  ///< Around local X (negative looks down).
    float fieldOfView = math::radians(60.0f);
    float orthoSize = 6.0f;               ///< 2D half height.
    math::Vec2 position2D{0.0f, 0.0f};

    math::Vec3 forward() const;
    math::Vec3 right() const;
    math::Vec3 up() const;
    math::Vec3 position() const;

    void orbit(float deltaYaw, float deltaPitch);
    /// Pans by a screen-space delta in pixels for a viewport `viewportHeight` pixels tall.
    void pan(math::Vec2 pixels, float viewportHeight);
    void zoom(float wheelSteps);
    /// Fly mode: rotate in place and move in camera space (units / second).
    void look(float deltaYaw, float deltaPitch);
    void move(math::Vec3 localDirection, float distanceUnits);
    /// Frames a bounding box.
    void focus(const math::Aabb& box);

    RenderView renderView() const;
    /// World-space ray through a viewport point given in [0,1]² (origin top-left).
    math::Ray ray(math::Vec2 viewportUv, float aspect) const;
};

} // namespace agr::editor
