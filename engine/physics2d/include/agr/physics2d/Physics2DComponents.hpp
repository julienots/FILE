#pragma once
#include "agr/math/Math.hpp"
#include "agr/scene/Scene.hpp"

namespace agr {

enum class BodyType2D : i32 {
    Static = 0,     ///< Never moves (ground, walls).
    Kinematic = 1,  ///< Moved by its transform or its velocity, pushes dynamic bodies, ignores forces.
    Dynamic = 2,    ///< Moved by the simulation (gravity, forces, collisions).
};

/// Makes the entity a 2D physics body (XY plane, rotation about Z). Its colliders
/// are the Collider2D of the entity and of its descendants without their own body.
struct RigidBody2DComponent {
    BodyType2D type = BodyType2D::Dynamic;
    float gravityScale = 1.0f;
    float linearDamping = 0.0f;
    float angularDamping = 0.05f;
    bool fixedRotation = false;  ///< Characters: never tips over.
    bool bullet = false;         ///< Continuous collision against dynamic bodies (fast projectiles).
    math::Vec2 initialVelocity{0.0f, 0.0f};
};

enum class ColliderShape2D : i32 { Box = 0, Circle = 1, Capsule = 2 };

/// Collision shape in the entity's XY plane, scaled by the entity's world scale.
/// Without a RigidBody2D on the entity or an ancestor, it is a static collider.
struct Collider2DComponent {
    ColliderShape2D shape = ColliderShape2D::Box;
    math::Vec2 size{1.0f, 1.0f};    ///< Box; Capsule: width x height (vertical capsule).
    float radius = 0.5f;            ///< Circle.
    math::Vec2 offset{0.0f, 0.0f};
    float density = 1.0f;           ///< kg / m^2: with the area, gives the mass.
    float friction = 0.6f;
    float restitution = 0.0f;       ///< Bounciness, 0..1.
    bool isTrigger = false;         ///< Detects overlaps (events) without colliding.
};

/// Static collision for every painted cell of the Tilemap on the same entity
/// (cells merged into horizontal runs).
struct TilemapCollider2DComponent {
    float friction = 0.6f;
    float restitution = 0.0f;
};

/// Registers the reflection data (idempotent).
void registerPhysics2DComponents();

} // namespace agr
