// Card duel: each turn draw up to 5 cards and spend 3 energy on attacks, shields, potions and
// lightning; the opponent (simple AI) answers; first to 0 HP loses. Entirely UI-driven.
#include "../GameTemplates.hpp"

namespace agr::editor::games {

void buildCards(Scene& scene) {
    Builder b(scene);
    auto& reg = b.reg;
    b.camera2D(5.0f, {0.10f, 0.08f, 0.14f, 1.0f});
    const auto table = b.sprite("Table", "assets://games/topdown_tiles.png", {0.0f, 0.0f, 0.0f}, {40.0f, 20.0f}, -10);
    b.spriteOf(table).uvRect = {0.875f, 0.0f, 0.125f, 0.5f};  // wood floor tile, stretched and tinted
    b.spriteOf(table).color = {0.45f, 0.32f, 0.28f, 1.0f};
    const auto game = b.entity("Game");
    b.script(game, "assets://games/scripts/cards_game.lua");
    templates::audioSource(scene, "Music", "assets://audio/ambient.wav", AudioBus::Music, 0.35f);

    const auto hud = b.hud();
    // Opponent (top) and player (bottom-left) stats.
    const auto foe = b.panel("Foe Panel", hud, {0.5f, 0.0f}, {0.0f, 30.0f}, {900.0f, 200.0f}, {0.2f, 0.06f, 0.08f, 0.8f});
    b.label("FoeName", foe, {0.0f, 0.0f}, {30.0f, 14.0f}, {840.0f, 56.0f}, "Golem de pierre", 44.0f, UIHAlign::Center);
    b.label("FoeStats", foe, {0.0f, 0.0f}, {30.0f, 74.0f}, {840.0f, 50.0f}, "", 40.0f, UIHAlign::Center);
    b.label("FoeIntent", foe, {0.0f, 0.0f}, {30.0f, 130.0f}, {840.0f, 50.0f}, "", 32.0f, UIHAlign::Center, {1.0f, 0.8f, 0.6f, 1.0f});
    const auto me = b.panel("Player Panel", hud, {0.0f, 1.0f}, {30.0f, -470.0f}, {520.0f, 170.0f}, {0.06f, 0.12f, 0.22f, 0.85f});
    b.label("PlayerStats", me, {0.0f, 0.0f}, {24.0f, 14.0f}, {480.0f, 50.0f}, "", 40.0f);
    b.label("EnergyText", me, {0.0f, 0.0f}, {24.0f, 90.0f}, {480.0f, 50.0f}, "", 40.0f, UIHAlign::Left, {0.6f, 0.9f, 1.0f, 1.0f});
    b.label("Log", hud, {0.5f, 0.0f}, {0.0f, 260.0f}, {1500.0f, 140.0f}, "", 34.0f, UIHAlign::Center);
    b.label("Message", hud, {0.5f, 0.42f}, {0.0f, 0.0f}, {1400.0f, 120.0f}, "", 68.0f, UIHAlign::Center, {1.0f, 0.9f, 0.5f, 1.0f});

    // Hand: five card buttons (image, cost, name, text).
    for (int i = 0; i < 5; ++i) {
        const auto card = b.button("Card " + std::to_string(i + 1), hud, {0.5f, 1.0f}, {(static_cast<float>(i) - 2.0f) * 250.0f, -20.0f},
                                   {230.0f, 324.0f}, "", game, "playCard");
        auto& img = reg.get<UIImageComponent>(card);
        img.texture = "assets://games/card.png";
        img.color = {1.0f, 1.0f, 1.0f, 1.0f};
        reg.get<UIRectComponent>(card).pivot = {0.5f, 1.0f};
        scene.destroyEntity(scene.children(card).front());  // the default centered label
        const auto icon = templates::uiElement(scene, "Icon", card, {0.5f, 0.0f}, {0.0f, 22.0f}, {150.0f, 150.0f});
        reg.get<UIRectComponent>(icon).pivot = {0.5f, 0.0f};
        reg.emplace<UIImageComponent>(icon).preserveAspect = true;
        b.label("Cost", card, {0.0f, 0.0f}, {10.0f, 8.0f}, {60.0f, 56.0f}, "1", 44.0f, UIHAlign::Center, {0.2f, 0.45f, 0.9f, 1.0f});
        b.label("Name", card, {0.0f, 0.0f}, {14.0f, 176.0f}, {202.0f, 44.0f}, "", 30.0f, UIHAlign::Center, {0.15f, 0.12f, 0.12f, 1.0f});
        b.label("Text", card, {0.0f, 0.0f}, {16.0f, 222.0f}, {198.0f, 90.0f}, "", 22.0f, UIHAlign::Center, {0.25f, 0.22f, 0.2f, 1.0f});
    }
    const auto end = b.button("EndTurn", hud, {1.0f, 1.0f}, {-40.0f, -360.0f}, {360.0f, 130.0f}, "Fin du tour", game, "endTurn");
    b.buttonText(end).localizationKey = "cards.endturn";
    const auto restart = b.button("Restart", hud, {0.5f, 0.55f}, {0.0f, 0.0f}, {420.0f, 120.0f}, "Rejouer", game, "restart");
    b.buttonText(restart).localizationKey = "game.restart";
    b.hide(restart);
}

} // namespace agr::editor::games
