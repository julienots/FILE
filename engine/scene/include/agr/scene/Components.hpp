#pragma once
// Built-in components owned by the scene module. Feature modules (render,
// physics, audio...) register their own components through ComponentRegistry.
#include <string>

#include "agr/core/Uuid.hpp"
#include "agr/ecs/Entity.hpp"
#include "agr/math/Math.hpp"

namespace agr {

/// Persistent identity (survives save / load, used for references).
struct IdComponent {
    Uuid id;
};

struct NameComponent {
    std::string name = "Entity";
};

/// Activation and editor state of an entity.
struct StateComponent {
    bool active = true;
    u32 layer = 0;           ///< 0..31, used by cameras, physics and raycasts.
    bool hiddenInEditor = false;
    bool lockedInEditor = false;
};

/// Local transform relative to the parent.
struct TransformComponent {
    math::Vec3 position{0.0f};
    math::Quat rotation{};
    math::Vec3 scale{1.0f};

    math::Mat4 localMatrix() const { return math::Mat4::trs(position, rotation, scale); }
};

/// World matrix computed by Scene::updateTransforms() (runtime only).
struct WorldTransformComponent {
    math::Mat4 matrix{};
    /// Entity and all its ancestors active, as of the last updateTransforms().
    bool activeInHierarchy = true;
};

/// Intrusive tree links. Managed exclusively by Scene; never edit directly.
struct HierarchyComponent {
    ecs::Entity parent{};
    ecs::Entity firstChild{};
    ecs::Entity lastChild{};
    ecs::Entity prevSibling{};
    ecs::Entity nextSibling{};
    u32 childCount = 0;
};

/// Registers reflection data of the built-in components (idempotent).
void registerSceneComponents();

} // namespace agr
