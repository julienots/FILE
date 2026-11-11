// Real-time strategy (light): workers mine crystals, train workers and soldiers, select units by
// tapping them and send them across the map; the red AI builds an army and attacks; destroy the
// red base before yours falls.
#include "../GameTemplates.hpp"
#include "agr/navigation/NavigationComponents.hpp"

namespace agr::editor::games {

namespace {

ecs::Entity unitPrototype(Builder& b, const std::string& name, ecs::Entity parent, Vec4 color, bool soldier, const std::string& properties) {
    const auto root = b.entity(name, {}, parent);
    b.reg.get<TransformComponent>(root).scale = Vec3{1.4f};  // readable from the high camera
    const float s = soldier ? 1.0f : 0.8f;
    b.mesh("Body", PrimitiveMesh::Cube, {0.0f, 0.45f * s, 0.0f}, Vec3{0.6f, 0.9f, 0.6f} * s, color, root);
    b.mesh("Head", PrimitiveMesh::Sphere, {0.0f, 1.05f * s, 0.0f}, Vec3{0.45f} * s, {0.95f, 0.85f, 0.75f, 1.0f}, root);
    if (soldier) b.mesh("Spear", PrimitiveMesh::Cube, {0.38f, 0.8f, 0.0f}, {0.07f, 1.3f, 0.07f}, {0.8f, 0.8f, 0.85f, 1.0f}, root);
    else b.mesh("Pick", PrimitiveMesh::Cube, {0.35f, 0.5f, 0.0f}, {0.08f, 0.5f, 0.08f}, {0.6f, 0.45f, 0.3f, 1.0f}, root);
    const auto ring = b.mesh("Ring", PrimitiveMesh::Plane, {0.0f, 0.03f, 0.0f}, {1.1f, 1.0f, 1.1f}, {1.0f, 0.85f, 0.2f, 0.8f}, root);
    b.reg.get<MeshRendererComponent>(ring).castShadows = false;
    b.hide(ring);
    auto& agent = b.reg.emplace<NavAgentComponent>(root);
    agent.speed = soldier ? 3.2f : 3.6f;
    agent.stoppingDistance = 0.3f;
    b.script(root, "assets://games/scripts/strategy_unit.lua", properties);
    return root;
}

} // namespace

void buildStrategy(Scene& scene) {
    Builder b(scene);
    auto& reg = b.reg;
    b.camera3D({0.0f, 27.0f, 17.0f}, {0.0f, 0.0f, 1.0f}, 45.0f);
    const auto sunLight = templates::sun(scene);
    reg.get<EnvironmentComponent>(sunLight).sky = SkyMode::Gradient;

    const auto ground = b.mesh("Ground", PrimitiveMesh::Plane, {}, {44.0f, 1.0f, 28.0f}, {0.9f, 1.0f, 0.85f, 1.0f});
    reg.get<MeshRendererComponent>(ground).texture = "assets://terrain/grass.png";
    reg.get<MeshRendererComponent>(ground).uvScale = {11.0f, 7.0f};
    reg.emplace<RigidBodyComponent>(ground).type = BodyType3D::Static;
    {
        auto& col = reg.emplace<ColliderComponent>(ground);
        col.size = {1.0f, 0.02f, 1.0f};  // scaled by the transform
        col.offset = {0.0f, -0.01f, 0.0f};
    }
    // Rocks: obstacles the units walk around.
    const auto rocks = b.entity("Rocks");
    const Vec3 rockAt[] = {{-4.0f, 0.0f, -6.0f}, {5.0f, 0.0f, 6.0f}, {0.0f, 0.0f, 0.0f}, {-6.0f, 0.0f, 7.0f}, {6.0f, 0.0f, -7.0f}};
    int n = 0;
    for (Vec3 p : rockAt) {
        const auto r = b.entity("Rock " + std::to_string(++n), p, rocks);
        reg.get<TransformComponent>(r).scale = Vec3{2.2f};
        auto& mr = reg.emplace<MeshRendererComponent>(r);
        mr.model = "assets://3d/rock.glb";
        reg.emplace<RigidBodyComponent>(r).type = BodyType3D::Static;
        auto& col = reg.emplace<ColliderComponent>(r);
        col.size = {0.9f, 0.8f, 0.9f};
        col.offset = {0.0f, 0.4f, 0.0f};
    }
    // Bases and crystal mines.
    const auto blueBase = b.mesh("Blue Base", PrimitiveMesh::Cube, {-17.0f, 1.25f, 0.0f}, {3.0f, 2.5f, 3.0f}, {0.25f, 0.45f, 0.9f, 1.0f});
    const auto redBase = b.mesh("Red Base", PrimitiveMesh::Cube, {17.0f, 1.25f, 0.0f}, {3.0f, 2.5f, 3.0f}, {0.9f, 0.3f, 0.25f, 1.0f});
    for (const auto base : {blueBase, redBase}) {
        reg.emplace<RigidBodyComponent>(base).type = BodyType3D::Static;
        reg.emplace<ColliderComponent>(base).size = {1.0f, 1.0f, 1.0f};
        b.mesh("Roof", PrimitiveMesh::Cube, {0.0f, 0.65f, 0.0f}, {1.15f, 0.3f, 1.15f}, {0.35f, 0.3f, 0.28f, 1.0f}, base);
    }
    const auto mines = b.entity("Mines");
    const Vec3 mineAt[] = {{-9.0f, 0.6f, -8.0f}, {-9.0f, 0.6f, 8.0f}, {9.0f, 0.6f, -8.0f}, {9.0f, 0.6f, 8.0f}};
    n = 0;
    for (Vec3 p : mineAt) {
        const auto m = b.mesh("Mine " + std::to_string(++n), PrimitiveMesh::Sphere, {p.x, 1.1f, p.z}, {2.2f, 2.6f, 2.2f}, {0.7f, 0.45f, 1.0f, 1.0f}, mines);
        reg.get<MeshRendererComponent>(m).roughness = 0.15f;
        auto& light = reg.emplace<PointLightComponent>(m);
        light.color = {0.7f, 0.5f, 1.0f, 1.0f};
        light.intensity = 4.0f;
        light.range = 5.0f;
    }
    const auto prototypes = b.entity("Prototypes");
    b.hide(prototypes);
    unitPrototype(b, "Blue Worker", prototypes, {0.3f, 0.5f, 0.95f, 1.0f}, false, R"({"team": "blue", "kind": "worker", "hp": 3})");
    unitPrototype(b, "Blue Soldier", prototypes, {0.2f, 0.35f, 0.85f, 1.0f}, true, R"({"team": "blue", "kind": "soldier", "hp": 6})");
    unitPrototype(b, "Red Soldier", prototypes, {0.85f, 0.25f, 0.2f, 1.0f}, true, R"({"team": "red", "kind": "soldier", "hp": 6})");
    unitPrototype(b, "Red Worker", prototypes, {0.95f, 0.45f, 0.35f, 1.0f}, false, R"({"team": "red", "kind": "worker", "hp": 3})");

    const auto nav = b.entity("Navigation");
    auto& ns = reg.emplace<NavigationSettingsComponent>(nav);
    ns.cellSize = 0.25f;
    ns.agentRadius = 0.35f;

    const auto game = b.entity("Game");
    b.script(game, "assets://games/scripts/strategy_game.lua");
    templates::audioSource(scene, "Music", "assets://audio/music.wav", AudioBus::Music, 0.3f);

    const auto hud = b.hud();
    const auto top = b.panel("Top Panel", hud, {0.0f, 0.0f}, {30.0f, 30.0f}, {620.0f, 120.0f});
    b.label("CrystalsText", top, {0.0f, 0.0f}, {24.0f, 10.0f}, {580.0f, 50.0f}, "", 40.0f, UIHAlign::Left, {0.85f, 0.7f, 1.0f, 1.0f});
    b.label("ArmyText", top, {0.0f, 0.0f}, {24.0f, 62.0f}, {580.0f, 46.0f}, "", 34.0f);
    b.label("BasesText", hud, {1.0f, 0.0f}, {-30.0f, 30.0f}, {800.0f, 60.0f}, "", 36.0f, UIHAlign::Right);
    b.label("Hint", hud, {0.5f, 0.0f}, {0.0f, 110.0f}, {1300.0f, 60.0f}, "", 30.0f, UIHAlign::Center, {1.0f, 1.0f, 0.85f, 1.0f});
    b.label("Message", hud, {0.5f, 0.35f}, {0.0f, 0.0f}, {1400.0f, 120.0f}, "", 60.0f, UIHAlign::Center, {1.0f, 0.9f, 0.5f, 1.0f});
    const char* buttons[][3] = {{"TrainWorker", "Ouvrier (50)", "strat.worker"}, {"TrainSoldier", "Soldat (80)", "strat.soldier"},
                                {"SelectArmy", "Armée", "strat.army"}};
    for (int i = 0; i < 3; ++i) {
        const auto bt = b.button(buttons[i][0], hud, {0.5f, 1.0f}, {(static_cast<float>(i) - 1.0f) * 360.0f, -30.0f}, {340.0f, 130.0f}, buttons[i][1], game,
                                 i == 2 ? "selectArmy" : "train");
        b.buttonText(bt).localizationKey = buttons[i][2];
        reg.get<UIRectComponent>(bt).pivot = {0.5f, 1.0f};
    }
    const auto restart = b.button("Restart", hud, {0.5f, 0.5f}, {0.0f, 0.0f}, {420.0f, 120.0f}, "Rejouer", game, "restart");
    b.buttonText(restart).localizationKey = "game.restart";
    b.hide(restart);
}

} // namespace agr::editor::games
