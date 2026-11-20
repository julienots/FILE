#pragma once
#include <memory>
#include <optional>
#include <vector>

#include "agr/physics2d/Physics2DComponents.hpp"

namespace agr {

/// Contact (begin / end) between two colliders, or a trigger overlap.
struct Collision2DEvent {
    ecs::Entity a, b;     ///< Entities of the two colliders (for triggers: a = trigger).
    bool begin = true;    ///< false: the contact ended.
    bool trigger = false;
};

/// A Joint2D pulled harder than its breakForce: the joint was removed and `broken` set.
struct JointBreak2DEvent {
    ecs::Entity entity;  ///< Entity carrying the Joint2D.
    float force = 0.0f;  ///< N.
};

struct RaycastHit2D {
    ecs::Entity entity;   ///< Entity of the collider hit.
    math::Vec2 point;
    math::Vec2 normal;
    float distance = 0.0f;
};

/// Shape of an overlap or cast query (world units, centered on the query position).
struct QueryShape2D {
    ColliderShape2D shape = ColliderShape2D::Circle;  ///< Box, Circle, Capsule or Polygon (convex, up to 8 points).
    math::Vec2 size{1.0f, 1.0f};                      ///< Box size; Capsule width x height.
    float radius = 0.5f;                              ///< Circle.
    std::vector<math::Vec2> points;                   ///< Polygon.
    static QueryShape2D circle(float r) { QueryShape2D q; q.shape = ColliderShape2D::Circle; q.radius = r; return q; }
    static QueryShape2D box(math::Vec2 size) { QueryShape2D q; q.shape = ColliderShape2D::Box; q.size = size; return q; }
};

struct Physics2DStats {
    u32 bodies = 0;
    u32 shapes = 0;
    u32 dynamicBodies = 0;
    u32 rebuilt = 0;      ///< Bodies (re)created during the last step (new or edited colliders).
    f64 stepMs = 0.0;     ///< Last step: sync + simulation + write back.
};

/// 2D physics of a Scene (Box2D v3). Bodies mirror the RigidBody2D / Collider2D /
/// TilemapCollider2D components: they are created, rebuilt when their parameters
/// change and destroyed with their entity. Units: meters, kilograms, seconds.
///
/// step() order: sync (scene -> bodies, including transforms moved by gameplay),
/// simulation, write back (dynamic and kinematic bodies -> transforms), events.
class Physics2DWorld {
public:
    Physics2DWorld();
    ~Physics2DWorld();
    Physics2DWorld(const Physics2DWorld&) = delete;
    Physics2DWorld& operator=(const Physics2DWorld&) = delete;

    void step(Scene& scene, float dt);
    /// Creates / updates / removes bodies without simulating (e.g. before queries).
    void sync(Scene& scene);
    /// Destroys every body (scene replaced).
    void reset();

    void setGravity(math::Vec2 gravity);
    math::Vec2 gravity() const;
    void setSubSteps(int subSteps);

    /// Contacts and trigger overlaps that began or ended during the last step.
    const std::vector<Collision2DEvent>& events() const;
    /// Joints that broke during the last step.
    const std::vector<JointBreak2DEvent>& jointBreaks() const;
    /// Closest hit on a collider whose layer is in `layerMask` (all by default).
    std::optional<RaycastHit2D> raycast(math::Vec2 origin, math::Vec2 direction, float maxDistance, u32 layerMask = 0xFFFFFFFFu) const;
    /// Entities whose colliders overlap `shape` placed at `position` (rotated by `angle`), each
    /// once, sorted. Triggers only with includeTriggers. Characters are included.
    std::vector<ecs::Entity> overlap(const QueryShape2D& shape, math::Vec2 position, float angle = 0.0f, u32 layerMask = 0xFFFFFFFFu,
                                     bool includeTriggers = false) const;
    /// First collider hit when sweeping `shape` from `position` along `direction` up to
    /// maxDistance (shapes already overlapping at the start are skipped). distance = travel.
    std::optional<RaycastHit2D> castShape(const QueryShape2D& shape, math::Vec2 position, float angle, math::Vec2 direction, float maxDistance,
                                          u32 layerMask = 0xFFFFFFFFu) const;
    /// Current state of the Joint2D of an entity: hinge angle (radians), slider translation or
    /// distance length (m); nullopt without a live joint.
    std::optional<float> jointValue(ecs::Entity e) const;
    /// Joints currently alive.
    u32 jointCount() const;

    // Body control (the entity must carry a RigidBody2D whose body exists).
    std::optional<math::Vec2> velocity(ecs::Entity e) const;
    bool setVelocity(ecs::Entity e, math::Vec2 velocity);
    bool setAngularVelocity(ecs::Entity e, float radiansPerSecond);
    bool applyForce(ecs::Entity e, math::Vec2 force);
    bool applyImpulse(ecs::Entity e, math::Vec2 impulse);
    std::optional<float> mass(ecs::Entity e) const;
    bool hasBody(ecs::Entity e) const;

    Physics2DStats stats() const;

private:
    struct Impl;
    std::unique_ptr<Impl> m;
};

} // namespace agr
