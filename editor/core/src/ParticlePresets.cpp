#include "agr/editor/ParticlePresets.hpp"

namespace agr::editor {

namespace {

/// Settings shared by the 2D and 3D emitters; `up` / `spread` / `gravity` along the emitter's up.
struct Look {
    bool loop = true;
    float duration = 2.0f, rate = 20.0f;
    u32 burst = 0, maxParticles = 300;
    float lifeMin = 1.0f, lifeMax = 1.5f, speedMin = 1.0f, speedMax = 2.0f;
    float spread = 0.3f;     ///< Half angle (radians) around the emitter's up.
    float radius = 0.0f;     ///< Spawn radius.
    float gravity = 0.0f;    ///< Along up (positive rises).
    float drag = 0.0f;
    float size = 0.3f, endSize = 0.1f;
    math::Color start{1, 1, 1, 1}, end{1, 1, 1, 0};
    float spin = 0.0f;
    const char* texture = "";
    const char* curves = "";
    i32 columns = 1, rows = 1;
    bool additive = false;
    bool down = false;  ///< Snow, rain: fall from a wide area.
    float stretch = 0.0f;
};

Look lookOf(ParticlePreset p) {
    Look l;
    switch (p) {
        case ParticlePreset::Fire:
            l.rate = 60.0f, l.lifeMin = 0.7f, l.lifeMax = 1.2f, l.speedMin = 1.2f, l.speedMax = 2.4f, l.spread = math::radians(10.0f);
            l.radius = 0.25f, l.gravity = 1.5f, l.drag = 0.8f, l.size = 0.85f, l.spin = 0.5f;
            l.texture = "assets://particles/flame.png", l.curves = "assets://particles/fire.agrparticle", l.additive = true;
            break;
        case ParticlePreset::Smoke:
            l.rate = 12.0f, l.lifeMin = 2.0f, l.lifeMax = 3.5f, l.speedMin = 0.3f, l.speedMax = 0.7f, l.spread = math::radians(15.0f);
            l.radius = 0.2f, l.gravity = 0.3f, l.drag = 0.3f, l.size = 1.2f, l.spin = 0.4f;
            l.texture = "assets://particles/smoke.png", l.curves = "assets://particles/smoke.agrparticle";
            break;
        case ParticlePreset::Explosion:
            l.duration = 1.6f, l.rate = 0.0f, l.burst = 6, l.lifeMin = 0.9f, l.lifeMax = 1.2f, l.speedMin = 0.2f, l.speedMax = 0.8f;
            l.spread = math::radians(180.0f), l.radius = 0.15f, l.size = 2.2f, l.spin = 0.6f;
            l.texture = "assets://particles/explosion.png", l.curves = "assets://particles/explosion.agrparticle", l.columns = l.rows = 4;
            break;
        case ParticlePreset::Sparks:
            l.duration = 1.0f, l.rate = 0.0f, l.burst = 40, l.lifeMin = 0.4f, l.lifeMax = 0.9f, l.speedMin = 3.0f, l.speedMax = 7.0f;
            l.spread = math::radians(75.0f), l.gravity = -9.0f, l.drag = 1.0f, l.size = 0.3f;
            l.texture = "assets://particles/spark.png", l.curves = "assets://particles/sparks.agrparticle", l.additive = true;
            l.stretch = 0.035f;
            break;
        case ParticlePreset::Magic:
            l.rate = 25.0f, l.lifeMin = 1.0f, l.lifeMax = 1.8f, l.speedMin = 0.3f, l.speedMax = 1.0f, l.spread = math::radians(180.0f);
            l.radius = 0.5f, l.gravity = 0.4f, l.size = 0.35f, l.spin = 2.0f;
            l.texture = "assets://particles/star.png", l.curves = "assets://particles/magic.agrparticle", l.additive = true;
            break;
        case ParticlePreset::Dust:
            l.rate = 6.0f, l.lifeMin = 1.5f, l.lifeMax = 2.5f, l.speedMin = 0.2f, l.speedMax = 0.5f, l.spread = math::radians(70.0f);
            l.radius = 0.6f, l.gravity = 0.05f, l.size = 0.8f, l.spin = 0.3f;
            l.texture = "assets://particles/smoke.png", l.curves = "assets://particles/dust.agrparticle";
            break;
        case ParticlePreset::Snow:
            l.rate = 30.0f, l.maxParticles = 400, l.lifeMin = 4.0f, l.lifeMax = 6.0f, l.speedMin = 0.0f, l.speedMax = 0.2f, l.spread = math::radians(180.0f);
            l.radius = 8.0f, l.gravity = -0.35f, l.drag = 0.5f, l.size = 0.12f, l.endSize = 0.1f, l.spin = 1.0f;
            l.start = {1, 1, 1, 0.9f}, l.end = {0.9f, 0.95f, 1, 0}, l.down = true;
            break;
        case ParticlePreset::Rain:
            l.rate = 220.0f, l.maxParticles = 800, l.lifeMin = l.lifeMax = 1.2f, l.speedMin = 0.0f, l.speedMax = 0.5f, l.spread = math::radians(180.0f);
            l.radius = 8.0f, l.gravity = -14.0f, l.size = 0.05f, l.endSize = 0.05f, l.stretch = 0.05f;
            l.start = {0.7f, 0.8f, 1.0f, 0.6f}, l.end = {0.7f, 0.8f, 1.0f, 0.45f}, l.down = true;
            break;
    }
    return l;
}

template <typename E>
void common(const Look& l, E& em) {
    em.emitting = true;
    em.loop = l.loop;
    em.duration = l.duration;
    em.rate = l.rate;
    em.burst = l.burst;
    em.maxParticles = l.maxParticles;
    em.lifetimeMin = l.lifeMin, em.lifetimeMax = l.lifeMax;
    em.speedMin = l.speedMin, em.speedMax = l.speedMax;
    em.drag = l.drag;
    em.startSize = l.size, em.endSize = l.endSize;
    em.startColor = l.start, em.endColor = l.end;
    em.angularVelocity = l.spin;
    em.texture = l.texture;
    em.curves = l.curves;
    em.flipbookColumns = l.columns, em.flipbookRows = l.rows, em.flipbookCycles = 1.0f;
    em.additive = l.additive;
    em.velocityStretch = l.stretch;
}

} // namespace

const char* particlePresetName(ParticlePreset preset) {
    switch (preset) {
        case ParticlePreset::Fire: return "Feu";
        case ParticlePreset::Smoke: return "Fumée";
        case ParticlePreset::Explosion: return "Explosion";
        case ParticlePreset::Sparks: return "Étincelles";
        case ParticlePreset::Magic: return "Magie";
        case ParticlePreset::Dust: return "Poussière";
        case ParticlePreset::Snow: return "Neige";
        case ParticlePreset::Rain: return "Pluie";
    }
    return "";
}

void applyParticlePreset(ParticlePreset preset, ParticleEmitter2DComponent& em) {
    const Look l = lookOf(preset);
    common(l, em);
    em.direction = math::radians(90.0f);
    em.spread = l.spread;
    em.spawnRadius = l.radius;
    em.gravity = {0.0f, l.gravity};
}

void applyParticlePreset(ParticlePreset preset, ParticleEmitter3DComponent& em) {
    const Look l = lookOf(preset);
    common(l, em);
    if (l.down) {  // a wide flat area above the scene
        em.shape = EmitterShape3D::Box;
        em.boxSize = {l.radius * 2.0f, 0.1f, l.radius * 2.0f};
    } else if (l.spread >= math::radians(179.0f)) {
        em.shape = EmitterShape3D::Sphere;
        em.shapeRadius = std::max(l.radius, 0.05f);
    } else {
        em.shape = EmitterShape3D::Cone;
        em.shapeRadius = l.radius;
        em.coneAngle = l.spread;
    }
    em.gravity = {0.0f, l.gravity, 0.0f};
}

} // namespace agr::editor
