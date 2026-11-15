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
    i32 layer = 0;                  ///< Collision layer, 0..31 (names in the project settings); per body (first collider).
    u32 collidesWith = 0xFFFFFFFFu; ///< Layers it collides with (bit n = layer n). Two bodies touch when each one's mask has the other's layer.
};

enum class JointType3D : i32 {
    Fixed = 0,     ///< Welds the two bodies together.
    Hinge = 1,     ///< Rotation about `axis` through the anchor (doors, wheels); angle limits, motor.
    Distance = 2,  ///< Keeps the anchors between lowerLimit / upperLimit (rope) or at `length`, optionally springy.
    Slider = 3,    ///< Translation along `axis` only (elevators, drawers); limits, motor.
    Ball = 4,      ///< Free rotation about the anchor (chains, ragdoll shoulders).
};

/// Joins the body of this entity (it needs a RigidBody) to `connectedBody` (another body, or none
/// = a fixed point of the world). Same conventions as Joint2D: local anchors, autoConnectedAnchor,
/// breakForce -> `broken` and onJointBreak(force).
struct Joint3DComponent {
    JointType3D type = JointType3D::Hinge;
    ecs::Entity connectedBody;
    math::Vec3 anchor{0.0f};
    math::Vec3 connectedAnchor{0.0f};
    bool autoConnectedAnchor = true;
    math::Vec3 axis{0.0f, 1.0f, 0.0f}; ///< Hinge / Slider axis (local to this entity).
    float length = 0.0f;               ///< Distance: rest length; 0 = the distance when created.
    float springFrequency = 0.0f;      ///< Hz; 0 = rigid. Distance: spring; Hinge / Slider: soft limits.
    float springDamping = 0.7f;
    bool enableLimits = false;
    float lowerAngle = -0.7853982f;    ///< Hinge limits (radians).
    float upperAngle = 0.7853982f;
    float lowerLimit = -1.0f;          ///< Slider translation / Distance min length (m).
    float upperLimit = 1.0f;
    bool enableMotor = false;
    float motorSpeed = 0.0f;           ///< Hinge rad/s; Slider m/s.
    float maxMotorForce = 1000.0f;     ///< N (torque N·m for a hinge).
    float breakForce = 0.0f;           ///< N; 0 = unbreakable.
    bool collideConnected = false;
    bool broken = false;
};

/// Kinematic character (Jolt CharacterVirtual): a capsule moved by its velocity (set by scripts
/// with setVelocity), that slides along walls, climbs steps and slopes up to maxSlope, sticks to
/// the ground going down, stands on moving platforms and pushes dynamic bodies. Gravity is added
/// to its velocity while it is in the air. Scripts: isGrounded(), groundNormal(). Do not combine
/// with a RigidBody on the same entity. The capsule stands on the entity's origin (feet).
struct CharacterController3DComponent {
    float radius = 0.35f;
    float height = 1.8f;               ///< Total height, feet to head.
    float maxSlope = 0.8726646f;       ///< Steepest walkable slope (radians, 50°).
    float stepHeight = 0.3f;           ///< Steps climbed without jumping.
    float gravityScale = 1.0f;
    float mass = 70.0f;                ///< kg, for pushing bodies.
    float pushForce = 200.0f;          ///< N, strongest push on dynamic bodies.
    i32 layer = 0;
    u32 collidesWith = 0xFFFFFFFFu;
};

void registerPhysics3DComponents();

} // namespace agr
