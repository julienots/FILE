#pragma once
// Forces and obstacles shared by the 2D and 3D particle simulations.
#include <algorithm>
#include <cmath>
#include <vector>

#include "agr/render/RenderComponents.hpp"
#include "agr/scene/Components.hpp"
#include "agr/scene/Scene.hpp"

namespace agr::particles {

struct Attractor {
    math::Vec3 center;
    float strength, radius;
};
struct Collider {
    math::Mat4 world, inverse;
    ParticleColliderComponent::Shape shape;
    math::Vec3 size;
    float radius;  // sphere, world units
};
struct World {
    std::vector<Attractor> attractors;
    std::vector<Collider> colliders;
};

inline World gather(Scene& scene) {
    World w;
    auto& reg = scene.registry();
    reg.view<ParticleAttractorComponent, WorldTransformComponent>().each([&](ecs::Entity, ParticleAttractorComponent& a, WorldTransformComponent& t) {
        if (a.enabled && t.activeInHierarchy && a.radius > 0.0f && a.strength != 0.0f) w.attractors.push_back({t.matrix.translation(), a.strength, a.radius});
    });
    reg.view<ParticleColliderComponent, WorldTransformComponent>().each([&](ecs::Entity, ParticleColliderComponent& c, WorldTransformComponent& t) {
        if (!t.activeInHierarchy) return;
        const float scale = std::max({math::length(t.matrix.transformVector({1, 0, 0})), math::length(t.matrix.transformVector({0, 1, 0})),
                                      math::length(t.matrix.transformVector({0, 0, 1}))});
        w.colliders.push_back({t.matrix, math::inverse(t.matrix), c.shape, c.size, c.size.x * 0.5f * scale});
    });
    return w;
}

/// Divergence-free swirling field in [-2, 2]^3 (each component ignores its own axis), drifting
/// with time `t`; `scale` = size of the swirls.
inline math::Vec3 turbulence(math::Vec3 p, float t, float scale) {
    const float f = 1.0f / std::max(scale, 1e-3f);
    const float x = p.x * f, y = p.y * f, z = p.z * f;
    return {std::sin(y * 1.31f + t) + std::cos(z * 1.73f - t * 0.83f), std::sin(z * 1.19f + t * 0.91f) + std::cos(x * 1.57f + t * 1.07f),
            std::sin(x * 1.43f - t * 1.13f) + std::cos(y * 1.61f + t * 0.77f)};
}

/// Acceleration from the attractors at `p` (flat2D: in the XY plane only).
inline math::Vec3 attraction(const World& w, math::Vec3 p, bool flat2D) {
    math::Vec3 a{0.0f};
    for (const Attractor& at : w.attractors) {
        math::Vec3 d = at.center - p;
        if (flat2D) d.z = 0.0f;
        const float dist = math::length(d);
        if (dist >= at.radius || dist < 1e-4f) continue;
        a = a + d * (at.strength * (1.0f - dist / at.radius) / dist);
    }
    return a;
}

/// Pushes a particle out of the ground and the colliders, bouncing its velocity. True on a hit.
template <typename Emitter>
bool collide(const World& w, const Emitter& em, math::Vec3& p, math::Vec3& v, bool flat2D) {
    bool hit = false;
    auto respond = [&](math::Vec3 n) {
        const float vn = math::dot(v, n);
        if (vn < 0.0f) {
            const math::Vec3 normal = n * vn, tangent = v - normal;
            v = tangent * (1.0f - std::clamp(em.collisionFriction, 0.0f, 1.0f)) - normal * std::clamp(em.bounce, 0.0f, 1.0f);
        }
        hit = true;
    };
    if (em.groundPlane && p.y < em.groundHeight) {
        p.y = em.groundHeight;
        respond({0.0f, 1.0f, 0.0f});
    }
    for (const Collider& c : w.colliders) {
        if (c.shape == ParticleColliderComponent::Shape::Sphere) {
            const math::Vec3 center = c.world.translation();
            math::Vec3 d = p - center;
            if (flat2D) d.z = 0.0f;
            const float dist = math::length(d);
            if (dist >= c.radius) continue;
            const math::Vec3 n = dist > 1e-5f ? d * (1.0f / dist) : math::Vec3{0.0f, 1.0f, 0.0f};
            p = p + n * (c.radius - dist);
            respond(n);
        } else {
            math::Vec3 local = c.inverse.transformPoint(flat2D ? math::Vec3{p.x, p.y, c.world.translation().z} : p);
            const math::Vec3 h = c.size * 0.5f;
            if (std::abs(local.x) >= h.x || std::abs(local.y) >= h.y || (!flat2D && std::abs(local.z) >= h.z)) continue;
            // Out through the nearest face.
            float depth[3] = {h.x - std::abs(local.x), h.y - std::abs(local.y), flat2D ? 1e30f : h.z - std::abs(local.z)};
            int axis = depth[0] < depth[1] ? 0 : 1;
            if (depth[2] < depth[axis]) axis = 2;
            math::Vec3 n{0.0f};
            (&n.x)[axis] = (&local.x)[axis] >= 0.0f ? 1.0f : -1.0f;
            (&local.x)[axis] = (&n.x)[axis] * (&h.x)[axis];
            const math::Vec3 out = c.world.transformPoint(local);
            p = flat2D ? math::Vec3{out.x, out.y, p.z} : out;
            math::Vec3 wn = c.world.transformVector(n);
            if (flat2D) wn.z = 0.0f;
            respond(math::length(wn) > 1e-6f ? math::normalize(wn) : math::Vec3{0.0f, 1.0f, 0.0f});
        }
    }
    return hit;
}

struct SubBurst {
    ecs::Entity emitter;
    math::Vec3 position;
    u32 count;
};

} // namespace agr::particles
