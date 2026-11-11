// RPG 2D (top-down): explore, talk to the sage, find three gems, fight bats (pathfinding),
// gain experience and levels; progress kept in the local save.
#include "../GameTemplates.hpp"
#include "agr/navigation/NavigationComponents.hpp"
#include "agr/twod/TilemapEditing.hpp"

namespace agr::editor::games {

namespace {
// Tiles of assets/games/topdown_tiles.agrsheet.
enum : i32 { Grass = 0, Tufts = 1, Path = 2, Water = 3, Wall = 4, Sand = 5, Forest = 6, Floor = 7, Flowers = 8, Rock = 11, Fence = 12, Bridge = 13 };
constexpr int kW = 40, kH = 30;
}

void buildRpg2D(Scene& scene) {
    Builder b(scene);
    auto& reg = b.reg;
    const auto cam = b.camera2D(6.0f, {0.10f, 0.14f, 0.10f, 1.0f});
    b.script(cam, "assets://games/scripts/follow_camera.lua", R"({"target": "Hero", "minX": -9.3, "maxX": 9.3, "minY": -9, "maxY": 9})");
    const auto env = b.entity("Ambience");
    auto& en = reg.emplace<EnvironmentComponent>(env);
    en.ambientColor = {1.0f, 1.0f, 1.0f, 1.0f};
    en.ambientIntensity = 1.0f;

    // Ground (walkable) and walls (solid: border, lake, rocks, fences). 40 x 30 tiles from (-20, -15).
    const auto ground = b.entity("Ground", {-20.0f, -15.0f, 0.0f});
    {
        auto& tm = reg.emplace<TilemapComponent>(ground);
        tm.tileset = "assets://games/topdown_tiles.agrsheet";
        tm.width = kW;
        tm.height = kH;
        tm.sortingLayer = -5;
        tilemapEnsureLayout(tm);
        for (int y = 0; y < kH; ++y)
            for (int x = 0; x < kW; ++x) tilemapSet(tm, x, y, (x * 7 + y * 13) % 11 == 0 ? Tufts : (x * 5 + y * 3) % 17 == 0 ? Flowers : Grass);
        tilemapFillRect(tm, 1, 14, 38, 15, Path);  // crossroads
        tilemapFillRect(tm, 19, 1, 20, 28, Path);
        tilemapFillRect(tm, 25, 3, 34, 11, Sand);
        tilemapFillRect(tm, 2, 20, 9, 27, Forest);
        tilemapFillRect(tm, 14, 16, 19, 19, Floor);  // the sage's terrace
    }
    const auto walls = b.entity("Walls", {-20.0f, -15.0f, 0.0f});
    {
        auto& tm = reg.emplace<TilemapComponent>(walls);
        tm.tileset = "assets://games/topdown_tiles.agrsheet";
        tm.width = kW;
        tm.height = kH;
        tm.sortingLayer = -4;
        tilemapEnsureLayout(tm);
        tilemapFillRect(tm, 0, 0, kW - 1, 0, Wall);
        tilemapFillRect(tm, 0, kH - 1, kW - 1, kH - 1, Wall);
        tilemapFillRect(tm, 0, 0, 0, kH - 1, Wall);
        tilemapFillRect(tm, kW - 1, 0, kW - 1, kH - 1, Wall);
        tilemapFillRect(tm, 26, 4, 33, 10, Water);  // lake
        tilemapFillRect(tm, 29, 4, 30, 10, -1);     // with a bridge (drawn on the ground layer below)
        for (auto [x, y] : {std::pair{6, 6}, {12, 24}, {33, 22}, {24, 20}, {9, 11}, {35, 16}}) tilemapSet(tm, x, y, Rock);
        tilemapFillRect(tm, 13, 16, 13, 19, Fence);
        tilemapFillRect(tm, 20 + 1, 19, 22, 19, Fence);
    }
    reg.emplace<TilemapCollider2DComponent>(walls);
    tilemapFillRect(reg.get<TilemapComponent>(ground), 29, 4, 30, 10, Bridge);

    const auto trees = b.entity("Trees");
    const Vec2 treeAt[] = {{-15, 9}, {-12, 11}, {-16, 5}, {-9, 9}, {-5, -10}, {-2, -12}, {3, -9}, {6, 10}, {9, 12}, {13, 4}, {-14, -4}, {16, -10}, {-7, 3}, {7, 4}};
    for (int i = 0; i < 14; ++i) {
        const auto t = b.sprite("Tree " + std::to_string(i + 1), "assets://games/tree.png", {treeAt[i].x, treeAt[i].y, 0.0f}, {2.2f, 2.2f}, 2, trees);
        b.spriteOf(t).pivot = {0.5f, 0.12f};
        b.body2D(t, BodyType2D::Static);
        b.circle2D(t, 0.35f, false, {0.0f, 0.2f});
    }

    // Hero: top-down movement (no gravity), the RPG script.
    const auto hero = b.sprite("Hero", "assets://games/hero_top.png", {0.0f, 0.0f, 0.0f}, {1.3f, 1.3f}, 1);
    b.spriteOf(hero).pivot = {0.5f, 0.12f};
    {
        auto& body = b.body2D(hero, BodyType2D::Dynamic);
        body.gravityScale = 0.0f;
        body.linearDamping = 8.0f;
    }
    b.circle2D(hero, 0.32f, false, {0.0f, 0.3f}).friction = 0.0f;
    b.script(hero, "assets://games/scripts/rpg_hero.lua");

    const auto npc = b.sprite("Sage", "assets://games/npc.png", {-3.0f, 2.5f, 0.0f}, {1.3f, 1.3f}, 1);
    b.spriteOf(npc).pivot = {0.5f, 0.12f};
    b.body2D(npc, BodyType2D::Static);
    b.circle2D(npc, 0.35f, false, {0.0f, 0.3f});
    b.script(npc, "assets://games/scripts/rpg_npc.lua");

    const auto gems = b.entity("Gems");
    const Vec2 gemAt[] = {{12.5f, 8.5f}, {-14.5f, -9.5f}, {8.5f, -11.5f}};
    for (int i = 0; i < 3; ++i) {
        const auto g = b.sprite("Gem " + std::to_string(i + 1), "assets://games/gem.png", {gemAt[i].x, gemAt[i].y, 0.0f}, {0.7f, 0.7f}, 1, gems);
        b.circle2D(g, 0.35f, true);
        b.script(g, "assets://games/scripts/pickup.lua", R"({"kind": "gem", "sound": "assets://audio/coin.wav"})");
    }

    // Bats: follow the hero along the navigation grid (walls, lake, trees and rocks are avoided).
    const auto enemies = b.entity("Enemies");
    const Vec2 batAt[] = {{10.0f, -5.0f}, {-12.0f, 7.0f}, {14.0f, 10.0f}, {-8.0f, -8.0f}};
    for (int i = 0; i < 4; ++i) {
        const auto e = b.sprite("Bat " + std::to_string(i + 1), "assets://games/bat.png", {batAt[i].x, batAt[i].y, 0.0f}, {1.2f, 1.2f}, 3, enemies);
        auto& agent = reg.emplace<NavAgentComponent>(e);
        agent.speed = 2.4f;
        agent.faceMovement = false;
        agent.stoppingDistance = 0.3f;
        b.script(e, "assets://games/scripts/rpg_enemy.lua");
    }
    const auto nav = b.entity("Navigation");
    reg.emplace<NavigationSettingsComponent>(nav).gridCellSize = 0.5f;

    templates::audioSource(scene, "Music", "assets://audio/ambient.wav", AudioBus::Music, 0.4f);

    // HUD.
    const auto hud = b.hud();
    const auto stats = b.panel("Stats Panel", hud, {0.0f, 0.0f}, {30.0f, 30.0f}, {600.0f, 170.0f});
    b.label("HpText", stats, {0.0f, 0.0f}, {24.0f, 10.0f}, {560.0f, 50.0f}, "PV : 10 / 10", 38.0f);
    b.panel("HpBar", stats, {0.0f, 0.0f}, {24.0f, 66.0f}, {552.0f, 26.0f}, {0.2f, 0.05f, 0.05f, 0.9f});
    b.panel("HpFill", stats, {0.0f, 0.0f}, {24.0f, 66.0f}, {552.0f, 26.0f}, {0.85f, 0.2f, 0.25f, 1.0f});
    b.label("LevelText", stats, {0.0f, 0.0f}, {24.0f, 104.0f}, {560.0f, 50.0f}, "Niveau 1 — XP 0 / 5", 34.0f);
    const auto quest = b.panel("Quest Panel", hud, {1.0f, 0.0f}, {-30.0f, 30.0f}, {640.0f, 120.0f});
    b.label("QuestText", quest, {0.0f, 0.0f}, {20.0f, 10.0f}, {600.0f, 100.0f}, "", 32.0f);
    const auto dialog = b.panel("Dialog", hud, {0.5f, 1.0f}, {0.0f, -230.0f}, {1200.0f, 230.0f}, {0.06f, 0.05f, 0.1f, 0.88f});
    b.label("DialogText", dialog, {0.0f, 0.0f}, {30.0f, 20.0f}, {1140.0f, 190.0f}, "", 36.0f);
    b.hide(dialog);
    b.label("Hint", hud, {0.5f, 1.0f}, {0.0f, -40.0f}, {800.0f, 60.0f}, "", 34.0f, UIHAlign::Center, {1.0f, 0.95f, 0.6f, 1.0f});
    b.label("Message", hud, {0.5f, 0.3f}, {0.0f, 0.0f}, {1400.0f, 120.0f}, "", 64.0f, UIHAlign::Center, {1.0f, 0.9f, 0.5f, 1.0f});
    const auto restart = b.button("Restart", hud, {0.5f, 0.55f}, {0.0f, 0.0f}, {420.0f, 120.0f}, "Rejouer", hero, "restart");
    b.buttonText(restart).localizationKey = "game.restart";
    b.hide(restart);
    const auto attack = templates::uiButton(scene, "Attack", hud, {1.0f, 1.0f}, {-40.0f, -40.0f}, {230.0f, 200.0f}, "Épée");
    reg.get<UITextComponent>(scene.children(attack).front()).localizationKey = "rpg.attack";
    const auto talk = templates::uiButton(scene, "Talk", hud, {1.0f, 1.0f}, {-300.0f, -40.0f}, {230.0f, 200.0f}, "Parler");
    reg.get<UITextComponent>(scene.children(talk).front()).localizationKey = "rpg.talk";
}

} // namespace agr::editor::games
