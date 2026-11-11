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
                .enumEntries = {{"Auto", 0}, {"Grid2D", 1}, {"Mesh3D", 2}}});
    reg.add<NavigationSettingsComponent>("NavigationSettings")
        .category("Navigation")
        .description("Build parameters of the 2D grid and the 3D navigation mesh (static colliders and tilemaps).")
        .field("gridCellSize", &NavigationSettingsComponent::gridCellSize, {.rangeMin = 0.05f, .rangeMax = 10.0f, .speed = 0.01f})
        .field("agentRadius2D", &NavigationSettingsComponent::agentRadius2D, {.rangeMin = 0.0f, .rangeMax = 10.0f, .speed = 0.01f})
        .field("cellSize", &NavigationSettingsComponent::cellSize, {.rangeMin = 0.02f, .rangeMax = 5.0f, .speed = 0.01f})
        .field("agentRadius", &NavigationSettingsComponent::agentRadius, {.rangeMin = 0.0f, .rangeMax = 10.0f, .speed = 0.01f})
        .field("agentHeight", &NavigationSettingsComponent::agentHeight, {.rangeMin = 0.1f, .rangeMax = 20.0f, .speed = 0.01f})
        .field("agentMaxClimb", &NavigationSettingsComponent::agentMaxClimb, {.rangeMin = 0.0f, .rangeMax = 5.0f, .speed = 0.01f})
        .field("agentMaxSlope", &NavigationSettingsComponent::agentMaxSlope, {.rangeMin = 0.0f, .rangeMax = 89.0f, .speed = 0.5f});
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
    const NavigationSettingsComponent st = settingsOf(s);
    for (float f : {st.gridCellSize, st.agentRadius2D, st.cellSize, st.agentRadius, st.agentHeight, st.agentMaxClimb, st.agentMaxSlope}) h = mixf(h, f);
    return h;
}

void NavigationSystem::ensureBuilt(Scene& s) {
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
    m_grid.reset({0, 0}, 1.0f, 0, 0);
    if (m_stats.sourceShapes2D > 0) {
        const float cell = std::max(st.gridCellSize, 0.05f);
        const float margin = cell * 8.0f;
        lo = lo - Vec2{margin, margin};
        hi = hi + Vec2{margin, margin};
        const u32 w = std::min<u32>(2048, static_cast<u32>(std::ceil((hi.x - lo.x) / cell)) + 1);
        const u32 h = std::min<u32>(2048, static_cast<u32>(std::ceil((hi.y - lo.y) / cell)) + 1);
        m_grid.reset(lo, cell, w, h);
        for (const Rect2& r : rects) m_grid.blockRect(r.mn, r.mx, st.agentRadius2D);
        for (const Circle2& c : circles) m_grid.blockCircle(c.c, c.r, st.agentRadius2D);
    }
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
        }
        ++m_stats.sourceShapes3D;
    });
    m_mesh.clear();
    m_stats.lastError.clear();
    if (!idx.empty()) {
        NavMeshBuildSettings bs;
        bs.cellSize = st.cellSize;
        bs.cellHeight = std::max(st.cellSize * 0.5f, 0.02f);
        bs.agentRadius = st.agentRadius;
        bs.agentHeight = st.agentHeight;
        bs.agentMaxClimb = st.agentMaxClimb;
        bs.agentMaxSlope = st.agentMaxSlope;
        if (Status r = m_mesh.build(verts, idx, bs); !r) {
            m_stats.lastError = r.error().message;
            AGR_LOG_WARN("Navigation", "navigation mesh not built: {}", r.error().message);
        }
    }
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

void NavigationSystem::reset() {
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
    a.next = 1;
    a.corners.clear();
    const Vec3 from = worldPosition(s, e);
    bool found = false, complete = true;
    if (a.use3D) {
        found = m_mesh.findPath(from, destination, a.corners, &complete);
    } else {
        std::vector<Vec2> pts;
        found = m_grid.findPath({from.x, from.y}, {destination.x, destination.y}, pts);
        for (const Vec2& p : pts) a.corners.push_back({p.x, p.y, from.z});
        complete = found && math::length(Vec2{destination.x, destination.y} - pts.back()) < m_grid.cellSize();
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
    for (auto it = m_agents.begin(); it != m_agents.end();) {
        const ecs::Entity e{static_cast<u32>(it->first), static_cast<u32>(it->first >> 32)};
        const auto* comp = s.valid(e) ? s.registry().tryGet<NavAgentComponent>(e) : nullptr;
        if (!comp) {
            it = m_agents.erase(it);
            continue;
        }
        Agent& a = it->second;
        ++it;
        if (a.status != NavStatus::Moving) continue;
        ++m_stats.agentsMoving;
        Vec3 pos = worldPosition(s, e);
        float step = std::max(comp->speed, 0.0f) * dt;
        Vec3 heading{0.0f};
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
        setWorldPosition(s, e, pos);
        if (comp->faceMovement && a.use3D && (heading.x != 0.0f || heading.z != 0.0f))
            s.registry().get<TransformComponent>(e).rotation = math::Quat::fromAxisAngle({0, 1, 0}, std::atan2(heading.x, heading.z));
        if (a.next >= a.corners.size()) a.status = a.complete ? NavStatus::Arrived : NavStatus::Unreachable;
    }
}

} // namespace agr
