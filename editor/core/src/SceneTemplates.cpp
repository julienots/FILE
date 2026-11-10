#include "agr/editor/SceneTemplates.hpp"

#include "agr/render/RenderComponents.hpp"
#include "agr/twod/SpriteAnimator.hpp"
#include "agr/twod/TilemapEditing.hpp"

namespace agr::editor {

using math::Quat;
using math::Vec3;

namespace {

void lookAt(Scene& scene, ecs::Entity e, Vec3 eye, Vec3 target) {
    auto& t = scene.registry().get<TransformComponent>(e);
    Vec3 s;
    math::decompose(math::inverse(math::Mat4::lookAt(eye, target, Vec3::up())), t.position, t.rotation, s);
}

ecs::Entity sun(Scene& scene) {
    const auto light = scene.createEntity("Sun Light");
    scene.registry().emplace<DirectionalLightComponent>(light).intensity = 1.1f;
    scene.registry().emplace<EnvironmentComponent>(light);
    scene.registry().get<TransformComponent>(light).rotation =
        Quat::fromEuler({math::radians(-50.0f), math::radians(35.0f), 0.0f});
    return light;
}

} // namespace

const char* templateName(SceneTemplate t) {
    switch (t) {
        case SceneTemplate::Empty2D: return "Scène 2D vide";
        case SceneTemplate::Empty25D: return "Scène 2.5D vide";
        case SceneTemplate::Empty3D: return "Scène 3D vide";
        case SceneTemplate::Showcase2D: return "Vitrine 2D";
    }
    return "?";
}

const char* templateDescription(SceneTemplate t) {
    switch (t) {
        case SceneTemplate::Empty2D: return "Caméra orthographique et un sprite : base d'un jeu 2D (plateforme, puzzle, cartes...).";
        case SceneTemplate::Empty25D: return "Caméra perspective inclinée, sol 3D et sprites : base d'un jeu 2.5D (stratégie, tower defense...).";
        case SceneTemplate::Empty3D: return "Caméra, soleil, ambiance et sol texturé : base d'un jeu 3D.";
        case SceneTemplate::Showcase2D:
            return "Décor de nuit en pixel art : tilemap, héros animé, fonds en parallaxe, lampe (lumière 2D) et lucioles "
                   "(particules). Montre les systèmes 2D ; pas encore de gameplay (scripts et physique : phase 6).";
    }
    return "";
}

SceneTemplate defaultTemplateFor(ProjectMode mode) {
    switch (mode) {
        case ProjectMode::Mode2D: return SceneTemplate::Empty2D;
        case ProjectMode::Mode25D: return SceneTemplate::Empty25D;
        case ProjectMode::Mode3D: return SceneTemplate::Empty3D;
    }
    return SceneTemplate::Empty3D;
}

std::vector<SceneTemplate> templatesFor(ProjectMode mode) {
    if (mode == ProjectMode::Mode2D) return {SceneTemplate::Empty2D, SceneTemplate::Showcase2D};
    return {defaultTemplateFor(mode)};
}

std::vector<SceneTemplate> allTemplates() {
    return {SceneTemplate::Empty2D, SceneTemplate::Showcase2D, SceneTemplate::Empty25D, SceneTemplate::Empty3D};
}

namespace {

/// Night scene built only from the original template art in assets/2d.
void buildShowcase2D(Scene& scene) {
    auto& reg = scene.registry();
    const auto cam = scene.createEntity("Main Camera");
    auto& c = reg.emplace<CameraComponent>(cam);
    c.projection = Projection::Orthographic;
    c.orthographicSize = 5.0f;
    c.scaleMode = ScaleMode::FixedHeight;
    c.clearColor = {0.11f, 0.12f, 0.24f, 1.0f};
    reg.get<TransformComponent>(cam).position = {0.0f, 0.0f, 10.0f};

    const auto env = scene.createEntity("Night Ambience");
    auto& en = reg.emplace<EnvironmentComponent>(env);
    en.ambientColor = {0.55f, 0.62f, 0.85f, 1.0f};
    en.ambientIntensity = 0.5f;

    // Background layers: the farther, the closer the parallax factor gets to 0
    // (0 = follows the camera). Textures repeat horizontally (uvRect.z = repeats).
    struct Layer {
        const char* name;
        const char* texture;
        float factor, y, height, repeats;
        i32 order;
    };
    const Layer layers[] = {{"Sky", "assets://2d/bg_sky.png", 0.0f, 0.0f, 10.5f, 2.0f, 0},  // 40 units: up to ~4:1 screens
                            {"Mountains", "assets://2d/bg_mountains.png", 0.25f, 0.5f, 10.0f, 4.0f, 1},
                            {"Hills", "assets://2d/bg_hills.png", 0.5f, -0.5f, 10.0f, 4.0f, 2}};
    const auto background = scene.createEntity("Background");
    for (const Layer& l : layers) {
        const auto e = scene.createEntity(l.name, background);
        reg.get<TransformComponent>(e).position = {0.0f, l.y, 0.0f};
        reg.emplace<ParallaxComponent>(e).factor = {l.factor, l.factor};
        auto& s = reg.emplace<SpriteRendererComponent>(e);
        s.texture = l.texture;
        s.size = {20.0f * l.repeats, l.height};
        s.uvRect = {0.0f, 0.0f, l.repeats, 1.0f};
        s.sortingLayer = -10;
        s.orderInLayer = l.order;
    }

    // Ground: tile indices of assets/2d/forest_tiles.agrsheet (8 columns).
    enum : i32 { GrassTop = 0, Dirt = 1, DarkDirt = 2, Stone = 3, StoneTop = 4, Plank = 5, Leaves = 6,
                 GrassLeft = 8, GrassRight = 9, Flowers = 10, Rock = 11, Bush = 12, Water = 16, WaterTop = 17, Crate = 18,
                 Sign = 19, Mushroom = 20 };
    const auto ground = scene.createEntity("Ground");
    reg.get<TransformComponent>(ground).position = {-20.0f, -5.0f, 0.0f};
    auto& tm = reg.emplace<TilemapComponent>(ground);
    tm.tileset = "assets://2d/forest_tiles.agrsheet";
    tm.width = 40;
    tm.height = 9;
    tm.lit = true;
    tilemapEnsureLayout(tm);
    tilemapFillRect(tm, 0, 0, 39, 0, DarkDirt);
    tilemapFillRect(tm, 0, 1, 39, 2, Dirt);
    tilemapFillRect(tm, 0, 3, 39, 3, GrassTop);
    tilemapFillRect(tm, 24, 1, 27, 3, -1);  // pond
    tilemapFillRect(tm, 24, 1, 27, 1, Water);
    tilemapFillRect(tm, 24, 2, 27, 2, WaterTop);
    tilemapSet(tm, 23, 3, GrassRight);
    tilemapSet(tm, 28, 3, GrassLeft);
    tilemapFillRect(tm, 30, 4, 33, 5, Stone);
    tilemapFillRect(tm, 30, 6, 33, 6, StoneTop);
    tilemapFillRect(tm, 9, 7, 13, 7, Plank);
    tilemapFillRect(tm, 3, 6, 4, 7, Leaves);
    for (auto [x, tile] : {std::pair{1, Bush}, {6, Flowers}, {11, Rock}, {15, Mushroom}, {17, Flowers}, {21, Crate}, {29, Sign},
                           {35, Bush}, {38, Flowers}})
        tilemapSet(tm, x, 4, tile);

    const auto hero = scene.createEntity("Hero");
    reg.get<TransformComponent>(hero).position = {-3.0f, -1.0f, 0.0f};
    auto& hs = reg.emplace<SpriteRendererComponent>(hero);
    hs.sheet = "assets://2d/hero.agrsheet";
    hs.frame = 4;
    hs.size = {1.5f, 1.5f};
    hs.lit = true;
    hs.sortingLayer = 1;
    reg.emplace<SpriteAnimatorComponent>(hero).clip = "run";

    const auto lamp = scene.createEntity("Lamp");
    reg.get<TransformComponent>(lamp).position = {2.0f, -1.0f, 0.0f};
    auto& ls = reg.emplace<SpriteRendererComponent>(lamp);
    ls.texture = "assets://2d/lamp.png";
    ls.pixelArt = true;
    ls.size = {1.0f, 2.0f};
    ls.pivot = {0.5f, 0.0f};
    ls.sortingLayer = 1;
    const auto glow = scene.createEntity("Lamp Light", lamp);
    reg.get<TransformComponent>(glow).position = {0.0f, 1.6f, 0.0f};
    auto& light = reg.emplace<Light2DComponent>(glow);
    light.color = {1.0f, 0.8f, 0.5f, 1.0f};
    light.intensity = 2.6f;
    light.radius = 7.0f;
    const auto flies = scene.createEntity("Fireflies", lamp);
    reg.get<TransformComponent>(flies).position = {0.0f, 1.4f, 0.0f};
    auto& p = reg.emplace<ParticleEmitter2DComponent>(flies);
    p.rate = 4.0f;
    p.burst = 6;  // visible as soon as the scene opens
    p.duration = 3.0f;
    p.maxParticles = 64;
    p.lifetimeMin = 2.0f;
    p.lifetimeMax = 4.0f;
    p.speedMin = 0.15f;
    p.speedMax = 0.5f;
    p.spread = math::radians(180.0f);
    p.spawnRadius = 1.2f;
    p.gravity = {0.0f, 0.05f};
    p.drag = 0.2f;
    p.startColor = {1.0f, 0.9f, 0.45f, 1.0f};
    p.endColor = {1.0f, 0.7f, 0.2f, 0.0f};
    p.startSize = 0.16f;
    p.endSize = 0.08f;
    p.additive = true;
    p.sortingLayer = 2;
}

} // namespace

void buildSceneTemplate(Scene& scene, SceneTemplate tmpl) {
    registerRenderComponents();
    registerSpriteAnimatorComponent();
    auto& reg = scene.registry();
    const char* mark = "assets://textures/agr_mark.png";
    const char* grid = "assets://textures/grid.png";
    switch (tmpl) {
        case SceneTemplate::Empty2D: {
            const auto cam = scene.createEntity("Main Camera");
            auto& c = reg.emplace<CameraComponent>(cam);
            c.projection = Projection::Orthographic;
            c.orthographicSize = 5.0f;
            reg.get<TransformComponent>(cam).position = {0.0f, 0.0f, 10.0f};
            const auto logo = scene.createEntity("AGR Mark");
            auto& s = reg.emplace<SpriteRendererComponent>(logo);
            s.texture = mark;
            s.size = {2.0f, 2.0f};
            break;
        }
        case SceneTemplate::Empty25D: {
            const auto cam = scene.createEntity("Main Camera");
            reg.emplace<CameraComponent>(cam).fieldOfView = math::radians(45.0f);
            lookAt(scene, cam, {0.0f, 9.0f, 9.0f}, {0.0f, 0.0f, 0.0f});
            sun(scene);
            const auto ground = scene.createEntity("Ground");
            reg.get<TransformComponent>(ground).scale = {20.0f, 1.0f, 20.0f};
            auto& gm = reg.emplace<MeshRendererComponent>(ground);
            gm.mesh = PrimitiveMesh::Plane;
            gm.texture = grid;
            gm.uvScale = {10.0f, 10.0f};
            const auto unit = scene.createEntity("Unit");
            reg.get<TransformComponent>(unit).position = {0.0f, 1.0f, 0.0f};
            auto& s = reg.emplace<SpriteRendererComponent>(unit);
            s.texture = mark;
            s.size = {2.0f, 2.0f};
            break;
        }
        case SceneTemplate::Empty3D: {
            const auto cam = scene.createEntity("Main Camera");
            reg.emplace<CameraComponent>(cam);
            lookAt(scene, cam, {0.0f, 3.0f, 7.0f}, {0.0f, 0.5f, 0.0f});
            sun(scene);
            const auto ground = scene.createEntity("Ground");
            reg.get<TransformComponent>(ground).scale = {20.0f, 1.0f, 20.0f};
            auto& gm = reg.emplace<MeshRendererComponent>(ground);
            gm.mesh = PrimitiveMesh::Plane;
            gm.texture = grid;
            gm.uvScale = {10.0f, 10.0f};
            const auto cube = scene.createEntity("Cube");
            reg.get<TransformComponent>(cube).position = {0.0f, 0.5f, 0.0f};
            reg.emplace<MeshRendererComponent>(cube).color = {0.96f, 0.65f, 0.14f, 1.0f};
            break;
        }
        case SceneTemplate::Showcase2D: buildShowcase2D(scene); break;
    }
    scene.updateTransforms();
}

} // namespace agr::editor
