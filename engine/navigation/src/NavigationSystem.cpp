#include "agr/navigation/NavigationSystem.hpp"

#include <algorithm>
#include <bit>
#include <cmath>

#include "agr/core/Log.hpp"
#include "agr/core/Time.hpp"
#include "agr/physics2d/Physics2DComponents.hpp"
#include "agr/physics3d/Physics3DComponents.hpp"
#include "agr/render/RenderComponents.hpp"
#include "agr/scene/Components.hpp"
#include "agr/scene/Reflection.hpp"
#include "agr/scene/Terrain.hpp"

namespace agr {

using math::Mat4;
using math::Vec2;
using math::Vec3;

const char* enumName(NavStatus s) {
    switch (s) {
        case NavStatus::Idle: return "idle";
        case NavStatus::Moving: return "moving";
        case NavStatus::Arrived: return "arrived";
        case NavStatus::Unreachable: return "unreachable";
    }
    return "idle";
}

namespace {

u64 mix(u64 h, u64 v) {
    h ^= v + 0x9e3779b97f4a7c15ull + (h << 6) + (h >> 2);
    return h;
}
u64 mixf(u64 h, float f) { return mix(h, std::bit_cast<u32>(f)); }

/// A collider is static geometry when no moving body (dynamic / kinematic) owns it.
/// Agents' own colliders are not obstacles either (they would block their own way).
template <typename Body, typename Type>
bool staticCollider(Scene& s, ecs::Entity e, Type staticType) {
    for (ecs::Entity c = e; !c.isNull(); c = s.parent(c))
        if (s.registry().has<NavAgentComponent>(c)) return false;
    for (ecs::Entity c = e; !c.isNull(); c = s.parent(c))
        if (const auto* rb = s.registry().tryGet<Body>(c)) return rb->type == staticType;
    return true;
}

NavigationSettingsComponent settingsOf(Scene& s) {
    NavigationSettingsComponent out;
    bool found = false;
    s.registry().view<NavigationSettingsComponent>().each([&](ecs::Entity e, NavigationSettingsComponent& c) {
        if (!found && s.isActiveInHierarchy(e)) out = c, found = true;
    });
    return out;
}

Vec3 worldPosition(Scene& s, ecs::Entity e) {
    const Mat4 m = s.worldMatrix(e);
    return {m.c[3].x, m.c[3].y, m.c[3].z};
}

void setWorldPosition(Scene& s, ecs::Entity e, Vec3 p) {
    auto& t = s.registry().get<TransformComponent>(e);
    const ecs::Entity parent = s.parent(e);
    t.position = parent.isNull() ? p : math::inverse(s.worldMatrix(parent)).transformPoint(p);
}

// ---- 3D source triangles
void addBox(std::vector<Vec3>& v, std::vector<u32>& idx, const Mat4& m) {
    const u32 base = static_cast<u32>(v.size());
    for (int i = 0; i < 8; ++i) v.push_back(m.transformPoint({(i & 1) ? 0.5f : -0.5f, (i & 2) ? 0.5f : -0.5f, (i & 4) ? 0.5f : -0.5f}));
    // Faces wound counter-clockwise seen from outside (Recast: walkable = facing up).
    const u32 f[12][3] = {{2, 6, 7}, {2, 7, 3}, {0, 1, 5}, {0, 5, 4}, {0, 2, 3}, {0, 3, 1}, {4, 5, 7}, {4, 7, 6}, {0, 4, 6}, {0, 6, 2}, {1, 3, 7}, {1, 7, 5}};
    for (const auto& t : f)
        for (u32 k : t) idx.push_back(base + k);
}

void addSphere(std::vector<Vec3>& v, std::vector<u32>& idx, const Mat4& m, float radius, float halfCylinder) {
    constexpr int kRings = 6, kSegments = 10;
    const u32 base = static_cast<u32>(v.size());
    for (int r = 0; r <= kRings; ++r) {
        const float phi = 3.14159265f * static_cast<float>(r) / kRings;
        const float y = std::cos(phi) * radius + (r <= kRings / 2 ? halfCylinder : -halfCylinder);
        for (int sgm = 0; sgm < kSegments; ++sgm) {
            const float th = 6.2831853f * static_cast<float>(sgm) / kSegments;
            v.push_back(m.transformPoint({std::sin(phi) * radius * std::cos(th), y, std::sin(phi) * radius * std::sin(th)}));
        }
    }
    for (int r = 0; r < kRings; ++r)
        for (int sgm = 0; sgm < kSegments; ++sgm) {
            const u32 a = base + static_cast<u32>(r * kSegments + sgm), b = base + static_cast<u32>(r * kSegments + (sgm + 1) % kSegments);
            const u32 c = a + kSegments, d = b + kSegments;
            idx.insert(idx.end(), {a, b, c, b, d, c});
        }
}

} // namespace

void registerNavigationComponents() {
    auto& reg = ComponentRegistry::instance();
    if (reg.find<NavAgentComponent>()) return;
    reg.add<NavAgentComponent>("NavAgent")
        .category("Navigation")
        .description("Follows paths toward a destination given by a script (entity:navigateTo).")
        .field("speed", &NavAgentComponent::speed, {.rangeMin = 0.0f, .rangeMax = 100.0f, .speed = 0.05f})
        .field("stoppingDistance", &NavAgentComponent::stoppingDistance, {.rangeMin = 0.0f, .rangeMax = 10.0f, .speed = 0.01f})
        .field("faceMovement", &NavAgentComponent::faceMovement, {.tooltip = "3D: turns the entity (its +Z axis) toward where it goes"})
        .field("plane", &NavAgentComponent::plane,
               {.tooltip = "Auto: 3D navigation mesh if the scene has one, else the 2D grid",
                .enumEntries = {{"Auto", 0}, {"Grid2D", 1}, {"Mesh3D", 2}}})
        .field("avoidance", &NavAgentComponent::avoidance, {.tooltip = "Steers around other agents and obstacles (crowds)"})
        .field("radius", &NavAgentComponent::radius, {.rangeMin = 0.01f, .rangeMax = 20.0f, .speed = 0.01f});
    reg.add<NavObstacleComponent>("NavObstacle")
        .category("Navigation")
        .description("Agents steer around it; with carve, it is also cut out of the navigation (paths go around it).")
        .field("radius", &NavObstacleComponent::radius, {.rangeMin = 0.01f, .rangeMax = 100.0f, .speed = 0.01f})
        .field("height", &NavObstacleComponent::height, {.rangeMin = 0.01f, .rangeMax = 100.0f, .speed = 0.01f})
        .field("carve", &NavObstacleComponent::carve, {.tooltip = "Cut out of the navigation (rebuilt when it moves)"});
    reg.add<NavAreaComponent>("NavArea")
        .category("Navigation")
        .description("Zone where walking costs more (mud, water) or is forbidden: a box centered on the entity.")
        .field("size", &NavAreaComponent::size, {.speed = 0.05f})
        .field("cost", &NavAreaComponent::cost, {.tooltip = "Path cost multiplier (1 = normal, 3 = avoided unless much shorter)", .rangeMin = 1.0f, .rangeMax = 100.0f, .speed = 0.05f})
        .field("walkable", &NavAreaComponent::walkable, {.tooltip = "Off: a forbidden zone (a hole in the navigation)"});
    reg.add<NavLinkComponent>("NavLink")
        .category("Navigation")
        .description("Shortcut for paths (jump, ladder, teleporter) from this entity to `end` (relative).")
        .field("end", &NavLinkComponent::end, {.speed = 0.05f})
        .field("bidirectional", &NavLinkComponent::bidirectional)
        .field("cost", &NavLinkComponent::cost, {.tooltip = "Multiplies its length in path costs", .rangeMin = 0.01f, .rangeMax = 100.0f, .speed = 0.05f});
    reg.add<NavigationSettingsComponent>("NavigationSettings")
        .category("Navigation")
        .description("Build parameters of the 2D grid and the 3D navigation mesh (static colliders and tilemaps).")
        .field("gridCellSize", &NavigationSettingsComponent::gridCellSize, {.rangeMin = 0.05f, .rangeMax = 10.0f, .speed = 0.01f})
        .field("agentRadius2D", &NavigationSettingsComponent::agentRadius2D, {.rangeMin = 0.0f, .rangeMax = 10.0f, .speed = 0.01f})
        .field("cellSize", &NavigationSettingsComponent::cellSize, {.rangeMin = 0.02f, .rangeMax = 5.0f, .speed = 0.01f})
        .field("agentRadius", &NavigationSettingsComponent::agentRadius, {.rangeMin = 0.0f, .rangeMax = 10.0f, .speed = 0.01f})
        .field("backgroundBuild", &NavigationSettingsComponent::backgroundBuild, {.tooltip = "Rebuild the 3D mesh on a worker thread (the old one serves meanwhile)"})
        .field("agentSizes", &NavigationSettingsComponent::agentSizes, {.tooltip = "Extra agent radii, ';'-separated (\"0.8;1.5\"): a grid and a mesh per size"})
        .field("agentHeight", &NavigationSettingsComponent::agentHeight, {.rangeMin = 0.1f, .rangeMax = 20.0f, .speed = 0.01f})
        .field("agentMaxClimb", &NavigationSettingsComponent::agentMaxClimb, {.rangeMin = 0.0f, .rangeMax = 5.0f, .speed = 0.01f})
        .field("agentMaxSlope", &NavigationSettingsComponent::agentMaxSlope, {.rangeMin = 0.0f, .rangeMax = 89.0f, .speed = 0.5f})
        .field("meshRenderersAsSources", &NavigationSettingsComponent::meshRenderersAsSources, {.tooltip = "Static imported models are walkable geometry"});
}

// ------------------------------------------------------------------ build
u64 NavigationSystem::geometryHash(Scene& s) const {
    auto& reg = s.registry();
    u64 h = 1469598103934665603ull;
    auto mat = [&](ecs::Entity e) {
        const Mat4 m = s.worldMatrix(e);
        for (int c = 0; c < 4; ++c) h = mixf(mixf(mixf(h, m.c[c].x), m.c[c].y), m.c[c].z);
    };
    reg.view<ColliderComponent>().each([&](ecs::Entity e, ColliderComponent& c) {
        if (c.isTrigger || !s.isActiveInHierarchy(e) || !staticCollider<RigidBodyComponent>(s, e, BodyType3D::Static)) return;
        h = mix(h, static_cast<u64>(c.shape) + 7);
        for (float f : {c.size.x, c.size.y, c.size.z, c.radius, c.height, c.offset.x, c.offset.y, c.offset.z}) h = mixf(h, f);
        mat(e);
    });
    reg.view<Collider2DComponent>().each([&](ecs::Entity e, Collider2DComponent& c) {
        if (c.isTrigger || !s.isActiveInHierarchy(e) || !staticCollider<RigidBody2DComponent>(s, e, BodyType2D::Static)) return;
        h = mix(h, static_cast<u64>(c.shape) + 101);
        for (float f : {c.size.x, c.size.y, c.radius, c.offset.x, c.offset.y}) h = mixf(h, f);
        mat(e);
    });
    reg.view<TilemapCollider2DComponent, TilemapComponent>().each([&](ecs::Entity e, TilemapCollider2DComponent&, TilemapComponent& tm) {
        if (!s.isActiveInHierarchy(e)) return;
        h = mix(mixf(mixf(h, tm.cellSize.x), tm.cellSize.y), (static_cast<u64>(tm.width) << 32) | tm.height);
        for (i32 t : tm.tiles) h = mix(h, static_cast<u64>(t >= 0));
        mat(e);
    });
    reg.view<TerrainComponent>().each([&](ecs::Entity e, TerrainComponent& t) {
        if (!t.collision || !s.isActiveInHierarchy(e)) return;
        h = mix(h, terrain::heightsHash(t));
        mat(e);
    });
    reg.view<NavObstacleComponent>().each([&](ecs::Entity e, NavObstacleComponent& o) {
        if (!o.carve || !s.isActiveInHierarchy(e)) return;
        h = mixf(mixf(h, o.radius), o.height);
        mat(e);
    });
    reg.view<NavLinkComponent>().each([&](ecs::Entity e, NavLinkComponent& l) {
        if (!s.isActiveInHierarchy(e)) return;
        h = mix(mixf(mixf(mixf(mixf(h, l.end.x), l.end.y), l.end.z), l.cost), l.bidirectional ? 3 : 5);
        mat(e);
    });
    reg.view<NavAreaComponent>().each([&](ecs::Entity e, NavAreaComponent& a) {
        if (!s.isActiveInHierarchy(e)) return;
        h = mix(mixf(mixf(mixf(mixf(h, a.size.x), a.size.y), a.size.z), a.cost), a.walkable ? 7 : 11);
        mat(e);
    });
    const NavigationSettingsComponent st = settingsOf(s);
    if (st.meshRenderersAsSources && m_meshSource)
        reg.view<MeshRendererComponent>().each([&](ecs::Entity e, MeshRendererComponent& mr) {
            if (mr.model.empty() || !s.isActiveInHierarchy(e) || !staticCollider<RigidBodyComponent>(s, e, BodyType3D::Static)) return;
            h = mix(h, std::hash<std::string>{}(mr.model) + static_cast<u64>(mr.meshIndex + 2));
            mat(e);
        });
    for (float f : {st.gridCellSize, st.agentRadius2D, st.cellSize, st.agentRadius, st.agentHeight, st.agentMaxClimb, st.agentMaxSlope}) h = mixf(h, f);
    h = mix(h, st.meshRenderersAsSources ? 1 : 2);
    h = mix(h, std::hash<std::string>{}(st.agentSizes));
    return h;
}

void NavigationSystem::finishBackgroundBuild(Scene& s) {
    if (!m_pendingMesh.valid() || m_pendingMesh.wait_for(std::chrono::seconds(0)) != std::future_status::ready) return;
    std::unique_ptr<NavMesh> built = m_pendingMesh.get();
    if (!built) return;
    m_mesh.swap(*built);
    m_stats.meshPolygons = m_mesh.stats().polygons;
    for (auto& [key, a] : m_agents) {  // moving agents follow the new mesh
        if (a.status != NavStatus::Moving || a.corners.empty() || !a.use3D) continue;
        const ecs::Entity e{static_cast<u32>(key), static_cast<u32>(key >> 32)};
        if (s.valid(e)) navigateTo(s, e, a.destination);
    }
}

void NavigationSystem::ensureBuilt(Scene& s) {
    finishBackgroundBuild(s);
    if (&s != m_scene || s.generation() != m_sceneGeneration) {
        reset();
        m_scene = &s;
        m_sceneGeneration = s.generation();
    }
    if (m_checkCountdown > 0 && m_stats.builds > 0) {
        --m_checkCountdown;
        return;
    }
    m_checkCountdown = 30;  // geometry hash every 30 calls (~0.5 s)
    const u64 h = geometryHash(s);
    if (h != m_hash || m_stats.builds == 0) {
        m_hash = h;
        rebuild(s);
    }
}

void NavigationSystem::rebuild(Scene& s) {
    Stopwatch sw;
    auto& reg = s.registry();
    const NavigationSettingsComponent st = settingsOf(s);
    m_stats.sourceShapes2D = m_stats.sourceShapes3D = 0;

    // ---- 2D grid: static 2D colliders and tilemap cells, as world rectangles / circles.
    struct Rect2 {
        Vec2 mn, mx;
    };
    struct Circle2 {
        Vec2 c;
        float r;
    };
    std::vector<Rect2> rects;
    std::vector<Circle2> circles;
    Vec2 lo{1e30f, 1e30f}, hi{-1e30f, -1e30f};
    auto grow = [&](Vec2 a, Vec2 b) {
        lo = {std::min(lo.x, a.x), std::min(lo.y, a.y)};
        hi = {std::max(hi.x, b.x), std::max(hi.y, b.y)};
    };
    reg.view<Collider2DComponent>().each([&](ecs::Entity e, Collider2DComponent& c) {
        if (c.isTrigger || !s.isActiveInHierarchy(e) || !staticCollider<RigidBody2DComponent>(s, e, BodyType2D::Static)) return;
        const Mat4 m = s.worldMatrix(e);
        if (c.shape == ColliderShape2D::Circle) {
            const Vec3 p = m.transformPoint({c.offset.x, c.offset.y, 0.0f});
            const float r = c.radius * std::max(math::length(Vec3{m.c[0].x, m.c[0].y, m.c[0].z}), math::length(Vec3{m.c[1].x, m.c[1].y, m.c[1].z}));
            circles.push_back({{p.x, p.y}, r});
            grow({p.x - r, p.y - r}, {p.x + r, p.y + r});
        } else {  // box / capsule: bounding box of the rotated rectangle
            Vec2 mn{1e30f, 1e30f}, mx{-1e30f, -1e30f};
            for (int i = 0; i < 4; ++i) {
                const Vec3 p = m.transformPoint({c.offset.x + ((i & 1) ? 0.5f : -0.5f) * c.size.x, c.offset.y + ((i & 2) ? 0.5f : -0.5f) * c.size.y, 0.0f});
                mn = {std::min(mn.x, p.x), std::min(mn.y, p.y)};
                mx = {std::max(mx.x, p.x), std::max(mx.y, p.y)};
            }
            rects.push_back({mn, mx});
            grow(mn, mx);
        }
        ++m_stats.sourceShapes2D;
    });
    reg.view<TilemapCollider2DComponent, TilemapComponent>().each([&](ecs::Entity e, TilemapCollider2DComponent&, TilemapComponent& tm) {
        if (!s.isActiveInHierarchy(e)) return;
        const Mat4 m = s.worldMatrix(e);
        for (u32 y = 0; y < tm.height; ++y)
            for (u32 x = 0; x < tm.width; ++x) {
                if (tm.tileAt(x, y) < 0) continue;
                const Vec3 a = m.transformPoint({static_cast<float>(x) * tm.cellSize.x, static_cast<float>(y) * tm.cellSize.y, 0.0f});
                const Vec3 b = m.transformPoint({static_cast<float>(x + 1) * tm.cellSize.x, static_cast<float>(y + 1) * tm.cellSize.y, 0.0f});
                const Vec2 mn{std::min(a.x, b.x), std::min(a.y, b.y)}, mx{std::max(a.x, b.x), std::max(a.y, b.y)};
                rects.push_back({mn, mx});
                grow(mn, mx);
                ++m_stats.sourceShapes2D;
            }
    });
    // 2D agents extend the area (they may walk around the obstacles' bounds).
    reg.view<NavAgentComponent>().each([&](ecs::Entity e, NavAgentComponent& a) {
        if (a.plane == NavPlane::Mesh3D || !s.isActiveInHierarchy(e)) return;
        const Vec3 p = worldPosition(s, e);
        grow({p.x, p.y}, {p.x, p.y});
    });
    // Extra agent sizes.
    std::vector<float> sizes;
    {
        std::string cur;
        for (char ch : st.agentSizes + ";") {
            if (ch == ';' || ch == ',') {
                const float r = cur.empty() ? 0.0f : std::strtof(cur.c_str(), nullptr);
                if (r > 0.0f) sizes.push_back(r);
                cur.clear();
            } else if (ch != ' ') {
                cur += ch;
            }
        }
        std::sort(sizes.begin(), sizes.end());
        sizes.erase(std::unique(sizes.begin(), sizes.end()), sizes.end());
        std::erase_if(sizes, [&](float r) { return r <= std::max(st.agentRadius2D, st.agentRadius) + 1e-4f; });
    }
    m_layers.clear();
    for (float r : sizes) {
        m_layers.push_back(std::make_unique<SizeLayer>());
        m_layers.back()->radius = r;
    }
    auto buildGrid = [&](NavGrid& grid, float radius) {
        grid.reset({0, 0}, 1.0f, 0, 0);
        if (m_stats.sourceShapes2D == 0) return;
        const float cell = std::max(st.gridCellSize, 0.05f);
        const float margin = cell * 8.0f + radius;
        const Vec2 glo = lo - Vec2{margin, margin}, ghi = hi + Vec2{margin, margin};
        const u32 w = std::min<u32>(2048, static_cast<u32>(std::ceil((ghi.x - glo.x) / cell)) + 1);
        const u32 h = std::min<u32>(2048, static_cast<u32>(std::ceil((ghi.y - glo.y) / cell)) + 1);
        grid.reset(glo, cell, w, h);
        for (const Rect2& r : rects) grid.blockRect(r.mn, r.mx, radius);
        for (const Circle2& c : circles) grid.blockCircle(c.c, c.r, radius);
        reg.view<NavObstacleComponent>().each([&](ecs::Entity e, NavObstacleComponent& o) {
            if (!o.carve || !s.isActiveInHierarchy(e)) return;
            const Vec3 p = worldPosition(s, e);
            grid.blockCircle({p.x, p.y}, o.radius, radius);
        });
        reg.view<NavAreaComponent>().each([&](ecs::Entity e, NavAreaComponent& a) {
            if (!s.isActiveInHierarchy(e)) return;
            const Mat4 m = s.worldMatrix(e);
            Vec2 mn{1e30f, 1e30f}, mx{-1e30f, -1e30f};
            for (int k = 0; k < 4; ++k) {  // corners of the (possibly rotated / scaled) rectangle
                const Vec3 c = m.transformPoint({(k & 1 ? 0.5f : -0.5f) * a.size.x, (k & 2 ? 0.5f : -0.5f) * a.size.y, 0.0f});
                mn = {std::min(mn.x, c.x), std::min(mn.y, c.y)}, mx = {std::max(mx.x, c.x), std::max(mx.y, c.y)};
            }
            if (a.walkable) grid.costRect(mn, mx, a.cost);
            else grid.blockRect(mn, mx, radius);
        });
    };
    buildGrid(m_grid, st.agentRadius2D);
    for (auto& l : m_layers) buildGrid(l->grid, l->radius);
    m_stats.links = 0;
    std::vector<NavMeshLink> meshLinks;
    reg.view<NavLinkComponent>().each([&](ecs::Entity e, NavLinkComponent& l) {
        if (!s.isActiveInHierarchy(e)) return;
        const Mat4 m = s.worldMatrix(e);
        const Vec3 a = m.transformPoint({0, 0, 0}), b = m.transformPoint(l.end);
        if (!m_grid.empty()) m_grid.addLink({a.x, a.y}, {b.x, b.y}, l.bidirectional, l.cost);
        for (auto& layer : m_layers)
            if (!layer->grid.empty()) layer->grid.addLink({a.x, a.y}, {b.x, b.y}, l.bidirectional, l.cost);
        meshLinks.push_back({a, b, l.bidirectional});
        ++m_stats.links;
    });
    std::vector<NavMeshArea> meshAreas;
    reg.view<NavAreaComponent>().each([&](ecs::Entity e, NavAreaComponent& a) {
        if (!s.isActiveInHierarchy(e)) return;
        const Mat4 m = s.worldMatrix(e);
        NavMeshArea box{{1e30f, 1e30f, 1e30f}, {-1e30f, -1e30f, -1e30f}, a.cost, a.walkable};
        for (int k = 0; k < 8; ++k) {
            const Vec3 c = m.transformPoint({(k & 1 ? 0.5f : -0.5f) * a.size.x, (k & 2 ? 0.5f : -0.5f) * a.size.y, (k & 4 ? 0.5f : -0.5f) * a.size.z});
            box.min = {std::min(box.min.x, c.x), std::min(box.min.y, c.y), std::min(box.min.z, c.z)};
            box.max = {std::max(box.max.x, c.x), std::max(box.max.y, c.y), std::max(box.max.z, c.z)};
        }
        meshAreas.push_back(box);
    });
    m_stats.gridCells = m_grid.width() * m_grid.height();
    m_stats.gridBlocked = m_grid.blockedCount();

    // ---- 3D navigation mesh: static 3D colliders as triangles.
    std::vector<Vec3> verts;
    std::vector<u32> idx;
    reg.view<ColliderComponent>().each([&](ecs::Entity e, ColliderComponent& c) {
        if (c.isTrigger || !s.isActiveInHierarchy(e) || !staticCollider<RigidBodyComponent>(s, e, BodyType3D::Static)) return;
        const Mat4 world = s.worldMatrix(e);
        switch (c.shape) {
            case ColliderShape3D::Box:
                addBox(verts, idx, world * Mat4::trs(c.offset, math::Quat::identity(), c.size));
                break;
            case ColliderShape3D::Sphere:
                addSphere(verts, idx, world * Mat4::trs(c.offset, math::Quat::identity(), Vec3{1.0f}), c.radius, 0.0f);
                break;
            case ColliderShape3D::Capsule:
                addSphere(verts, idx, world * Mat4::trs(c.offset, math::Quat::identity(), Vec3{1.0f}), c.radius,
                          std::max(c.height * 0.5f - c.radius, 0.0f));
                break;
            case ColliderShape3D::Mesh:
            case ColliderShape3D::Convex: {  // the model's triangles (its convex hull walks like its mesh here)
                std::string model = c.model;
                i32 index = c.meshIndex;
                if (const auto* mr = reg.tryGet<MeshRendererComponent>(e)) {
                    if (model.empty()) model = mr->model;
                    if (index < 0) index = mr->meshIndex;
                }
                if (m_meshSource && !model.empty())
                    if (auto mesh = m_meshSource(model, std::max(index, 0))) {
                        const u32 base = static_cast<u32>(verts.size());
                        for (const Vec3& p : mesh->positions) verts.push_back(world.transformPoint(p));
                        for (u32 i : mesh->indices) idx.push_back(base + i);
                    }
                break;
            }
        }
        ++m_stats.sourceShapes3D;
    });
    if (st.meshRenderersAsSources && m_meshSource)  // static imported models without a collider
        reg.view<MeshRendererComponent>().each([&](ecs::Entity e, MeshRendererComponent& mr) {
            if (mr.model.empty() || reg.has<ColliderComponent>(e) || !s.isActiveInHierarchy(e) ||
                !staticCollider<RigidBodyComponent>(s, e, BodyType3D::Static))
                return;
            const Mat4 world = s.worldMatrix(e);
            for (i32 i = mr.meshIndex >= 0 ? mr.meshIndex : 0; i < (mr.meshIndex >= 0 ? mr.meshIndex + 1 : 64); ++i) {
                auto mesh = m_meshSource(mr.model, i);
                if (!mesh) break;
                const u32 base = static_cast<u32>(verts.size());
                for (const Vec3& p : mesh->positions) verts.push_back(world.transformPoint(p));
                for (u32 k : mesh->indices) idx.push_back(base + k);
            }
            ++m_stats.sourceShapes3D;
        });
    reg.view<NavObstacleComponent>().each([&](ecs::Entity e, NavObstacleComponent& o) {
        if (!o.carve || !s.isActiveInHierarchy(e) || idx.empty()) return;
        const Vec3 p = worldPosition(s, e);  // a tall box: its top is out of reach, its sides are walls
        addBox(verts, idx, Mat4::trs(p + Vec3{0, o.height * 0.5f, 0}, math::Quat::identity(), {o.radius * 2.0f, o.height, o.radius * 2.0f}));
    });
    reg.view<TerrainComponent>().each([&](ecs::Entity e, TerrainComponent& t) {
        if (!t.collision || !s.isActiveInHierarchy(e)) return;
        std::vector<Vec3> positions;
        std::vector<u32> indices;
        terrain::buildMesh(t, positions, indices);
        const Mat4 world = s.worldMatrix(e);
        const u32 base = static_cast<u32>(verts.size());
        for (const Vec3& p : positions) verts.push_back(world.transformPoint(p));
        for (u32 i : indices) idx.push_back(base + i);
        ++m_stats.sourceShapes3D;
    });
    m_stats.lastError.clear();
    auto buildMesh = [&](NavMesh& mesh, float radius) {
        mesh.clear();
        if (idx.empty()) return;
        NavMeshBuildSettings bs;
        bs.cellSize = st.cellSize;
        bs.cellHeight = std::max(st.cellSize * 0.5f, 0.02f);
        bs.agentRadius = radius;
        bs.agentHeight = st.agentHeight;
        bs.agentMaxClimb = st.agentMaxClimb;
        bs.agentMaxSlope = st.agentMaxSlope;
        if (Status r = mesh.build(verts, idx, bs, meshLinks, meshAreas); !r) {
            m_stats.lastError = r.error().message;
            AGR_LOG_WARN("Navigation", "navigation mesh (agent radius {}) not built: {}", radius, r.error().message);
        }
    };
    if (st.backgroundBuild && !m_mesh.empty() && !idx.empty()) {
        // The live mesh keeps serving; the new one replaces it when ready (ensureBuilt).
        if (m_pendingMesh.valid()) m_pendingMesh.wait();  // one rebuild at a time
        NavMeshBuildSettings bs;
        bs.cellSize = st.cellSize;
        bs.cellHeight = std::max(st.cellSize * 0.5f, 0.02f);
        bs.agentRadius = st.agentRadius;
        bs.agentHeight = st.agentHeight;
        bs.agentMaxClimb = st.agentMaxClimb;
        bs.agentMaxSlope = st.agentMaxSlope;
        m_pendingMesh = std::async(std::launch::async, [verts, idx, bs, meshLinks, meshAreas]() -> std::unique_ptr<NavMesh> {
            auto mesh = std::make_unique<NavMesh>();
            if (!mesh->build(verts, idx, bs, meshLinks, meshAreas)) return nullptr;
            return mesh;
        });
    } else {
        buildMesh(m_mesh, st.agentRadius);
    }
    for (auto& l : m_layers) buildMesh(l->mesh, l->radius);
    m_stats.meshPolygons = m_mesh.stats().polygons;
    ++m_stats.builds;
    m_stats.lastBuildMs = sw.elapsedMilliseconds();
    // Moving agents follow the new geometry.
    for (auto& [key, a] : m_agents) {
        if (a.status != NavStatus::Moving || a.corners.empty()) continue;
        const ecs::Entity e{static_cast<u32>(key), static_cast<u32>(key >> 32)};
        if (s.valid(e)) navigateTo(s, e, a.destination);
    }
}

i32 NavigationSystem::layerFor(float radius) const {
    if (m_layers.empty() || !m_scene) return -1;
    const NavigationSettingsComponent st = settingsOf(*const_cast<Scene*>(m_scene));
    if (radius <= std::max(st.agentRadius2D, st.agentRadius) + 1e-3f) return -1;
    for (usize i = 0; i < m_layers.size(); ++i)
        if (m_layers[i]->radius + 1e-3f >= radius) return static_cast<i32>(i);
    return static_cast<i32>(m_layers.size()) - 1;  // the largest built
}

void NavigationSystem::reset() {
    if (m_pendingMesh.valid()) m_pendingMesh.get();  // a background build of the old scene: dropped
    m_layers.clear();
    m_agents.clear();
    m_grid.reset({0, 0}, 1.0f, 0, 0);
    m_mesh.clear();
    m_hash = 0;
    m_stats = {};
    m_checkCountdown = 0;
}

// ------------------------------------------------------------------ queries
bool NavigationSystem::uses3D(Scene& s, ecs::Entity e) const {
    const auto* a = s.registry().tryGet<NavAgentComponent>(e);
    const NavPlane plane = a ? a->plane : NavPlane::Auto;
    if (plane == NavPlane::Mesh3D) return true;
    if (plane == NavPlane::Grid2D) return false;
    return !m_mesh.empty();
}

bool NavigationSystem::findPath(Scene& s, Vec3 from, Vec3 to, bool use3D, std::vector<Vec3>& out) {
    ensureBuilt(s);
    out.clear();
    if (use3D) return m_mesh.findPath(from, to, out);
    std::vector<Vec2> pts;
    if (!m_grid.findPath({from.x, from.y}, {to.x, to.y}, pts)) return false;
    for (const Vec2& p : pts) out.push_back({p.x, p.y, from.z});
    return true;
}

bool NavigationSystem::navigateTo(Scene& s, ecs::Entity e, Vec3 destination) {
    if (!s.valid(e)) return false;
    ensureBuilt(s);
    Agent& a = m_agents[e.packed()];
    a.use3D = uses3D(s, e);
    const auto* agentComp = s.registry().tryGet<NavAgentComponent>(e);
    a.layer = layerFor(agentComp ? agentComp->radius : 0.0f);
    const NavGrid& grid = gridOf(a.layer);
    const NavMesh& mesh = meshOf(a.layer);
    a.next = 1;
    a.corners.clear();
    const Vec3 from = worldPosition(s, e);
    bool found = false, complete = true;
    if (a.use3D) {
        found = mesh.findPath(from, destination, a.corners, &complete);
    } else {
        std::vector<Vec2> pts;
        found = grid.findPath({from.x, from.y}, {destination.x, destination.y}, pts);
        for (const Vec2& p : pts) a.corners.push_back({p.x, p.y, from.z});
        complete = found && math::length(Vec2{destination.x, destination.y} - pts.back()) < grid.cellSize();
    }
    if (!found || a.corners.empty()) {
        a.status = NavStatus::Unreachable;
        a.corners.clear();
        return false;
    }
    a.status = NavStatus::Moving;
    a.complete = complete;
    a.destination = destination;
    if (!complete) AGR_LOG_INFO("Navigation", "'{}': destination not reachable, going as close as possible", s.nameOf(e));
    return complete;
}

void NavigationSystem::stop(ecs::Entity e) {
    if (auto it = m_agents.find(e.packed()); it != m_agents.end()) {
        it->second.corners.clear();
        it->second.status = NavStatus::Idle;
    }
}

NavStatus NavigationSystem::status(ecs::Entity e) const {
    auto it = m_agents.find(e.packed());
    return it == m_agents.end() ? NavStatus::Idle : it->second.status;
}

std::vector<Vec3> NavigationSystem::path(ecs::Entity e) const {
    auto it = m_agents.find(e.packed());
    if (it == m_agents.end() || it->second.status != NavStatus::Moving) return {};
    return {it->second.corners.begin() + static_cast<std::ptrdiff_t>(std::min(it->second.next, it->second.corners.size())), it->second.corners.end()};
}

// ------------------------------------------------------------------ agents
void NavigationSystem::update(Scene& s, float dt) {
    ensureBuilt(s);
    m_stats.agentsMoving = 0;
    m_stats.avoiding = 0;
    auto& reg = s.registry();
    // Every agent and obstacle (moving or not), in the plane of its navigation (XY grid, XZ mesh).
    struct Body {
        u64 key;
        Vec2 p, v;
        float r;
        bool use3D, agent, moving;
    };
    std::vector<Body> bodies;
    auto planar = [](Vec3 p, bool use3D) { return use3D ? Vec2{p.x, p.z} : Vec2{p.x, p.y}; };
    reg.view<NavAgentComponent>().each([&](ecs::Entity e, NavAgentComponent& c) {
        if (!s.isActiveInHierarchy(e)) return;
        auto it = m_agents.find(e.packed());
        const bool use3D = it != m_agents.end() ? it->second.use3D : uses3D(s, e);
        const bool moving = it != m_agents.end() && it->second.status == NavStatus::Moving;
        bodies.push_back({e.packed(), planar(worldPosition(s, e), use3D), it != m_agents.end() ? planar(it->second.velocity, use3D) : Vec2{},
                          std::max(c.radius, 0.01f), use3D, true, moving});
    });
    std::unordered_map<u64, Vec3> seenObstacles;
    reg.view<NavObstacleComponent>().each([&](ecs::Entity e, NavObstacleComponent& o) {
        if (!s.isActiveInHierarchy(e)) return;
        const Vec3 p = worldPosition(s, e);
        auto last = m_obstaclePositions.find(e.packed());
        const Vec3 vel = last != m_obstaclePositions.end() && dt > 0.0f ? (p - last->second) / dt : Vec3{0.0f};
        seenObstacles[e.packed()] = p;
        for (bool use3D : {false, true})  // seen by agents of both planes
            bodies.push_back({e.packed(), planar(p, use3D), planar(vel, use3D), std::max(o.radius, 0.01f), use3D, false, true});
    });
    m_obstaclePositions = std::move(seenObstacles);

    for (auto it = m_agents.begin(); it != m_agents.end();) {
        const ecs::Entity e{static_cast<u32>(it->first), static_cast<u32>(it->first >> 32)};
        const auto* comp = s.valid(e) ? reg.tryGet<NavAgentComponent>(e) : nullptr;
        if (!comp) {
            it = m_agents.erase(it);
            continue;
        }
        Agent& a = it->second;
        ++it;
        if (a.status != NavStatus::Moving) {
            a.velocity = Vec3{0.0f};
            continue;
        }
        ++m_stats.agentsMoving;
        const Vec3 start = worldPosition(s, e);
        Vec3 pos = start;
        const float speed = std::max(comp->speed, 0.0f);
        float step = speed * dt;
        Vec3 heading{0.0f};
        // Neighbors close enough to matter during the next second.
        const Vec2 me = planar(pos, a.use3D);
        const float r = std::max(comp->radius, 0.01f);
        std::vector<const Body*> near;
        a.ghostTime = std::max(0.0f, a.ghostTime - dt);
        if (comp->avoidance && a.ghostTime <= 0.0f)
            for (const Body& b : bodies) {
                if (b.key == e.packed() || b.use3D != a.use3D) continue;
                if (math::length(b.p - me) < r + b.r + speed * 1.5f + math::length(b.v) * 1.5f) near.push_back(&b);
            }
        if (near.empty()) {
            // Exact path following.
            while (step > 0.0f && a.next < a.corners.size()) {
                Vec3 target = a.corners[a.next];
                if (!a.use3D) target.z = pos.z;
                const Vec3 d = target - pos;
                const float dist = math::length(d);
                const bool last = a.next + 1 == a.corners.size();
                const float travel = dist - (last ? std::max(comp->stoppingDistance, 0.0f) : 0.0f);  // to the corner, or to the stop
                if (dist > 1e-6f) heading = d / dist;
                if (travel <= step) {
                    if (travel > 0.0f) pos = pos + heading * travel;
                    step -= std::max(travel, 0.0f);
                    ++a.next;
                } else {
                    pos = pos + heading * step;
                    step = 0.0f;
                }
            }
        } else {
            // Avoidance: sampled velocities scored by distance to the preferred one and by the time
            // to collision with each neighbor (reciprocal: the other agent takes half of the turn).
            ++m_stats.avoiding;
            while (a.next < a.corners.size()) {  // corners already reached
                Vec3 target = a.corners[a.next];
                if (!a.use3D) target.z = pos.z;
                const bool last = a.next + 1 == a.corners.size();
                if (math::length(target - pos) > (last ? std::max(comp->stoppingDistance, 0.01f) : r * 0.6f)) break;
                ++a.next;
            }
            if (a.next < a.corners.size()) {
                Vec3 target = a.corners[a.next];
                if (!a.use3D) target.z = pos.z;
                const Vec2 toTarget = planar(target, a.use3D) - me;
                const float dist = math::length(toTarget);
                const Vec2 pref = dist > 1e-6f ? toTarget / dist * std::min(speed, dist / std::max(dt, 1e-4f)) : Vec2{};
                const Vec2 current = planar(a.velocity, a.use3D);
                auto timeToCollision = [&](Vec2 v) {
                    float best = 1e9f;
                    for (const Body* b : near) {
                        const Vec2 rel = b->p - me;
                        // Reciprocal for moving agents (they take half of the turn); standing ones are obstacles.
                        const Vec2 w = b->agent && b->moving ? v * 2.0f - current - b->v : v - b->v;
                        const float R = r + b->r;
                        const float c = math::dot(rel, rel) - R * R;
                        if (c < 0.0f) {  // already overlapping: only moving apart is fine
                            if (math::dot(w, rel) > 0.0f) best = 0.0f;
                            continue;
                        }
                        const float aa = math::dot(w, w), bb = math::dot(w, rel);
                        const float disc = bb * bb - aa * c;
                        if (aa < 1e-8f || bb <= 0.0f || disc < 0.0f) continue;
                        best = std::min(best, (bb - std::sqrt(disc)) / aa);
                    }
                    return best;
                };
                Vec2 bestV = pref;
                float bestScore = 1e30f;
                auto consider = [&](Vec2 v) {
                    // Stay on walkable ground.
                    const Vec2 next = me + v * dt;
                    if (!a.use3D && !m_grid.empty()) {
                        i32 cx, cy;
                        if (gridOf(a.layer).cellOf(next, cx, cy) && gridOf(a.layer).blocked(cx, cy)) return;
                    }
                    const float ttc = timeToCollision(v);
                    const float score = math::length(v - pref) * 1.0f + math::length(v - current) * 0.25f + (ttc < 3.0f ? 2.0f * speed / std::max(ttc, 0.05f) : 0.0f);
                    if (score < bestScore) bestScore = score, bestV = v;
                };
                consider(pref);
                consider({0.0f, 0.0f});
                const float baseAngle = std::atan2(pref.y, pref.x);
                for (int k = 0; k < 16; ++k) {
                    const float ang = baseAngle + 6.2831853f * static_cast<float>(k) / 16.0f;
                    for (float f : {1.0f, 0.5f}) consider(Vec2{std::cos(ang), std::sin(ang)} * speed * f);
                }
                const Vec2 moved = me + bestV * dt;
                // Jammed (blocked by agents that are blocked too): after 1 s without progress
                // toward the corner, walk the path through them for 0.75 s.
                const float progress = dist > 1e-6f ? math::dot(moved - me, toTarget / dist) : 0.0f;
                a.stuckTime = progress < speed * dt * 0.1f ? a.stuckTime + dt : 0.0f;
                if (a.stuckTime > 1.0f) {
                    a.stuckTime = 0.0f;
                    a.ghostTime = 0.75f;
                }
                if (a.use3D) {
                    Vec3 p3{moved.x, pos.y, moved.y};
                    Vec3 onMesh;
                    if (meshOf(a.layer).nearestPoint(p3, onMesh, {r, 2.0f, r})) p3 = onMesh;
                    pos = p3;
                } else {
                    pos = {moved.x, moved.y, pos.z};
                }
                if (math::length(bestV) > 1e-4f) heading = a.use3D ? Vec3{bestV.x, 0.0f, bestV.y} / math::length(bestV) : Vec3{bestV.x, bestV.y, 0.0f} / math::length(bestV);
            }
        }
        a.velocity = dt > 0.0f ? (pos - start) / dt : Vec3{0.0f};
        setWorldPosition(s, e, pos);
        if (comp->faceMovement && a.use3D && (heading.x != 0.0f || heading.z != 0.0f))
            reg.get<TransformComponent>(e).rotation = math::Quat::fromAxisAngle({0, 1, 0}, std::atan2(heading.x, heading.z));
        if (a.next >= a.corners.size()) {
            a.status = a.complete ? NavStatus::Arrived : NavStatus::Unreachable;
            a.velocity = Vec3{0.0f};
        }
    }
}

} // namespace agr
