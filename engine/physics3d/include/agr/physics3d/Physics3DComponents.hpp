#pragma once
#include <string>
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
    /// Moves the center of mass from the colliders' center (local units): down = steadier (cars,
    /// motorcycles, boats, a toy that rights itself). Dynamic bodies.
    math::Vec3 centerOfMassOffset{0.0f};
};

enum class ColliderShape3D : i32 {
    Box = 0,
    Sphere = 1,
    Capsule = 2,
    Mesh = 3,    ///< Exact triangles of a model's mesh: static or kinematic bodies (levels, decor).
    Convex = 4,  ///< Convex hull of a model's mesh: any body (moving objects). Dynamic Mesh bodies use it too.
};

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
    std::string model;              ///< Mesh / Convex: model file (.glb, .gltf, .obj, .fbx); empty = the entity's MeshRenderer model.
    i32 meshIndex = -1;             ///< Mesh / Convex: mesh of the model; -1 = the entity's MeshRenderer mesh (else 0).
};

enum class JointType3D : i32 {
    Fixed = 0,     ///< Welds the two bodies together.
    Hinge = 1,     ///< Rotation about `axis` through the anchor (doors, wheels); angle limits, motor.
    Distance = 2,  ///< Keeps the anchors between lowerLimit / upperLimit (rope) or at `length`, optionally springy.
    Slider = 3,    ///< Translation along `axis` only (elevators, drawers); limits, motor.
    Ball = 4,      ///< Free rotation about the anchor (chains, ragdoll shoulders).
    Pulley = 5,    ///< Rope over two fixed points: lengthA + ratio * lengthB stays constant (lifts, counterweights). Needs a connected body.
    SwingTwist = 6,  ///< Ragdoll joint: swing inside a cone of half angle `swingLimit` around `axis`, twist between lowerAngle / upperAngle.
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
    math::Vec3 pulleyPoint{0.0f, 2.0f, 0.0f};           ///< Pulley: fixed world point the rope goes up to from this body.
    math::Vec3 connectedPulleyPoint{1.0f, 2.0f, 0.0f};  ///< Pulley: fixed world point above the connected body.
    float ratio = 1.0f;                                 ///< Pulley: block and tackle ratio (2 = the connected side moves half as far).
    float swingLimit = 0.7853982f;                      ///< SwingTwist: cone half angle (radians).
};

/// Shape of a soft body: a cloth (flag, curtain, cape), or a closed jelly / balloon.
enum class SoftBodyShape3D : i32 { Cloth = 0, Box = 1, Sphere = 2 };
/// Which cloth vertices hold still (follow the entity, so a moving pole carries its flag).
enum class SoftBodyPin3D : i32 { None = 0, Edge = 1, Corners = 2 };

/// Deformable body simulated by Jolt (cloth, jelly, balloon): collides with the other bodies and
/// is drawn by the entity's MeshRenderer material (DynamicMeshComponent). The rest shape is built
/// from `shape`, `size` and `resolution` in the entity's space (its scale included).
struct SoftBody3DComponent {
    SoftBodyShape3D shape = SoftBodyShape3D::Cloth;
    math::Vec3 size{2.0f, 2.0f, 2.0f};  ///< Cloth: x by z (in the XZ plane, edge at -z); box: xyz; sphere: x = diameter.
    u32 resolution = 12;                ///< Vertices per side (cloth / box), rings of a sphere.
    float mass = 1.0f;                  ///< kg, spread over the vertices.
    float stiffness = 0.95f;            ///< 0..1: stretching resistance (1 = inextensible).
    float bendStiffness = 0.3f;         ///< 0..1: folding resistance.
    float pressure = 0.0f;              ///< Closed shapes: > 0 inflates (balloon), 0 = jelly.
    SoftBodyPin3D pin = SoftBodyPin3D::Edge;  ///< Cloth only.
    u32 iterations = 6;
    float damping = 0.5f;               ///< Air drag: a cloth settles instead of swinging for long.
    float gravityScale = 1.0f;
    bool enabled = true;
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

/// Wheeled vehicle (car, truck, kart) on the entity's dynamic RigidBody: 4 ray-cast wheels with
/// suspension, an engine with automatic gears, steering on the front wheels, brakes and a hand
/// brake on the rear ones. The vehicle drives toward its local -Z (forward), +Y up. Children named
/// WheelFL, WheelFR, WheelRL, WheelRR (front / rear, left / right) follow their wheel (spin,
/// steering, suspension). Scripts drive it with throttle / steering / brake / handBrake.
/// Water (or any liquid) in a box: its top face is the surface. Dynamic bodies in it float
/// (Archimedes on their submerged part: boats, crates, barrels), are slowed down, and carried by
/// `flow` (rivers). Characters and static bodies are not affected.
struct WaterVolume3DComponent {
    math::Vec3 size{20.0f, 4.0f, 20.0f};  ///< Full size (local units, centered on the entity; top = surface).
    float buoyancy = 1.2f;     ///< 1 = neutral, < 1 sinks slowly, > 1 floats (higher sits higher).
    float linearDrag = 0.6f;   ///< Slows the submerged part (water resistance).
    float angularDrag = 0.05f;
    math::Vec3 flow{0.0f};     ///< Current (m/s, world space).
    bool enabled = true;
};

/// Car: 2 to 4 axles (front one steers). Motorcycle: 2 wheels in line, leans into turns.
/// Tracked: tank tracks on each side (steering turns the tracks at different speeds, on the spot too).
enum class VehicleKind3D : i32 { Car = 0, Motorcycle = 1, Tracked = 2 };

struct Vehicle3DComponent {
    VehicleKind3D kind = VehicleKind3D::Car;
    i32 axles = 2;  ///< Car: 2..4 (trucks); Tracked: road wheels per side (2..8).
    // Driver input (scripts, every frame).
    float throttle = 0.0f;   ///< -1 (reverse) .. 1 (full gas).
    float steering = 0.0f;   ///< -1 (left) .. 1 (right).
    float brake = 0.0f;      ///< 0 .. 1.
    bool handBrake = false;
    // Wheels (local space of the body).
    float wheelBase = 2.6f;      ///< Distance between front and rear axles.
    float trackWidth = 1.6f;     ///< Distance between left and right wheels.
    float wheelHeight = 0.0f;    ///< Height of the suspension attachment (local y).
    float wheelRadius = 0.35f;
    float wheelWidth = 0.25f;
    float suspensionMin = 0.05f; ///< Suspension length fully compressed (m).
    float suspensionMax = 0.4f;  ///< Suspension length fully extended (m).
    float suspensionFrequency = 1.5f;  ///< Spring stiffness (Hz).
    float suspensionDamping = 0.5f;
    float maxSteerAngle = 0.5235988f;  ///< Radians (30°).
    // Drive.
    float engineTorque = 500.0f;  ///< N·m.
    float maxRpm = 6000.0f;
    float brakeTorque = 1500.0f;      ///< N·m per wheel.
    float handBrakeTorque = 4000.0f;  ///< N·m per rear wheel.
    bool frontDrive = false;
    bool rearDrive = true;
    float tireFriction = 1.0f;  ///< Grip multiplier.
    // State (written by the simulation).
    float speed = 0.0f;  ///< m/s along the forward axis (negative = reversing).
    float rpm = 0.0f;
    i32 gear = 0;
};

void registerPhysics3DComponents();

} // namespace agr
