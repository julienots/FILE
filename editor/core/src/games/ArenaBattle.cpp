// "Duel des Tours" — a real-time card battle in the spirit of mobile arena games: two sides
// separated by a river with two bridges, three towers each (king + two princesses), elixir that
// refills over time, a hand of four cards from a deck of six (knight, archers, giant, goblins,
// fireball, mini tank) dragged onto your half to deploy, units that find their way and fight, an
// AI opponent, a three-minute match with double elixir at the end, crowns, trophies.
// games/scripts/duel_*.lua run the game.
#include "../GameTemplates.hpp"
#include "agr/navigation/NavigationComponents.hpp"

namespace agr::editor::games {

namespace {

constexpr Vec4 kBlue{0.25f, 0.5f, 1.0f, 1.0f}, kRed{0.95f, 0.3f, 0.25f, 1.0f};

ecs::Entity ground(Builder& b, const std::string& name, Vec3 center, Vec3 size, Vec4 color, const char* texture, ecs::Entity parent) {
    const auto e = b.mesh(name, PrimitiveMesh::Cube, center, size, color, parent);
    auto& mr = b.reg.get<MeshRendererComponent>(e);
    mr.texture = texture;
    mr.uvScale = {size.x / 2.0f, size.z / 2.0f};
    mr.roughness = 0.9f;
    b.reg.emplace<RigidBodyComponent>(e).type = BodyType3D::Static;
    b.reg.emplace<ColliderComponent>(e);
    return e;
}

/// Health bar floating above an entity: a dark back and a colored fill scaled by the script.
void healthBar(Builder& b, ecs::Entity parent, float height, float width, Vec4 color) {
    const auto back = b.mesh("HpBack", PrimitiveMesh::Cube, {0.0f, height, 0.0f}, {width, 0.12f, 0.12f}, {0.1f, 0.1f, 0.1f, 1.0f}, parent);
    b.reg.get<MeshRendererComponent>(back).castShadows = false;
    const auto fill = b.mesh("HpFill", PrimitiveMesh::Cube, {0.0f, height, -0.02f}, {width, 0.14f, 0.12f}, color, parent);
    b.reg.get<MeshRendererComponent>(fill).castShadows = false;
}

ecs::Entity tower(Builder& b, const std::string& name, ecs::Entity parent, Vec3 pos, Vec4 color, bool king, const std::string& team) {
    const auto t = b.entity(name, pos, parent);
    const float s = king ? 1.35f : 1.0f;
    const auto base = b.mesh("Base", PrimitiveMesh::Cube, {0.0f, 1.0f * s, 0.0f}, Vec3{1.8f, 2.0f, 1.8f} * s, {0.75f, 0.72f, 0.68f, 1.0f}, t);
    b.reg.get<MeshRendererComponent>(base).texture = "assets://terrain/rock.png";
    b.mesh("Roof", PrimitiveMesh::Cube, {0.0f, 2.15f * s, 0.0f}, Vec3{2.1f, 0.35f, 2.1f} * s, color, t);
    b.mesh(king ? "Crown" : "Flag", PrimitiveMesh::Sphere, {0.0f, 2.7f * s, 0.0f}, Vec3{king ? 0.9f : 0.6f}, king ? Vec4{1.0f, 0.82f, 0.25f, 1.0f} : color, t);
    b.reg.emplace<RigidBodyComponent>(t).type = BodyType3D::Static;
    auto& col = b.reg.emplace<ColliderComponent>(t);
    col.size = Vec3{1.8f, 2.0f, 1.8f} * s;
    col.offset = {0.0f, 1.0f * s, 0.0f};
    healthBar(b, t, 3.5f * s, 2.0f, color);
    b.script(t, "assets://games/scripts/duel_tower.lua",
             "{\"team\": \"" + team + "\", \"king\": " + (king ? "true" : "false") + ", \"hp\": " + (king ? "2400" : "1400") + "}");
    return t;
}

/// Unit prototypes (hidden): blocky soldiers tinted by team when cloned.
ecs::Entity unit(Builder& b, const std::string& name, ecs::Entity parent, const std::string& kind) {
    const auto u = b.entity(name, {}, parent);
    const bool giant = kind == "giant", goblin = kind == "goblin", tank = kind == "tank", archer = kind == "archer";
    const float s = giant ? 1.6f : goblin ? 0.7f : 1.0f;
    if (tank) {
        b.mesh("Body", PrimitiveMesh::Cube, {0.0f, 0.45f, 0.0f}, {1.3f, 0.6f, 0.9f}, {0.5f, 0.5f, 0.55f, 1.0f}, u);
        b.mesh("Turret", PrimitiveMesh::Cube, {0.0f, 0.9f, 0.0f}, {0.6f, 0.35f, 0.6f}, kBlue, u);
        b.mesh("Gun", PrimitiveMesh::Cube, {0.55f, 0.9f, 0.0f}, {0.8f, 0.12f, 0.12f}, {0.3f, 0.3f, 0.32f, 1.0f}, u);
    } else {
        const Vec4 skin = goblin ? Vec4{0.45f, 0.8f, 0.35f, 1.0f} : Vec4{0.95f, 0.8f, 0.65f, 1.0f};
        b.mesh("Body", PrimitiveMesh::Cube, {0.0f, 0.5f * s, 0.0f}, Vec3{0.55f, 0.75f, 0.4f} * s, kBlue, u);
        b.mesh("Head", PrimitiveMesh::Sphere, {0.0f, 1.08f * s, 0.0f}, Vec3{0.42f} * s, skin, u);
        if (archer) b.mesh("Bow", PrimitiveMesh::Cube, {0.35f * s, 0.6f * s, 0.0f}, Vec3{0.06f, 0.7f, 0.06f} * s, {0.55f, 0.35f, 0.15f, 1.0f}, u);
        else b.mesh("Weapon", PrimitiveMesh::Cube, {0.38f * s, 0.65f * s, 0.0f}, Vec3{0.08f, 0.8f, 0.08f} * s, {0.8f, 0.8f, 0.85f, 1.0f}, u);
    }
    healthBar(b, u, (giant ? 2.4f : goblin ? 1.3f : tank ? 1.6f : 1.7f), giant ? 1.4f : 0.9f, kBlue);
    auto& agent = b.reg.emplace<NavAgentComponent>(u);
    agent.speed = giant ? 1.3f : goblin ? 3.4f : tank ? 1.6f : 2.2f;
    agent.stoppingDistance = 0.2f;
    agent.faceMovement = true;
    b.script(u, "assets://games/scripts/duel_unit.lua", "{\"kind\": \"" + kind + "\"}");
    return u;
}

} // namespace

void buildArenaBattle(Scene& scene) {
    Builder b(scene);
    auto& reg = b.reg;
    b.camera3D({0.0f, 21.0f, 14.5f}, {0.0f, 0.0f, 2.3f}, 44.0f);
    const auto sunLight = templates::sun(scene);
    reg.get<EnvironmentComponent>(sunLight).sky = SkyMode::Gradient;

    // Arena: two halves, a river, two bridges (the navigation mesh crosses the river only there).
    const auto arena = b.entity("Arena");
    ground(b, "Blue Side", {-8.0f, -0.5f, 0.0f}, {14.0f, 1.0f, 16.0f}, {0.85f, 1.0f, 0.85f, 1.0f}, "assets://terrain/grass.png", arena);
    ground(b, "Red Side", {8.0f, -0.5f, 0.0f}, {14.0f, 1.0f, 16.0f}, {1.0f, 0.95f, 0.85f, 1.0f}, "assets://terrain/grass.png", arena);
    const auto river = b.mesh("River", PrimitiveMesh::Plane, {0.0f, -0.35f, 0.0f}, {2.0f, 1.0f, 16.0f}, {0.3f, 0.6f, 0.95f, 1.0f}, arena);
    reg.get<MeshRendererComponent>(river).roughness = 0.1f;
    for (float z : {-4.5f, 4.5f})
        ground(b, z < 0 ? "Bridge N" : "Bridge S", {0.0f, -0.45f, z}, {2.2f, 0.9f, 2.2f}, {0.65f, 0.45f, 0.3f, 1.0f}, "assets://terrain/dirt.png", arena);
    for (float x : {-15.5f, 15.5f}) ground(b, x < 0 ? "Wall W" : "Wall E", {x, 0.5f, 0.0f}, {1.0f, 1.0f, 16.0f}, {0.6f, 0.58f, 0.55f, 1.0f}, "assets://terrain/rock.png", arena);
    for (float z : {-8.5f, 8.5f}) ground(b, z < 0 ? "Wall N" : "Wall S", {0.0f, 0.5f, z}, {32.0f, 1.0f, 1.0f}, {0.6f, 0.58f, 0.55f, 1.0f}, "assets://terrain/rock.png", arena);
    // Deploy zone highlight (shown while dragging a card).
    const auto zone = b.mesh("Deploy Zone", PrimitiveMesh::Plane, {-8.0f, 0.03f, 0.0f}, {14.0f, 1.0f, 16.0f}, {0.3f, 0.6f, 1.0f, 0.18f}, arena);
    reg.get<MeshRendererComponent>(zone).castShadows = false;
    b.hide(zone);
    const auto marker = b.mesh("Drop Marker", PrimitiveMesh::Sphere, {0.0f, 0.1f, 0.0f}, {1.2f, 0.1f, 1.2f}, {1.0f, 1.0f, 1.0f, 0.6f}, arena);
    reg.get<MeshRendererComponent>(marker).castShadows = false;
    b.hide(marker);

    // Towers.
    const auto towers = b.entity("Towers");
    tower(b, "Blue King", towers, {-12.5f, 0.0f, 0.0f}, kBlue, true, "blue");
    tower(b, "Blue Princess N", towers, {-9.0f, 0.0f, -4.5f}, kBlue, false, "blue");
    tower(b, "Blue Princess S", towers, {-9.0f, 0.0f, 4.5f}, kBlue, false, "blue");
    tower(b, "Red King", towers, {12.5f, 0.0f, 0.0f}, kRed, true, "red");
    tower(b, "Red Princess N", towers, {9.0f, 0.0f, -4.5f}, kRed, false, "red");
    tower(b, "Red Princess S", towers, {9.0f, 0.0f, 4.5f}, kRed, false, "red");

    const auto units = b.entity("Units");
    (void)units;
    const auto prototypes = b.entity("Prototypes");
    b.hide(prototypes);
    for (const char* kind : {"knight", "archer", "giant", "goblin", "tank"}) {
        std::string name = kind;
        name[0] = static_cast<char>(name[0] - 'a' + 'A');
        unit(b, name + " Prototype", prototypes, kind);
    }
    const auto shot = b.mesh("Shot Prototype", PrimitiveMesh::Sphere, {}, Vec3{0.3f}, {1.0f, 0.9f, 0.5f, 1.0f}, prototypes);
    reg.get<MeshRendererComponent>(shot).castShadows = false;

    const auto nav = b.entity("Navigation");
    auto& ns = reg.emplace<NavigationSettingsComponent>(nav);
    ns.cellSize = 0.25f;
    ns.agentRadius = 0.3f;

    const auto game = b.entity("Game");
    b.script(game, "assets://games/scripts/duel_game.lua");
    templates::audioSource(scene, "Music", "assets://audio/music.wav", AudioBus::Music, 0.3f);

    // ---- interface: crowns, timer, elixir, hand of cards, menu.
    const auto hud = b.hud();
    b.label("TimerText", hud, {0.5f, 0.0f}, {0.0f, 20.0f}, {400.0f, 70.0f}, "3:00", 56.0f, UIHAlign::Center);
    b.label("CrownsText", hud, {0.0f, 0.0f}, {30.0f, 30.0f}, {600.0f, 60.0f}, "", 44.0f, UIHAlign::Left, {1.0f, 0.85f, 0.3f, 1.0f});
    b.label("TrophiesText", hud, {1.0f, 0.0f}, {-30.0f, 30.0f}, {500.0f, 60.0f}, "", 38.0f, UIHAlign::Right);
    b.label("Message", hud, {0.5f, 0.25f}, {0.0f, 0.0f}, {1400.0f, 110.0f}, "", 60.0f, UIHAlign::Center, {1.0f, 0.95f, 0.6f, 1.0f});
    const auto tray = b.panel("Card Tray", hud, {0.5f, 1.0f}, {0.0f, 0.0f}, {1300.0f, 300.0f}, {0.05f, 0.07f, 0.12f, 0.75f});
    reg.get<UIRectComponent>(tray).pivot = {0.5f, 1.0f};
    for (int i = 0; i < 4; ++i) {
        const auto card = b.button("Card " + std::to_string(i + 1), tray, {0.0f, 0.0f}, {230.0f + static_cast<float>(i) * 230.0f, 20.0f}, {210.0f, 190.0f}, "", game,
                                   "cardTapped");
        reg.get<UIRectComponent>(card).pivot = {0.5f, 0.0f};
        b.buttonText(card).fontSize = 30.0f;
    }
    b.label("NextCard", tray, {0.0f, 0.0f}, {20.0f, 40.0f}, {110.0f, 140.0f}, "", 24.0f, UIHAlign::Center, {0.8f, 0.85f, 1.0f, 0.8f});
    // Elixir bar: 10 segments.
    const auto bar = b.panel("Elixir Bar", tray, {0.0f, 0.0f}, {150.0f, 225.0f}, {1000.0f, 50.0f}, {0.15f, 0.1f, 0.2f, 0.9f});
    for (int i = 0; i < 10; ++i)
        b.panel("Elixir " + std::to_string(i + 1), bar, {0.0f, 0.0f}, {5.0f + static_cast<float>(i) * 99.0f, 5.0f}, {94.0f, 40.0f}, {0.85f, 0.3f, 0.95f, 1.0f});
    b.label("ElixirText", tray, {0.0f, 0.0f}, {1160.0f, 220.0f}, {120.0f, 60.0f}, "5", 44.0f, UIHAlign::Center, {1.0f, 0.7f, 1.0f, 1.0f});
    // Menu (start / results).
    const auto menu = b.panel("Menu", hud, {0.5f, 0.5f}, {0.0f, 0.0f}, {900.0f, 640.0f}, {0.05f, 0.07f, 0.12f, 0.95f});
    b.label("MenuTitle", menu, {0.5f, 0.0f}, {0.0f, 40.0f}, {860.0f, 100.0f}, "Duel des Tours", 72.0f, UIHAlign::Center, {1.0f, 0.85f, 0.3f, 1.0f});
    b.label("MenuText", menu, {0.5f, 0.0f}, {0.0f, 160.0f}, {820.0f, 260.0f}, "", 34.0f, UIHAlign::Center);
    const auto battle = b.button("BattleButton", menu, {0.5f, 1.0f}, {0.0f, -50.0f}, {520.0f, 110.0f}, "Combat !", game, "startBattle");
    reg.get<UIRectComponent>(battle).pivot = {0.5f, 1.0f};
    b.buttonText(battle).localizationKey = "duel.battle";
    reg.get<UICanvasComponent>(hud).keyboardNavigation = true;
}

} // namespace agr::editor::games
