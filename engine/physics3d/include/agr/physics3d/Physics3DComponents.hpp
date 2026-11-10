#pragma once
#include "agr/math/Math.hpp"
#include "agr/scene/Scene.hpp"

namespace agr {

enum class BodyType3D : i32 { Static = 0, Kinematic = 1, Dynamic = 2 };

/// Makes the entity a 3D physics body. Its colliders are the Collider of the entity
/// and of its descendants without their own body (compound shape).
struct RigidBodyComponent {
    BodyType3D type = BodyType3D::Dynamic;
    float mass = 0.0f;            ///< kg; 0 = computed from the colliders' density and volume.
    float linearDamping = 0.05f;
    float angularDamping = 0.05f;
    float gravityScale = 1.0f;
    bool freezeRotation = false;  ///< Characters: translation only.
    bool bullet = false;          ///< Continuous collision (fast projectiles).
    math::Vec3 initialVelocity{0.0f};
};

enum class ColliderShape3D : i32 { Box = 0, Sphere = 1, Capsule = 2 };

/// Collision shape, scaled by the entity's world scale. Without a RigidBody on the
/// entity or an ancestor, it is a static collider. Friction / restitution apply to
/// the whole body (the first collider's values); a body with a trigger collider is a sensor.
struct ColliderComponent {
    ColliderShape3D shape = ColliderShape3D::Box;
    math::Vec3 size{1.0f};        ///< Box.
    float radius = 0.5f;          ///< Sphere, capsule.
    float height = 2.0f;          ///< Capsule, total height along Y (including the caps).
    math::Vec3 offset{0.0f};
    float density = 1000.0f;      ///< kg / m^3 (water = 1000).
    float friction = 0.5f;
    float restitution = 0.0f;
    bool isTrigger = false;
};

void registerPhysics3DComponents();

} // namespace agr
