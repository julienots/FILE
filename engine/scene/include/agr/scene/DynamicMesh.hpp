#pragma once
// A mesh whose vertices change at run time (soft bodies, procedural geometry): drawn by the 3D
// renderer with the entity's MeshRenderer material (texture, color, .agrmat). Not saved: the
// system that fills it (soft body physics, a script) rebuilds it.
#include <vector>

#include "agr/core/Types.hpp"
#include "agr/math/Math.hpp"

namespace agr {

struct DynamicMeshComponent {
    std::vector<math::Vec3> positions;
    std::vector<math::Vec2> uvs;  ///< Optional (same count as positions).
    std::vector<u32> indices;     ///< Triangles.
    bool worldSpace = false;      ///< Positions in world space (soft bodies) instead of the entity's space.
    bool doubleSided = false;     ///< Cloth: both faces lit and drawn.
    u32 version = 0;              ///< Bumped by the writer when positions change.
};

} // namespace agr
