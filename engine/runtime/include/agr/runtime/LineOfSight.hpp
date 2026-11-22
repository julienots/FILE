#pragma once
// Line-of-sight query shared between plugins without linking them: the 3D physics registers it
// as an engine service, audio occlusion (and others) use it when it exists.
#include <functional>

#include "agr/ecs/Entity.hpp"
#include "agr/math/Math.hpp"

namespace agr {

struct LineOfSightService {
    /// True when a collider other than `ignore` (and its children) lies between the points.
    std::function<bool(math::Vec3 from, math::Vec3 to, ecs::Entity ignore)> blocked;
};

} // namespace agr
