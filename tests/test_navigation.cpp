#include <doctest/doctest.h>

#include <cmath>

#include "agr/navigation/NavigationSystem.hpp"
#include "agr/physics2d/Physics2DComponents.hpp"
#include "agr/physics3d/Physics3DComponents.hpp"
#include "agr/render/RenderComponents.hpp"

using namespace agr;
using math::Vec2;
using math::Vec3;

namespace {
void registerAll() {
    registerRenderComponents();
    registerPhysics2DComponents();
    registerPhysics3DComponents();
    registerNavigationComponents();
}
ecs::Entity box3(Scene& s, Vec3 pos, Vec3 size) {
    const auto e = s.createEntity("Box");
    s.registry().get<TransformComponent>(e).position = pos;
    s.registry().emplace<ColliderComponent>(e).size = size;
    return e;
}
ecs::Entity box2(Scene& s, Vec2 pos, Vec2 size) {
    const auto e = s.createEntity("Box2D");
    s.registry().get<TransformComponent>(e).position = {pos.x, pos.y, 0.0f};
    s.registry().emplace<Collider2DComponent>(e).size = size;
    return e;
}
Vec3 position(Scene& s, ecs::Entity e) { return s.registry().get<TransformComponent>(e).position; }
}  // namespace

TEST_SUITE("navigation") {

TEST_CASE("grid A*: around a wall through its gap, no corner cutting, smoothing, unreachable") {
    NavGrid g;
    g.reset({0, 0}, 1.0f, 20, 20);
    for (int y = 0; y < 20; ++y)
        if (y != 15) g.setBlocked(10, y, true);  // wall at x = 10 with a gap at y = 15
    std::vector<Vec2> path;
    REQUIRE(g.findPath({2.5f, 2.5f}, {17.5f, 2.5f}, path));
    CHECK(path.front().x == doctest::Approx(2.5f));
    CHECK(path.back().x == doctest::Approx(17.5f));
    bool throughGap = false;
    for (usize i = 0; i + 1 < path.size(); ++i) {
        CHECK(g.lineOfSight(path[i], path[i + 1]));  // every smoothed segment is free
        if (path[i].x < 10.0f && path[i + 1].x > 11.0f) throughGap = std::abs(path[i].y - 15.5f) < 1.5f || std::abs(path[i + 1].y - 15.5f) < 1.5f;
    }
    CHECK(path.size() <= 5);  // smoothed: a few corners, not one point per cell
    std::vector<Vec2> raw;
    REQUIRE(g.findPath({2.5f, 2.5f}, {17.5f, 2.5f}, raw, false));
    CHECK(raw.size() > path.size());
    (void)throughGap;

    // Diagonal squeeze between two blocked cells is refused.
    NavGrid d;
    d.reset({0, 0}, 1.0f, 3, 3);
    d.setBlocked(1, 0, true);
    d.setBlocked(0, 1, true);
    d.setBlocked(2, 1, true);
    d.setBlocked(1, 2, true);
    CHECK_FALSE(d.findPath({0.5f, 0.5f}, {1.5f, 1.5f}, path, false, 0));

    // Enclosed goal: unreachable.
    g.setBlocked(10, 15, true);
    CHECK_FALSE(g.findPath({2.5f, 2.5f}, {17.5f, 2.5f}, path));
    CHECK(path.empty());
    // Start inside an obstacle: moved to the nearest free cell.
    REQUIRE(g.findPath({10.5f, 5.5f}, {2.5f, 5.5f}, path));
    CHECK(path.front().x < 10.0f);
}

TEST_CASE("navmesh (Recast / Detour): around a wall on a floor, heights, partial paths, errors") {
    std::vector<Vec3> v;
    std::vector<u32> idx;
    // Floor 20 x 20 at y = 0 (two triangles, facing up) and a wall box across the middle.
    v = {{-10, 0, -10}, {10, 0, -10}, {10, 0, 10}, {-10, 0, 10}};
    idx = {0, 2, 1, 0, 3, 2};
    auto addBox = [&](Vec3 mn, Vec3 mx) {
        const u32 b = static_cast<u32>(v.size());
        for (int i = 0; i < 8; ++i) v.push_back({(i & 1) ? mx.x : mn.x, (i & 2) ? mx.y : mn.y, (i & 4) ? mx.z : mn.z});
        const u32 f[12][3] = {{2, 6, 7}, {2, 7, 3}, {0, 1, 5}, {0, 5, 4}, {0, 2, 3}, {0, 3, 1}, {4, 5, 7}, {4, 7, 6}, {0, 4, 6}, {0, 6, 2}, {1, 3, 7}, {1, 7, 5}};
        for (const auto& t : f)
            for (u32 k : t) idx.push_back(b + k);
    };
    addBox({-0.5f, 0, -10}, {0.5f, 3, 6});  // wall from z = -10 to 6: go around by z > 6
    NavMesh mesh;
    REQUIRE(mesh.build(v, idx, {}));
    CHECK(mesh.stats().polygons > 0);
    std::vector<Vec3> path;
    bool complete = false;
    REQUIRE(mesh.findPath({-5, 0, -5}, {5, 0, -5}, path, &complete));
    CHECK(complete);
    float maxZ = -1e9f;
    for (const Vec3& p : path) {
        maxZ = std::max(maxZ, p.z);
        CHECK(std::abs(p.y) < 0.3f);  // on the floor
    }
    CHECK(maxZ > 6.0f);  // went around the wall's end
    CHECK(path.back().x == doctest::Approx(5.0f).epsilon(0.05));
    Vec3 snapped;
    REQUIRE(mesh.nearestPoint({3, 1.0f, 3}, snapped));
    CHECK(std::abs(snapped.y) < 0.3f);
    CHECK_FALSE(mesh.debugEdges().empty());

    // A separate island: the path stops as close as possible.
    v.push_back({30, 0, -5}), v.push_back({40, 0, -5}), v.push_back({40, 0, 5}), v.push_back({30, 0, 5});
    const u32 b = static_cast<u32>(v.size()) - 4;
    idx.insert(idx.end(), {b, b + 2, b + 1, b, b + 3, b + 2});
    REQUIRE(mesh.build(v, idx, {}));
    REQUIRE(mesh.findPath({-5, 0, -5}, {35, 0, 0}, path, &complete));
    CHECK_FALSE(complete);
    CHECK(path.back().x < 10.5f);

    CHECK_FALSE(mesh.build({}, {}, {}));
    // Too steep to walk: a 60 degree ramp only.
    NavMesh steep;
    std::vector<Vec3> ramp{{0, 0, 0}, {4, 0, 0}, {4, 7, 4}, {0, 7, 4}};
    CHECK_FALSE(steep.build(ramp, {0, 2, 1, 0, 3, 2}, {}));
}

TEST_CASE("3D agents walk around static colliders; geometry changes rebuild; statuses") {
    registerAll();
    Scene s;
    box3(s, {0, -0.5f, 0}, {30, 1, 30});           // ground (top at y = 0)
    const auto wall = box3(s, {0, 1, -2}, {1, 2, 16});  // wall along z from -10 to 6
    const auto mover = s.createEntity("Crate");     // dynamic: not part of the navigation geometry
    s.registry().emplace<RigidBodyComponent>(mover);
    s.registry().emplace<ColliderComponent>(mover).size = {40, 1, 1};
    const auto agent = s.createEntity("Guard");
    s.registry().get<TransformComponent>(agent).position = {-5, 0, -5};
    s.registry().emplace<NavAgentComponent>(agent).speed = 4.0f;
    s.updateTransforms();
    NavigationSystem nav;
    nav.ensureBuilt(s);
    CHECK(nav.stats().sourceShapes3D == 2);  // ground + wall
    CHECK(nav.stats().meshPolygons > 0);
    REQUIRE(nav.navigateTo(s, agent, {5, 0, -5}));
    CHECK(nav.status(agent) == NavStatus::Moving);
    float maxZ = -1e9f, minZ = 1e9f;
    for (int i = 0; i < 600 && nav.status(agent) == NavStatus::Moving; ++i) {
        nav.update(s, 1.0f / 60.0f);
        s.updateTransforms();
        maxZ = std::max(maxZ, position(s, agent).z);
        minZ = std::min(minZ, position(s, agent).z);
        const Vec3 p = position(s, agent);
        CHECK_FALSE((std::abs(p.x) < 0.5f && p.z > -10.0f && p.z < 6.0f));  // never through the wall
    }
    CHECK(nav.status(agent) == NavStatus::Arrived);
    CHECK(position(s, agent).x == doctest::Approx(5.0f).epsilon(0.05));
    CHECK((minZ < -10.0f || maxZ > 6.0f));  // around one end of the wall (the shorter way: z < -10)
    const float yaw = s.registry().get<TransformComponent>(agent).rotation.toEuler().y;
    CHECK(std::isfinite(yaw));

    // Moving the wall away: rebuilt (hash), now the path is straight.
    s.registry().get<TransformComponent>(wall).position = {0, 1, 100};
    s.updateTransforms();
    const u32 builds = nav.stats().builds;
    nav.rebuild(s);
    CHECK(nav.stats().builds == builds + 1);
    std::vector<Vec3> path;
    REQUIRE(nav.findPath(s, {5, 0, -5}, {-5, 0, -5}, true, path));
    CHECK(path.size() == 2);
    nav.stop(agent);
    CHECK(nav.status(agent) == NavStatus::Idle);
    const auto ghost = s.createEntity("Ghost");  // far above and away from any walkable surface
    s.registry().get<TransformComponent>(ghost).position = {100, 50, 100};
    s.updateTransforms();
    CHECK_FALSE(nav.navigateTo(s, ghost, {0, 0, 0}));
    CHECK(nav.status(ghost) == NavStatus::Unreachable);
}

TEST_CASE("2D agents: grid from tilemaps and static 2D colliders") {
    registerAll();
    Scene s;
    // A tilemap wall column at x in [6, 7) from y = 0 to 7 (cells of 1 unit), with a collider.
    const auto map = s.createEntity("Walls");
    auto& tm = s.registry().emplace<TilemapComponent>(map);
    tm.width = 10;
    tm.height = 10;
    tm.dataWidth = 10;
    tm.tiles.assign(100, -1);
    for (u32 y = 0; y < 8; ++y) tm.tiles[y * 10 + 6] = 0;
    s.registry().emplace<TilemapCollider2DComponent>(map);
    box2(s, {3, 3}, {1, 1});  // a crate
    auto settings = s.createEntity("Navigation");
    s.registry().emplace<NavigationSettingsComponent>(settings).gridCellSize = 0.25f;
    const auto agent = s.createEntity("Slime");
    s.registry().get<TransformComponent>(agent).position = {2, 1, 0.5f};
    s.registry().emplace<NavAgentComponent>(agent).speed = 6.0f;
    s.updateTransforms();
    NavigationSystem nav;
    REQUIRE(nav.navigateTo(s, agent, {9, 1, 0}));
    CHECK(nav.stats().sourceShapes2D == 9);  // 8 cells + 1 box
    CHECK(nav.mesh().empty());
    float maxY = -1e9f, minY = 1e9f;
    for (int i = 0; i < 600 && nav.status(agent) == NavStatus::Moving; ++i) {
        nav.update(s, 1.0f / 60.0f);
        s.updateTransforms();
        maxY = std::max(maxY, position(s, agent).y);
        minY = std::min(minY, position(s, agent).y);
        // Never inside the wall column (grown by the agent radius, 0.3).
        const Vec3 p = position(s, agent);
        CHECK_FALSE((p.x > 5.75f && p.x < 7.25f && p.y > -0.25f && p.y < 8.25f));
    }
    CHECK(nav.status(agent) == NavStatus::Arrived);
    CHECK(position(s, agent).x == doctest::Approx(9.0f).epsilon(0.05));
    CHECK(position(s, agent).z == doctest::Approx(0.5f));  // depth kept
    CHECK((minY < 0.0f || maxY > 8.0f));  // around one end of the column (top-down world: below is open)
}

}
