// "Les Ruines d'Ambre" — a complete 2.5D action-adventure: 3D ruins seen from a tilted camera,
// 2D sprite characters leaning toward it. Six halls side by side in one scene, each with its own
// mechanic (tutorial, spike traps, chasm to jump, lights-out puzzle, gauntlet, boss), skeleton
// archers, stone brutes, slimes and bats, breakable pots, chests, hint signs; a shop between halls,
// three stars per hall, level select, options (volumes, language), story, saved progress.
// The level layout is data (levels() below); games/scripts/ambre_*.lua run the game.
#include <cctype>
#include <cmath>
#include <map>
#include <vector>

#include "../GameTemplates.hpp"
#include "agr/animation/AnimationSystem.hpp"
#include "agr/navigation/NavigationComponents.hpp"
#include "agr/twod/SpriteAnimator.hpp"

namespace agr::editor::games {

namespace {

constexpr float kAreaSpacing = 70.0f, kHalfW = 17.0f, kHalfD = 11.0f;
constexpr float kLean = -38.0f;  // sprites lean back toward the camera (its pitch)
constexpr int kAreaCount = 6;

struct Spot {
    float x, z;
};
struct Foe {
    const char* kind;
    float x, z;
};
struct Hint {
    const char* key;
    float x, z;
};
struct Rect {  // floor piece: center and size on the ground
    float x, z, w, d;
};

struct Level {
    const char* name;
    Vec4 floor, wall, light;
    const char* floorTexture;
    std::vector<Spot> gems;  // up to 3 (the third of the puzzle hall is hidden until solved)
    std::vector<Spot> coins;
    std::vector<Spot> hearts;
    std::vector<Spot> pots;
    std::vector<Spot> chests;
    std::vector<Hint> signs;
    std::vector<Spot> spikes;  // 2 x 2 m spike traps
    std::vector<Spot> plates;  // lights-out puzzle (2 x 2)
    std::vector<Spot> pillars;
    std::vector<Foe> foes;
    std::vector<Rect> floorPieces;  // empty = one floor; else pieces around chasms
    std::vector<Spot> stones;       // stepping stones in a chasm
    bool boss = false;
};

const std::vector<Level>& levels() {
    static const std::vector<Level> l = {
        {"Le Hall des Statues", {0.78f, 0.7f, 0.56f, 1}, {0.88f, 0.8f, 0.66f, 1}, {1.0f, 0.75f, 0.4f, 1}, "assets://terrain/dirt.png",
         {{-4, -8}, {6, 8}, {13, -7}},
         {{-10, 0}, {-7, 0}, {-1, 6}, {2, 6}, {9, 2}, {11, -3}},
         {{-2, -2}},
         {{-15, 9}, {-14, 9.5f}, {11, 9}, {15, -9}},
         {{0, 9}},
         {{"ambre.sign.move", -11, -3}, {"ambre.sign.attack", -6, 4}, {"ambre.sign.gems", 4, -3}},
         {},
         {},
         {{-8, -5}, {-8, 5}, {0, -5.5f}, {8, -5}, {8, 5}},
         {{"slime", -1, -3}, {"slime", 4, 4}, {"slime", 9, -3}},
         {},
         {}},
        {"La Crypte Engloutie", {0.45f, 0.62f, 0.68f, 1}, {0.6f, 0.7f, 0.75f, 1}, {0.5f, 0.8f, 1.0f, 1}, "assets://terrain/rock.png",
         {{-12, -7}, {4, 0}, {12, 8}},
         {{-12, 3}, {-9, 3}, {-3, 8}, {-3, -8}, {9, -2}, {14, 0}},
         {{0, 8}},
         {{-15, -9}, {-3, 9.5f}, {15, 9}},
         {{15, -8}},
         {{"ambre.sign.spikes", -10, -1}},
         {{-6, -6}, {-6, -4}, {-6, -2}, {-6, 0}, {-6, 2}, {-6, 4}, {-6, 6}, {7, 4}, {7, 6}, {7, 8}},
         {},
         {{0, -5}, {0, 5}, {10, -5}},
         {{"slime", -1, -3}, {"slime", 2, 4}, {"slime", 11, 2}, {"bat", 4, -6}, {"bat", 12, 6}},
         {},
         {}},
        {"Les Jardins Suspendus", {0.55f, 0.75f, 0.45f, 1}, {0.7f, 0.75f, 0.6f, 1}, {0.7f, 1.0f, 0.5f, 1}, "assets://terrain/grass.png",
         {{-12, -7}, {1.8f, 0}, {12, 8}},
         {{-10, 6}, {-7, 6}, {-2.2f, 0}, {8, -4}, {11, -4}, {14, 2}},
         {{-14, 0}},
         {{-15, 9}, {15, -9}},
         {{-15, -9}},
         {{"ambre.sign.jump", -7, -2}},
         {},
         {},
         {{-10, -5}, {10, 5}},
         {{"slime", -9, 3}, {"slime", -6, -5}, {"archer", 12, -6}, {"archer", 12, 6}, {"bat", 8, 0}},
         {{-12.0f, 0.0f, 16.0f, 22.0f}, {12.0f, 0.0f, 16.0f, 22.0f}},  // chasm between x = -4 and x = 4
         {{-2.2f, 0}, {1.8f, 0}, {-2.2f, -7}, {1.8f, -7}}},
        {"La Forge Oubliée", {0.7f, 0.55f, 0.45f, 1}, {0.65f, 0.5f, 0.42f, 1}, {1.0f, 0.55f, 0.3f, 1}, "assets://terrain/rock.png",
         {{-12, 8}, {12, -8}, {0, 0}},  // the third appears when the puzzle is solved
         {{-12, -4}, {-9, -4}, {-4, 8}, {4, 8}, {9, 4}, {13, 4}},
         {{-4, -8}},
         {{-15, 9}, {-15, -9}, {15, 9}},
         {{14, -9}},
         {{"ambre.sign.plates", -6, 0}},
         {{9, -1}, {9, 1}},
         {{-2, -2}, {2, -2}, {-2, 2}, {2, 2}},
         {{-8, -5}, {-8, 5}, {6, -6}, {6, 6}},
         {{"slime", -11, 0}, {"slime", 11, 7}, {"archer", 13, -3}, {"brute", 10, 0}},
         {},
         {}},
        {"Le Sanctuaire", {0.6f, 0.55f, 0.7f, 1}, {0.7f, 0.65f, 0.78f, 1}, {0.75f, 0.55f, 1.0f, 1}, "assets://terrain/rock.png",
         {{-10, 8}, {2, -8}, {13, 6}},
         {{-12, -6}, {-6, -6}, {-2, 4}, {4, 4}, {9, -2}, {14, -6}},
         {{0, 0}, {10, 8}},
         {{-15, 9}, {15, -9}, {15, 9}},
         {{-15, -9}},
         {},
         {{-6, -2}, {-6, 0}, {-6, 2}, {-4, -2}, {-4, 2}, {6, -6}, {6, -4}, {6, 4}, {6, 6}},
         {},
         {{-8, -6}, {0, 6}, {8, -6}},
         {{"brute", -1, -4}, {"brute", 8, 5}, {"archer", 13, -7}, {"archer", 13, 7}, {"bat", 0, 6}, {"bat", 10, 0}, {"slime", -10, 4}},
         {},
         {}},
        {"Le Cœur d'Ambre", {0.7f, 0.45f, 0.35f, 1}, {0.75f, 0.5f, 0.4f, 1}, {1.0f, 0.5f, 0.25f, 1}, "assets://terrain/dirt.png",
         {},
         {{-12, -6}, {-12, 6}, {-6, 0}},
         {{-14, -8}, {-14, 8}},
         {{-15, 9}, {-15, -9}},
         {},
         {},
         {},
         {},
         {{-8, -6}, {-8, 6}, {8, -6}, {8, 6}},
         {{"boss", 6, 0}},
         {},
         {},
         true},
    };
    return l;
}

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
ecs::Entity billboard(Builder& b, ecs::Entity parent, const std::string& texture, float size, Vec4 color = {1, 1, 1, 1}, float height = 0.0f) {
    const auto s = b.entity("Sprite", {}, parent);
    b.reg.get<TransformComponent>(s).rotation = math::Quat::fromEuler({math::radians(kLean), 0.0f, 0.0f});
    auto& sr = b.reg.emplace<SpriteRendererComponent>(s);
    sr.texture = texture;
    sr.size = {size, height > 0.0f ? height : size};
    sr.pivot = {0.5f, 0.0f};
    sr.color = color;
    sr.sortingLayer = 1;
    return s;
}

/// Sprite lying on the ground (shadows, plates, danger zones); scripts keep shadows on the floor.
ecs::Entity flat(Builder& b, const std::string& name, ecs::Entity parent, const std::string& texture, Vec2 size, Vec4 color, float y = 0.02f) {
    const auto s = b.entity(name, {0.0f, y, 0.0f}, parent);
    b.reg.get<TransformComponent>(s).rotation = math::Quat::fromEuler({math::radians(-90.0f), 0.0f, 0.0f});
    auto& sr = b.reg.emplace<SpriteRendererComponent>(s);
    sr.texture = texture;
    sr.size = size;
    sr.color = color;
    sr.sortingLayer = 0;
    return s;
}

ecs::Entity groundShadow(Builder& b, ecs::Entity parent, float size, float alpha = 0.55f) {
    return flat(b, "Shadow", parent, "assets://games/shadow.png", {size, size * 0.75f}, {1.0f, 1.0f, 1.0f, alpha});
}

ecs::Entity enemy(Builder& b, const std::string& name, ecs::Entity parent, Vec3 pos, const std::string& kind) {
    const auto e = b.entity(name, pos, parent);
    const bool bat = kind == "bat", boss = kind == "boss", brute = kind == "brute", archer = kind == "archer";
    groundShadow(b, e, boss ? 3.6f : brute ? 1.8f : bat ? 0.9f : 1.1f, bat ? 0.35f : 0.55f);
    const char* tex = bat ? "assets://games/bat.png" : archer ? "assets://games/archer.png" : brute ? "assets://games/brute.png" : "assets://games/slime.png";
    const float size = boss ? 4.0f : brute ? 2.2f : archer ? 1.7f : bat ? 1.3f : kind == "minion" ? 0.9f : 1.2f;
    billboard(b, e, tex, size, boss ? Vec4{1.0f, 0.45f, 0.35f, 1.0f} : Vec4{1, 1, 1, 1});
    if (boss) flat(b, "Telegraph", e, "assets://games/disc.png", {6.4f, 6.4f}, {1.0f, 0.2f, 0.1f, 0.0f}, 0.03f);  // the slam's danger zone
    if (!bat) {
        auto& agent = b.reg.emplace<NavAgentComponent>(e);
        agent.speed = boss ? 3.2f : brute ? 1.8f : archer ? 2.2f : 2.4f;
        agent.stoppingDistance = boss ? 1.4f : 0.5f;
        agent.faceMovement = false;
    }
    b.script(e, "assets://games/scripts/ambre_enemy.lua", "{\"kind\": \"" + kind + "\"}");
    return e;
}

ecs::Entity prop(Builder& b, const std::string& name, ecs::Entity parent, Vec3 pos, const std::string& kind, const std::string& extra = {}) {
    const auto e = b.entity(name, pos, parent);
    b.script(e, "assets://games/scripts/ambre_prop.lua", "{\"kind\": \"" + kind + "\"" + extra + "}");
    return e;
}

void buildArea(Builder& b, ecs::Entity world, int k, const Level& a) {
    auto& reg = b.reg;
    const float cx = static_cast<float>(k) * kAreaSpacing;
    const auto area = b.entity("Area " + std::to_string(k + 1), {cx, 0.0f, 0.0f}, world);
    // Floor (one slab, or pieces around a chasm with stepping stones), walls, exit alcove and gate.
    if (a.floorPieces.empty()) {
        block(b, "Floor", {0.0f, -0.5f, 0.0f}, {kHalfW * 2.0f + 6.0f, 1.0f, kHalfD * 2.0f}, a.floor, a.floorTexture, area);
    } else {
        int n = 0;
        for (const Rect& r : a.floorPieces) {
            const float extra = r.x > 0.0f ? 3.0f : 0.0f;  // the right piece also floors the alcove
            block(b, n == 0 ? "Floor" : "Floor " + std::to_string(n + 1), {r.x + extra * 0.5f, -0.5f, r.z}, {r.w + extra, 1.0f, r.d}, a.floor, a.floorTexture,
                  area);
            ++n;
        }
        n = 0;
        for (const Spot& s : a.stones)
            block(b, "Stone " + std::to_string(++n), {s.x, -1.5f, s.z}, {1.8f, 3.0f, 1.8f}, a.wall, "assets://terrain/rock.png", area);
        const auto abyss = b.mesh("Abyss", PrimitiveMesh::Plane, {0.0f, -9.0f, 0.0f}, {8.0f, 1.0f, kHalfD * 2.0f}, {0.05f, 0.04f, 0.06f, 1.0f}, area);
        reg.get<MeshRendererComponent>(abyss).castShadows = false;
    }
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
    int n = 0;
    for (const Spot& p : a.pillars)
        block(b, "Pillar " + std::to_string(++n), {p.x, 1.5f, p.z}, {1.2f, 3.0f, 1.2f}, a.wall, "assets://terrain/rock.png", area);
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
    // Items: gems, coins, hearts.
    const auto items = b.entity("Items", {}, area);
    n = 0;
    for (const Spot& g : a.gems) {
        const auto e = b.entity("Gem " + std::to_string(++n), {g.x, 0.3f, g.z}, items);
        reg.get<TransformComponent>(groundShadow(b, e, 0.7f, 0.4f)).position.y = -0.28f;
        billboard(b, e, "assets://games/amber.png", 0.85f);
        auto& l = reg.emplace<PointLightComponent>(e);
        l.color = {1.0f, 0.7f, 0.3f, 1.0f};
        l.intensity = 2.5f;
        l.range = 3.0f;
    }
    n = 0;
    for (const Spot& c : a.coins) {
        const auto e = b.entity("Coin " + std::to_string(++n), {c.x, 0.3f, c.z}, items);
        reg.get<TransformComponent>(groundShadow(b, e, 0.5f, 0.35f)).position.y = -0.28f;
        billboard(b, e, "assets://games/coin.png", 0.6f);
    }
    n = 0;
    for (const Spot& h : a.hearts) {
        const auto e = b.entity("Heart " + std::to_string(++n), {h.x, 0.3f, h.z}, items);
        reg.get<TransformComponent>(groundShadow(b, e, 0.6f, 0.35f)).position.y = -0.28f;
        billboard(b, e, "assets://games/heart.png", 0.7f);
    }
    // Props: pots (break them), chests, hint signs, spike traps, pressure plates.
    const auto props = b.entity("Props", {}, area);
    n = 0;
    for (const Spot& p : a.pots) {
        const auto e = prop(b, "Pot " + std::to_string(++n), props, {p.x, 0.0f, p.z}, "pot");
        groundShadow(b, e, 0.9f, 0.45f);
        billboard(b, e, "assets://games/pot.png", 0.9f);
    }
    n = 0;
    for (const Spot& p : a.chests) {
        const auto e = prop(b, "Chest " + std::to_string(++n), props, {p.x, 0.0f, p.z}, "chest");
        groundShadow(b, e, 1.5f, 0.5f);
        billboard(b, e, "assets://games/chest.png", 1.3f);
    }
    n = 0;
    for (const Hint& s : a.signs) {
        const auto e = prop(b, "Sign " + std::to_string(++n), props, {s.x, 0.0f, s.z}, "sign", std::string(", \"text\": \"") + s.key + "\"");
        groundShadow(b, e, 0.9f, 0.4f);
        billboard(b, e, "assets://games/sign.png", 1.1f);
    }
    n = 0;
    for (const Spot& s : a.spikes) {
        const float phase = static_cast<float>(n % 4) * 0.7f;
        const auto e = prop(b, "Spikes " + std::to_string(++n), props, {s.x, 0.0f, s.z}, "spikes", ", \"phase\": " + std::to_string(phase));
        flat(b, "Base", e, "assets://games/plate.png", {1.9f, 1.9f}, {0.35f, 0.32f, 0.3f, 1.0f});
        billboard(b, e, "assets://games/spikes.png", 1.8f, {1, 1, 1, 1}, 1.2f);
    }
    n = 0;
    for (const Spot& p : a.plates) {
        ++n;
        const auto e = prop(b, "Plate " + std::to_string(n), props, {p.x, 0.0f, p.z}, "plate", ", \"index\": " + std::to_string(n));
        flat(b, "Sprite", e, "assets://games/plate.png", {1.8f, 1.8f}, {0.5f, 0.45f, 0.4f, 1.0f}, 0.03f);
        auto& l = reg.emplace<PointLightComponent>(e);
        l.color = {1.0f, 0.6f, 0.2f, 1.0f};
        l.intensity = 0.0f;  // lit by the puzzle
        l.range = 3.0f;
    }
    // Enemies.
    const auto foes = b.entity("Enemies", {}, area);
    std::map<std::string, int> count;
    for (const Foe& f : a.foes) {
        std::string label = f.kind;
        label[0] = static_cast<char>(std::toupper(static_cast<unsigned char>(label[0])));
        if (a.boss) label = "Boss";
        else label += " " + std::to_string(++count[f.kind]);
        enemy(b, label, foes, {f.x, std::string(f.kind) == "bat" ? 1.6f : 0.0f, f.z}, f.kind);
    }
    if (a.boss) {  // falling boulders of the boss's fury, announced by a red circle
        for (int i = 0; i < 3; ++i) {
            const auto r = b.entity("Boulder " + std::to_string(i + 1), {0.0f, 0.0f, 0.0f}, foes);
            flat(b, "Zone", r, "assets://games/disc.png", {2.6f, 2.6f}, {1.0f, 0.2f, 0.1f, 0.0f}, 0.03f);
            const auto rock = billboard(b, r, "assets://games/boulder.png", 1.4f);
            reg.get<TransformComponent>(rock).position.y = 12.0f;
            b.hide(r);
        }
    }
}

} // namespace

void buildAdventure25D(Scene& scene) {
    Builder b(scene);
    auto& reg = b.reg;
    const auto cam = b.camera3D({-13.0f, 10.0f, 12.0f}, {-13.0f, 1.0f, 0.0f}, 45.0f);
    b.script(cam, "assets://games/scripts/ambre_camera.lua");
    const auto sunLight = templates::sun(scene);
    {
        auto& env = reg.get<EnvironmentComponent>(sunLight);
        env.sky = SkyMode::Gradient;
        env.ambientIntensity = 0.85f;
        env.fog = true;
        env.fogStart = 28.0f;
        env.fogEnd = 62.0f;
        env.fogColor = {0.55f, 0.45f, 0.35f, 1.0f};
        reg.get<DirectionalLightComponent>(sunLight).color = {1.0f, 0.9f, 0.75f, 1.0f};
    }
    const auto world = b.entity("World");
    for (int k = 0; k < kAreaCount; ++k) buildArea(b, world, k, levels()[static_cast<usize>(k)]);

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
    groundShadow(b, hero, 1.2f);
    const auto heroSprite = billboard(b, hero, "", 1.7f);
    {
        auto& sr = reg.get<SpriteRendererComponent>(heroSprite);
        sr.sheet = "assets://2d/hero.agrsheet";
        reg.emplace<SpriteAnimatorComponent>(heroSprite).clip = "idle";
        reg.emplace<AnimatorComponent>(heroSprite).controller = "assets://animations/hero.agrctrl";
    }
    {  // sword swing: a crescent of light in front of the hero, shown by the script
        const auto slash = b.entity("Slash", {0.9f, 0.75f, 0.0f}, hero);
        reg.get<TransformComponent>(slash).rotation = math::Quat::fromEuler({math::radians(kLean), 0.0f, math::radians(-90.0f)});
        auto& sr = reg.emplace<SpriteRendererComponent>(slash);
        sr.texture = "assets://games/slash.png";
        sr.size = {1.9f, 1.2f};
        sr.color = {1.0f, 1.0f, 1.0f, 0.0f};
        sr.sortingLayer = 2;
    }
    b.script(hero, "assets://games/scripts/ambre_hero.lua");
    // Hidden prototypes: the boss's minions, the archers' arrows.
    const auto prototypes = b.entity("Prototypes");
    b.hide(prototypes);
    enemy(b, "Minion Prototype", prototypes, {}, "minion");
    {
        const auto arrow = b.entity("Arrow Prototype", {}, prototypes);
        const auto s = b.entity("Sprite", {}, arrow);
        reg.get<TransformComponent>(s).rotation = math::Quat::fromEuler({math::radians(-60.0f), 0.0f, 0.0f});
        auto& sr = reg.emplace<SpriteRendererComponent>(s);
        sr.texture = "assets://games/arrow.png";
        sr.size = {1.1f, 0.28f};
        sr.sortingLayer = 2;
        b.script(arrow, "assets://games/scripts/ambre_arrow.lua");
    }

    const auto nav = b.entity("Navigation");
    auto& ns = reg.emplace<NavigationSettingsComponent>(nav);
    ns.cellSize = 0.3f;
    ns.agentRadius = 0.4f;

    const auto game = b.entity("Game");
    b.script(game, "assets://games/scripts/ambre_game.lua");
    templates::audioSource(scene, "Music", "assets://audio/ambre_music.wav", AudioBus::Music, 0.45f);

    // ---- interface: HUD, touch controls, menus, shop.
    const auto hud = b.hud();
    const auto top = b.panel("Top Panel", hud, {0.0f, 0.0f}, {30.0f, 30.0f}, {560.0f, 190.0f}, {0.08f, 0.05f, 0.03f, 0.55f});
    const auto hearts = templates::uiElement(scene, "Hearts", top, {0.0f, 0.0f}, {20.0f, 12.0f}, {520.0f, 70.0f});
    for (int i = 0; i < 8; ++i) {  // up to 8 with the shop's upgrades
        const auto h = templates::uiElement(scene, "HeartIcon " + std::to_string(i + 1), hearts, {0.0f, 0.0f}, {static_cast<float>(i) * 64.0f, 0.0f},
                                            {58.0f, 58.0f});
        reg.emplace<UIImageComponent>(h).texture = "assets://games/heart.png";
    }
    const auto gems = templates::uiElement(scene, "GemSlots", top, {0.0f, 0.0f}, {20.0f, 90.0f}, {300.0f, 56.0f});
    for (int i = 0; i < 3; ++i) {
        const auto g = templates::uiElement(scene, "GemIcon " + std::to_string(i + 1), gems, {0.0f, 0.0f}, {static_cast<float>(i) * 64.0f, 0.0f}, {56.0f, 56.0f});
        auto& img = reg.emplace<UIImageComponent>(g);
        img.texture = "assets://games/amber.png";
        img.color = {0.25f, 0.2f, 0.15f, 0.8f};  // empty slot
    }
    const auto coinIcon = templates::uiElement(scene, "CoinIcon", top, {0.0f, 0.0f}, {300.0f, 92.0f}, {50.0f, 50.0f});
    reg.emplace<UIImageComponent>(coinIcon).texture = "assets://games/coin.png";
    b.label("CoinsText", top, {0.0f, 0.0f}, {360.0f, 90.0f}, {190.0f, 56.0f}, "", 40.0f);
    b.label("HealthText", top, {0.0f, 0.0f}, {20.0f, 150.0f}, {250.0f, 34.0f}, "", 26.0f, UIHAlign::Left, {1.0f, 0.6f, 0.55f, 0.9f});
    b.label("GemsText", top, {0.0f, 0.0f}, {270.0f, 150.0f}, {280.0f, 34.0f}, "", 26.0f, UIHAlign::Left, {1.0f, 0.8f, 0.4f, 0.9f});
    b.label("LevelText", hud, {0.5f, 0.0f}, {0.0f, 30.0f}, {1000.0f, 60.0f}, "", 40.0f, UIHAlign::Center);
    b.label("TimeText", hud, {1.0f, 0.0f}, {-200.0f, 40.0f}, {220.0f, 50.0f}, "", 36.0f, UIHAlign::Right, {1.0f, 0.95f, 0.8f, 0.9f});
    b.label("Message", hud, {0.5f, 0.3f}, {0.0f, 0.0f}, {1500.0f, 120.0f}, "", 60.0f, UIHAlign::Center, {1.0f, 0.88f, 0.5f, 1.0f});
    {
        const auto hint = b.panel("HintPanel", hud, {0.5f, 1.0f}, {0.0f, -40.0f}, {1100.0f, 110.0f}, {0.08f, 0.05f, 0.03f, 0.75f});
        reg.get<UIRectComponent>(hint).pivot = {0.5f, 1.0f};
        reg.get<UIImageComponent>(hint).blocksPointer = false;
        b.label("Hint", hint, {0.5f, 0.5f}, {0.0f, 0.0f}, {1060.0f, 100.0f}, "", 34.0f, UIHAlign::Center);
        b.hide(hint);
    }
    const auto bossBar = b.panel("BossBar", hud, {0.5f, 0.0f}, {0.0f, 100.0f}, {900.0f, 30.0f}, {0.2f, 0.05f, 0.05f, 0.9f});
    b.panel("BossFill", bossBar, {0.0f, 0.0f}, {0.0f, 0.0f}, {900.0f, 30.0f}, {0.9f, 0.35f, 0.2f, 1.0f});
    b.hide(bossBar);
    b.button("PauseButton", hud, {1.0f, 0.0f}, {-30.0f, 30.0f}, {150.0f, 110.0f}, "II", game, "togglePause");
    const auto jump = templates::uiButton(scene, "Jump", hud, {1.0f, 1.0f}, {-40.0f, -40.0f}, {220.0f, 200.0f}, "Saut");
    reg.get<UITextComponent>(scene.children(jump).front()).localizationKey = "hud.jump";
    const auto attack = templates::uiButton(scene, "Attack", hud, {1.0f, 1.0f}, {-290.0f, -40.0f}, {220.0f, 200.0f}, "Épée");
    reg.get<UITextComponent>(scene.children(attack).front()).localizationKey = "rpg.attack";
    const auto dash = templates::uiButton(scene, "Dash", hud, {1.0f, 1.0f}, {-165.0f, -270.0f}, {200.0f, 150.0f}, "Roulade");
    reg.get<UITextComponent>(scene.children(dash).front()).localizationKey = "ambre.dash";
    // Virtual stick (the move_x / move_y touch stick reads the lower-left of the screen).
    const auto stick = templates::uiElement(scene, "Stick", hud, {0.0f, 1.0f}, {90.0f, -90.0f}, {260.0f, 260.0f});
    {
        auto& img = reg.emplace<UIImageComponent>(stick);
        img.texture = "assets://games/ring.png";
        img.color = {1.0f, 0.95f, 0.85f, 0.45f};
        img.blocksPointer = false;
        const auto knob = templates::uiElement(scene, "StickKnob", stick, {0.5f, 0.5f}, {0.0f, 0.0f}, {120.0f, 120.0f});
        auto& kn = reg.emplace<UIImageComponent>(knob);
        kn.texture = "assets://games/disc.png";
        kn.color = {1.0f, 0.85f, 0.55f, 0.7f};
        kn.blocksPointer = false;
    }

    // Menu: one panel; ambre_game.lua shows the buttons of the current page and places them.
    const auto menu = b.panel("Menu", hud, {0.5f, 0.5f}, {0.0f, 0.0f}, {1000.0f, 880.0f}, {0.08f, 0.06f, 0.05f, 0.94f});
    b.label("MenuTitle", menu, {0.5f, 0.0f}, {0.0f, 40.0f}, {960.0f, 100.0f}, "Les Ruines d'Ambre", 72.0f, UIHAlign::Center, {1.0f, 0.75f, 0.3f, 1.0f});
    b.label("MenuText", menu, {0.5f, 0.0f}, {0.0f, 150.0f}, {900.0f, 230.0f}, "", 32.0f, UIHAlign::Center);
    const char* buttons[][2] = {{"PlayButton", ""} /* label set by the script */, {"LevelsButton", "ambre.levels"}, {"OptionsButton", "ambre.options"},
                                {"ResumeButton", "ambre.resume"}, {"RestartButton", "ambre.restart"}, {"RetryButton", "ambre.retry"},
                                {"NextButton", "ambre.next"},     {"MenuButton", "ambre.menu"},       {"BackButton", "ambre.back"},
                                {"MusicButton", ""},              {"SfxButton", ""},                  {"LanguageButton", ""}};
    for (const auto& bt : buttons) {
        const auto e = b.button(bt[0], menu, {0.5f, 0.0f}, {0.0f, 400.0f}, {620.0f, 88.0f}, "", game, "pressed");
        b.buttonText(e).localizationKey = bt[1];
        reg.get<UIRectComponent>(e).pivot = {0.5f, 0.0f};
    }
    for (int i = 0; i < kAreaCount; ++i) {
        const auto e = b.button("LevelButton " + std::to_string(i + 1), menu, {0.5f, 0.0f}, {0.0f, 300.0f}, {450.0f, 120.0f}, "", game, "pressed");
        reg.get<UIRectComponent>(e).pivot = {0.5f, 0.0f};
        b.buttonText(e).fontSize = 30.0f;
    }
    // Shop between halls: results of the hall, upgrades, continue.
    const auto shop = b.panel("Shop", hud, {0.5f, 0.5f}, {0.0f, 0.0f}, {1200.0f, 880.0f}, {0.08f, 0.06f, 0.05f, 0.95f});
    b.label("ShopTitle", shop, {0.5f, 0.0f}, {0.0f, 30.0f}, {1100.0f, 90.0f}, "", 64.0f, UIHAlign::Center, {1.0f, 0.75f, 0.3f, 1.0f});
    b.label("ShopResults", shop, {0.5f, 0.0f}, {0.0f, 125.0f}, {1100.0f, 150.0f}, "", 34.0f, UIHAlign::Center);
    b.label("ShopCoins", shop, {0.5f, 0.0f}, {0.0f, 280.0f}, {1100.0f, 60.0f}, "", 40.0f, UIHAlign::Center, {1.0f, 0.85f, 0.35f, 1.0f});
    const char* wares[][2] = {{"BuyHeart", "assets://games/heart.png"},
                              {"BuySword", "assets://games/icon_sword.png"},
                              {"BuyDash", "assets://games/icon_bolt.png"},
                              {"BuyPotion", "assets://games/icon_potion.png"}};
    for (int i = 0; i < 4; ++i) {
        const float x = (i % 2 == 0 ? -280.0f : 280.0f), y = 360.0f + static_cast<float>(i / 2) * 150.0f;
        const auto e = b.button(wares[i][0], shop, {0.5f, 0.0f}, {x, y}, {540.0f, 130.0f}, "", game, "pressed");
        reg.get<UIRectComponent>(e).pivot = {0.5f, 0.0f};
        b.buttonText(e).fontSize = 34.0f;
        reg.get<UIRectComponent>(scene.children(e).front()).position = {50.0f, 0.0f};
        const auto icon = templates::uiElement(scene, "Icon", e, {0.0f, 0.5f}, {20.0f, 0.0f}, {84.0f, 84.0f});
        auto& img = reg.emplace<UIImageComponent>(icon);
        img.texture = wares[i][1];
        img.blocksPointer = false;
    }
    const auto next = b.button("ShopContinue", shop, {0.5f, 0.0f}, {0.0f, 700.0f}, {620.0f, 100.0f}, "", game, "pressed");
    reg.get<UIRectComponent>(next).pivot = {0.5f, 0.0f};
    b.buttonText(next).localizationKey = "ambre.continue";
    b.hide(shop);
}

} // namespace agr::editor::games
