#pragma once
#include <optional>

#include "agr/math/Geometry.hpp"
#include "agr/scene/Scene.hpp"

namespace agr {
class SpriteSheetLibrary;
class ModelLibrary;
}

namespace agr::editor {

struct PickHit {
    ecs::Entity entity;
    float distance;
    math::Vec3 point;
};

/// Libraries giving the exact size of sprites using sheets and of imported meshes.
struct PickAssets {
    SpriteSheetLibrary* sheets = nullptr;
    ModelLibrary* models = nullptr;  ///< Without it (or while loading), model meshes use a unit box.
};

/// Nearest visible renderable (mesh, sprite or tilemap) hit by the ray. Meshes use
/// their world bounding box, sprites their exact quad, tilemaps their painted cells.
/// Entities hidden in the editor, locked or inactive are ignored. With sheets,
/// sprites using a sheet frame are placed with the frame pivot, as the renderer does.
std::optional<PickHit> pick(Scene& scene, const math::Ray& ray, PickAssets assets = {});

/// World-space bounds of an entity's renderable (a unit box at its position when none).
math::Aabb worldBounds(Scene& scene, ecs::Entity e, PickAssets assets = {});

/// Intersection of a ray with the plane dot(n, p) = d. Negative when parallel or behind.
float intersectPlane(const math::Ray& ray, math::Vec3 normal, float d);

} // namespace agr::editor
