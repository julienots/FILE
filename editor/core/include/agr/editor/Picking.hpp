#pragma once
#include <optional>

#include "agr/math/Geometry.hpp"
#include "agr/scene/Scene.hpp"

namespace agr::editor {

struct PickHit {
    ecs::Entity entity;
    float distance;
    math::Vec3 point;
};

/// Nearest visible renderable (mesh or sprite) hit by the ray. Meshes use their
/// world bounding box, sprites their exact quad. Entities hidden in the editor,
/// locked or inactive are ignored.
std::optional<PickHit> pick(Scene& scene, const math::Ray& ray);

/// World-space bounds of an entity's renderable (empty box when none).
math::Aabb worldBounds(Scene& scene, ecs::Entity e);

/// Intersection of a ray with the plane dot(n, p) = d. Negative when parallel or behind.
float intersectPlane(const math::Ray& ray, math::Vec3 normal, float d);

} // namespace agr::editor
