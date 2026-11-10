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

struct RaycastHit2D {
    ecs::Entity entity;   ///< Entity of the collider hit.
    math::Vec2 point;
    math::Vec2 normal;
    float distance = 0.0f;
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
    std::optional<RaycastHit2D> raycast(math::Vec2 origin, math::Vec2 direction, float maxDistance) const;

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
