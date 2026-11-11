// Tower defense: creeps walk a winding road in 10 waves; tap a green spot to build a tower,
// tap a tower to upgrade it; gold from kills; lose when 10 creeps get through.
#include "../GameTemplates.hpp"
#include "agr/twod/TilemapEditing.hpp"

namespace agr::editor::games {

namespace {
enum : i32 { Grass = 0, Tufts = 1, Path = 2, Water = 3, Sand = 5, Flowers = 8, Rock = 11 };
constexpr int kW = 24, kH = 14;
Vec3 cell(int c, int r) { return {static_cast<float>(c) - 11.5f, static_cast<float>(r) - 6.5f, 0.0f}; }
}

void buildTowerDefense(Scene& scene) {
    Builder b(scene);
    auto& reg = b.reg;
    b.camera2D(7.6f, {0.12f, 0.2f, 0.12f, 1.0f});

    // Road through waypoints (tile coordinates); creeps enter on the left and leave on the right.
    const std::pair<int, int> turns[] = {{0, 10}, {5, 10}, {5, 3}, {12, 3}, {12, 11}, {18, 11}, {18, 5}, {23, 5}};
    const auto map = b.entity("Map", {-12.0f, -7.0f, 0.0f});
    {
        auto& tm = reg.emplace<TilemapComponent>(map);
        tm.tileset = "assets://games/topdown_tiles.agrsheet";
        tm.width = kW;
        tm.height = kH;
        tm.sortingLayer = -5;
        tilemapEnsureLayout(tm);
        for (int y = 0; y < kH; ++y)
            for (int x = 0; x < kW; ++x) tilemapSet(tm, x, y, (x * 7 + y * 5) % 13 == 0 ? Tufts : (x * 3 + y * 11) % 19 == 0 ? Flowers : Grass);
        for (usize i = 0; i + 1 < std::size(turns); ++i) {
            const auto [c0, r0] = turns[i];
            const auto [c1, r1] = turns[i + 1];
            tilemapFillRect(tm, std::min(c0, c1), std::min(r0, r1), std::max(c0, c1), std::max(r0, r1), Path);
        }
        tilemapFillRect(tm, 0, 0, 3, 1, Water);
        tilemapFillRect(tm, 20, 12, 23, 13, Sand);
        for (auto [x, y] : {std::pair{9, 12}, {15, 1}, {22, 9}, {2, 4}}) tilemapSet(tm, x, y, Rock);
    }
    const auto path = b.entity("Path");
    b.entity("P0", {-13.5f, 3.5f, 0.0f}, path);  // off screen: creeps walk in
    for (usize i = 0; i < std::size(turns); ++i) b.entity("P" + std::to_string(i + 1), cell(turns[i].first, turns[i].second), path);
    b.entity("P" + std::to_string(std::size(turns) + 1), {13.5f, -1.5f, 0.0f}, path);

    const auto slots = b.entity("Slots");
    const std::pair<int, int> slotAt[] = {{3, 8}, {7, 7}, {7, 1}, {10, 5}, {14, 7}, {10, 9}, {16, 9}, {20, 8}, {16, 3}, {21, 3}};
    int n = 0;
    for (auto [c, r] : slotAt) {
        const auto s = b.sprite("Slot " + std::to_string(++n), "assets://games/plot.png", cell(c, r), {0.95f, 0.95f}, -1, slots);
        b.spriteOf(s).color = {0.8f, 1.0f, 0.8f, 0.85f};
    }
    const auto base = b.sprite("Base", "assets://games/shop.png", {11.0f, -3.4f, 0.0f}, {2.0f, 2.0f}, 0);
    b.spriteOf(base).color = {0.85f, 0.9f, 1.0f, 1.0f};

    // Prototypes (hidden): cloned by the game script.
    const auto prototypes = b.entity("Prototypes");
    b.hide(prototypes);
    const auto creep = b.sprite("Creep Prototype", "assets://games/creep.png", {}, {0.9f, 0.9f}, 1, prototypes);
    b.script(creep, "assets://games/scripts/td_creep.lua");
    const auto tower = b.sprite("Tower Prototype", "assets://games/tower.png", {}, {1.2f, 1.2f}, 2, prototypes);
    b.spriteOf(tower).pivot = {0.5f, 0.3f};
    b.script(tower, "assets://games/scripts/td_tower.lua");
    b.sprite("Bullet Prototype", "assets://games/bullet.png", {}, {0.35f, 0.35f}, 3, prototypes);

    const auto game = b.entity("Game");
    b.script(game, "assets://games/scripts/td_game.lua");
    templates::audioSource(scene, "Music", "assets://audio/music.wav", AudioBus::Music, 0.3f);

    const auto hud = b.hud();
    const auto top = b.panel("Top Panel", hud, {0.0f, 0.0f}, {30.0f, 30.0f}, {520.0f, 120.0f});
    b.label("GoldText", top, {0.0f, 0.0f}, {24.0f, 10.0f}, {480.0f, 50.0f}, "Or : 100", 40.0f, UIHAlign::Left, {1.0f, 0.86f, 0.3f, 1.0f});
    b.label("LivesText", top, {0.0f, 0.0f}, {24.0f, 62.0f}, {480.0f, 50.0f}, "Vies : 10", 40.0f);
    b.label("WaveText", hud, {0.5f, 0.0f}, {0.0f, 30.0f}, {700.0f, 70.0f}, "Vague 0 / 10", 48.0f, UIHAlign::Center);
    b.label("Hint", hud, {0.5f, 1.0f}, {0.0f, -30.0f}, {1500.0f, 60.0f}, "", 32.0f, UIHAlign::Center, {1.0f, 1.0f, 0.85f, 1.0f});
    b.label("Message", hud, {0.5f, 0.35f}, {0.0f, 0.0f}, {1400.0f, 120.0f}, "", 64.0f, UIHAlign::Center, {1.0f, 0.9f, 0.5f, 1.0f});
    const auto next = b.button("NextWave", hud, {1.0f, 1.0f}, {-40.0f, -40.0f}, {420.0f, 150.0f}, "Vague suivante", game, "nextWave");
    b.buttonText(next).localizationKey = "td.next";
    const auto restart = b.button("Restart", hud, {0.5f, 0.55f}, {0.0f, 0.0f}, {420.0f, 120.0f}, "Rejouer", game, "restart");
    b.buttonText(restart).localizationKey = "game.restart";
    b.hide(restart);
}

} // namespace agr::editor::games
