#pragma once
#include "agr/render/RenderComponents.hpp"
#include "agr/scene/Scene.hpp"

namespace agr {

/// Simulates every ParticleEmitter2D of a scene (runtime state lives in
/// ParticleState2DComponent, created on demand and never serialized).
struct ParticleStats2D {
    u32 emitters = 0;
    u32 particles = 0;
    u32 spawned = 0;
};
ParticleStats2D simulateParticles2D(Scene& scene, float dt);

/// Restarts an emitter (new cycle, particles cleared).
void restartParticles2D(Scene& scene, ecs::Entity emitter);

} // namespace agr
