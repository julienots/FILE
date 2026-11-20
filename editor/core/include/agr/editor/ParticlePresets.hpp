#pragma once
#include "agr/render/RenderComponents.hpp"

namespace agr::editor {

/// Ready-made effects for particle emitters, built on the textures and curves of
/// assets/particles (tools/generate_particle_art.py), for 2D and 3D emitters alike.
enum class ParticlePreset : u8 { Fire, Smoke, Explosion, Sparks, Magic, Dust, Snow };
inline constexpr ParticlePreset kParticlePresets[] = {ParticlePreset::Fire,  ParticlePreset::Smoke, ParticlePreset::Explosion, ParticlePreset::Sparks,
                                                      ParticlePreset::Magic, ParticlePreset::Dust,  ParticlePreset::Snow};
const char* particlePresetName(ParticlePreset preset);  ///< French UI name ("Feu"...)

void applyParticlePreset(ParticlePreset preset, ParticleEmitter2DComponent& emitter);
void applyParticlePreset(ParticlePreset preset, ParticleEmitter3DComponent& emitter);

} // namespace agr::editor
