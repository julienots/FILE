// Survival: gather berries and wood, keep a campfire burning, fight off the zombies that come
// at night; hunger drains health; survive three nights.
#include "../GameTemplates.hpp"
#include "agr/navigation/NavigationComponents.hpp"
#include "agr/twod/TilemapEditing.hpp"

namespace agr::editor::games {

void buildSurvival(Scene& scene) {
    Builder b(scene);
    auto& reg = b.reg;
    const auto cam = b.camera2D(6.0f, {0.08f, 0.12f, 0.08f, 1.0f});
    b.script(cam, "assets://games/scripts/follow_camera.lua", R"({"target": "Survivor", "minX": -7.3, "maxX": 7.3, "minY": -6, "maxY": 6})");
    const auto ground = b.entity("Ground", {-18.0f, -12.0f, 0.0f});
    {
        auto& tm = reg.emplace<TilemapComponent>(ground);
        tm.tileset = "assets://games/topdown_tiles.agrsheet";
        tm.width = 36;
        tm.height = 24;
        tm.sortingLayer = -5;
        tilemapEnsureLayout(tm);
        for (int y = 0; y < 24; ++y)
            for (int x = 0; x < 36; ++x) tilemapSet(tm, x, y, (x * 7 + y * 3) % 5 == 0 ? 6 : (x + y * 5) % 13 == 0 ? 1 : 0);
        tilemapFillRect(tm, 15, 9, 20, 14, 5);  // clearing (sand) in the middle
    }
    const auto walls = b.entity("Walls", {-18.0f, -12.0f, 0.0f});
    {
        auto& tm = reg.emplace<TilemapComponent>(walls);
        tm.tileset = "assets://games/topdown_tiles.agrsheet";
        tm.width = 36;
        tm.height = 24;
        tm.sortingLayer = -4;
        tilemapEnsureLayout(tm);
        tilemapFillRect(tm, 0, 0, 35, 0, 4);
        tilemapFillRect(tm, 0, 23, 35, 23, 4);
        tilemapFillRect(tm, 0, 0, 0, 23, 4);
        tilemapFillRect(tm, 35, 0, 35, 23, 4);
        tilemapFillRect(tm, 5, 16, 8, 19, 3);  // pond
        for (auto [x, y] : {std::pair{12, 5}, {26, 18}, {29, 6}, {9, 9}}) tilemapSet(tm, x, y, 11);
    }
    reg.emplace<TilemapCollider2DComponent>(walls);

    const auto trees = b.entity("Trees");
    const Vec2 treeAt[] = {{-11, 1.5f}, {-9, 9.5f}, {-14, -3}, {-6, -8}, {-2, 8}, {4, 9}, {9, 6}, {13, 8}, {14, -2}, {10, -8}, {3, -9}, {-10, -9}, {6, -4}, {-5, 3}};
    for (int i = 0; i < 14; ++i) {
        const auto t = b.sprite("Tree " + std::to_string(i + 1), "assets://games/tree.png", {treeAt[i].x, treeAt[i].y, 0.0f}, {2.2f, 2.2f}, 2, trees);
        b.spriteOf(t).pivot = {0.5f, 0.12f};
        b.body2D(t, BodyType2D::Static);
        b.circle2D(t, 0.35f, false, {0.0f, 0.2f});
    }
    const auto bushes = b.entity("Bushes");
    const Vec2 bushAt[] = {{-7, 6}, {7, 2}, {-12, -6}, {11, -5}, {0, -6}, {15, 4}, {-15, 2}, {2, 5}};
    for (int i = 0; i < 8; ++i) {
        const auto s = b.sprite("Bush " + std::to_string(i + 1), "assets://games/bush.png", {bushAt[i].x, bushAt[i].y, 0.0f}, {1.4f, 1.4f}, 1, bushes);
        b.spriteOf(s).pivot = {0.5f, 0.15f};
    }
    const auto hero = b.sprite("Survivor", "assets://games/survivor.png", {0.0f, -0.5f, 0.0f}, {1.3f, 1.3f}, 1);
    b.spriteOf(hero).pivot = {0.5f, 0.12f};
    {
        auto& body = b.body2D(hero, BodyType2D::Dynamic);
        body.gravityScale = 0.0f;
        body.linearDamping = 8.0f;
    }
    b.circle2D(hero, 0.32f, false, {0.0f, 0.3f}).friction = 0.0f;
    b.script(hero, "assets://games/scripts/survival_player.lua");

    const auto prototypes = b.entity("Prototypes");
    b.hide(prototypes);
    const auto zombie = b.sprite("Zombie Prototype", "assets://games/zombie.png", {}, {1.3f, 1.3f}, 1, prototypes);
    b.spriteOf(zombie).pivot = {0.5f, 0.12f};
    {
        auto& agent = reg.emplace<NavAgentComponent>(zombie);
        agent.speed = 1.9f;
        agent.faceMovement = false;
        agent.stoppingDistance = 0.4f;
    }
    b.script(zombie, "assets://games/scripts/survival_zombie.lua");
    const auto fire = b.sprite("Campfire Prototype", "assets://games/campfire.png", {}, {1.3f, 1.3f}, 1, prototypes);
    b.spriteOf(fire).pivot = {0.5f, 0.15f};
    const auto glow = b.entity("Glow", {0.0f, 0.6f, 0.0f}, fire);
    {
        auto& p = reg.emplace<ParticleEmitter2DComponent>(glow);
        p.rate = 10.0f;
        p.lifetimeMin = 0.5f;
        p.lifetimeMax = 1.0f;
        p.speedMin = 0.5f;
        p.speedMax = 1.2f;
        p.spread = math::radians(25.0f);
        p.startColor = {1.0f, 0.7f, 0.2f, 1.0f};
        p.endColor = {1.0f, 0.3f, 0.1f, 0.0f};
        p.startSize = 0.16f;
        p.endSize = 0.04f;
        p.additive = true;
        p.sortingLayer = 9;
    }
    const auto nav = b.entity("Navigation");
    reg.emplace<NavigationSettingsComponent>(nav).gridCellSize = 0.5f;
    const auto night = b.sprite("Night", "", {0.0f, 0.0f, 0.0f}, {80.0f, 60.0f}, 8);
    b.spriteOf(night).color = {0.02f, 0.02f, 0.1f, 0.0f};
    templates::audioSource(scene, "Music", "assets://audio/ambient.wav", AudioBus::Music, 0.4f);

    const auto hud = b.hud();
    const auto stats = b.panel("Stats Panel", hud, {0.0f, 0.0f}, {30.0f, 30.0f}, {600.0f, 230.0f});
    b.label("HealthText", stats, {0.0f, 0.0f}, {24.0f, 8.0f}, {560.0f, 44.0f}, "", 34.0f);
    b.panel("HealthBar", stats, {0.0f, 0.0f}, {24.0f, 56.0f}, {552.0f, 20.0f}, {0.2f, 0.05f, 0.05f, 0.9f});
    b.panel("HealthFill", stats, {0.0f, 0.0f}, {24.0f, 56.0f}, {552.0f, 20.0f}, {0.85f, 0.2f, 0.25f, 1.0f});
    b.label("HungerText", stats, {0.0f, 0.0f}, {24.0f, 84.0f}, {560.0f, 44.0f}, "", 34.0f);
    b.panel("HungerBar", stats, {0.0f, 0.0f}, {24.0f, 132.0f}, {552.0f, 20.0f}, {0.2f, 0.15f, 0.05f, 0.9f});
    b.panel("HungerFill", stats, {0.0f, 0.0f}, {24.0f, 132.0f}, {552.0f, 20.0f}, {0.95f, 0.65f, 0.2f, 1.0f});
    b.label("WoodText", stats, {0.0f, 0.0f}, {24.0f, 164.0f}, {560.0f, 50.0f}, "", 36.0f);
    b.label("TimeText", hud, {1.0f, 0.0f}, {-30.0f, 30.0f}, {700.0f, 60.0f}, "", 40.0f, UIHAlign::Right);
    b.label("Hint", hud, {0.5f, 1.0f}, {0.0f, -40.0f}, {1000.0f, 60.0f}, "", 32.0f, UIHAlign::Center, {1.0f, 0.95f, 0.6f, 1.0f});
    b.label("Message", hud, {0.5f, 0.3f}, {0.0f, 0.0f}, {1400.0f, 120.0f}, "", 60.0f, UIHAlign::Center, {1.0f, 0.9f, 0.5f, 1.0f});
    const auto action = templates::uiButton(scene, "Action", hud, {1.0f, 1.0f}, {-40.0f, -40.0f}, {240.0f, 200.0f}, "Action");
    reg.get<UITextComponent>(scene.children(action).front()).localizationKey = "surv.action";
    const auto craft = templates::uiButton(scene, "Craft", hud, {1.0f, 1.0f}, {-300.0f, -40.0f}, {240.0f, 200.0f}, "Feu");
    reg.get<UITextComponent>(scene.children(craft).front()).localizationKey = "surv.craft";
    const auto restart = b.button("Restart", hud, {0.5f, 0.5f}, {0.0f, 0.0f}, {420.0f, 120.0f}, "Rejouer", hero, "restart");
    b.buttonText(restart).localizationKey = "game.restart";
    b.hide(restart);
}

} // namespace agr::editor::games
