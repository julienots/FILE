// Idle / incremental game: tap the crystal, buy miners, drills and factories that produce on
// their own, upgrade the pickaxe, earn while away (offline progress), prestige for a bonus.
#include "../GameTemplates.hpp"

namespace agr::editor::games {

void buildIdle(Scene& scene) {
    Builder b(scene);
    auto& reg = b.reg;
    b.camera2D(5.0f, {0.09f, 0.07f, 0.16f, 1.0f});
    const auto cave = b.sprite("Cave", "assets://games/topdown_tiles.png", {0.0f, 0.0f, 0.0f}, {40.0f, 20.0f}, -10);
    b.spriteOf(cave).uvRect = {0.75f, 0.5f, 0.125f, 0.5f};  // cobble tile
    b.spriteOf(cave).color = {0.35f, 0.3f, 0.45f, 1.0f};
    const auto crystal = b.sprite("Crystal", "assets://games/crystal.png", {-3.2f, -0.2f, 0.0f}, {3.4f, 3.4f}, 1);
    (void)crystal;
    const auto sparks = b.entity("Sparks", {-3.2f, -0.2f, 0.0f});
    {
        auto& p = reg.emplace<ParticleEmitter2DComponent>(sparks);
        p.emitting = false;
        p.loop = false;
        p.rate = 0.0f;
        p.burst = 12;
        p.duration = 0.5f;
        p.lifetimeMin = 0.3f;
        p.lifetimeMax = 0.6f;
        p.speedMin = 2.0f;
        p.speedMax = 4.0f;
        p.spread = math::radians(180.0f);
        p.startColor = {0.85f, 0.6f, 1.0f, 1.0f};
        p.endColor = {0.5f, 0.3f, 1.0f, 0.0f};
        p.startSize = 0.18f;
        p.endSize = 0.05f;
        p.additive = true;
        p.sortingLayer = 3;
    }
    const auto game = b.entity("Game");
    b.script(game, "assets://games/scripts/idle_game.lua");
    templates::audioSource(scene, "Music", "assets://audio/music.wav", AudioBus::Music, 0.3f);

    const auto hud = b.hud();
    b.label("CrystalsText", hud, {0.3f, 0.0f}, {0.0f, 40.0f}, {1000.0f, 90.0f}, "0", 80.0f, UIHAlign::Center, {0.9f, 0.8f, 1.0f, 1.0f});
    b.label("RateText", hud, {0.3f, 0.0f}, {0.0f, 135.0f}, {1000.0f, 50.0f}, "", 36.0f, UIHAlign::Center);
    b.label("Message", hud, {0.3f, 1.0f}, {0.0f, -60.0f}, {1100.0f, 110.0f}, "", 38.0f, UIHAlign::Center, {1.0f, 0.95f, 0.6f, 1.0f});
    // Shop column on the right: one button per generator, then upgrade and prestige.
    const char* items[] = {"BuyMiner", "BuyDrill", "BuyFactory", "BuyPickaxe", "Prestige"};
    const char* icons[] = {"assets://games/icon_miner.png", "assets://games/icon_drill.png", "assets://games/icon_factory.png",
                           "assets://games/icon_sword.png", "assets://games/gem.png"};
    for (int i = 0; i < 5; ++i) {
        const auto btn = b.button(items[i], hud, {1.0f, 0.0f}, {-30.0f, 30.0f + static_cast<float>(i) * 190.0f}, {720.0f, 175.0f}, "", game, "buy");
        reg.get<UIRectComponent>(btn).pivot = {1.0f, 0.0f};
        scene.destroyEntity(scene.children(btn).front());
        const auto icon = templates::uiElement(scene, "Icon", btn, {0.0f, 0.5f}, {20.0f, 0.0f}, {130.0f, 130.0f});
        reg.get<UIRectComponent>(icon).pivot = {0.0f, 0.5f};
        auto& img = reg.emplace<UIImageComponent>(icon);
        img.texture = icons[i];
        img.preserveAspect = true;
        b.label("Title", btn, {0.0f, 0.0f}, {170.0f, 14.0f}, {530.0f, 56.0f}, "", 40.0f);
        b.label("Info", btn, {0.0f, 0.0f}, {170.0f, 76.0f}, {530.0f, 84.0f}, "", 28.0f, UIHAlign::Left, {0.85f, 0.85f, 0.9f, 1.0f});
    }
}

} // namespace agr::editor::games
