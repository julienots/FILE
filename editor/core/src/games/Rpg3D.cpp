// RPG 3D: third-person hero on a sculpted terrain, a village elder with a quest (three light
// orbs), stone golems chasing on the navigation mesh, sword, experience and levels.
#include "../GameTemplates.hpp"
#include "agr/navigation/NavigationComponents.hpp"
#include "agr/scene/Terrain.hpp"

namespace agr::editor::games {

namespace {

/// A blocky character: body, head and two eyes (children), all primitives.
ecs::Entity figure(Builder& b, const std::string& name, Vec3 pos, Vec4 body, Vec4 head, float scale = 1.0f, ecs::Entity parent = {}) {
    const auto root = b.entity(name, pos, parent);
    b.mesh("Body", PrimitiveMesh::Cube, {0.0f, 0.55f * scale, 0.0f}, Vec3{0.7f, 0.8f, 0.45f} * scale, body, root);
    b.mesh("Head", PrimitiveMesh::Sphere, {0.0f, 1.25f * scale, 0.0f}, Vec3{0.55f} * scale, head, root);
    b.mesh("Eye L", PrimitiveMesh::Sphere, {-0.12f * scale, 1.3f * scale, 0.24f * scale}, Vec3{0.1f} * scale, {0.05f, 0.05f, 0.08f, 1.0f}, root);
    b.mesh("Eye R", PrimitiveMesh::Sphere, {0.12f * scale, 1.3f * scale, 0.24f * scale}, Vec3{0.1f} * scale, {0.05f, 0.05f, 0.08f, 1.0f}, root);
    return root;
}

} // namespace

void buildRpg3D(Scene& scene) {
    Builder b(scene);
    auto& reg = b.reg;
    const auto cam = b.camera3D({0.0f, 9.0f, 10.0f}, {0.0f, 1.0f, 0.0f}, 50.0f);
    b.script(cam, "assets://games/scripts/follow_camera3d.lua", R"({"target": "Hero", "offsetY": 8, "offsetZ": 9})");
    const auto sunLight = templates::sun(scene);
    {
        auto& env = reg.get<EnvironmentComponent>(sunLight);
        env.sky = SkyMode::Image;
        env.skyTexture = "assets://sky/afternoon.hdr";
        env.fog = true;
        env.fogStart = 35.0f;
        env.fogEnd = 80.0f;
    }

    // Terrain: hills around a flat village, painted paths; collision and navigation source.
    const auto land = b.entity("Terrain");
    {
        auto& tc = reg.emplace<TerrainComponent>(land);
        tc.size = 70.0f;
        tc.resolution = 71;
        tc.layer0 = "assets://terrain/grass.png";
        tc.layer1 = "assets://terrain/rock.png";
        tc.layer2 = "assets://terrain/dirt.png";
        tc.layer3 = "assets://terrain/sand.png";
        terrain::ensureData(tc);
        terrain::BrushSettings brush;
        for (auto [x, z, r, h] : {std::tuple{-22.0f, -20.0f, 10.0f, 6.0f}, {24.0f, -18.0f, 12.0f, 7.0f}, {-25.0f, 22.0f, 9.0f, 5.0f},
                                  {22.0f, 24.0f, 10.0f, 4.0f}, {0.0f, -30.0f, 12.0f, 6.0f}}) {
            brush.radius = r;
            brush.strength = h;
            terrain::applyBrush(tc, {x, z}, brush);
        }
        brush.brush = terrain::Brush::Paint;
        brush.strength = 1.0f;
        brush.layer = 1;
        for (auto [x, z] : {std::pair{-22.0f, -20.0f}, {24.0f, -18.0f}, {0.0f, -30.0f}}) {
            brush.radius = 6.0f;
            terrain::applyBrush(tc, {x, z}, brush);
        }
        brush.layer = 2;
        brush.radius = 2.0f;
        for (float t = -24.0f; t <= 24.0f; t += 1.5f) {
            terrain::applyBrush(tc, {t, 0.0f}, brush);  // roads crossing the village
            terrain::applyBrush(tc, {0.0f, t}, brush);
        }
    }
    auto ground = [&](float x, float z) { return terrain::heightAt(reg.get<TerrainComponent>(land), x, z); };

    // Village: crates and rocks (static obstacles), the elder.
    const auto village = b.entity("Village");
    const std::tuple<const char*, float, float, float> props[] = {{"crate", 3.0f, 3.0f, 1.0f}, {"crate", 3.9f, 3.2f, 1.0f}, {"crate", -4.0f, -3.0f, 1.0f},
                                                                  {"rock", -6.0f, 5.0f, 1.6f}, {"rock", 7.0f, -6.0f, 1.8f}, {"rock", -9.0f, -8.0f, 1.4f}};
    int n = 0;
    for (auto [model, x, z, s] : props) {
        const auto e = b.entity(std::string(model == std::string("crate") ? "Crate " : "Rock ") + std::to_string(++n), {x, ground(x, z), z}, village);
        reg.get<TransformComponent>(e).scale = Vec3{s};
        auto& mr = reg.emplace<MeshRendererComponent>(e);
        mr.model = std::string("assets://3d/") + model + ".glb";
        mr.meshIndex = 0;
        reg.emplace<RigidBodyComponent>(e).type = BodyType3D::Static;
        auto& col = reg.emplace<ColliderComponent>(e);
        col.size = {0.9f, 0.9f, 0.9f};
        col.offset = {0.0f, 0.45f, 0.0f};
    }
    const auto elder = figure(b, "Sage", {-2.0f, ground(-2.0f, 2.0f), 2.0f}, {0.55f, 0.3f, 0.65f, 1.0f}, {0.95f, 0.85f, 0.75f, 1.0f});
    b.script(elder, "assets://games/scripts/rpg_npc.lua", R"({"texts": "rpg3d"})");
    reg.emplace<RigidBodyComponent>(elder).type = BodyType3D::Static;
    {
        auto& col = reg.emplace<ColliderComponent>(elder);
        col.shape = ColliderShape3D::Capsule;
        col.radius = 0.4f;
        col.height = 1.6f;
        col.offset = {0.0f, 0.8f, 0.0f};
    }

    // Hero: dynamic capsule that never tips over.
    const auto hero = figure(b, "Hero", {0.0f, ground(0.0f, 0.0f) + 0.1f, 0.0f}, {0.25f, 0.45f, 0.85f, 1.0f}, {0.98f, 0.82f, 0.68f, 1.0f});
    {
        auto& body = reg.emplace<RigidBodyComponent>(hero);
        body.freezeRotation = true;
        body.linearDamping = 0.5f;
        auto& col = reg.emplace<ColliderComponent>(hero);
        col.shape = ColliderShape3D::Capsule;
        col.radius = 0.4f;
        col.height = 1.6f;
        col.offset = {0.0f, 0.8f, 0.0f};
        col.friction = 0.0f;
    }
    b.script(hero, "assets://games/scripts/rpg3d_hero.lua");
    const auto sword = b.mesh("Sword", PrimitiveMesh::Cube, {0.5f, 0.7f, 0.3f}, {0.08f, 0.08f, 0.9f}, {0.85f, 0.88f, 0.95f, 1.0f}, hero);
    reg.get<MeshRendererComponent>(sword).metallic = 0.9f;

    // Light orbs to find (kinematic triggers, they float).
    const auto gems = b.entity("Gems");
    const Vec2 orbAt[] = {{-22.0f, -20.0f}, {24.0f, 24.0f}, {14.0f, -12.0f}};
    for (int i = 0; i < 3; ++i) {
        const float x = orbAt[i].x, z = orbAt[i].y;
        const auto orb = b.mesh("Orb " + std::to_string(i + 1), PrimitiveMesh::Sphere, {x, ground(x, z) + 1.2f, z}, Vec3{0.6f}, {0.5f, 0.9f, 1.0f, 1.0f}, gems);
        auto& light = reg.emplace<PointLightComponent>(orb);
        light.color = {0.5f, 0.85f, 1.0f, 1.0f};
        light.intensity = 6.0f;
        light.range = 6.0f;
        reg.emplace<RigidBodyComponent>(orb).type = BodyType3D::Kinematic;
        auto& col = reg.emplace<ColliderComponent>(orb);
        col.shape = ColliderShape3D::Sphere;
        col.radius = 0.9f;
        col.isTrigger = true;
        b.script(orb, "assets://games/scripts/pickup.lua", R"({"kind": "gem", "bob": 0.25})");
    }

    // Golems: navigation mesh agents.
    const auto enemies = b.entity("Enemies");
    const Vec2 golemAt[] = {{18.0f, 8.0f}, {-16.0f, 12.0f}, {10.0f, -20.0f}, {-14.0f, -10.0f}};
    for (int i = 0; i < 4; ++i) {
        const float x = golemAt[i].x, z = golemAt[i].y;
        const auto g = figure(b, "Golem " + std::to_string(i + 1), {x, ground(x, z), z}, {0.45f, 0.42f, 0.4f, 1.0f}, {0.55f, 0.52f, 0.5f, 1.0f}, 1.2f, enemies);
        auto& agent = reg.emplace<NavAgentComponent>(g);
        agent.speed = 2.6f;
        agent.stoppingDistance = 0.8f;
        b.script(g, "assets://games/scripts/rpg3d_enemy.lua");
    }
    const auto nav = b.entity("Navigation");
    auto& ns = reg.emplace<NavigationSettingsComponent>(nav);
    ns.cellSize = 0.3f;
    ns.agentRadius = 0.5f;

    templates::audioSource(scene, "Music", "assets://audio/ambient.wav", AudioBus::Music, 0.4f);
    rpgHud(b, hero);
}

} // namespace agr::editor::games
