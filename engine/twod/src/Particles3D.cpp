#include <algorithm>
#include <cmath>
#include <random>

#include "agr/twod/Particles2D.hpp"

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

void spawn(const ParticleEmitter3DComponent& em, ParticleState3DComponent& st, const math::Mat4& world, u32 count, u32& spawned) {
    for (u32 i = 0; i < count && st.particles.size() < em.maxParticles; ++i) {
        Vec3 pos{0.0f}, dir{0.0f, 1.0f, 0.0f};
        switch (em.shape) {
            case EmitterShape3D::Point: break;
            case EmitterShape3D::Sphere: {
                dir = randomUnit(st.rng);
                pos = dir * (em.shapeRadius * std::cbrt(uniform(st.rng)));
                break;
            }
            case EmitterShape3D::Box:
                pos = {range(st.rng, -0.5f, 0.5f) * em.boxSize.x, range(st.rng, -0.5f, 0.5f) * em.boxSize.y, range(st.rng, -0.5f, 0.5f) * em.boxSize.z};
                break;
            case EmitterShape3D::Cone: {
                // Direction within the cone around +Y, start on the base disc.
                const float cosMax = std::cos(std::clamp(em.coneAngle, 0.0f, math::kPi));
                const float c = range(st.rng, cosMax, 1.0f), sn = std::sqrt(std::max(0.0f, 1.0f - c * c)), a = range(st.rng, 0.0f, 2.0f * math::kPi);
                dir = {sn * std::cos(a), c, sn * std::sin(a)};
                const float r = em.shapeRadius * std::sqrt(uniform(st.rng)), b = range(st.rng, 0.0f, 2.0f * math::kPi);
                pos = {r * std::cos(b), 0.0f, r * std::sin(b)};
                break;
            }
        }
        Particle3D p;
        const Vec3 velocity = dir * range(st.rng, em.speedMin, std::max(em.speedMin, em.speedMax));
        if (em.worldSpace) {
            p.position = world.transformPoint(pos);
            p.velocity = world.transformVector(velocity);
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
    std::vector<ecs::Entity> missing;
    reg.view<ParticleEmitter3DComponent>().each([&](ecs::Entity e, ParticleEmitter3DComponent&) {
        if (!reg.has<ParticleState3DComponent>(e)) missing.push_back(e);
    });
    for (ecs::Entity e : missing) reg.emplace<ParticleState3DComponent>(e);
    reg.view<ParticleEmitter3DComponent, ParticleState3DComponent, WorldTransformComponent>().each(
        [&](ecs::Entity, ParticleEmitter3DComponent& em, ParticleState3DComponent& st, WorldTransformComponent& w) {
            ++stats.emitters;
            if (!w.activeInHierarchy) return;
            if (!st.started) {
                st.started = true;
                st.time = 0.0f;
                st.emitAccumulator = 0.0f;
                st.rng = em.seed ? em.seed : static_cast<u32>(std::random_device{}()) | 1u;
                if (em.emitting) spawn(em, st, w.matrix, em.burst, stats.spawned);
            }
            if (dt > 0.0f) {
                const float damping = std::max(0.0f, 1.0f - em.drag * dt);
                // Gravity is in world units: in local space, bring it into the emitter's frame.
                const Vec3 g = em.worldSpace ? em.gravity : math::inverse(w.matrix).transformVector(em.gravity);
                for (usize i = 0; i < st.particles.size();) {
                    Particle3D& p = st.particles[i];
                    p.age += dt;
                    if (p.age >= p.lifetime) {
                        p = st.particles.back();
                        st.particles.pop_back();
                        continue;
                    }
                    p.velocity = (p.velocity + g * dt) * damping;
                    p.position = p.position + p.velocity * dt;
                    p.rotation += p.angularVelocity * dt;
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
