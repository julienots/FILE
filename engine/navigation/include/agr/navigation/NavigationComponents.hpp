#pragma once
#include "agr/math/Math.hpp"

namespace agr {

enum class NavPlane : i32 {
    Auto = 0,  ///< 3D navigation mesh when the scene has one, else the 2D grid.
    Grid2D = 1,  ///< XY plane (2D games): grid built from tilemaps and static 2D colliders.
    Mesh3D = 2,  ///< Ground (XZ): navigation mesh built from static 3D colliders.
};

/// Moves the entity along paths toward a destination set by a script
/// (entity:navigateTo(x, y [, z])). Moves the Transform: use it on entities without a body
/// or with a kinematic body.
struct NavAgentComponent {
    float speed = 3.5f;             ///< Units per second.
    float stoppingDistance = 0.05f; ///< Arrived when this close to the destination.
    bool faceMovement = true;       ///< Turns the entity toward where it goes (yaw in 3D, flipX-free in 2D: rotation only in 3D).
    NavPlane plane = NavPlane::Auto;
};

/// Scene-wide build parameters (first active one). Without it, defaults are used.
struct NavigationSettingsComponent {
    // 2D grid
    float gridCellSize = 0.5f;     ///< World units per cell.
    float agentRadius2D = 0.3f;    ///< Obstacles are grown by it.
    // 3D navigation mesh (Recast)
    float cellSize = 0.2f;
    float agentRadius = 0.4f;
    float agentHeight = 1.8f;
    float agentMaxClimb = 0.4f;
    float agentMaxSlope = 45.0f;   ///< Degrees.
};

void registerNavigationComponents();

} // namespace agr
