#include "agr/navigation/NavMesh.hpp"

#include <algorithm>
#include <cstring>
#include <unordered_map>

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
    // Input kept for tile rebuilds.
    std::vector<float> verts;
    std::vector<int> tris;
    std::vector<NavMeshLink> links;
    std::vector<NavMeshArea> areas;
    NavMeshBuildSettings settings;
    bool tiled = false;
    float orig[3]{}, top = 0.0f, bottom = 0.0f;
    int tilesX = 0, tilesY = 0;
    float tileWorld = 0.0f;
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
    m->tiled = false;
    m->verts.clear();
    m->tris.clear();
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

rcConfig baseConfig(const NavMeshBuildSettings& s) {
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
    return cfg;
}

/// Detour data of one tile (or of the whole level when `tile` is false). `data` stays null when
/// the tile holds no polygon (not an error for a tile).
Status buildTileData(const std::vector<float>& verts, const std::vector<int>& allTris, const std::vector<NavMeshLink>& links,
                     const std::vector<NavMeshArea>& areaBoxes, const NavMeshBuildSettings& s, rcConfig cfg, bool tile, int tx, int ty,
                     unsigned char** data, int* dataSize, int* polys, int* vertsOut) {
    *data = nullptr;
    *dataSize = 0;
    *polys = 0;
    const int nverts = static_cast<int>(verts.size() / 3);
    // Tile: keep the triangles overlapping the tile grown by its border.
    std::vector<int> tileTris;
    const std::vector<int>* tris = &allTris;
    float tileMin[3]{}, tileMax[3]{};
    if (tile) {
        rcVcopy(tileMin, cfg.bmin);
        rcVcopy(tileMax, cfg.bmax);
        cfg.borderSize = cfg.walkableRadius + 3;
        cfg.width = cfg.tileSize + cfg.borderSize * 2;
        cfg.height = cfg.tileSize + cfg.borderSize * 2;
        cfg.bmin[0] -= cfg.borderSize * cfg.cs;
        cfg.bmin[2] -= cfg.borderSize * cfg.cs;
        cfg.bmax[0] += cfg.borderSize * cfg.cs;
        cfg.bmax[2] += cfg.borderSize * cfg.cs;
        for (usize t = 0; t + 2 < allTris.size(); t += 3) {
            float mn[2] = {1e30f, 1e30f}, mx[2] = {-1e30f, -1e30f};
            for (int k = 0; k < 3; ++k) {
                const float* v = &verts[static_cast<usize>(allTris[t + k]) * 3];
                mn[0] = std::min(mn[0], v[0]), mx[0] = std::max(mx[0], v[0]);
                mn[1] = std::min(mn[1], v[2]), mx[1] = std::max(mx[1], v[2]);
            }
            if (mx[0] < cfg.bmin[0] || mn[0] > cfg.bmax[0] || mx[1] < cfg.bmin[2] || mn[1] > cfg.bmax[2]) continue;
            tileTris.insert(tileTris.end(), {allTris[t], allTris[t + 1], allTris[t + 2]});
        }
        if (tileTris.empty()) return {};
        tris = &tileTris;
    }
    const int ntris = static_cast<int>(tris->size() / 3);
    RecastData r;
    r.solid = rcAllocHeightfield();
    if (!r.solid || !rcCreateHeightfield(&r.ctx, *r.solid, cfg.width, cfg.height, cfg.bmin, cfg.bmax, cfg.cs, cfg.ch))
        return makeError(Error::Code::Unknown, "navmesh: heightfield allocation failed");
    std::vector<unsigned char> areas(static_cast<usize>(ntris), 0);
    rcMarkWalkableTriangles(&r.ctx, cfg.walkableSlopeAngle, verts.data(), nverts, tris->data(), ntris, areas.data());
    if (!rcRasterizeTriangles(&r.ctx, verts.data(), nverts, tris->data(), areas.data(), ntris, *r.solid, cfg.walkableClimb))
        return makeError(Error::Code::Unknown, "navmesh: rasterization failed");
    rcFilterLowHangingWalkableObstacles(&r.ctx, cfg.walkableClimb, *r.solid);
    rcFilterLedgeSpans(&r.ctx, cfg.walkableHeight, cfg.walkableClimb, *r.solid);
    rcFilterWalkableLowHeightSpans(&r.ctx, cfg.walkableHeight, *r.solid);
    r.chf = rcAllocCompactHeightfield();
    if (!r.chf || !rcBuildCompactHeightfield(&r.ctx, cfg.walkableHeight, cfg.walkableClimb, *r.solid, *r.chf))
        return makeError(Error::Code::Unknown, "navmesh: compact heightfield failed");
    if (!rcErodeWalkableArea(&r.ctx, cfg.walkableRadius, *r.chf)) return makeError(Error::Code::Unknown, "navmesh: erosion failed");
    // Cost areas (ids 1..62) and forbidden boxes, marked on the voxels before the regions.
    const usize areaCount = std::min<usize>(areaBoxes.size(), 62);
    for (usize i = 0; i < areaCount; ++i) {
        const NavMeshArea& a = areaBoxes[i];
        const float bmin[3] = {a.min.x, a.min.y, a.min.z}, bmax[3] = {a.max.x, a.max.y, a.max.z};
        rcMarkBoxArea(&r.ctx, bmin, bmax, a.walkable ? static_cast<unsigned char>(i + 1) : RC_NULL_AREA, *r.chf);
    }
    if (!rcBuildDistanceField(&r.ctx, *r.chf)) return makeError(Error::Code::Unknown, "navmesh: distance field failed");
    if (!rcBuildRegions(&r.ctx, *r.chf, cfg.borderSize, cfg.minRegionArea, cfg.mergeRegionArea))
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
    if (r.pmesh->npolys == 0) {
        if (tile) return {};
        return makeError(Error::Code::NotFound, "no walkable surface (check slopes, agent size and geometry)");
    }
    if (r.pmesh->nverts >= 0xffff) return makeError(Error::Code::InvalidArgument, "navmesh: too many vertices for one tile (lower tileSize)");

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
    params.tileX = tx;
    params.tileY = ty;
    params.tileLayer = 0;
    rcVcopy(params.bmin, r.pmesh->bmin);
    rcVcopy(params.bmax, r.pmesh->bmax);
    params.cs = cfg.cs;
    params.ch = cfg.ch;
    params.buildBvTree = true;
    // Off-mesh links: endpoints within the agent radius of the mesh; a tile owns the links that start in it.
    std::vector<float> linkVerts;
    std::vector<float> linkRad;
    std::vector<unsigned short> linkFlags;
    std::vector<unsigned char> linkAreas, linkDir;
    std::vector<unsigned int> linkIds;
    for (usize i = 0; i < links.size(); ++i) {
        const NavMeshLink& l = links[i];
        if (tile && (l.start.x < tileMin[0] || l.start.x >= tileMax[0] || l.start.z < tileMin[2] || l.start.z >= tileMax[2])) continue;
        linkVerts.insert(linkVerts.end(), {l.start.x, l.start.y, l.start.z, l.end.x, l.end.y, l.end.z});
        linkRad.push_back(std::max(s.agentRadius, cfg.cs * 2.0f));
        linkFlags.push_back(1);
        linkAreas.push_back(0);
        linkDir.push_back(l.bidirectional ? DT_OFFMESH_CON_BIDIR : 0);
        linkIds.push_back(static_cast<unsigned int>(i));
    }
    if (!linkIds.empty()) {
        params.offMeshConVerts = linkVerts.data();
        params.offMeshConRad = linkRad.data();
        params.offMeshConFlags = linkFlags.data();
        params.offMeshConAreas = linkAreas.data();
        params.offMeshConDir = linkDir.data();
        params.offMeshConUserID = linkIds.data();
        params.offMeshConCount = static_cast<int>(linkIds.size());
    }
    if (!dtCreateNavMeshData(&params, data, dataSize)) return makeError(Error::Code::Unknown, "navmesh: Detour data failed");
    *polys = r.pmesh->npolys;
    *vertsOut = r.pmesh->nverts;
    return {};
}
} // namespace

