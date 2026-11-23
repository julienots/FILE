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
    /// Steers around the other agents and the NavObstacles (crowds): each agent takes half of
    /// the avoidance, like RVO. Off: walks its path exactly (agents go through each other).
    bool avoidance = true;
    float radius = 0.4f;            ///< Size used by the avoidance.
};

/// Something the agents steer around: a rolling boulder, a creature without NavAgent, a closed
/// door. With `carve`, it is also cut out of the navigation itself (paths go around it; the
/// navigation is rebuilt when it moves: for objects that move seldom).
struct NavObstacleComponent {
    float radius = 0.5f;
    float height = 2.0f;            ///< 3D carving: height of the cut cylinder.
    bool carve = false;
};

/// Shortcut that paths may take between two places not joined by walkable ground: a jump down, a
/// ladder, a gap to leap, a teleporter. It starts at the entity and ends at `end` (relative to it).
/// Agents cross it in a straight line. One way unless `bidirectional`.
struct NavLinkComponent {
    math::Vec3 end{2.0f, 0.0f, 0.0f};
    bool bidirectional = true;
    float cost = 1.0f;              ///< Multiplies its length in the path cost (2D grid).
};

/// Zone where walking costs more or is forbidden (Godot's navigation region travel cost): a box of
/// `size` centered on the entity (2D: its x / y rectangle). Agents go around a cost-3 swamp unless
/// the detour is more than 3 times longer; not walkable = a hole in the navigation.
struct NavAreaComponent {
    math::Vec3 size{4.0f, 2.0f, 4.0f};
    float cost = 3.0f;
    bool walkable = true;
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
    /// Larger agents (tanks, giants, vehicles): extra radii, ';'-separated ("0.8;1.5"). A grid and
    /// a mesh are built for each, with obstacles grown by that radius; every NavAgent uses the
    /// smallest one that fits its `radius` (the default sizes above if they do).
    std::string agentSizes;
    /// Rebuilds of the 3D mesh (geometry changed during the game) run on a worker thread: the
    /// old mesh keeps serving paths until the new one is ready (no frame hitch on big levels).
    /// The first build is always immediate.
    bool backgroundBuild = false;
    /// Static imported models (MeshRenderer with a model, no moving body) are walkable geometry
    /// too, like static colliders.
    bool meshRenderersAsSources = true;
    /// 3D mesh in tiles of `tileSize` x `tileSize` cells (0 = one piece). Tiles lift the size limit
    /// (big open worlds) and a geometry change (a door, a carved obstacle) only rebuilds the tiles
    /// it touches. 48..128 is typical.
    i32 tileSize = 0;
};

void registerNavigationComponents();

} // namespace agr
