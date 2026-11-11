// Tycoon: build shops on a street, customers walk in, queue, pay or leave unhappy; reputation
// drives the crowd; upgrade shops to serve faster and sell higher; reach 2000 coins.
#include "../GameTemplates.hpp"
#include "agr/twod/TilemapEditing.hpp"

namespace agr::editor::games {

void buildTycoon(Scene& scene) {
    Builder b(scene);
    auto& reg = b.reg;
    b.camera2D(6.0f, {0.3f, 0.5f, 0.3f, 1.0f});
    const auto ground = b.entity("Ground", {-14.0f, -7.0f, 0.0f});
    {
        auto& tm = reg.emplace<TilemapComponent>(ground);
        tm.tileset = "assets://games/topdown_tiles.agrsheet";
        tm.width = 28;
        tm.height = 14;
        tm.sortingLayer = -5;
        tilemapEnsureLayout(tm);
        for (int y = 0; y < 14; ++y)
            for (int x = 0; x < 28; ++x) tilemapSet(tm, x, y, (x * 3 + y * 7) % 11 == 0 ? 1 : 0);
        tilemapFillRect(tm, 0, 2, 27, 4, 14);   // the street (cobble)
        tilemapFillRect(tm, 0, 5, 27, 5, 2);    // pavement
        tilemapFillRect(tm, 0, 12, 27, 13, 6);  // park at the back
    }
    // Plots along the street (above it): shops are built there.
    const auto plots = b.entity("Plots");
    for (int i = 0; i < 6; ++i) {
        const auto p = b.sprite("Plot " + std::to_string(i + 1), "assets://games/plot.png", {static_cast<float>(i) * 4.0f - 10.0f, 0.6f, 0.0f},
                                {2.2f, 2.2f}, -1, plots);
        const auto shop = b.sprite("Shop", "assets://games/shop.png", {0.0f, 0.15f, 0.0f}, {2.6f, 2.6f}, 1, p);
        b.spriteOf(shop).pivot = {0.5f, 0.2f};
        b.hide(shop);
    }
    for (int i = 0; i < 6; ++i) b.sprite("Tree " + std::to_string(i + 1), "assets://games/tree.png", {static_cast<float>(i) * 4.5f - 11.0f, 5.2f, 0.0f}, {1.8f, 1.8f}, 0);
    const auto prototypes = b.entity("Prototypes");
    b.hide(prototypes);
    const auto customer = b.sprite("Customer Prototype", "assets://games/customer.png", {}, {1.1f, 1.1f}, 2, prototypes);
    b.spriteOf(customer).pivot = {0.5f, 0.1f};
    b.script(customer, "assets://games/scripts/tycoon_customer.lua");

    const auto game = b.entity("Game");
    b.script(game, "assets://games/scripts/tycoon_game.lua");
    templates::audioSource(scene, "Music", "assets://audio/music.wav", AudioBus::Music, 0.3f);

    const auto hud = b.hud();
    const auto top = b.panel("Top Panel", hud, {0.0f, 0.0f}, {30.0f, 30.0f}, {620.0f, 170.0f});
    b.label("MoneyText", top, {0.0f, 0.0f}, {24.0f, 10.0f}, {580.0f, 50.0f}, "", 40.0f, UIHAlign::Left, {1.0f, 0.86f, 0.3f, 1.0f});
    b.label("ReputationText", top, {0.0f, 0.0f}, {24.0f, 62.0f}, {580.0f, 46.0f}, "", 34.0f);
    b.label("DayText", top, {0.0f, 0.0f}, {24.0f, 110.0f}, {580.0f, 46.0f}, "", 32.0f);
    b.label("Goal", hud, {1.0f, 0.0f}, {-30.0f, 30.0f}, {700.0f, 60.0f}, "", 34.0f, UIHAlign::Right);
    b.label("Hint", hud, {0.5f, 1.0f}, {0.0f, -30.0f}, {1600.0f, 60.0f}, "", 32.0f, UIHAlign::Center, {1.0f, 1.0f, 0.85f, 1.0f});
    b.label("Message", hud, {0.5f, 0.3f}, {0.0f, 0.0f}, {1400.0f, 120.0f}, "", 60.0f, UIHAlign::Center, {1.0f, 0.95f, 0.6f, 1.0f});
    const auto restart = b.button("Restart", hud, {0.5f, 0.5f}, {0.0f, 0.0f}, {420.0f, 120.0f}, "Rejouer", game, "restart");
    b.buttonText(restart).localizationKey = "game.restart";
    b.hide(restart);
}

} // namespace agr::editor::games
