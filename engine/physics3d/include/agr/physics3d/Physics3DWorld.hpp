#pragma once
#include <string>
#include <functional>
#include <memory>
#include <optional>
#include <vector>

#include "agr/physics3d/Physics3DComponents.hpp"

namespace agr {

class JobSystem;

struct Collision3DEvent {
    ecs::Entity a, b;   ///< Body entities (for triggers: a = the sensor).
    bool begin = true;
    bool trigger = false;
};

/// A Joint3D pulled harder than its breakForce: the joint was removed and `broken` set.
struct JointBreak3DEvent {
    ecs::Entity entity;
    float force = 0.0f;  ///< N.
};

struct RaycastHit3D {
    ecs::Entity entity;  ///< Body entity.
    math::Vec3 point;
    math::Vec3 normal;
    float distance = 0.0f;
};

struct Physics3DStats {
    u32 bodies = 0;
    u32 activeBodies = 0;
    u32 rebuilt = 0;
    f64 stepMs = 0.0;
};

/// 3D physics of a Scene (Jolt Physics). Same model as Physics2DWorld: bodies mirror
/// RigidBody / Collider components, two-way transform sync, rebuild on edits,
/// contact and trigger events, raycasts. The simulation runs on the engine's
/// JobSystem when one is attached (setJobSystem), single-threaded otherwise;
/// both are deterministic on a given platform and report events in the same order.
/// Triangles of a model's mesh for Mesh / Convex colliders (positions in the mesh's space).
struct CollisionMesh {
    std::vector<math::Vec3> positions;
    std::vector<u32> indices;  ///< Triangles.
};
/// Loads collision geometry: (model URI, mesh index) -> mesh, or nullptr (missing file or mesh).
/// Physics3DPlugin installs one that imports models through the engine's file system (cached).
using CollisionMeshSource = std::function<std::shared_ptr<const CollisionMesh>(const std::string& model, i32 meshIndex)>;

class Physics3DWorld {
public:
    Physics3DWorld();
    ~Physics3DWorld();
    Physics3DWorld(const Physics3DWorld&) = delete;
    Physics3DWorld& operator=(const Physics3DWorld&) = delete;

    /// Runs the Jolt simulation on `jobs` (nullptr: single-threaded). The JobSystem
    /// must outlive this world or be detached first.
    void setJobSystem(JobSystem* jobs);
    /// Geometry of Mesh / Convex colliders (without a source they are skipped with a warning).
    void setCollisionMeshSource(CollisionMeshSource source);
    const CollisionMeshSource& collisionMeshSource() const;

    void step(Scene& scene, float dt);
    void sync(Scene& scene);
    void reset();

    void setGravity(math::Vec3 gravity);
    math::Vec3 gravity() const;

    const std::vector<Collision3DEvent>& events() const;
    const std::vector<JointBreak3DEvent>& jointBreaks() const;
    /// Closest hit on a body whose layer is in `layerMask` (all by default).
    std::optional<RaycastHit3D> raycast(math::Vec3 origin, math::Vec3 direction, float maxDistance, u32 layerMask = 0xFFFFFFFFu) const;
    /// Query shape: a sphere (halfExtents.x = radius when `box` is false) or a box.
    struct QueryShape3D {
        bool box = false;
        math::Vec3 halfExtents{0.5f, 0.5f, 0.5f};
        static QueryShape3D sphere(float r) { return {false, {r, r, r}}; }
        static QueryShape3D cube(math::Vec3 half) { return {true, half}; }
    };
    /// Entities whose colliders overlap the shape at `position` (rotated), each once, sorted.
    std::vector<ecs::Entity> overlap(const QueryShape3D& shape, math::Vec3 position, math::Quat rotation = {}, u32 layerMask = 0xFFFFFFFFu,
                                     bool includeTriggers = false) const;
    /// First collider hit sweeping the shape from `position` along `direction` (maxDistance).
    std::optional<RaycastHit3D> castShape(const QueryShape3D& shape, math::Vec3 position, math::Quat rotation, math::Vec3 direction,
                                          float maxDistance, u32 layerMask = 0xFFFFFFFFu) const;
    /// Hinge angle (radians), slider position or distance between the anchors (m) of the
    /// Joint3D of an entity; nullopt without a live joint.
    std::optional<float> jointValue(ecs::Entity e) const;
    u32 jointCount() const;

    // Character controllers (CharacterController3D): velocity is the desired motion, kept between
    // steps (gravity is added in the air); grounded = standing on walkable ground.
    bool hasCharacter(ecs::Entity e) const;
    bool isGrounded(ecs::Entity e) const;
    std::optional<math::Vec3> groundNormal(ecs::Entity e) const;

    std::optional<math::Vec3> velocity(ecs::Entity e) const;
    bool setVelocity(ecs::Entity e, math::Vec3 velocity);
    bool setAngularVelocity(ecs::Entity e, math::Vec3 radiansPerSecond);
    bool applyForce(ecs::Entity e, math::Vec3 force);
    bool applyImpulse(ecs::Entity e, math::Vec3 impulse);
    std::optional<float> mass(ecs::Entity e) const;
    bool hasBody(ecs::Entity e) const;

    Physics3DStats stats() const;

private:
    struct Impl;
    std::unique_ptr<Impl> m;
};

} // namespace agr
