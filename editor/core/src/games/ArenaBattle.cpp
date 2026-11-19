// "Duel des Tours" — a real-time card battle in the spirit of mobile arena games: two sides
// separated by a river with two bridges, three towers each (king + two princesses), elixir that
// refills over time, a hand of four cards from a deck of six (knight, archers, giant, goblins,
// fireball, mini tank) dragged onto your half to deploy, units that find their way and fight, an
// AI opponent, a three-minute match with double elixir at the end, crowns, trophies.
// games/scripts/duel_*.lua run the game.
#include "../GameTemplates.hpp"
#include "agr/animation/AnimationSystem.hpp"
#include "agr/navigation/NavigationComponents.hpp"

namespace agr::editor::games {

namespace {

constexpr Vec4 kBlue{0.25f, 0.5f, 1.0f, 1.0f}, kRed{0.95f, 0.3f, 0.25f, 1.0f};

/// Skeleton shared by the characters of tools/generate_duel_art.py (JOINTS there): name,
/// parent index, translation relative to the parent. The entities below a unit mirror the
/// model's nodes so the skin and the clips ("Model/Hips/Spine"...) find their joints.
struct JointDef {
    const char* name;
    int parent;
    Vec3 local;
};
constexpr JointDef kJoints[] = {{"Hips", -1, {0.0f, 0.50f, 0.0f}},   {"Spine", 0, {0.0f, 0.22f, 0.0f}}, {"Head", 1, {0.0f, 0.28f, 0.0f}},
                                {"ArmL", 1, {0.31f, 0.18f, 0.0f}},   {"ArmR", 1, {-0.31f, 0.18f, 0.0f}},
                                {"LegL", 0, {0.13f, -0.04f, 0.0f}},  {"LegR", 0, {-0.13f, -0.04f, 0.0f}}};

/// The node hierarchy of a duel model under `parent`: "Model" > two meshes (fixed colors, then
/// team-colored parts tinted by `team`) and, for (skinned) characters, the joints.
ecs::Entity modelInstance(Builder& b, ecs::Entity parent, const std::string& file, float scale, Vec4 team, bool character) {
    Scene& scene = b.scene;
    const auto model = scene.createEntity("Model", parent);
    b.reg.get<TransformComponent>(model).scale = Vec3{scale};
    const bool tower = file.find("tower") != std::string::npos;
    const char* names[2] = {tower ? "Base" : "Skin", tower ? "Roof" : "Body"};  // "Body": tinted by duel_unit.lua
    for (int i = 0; i < 2; ++i) {
        const auto e = scene.createEntity(names[i], model);
        auto& mr = b.reg.emplace<MeshRendererComponent>(e);
        mr.model = "assets://games/duel/" + file + ".glb";
        mr.meshIndex = i;
        mr.skin = character ? 0 : -1;
        if (i == 1) mr.color = team;
    }
    if (character) {
        ecs::Entity joints[std::size(kJoints)];
        for (usize j = 0; j < std::size(kJoints); ++j) {
            joints[j] = scene.createEntity(kJoints[j].name, kJoints[j].parent < 0 ? model : joints[kJoints[j].parent]);
            b.reg.get<TransformComponent>(joints[j]).position = kJoints[j].local;
        }
    }
    return model;
}

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
    // Round stone tower, team roof; the king's is larger and crowned (tools/generate_duel_art.py).
    const auto model = modelInstance(b, t, king ? "king_tower" : "tower", 1.0f, color, false);
    // Doors toward the river (the model only: the health bar keeps facing the camera).
    b.reg.get<TransformComponent>(model).rotation = math::Quat::fromAxisAngle({0.0f, 1.0f, 0.0f}, math::radians(pos.x < 0.0f ? 90.0f : -90.0f));
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
    const bool giant = kind == "giant", goblin = kind == "goblin", tank = kind == "tank";
    const float s = giant ? 1.6f : goblin ? 0.7f : 1.0f;
    if (tank) {
        modelInstance(b, u, "tank", 1.1f, kBlue, false);  // rigid model: no Animator
    } else {
        // Animated character: Idle / Walk / Attack driven by duel_unit.lua through the Animator.
        modelInstance(b, u, kind, s * 1.3f, kBlue, true);  // 1.3: readable from the high camera
        b.reg.emplace<AnimatorComponent>(u).controller = "assets://games/duel/" + kind + ".agrctrl";
    }
    healthBar(b, u, (giant ? 3.1f : goblin ? 1.6f : tank ? 1.6f : 2.25f), giant ? 1.4f : 0.9f, kBlue);
    auto& agent = b.reg.emplace<NavAgentComponent>(u);
    agent.speed = giant ? 1.3f : goblin ? 3.4f : tank ? 1.6f : 2.2f;
    agent.stoppingDistance = 0.2f;
    agent.faceMovement = true;
    b.script(u, "assets://games/scripts/duel_unit.lua", "{\"kind\": \"" + kind + "\"}");
    return u;
}

/// Mobile cartoon look: toon lighting everywhere, outlines on towers and units (their meshes
/// stop casting map shadows: a soft blob shadow under each unit reads better from above).
void cartoonStyle(Scene& scene, ecs::Entity e, bool outline, bool blobs) {
    auto& reg = scene.registry();
    if (auto* mr = reg.tryGet<MeshRendererComponent>(e)) {
        const std::string& name = scene.nameOf(e);
        const bool bar = name == "HpBack" || name == "HpFill";
        const bool transparent = mr->color.w < 0.99f;
        if (!transparent) mr->toon = true;
        if (outline && !bar && !transparent) mr->outlineWidth = 2.0f;
        if (blobs) mr->castShadows = false;
    }
    for (ecs::Entity c : scene.children(e)) cartoonStyle(scene, c, outline, blobs);
}

} // namespace

