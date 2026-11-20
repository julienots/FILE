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
    u32 collisions = 0;  ///< Particle hits (ground, ParticleCollider) this step.
};
ParticleStats2D simulateParticles2D(Scene& scene, float dt);

/// Restarts an emitter (new cycle, particles cleared).
void restartParticles2D(Scene& scene, ecs::Entity emitter);
/// Emits `count` particles of the emitter at a world position (impacts, sub-emitters, scripts).
u32 burstParticles2D(Scene& scene, ecs::Entity emitter, math::Vec3 position, u32 count);
u32 burstParticles3D(Scene& scene, ecs::Entity emitter, math::Vec3 position, u32 count);
/// Records the points of the Trail components (TrailState, created on demand).
u32 updateTrails(Scene& scene, float dt);

/// Same for ParticleEmitter3D (state in ParticleState3DComponent, never serialized).
ParticleStats2D simulateParticles3D(Scene& scene, float dt);
void restartParticles3D(Scene& scene, ecs::Entity emitter);

/// Starting points for 3D emitters (editor presets).
enum class ParticlePreset : u8 { Fire, Smoke, Sparks, Magic, Rain, Snow, Dust };
inline constexpr ParticlePreset kParticlePresets[] = {ParticlePreset::Fire,  ParticlePreset::Smoke, ParticlePreset::Sparks, ParticlePreset::Magic,
                                                       ParticlePreset::Rain,  ParticlePreset::Snow,  ParticlePreset::Dust};
const char* particlePresetName(ParticlePreset preset);
/// Replaces the emitter's settings by the preset's (keeps its texture).
void applyParticlePreset(ParticleEmitter3DComponent& emitter, ParticlePreset preset);

} // namespace agr
