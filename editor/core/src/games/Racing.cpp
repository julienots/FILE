// Racing: arcade car on a stadium-shaped circuit, three laps against an AI opponent, countdown,
// lap and best times (saved), slower off the road, chase camera, touch pedals.
#include <cmath>

#include "../GameTemplates.hpp"

namespace agr::editor::games {

namespace {

/// Centerline of the circuit: two 40 m straights joined by half circles of radius 13.
std::vector<Vec3> circuit() {
    std::vector<Vec3> pts;
    const float r = 13.0f, half = 20.0f;
    for (int i = 0; i <= 4; ++i) pts.push_back({-half + static_cast<float>(i) * 10.0f, 0.0f, r});  // bottom straight, left to right
    for (int i = 1; i < 8; ++i) {
        const float a = math::kPi * 0.5f - static_cast<float>(i) * math::kPi / 8.0f;
        pts.push_back({half + std::cos(a) * r, 0.0f, std::sin(a) * r});
    }
    for (int i = 0; i <= 4; ++i) pts.push_back({half - static_cast<float>(i) * 10.0f, 0.0f, -r});
    for (int i = 1; i < 8; ++i) {
        const float a = -math::kPi * 0.5f - static_cast<float>(i) * math::kPi / 8.0f;
        pts.push_back({-half + std::cos(a) * r, 0.0f, std::sin(a) * r});
    }
    return pts;
}

ecs::Entity car(Builder& b, const std::string& name, Vec3 pos, float yawDeg, Vec4 color) {
    const auto root = b.entity(name, pos);
    b.reg.get<TransformComponent>(root).rotation = math::Quat::fromEuler({0.0f, math::radians(yawDeg), 0.0f});
    const auto body = b.mesh("Body", PrimitiveMesh::Cube, {0.0f, 0.45f, 0.0f}, {1.6f, 0.5f, 3.2f}, color, root);
    b.reg.get<MeshRendererComponent>(body).metallic = 0.6f;
    b.reg.get<MeshRendererComponent>(body).roughness = 0.3f;
    b.mesh("Cabin", PrimitiveMesh::Cube, {0.0f, 0.9f, 0.2f}, {1.3f, 0.45f, 1.5f}, {0.6f, 0.8f, 0.95f, 1.0f}, root);
    for (auto [x, z] : {std::pair{-0.8f, 1.1f}, {0.8f, 1.1f}, {-0.8f, -1.1f}, {0.8f, -1.1f}})
        b.mesh("Wheel", PrimitiveMesh::Sphere, {x, 0.35f, z}, {0.35f, 0.7f, 0.7f}, {0.1f, 0.1f, 0.12f, 1.0f}, root);
    return root;
}

} // namespace

void buildRacing(Scene& scene) {
    Builder b(scene);
    auto& reg = b.reg;
    const auto cam = b.camera3D({-14.0f, 5.0f, 21.0f}, {-14.0f, 1.0f, 13.0f}, 60.0f);
    b.script(cam, "assets://games/scripts/chase_camera.lua", R"({"target": "Player Car"})");
    const auto sunLight = templates::sun(scene);
    reg.get<EnvironmentComponent>(sunLight).sky = SkyMode::Gradient;

    const auto ground = b.mesh("Ground", PrimitiveMesh::Plane, {0.0f, 0.0f, 0.0f}, {120.0f, 1.0f, 80.0f}, {0.35f, 0.62f, 0.3f, 1.0f});
    reg.get<MeshRendererComponent>(ground).texture = "assets://terrain/grass.png";
    reg.get<MeshRendererComponent>(ground).uvScale = {30.0f, 20.0f};
    reg.emplace<RigidBodyComponent>(ground).type = BodyType3D::Static;
    {
        auto& col = reg.emplace<ColliderComponent>(ground);
        col.size = {1.0f, 0.02f, 1.0f};
        col.offset = {0.0f, -0.01f, 0.0f};
        col.friction = 0.2f;
    }
    // Road pieces along the centerline (visual), waypoints for the race logic.
    const auto pts = circuit();
    const auto road = b.entity("Road");
    const auto waypoints = b.entity("Waypoints");
    for (usize i = 0; i < pts.size(); ++i) {
        const Vec3 a = pts[i], c = pts[(i + 1) % pts.size()];
        const Vec3 mid = (a + c) * 0.5f;
        const float len = math::length(c - a);
        const auto seg = b.mesh("Segment " + std::to_string(i + 1), PrimitiveMesh::Cube, {mid.x, 0.02f, mid.z}, {8.0f, 0.04f, len + 1.5f},
                                {0.22f, 0.22f, 0.25f, 1.0f}, road);
        reg.get<TransformComponent>(seg).rotation = math::Quat::fromEuler({0.0f, std::atan2(c.x - a.x, c.z - a.z), 0.0f});
        reg.get<MeshRendererComponent>(seg).roughness = 0.9f;
        reg.get<MeshRendererComponent>(seg).castShadows = false;
        b.entity("WP " + std::to_string(i + 1), a, waypoints);
    }
    // Start / finish gate and stands of trees.
    const auto gate = b.entity("Start Gate", {-20.0f, 0.0f, 13.0f});  // at the first waypoint: a lap ends there
    b.mesh("Pole L", PrimitiveMesh::Cube, {0.0f, 2.0f, -4.6f}, {0.4f, 4.0f, 0.4f}, {0.9f, 0.9f, 0.9f, 1.0f}, gate);
    b.mesh("Pole R", PrimitiveMesh::Cube, {0.0f, 2.0f, 4.6f}, {0.4f, 4.0f, 0.4f}, {0.9f, 0.9f, 0.9f, 1.0f}, gate);
    b.mesh("Banner", PrimitiveMesh::Cube, {0.0f, 4.0f, 0.0f}, {0.3f, 0.8f, 9.6f}, {0.95f, 0.3f, 0.25f, 1.0f}, gate);
    b.mesh("Line", PrimitiveMesh::Cube, {0.0f, 0.05f, 0.0f}, {0.6f, 0.02f, 8.0f}, {0.95f, 0.95f, 0.95f, 1.0f}, gate);
    const auto trees = b.entity("Scenery");
    for (int i = 0; i < 12; ++i) {
        const float a = static_cast<float>(i) * math::kPi * 2.0f / 12.0f;
        const auto t = b.entity("Sprout " + std::to_string(i + 1), {std::cos(a) * 42.0f, 0.0f, std::sin(a) * 30.0f}, trees);
        reg.get<TransformComponent>(t).scale = Vec3{2.5f};
        reg.emplace<MeshRendererComponent>(t).model = "assets://3d/rock.glb";
    }

    // Cars: the player (dynamic body, arcade script) and the AI (follows the waypoints).
    const auto player = car(b, "Player Car", {-17.0f, 0.3f, 14.5f}, 90.0f, {0.85f, 0.15f, 0.15f, 1.0f});
    {
        auto& body = reg.emplace<RigidBodyComponent>(player);
        body.freezeRotation = true;
        body.mass = 800.0f;
        auto& col = reg.emplace<ColliderComponent>(player);
        col.size = {1.6f, 0.7f, 3.2f};
        col.offset = {0.0f, 0.45f, 0.0f};
        col.friction = 0.0f;
    }
    b.script(player, "assets://games/scripts/racing_car.lua");
    const auto ai = car(b, "Rival Car", {-17.0f, 0.0f, 11.5f}, 90.0f, {0.2f, 0.4f, 0.9f, 1.0f});
    b.script(ai, "assets://games/scripts/racing_ai.lua", R"({"lane": -1.5})");

    const auto race = b.entity("Race");
    b.script(race, "assets://games/scripts/racing_race.lua");
    templates::audioSource(scene, "Music", "assets://audio/music.wav", AudioBus::Music, 0.35f);

    const auto hud = b.hud();
    const auto top = b.panel("Top Panel", hud, {0.0f, 0.0f}, {30.0f, 30.0f}, {600.0f, 220.0f});
    b.label("LapText", top, {0.0f, 0.0f}, {24.0f, 10.0f}, {560.0f, 50.0f}, "", 42.0f);
    b.label("TimeText", top, {0.0f, 0.0f}, {24.0f, 62.0f}, {560.0f, 46.0f}, "", 36.0f);
    b.label("BestText", top, {0.0f, 0.0f}, {24.0f, 110.0f}, {560.0f, 46.0f}, "", 32.0f);
    b.label("PositionText", top, {0.0f, 0.0f}, {24.0f, 158.0f}, {560.0f, 50.0f}, "", 40.0f, UIHAlign::Left, {1.0f, 0.85f, 0.3f, 1.0f});
    b.label("SpeedText", hud, {1.0f, 0.0f}, {-30.0f, 30.0f}, {500.0f, 80.0f}, "", 64.0f, UIHAlign::Right);
    b.label("Message", hud, {0.5f, 0.3f}, {0.0f, 0.0f}, {1400.0f, 160.0f}, "", 96.0f, UIHAlign::Center, {1.0f, 0.95f, 0.5f, 1.0f});
    templates::uiButton(scene, "Left", hud, {0.0f, 1.0f}, {40.0f, -40.0f}, {200.0f, 180.0f}, "<");
    templates::uiButton(scene, "Right", hud, {0.0f, 1.0f}, {270.0f, -40.0f}, {200.0f, 180.0f}, ">");
    const auto gas = templates::uiButton(scene, "Gas", hud, {1.0f, 1.0f}, {-40.0f, -40.0f}, {240.0f, 220.0f}, "Gaz");
    reg.get<UITextComponent>(scene.children(gas).front()).localizationKey = "race.gas";
    const auto brake = templates::uiButton(scene, "Brake", hud, {1.0f, 1.0f}, {-300.0f, -40.0f}, {220.0f, 180.0f}, "Frein");
    reg.get<UITextComponent>(scene.children(brake).front()).localizationKey = "race.brake";
    const auto restart = b.button("Restart", hud, {0.5f, 0.55f}, {0.0f, 0.0f}, {420.0f, 120.0f}, "Rejouer", race, "restart");
    b.buttonText(restart).localizationKey = "game.restart";
    b.hide(restart);
}

} // namespace agr::editor::games
