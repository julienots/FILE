// Multiplayer arena: collect coins, first to 10 wins. Local (2 players on one keyboard or
// gamepads) or network (host / join through agr.net, host-authoritative, up to 4 players).
#include "../GameTemplates.hpp"
#include "agr/twod/TilemapEditing.hpp"

namespace agr::editor::games {

void buildArena(Scene& scene) {
    Builder b(scene);
    auto& reg = b.reg;
    b.camera2D(6.2f, {0.1f, 0.1f, 0.14f, 1.0f});
    const auto floor = b.entity("Floor", {-11.0f, -6.0f, 0.0f});
    {
        auto& tm = reg.emplace<TilemapComponent>(floor);
        tm.tileset = "assets://games/topdown_tiles.agrsheet";
        tm.width = 22;
        tm.height = 12;
        tm.sortingLayer = -5;
        tilemapEnsureLayout(tm);
        for (int y = 0; y < 12; ++y)
            for (int x = 0; x < 22; ++x) tilemapSet(tm, x, y, x == 0 || y == 0 || x == 21 || y == 11 ? 4 : ((x + y) % 2 ? 14 : 7));
    }
    const auto players = b.entity("Players");
    const Vec4 colors[] = {{1.0f, 0.4f, 0.4f, 1.0f}, {0.4f, 0.6f, 1.0f, 1.0f}, {0.5f, 0.95f, 0.5f, 1.0f}, {1.0f, 0.85f, 0.35f, 1.0f}};
    const Vec2 spawn[] = {{-7.0f, 0.0f}, {7.0f, 0.0f}, {0.0f, 3.5f}, {0.0f, -3.5f}};
    for (int i = 0; i < 4; ++i) {
        const auto p = b.sprite("Player " + std::to_string(i + 1), "assets://games/blob.png", {spawn[i].x, spawn[i].y, 0.0f}, {1.2f, 1.2f}, 1, players);
        b.spriteOf(p).color = colors[i];
        if (i >= 2) b.hide(p);
    }
    b.sprite("Coin", "assets://games/coin.png", {0.0f, 0.0f, 0.0f}, {0.8f, 0.8f}, 2);
    const auto game = b.entity("Game");
    b.script(game, "assets://games/scripts/arena_game.lua");
    templates::audioSource(scene, "Music", "assets://audio/music.wav", AudioBus::Music, 0.3f);

    const auto hud = b.hud();
    const char* names[] = {"Score1", "Score2", "Score3", "Score4"};
    const Vec2 anchors[] = {{0.0f, 0.0f}, {1.0f, 0.0f}, {0.0f, 1.0f}, {1.0f, 1.0f}};
    for (int i = 0; i < 4; ++i) {
        const float sx = anchors[i].x == 0.0f ? 30.0f : -30.0f, sy = anchors[i].y == 0.0f ? 30.0f : -200.0f;
        b.label(names[i], hud, anchors[i], {sx, sy}, {500.0f, 70.0f}, "", 52.0f, anchors[i].x == 0.0f ? UIHAlign::Left : UIHAlign::Right, colors[i]);
    }
    b.label("Status", hud, {0.5f, 0.0f}, {0.0f, 30.0f}, {1100.0f, 60.0f}, "", 32.0f, UIHAlign::Center);
    b.label("Message", hud, {0.5f, 0.3f}, {0.0f, 0.0f}, {1400.0f, 120.0f}, "", 64.0f, UIHAlign::Center, {1.0f, 0.95f, 0.6f, 1.0f});
    const char* modes[][3] = {{"Local", "2 joueurs (local)", "arena.local"}, {"Host", "Héberger", "arena.host"}, {"Join", "Rejoindre", "arena.join"}};
    for (int i = 0; i < 3; ++i) {
        const auto bt = b.button(modes[i][0], hud, {0.5f, 1.0f}, {(static_cast<float>(i) - 1.0f) * 420.0f, -30.0f}, {400.0f, 120.0f}, modes[i][1], game, "setMode");
        b.buttonText(bt).localizationKey = modes[i][2];
        reg.get<UIRectComponent>(bt).pivot = {0.5f, 1.0f};
    }
}

} // namespace agr::editor::games
