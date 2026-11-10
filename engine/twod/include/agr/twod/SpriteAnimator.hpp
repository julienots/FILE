#pragma once
#include <string>

#include "agr/render/SpriteSheet.hpp"
#include "agr/scene/Scene.hpp"

namespace agr {

/// Plays a clip of the sprite sheet referenced by the entity's SpriteRenderer
/// (frame-by-frame animation). Requires a SpriteRenderer with a sheet.
struct SpriteAnimatorComponent {
    std::string clip;          ///< Clip name in the sheet.
    float speed = 1.0f;        ///< Playback rate multiplier.
    bool playing = true;
    float time = 0.0f;         ///< Runtime (not serialized).
    std::string currentClip;   ///< Runtime: restarts the clock when `clip` changes.
};

void registerSpriteAnimatorComponent();

/// Advances every animator by `dt` seconds and writes SpriteRenderer::frame.
/// Returns the number of animators updated.
u32 updateSpriteAnimators(Scene& scene, SpriteSheetLibrary& sheets, float dt);

/// Frame index of a clip at time t (loops or holds the last frame).
u32 clipFrameAt(const SpriteClip& clip, float time);

} // namespace agr
