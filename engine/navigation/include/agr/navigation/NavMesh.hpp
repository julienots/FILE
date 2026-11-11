#pragma once
// 3D navigation mesh (Recast) and path queries (Detour), built from world-space triangles.
#include <memory>
#include <vector>

#include "agr/core/Result.hpp"
#include "agr/core/Types.hpp"
#include "agr/math/Math.hpp"

namespace agr {

struct NavMeshBuildSettings {
    float cellSize = 0.2f;      ///< Horizontal voxel size (meters): precision vs build time.
    float cellHeight = 0.1f;
    float agentRadius = 0.4f;   ///< Walls are kept this far from the paths.
    float agentHeight = 1.8f;   ///< Minimum ceiling height.
    float agentMaxClimb = 0.4f; ///< Steps the agent can walk up.
    float agentMaxSlope = 45.0f;  ///< Degrees.
};

struct NavMeshStats {
    u32 polygons = 0;
    u32 vertices = 0;
    u32 inputTriangles = 0;
    f64 buildMs = 0.0;
};

class NavMesh {
public:
    NavMesh();
    ~NavMesh();
    NavMesh(const NavMesh&) = delete;
    NavMesh& operator=(const NavMesh&) = delete;

    /// Builds from triangles (indices into `vertices`, counter-clockwise seen from above
    /// for walkable surfaces). Replaces the previous mesh; an error leaves it empty.
    Status build(const std::vector<math::Vec3>& vertices, const std::vector<u32>& indices, const NavMeshBuildSettings& settings);
    void clear();
    bool empty() const;

    /// Straight path (corners) from `from` to `to`, both moved onto the mesh (within the
    /// search extents). When the goal is not reachable, the path ends at the closest
    /// reachable point and `complete` is false. False if `from` is not near the mesh.
    bool findPath(math::Vec3 from, math::Vec3 to, std::vector<math::Vec3>& out, bool* complete = nullptr) const;
    /// Closest point on the mesh (within `extents` around p).
    bool nearestPoint(math::Vec3 p, math::Vec3& out, math::Vec3 extents = {2.0f, 4.0f, 2.0f}) const;
    /// Polygon edges (pairs of points), for debug display.
    std::vector<math::Vec3> debugEdges() const;
    const NavMeshStats& stats() const { return m_stats; }

private:
    struct Impl;
    std::unique_ptr<Impl> m;
    NavMeshStats m_stats;
};

} // namespace agr
