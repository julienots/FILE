// "Robo et les Cristaux" — a complete 2D platformer: five themed levels (meadow, hills, cave,
// lake, castle) stacked in one scene, each with three hidden crystals, coins, a checkpoint and a
// goal flag; slimes, spiky hedgehogs (no stomping), bats, jumping fish, cannons; springs, moving
// and crumbling platforms; hint signs; title menu, level select with crystals, options, pause,
// results, game over, ending; saved progress. games/scripts/plat_*.lua run the game.
#include <map>
#include <tuple>

#include "../GameTemplates.hpp"
#include "agr/animation/AnimationSystem.hpp"
#include "agr/twod/SpriteAnimator.hpp"
#include "agr/twod/TilemapEditing.hpp"

namespace agr::editor::games {

namespace {

enum : i32 { GrassTop = 0, Dirt = 1, DarkDirt = 2, Stone = 3, StoneTop = 4, Plank = 5, GrassLeft = 8, GrassRight = 9, Flowers = 10, Rock = 11,
             Bush = 12, Water = 16, WaterTop = 17, Crate = 18, Sign = 19, Mushroom = 20 };
constexpr int kLevels = 5, kWidth = 110, kHeight = 18;
constexpr float kLevelStep = 40.0f;  // levels are stacked downward

/// Builds one level: tiles in tile coordinates (x right, y up, row 0 at the bottom), entities at
/// tile positions converted to world units (1 tile = 1 unit).
class LevelBuilder {
public:
    LevelBuilder(Builder& b, int k, bool stone) : m_b(b), m_k(k), m_stone(stone) {
        m_ox = -10.0f;
        m_oy = -5.0f - static_cast<float>(k - 1) * kLevelStep;
        m_root = b.entity("Level " + std::to_string(k));
        m_ground = tilemap("Ground", 0);
        b.reg.emplace<TilemapCollider2DComponent>(m_ground).friction = 0.8f;
        m_decor = tilemap("Decor", 1);
        m_items = b.entity("Items", {}, m_root);
        m_enemies = b.entity("Enemies", {}, m_root);
        m_props = b.entity("Props", {}, m_root);
    }

    TilemapComponent& tiles() { return m_b.reg.get<TilemapComponent>(m_ground); }  // re-fetched: entity creation can move components
    TilemapComponent& decor() { return m_b.reg.get<TilemapComponent>(m_decor); }
    Vec3 at(float x, float y) const { return {m_ox + x, m_oy + y, 0.0f}; }

    // ---- tiles
    void floor(int x0, int x1, int top) {
        tilemapFillRect(tiles(), x0, 0, x1, top - 1, m_stone ? Stone : Dirt);
        tilemapFillRect(tiles(), x0, 0, x1, 0, m_stone ? Stone : DarkDirt);
        tilemapFillRect(tiles(), x0, top, x1, top, m_stone ? StoneTop : GrassTop);
        if (!m_stone) {
            if (x0 > 0) tilemapSet(tiles(), x0, top, GrassLeft);
            if (x1 < kWidth - 1) tilemapSet(tiles(), x1, top, GrassRight);
        }
    }
    void block(int x0, int x1, int y0, int y1) {
        tilemapFillRect(tiles(), x0, y0, x1, y1, Stone);
        tilemapFillRect(tiles(), x0, y1, x1, y1, StoneTop);
    }
    void plank(int x0, int x1, int y) { tilemapFillRect(tiles(), x0, y, x1, y, Plank); }
    void ceiling(int x0, int x1, int y0) { tilemapFillRect(tiles(), x0, y0, x1, kHeight - 1, Stone); }
    void water(int x0, int x1) {
        tilemapFillRect(decor(), x0, 0, x1, 1, Water);
        tilemapFillRect(decor(), x0, 2, x1, 2, WaterTop);
    }
    void deco(int x, int y, i32 tile) { tilemapSet(decor(), x, y, tile); }