Status NavMesh::build(const std::vector<Vec3>& vertices, const std::vector<u32>& indices, const NavMeshBuildSettings& s, const std::vector<NavMeshLink>& links,
                      const std::vector<NavMeshArea>& areaBoxes) {
    Stopwatch sw;
    clear();
    if (vertices.empty() || indices.size() < 3) return makeError(Error::Code::InvalidArgument, "no walkable geometry");
    m->verts.resize(vertices.size() * 3);
    for (usize i = 0; i < vertices.size(); ++i) {
        m->verts[i * 3] = vertices[i].x;
        m->verts[i * 3 + 1] = vertices[i].y;
        m->verts[i * 3 + 2] = vertices[i].z;
    }
    m->tris.assign(indices.begin(), indices.begin() + static_cast<std::ptrdiff_t>(indices.size() / 3 * 3));
    m->links = links;
    m->areas = areaBoxes;
    m->settings = s;
    const int nverts = static_cast<int>(vertices.size());
    rcConfig cfg = baseConfig(s);
    rcCalcBounds(m->verts.data(), nverts, cfg.bmin, cfg.bmax);
    rcCalcGridSize(cfg.bmin, cfg.bmax, cfg.cs, &cfg.width, &cfg.height);
    const usize areaCount = std::min<usize>(areaBoxes.size(), 62);
    auto finish = [&]() -> Status {
        m->query = dtAllocNavMeshQuery();
        if (!m->query || dtStatusFailed(m->query->init(m->mesh, 2048))) {
            m->reset();
            return makeError(Error::Code::Unknown, "navmesh: query init failed");
        }
        m->filter.setIncludeFlags(0xffff);
        m->filter.setExcludeFlags(0);
        m->filter.setAreaCost(0, 1.0f);
        for (usize i = 0; i < areaCount; ++i) m->filter.setAreaCost(static_cast<int>(i + 1), std::max(areaBoxes[i].cost, 1.0f));
        m_stats.inputTriangles = static_cast<u32>(m->tris.size() / 3);
        m_stats.links = static_cast<u32>(links.size());
        m_stats.buildMs = sw.elapsedMilliseconds();
        return {};
    };

    if (s.tileSize <= 0) {
        if (static_cast<i64>(cfg.width) * cfg.height > 4096ll * 4096ll)
            return makeError(Error::Code::InvalidArgument, "navigation area too large for the cell size (raise cellSize or use tiles)");
        unsigned char* navData = nullptr;
        int navDataSize = 0, polys = 0, nv = 0;
        if (Status st = buildTileData(m->verts, m->tris, links, areaBoxes, s, cfg, false, 0, 0, &navData, &navDataSize, &polys, &nv); !st) return st;
        m->mesh = dtAllocNavMesh();
        if (!m->mesh || dtStatusFailed(m->mesh->init(navData, navDataSize, DT_TILE_FREE_DATA))) {
            dtFree(navData);
            m->reset();
            return makeError(Error::Code::Unknown, "navmesh: Detour init failed");
        }
        m_stats.polygons = static_cast<u32>(polys);
        m_stats.vertices = static_cast<u32>(nv);
        m_stats.tiles = 1;
        m_stats.tilesRebuilt = 1;
        return finish();
    }

    // Tiled: a grid of tiles over the bounds, each built with a border so that they join.
    const int ts = std::clamp(s.tileSize, 16, 1024);
    m->tiled = true;
    m->tileWorld = static_cast<float>(ts) * cfg.cs;
    rcVcopy(m->orig, cfg.bmin);
    m->bottom = cfg.bmin[1];
    m->top = cfg.bmax[1];
    m->tilesX = (cfg.width + ts - 1) / ts;
    m->tilesY = (cfg.height + ts - 1) / ts;
    const int tileCount = m->tilesX * m->tilesY;
    int tileBits = 0;
    while ((1 << tileBits) < tileCount) ++tileBits;
    if (tileBits > 14) return makeError(Error::Code::InvalidArgument, "navmesh: too many tiles (raise tileSize or cellSize)");
    dtNavMeshParams np{};
    rcVcopy(np.orig, cfg.bmin);
    np.tileWidth = m->tileWorld;
    np.tileHeight = m->tileWorld;
    np.maxTiles = 1 << tileBits;
    np.maxPolys = 1 << (22 - tileBits);
    m->mesh = dtAllocNavMesh();
    if (!m->mesh || dtStatusFailed(m->mesh->init(&np))) {
        m->reset();
        return makeError(Error::Code::Unknown, "navmesh: Detour init failed");
    }
    for (int y = 0; y < m->tilesY; ++y)
        for (int x = 0; x < m->tilesX; ++x)
            if (Status st = rebuildTile(x, y); !st) {
                m->reset();
                return st;
            }
    m_stats.tilesRebuilt = static_cast<u32>(tileCount);
    countTiles();
    if (m_stats.polygons == 0) {
        m->reset();
        return makeError(Error::Code::NotFound, "no walkable surface (check slopes, agent size and geometry)");
    }
    return finish();
}

