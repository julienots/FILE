// "Les Ruines d'Ambre" — a complete 2.5D action-adventure: 3D ruins seen from a tilted camera,
// 2D sprite characters leaning toward it. Title menu, three levels (find the three amber gems to
// open the exit), a boss arena, pause, game over, victory, saved progress, touch controls.
// The four areas sit side by side in one scene; games/scripts/ambre_game.lua runs the game.
#include <cmath>

#include "../GameTemplates.hpp"
#include "agr/navigation/NavigationComponents.hpp"
#include "agr/twod/SpriteAnimator.hpp"
#include "agr/animation/AnimationSystem.hpp"

namespace agr::editor::games {

namespace {

constexpr float kAreaSpacing = 70.0f, kHalfW = 17.0f, kHalfD = 11.0f;
constexpr float kLean = -38.0f;  // sprites lean back toward the camera (its pitch)

struct Area {
    const char* name;
    Vec4 floor, wall, light;
};

ecs::Entity block(Builder& b, const std::string& name, Vec3 center, Vec3 size, Vec4 color, const char* texture, ecs::Entity parent) {
    const auto e = b.mesh(name, PrimitiveMesh::Cube, center, size, color, parent);
    auto& mr = b.reg.get<MeshRendererComponent>(e);
    mr.texture = texture;
    mr.uvScale = {std::max(1.0f, std::max(size.x, size.z) / 2.0f), std::max(1.0f, size.y / 2.0f)};
    mr.roughness = 0.85f;
    b.reg.emplace<RigidBodyComponent>(e).type = BodyType3D::Static;
    b.reg.emplace<ColliderComponent>(e);  // unit box scaled by the transform
    return e;
}

/// Sprite on a child entity, feet at the parent's origin, leaning toward the camera.
ecs::Entity billboard(Builder& b, ecs::Entity parent, const std::string& texture, float size, Vec4 color = {1, 1, 1, 1}) {
    const auto s = b.entity("Sprite", {}, parent);
    b.reg.get<TransformComponent>(s).rotation = math::Quat::fromEuler({math::radians(kLean), 0.0f, 0.0f});
    auto& sr = b.reg.emplace<SpriteRendererComponent>(s);
    sr.texture = texture;
    sr.size = {size, size};
    sr.pivot = {0.5f, 0.0f};
    sr.color = color;
    sr.sortingLayer = 1;
    return s;
}

ecs::Entity enemy(Builder& b, const std::string& name, ecs::Entity parent, Vec3 pos, const std::string& kind) {
    const auto e = b.entity(name, pos, parent);
    const bool bat = kind == "bat", boss = kind == "boss";
    billboard(b, e, bat ? "assets://games/bat.png" : "assets://games/slime.png", boss ? 4.0f : bat ? 1.3f : 1.2f,
              boss ? Vec4{1.0f, 0.45f, 0.35f, 1.0f} : Vec4{1, 1, 1, 1});
    if (!bat) {
        auto& agent = b.reg.emplace<NavAgentComponent>(e);
        agent.speed = boss ? 3.2f : 2.4f;
        agent.stoppingDistance = boss ? 1.4f : 0.5f;
        agent.faceMovement = false;
    }
    b.script(e, "assets://games/scripts/ambre_enemy.lua", "{\"kind\": \"" + kind + "\"}");
    return e;
}

} // namespace

void buildAdventure25D(Scene& scene) {
    Builder b(scene);
    auto& reg = b.reg;
    const auto cam = b.camera3D({-13.0f, 9.0f, 10.0f}, {-13.0f, 1.0f, 0.0f}, 45.0f);
    b.script(cam, "assets://games/scripts/ambre_camera.lua");
    const auto sunLight = templates::sun(scene);
    {
        auto& env = reg.get<EnvironmentComponent>(sunLight);
        env.sky = SkyMode::Gradient;
        env.ambientIntensity = 0.7f;
        env.fog = true;
        env.fogStart = 25.0f;
        env.fogEnd = 60.0f;
        env.fogColor = {0.55f, 0.45f, 0.35f, 1.0f};
        reg.get<DirectionalLightComponent>(sunLight).color = {1.0f, 0.88f, 0.7f, 1.0f};
    }

    const Area areas[] = {{"Le Hall des Statues", {0.75f, 0.68f, 0.55f, 1}, {0.85f, 0.78f, 0.65f, 1}, {1.0f, 0.75f, 0.4f, 1}},
                          {"La Crypte Engloutie", {0.45f, 0.62f, 0.68f, 1}, {0.6f, 0.7f, 0.75f, 1}, {0.5f, 0.8f, 1.0f, 1}},
                          {"Les Jardins Suspendus", {0.55f, 0.75f, 0.45f, 1}, {0.7f, 0.75f, 0.6f, 1}, {0.7f, 1.0f, 0.5f, 1}},
                          {"Le Coeur d'Ambre", {0.7f, 0.45f, 0.35f, 1}, {0.75f, 0.5f, 0.4f, 1}, {1.0f, 0.5f, 0.25f, 1}}};
    const auto world = b.entity("World");
    for (int k = 0; k < 4; ++k) {
        const Area& a = areas[k];
        const float cx = static_cast<float>(k) * kAreaSpacing;
        const auto area = b.entity("Area " + std::to_string(k + 1), {cx, 0.0f, 0.0f}, world);
        // Floor, walls (an opening on the right leads to the exit alcove), the alcove and its gate.
        block(b, "Floor", {0.0f, -0.5f, 0.0f}, {kHalfW * 2.0f + 6.0f, 1.0f, kHalfD * 2.0f}, a.floor, "assets://terrain/dirt.png", area);
        block(b, "Wall N", {0.0f, 1.0f, -kHalfD - 0.5f}, {kHalfW * 2.0f + 2.0f, 2.0f, 1.0f}, a.wall, "assets://terrain/rock.png", area);
        block(b, "Wall S", {0.0f, 0.35f, kHalfD + 0.5f}, {kHalfW * 2.0f + 2.0f, 0.7f, 1.0f}, a.wall, "assets://terrain/rock.png", area);  // low: the camera sees over it
        block(b, "Wall W", {-kHalfW - 0.5f, 1.0f, 0.0f}, {1.0f, 2.0f, kHalfD * 2.0f}, a.wall, "assets://terrain/rock.png", area);
        block(b, "Wall E1", {kHalfW + 0.5f, 1.0f, -6.5f}, {1.0f, 2.0f, 9.0f}, a.wall, "assets://terrain/rock.png", area);
        block(b, "Wall E2", {kHalfW + 0.5f, 1.0f, 6.5f}, {1.0f, 2.0f, 9.0f}, a.wall, "assets://terrain/rock.png", area);
        block(b, "Alcove N", {kHalfW + 3.0f, 1.0f, -2.5f}, {4.0f, 2.0f, 1.0f}, a.wall, "assets://terrain/rock.png", area);
        block(b, "Alcove S", {kHalfW + 3.0f, 0.35f, 2.5f}, {4.0f, 0.7f, 1.0f}, a.wall, "assets://terrain/rock.png", area);
        block(b, "Alcove E", {kHalfW + 5.5f, 1.0f, 0.0f}, {1.0f, 2.0f, 6.0f}, a.wall, "assets://terrain/rock.png", area);
        const auto gate = block(b, "Gate", {kHalfW + 0.5f, 1.25f, 0.0f}, {0.6f, 2.5f, 4.0f}, {1.0f, 0.65f, 0.2f, 1.0f}, "", area);
        reg.get<MeshRendererComponent>(gate).metallic = 0.6f;
        const auto portal = b.mesh("Portal", PrimitiveMesh::Sphere, {kHalfW + 3.0f, 1.2f, 0.0f}, Vec3{1.6f}, {1.0f, 0.75f, 0.3f, 1.0f}, area);
        reg.get<MeshRendererComponent>(portal).roughness = 0.1f;
        {
            auto& l = reg.emplace<PointLightComponent>(portal);
            l.color = a.light;
            l.intensity = 8.0f;
            l.range = 7.0f;
        }
        // Pillars and props: obstacles (and cover).
        const Vec2 pillars[] = {{-8.0f, -5.0f}, {-8.0f, 5.0f}, {0.0f, -5.5f}, {0.0f, 5.5f}, {8.0f, -5.0f}, {8.0f, 5.0f}};
        for (int i = 0; i < 6; ++i)
            if ((i + k) % 3 != 2)
                block(b, "Pillar " + std::to_string(i + 1), {pillars[i].x, 1.5f, pillars[i].y}, {1.2f, 3.0f, 1.2f}, a.wall, "assets://terrain/rock.png", area);
        for (int i = 0; i < 3; ++i) {
            const float x = -12.0f + static_cast<float>(i) * 9.0f + static_cast<float>(k), z = (i % 2 ? -8.0f : 8.0f);
            const auto prop = b.entity(i == 1 ? "Rock" : "Crate", {x, 0.0f, z}, area);
            reg.get<TransformComponent>(prop).scale = Vec3{i == 1 ? 1.8f : 1.2f};
            reg.emplace<MeshRendererComponent>(prop).model = i == 1 ? "assets://3d/rock.glb" : "assets://3d/crate.glb";
            reg.emplace<RigidBodyComponent>(prop).type = BodyType3D::Static;
            auto& col = reg.emplace<ColliderComponent>(prop);
            col.size = {0.9f, 0.9f, 0.9f};
            col.offset = {0.0f, 0.45f, 0.0f};
        }
        // Torches: warm lights along the north wall.
        for (int i = 0; i < 3; ++i) {
            const auto torch = b.entity("Torch " + std::to_string(i + 1), {-10.0f + static_cast<float>(i) * 10.0f, 2.3f, -kHalfD + 0.3f}, area);
            auto& l = reg.emplace<PointLightComponent>(torch);
            l.color = a.light;
            l.intensity = 5.0f;
            l.range = 8.0f;
            auto& fire = reg.emplace<ParticleEmitter3DComponent>(torch);
            fire.rate = 18.0f;
            fire.maxParticles = 40;
            fire.lifetimeMin = 0.4f;
            fire.lifetimeMax = 0.8f;
            fire.speedMin = 0.6f;
            fire.speedMax = 1.2f;
            fire.shape = EmitterShape3D::Cone;
            fire.coneAngle = math::radians(15.0f);
            fire.startColor = {1.0f, 0.75f, 0.3f, 1.0f};
            fire.endColor = {1.0f, 0.3f, 0.1f, 0.0f};
            fire.startSize = 0.35f;
            fire.endSize = 0.1f;
            fire.additive = true;
        }
        // Items: gems (levels 1-3), hearts and coins; enemies.
        const auto items = b.entity("Items", {}, area);
        if (k < 3) {
            const Vec2 gemAt[3][3] = {{{-4, -8}, {6, 8}, {13, -7}}, {{-12, -7}, {4, 0}, {12, 8}}, {{-9, 8}, {3, -8}, {14, 3}}};
            for (int i = 0; i < 3; ++i) {
                const auto g = b.entity("Gem " + std::to_string(i + 1), {gemAt[k][i].x, 0.3f, gemAt[k][i].y}, items);
                billboard(b, g, "assets://games/gem.png", 0.8f, {1.0f, 0.7f, 0.25f, 1.0f});
                auto& l = reg.emplace<PointLightComponent>(g);
                l.color = {1.0f, 0.7f, 0.3f, 1.0f};
                l.intensity = 2.5f;
                l.range = 3.0f;
            }
        }
        for (int i = 0; i < 6; ++i) {
            const float x = -12.0f + static_cast<float>(i) * 5.0f, z = std::sin(static_cast<float>(i + k) * 1.7f) * 4.0f;
            const auto c = b.entity("Coin " + std::to_string(i + 1), {x, 0.3f, z}, items);
            billboard(b, c, "assets://games/coin.png", 0.6f);
        }
        const auto heart = b.entity("Heart 1", {-2.0f + static_cast<float>(k) * 3.0f, 0.3f, -2.0f}, items);
        billboard(b, heart, "assets://games/heart.png", 0.7f);
        const auto foes = b.entity("Enemies", {}, area);
        if (k < 3) {
            const int slimes = 3 + k, bats = k == 0 ? 0 : 1 + k;
            for (int i = 0; i < slimes; ++i)
                enemy(b, "Slime " + std::to_string(i + 1), foes, {-4.0f + static_cast<float>(i) * 5.0f, 0.0f, static_cast<float>(i % 2 ? 4 : -3)}, "slime");
            for (int i = 0; i < bats; ++i)
                enemy(b, "Bat " + std::to_string(i + 1), foes, {2.0f + static_cast<float>(i) * 4.0f, 1.6f, static_cast<float>(i % 2 ? -6 : 6)}, "bat");
        } else {
            enemy(b, "Boss", foes, {6.0f, 0.0f, 0.0f}, "boss");
        }
    }
    // Hero: a dynamic capsule (no tipping) carrying the robot sprite and its animations.
    const auto hero = b.entity("Hero", {-13.0f, 0.05f, 0.0f});
    {
        auto& body = reg.emplace<RigidBodyComponent>(hero);
        body.freezeRotation = true;
        auto& col = reg.emplace<ColliderComponent>(hero);
        col.shape = ColliderShape3D::Capsule;
        col.radius = 0.35f;
        col.height = 1.4f;
        col.offset = {0.0f, 0.7f, 0.0f};
        col.friction = 0.0f;
    }
    const auto heroSprite = billboard(b, hero, "", 1.7f);
    {
        auto& sr = reg.get<SpriteRendererComponent>(heroSprite);
        sr.texture.clear();
        sr.sheet = "assets://2d/hero.agrsheet";
        reg.emplace<SpriteAnimatorComponent>(heroSprite).clip = "idle";
        reg.emplace<AnimatorComponent>(heroSprite).controller = "assets://animations/hero.agrctrl";
    }
    b.script(hero, "assets://games/scripts/ambre_hero.lua");
    // Hidden prototype for the minions the boss summons.
    const auto prototypes = b.entity("Prototypes");
    b.hide(prototypes);
    enemy(b, "Minion Prototype", prototypes, {}, "minion");

    const auto nav = b.entity("Navigation");
    auto& ns = reg.emplace<NavigationSettingsComponent>(nav);
    ns.cellSize = 0.3f;
    ns.agentRadius = 0.4f;

    const auto game = b.entity("Game");
    b.script(game, "assets://games/scripts/ambre_game.lua");
    templates::audioSource(scene, "Music", "assets://audio/music.wav", AudioBus::Music, 0.35f);

    // ---- interface: HUD, touch controls, menus.
    const auto hud = b.hud();
    const auto top = b.panel("Top Panel", hud, {0.0f, 0.0f}, {30.0f, 30.0f}, {640.0f, 170.0f});
    b.label("HealthText", top, {0.0f, 0.0f}, {24.0f, 10.0f}, {600.0f, 50.0f}, "", 40.0f, UIHAlign::Left, {1.0f, 0.45f, 0.45f, 1.0f});
    b.label("GemsText", top, {0.0f, 0.0f}, {24.0f, 62.0f}, {600.0f, 46.0f}, "", 36.0f, UIHAlign::Left, {1.0f, 0.75f, 0.3f, 1.0f});
    b.label("CoinsText", top, {0.0f, 0.0f}, {24.0f, 110.0f}, {600.0f, 46.0f}, "", 32.0f);
    b.label("LevelText", hud, {0.5f, 0.0f}, {0.0f, 30.0f}, {1000.0f, 60.0f}, "", 40.0f, UIHAlign::Center);
    b.label("Message", hud, {0.5f, 0.3f}, {0.0f, 0.0f}, {1500.0f, 120.0f}, "", 60.0f, UIHAlign::Center, {1.0f, 0.88f, 0.5f, 1.0f});
    const auto bossBar = b.panel("BossBar", hud, {0.5f, 0.0f}, {0.0f, 100.0f}, {900.0f, 30.0f}, {0.2f, 0.05f, 0.05f, 0.9f});
    b.panel("BossFill", bossBar, {0.0f, 0.0f}, {0.0f, 0.0f}, {900.0f, 30.0f}, {0.9f, 0.35f, 0.2f, 1.0f});
    b.hide(bossBar);
    const auto pause = b.button("PauseButton", hud, {1.0f, 0.0f}, {-30.0f, 30.0f}, {150.0f, 110.0f}, "II", game, "togglePause");
    (void)pause;
    const auto jump = templates::uiButton(scene, "Jump", hud, {1.0f, 1.0f}, {-40.0f, -40.0f}, {220.0f, 200.0f}, "Saut");
    reg.get<UITextComponent>(scene.children(jump).front()).localizationKey = "hud.jump";
    const auto attack = templates::uiButton(scene, "Attack", hud, {1.0f, 1.0f}, {-290.0f, -40.0f}, {220.0f, 200.0f}, "Épée");
    reg.get<UITextComponent>(scene.children(attack).front()).localizationKey = "rpg.attack";
    // Menus (one panel, buttons shown by the game script as needed).
    const auto menu = b.panel("Menu", hud, {0.5f, 0.5f}, {0.0f, 0.0f}, {900.0f, 720.0f}, {0.08f, 0.06f, 0.05f, 0.92f});
    b.label("MenuTitle", menu, {0.5f, 0.0f}, {0.0f, 40.0f}, {860.0f, 100.0f}, "Les Ruines d'Ambre", 76.0f, UIHAlign::Center, {1.0f, 0.75f, 0.3f, 1.0f});
    b.label("MenuText", menu, {0.5f, 0.0f}, {0.0f, 150.0f}, {820.0f, 150.0f}, "", 34.0f, UIHAlign::Center);
    const char* buttons[][3] = {{"PlayButton", "Nouvelle partie", "ambre.new"}, {"ContinueButton", "Continuer", "ambre.continue"},
                                {"ResumeButton", "Reprendre", "ambre.resume"}, {"MenuButton", "Menu principal", "ambre.menu"}};
    const char* messages[] = {"newGame", "continueGame", "togglePause", "toMenu"};
    for (int i = 0; i < 4; ++i) {
        const auto bt = b.button(buttons[i][0], menu, {0.5f, 0.0f}, {0.0f, 330.0f + static_cast<float>(i) * 95.0f}, {560.0f, 85.0f}, buttons[i][1], game,
                                 messages[i]);
        b.buttonText(bt).localizationKey = buttons[i][2];
        reg.get<UIRectComponent>(bt).pivot = {0.5f, 0.0f};
    }
}

} // namespace agr::editor::games
