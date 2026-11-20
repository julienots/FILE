#pragma once
#include "agr/scene/Scene.hpp"

namespace agr {

/// Two-bone inverse kinematics for 2D skeletons (arms, legs): placed on the first Bone2D (upper
/// arm, thigh), it turns that bone and its first Bone2D child (forearm, shin) so that the tip of
/// the child reaches the target entity (a hand target, a foot on the ground). Solved after the
/// animations, every frame of the game.
struct TwoBoneIK2DComponent {
    ecs::Entity target{};
    bool flipBend = false;  ///< Bends the other way (elbow / knee direction).
    float weight = 1.0f;    ///< 0..1: blends with the animated pose.
    bool enabled = true;
};

/// Long 2D chain IK (FABRIK) on the root Bone2D of `boneCount` bones (tail, rope, tentacle): the
/// tip of the last bone reaches `target`; `maxAngle` limits each joint's bend.
struct ChainIK2DComponent {
    ecs::Entity target{};
    i32 boneCount = 3;
    i32 iterations = 12;
    float maxAngle = 3.1416f;
    float weight = 1.0f;
    bool enabled = true;
};

void registerIK2DComponents();

/// Solves every enabled TwoBoneIK2D of the scene (local rotations of the two bones).
/// Returns the number of chains solved.
u32 solveIK2D(Scene& scene);

} // namespace agr
