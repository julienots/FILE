#include "agr/twod/Particles2D.hpp"

#include <algorithm>
#include <cmath>
#include <random>

namespace agr {

namespace {

u32 nextRandom(u32& state) {  // xorshift32: fast, deterministic per emitter
    state ^= state << 13;
    state ^= state >> 17;
    state ^= state << 5;
    return state;
}
float uniform(u32& state) { return static_cast<float>(nextRandom(state) >> 8) * (1.0f / 16777216.0f); }
float range(u32& state, float a, float b) { return a + (b - a) * uniform(state); }

void spawn(const ParticleEmitter2DComponent& em, ParticleState2DComponent& st, const math::Mat4& world, u32 count, u32& spawned) {
    const float baseAngle = std::atan2(world.c[0].y, world.c[0].x);  // emitter rotation around Z
    const math::Vec3 origin = world.translation();
    for (u32 i = 0; i < count && st.particles.size() < em.maxParticles; ++i) {
        Particle2D p;
        // World space: the emitter rotation turns the emission direction. Local space:
        // the whole system is transformed at render time.
        const float angle = em.direction + (em.worldSpace ? baseAngle : 0.0f) + range(st.rng, -em.spread, em.spread);
        const float speed = range(st.rng, em.speedMin, em.speedMax);
        p.velocity = {std::cos(angle) * speed, std::sin(angle) * speed};
        math::Vec2 offset{};
        if (em.spawnRadius > 0.0f) {
            const float r = em.spawnRadius * std::sqrt(uniform(st.rng)), a = range(st.rng, 0.0f, 2.0f * math::kPi);
            offset = {std::cos(a) * r, std::sin(a) * r};
        }
        if (em.worldSpace) {
            p.position = math::Vec2{origin.x, origin.y} + offset;
            p.z = origin.z;
        } else {
            p.position = offset;
        }
        p.lifetime = std::max(range(st.rng, em.lifetimeMin, std::max(em.lifetimeMin, em.lifetimeMax)), 0.01f);
        p.rotation = range(st.rng, 0.0f, 2.0f * math::kPi);
        p.angularVelocity = em.angularVelocity * (uniform(st.rng) < 0.5f ? -1.0f : 1.0f);
        st.particles.push_back(p);
        ++spawned;
    }
}

} // namespace

ParticleStats2D simulateParticles2D(Scene& scene, float dt) {
    ParticleStats2D stats;
    auto& reg = scene.registry();
    std::vector<ecs::Entity> missing;
    reg.view<ParticleEmitter2DComponent>().each([&](ecs::Entity e, ParticleEmitter2DComponent&) {
        if (!reg.has<ParticleState2DComponent>(e)) missing.push_back(e);
    });
    for (ecs::Entity e : missing) reg.emplace<ParticleState2DComponent>(e);

    reg.view<ParticleEmitter2DComponent, ParticleState2DComponent, WorldTransformComponent>().each(
        [&](ecs::Entity, ParticleEmitter2DComponent& em, ParticleState2DComponent& st, WorldTransformComponent& w) {
            ++stats.emitters;
            if (!w.activeInHierarchy) return;  // inactive emitters are paused
            if (!st.started) {
                st.started = true;
                st.time = 0.0f;
                st.emitAccumulator = 0.0f;
                st.rng = em.seed ? em.seed : static_cast<u32>(std::random_device{}()) | 1u;
                if (em.emitting) spawn(em, st, w.matrix, em.burst, stats.spawned);
            }
            if (dt > 0.0f) {
                // Integrate and remove dead particles (swap-and-pop).
                const float damping = std::max(0.0f, 1.0f - em.drag * dt);
                for (usize i = 0; i < st.particles.size();) {
                    Particle2D& p = st.particles[i];
                    p.age += dt;
                    if (p.age >= p.lifetime) {
                        p = st.particles.back();
                        st.particles.pop_back();
                        continue;
                    }
                    p.velocity = (p.velocity + em.gravity * dt) * damping;
                    p.position += p.velocity * dt;
                    p.rotation += p.angularVelocity * dt;
                    ++i;
                }
                // Emission over the cycle.
                if (em.emitting) {
                    const float duration = std::max(em.duration, 0.01f);
                    const bool active = em.loop || st.time < duration;
                    st.time += dt;
                    if (active) {
                        st.emitAccumulator += em.rate * dt;
                        const u32 n = static_cast<u32>(st.emitAccumulator);
                        st.emitAccumulator -= static_cast<float>(n);
                        spawn(em, st, w.matrix, n, stats.spawned);
                    }
                    if (em.loop && st.time >= duration) {
                        st.time = std::fmod(st.time, duration);
                        spawn(em, st, w.matrix, em.burst, stats.spawned);
                    }
                }
            }
            stats.particles += static_cast<u32>(st.particles.size());
        });
    return stats;
}

void restartParticles2D(Scene& scene, ecs::Entity e) {
    if (auto* st = scene.registry().tryGet<ParticleState2DComponent>(e)) *st = ParticleState2DComponent{};
}

} // namespace agr