void buildArenaBattle(Scene& scene) {
    Builder b(scene);
    auto& reg = b.reg;
    b.camera3D({0.0f, 15.5f, 17.5f}, {0.0f, 0.0f, 1.6f}, 46.0f);  // tilted: characters seen from the front
    const auto sunLight = templates::sun(scene);
    auto& env = reg.get<EnvironmentComponent>(sunLight);
    env.sky = SkyMode::Gradient;
    env.ambientIntensity = 0.6f;  // cartoon: shadows stay colorful, never black
    reg.get<DirectionalLightComponent>(sunLight).intensity = 1.05f;

    // Arena: two halves, a river, two bridges (the navigation mesh crosses the river only there).
    const auto arena = b.entity("Arena");
    ground(b, "Blue Side", {-8.0f, -0.5f, 0.0f}, {14.0f, 1.0f, 16.0f}, {0.85f, 1.0f, 0.85f, 1.0f}, "assets://games/duel/grass.png", arena);
    ground(b, "Red Side", {8.0f, -0.5f, 0.0f}, {14.0f, 1.0f, 16.0f}, {1.0f, 0.95f, 0.85f, 1.0f}, "assets://games/duel/grass.png", arena);
    const auto river = b.mesh("River", PrimitiveMesh::Plane, {0.0f, -0.35f, 0.0f}, {2.0f, 1.0f, 16.0f}, {0.3f, 0.6f, 0.95f, 1.0f}, arena);
    reg.get<MeshRendererComponent>(river).roughness = 0.1f;
    for (float z : {-4.5f, 4.5f})
        ground(b, z < 0 ? "Bridge N" : "Bridge S", {0.0f, -0.45f, z}, {2.2f, 0.9f, 2.2f}, {0.65f, 0.45f, 0.3f, 1.0f}, "assets://games/duel/path.png", arena);
    for (float x : {-15.5f, 15.5f}) ground(b, x < 0 ? "Wall W" : "Wall E", {x, 0.5f, 0.0f}, {1.0f, 1.0f, 16.0f}, {0.6f, 0.58f, 0.55f, 1.0f}, "assets://games/duel/stone.png", arena);
    for (float z : {-8.5f, 8.5f}) ground(b, z < 0 ? "Wall N" : "Wall S", {0.0f, 0.5f, z}, {32.0f, 1.0f, 1.0f}, {0.6f, 0.58f, 0.55f, 1.0f}, "assets://games/duel/stone.png", arena);
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

    // Cartoon look (see cartoonStyle): ground and walls toon, towers and units outlined,
    // units on blob shadows (clones made by the scripts keep all of it).
    cartoonStyle(scene, arena, false, false);
    cartoonStyle(scene, towers, true, false);
    for (ecs::Entity proto : scene.children(prototypes)) {
        cartoonStyle(scene, proto, true, true);
        if (reg.has<NavAgentComponent>(proto)) {
            auto& blob = reg.emplace<BlobShadowComponent>(proto);
            blob.radius = scene.nameOf(proto) == "Giant Prototype" ? 1.0f : scene.nameOf(proto) == "Goblin Prototype" ? 0.45f : 0.6f;
        }
    }
    reg.get<MeshRendererComponent>(shot).outlineWidth = 0.0f;  // projectiles: small and fast

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
        auto& label = b.buttonText(card);
        label.fontSize = 28.0f;
        label.vAlign = UIVAlign::Bottom;
        // Portrait of the card's unit (duel_game.lua sets the texture: card_<kind>.png).
        const auto icon = b.panel("Icon", card, {0.5f, 0.0f}, {0.0f, 6.0f}, {112.0f, 112.0f}, {1.0f, 1.0f, 1.0f, 1.0f});
        reg.get<UIRectComponent>(icon).pivot = {0.5f, 0.0f};
        auto& img = reg.get<UIImageComponent>(icon);
        img.texture = "assets://games/duel/card_knight.png";
        img.preserveAspect = true;
        img.blocksPointer = false;
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
