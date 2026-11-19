#pragma once
#include <vector>

#include "agr/math/Math.hpp"
#include "agr/scene/Scene.hpp"

namespace agr {

/// Copies the current local transform of every Bone2D (below `root`, or in the whole scene)
/// to its rest pose: the pose the Skin2D parts are drawn in. The editor does it continuously
/// in edit mode; code building a skeleton calls it once the bones are placed.
void captureBone2DRest(Scene& scene, ecs::Entity root = {});

/// The nearest ancestor (or the entity itself) with a Skeleton2D, or null.
ecs::Entity skeleton2DOf(Scene& scene, ecs::Entity e);

/// World positions of the grid points of a Skin2D, (columns + 1) x (rows + 1), row by row from
/// the bottom-left. Uses the WorldTransform matrices (call Scene::updateTransforms first).
/// Each point follows the bones of its skeleton with automatic weights (4 nearest bones at
/// rest, inverse distance to the power `stiffness`). Returns false without a Skin2D.
bool skin2DPoints(Scene& scene, ecs::Entity skin, std::vector<math::Vec3>& out);

} // namespace agr
