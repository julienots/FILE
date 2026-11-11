// Platformer 2D: run, jump, stomp slimes, collect coins, checkpoint and goal flag, lives.
#include "../GameTemplates.hpp"
#include "agr/twod/SpriteAnimator.hpp"
#include "agr/twod/TilemapEditing.hpp"
#include "agr/animation/AnimationSystem.hpp"

namespace agr::editor::games {

void buildPlatformer(Scene& scene) {
    Builder b(scene);
    auto& reg = b.reg;
    const auto cam = b.camera2D(5.0f, {0.36f, 0.62f, 0.86f, 1.0f}, {-1.1f, 0.0f, 10.0f});
    b.script(cam, "assets://scripts/camera_follow.lua", R"({"target": "Hero", "minX": -1.1, "maxX": 71.1, "smoothing": 6})");

    // Daylight parallax backgrounds (the template's original art).
    struct Layer {
        const char* name;
        const char* texture;
        float factor, y, height, repeats;
        i32 order;
    };
    const Layer layers[] = {{"Sky", "assets://2d/bg_sky.png", 0.0f, 0.0f, 10.5f, 8.0f, 0},
                            {"Mountains", "assets://2d/bg_mountains.png", 0.25f, 0.5f, 10.0f, 8.0f, 1},
                            {"Hills", "assets://2d/bg_hills.png", 0.5f, -0.5f, 10.0f, 8.0f, 2}};
    const auto background = b.entity("Background", {35.0f, 0.0f, 0.0f});
    for (const Layer& l : layers) {
        const auto e = b.sprite(l.name, l.texture, {0.0f, l.y, 0.0f}, {20.0f * l.repeats, l.height}, -10, background);
        reg.emplace<ParallaxComponent>(e).factor = {l.factor, l.factor};
        auto& s = b.spriteOf(e);
        s.uvRect = {0.0f, 0.0f, l.repeats, 1.0f};
        s.orderInLayer = l.order;
        s.color = {1.0f, 1.0f, 1.0f, 1.0f};
    }

    // Level: 90 x 10 tiles of 1 unit from (-10, -5); ground surface at y = -1.
    enum : i32 { GrassTop = 0, Dirt = 1, DarkDirt = 2, Stone = 3, StoneTop = 4, Plank = 5, GrassLeft = 8, GrassRight = 9, Flowers = 10,
                 Rock = 11, Bush = 12, Water = 16, WaterTop = 17, Sign = 19 };
    const auto ground = b.entity("Ground", {-10.0f, -5.0f, 0.0f});
    auto& tm = reg.emplace<TilemapComponent>(ground);
    tm.tileset = "assets://2d/forest_tiles.agrsheet";
    tm.width = 90;
    tm.height = 10;
    tilemapEnsureLayout(tm);
    tilemapFillRect(tm, 0, 0, 89, 0, DarkDirt);
    tilemapFillRect(tm, 0, 1, 89, 2, Dirt);
    tilemapFillRect(tm, 0, 3, 89, 3, GrassTop);
    const int pits[][2] = {{20, 22}, {41, 44}, {63, 66}};
    for (auto [a, z] : pits) {
        tilemapFillRect(tm, a, 0, z, 3, -1);
        tilemapSet(tm, a - 1, 3, GrassRight);
        tilemapSet(tm, z + 1, 3, GrassLeft);
    }
    tilemapFillRect(tm, 19, 6, 23, 6, Plank);   // over the first pit
    tilemapFillRect(tm, 40, 7, 45, 7, Plank);
    tilemapFillRect(tm, 28, 4, 30, 5, Stone);   // a wall to jump over
    tilemapFillRect(tm, 28, 6, 30, 6, StoneTop);
    tilemapFillRect(tm, 62, 6, 67, 6, Plank);
    for (int i = 0; i < 4; ++i) tilemapFillRect(tm, 78 + i, 4, 78 + i, 4 + i, i == 3 ? StoneTop : Stone);  // stairs to the goal
    tilemapFillRect(tm, 82, 4, 89, 7, Stone);
    tilemapFillRect(tm, 82, 8, 89, 8, StoneTop);
    reg.emplace<TilemapCollider2DComponent>(ground).friction = 0.8f;
    const auto decor = b.entity("Decor", {-10.0f, -5.0f, 0.0f});
    auto& dm = reg.emplace<TilemapComponent>(decor);
    dm.tileset = "assets://2d/forest_tiles.agrsheet";  // (tm may have moved: entities were created since)
    dm.width = 90;
    dm.height = 10;
    dm.orderInLayer = 1;
    tilemapEnsureLayout(dm);
    for (auto [a, z] : pits) {
        tilemapFillRect(dm, a, 0, z, 1, Water);
        tilemapFillRect(dm, a, 2, z, 2, WaterTop);
    }
    for (auto [x, tile] : {std::pair{2, Sign}, {6, Bush}, {11, Flowers}, {26, Rock}, {35, Flowers}, {49, Bush}, {57, Flowers}, {71, Rock}})
        tilemapSet(dm, x, 4, tile);

    // Hero: the template's robot (sheet, animator, state machine) with the platformer script.
    const auto hero = b.entity("Hero", {-7.0f, -1.0f, 0.0f});
    {
        auto& s = reg.emplace<SpriteRendererComponent>(hero);
        s.sheet = "assets://2d/hero.agrsheet";
        s.size = {1.5f, 1.5f};
        s.sortingLayer = 1;
    }
    reg.emplace<SpriteAnimatorComponent>(hero).clip = "idle";
    reg.emplace<AnimatorComponent>(hero).controller = "assets://animations/hero.agrctrl";
    b.body2D(hero, BodyType2D::Dynamic).gravityScale = 2.0f;
    {
        auto& c = reg.emplace<Collider2DComponent>(hero);
        c.shape = ColliderShape2D::Capsule;
        c.size = {0.7f, 1.3f};
        c.offset = {0.0f, 0.65f};
        c.friction = 0.0f;
    }
    b.script(hero, "assets://games/scripts/platformer_player.lua", R"({"coinsTotal": 12})");

    // Coins and enemies under parents (the restart re-activates what was taken or defeated).
    const auto coins = b.entity("Coins");
    const math::Vec2 coinAt[] = {{-4, -0.4f}, {-2, -0.4f}, {0, -0.4f}, {11, 2.6f}, {12, 2.6f}, {13, 2.6f},
                                 {19.5f, 0.2f}, {31.5f, 3.0f}, {33, 3.0f}, {54, -0.4f}, {55, -0.4f}, {64.5f, 2.6f}};
    for (int i = 0; i < 12; ++i) {
        const auto c = b.sprite("Coin " + std::to_string(i + 1), "assets://games/coin.png", {coinAt[i].x, coinAt[i].y, 0.0f}, {0.6f, 0.6f}, 1, coins);
        b.circle2D(c, 0.3f, true);
        b.script(c, "assets://games/scripts/pickup.lua");
    }
    const auto enemies = b.entity("Enemies");
    const float slimeX[] = {4.0f, 16.5f, 44.0f, 49.5f, 72.0f};  // on solid ground, away from the checkpoint
    for (int i = 0; i < 5; ++i) {
        const auto s = b.sprite("Slime " + std::to_string(i + 1), "assets://games/slime.png", {slimeX[i], -1.0f, 0.0f}, {1.0f, 1.0f}, 1, enemies);
        b.spriteOf(s).pivot = {0.5f, 0.06f};
        b.body2D(s, BodyType2D::Dynamic).gravityScale = 2.0f;
        b.box2D(s, {0.85f, 0.65f}, {0.0f, 0.33f}).friction = 0.0f;
        b.script(s, "assets://games/scripts/patrol2d.lua", i == 1 || i == 2 ? R"({"range": 1.5, "speed": 2.2})" : R"({"range": 2.5})");
    }
    // Checkpoint and goal flags (triggers).
    const auto checkpoint = b.sprite("Checkpoint", "assets://games/flag.png", {37.5f, -1.0f, 0.0f}, {1.4f, 1.4f}, 0);
    b.spriteOf(checkpoint).pivot = {0.25f, 0.0f};
    b.spriteOf(checkpoint).color = {0.6f, 0.8f, 1.0f, 1.0f};
    b.box2D(checkpoint, {1.0f, 2.0f}, {0.0f, 1.0f}, true);
    b.script(checkpoint, "assets://games/scripts/flag.lua", R"({"kind": "checkpoint"})");
    const auto goal = b.sprite("Goal", "assets://games/flag.png", {76.0f, 4.0f, 0.0f}, {2.0f, 2.0f}, 0);
    b.spriteOf(goal).pivot = {0.25f, 0.0f};
    b.box2D(goal, {1.2f, 3.0f}, {0.0f, 1.5f}, true);
    b.script(goal, "assets://games/scripts/flag.lua", R"({"kind": "goal"})");

    templates::audioSource(scene, "Music", "assets://audio/music.wav", AudioBus::Music, 0.35f);

    // HUD: coins and lives, a center message, restart, touch controls.
    const auto hud = b.hud();
    const auto top = b.panel("Top Panel", hud, {0.0f, 0.0f}, {30.0f, 30.0f}, {560.0f, 120.0f});
    b.label("CoinsText", top, {0.0f, 0.0f}, {24.0f, 12.0f}, {520.0f, 50.0f}, "Pièces : 0 / 12", 40.0f);
    b.label("LivesText", top, {0.0f, 0.0f}, {24.0f, 62.0f}, {520.0f, 50.0f}, "Vies : 3", 40.0f);
    b.label("Message", hud, {0.5f, 0.35f}, {0.0f, 0.0f}, {1400.0f, 120.0f}, "", 72.0f, UIHAlign::Center, {1.0f, 0.92f, 0.5f, 1.0f});
    const auto restart = b.button("Restart", hud, {0.5f, 0.55f}, {0.0f, 0.0f}, {420.0f, 120.0f}, "Rejouer", hero, "restart");
    b.buttonText(restart).localizationKey = "game.restart";
    b.hide(restart);
    templates::uiButton(scene, "Left", hud, {0.0f, 1.0f}, {40.0f, -40.0f}, {190.0f, 170.0f}, "<");
    templates::uiButton(scene, "Right", hud, {0.0f, 1.0f}, {260.0f, -40.0f}, {190.0f, 170.0f}, ">");
    const auto jump = templates::uiButton(scene, "Jump", hud, {1.0f, 1.0f}, {-40.0f, -40.0f}, {260.0f, 170.0f}, "Saut");
    reg.get<UITextComponent>(scene.children(jump).front()).localizationKey = "hud.jump";
}

} // namespace agr::editor::games
