#pragma once
#include <string>
#include <unordered_map>
#include <vector>

#include "agr/navigation/NavGrid.hpp"
#include "agr/navigation/NavMesh.hpp"
#include "agr/navigation/NavigationComponents.hpp"
#include "agr/scene/Scene.hpp"

namespace agr {

enum class NavStatus : u8 { Idle, Moving, Arrived, Unreachable };
const char* enumName(NavStatus s);

struct NavigationStats {
    u32 gridCells = 0, gridBlocked = 0;
    u32 meshPolygons = 0;
    u32 sourceShapes2D = 0, sourceShapes3D = 0;
    u32 agentsMoving = 0;
    u32 builds = 0;
    f64 lastBuildMs = 0.0;
    std::string lastError;  ///< Of the last 3D build (empty when fine or no 3D geometry).
};

/// Builds the 2D grid and the 3D navigation mesh from the scene's static geometry
/// (rebuilt automatically when it changes) and moves NavAgent entities.
class NavigationSystem {
public:
    /// Rebuilds when the static geometry or the settings changed (cheap check otherwise).
    void ensureBuilt(Scene& scene);
    void rebuild(Scene& scene);
    /// Moves the agents (dt in seconds).
    void update(Scene& scene, float dt);
    /// Forgets agents and built data (scene changed).
    void reset();

    /// Computes a path and starts moving. False when no path exists (status Unreachable).
    bool navigateTo(Scene& scene, ecs::Entity agent, math::Vec3 destination);
    void stop(ecs::Entity agent);
    NavStatus status(ecs::Entity agent) const;
    /// Remaining corners of the agent's path (empty if none).
    std::vector<math::Vec3> path(ecs::Entity agent) const;
    /// Path between two points. `use3D`: navigation mesh (XZ ground) instead of the 2D grid.
    bool findPath(Scene& scene, math::Vec3 from, math::Vec3 to, bool use3D, std::vector<math::Vec3>& out);

    const NavGrid& grid() const { return m_grid; }
    const NavMesh& mesh() const { return m_mesh; }
    const NavigationStats& stats() const { return m_stats; }

private:
    struct Agent {
        std::vector<math::Vec3> corners;
        usize next = 0;
        NavStatus status = NavStatus::Idle;
        bool use3D = false;
        bool complete = true;  ///< The path reaches the destination (else: as close as possible).
        math::Vec3 destination{0.0f};
    };
    bool uses3D(Scene& scene, ecs::Entity e) const;
    u64 geometryHash(Scene& scene) const;

    NavGrid m_grid;
    NavMesh m_mesh;
    std::unordered_map<u64, Agent> m_agents;
    NavigationStats m_stats;
    u64 m_hash = 0;
    u64 m_sceneGeneration = 0;
    const Scene* m_scene = nullptr;
    u32 m_checkCountdown = 0;
};

} // namespace agr
