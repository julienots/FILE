#pragma once
#include "agr/scene/Scene.hpp"

namespace agr {

/// Two-bone inverse kinematics for 3D skeletons (arms, legs of imported models): on the upper
/// bone (upper arm, thigh), turns it and `mid` (forearm, shin) so that `tip` (hand, foot)
/// reaches `target`; the elbow / knee bends toward `pole` when set. `mid` and `tip` default to
/// the first child and grandchild. Solved after the animations (feet on uneven ground, hands on
/// a ledge or a weapon).
struct TwoBoneIK3DComponent {
    ecs::Entity target{};
    ecs::Entity pole{};  ///< Optional: the elbow / knee points toward it.
    ecs::Entity mid{};
    ecs::Entity tip{};
    float weight = 1.0f;  ///< 0..1: blends with the animated pose.
    bool enabled = true;
};

/// Turns a bone (head, eyes, a turret) toward a target, limited to `maxAngle` from its
/// animated direction, after the animations.
struct LookAtIK3DComponent {
    ecs::Entity target{};
    math::Vec3 forwardAxis{0.0f, 0.0f, 1.0f};  ///< Local axis that should point at the target.
    float maxAngle = 1.4f;                      ///< Radians.
    float weight = 1.0f;
    bool enabled = true;
};

void registerIK3DComponents();

/// Solves every enabled TwoBoneIK3D then LookAtIK3D of the scene; returns how many were solved.
u32 solveIK3D(Scene& scene);

/// One two-bone solve (exposed for tests and tools): returns false when the chain is invalid.
bool solveTwoBoneIK3D(Scene& scene, ecs::Entity upper, const TwoBoneIK3DComponent& ik);

} // namespace agr
