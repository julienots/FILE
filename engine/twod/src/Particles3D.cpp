#include <algorithm>
#include <cmath>
#include <random>

#include "agr/twod/Particles2D.hpp"

#include "ParticleWorld.hpp"

namespace agr {

using math::Vec3;

namespace {

u32 nextRandom(u32& state) {  // xorshift32
    state ^= state << 13;
    state ^= state >> 17;
    state ^= state << 5;
    return state;
}
float uniform(u32& s) { return static_cast<float>(nextRandom(s) >> 8) * (1.0f / 16777216.0f); }
float range(u32& s, float a, float b) { return a + (b - a) * uniform(s); }

Vec3 randomUnit(u32& s) {
    const float z = range(s, -1.0f, 1.0f), a = range(s, 0.0f, 2.0f * math::kPi), r = std::sqrt(std::max(0.0f, 1.0f - z * z));
    return {r * std::cos(a), z, r * std::sin(a)};
}

void spawn(const ParticleEmitter3DComponent& em, ParticleState3DComponent& st, const math::Mat4& world, u32 count, u32& spawned,
           Vec3 inherited = Vec3{0.0f}) {
    for (u32 i = 0; i < count && st.particles.size() < em.maxParticles; ++i) {
        Vec3 pos{0.0f}, dir{0.0f, 1.0f, 0.0f};
        switch (em.shape) {
            case EmitterShape3D::Point: break;
            case EmitterShape3D::Sphere: {
                dir = randomUnit(st.rng);
                pos = dir * (em.emitFromSurface ? em.shapeRadius : em.shapeRadius * std::cbrt(uniform(st.rng)));
                break;
            }
            case EmitterShape3D::Box:
                pos = {range(st.rng, -0.5f, 0.5f) * em.boxSize.x, range(st.rng, -0.5f, 0.5f) * em.boxSize.y, range(st.rng, -0.5f, 0.5f) * em.boxSize.z};
                if (em.emitFromSurface) {  // pushed onto a face, chosen in proportion to its area
                    const float ax = em.boxSize.y * em.boxSize.z, ay = em.boxSize.x * em.boxSize.z, az = em.boxSize.x * em.boxSize.y;
                    const float pick = uniform(st.rng) * std::max(ax + ay + az, 1e-6f);
                    const float side = uniform(st.rng) < 0.5f ? -0.5f : 0.5f;
                    if (pick < ax) pos.x = side * em.boxSize.x;
                    else if (pick < ax + ay) pos.y = side * em.boxSize.y;
                    else pos.z = side * em.boxSize.z;
                }
                break;
            case EmitterShape3D::Cone:
            case EmitterShape3D::Ring: {
                // Direction within the cone around +Y; start on the base disc (cone) or its rim (ring).
                const float cosMax = std::cos(std::clamp(em.coneAngle, 0.0f, math::kPi));
                const float c = range(st.rng, cosMax, 1.0f), sn = std::sqrt(std::max(0.0f, 1.0f - c * c)), a = range(st.rng, 0.0f, 2.0f * math::kPi);
                dir = {sn * std::cos(a), c, sn * std::sin(a)};
                const float r = em.shape == EmitterShape3D::Ring ? em.shapeRadius : em.shapeRadius * std::sqrt(uniform(st.rng));
                const float b = range(st.rng, 0.0f, 2.0f * math::kPi);
                pos = {r * std::cos(b), 0.0f, r * std::sin(b)};
                break;
            }
        }
        Particle3D p;
        const Vec3 velocity = dir * range(st.rng, em.speedMin, std::max(em.speedMin, em.speedMax));
        if (em.worldSpace) {
            p.position = world.transformPoint(pos);
            p.velocity = world.transformVector(velocity) + inherited;
        } else {
            p.position = pos;
            p.velocity = velocity;
        }
        p.lifetime = std::max(range(st.rng, em.lifetimeMin, std::max(em.lifetimeMin, em.lifetimeMax)), 0.01f);
        p.rotation = range(st.rng, 0.0f, 2.0f * math::kPi);
        p.angularVelocity = em.angularVelocity * (uniform(st.rng) < 0.5f ? -1.0f : 1.0f);
        st.particles.push_back(p);
        ++spawned;
    }
}

} // namespace

ParticleStats2D simulateParticles3D(Scene& scene, float dt) {
    ParticleStats2D stats;
    auto& reg = scene.registry();
    const particles::World world = particles::gather(scene);
    std::vector<particles::SubBurst> bursts;
    std::vector<ecs::Entity> missing;
    reg.view<ParticleEmitter3DComponent>().each([&](ecs::Entity e, ParticleEmitter3DComponent&) {
        if (!reg.has<ParticleState3DComponent>(e)) missing.push_back(e);
    });
    for (ecs::Entity e : missing) reg.emplace<ParticleState3DComponent>(e);
    reg.view<ParticleEmitter3DComponent, ParticleState3DComponent, WorldTransformComponent>().each(
        [&](ecs::Entity, ParticleEmitter3DComponent& em, ParticleState3DComponent& st, WorldTransformComponent& w) {
            ++stats.emitters;
            if (!w.activeInHierarchy) return;
            if (em.gpu) {  // the GPU computes the particles: only the clock and the spawn window here
                advanceGpuEmitter(em, st, dt, st.started ? 0u : em.seed ? em.seed : static_cast<u32>(std::random_device{}()));
                stats.particles += st.gpuAlive;
                return;
            }
            const bool attract = em.worldSpace && em.affectedByAttractors && !world.attractors.empty();
            const bool collide = em.worldSpace && em.collisions;
            auto subBurst = [&](SubEmitTrigger when, const Particle3D& p) {
                if (em.subEmitOn == when && !em.subEmitter.isNull() && em.subEmitCount > 0)
                    bursts.push_back({em.subEmitter, em.worldSpace ? p.position : w.matrix.transformPoint(p.position), static_cast<u32>(em.subEmitCount)});
            };
            if (!st.started) {
                st.started = true;
                st.time = 0.0f;
                st.emitAccumulator = 0.0f;
                st.rng = em.seed ? em.seed : static_cast<u32>(std::random_device{}()) | 1u;
                st.lastEmitterPos = w.matrix.translation();
                if (em.emitting) spawn(em, st, w.matrix, em.burst, stats.spawned);
            }
            Vec3 inherited{0.0f};
            if (dt > 0.0f) {
                const Vec3 now = w.matrix.translation();
                if (em.inheritVelocity != 0.0f && em.worldSpace) inherited = (now - st.lastEmitterPos) * (em.inheritVelocity / dt);
                st.lastEmitterPos = now;
                st.clock = std::fmod(st.clock + dt, 3600.0f);
            }
            const bool swirl = em.turbulence != 0.0f;
            if (dt > 0.0f) {
                const float damping = std::max(0.0f, 1.0f - em.drag * dt);
                // Gravity is in world units: in local space, bring it into the emitter's frame.
                const Vec3 g = em.worldSpace ? em.gravity : math::inverse(w.matrix).transformVector(em.gravity);
                for (usize i = 0; i < st.particles.size();) {
                    Particle3D& p = st.particles[i];
                    p.age += dt;
                    bool dead = p.age >= p.lifetime;
                    if (!dead) {
                        Vec3 accel = attract ? g + particles::attraction(world, p.position, false) : g;
                        if (swirl) accel = accel + particles::turbulence(p.position, st.clock * em.turbulenceSpeed, em.turbulenceScale) * em.turbulence;
                        p.velocity = (p.velocity + accel * dt) * damping;
                        p.position = p.position + p.velocity * dt;
                        p.rotation += p.angularVelocity * dt;
                        if (collide && particles::collide(world, em, p.position, p.velocity, false)) {
                            ++stats.collisions;
                            subBurst(SubEmitTrigger::Collision, p);
                            dead = em.dieOnCollision;
                        }
                    }
                    if (dead) {
                        subBurst(SubEmitTrigger::Death, p);
                        p = st.particles.back();
                        st.particles.pop_back();
                        continue;
                    }
                    ++i;
                }
                if (em.emitting) {
                    const float duration = std::max(em.duration, 0.01f);
                    const bool active = em.loop || st.time < duration;
                    st.time += dt;
                    if (active) {
                        st.emitAccumulator += em.rate * dt;
                        const u32 n = static_cast<u32>(st.emitAccumulator);
                        st.emitAccumulator -= static_cast<float>(n);
                        spawn(em, st, w.matrix, n, stats.spawned, inherited);
                    }
                    if (em.loop && st.time >= duration) {
                        st.time = std::fmod(st.time, duration);
                        spawn(em, st, w.matrix, em.burst, stats.spawned, inherited);
                    }
                }
            }
            stats.particles += static_cast<u32>(st.particles.size());
        });
    for (const particles::SubBurst& b : bursts) stats.spawned += burstParticles3D(scene, b.emitter, b.position, b.count);
    return stats;
}

u32 burstParticles3D(Scene& scene, ecs::Entity e, Vec3 position, u32 count) {
    auto& reg = scene.registry();
    auto* em = reg.tryGet<ParticleEmitter3DComponent>(e);
    auto* w = reg.tryGet<WorldTransformComponent>(e);
    if (!em || !w) return 0;
    auto* st = reg.tryGet<ParticleState3DComponent>(e);
    if (!st) st = &reg.emplace<ParticleState3DComponent>(e);
    if (!st->started) {
        st->started = true;
        st->rng = em->seed ? em->seed : static_cast<u32>(std::random_device{}()) | 1u;
    }
    u32 spawned = 0;
    if (!em->worldSpace) {
        const Vec3 local = math::inverse(w->matrix).transformPoint(position);
        const usize first = st->particles.size();
        spawn(*em, *st, w->matrix, count, spawned);
        for (usize i = first; i < st->particles.size(); ++i) st->particles[i].position = st->particles[i].position + local;
        return spawned;
    }
    math::Mat4 m = w->matrix;
    m.c[3] = {position.x, position.y, position.z, 1.0f};
    spawn(*em, *st, m, count, spawned);
    return spawned;
}

void restartParticles3D(Scene& scene, ecs::Entity e) {
    if (auto* st = scene.registry().tryGet<ParticleState3DComponent>(e)) *st = ParticleState3DComponent{};
}

const char* particlePresetName(ParticlePreset p) {
    switch (p) {
        case ParticlePreset::Fire: return "Feu";
        case ParticlePreset::Smoke: return "Fumée";
        case ParticlePreset::Sparks: return "Étincelles";
        case ParticlePreset::Magic: return "Magie";
        case ParticlePreset::Rain: return "Pluie";
        case ParticlePreset::Snow: return "Neige";
        case ParticlePreset::Dust: return "Poussière";
    }
    return "";
}

void applyParticlePreset(ParticleEmitter3DComponent& em, ParticlePreset preset) {
    const std::string texture = em.texture;
    em = ParticleEmitter3DComponent{};
    em.texture = texture;
    switch (preset) {
        case ParticlePreset::Fire:
            em.rate = 60.0f, em.lifetimeMin = 0.5f, em.lifetimeMax = 1.0f, em.speedMin = 0.8f, em.speedMax = 1.8f;
            em.shape = EmitterShape3D::Cone, em.shapeRadius = 0.25f, em.coneAngle = math::radians(12.0f);
            em.startColor = {1.0f, 0.75f, 0.25f, 1.0f}, em.endColor = {0.9f, 0.15f, 0.05f, 0.0f};
            em.startSize = 0.55f, em.endSize = 0.15f, em.additive = true, em.angularVelocity = 1.5f;
            break;
        case ParticlePreset::Smoke:
            em.rate = 18.0f, em.lifetimeMin = 2.5f, em.lifetimeMax = 4.0f, em.speedMin = 0.4f, em.speedMax = 0.9f;
            em.shape = EmitterShape3D::Cone, em.shapeRadius = 0.3f, em.coneAngle = math::radians(15.0f);
            em.gravity = {0.3f, 0.2f, 0.0f}, em.drag = 0.3f;
            em.startColor = {0.45f, 0.45f, 0.45f, 0.55f}, em.endColor = {0.7f, 0.7f, 0.7f, 0.0f};
            em.startSize = 0.4f, em.endSize = 1.8f, em.angularVelocity = 0.4f;
            break;
        case ParticlePreset::Sparks:
            em.rate = 0.0f, em.burst = 80, em.duration = 1.2f, em.lifetimeMin = 0.4f, em.lifetimeMax = 0.9f, em.speedMin = 3.0f, em.speedMax = 7.0f;
            em.shape = EmitterShape3D::Sphere, em.shapeRadius = 0.05f, em.gravity = {0.0f, -9.81f, 0.0f};
            em.startColor = {1.0f, 0.9f, 0.5f, 1.0f}, em.endColor = {1.0f, 0.4f, 0.1f, 0.0f};
            em.startSize = 0.08f, em.endSize = 0.02f, em.additive = true;
            break;
        case ParticlePreset::Magic:
            em.rate = 50.0f, em.lifetimeMin = 1.0f, em.lifetimeMax = 2.0f, em.speedMin = 0.2f, em.speedMax = 0.6f;
            em.shape = EmitterShape3D::Sphere, em.shapeRadius = 0.6f, em.gravity = {0.0f, 0.5f, 0.0f};
            em.startColor = {0.5f, 0.6f, 1.0f, 1.0f}, em.endColor = {0.9f, 0.4f, 1.0f, 0.0f};
            em.startSize = 0.15f, em.endSize = 0.0f, em.additive = true, em.angularVelocity = 3.0f;
            break;
        case ParticlePreset::Rain:
            em.rate = 400.0f, em.maxParticles = 3000, em.lifetimeMin = 1.2f, em.lifetimeMax = 1.5f;
            em.shape = EmitterShape3D::Box, em.boxSize = {20.0f, 0.1f, 20.0f}, em.coneAngle = 0.0f;
            em.gravity = {0.0f, -9.81f, 0.0f};
            em.startColor = {0.7f, 0.8f, 1.0f, 0.5f}, em.endColor = {0.7f, 0.8f, 1.0f, 0.4f};
            em.startSize = 0.05f, em.endSize = 0.05f;
            em.speedMin = -10.0f, em.speedMax = -8.0f;  // emitted downward (box emits along +Y)
            break;
        case ParticlePreset::Snow:
            em.rate = 120.0f, em.maxParticles = 3000, em.lifetimeMin = 6.0f, em.lifetimeMax = 8.0f, em.speedMin = -1.2f, em.speedMax = -0.6f;
            em.shape = EmitterShape3D::Box, em.boxSize = {20.0f, 0.1f, 20.0f}, em.drag = 0.2f, em.gravity = {0.3f, 0.0f, 0.1f};
            em.startColor = {1.0f, 1.0f, 1.0f, 0.9f}, em.endColor = {1.0f, 1.0f, 1.0f, 0.0f};
            em.startSize = 0.12f, em.endSize = 0.1f, em.angularVelocity = 1.0f;
            break;
        case ParticlePreset::Dust:
            em.rate = 25.0f, em.lifetimeMin = 3.0f, em.lifetimeMax = 5.0f, em.speedMin = 0.02f, em.speedMax = 0.15f;
            em.shape = EmitterShape3D::Box, em.boxSize = {6.0f, 2.0f, 6.0f}, em.drag = 0.1f;
            em.startColor = {1.0f, 0.95f, 0.8f, 0.0f}, em.endColor = {1.0f, 0.95f, 0.8f, 0.0f};
            em.startColor.w = 0.35f;
            em.startSize = 0.05f, em.endSize = 0.05f;
            break;
    }
}

} // namespace agr