Status NavMesh::rebuildTile(int x, int y) {
    rcConfig cfg = baseConfig(m->settings);
    cfg.tileSize = std::clamp(m->settings.tileSize, 16, 1024);
    cfg.bmin[0] = m->orig[0] + static_cast<float>(x) * m->tileWorld;
    cfg.bmin[1] = m->bottom;
    cfg.bmin[2] = m->orig[2] + static_cast<float>(y) * m->tileWorld;
    cfg.bmax[0] = cfg.bmin[0] + m->tileWorld;
    cfg.bmax[1] = m->top;
    cfg.bmax[2] = cfg.bmin[2] + m->tileWorld;
    if (const dtTileRef old = m->mesh->getTileRefAt(x, y, 0)) m->mesh->removeTile(old, nullptr, nullptr);
    unsigned char* data = nullptr;
    int size = 0, polys = 0, nv = 0;
    if (Status st = buildTileData(m->verts, m->tris, m->links, m->areas, m->settings, cfg, true, x, y, &data, &size, &polys, &nv); !st) return st;
    if (!data) return {};
    if (dtStatusFailed(m->mesh->addTile(data, size, DT_TILE_FREE_DATA, 0, nullptr))) {
        dtFree(data);
        return makeError(Error::Code::Unknown, "navmesh: Detour tile failed");
    }
    return {};
}

