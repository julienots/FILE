#pragma once
#include <memory>
#include <optional>
#include <vector>

#include "agr/physics3d/Physics3DComponents.hpp"

namespace agr {

struct Collision3DEvent {
    ecs::Entity a, b;   ///< Body entities (for triggers: a = the sensor).
    bool begin = true;
    bool trigger = false;
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
/// contact and trigger events, raycasts. Single-threaded simulation (deterministic
/// on a given platform).
class Physics3DWorld {
public:
    Physics3DWorld();
    ~Physics3DWorld();
    Physics3DWorld(const Physics3DWorld&) = delete;
    Physics3DWorld& operator=(const Physics3DWorld&) = delete;

    void step(Scene& scene, float dt);
    void sync(Scene& scene);
    void reset();

    void setGravity(math::Vec3 gravity);
    math::Vec3 gravity() const;

    const std::vector<Collision3DEvent>& events() const;
    std::optional<RaycastHit3D> raycast(math::Vec3 origin, math::Vec3 direction, float maxDistance) const;

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
