#pragma once
#include <vector>

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

enum class ColliderShape2D : i32 {
    Box = 0,
    Circle = 1,
    Capsule = 2,
    Polygon = 3,  ///< `points` (local), any simple polygon, convex or not (split into convex pieces).
    Chain = 4,    ///< `points` as a smooth outline (ground, hills, cave walls): no bumps between segments. Static bodies.
};

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
    i32 layer = 0;                  ///< Collision layer, 0..31 (names in the project settings).
    u32 collidesWith = 0xFFFFFFFFu; ///< Layers it collides with (bit n = layer n). Two shapes touch when each one's mask has the other's layer.
    std::vector<math::Vec2> points; ///< Polygon / Chain vertices in local units (counter-clockwise for polygons).
    bool loop = false;              ///< Chain: closes the outline (last point back to the first).
    /// One-way platform: solid only from above (its local +Y side); bodies and characters jump
    /// through it from below and from the sides.
    bool oneWay = false;
};

/// Platformer / top-down character moved by its `velocity` (the 2D counterpart of Godot's
/// CharacterBody2D + move_and_slide): a vertical capsule that slides along walls and slopes,
/// stands on floors up to `maxSlope`, follows moving platforms, goes through one-way platforms
/// from below and pushes dynamic bodies. Scripts set `velocity` (e.g. in onFixedUpdate) and read
/// `onFloor` / `onWall` / `onCeiling` after the physics step; gravity is applied unless
/// `gravityScale` is 0.
struct CharacterController2DComponent {
    math::Vec2 size{0.8f, 1.6f};    ///< Capsule width x height (world units, scaled by the entity).
    math::Vec2 offset{0.0f, 0.0f};
    math::Vec2 velocity{0.0f, 0.0f};///< m/s; walls and floors remove the part going into them.
    float gravityScale = 1.0f;
    float maxSlope = 0.785398f;     ///< Steepest floor (radians); steeper slopes are walls.
    float snapDistance = 0.1f;      ///< Stays glued to the floor going down slopes and steps (0: off).
    float pushForce = 1.0f;         ///< > 0: walks into dynamic bodies and pushes them; 0: they block like walls.
    i32 layer = 0;
    u32 collidesWith = 0xFFFFFFFFu;
    // State after the last physics step (read-only).
    bool onFloor = false;
    bool onWall = false;
    bool onCeiling = false;
    math::Vec2 floorNormal{0.0f, 1.0f};
};

/// Static collision for every painted cell of the Tilemap on the same entity
/// (cells merged into horizontal runs).
struct TilemapCollider2DComponent {
    float friction = 0.6f;
    float restitution = 0.0f;
    i32 layer = 0;
    u32 collidesWith = 0xFFFFFFFFu;
};

enum class JointType2D : i32 {
    Fixed = 0,     ///< Welds the two bodies together (springFrequency > 0: soft weld).
    Hinge = 1,     ///< Rotation about the anchor (doors, wheels, pendulums); angle limits, motor.
    Distance = 2,  ///< Keeps the anchors at `length` (rope with limits, spring with springFrequency); motor along the line.
    Slider = 3,    ///< Translation along `axis` only (elevators, pistons); limits, motor.
};

/// Joins the body of this entity (it needs a RigidBody2D) to `connectedBody` (another body, or
/// none = a fixed point of the world). Anchors are in each entity's local space; with
/// autoConnectedAnchor the connected anchor is the same world point as `anchor` when the joint is
/// created. A joint pulled harder than breakForce breaks: `broken` becomes true and the script of
/// the entity receives onJointBreak(force).
struct Joint2DComponent {
    JointType2D type = JointType2D::Hinge;
    ecs::Entity connectedBody;
    math::Vec2 anchor{0.0f, 0.0f};
    math::Vec2 connectedAnchor{0.0f, 0.0f};
    bool autoConnectedAnchor = true;
    math::Vec2 axis{1.0f, 0.0f};      ///< Slider: direction of travel (local to this entity).
    float length = 0.0f;              ///< Distance: rest length; 0 = the distance when created.
    float springFrequency = 0.0f;     ///< Hz; 0 = rigid. Distance: spring; Hinge: spring back to the start angle; Fixed: soft weld.
    float springDamping = 0.7f;       ///< Damping ratio (1 = critical).
    bool enableLimits = false;
    float lowerAngle = -0.7853982f;   ///< Hinge limits (radians).
    float upperAngle = 0.7853982f;
    float lowerLimit = -1.0f;         ///< Slider translation / Distance min length (m).
    float upperLimit = 1.0f;          ///< Slider translation / Distance max length (m).
    bool enableMotor = false;
    float motorSpeed = 0.0f;          ///< Hinge rad/s; Slider / Distance m/s.
    float maxMotorForce = 1000.0f;    ///< N (torque N·m for a hinge).
    float breakForce = 0.0f;          ///< N; 0 = unbreakable.
    bool collideConnected = false;
    bool broken = false;              ///< Set when the joint broke (cleared to rebuild it).
};

/// Registers the reflection data (idempotent).
void registerPhysics2DComponents();

} // namespace agr