    // ---- entities (x, y in tiles; y = the surface they stand on)
    void start(float x, float y) { m_b.entity("Start", at(x + 0.5f, y), m_root); }
    void coin(float x, float y) {
        const auto c = m_b.sprite("Coin " + std::to_string(++m_coins), "assets://games/coin.png", at(x + 0.5f, y + 0.5f), {0.6f, 0.6f}, 1, m_items);
        m_b.circle2D(c, 0.3f, true);
        m_b.script(c, "assets://games/scripts/pickup.lua", R"({"kind": "coin", "bob": 0.08})");
    }
    void coins(int x0, int x1, float y, int step = 1) {
        for (int x = x0; x <= x1; x += step) coin(static_cast<float>(x), y);
    }
    void crystal(float x, float y) {
        const auto c = m_b.sprite("Crystal " + std::to_string(++m_crystals), "assets://games/crystal.png", at(x + 0.5f, y + 0.5f), {0.9f, 0.9f}, 1, m_items);
        m_b.circle2D(c, 0.4f, true);
        m_b.script(c, "assets://games/scripts/pickup.lua", R"({"kind": "crystal", "bob": 0.15, "sound": "assets://audio/gem.wav"})");
    }
    void heart(float x, float y) {
        const auto c = m_b.sprite("Heart " + std::to_string(++m_hearts), "assets://games/heart.png", at(x + 0.5f, y + 0.5f), {0.7f, 0.7f}, 1, m_items);
        m_b.circle2D(c, 0.35f, true);
        m_b.script(c, "assets://games/scripts/pickup.lua", R"({"kind": "heart", "bob": 0.1, "sound": "assets://audio/gem.wav"})");
    }
    ecs::Entity enemy(const std::string& kind, float x, float y, const std::string& extra = {}) {
        const bool spiky = kind == "spiky", bat = kind == "bat", fish = kind == "fish", cannon = kind == "cannon";
        const char* tex = spiky ? "assets://games/spiky.png" : bat ? "assets://games/bat.png" : fish ? "assets://games/fish.png"
                        : cannon ? "assets://games/cannon.png" : "assets://games/slime.png";
        std::string label = kind;
        label[0] = static_cast<char>(label[0] - 'a' + 'A');
        const auto e = m_b.sprite(label + " " + std::to_string(++m_count[kind]), tex, at(x + 0.5f, y), {1.0f, 1.0f}, 1, m_enemies);
        auto& s = m_b.spriteOf(e);
        s.pivot = {0.5f, bat || fish ? 0.5f : 0.06f};
        if (spiky) s.size = {1.2f, 1.2f};
        if (cannon) s.size = {1.3f, 1.3f};
        const BodyType2D type = bat || fish ? BodyType2D::Kinematic : cannon ? BodyType2D::Static : BodyType2D::Dynamic;
        m_b.body2D(e, type).gravityScale = 2.0f;
        if (bat || fish) m_b.box2D(e, {0.8f, 0.6f}, {0.0f, 0.0f});
        else if (cannon) m_b.box2D(e, {1.1f, 0.9f}, {0.0f, 0.45f});
        else m_b.box2D(e, {0.85f, 0.65f}, {0.0f, 0.33f}).friction = 0.0f;
        m_b.script(e, "assets://games/scripts/plat_enemy.lua", "{\"kind\": \"" + kind + "\", \"level\": " + std::to_string(m_k) + extra + "}");
        return e;
    }
    void spring(float x, float y) {
        const auto e = m_b.sprite("Spring " + std::to_string(++m_springs), "assets://games/spring.png", at(x + 0.5f, y), {0.9f, 0.9f}, 1, m_props);
        m_b.spriteOf(e).pivot = {0.5f, 0.0f};
        m_b.box2D(e, {0.8f, 0.5f}, {0.0f, 0.25f}, true);
        m_b.script(e, "assets://games/scripts/plat_platform.lua", R"({"kind": "spring"})");
    }
    void mover(float x, float y, float w, float dx, float dy, float speed = 2.0f) {
        const auto e = m_b.sprite("Mover " + std::to_string(++m_movers), "assets://games/platform.png", at(x + w * 0.5f, y), {w, w / 4.0f}, 1, m_props);
        m_b.spriteOf(e).pivot = {0.5f, 0.8f};
        m_b.body2D(e, BodyType2D::Kinematic);
        m_b.box2D(e, {w, 0.4f}, {0.0f, -0.2f}).friction = 1.0f;
        m_b.script(e, "assets://games/scripts/plat_platform.lua",
                   "{\"kind\": \"mover\", \"dx\": " + std::to_string(dx) + ", \"dy\": " + std::to_string(dy) + ", \"speed\": " + std::to_string(speed) + "}");
    }
    void crumble(int x, int y) {  // a block whose top is the surface y
        const auto e = m_b.sprite("Crumble " + std::to_string(++m_crumbles), "assets://games/crumble.png", at(static_cast<float>(x) + 0.5f, static_cast<float>(y) - 0.5f),
                                  {1.0f, 1.0f}, 1, m_props);
        m_b.body2D(e, BodyType2D::Static);
        m_b.box2D(e, {1.0f, 1.0f}, {0.0f, 0.0f});
        m_b.script(e, "assets://games/scripts/plat_platform.lua", R"({"kind": "crumble"})");
    }
    void sign(float x, float y, const char* key) {
        deco(static_cast<int>(x), static_cast<int>(y), Sign);
        const auto e = m_b.entity("Sign " + std::to_string(++m_signs), at(x + 0.5f, y), m_props);
        m_b.box2D(e, {3.0f, 3.0f}, {0.0f, 1.5f}, true);
        m_b.script(e, "assets://games/scripts/plat_platform.lua", std::string(R"({"kind": "sign", "text": ")") + key + "\"}");
    }
    void checkpoint(float x, float y) {
        const auto e = m_b.sprite("Checkpoint", "assets://games/flag.png", at(x + 0.5f, y), {1.4f, 1.4f}, 0, m_props);
        m_b.spriteOf(e).pivot = {0.25f, 0.0f};
        m_b.spriteOf(e).color = {0.6f, 0.8f, 1.0f, 1.0f};
        m_b.box2D(e, {1.0f, 2.0f}, {0.0f, 1.0f}, true);
        m_b.script(e, "assets://games/scripts/flag.lua", R"({"kind": "checkpoint"})");
    }
    void goal(float x, float y) {
        const auto e = m_b.sprite("Goal", "assets://games/flag.png", at(x + 0.5f, y), {2.0f, 2.0f}, 0, m_props);
        m_b.spriteOf(e).pivot = {0.25f, 0.0f};
        m_b.box2D(e, {1.2f, 3.0f}, {0.0f, 1.5f}, true);
        m_b.script(e, "assets://games/scripts/flag.lua", R"({"kind": "goal"})");
    }

private:
    ecs::Entity tilemap(const char* name, i32 order) {
        const auto e = m_b.entity(name, {m_ox, m_oy, 0.0f}, m_root);
        auto& tm = m_b.reg.emplace<TilemapComponent>(e);
        tm.tileset = "assets://2d/forest_tiles.agrsheet";
        tm.width = kWidth;
        tm.height = kHeight;
        tm.orderInLayer = order;
        tilemapEnsureLayout(tm);
        return e;
    }

    Builder& m_b;
    int m_k;
    bool m_stone;
    float m_ox = 0.0f, m_oy = 0.0f;
    ecs::Entity m_root, m_ground, m_decor, m_items, m_enemies, m_props;
    int m_coins = 0, m_crystals = 0, m_hearts = 0, m_springs = 0, m_movers = 0, m_crumbles = 0, m_signs = 0;
    std::map<std::string, int> m_count;
};

void meadow(Builder& b) {  // 1: tutorial
    LevelBuilder l(b, 1, false);
    l.floor(0, 23, 3);
    l.floor(27, 51, 3);
    l.floor(56, 109, 3);
    l.water(24, 26);
    l.water(52, 55);
    l.plank(22, 28, 7);
    l.block(36, 38, 4, 6);
    l.block(92, 93, 4, 4);
    l.block(94, 95, 4, 5);
    l.block(96, 97, 4, 6);
    l.block(98, 109, 4, 7);
    l.plank(70, 74, 10);
    for (auto [x, t] : {std::pair{9, Flowers}, {14, Bush}, {33, Rock}, {46, Flowers}, {62, Bush}, {81, Mushroom}, {88, Flowers}}) l.deco(x, 4, t);
    l.start(3, 4);
    l.sign(5, 4, "plat.sign.move");
    l.sign(30, 4, "plat.sign.stomp");
    l.sign(77, 4, "plat.sign.spiky");
    l.coins(8, 12, 4.5f, 2);
    l.coins(23, 27, 8.2f, 2);
    l.coin(37, 7.5f);
    l.coins(44, 48, 4.5f, 2);
    l.coins(62, 66, 4.5f, 2);
    l.coins(85, 89, 4.5f, 2);
    l.coins(99, 101, 8.5f);
    l.crystal(25, 10.5f);
    l.spring(49, 4);
    l.crystal(54, 10);
    l.spring(68, 4);
    l.crystal(72, 11.5f);
    l.enemy("slime", 16, 4);
    l.enemy("slime", 44, 4);
    l.enemy("slime", 64, 4);
    l.enemy("spiky", 82, 4);
    l.checkpoint(59, 4);
    l.goal(104, 8);
}

void hills(Builder& b) {  // 2: heights, springs, a moving platform, bats
    LevelBuilder l(b, 2, false);
    l.floor(0, 18, 3);
    l.floor(19, 30, 5);
    l.floor(35, 48, 4);
    l.floor(61, 74, 6);
    l.floor(75, 82, 3);
    l.floor(87, 109, 4);
    l.water(31, 34);
    l.water(49, 60);
    l.water(83, 86);
    l.plank(14, 18, 10);
    l.plank(77, 81, 8);
    l.block(100, 109, 5, 7);
    for (auto [x, y, t] : {std::tuple{6, 4, Bush}, {24, 6, Flowers}, {42, 5, Rock}, {68, 7, Flowers}, {93, 5, Bush}}) l.deco(x, y, t);
    l.start(3, 4);
    l.sign(6, 4, "plat.sign.spring");
    l.spring(11, 4);
    l.crystal(16, 11.5f);
    l.coins(20, 28, 6.5f, 2);
    l.coins(36, 46, 5.5f, 2);
    l.mover(50, 6, 3, 7, 0);
    l.coins(51, 58, 8, 2);
    l.crystal(55, 10.5f);
    l.heart(45, 8);
    l.checkpoint(63, 7);
    l.coins(66, 72, 7.5f, 2);
    l.crystal(79, 9.5f);
    l.coins(88, 98, 5.5f, 2);
    l.coins(102, 104, 8.5f);
    l.enemy("slime", 24, 6);
    l.enemy("spiky", 41, 5);
    l.enemy("bat", 66, 10);
    l.enemy("slime", 71, 7);
    l.enemy("spiky", 92, 5);
    l.enemy("bat", 96, 8);
    l.goal(105, 8);
}

void cave(Builder& b) {  // 3: ceiling, crumbling blocks, bats
    LevelBuilder l(b, 3, true);
    l.ceiling(0, 109, 14);
    l.floor(0, 20, 3);
    l.floor(33, 55, 3);
    l.floor(63, 109, 3);
    l.block(40, 42, 4, 8);
    l.plank(37, 38, 6);
    l.block(70, 80, 10, 13);
    l.plank(84, 86, 7);
    l.block(98, 109, 4, 6);
    l.start(3, 4);
    l.sign(5, 4, "plat.sign.crumble");
    for (int x : {23, 26, 29}) l.crumble(x, 4);
    l.coins(23, 29, 5, 3);
    for (int x = 57; x <= 61; ++x) l.crumble(x, 6);
    l.coins(8, 18, 4.5f, 2);
    l.crystal(41, 10);
    l.crystal(59, 8.5f);
    l.crystal(85, 8.5f);
    l.coins(44, 52, 4.5f, 2);
    l.coins(66, 82, 4.5f, 2);
    l.coins(100, 102, 7.5f);
    l.heart(50, 4.2f);
    l.checkpoint(45, 4);
    l.enemy("bat", 12, 8);
    l.enemy("slime", 36, 4);
    l.enemy("bat", 48, 8);
    l.enemy("spiky", 67, 4);
    l.enemy("bat", 76, 7);
    l.enemy("slime", 90, 4);
    l.goal(104, 7);
}

void lake(Builder& b) {  // 4: water, moving platforms, jumping fish
    LevelBuilder l(b, 4, false);
    l.floor(0, 14, 3);
    l.floor(31, 36, 3);
    l.floor(51, 64, 5);
    l.floor(81, 109, 3);
    l.water(15, 30);
    l.water(37, 50);
    l.water(65, 80);
    l.block(100, 109, 4, 5);
    l.start(3, 4);
    l.sign(6, 4, "plat.sign.fish");
    l.mover(17, 5, 3, 9, 0);
    l.crystal(25, 9);
    l.mover(39, 4, 3, 0, 6, 1.6f);
    l.crystal(40, 12.5f);
    l.mover(43, 9, 3, 5, 0, 1.6f);
    l.mover(67, 7, 3, 10, 0, 2.4f);
    l.crystal(75, 11);
    l.coins(17, 27, 7, 2);
    l.coins(52, 62, 6.5f, 2);
    l.coins(68, 78, 8.5f, 2);
    l.coins(84, 96, 4.5f, 2);
    l.coins(101, 103, 6.5f);
    l.heart(34, 4.2f);
    l.checkpoint(55, 6);
    l.enemy("fish", 22, 1, R"(, "jump": 11.5)");
    l.enemy("fish", 47, 1, R"(, "jump": 12)");
    l.enemy("fish", 73, 1, R"(, "jump": 12.5)");
    l.enemy("slime", 8, 4);
    l.enemy("slime", 58, 6);
    l.enemy("spiky", 90, 4);
    l.goal(105, 6);
}

void castle(Builder& b) {  // 5: cannons, crumbles, movers, everything
    LevelBuilder l(b, 5, true);
    l.floor(0, 25, 3);
    l.floor(36, 60, 3);
    l.floor(73, 109, 3);
    l.block(12, 14, 4, 6);
    l.plank(78, 82, 7);
    l.block(97, 99, 4, 6);
    l.block(100, 109, 4, 9);
    l.ceiling(36, 60, 15);
    l.start(3, 4);
    l.sign(5, 4, "plat.sign.cannon");
    l.enemy("cannon", 22, 4, R"(, "dir": -1)");
    l.crystal(13, 8.5f);
    for (int x : {27, 30, 33}) l.crumble(x, 4);
    l.crystal(30, 8);
    l.coins(16, 20, 4.5f, 2);
    l.checkpoint(39, 4);
    l.enemy("bat", 45, 9);
    l.enemy("spiky", 48, 4);
    l.enemy("bat", 53, 8);
    l.enemy("cannon", 58, 4, R"(, "dir": -1)");
    l.coins(42, 56, 4.5f, 2);
    l.mover(62, 5, 3, 8, 0, 2.2f);
    l.coins(63, 70, 7.5f, 2);
    l.heart(75, 4.2f);
    l.crystal(80, 9);
    l.enemy("slime", 86, 4);
    l.enemy("cannon", 95, 4, R"(, "dir": -1)");
    l.coins(84, 92, 4.5f, 2);
    l.coins(102, 106, 10.5f, 2);
    l.goal(105, 10);
}

} // namespace

void buildPlatformer(Scene& scene) {
    Builder b(scene);
    auto& reg = b.reg;
    const auto cam = b.camera2D(5.5f, {0.36f, 0.62f, 0.86f, 1.0f}, {-1.1f, 0.0f, 10.0f});
    b.script(cam, "assets://games/scripts/plat_camera.lua");

    // Parallax backgrounds (follow the camera vertically: the levels are stacked).
    struct Layer {
        const char* name;
        const char* texture;
        float factor, y, height, repeats;
        i32 order;
    };
    const Layer layers[] = {{"Sky", "assets://2d/bg_sky.png", 0.0f, 0.0f, 11.5f, 8.0f, 0},
                            {"Mountains", "assets://2d/bg_mountains.png", 0.25f, 0.5f, 10.5f, 8.0f, 1},
                            {"Hills", "assets://2d/bg_hills.png", 0.5f, -0.5f, 10.5f, 8.0f, 2}};
    const auto background = b.entity("Background", {45.0f, 0.0f, 0.0f});
    for (const Layer& l : layers) {
        const auto e = b.sprite(l.name, l.texture, {0.0f, l.y, 0.0f}, {22.0f * l.repeats, l.height}, -10, background);
        reg.emplace<ParallaxComponent>(e).factor = {l.factor, 0.0f};
        auto& s = b.spriteOf(e);
        s.uvRect = {0.0f, 0.0f, l.repeats, 1.0f};
        s.orderInLayer = l.order;
    }

    meadow(b);
    hills(b);
    cave(b);
    lake(b);
    castle(b);

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
    b.script(hero, "assets://games/scripts/platformer_player.lua");
    // Hidden prototype: the cannons' bullets.
    const auto prototypes = b.entity("Prototypes");
    b.hide(prototypes);
    {
        const auto bullet = b.sprite("Bullet Prototype", "assets://games/bullet.png", {}, {0.5f, 0.5f}, 2, prototypes);
        b.spriteOf(bullet).color = {0.25f, 0.22f, 0.25f, 1.0f};
        b.script(bullet, "assets://games/scripts/plat_bullet.lua");
    }

    const auto game = b.entity("Game");
    b.script(game, "assets://games/scripts/plat_game.lua");
    templates::audioSource(scene, "Music", "assets://audio/music.wav", AudioBus::Music, 0.35f);

    // ---- interface: HUD, touch controls, menus.
    const auto hud = b.hud();
    const auto top = b.panel("Top Panel", hud, {0.0f, 0.0f}, {30.0f, 30.0f}, {560.0f, 170.0f}, {0.05f, 0.08f, 0.12f, 0.55f});
    for (int i = 0; i < 3; ++i) {
        const auto h = templates::uiElement(scene, "HeartIcon " + std::to_string(i + 1), top, {0.0f, 0.0f}, {20.0f + static_cast<float>(i) * 66.0f, 12.0f},
                                            {58.0f, 58.0f});
        reg.emplace<UIImageComponent>(h).texture = "assets://games/heart.png";
    }
    for (int i = 0; i < 3; ++i) {
        const auto c = templates::uiElement(scene, "CrystalIcon " + std::to_string(i + 1), top, {0.0f, 0.0f}, {20.0f + static_cast<float>(i) * 66.0f, 90.0f},
                                            {58.0f, 58.0f});
        auto& img = reg.emplace<UIImageComponent>(c);
        img.texture = "assets://games/crystal.png";
        img.color = {0.2f, 0.2f, 0.3f, 0.8f};
    }
    const auto coinIcon = templates::uiElement(scene, "CoinIcon", top, {0.0f, 0.0f}, {250.0f, 16.0f}, {50.0f, 50.0f});
    reg.emplace<UIImageComponent>(coinIcon).texture = "assets://games/coin.png";
    b.label("CoinsText", top, {0.0f, 0.0f}, {310.0f, 12.0f}, {240.0f, 56.0f}, "", 40.0f);
    b.label("LivesText", top, {0.0f, 0.0f}, {250.0f, 96.0f}, {300.0f, 46.0f}, "", 30.0f, UIHAlign::Left, {1.0f, 0.7f, 0.7f, 0.9f});
    b.label("LevelText", hud, {0.5f, 0.0f}, {0.0f, 30.0f}, {1000.0f, 60.0f}, "", 40.0f, UIHAlign::Center);
    b.label("TimeText", hud, {1.0f, 0.0f}, {-200.0f, 40.0f}, {220.0f, 50.0f}, "", 36.0f, UIHAlign::Right);
    b.label("Message", hud, {0.5f, 0.3f}, {0.0f, 0.0f}, {1400.0f, 120.0f}, "", 64.0f, UIHAlign::Center, {1.0f, 0.92f, 0.5f, 1.0f});
    {
        const auto hint = b.panel("HintPanel", hud, {0.5f, 1.0f}, {0.0f, -230.0f}, {1100.0f, 100.0f}, {0.05f, 0.08f, 0.12f, 0.75f});
        reg.get<UIRectComponent>(hint).pivot = {0.5f, 1.0f};
        reg.get<UIImageComponent>(hint).blocksPointer = false;
        b.label("Hint", hint, {0.5f, 0.5f}, {0.0f, 0.0f}, {1060.0f, 90.0f}, "", 34.0f, UIHAlign::Center);
        b.hide(hint);
    }
    b.button("PauseButton", hud, {1.0f, 0.0f}, {-30.0f, 30.0f}, {150.0f, 110.0f}, "II", game, "togglePause");
    templates::uiButton(scene, "Left", hud, {0.0f, 1.0f}, {40.0f, -40.0f}, {190.0f, 170.0f}, "<");
    templates::uiButton(scene, "Right", hud, {0.0f, 1.0f}, {260.0f, -40.0f}, {190.0f, 170.0f}, ">");
    const auto jump = templates::uiButton(scene, "Jump", hud, {1.0f, 1.0f}, {-40.0f, -40.0f}, {260.0f, 170.0f}, "Saut");
    reg.get<UITextComponent>(scene.children(jump).front()).localizationKey = "hud.jump";

    const auto menu = b.panel("Menu", hud, {0.5f, 0.5f}, {0.0f, 0.0f}, {1000.0f, 880.0f}, {0.05f, 0.08f, 0.12f, 0.94f});
    b.label("MenuTitle", menu, {0.5f, 0.0f}, {0.0f, 40.0f}, {960.0f, 100.0f}, "", 72.0f, UIHAlign::Center, {1.0f, 0.85f, 0.4f, 1.0f});
    b.label("MenuText", menu, {0.5f, 0.0f}, {0.0f, 150.0f}, {900.0f, 230.0f}, "", 32.0f, UIHAlign::Center);
    const char* buttons[][2] = {{"PlayButton", ""},
                                {"LevelsButton", "plat.levels"},
                                {"OptionsButton", "plat.options"},
                                {"ResumeButton", "plat.resume"},
                                {"RestartButton", "plat.restart"},
                                {"RetryButton", "plat.retry"},
                                {"NextButton", "plat.next"},
                                {"MenuButton", "plat.menu"},
                                {"BackButton", "plat.back"},
                                {"MusicButton", ""},
                                {"SfxButton", ""},
                                {"LanguageButton", ""}};
    for (const auto& bt : buttons) {
        const auto e = b.button(bt[0], menu, {0.5f, 0.0f}, {0.0f, 400.0f}, {620.0f, 88.0f}, "", game, "pressed");
        b.buttonText(e).localizationKey = bt[1];
        reg.get<UIRectComponent>(e).pivot = {0.5f, 0.0f};
    }
    for (int i = 0; i < kLevels; ++i) {
        const auto e = b.button("LevelButton " + std::to_string(i + 1), menu, {0.5f, 0.0f}, {0.0f, 300.0f}, {450.0f, 110.0f}, "", game, "pressed");
        reg.get<UIRectComponent>(e).pivot = {0.5f, 0.0f};
        b.buttonText(e).fontSize = 30.0f;
    }
}

} // namespace agr::editor::games
