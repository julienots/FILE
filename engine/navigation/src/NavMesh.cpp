#include "agr/navigation/NavMesh.hpp"

#include <algorithm>
#include <cstring>

#include <DetourNavMesh.h>
#include <DetourNavMeshBuilder.h>
#include <DetourNavMeshQuery.h>
#include <Recast.h>

#include "agr/core/Time.hpp"

namespace agr {

using math::Vec3;

struct NavMesh::Impl {
    dtNavMesh* mesh = nullptr;
    dtNavMeshQuery* query = nullptr;
    dtQueryFilter filter;
    ~Impl() { reset(); }
    void reset() {
        if (query) dtFreeNavMeshQuery(query);
        if (mesh) dtFreeNavMesh(mesh);
        query = nullptr;
        mesh = nullptr;
    }
};

NavMesh::NavMesh() : m(std::make_unique<Impl>()) {}
NavMesh::~NavMesh() = default;

void NavMesh::clear() {
    m->reset();
    m_stats = {};
}

bool NavMesh::empty() const { return m->query == nullptr; }

namespace {
struct RecastData {
    rcContext ctx{false};
    rcHeightfield* solid = nullptr;
    rcCompactHeightfield* chf = nullptr;
    rcContourSet* cset = nullptr;
    rcPolyMesh* pmesh = nullptr;
    rcPolyMeshDetail* dmesh = nullptr;
    ~RecastData() {
        rcFreeHeightField(solid);
        rcFreeCompactHeightfield(chf);
        rcFreeContourSet(cset);
        rcFreePolyMesh(pmesh);
        rcFreePolyMeshDetail(dmesh);
    }
};
} // namespace

Status NavMesh::build(const std::vector<Vec3>& vertices, const std::vector<u32>& indices, const NavMeshBuildSettings& s) {
    Stopwatch sw;
    clear();
    if (vertices.empty() || indices.size() < 3) return makeError(Error::Code::InvalidArgument, "no walkable geometry");
    const int nverts = static_cast<int>(vertices.size()), ntris = static_cast<int>(indices.size() / 3);
    std::vector<float> verts(vertices.size() * 3);
    for (usize i = 0; i < vertices.size(); ++i) {
        verts[i * 3] = vertices[i].x;
        verts[i * 3 + 1] = vertices[i].y;
        verts[i * 3 + 2] = vertices[i].z;
    }
    std::vector<int> tris(indices.begin(), indices.end());

    rcConfig cfg{};
    cfg.cs = std::max(s.cellSize, 0.02f);
    cfg.ch = std::max(s.cellHeight, 0.01f);
    cfg.walkableSlopeAngle = std::clamp(s.agentMaxSlope, 0.0f, 89.0f);
    cfg.walkableHeight = static_cast<int>(std::ceil(s.agentHeight / cfg.ch));
    cfg.walkableClimb = static_cast<int>(std::floor(s.agentMaxClimb / cfg.ch));
    cfg.walkableRadius = static_cast<int>(std::ceil(s.agentRadius / cfg.cs));
    cfg.maxEdgeLen = static_cast<int>(12.0f / cfg.cs);
    cfg.maxSimplificationError = 1.3f;
    cfg.minRegionArea = 8 * 8;
    cfg.mergeRegionArea = 20 * 20;
    cfg.maxVertsPerPoly = 6;
    cfg.detailSampleDist = cfg.cs * 6.0f;
    cfg.detailSampleMaxError = cfg.ch * 1.0f;
    rcCalcBounds(verts.data(), nverts, cfg.bmin, cfg.bmax);
    rcCalcGridSize(cfg.bmin, cfg.bmax, cfg.cs, &cfg.width, &cfg.height);
    if (static_cast<i64>(cfg.width) * cfg.height > 4096ll * 4096ll)
        return makeError(Error::Code::InvalidArgument, "navigation area too large for the cell size (raise cellSize)");

    RecastData r;
    r.solid = rcAllocHeightfield();
    if (!r.solid || !rcCreateHeightfield(&r.ctx, *r.solid, cfg.width, cfg.height, cfg.bmin, cfg.bmax, cfg.cs, cfg.ch))
        return makeError(Error::Code::Unknown, "navmesh: heightfield allocation failed");
    std::vector<unsigned char> areas(static_cast<usize>(ntris), 0);
    rcMarkWalkableTriangles(&r.ctx, cfg.walkableSlopeAngle, verts.data(), nverts, tris.data(), ntris, areas.data());
    if (!rcRasterizeTriangles(&r.ctx, verts.data(), nverts, tris.data(), areas.data(), ntris, *r.solid, cfg.walkableClimb))
        return makeError(Error::Code::Unknown, "navmesh: rasterization failed");
    rcFilterLowHangingWalkableObstacles(&r.ctx, cfg.walkableClimb, *r.solid);
    rcFilterLedgeSpans(&r.ctx, cfg.walkableHeight, cfg.walkableClimb, *r.solid);
    rcFilterWalkableLowHeightSpans(&r.ctx, cfg.walkableHeight, *r.solid);
    r.chf = rcAllocCompactHeightfield();
    if (!r.chf || !rcBuildCompactHeightfield(&r.ctx, cfg.walkableHeight, cfg.walkableClimb, *r.solid, *r.chf))
        return makeError(Error::Code::Unknown, "navmesh: compact heightfield failed");
    if (!rcErodeWalkableArea(&r.ctx, cfg.walkableRadius, *r.chf)) return makeError(Error::Code::Unknown, "navmesh: erosion failed");
    if (!rcBuildDistanceField(&r.ctx, *r.chf)) return makeError(Error::Code::Unknown, "navmesh: distance field failed");
    if (!rcBuildRegions(&r.ctx, *r.chf, 0, cfg.minRegionArea, cfg.mergeRegionArea))
        return makeError(Error::Code::Unknown, "navmesh: regions failed");
    r.cset = rcAllocContourSet();
    if (!r.cset || !rcBuildContours(&r.ctx, *r.chf, cfg.maxSimplificationError, cfg.maxEdgeLen, *r.cset))
        return makeError(Error::Code::Unknown, "navmesh: contours failed");
    r.pmesh = rcAllocPolyMesh();
    if (!r.pmesh || !rcBuildPolyMesh(&r.ctx, *r.cset, cfg.maxVertsPerPoly, *r.pmesh))
        return makeError(Error::Code::Unknown, "navmesh: polygon mesh failed");
    r.dmesh = rcAllocPolyMeshDetail();
    if (!r.dmesh || !rcBuildPolyMeshDetail(&r.ctx, *r.pmesh, *r.chf, cfg.detailSampleDist, cfg.detailSampleMaxError, *r.dmesh))
        return makeError(Error::Code::Unknown, "navmesh: detail mesh failed");
    if (r.pmesh->npolys == 0) return makeError(Error::Code::NotFound, "no walkable surface (check slopes, agent size and geometry)");
    if (r.pmesh->nverts >= 0xffff) return makeError(Error::Code::InvalidArgument, "navmesh: too many vertices for one tile");

    for (int i = 0; i < r.pmesh->npolys; ++i) {
        if (r.pmesh->areas[i] == RC_WALKABLE_AREA) r.pmesh->areas[i] = 0;
        r.pmesh->flags[i] = 1;  // walkable
    }
    dtNavMeshCreateParams params{};
    params.verts = r.pmesh->verts;
    params.vertCount = r.pmesh->nverts;
    params.polys = r.pmesh->polys;
    params.polyAreas = r.pmesh->areas;
    params.polyFlags = r.pmesh->flags;
    params.polyCount = r.pmesh->npolys;
    params.nvp = r.pmesh->nvp;
    params.detailMeshes = r.dmesh->meshes;
    params.detailVerts = r.dmesh->verts;
    params.detailVertsCount = r.dmesh->nverts;
    params.detailTris = r.dmesh->tris;
    params.detailTriCount = r.dmesh->ntris;
    params.walkableHeight = s.agentHeight;
    params.walkableRadius = s.agentRadius;
    params.walkableClimb = s.agentMaxClimb;
    rcVcopy(params.bmin, r.pmesh->bmin);
    rcVcopy(params.bmax, r.pmesh->bmax);
    params.cs = cfg.cs;
    params.ch = cfg.ch;
    params.buildBvTree = true;
    unsigned char* navData = nullptr;
    int navDataSize = 0;
    if (!dtCreateNavMeshData(&params, &navData, &navDataSize)) return makeError(Error::Code::Unknown, "navmesh: Detour data failed");
    m->mesh = dtAllocNavMesh();
    if (!m->mesh || dtStatusFailed(m->mesh->init(navData, navDataSize, DT_TILE_FREE_DATA))) {
        dtFree(navData);
        m->reset();
        return makeError(Error::Code::Unknown, "navmesh: Detour init failed");
    }
    m->query = dtAllocNavMeshQuery();
    if (!m->query || dtStatusFailed(m->query->init(m->mesh, 2048))) {
        m->reset();
        return makeError(Error::Code::Unknown, "navmesh: query init failed");
    }
    m->filter.setIncludeFlags(0xffff);
    m->filter.setExcludeFlags(0);
    m_stats.polygons = static_cast<u32>(r.pmesh->npolys);
    m_stats.vertices = static_cast<u32>(r.pmesh->nverts);
    m_stats.inputTriangles = static_cast<u32>(ntris);
    m_stats.buildMs = sw.elapsedMilliseconds();
    return {};
}

bool NavMesh::nearestPoint(Vec3 p, Vec3& out, Vec3 extents) const {
    if (empty()) return false;
    const float pos[3] = {p.x, p.y, p.z}, ext[3] = {extents.x, extents.y, extents.z};
    dtPolyRef ref = 0;
    float nearest[3];
    if (dtStatusFailed(m->query->findNearestPoly(pos, ext, &m->filter, &ref, nearest)) || !ref) return false;
    out = {nearest[0], nearest[1], nearest[2]};
    return true;
}

bool NavMesh::findPath(Vec3 from, Vec3 to, std::vector<Vec3>& out, bool* complete) const {
    out.clear();
    if (complete) *complete = false;
    if (empty()) return false;
    const float ext[3] = {2.0f, 4.0f, 2.0f};
    const float spos[3] = {from.x, from.y, from.z}, epos[3] = {to.x, to.y, to.z};
    dtPolyRef startRef = 0, endRef = 0;
    float sNear[3], eNear[3];
    m->query->findNearestPoly(spos, ext, &m->filter, &startRef, sNear);
    if (!startRef) return false;
    m->query->findNearestPoly(epos, ext, &m->filter, &endRef, eNear);
    if (!endRef) {  // goal far from the mesh: search wider, then end as close as possible
        const float wide[3] = {16.0f, 16.0f, 16.0f};
        m->query->findNearestPoly(epos, wide, &m->filter, &endRef, eNear);
        if (!endRef) return false;
    }
    constexpr int kMaxPolys = 512;
    dtPolyRef polys[kMaxPolys];
    int npolys = 0;
    const dtStatus st = m->query->findPath(startRef, endRef, sNear, eNear, &m->filter, polys, &npolys, kMaxPolys);
    if (dtStatusFailed(st) || npolys == 0) return false;
    float target[3];
    rcVcopy(target, eNear);
    const bool reachedGoal = polys[npolys - 1] == endRef;
    if (!reachedGoal) m->query->closestPointOnPoly(polys[npolys - 1], eNear, target, nullptr);  // partial path
    float straight[kMaxPolys * 3];
    int nstraight = 0;
    m->query->findStraightPath(sNear, target, polys, npolys, straight, nullptr, nullptr, &nstraight, kMaxPolys);
    for (int i = 0; i < nstraight; ++i) out.push_back({straight[i * 3], straight[i * 3 + 1], straight[i * 3 + 2]});
    if (complete) *complete = reachedGoal && !dtStatusDetail(st, DT_PARTIAL_RESULT);
    return !out.empty();
}

std::vector<Vec3> NavMesh::debugEdges() const {
    std::vector<Vec3> out;
    if (empty()) return out;
    const dtNavMesh& mesh = *m->mesh;
    for (int t = 0; t < mesh.getMaxTiles(); ++t) {
        const dtMeshTile* tile = mesh.getTile(t);
        if (!tile || !tile->header) continue;
        for (int p = 0; p < tile->header->polyCount; ++p) {
            const dtPoly& poly = tile->polys[p];
            for (int v = 0; v < poly.vertCount; ++v) {
                const float* a = &tile->verts[poly.verts[v] * 3];
                const float* b = &tile->verts[poly.verts[(v + 1) % poly.vertCount] * 3];
                out.push_back({a[0], a[1], a[2]});
                out.push_back({b[0], b[1], b[2]});
            }
        }
    }
    return out;
}

} // namespace agr
