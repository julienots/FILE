// Farming simulation: sow, water and harvest a field; crops grow while the soil is moist;
// days and nights pass, rain waters everything, market prices change every day.
#include "../GameTemplates.hpp"
#include "agr/twod/TilemapEditing.hpp"

namespace agr::editor::games {

void buildFarm(Scene& scene) {
    Builder b(scene);
    auto& reg = b.reg;
    b.camera2D(5.6f, {0.30f, 0.55f, 0.85f, 1.0f}, {0.0f, -0.4f, 10.0f});
    const auto meadow = b.entity("Meadow", {-12.0f, -7.0f, 0.0f});
    {
        auto& tm = reg.emplace<TilemapComponent>(meadow);
        tm.tileset = "assets://games/topdown_tiles.agrsheet";
        tm.width = 24;
        tm.height = 14;
        tm.sortingLayer = -5;
        tilemapEnsureLayout(tm);
        for (int y = 0; y < 14; ++y)
            for (int x = 0; x < 24; ++x) tilemapSet(tm, x, y, (x * 5 + y * 7) % 9 == 0 ? 1 : (x * 3 + y * 5) % 23 == 0 ? 8 : 0);
        tilemapFillRect(tm, 3, 12, 20, 12, 12);  // fence along the top
    }
    // Field: 8 x 4 plots (sprites from the tileset: frame 9 dry soil, 10 watered).
    const auto field = b.entity("Field");
    for (int r = 0; r < 4; ++r)
        for (int c = 0; c < 8; ++c) {
            const Vec3 pos{static_cast<float>(c) * 1.15f - 4.0f, static_cast<float>(r) * 1.15f - 2.6f, 0.0f};
            const auto plot = b.entity("Plot " + std::to_string(r + 1) + "-" + std::to_string(c + 1), pos, field);
            auto& s = reg.emplace<SpriteRendererComponent>(plot);
            s.sheet = "assets://games/topdown_tiles.agrsheet";
            s.frame = 9;
            s.size = {1.1f, 1.1f};
            s.sortingLayer = -1;
            const auto crop = b.sprite("Crop", "assets://games/crop_0.png", {0.0f, 0.05f, 0.0f}, {1.0f, 1.0f}, 1, plot);
            b.hide(crop);
        }
    const auto barn = b.sprite("Barn", "assets://games/shop.png", {7.6f, 1.2f, 0.0f}, {3.0f, 3.0f}, 0);
    b.spriteOf(barn).color = {1.0f, 0.85f, 0.75f, 1.0f};
    b.sprite("Tree A", "assets://games/tree.png", {-8.0f, 2.5f, 0.0f}, {2.4f, 2.4f}, 0);
    b.sprite("Tree B", "assets://games/tree.png", {-9.0f, -2.0f, 0.0f}, {2.0f, 2.0f}, 0);

    // Rain (2D particles over the field, off until a rainy day) and the night shade.
    const auto rain = b.entity("Rain", {0.0f, 6.5f, 0.0f});
    {
        auto& p = reg.emplace<ParticleEmitter2DComponent>(rain);
        p.emitting = false;
        p.rate = 120.0f;
        p.maxParticles = 600;
        p.lifetimeMin = 1.0f;
        p.lifetimeMax = 1.4f;
        p.speedMin = 9.0f;
        p.speedMax = 11.0f;
        p.direction = math::radians(-100.0f);
        p.spread = math::radians(4.0f);
        p.spawnRadius = 11.0f;
        p.startColor = {0.7f, 0.8f, 1.0f, 0.7f};
        p.endColor = {0.7f, 0.8f, 1.0f, 0.3f};
        p.startSize = 0.08f;
        p.endSize = 0.08f;
        p.sortingLayer = 5;
    }
    const auto night = b.sprite("Night", "", {0.0f, 0.0f, 0.0f}, {60.0f, 40.0f}, 8);
    b.spriteOf(night).color = {0.02f, 0.03f, 0.12f, 0.0f};

    const auto game = b.entity("Game");
    b.script(game, "assets://games/scripts/farm_game.lua");
    templates::audioSource(scene, "Music", "assets://audio/ambient.wav", AudioBus::Music, 0.4f);

    const auto hud = b.hud();
    const auto top = b.panel("Top Panel", hud, {0.0f, 0.0f}, {30.0f, 30.0f}, {640.0f, 170.0f});
    b.label("CoinsText", top, {0.0f, 0.0f}, {24.0f, 10.0f}, {600.0f, 50.0f}, "", 40.0f, UIHAlign::Left, {1.0f, 0.86f, 0.3f, 1.0f});
    b.label("DayText", top, {0.0f, 0.0f}, {24.0f, 62.0f}, {600.0f, 46.0f}, "", 34.0f);
    b.label("MarketText", top, {0.0f, 0.0f}, {24.0f, 110.0f}, {600.0f, 46.0f}, "", 32.0f);
    b.label("Goal", hud, {1.0f, 0.0f}, {-30.0f, 30.0f}, {700.0f, 60.0f}, "", 34.0f, UIHAlign::Right);
    b.label("Message", hud, {0.5f, 0.3f}, {0.0f, 0.0f}, {1400.0f, 120.0f}, "", 60.0f, UIHAlign::Center, {1.0f, 0.95f, 0.6f, 1.0f});
    struct Tool {
        const char* name;
        const char* text;
        const char* key;
    };
    const Tool tools[] = {{"ToolSeed", "Semer", "farm.tool.seed"}, {"ToolWater", "Arroser", "farm.tool.water"}, {"ToolHarvest", "Récolter", "farm.tool.harvest"}};
    for (int i = 0; i < 3; ++i) {
        const auto t = b.button(tools[i].name, hud, {0.5f, 1.0f}, {(static_cast<float>(i) - 1.0f) * 340.0f, -30.0f}, {320.0f, 130.0f}, tools[i].text,
                                game, "selectTool");
        b.buttonText(t).localizationKey = tools[i].key;
        reg.get<UIRectComponent>(t).pivot = {0.5f, 1.0f};
    }
    const auto restart = b.button("Restart", hud, {0.5f, 0.5f}, {0.0f, 0.0f}, {420.0f, 120.0f}, "Rejouer", game, "restart");
    b.buttonText(restart).localizationKey = "game.restart";
    b.hide(restart);
}

} // namespace agr::editor::games