void NavMesh::countTiles() {
    m_stats.polygons = m_stats.vertices = m_stats.tiles = 0;
    const dtNavMesh& mesh = *m->mesh;
    for (int t = 0; t < mesh.getMaxTiles(); ++t) {
        const dtMeshTile* tile = mesh.getTile(t);
        if (!tile || !tile->header) continue;
        ++m_stats.tiles;
        m_stats.polygons += static_cast<u32>(tile->header->polyCount);
        m_stats.vertices += static_cast<u32>(tile->header->vertCount);
    }
}

Status NavMesh::update(const std::vector<Vec3>& vertices, const std::vector<u32>& indices, const NavMeshBuildSettings& s, const std::vector<NavMeshLink>& links,
                       const std::vector<NavMeshArea>& areaBoxes) {
    if (!m->tiled || empty() || !(s == m->settings) || !(links == m->links) || !(areaBoxes == m->areas) || vertices.empty())
        return build(vertices, indices, s, links, areaBoxes);
    Stopwatch sw;
    // Triangles added or removed: a multiset of quantized triangles (1 mm), old minus new.
    struct Box {
        float mn[3], mx[3];
    };
    std::unordered_map<u64, std::pair<int, Box>> diff;
    auto key = [](const float* a, const float* b, const float* c) {
        u64 h = 1469598103934665603ull;
        for (const float* v : {a, b, c})
            for (int k = 0; k < 3; ++k) h = (h ^ static_cast<u64>(static_cast<i64>(std::lround(v[k] * 1000.0f)))) * 1099511628211ull;
        return h;
    };
    auto account = [&](const float* a, const float* b, const float* c, int delta) {
        auto& [count, box] = diff[key(a, b, c)];
        if (count == 0)
            for (int k = 0; k < 3; ++k) {
                box.mn[k] = std::min({a[k], b[k], c[k]});
                box.mx[k] = std::max({a[k], b[k], c[k]});
            }
        count += delta;
    };
    for (usize t = 0; t + 2 < m->tris.size(); t += 3)
        account(&m->verts[static_cast<usize>(m->tris[t]) * 3], &m->verts[static_cast<usize>(m->tris[t + 1]) * 3], &m->verts[static_cast<usize>(m->tris[t + 2]) * 3], 1);
    std::vector<float> verts(vertices.size() * 3);
    for (usize i = 0; i < vertices.size(); ++i) {
        verts[i * 3] = vertices[i].x;
        verts[i * 3 + 1] = vertices[i].y;
        verts[i * 3 + 2] = vertices[i].z;
    }
    for (usize t = 0; t + 2 < indices.size(); t += 3) account(&verts[indices[t] * 3], &verts[indices[t + 1] * 3], &verts[indices[t + 2] * 3], -1);
    // Changed tiles; geometry beyond the grid (or its height) needs a full build.
    const float border = (std::ceil(s.agentRadius / std::max(s.cellSize, 0.02f)) + 3.0f) * std::max(s.cellSize, 0.02f);
    std::vector<char> dirty(static_cast<usize>(m->tilesX * m->tilesY), 0);
    for (const auto& [h, entry] : diff) {
        if (entry.first == 0) continue;
        const Box& b = entry.second;
        if (b.mn[0] < m->orig[0] || b.mn[2] < m->orig[2] || b.mx[0] > m->orig[0] + m->tilesX * m->tileWorld ||
            b.mx[2] > m->orig[2] + m->tilesY * m->tileWorld || b.mn[1] < m->bottom - 1e-3f || b.mx[1] > m->top + 1e-3f)
            return build(vertices, indices, s, links, areaBoxes);
        const int x0 = std::max(0, static_cast<int>(std::floor((b.mn[0] - border - m->orig[0]) / m->tileWorld)));
        const int x1 = std::min(m->tilesX - 1, static_cast<int>(std::floor((b.mx[0] + border - m->orig[0]) / m->tileWorld)));
        const int y0 = std::max(0, static_cast<int>(std::floor((b.mn[2] - border - m->orig[2]) / m->tileWorld)));
        const int y1 = std::min(m->tilesY - 1, static_cast<int>(std::floor((b.mx[2] + border - m->orig[2]) / m->tileWorld)));
        for (int y = y0; y <= y1; ++y)
            for (int x = x0; x <= x1; ++x) dirty[static_cast<usize>(y * m->tilesX + x)] = 1;
    }
    m->verts = std::move(verts);
    m->tris.assign(indices.begin(), indices.begin() + static_cast<std::ptrdiff_t>(indices.size() / 3 * 3));
    u32 rebuilt = 0;
    for (int y = 0; y < m->tilesY; ++y)
        for (int x = 0; x < m->tilesX; ++x)
            if (dirty[static_cast<usize>(y * m->tilesX + x)]) {
                if (Status st = rebuildTile(x, y); !st) return build(vertices, indices, s, links, areaBoxes);
                ++rebuilt;
            }
    countTiles();
    m_stats.tilesRebuilt = rebuilt;
    m_stats.inputTriangles = static_cast<u32>(m->tris.size() / 3);
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
