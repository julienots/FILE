#pragma once
// Onion skinning for the animation editor: where the animated objects are a few steps before and
// after the current time (outlines and the trajectory drawn by the viewport).
#include <vector>

#include "agr/animation/AnimationSystem.hpp"
#include "agr/editor/Picking.hpp"

namespace agr::editor {

struct OnionGhost {
    ecs::Entity entity{};
    i32 offset = 0;       ///< -before..-1 (past), 1..after (future).
    math::Aabb bounds;    ///< World bounds of its sprite / mesh at that time.
    math::Vec3 position;  ///< World position of the entity at that time (trajectory).
};

/// Previews `clip` on `root` at time - k * step and time + k * step (k = 1..before / after, clamped
/// to the clip), records every animated entity that draws something (sprite, mesh, tilemap), then
/// previews `time` again. Ghosts are ordered by offset.
std::vector<OnionGhost> onionSkin(Scene& scene, AnimationSystem& anim, ecs::Entity root, const AnimationClip& clip, float time, float step,
                                  u32 before, u32 after, PickAssets assets = {});

} // namespace agr::editor
