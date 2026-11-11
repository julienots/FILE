#include "agr/editor/SceneTemplates.hpp"

#include "agr/render/RenderComponents.hpp"
#include "agr/physics2d/Physics2DComponents.hpp"
#include "agr/physics3d/Physics3DComponents.hpp"
#include "agr/script/ScriptComponent.hpp"
#include "agr/twod/SpriteAnimator.hpp"
#include "agr/twod/TilemapEditing.hpp"
#include "agr/animation/AnimationSystem.hpp"
#include "agr/audio/AudioComponents.hpp"
#include "agr/ui/UIComponents.hpp"

namespace agr::editor {

using math::Quat;
using math::Vec3;

namespace {

// ------------------------------------------------------------------ UI helpers
ecs::Entity uiCanvas(Scene& scene) {
    const auto c = scene.createEntity("HUD");
    auto& cv = scene.registry().emplace<UICanvasComponent>(c);
    cv.referenceResolution = {1920.0f, 1080.0f};
    cv.match = 0.5f;
    return c;
}

ecs::Entity uiElement(Scene& scene, const std::string& name, ecs::Entity parent, math::Vec2 anchor, math::Vec2 position, math::Vec2 size) {
    const auto e = scene.createEntity(name, parent);
    auto& r = scene.registry().emplace<UIRectComponent>(e);
    r.anchorMin = r.anchorMax = r.pivot = anchor;
    r.position = position;
    r.size = size;
    return e;
}

/// Text filling its parent.
ecs::Entity uiLabel(Scene& scene, const std::string& name, ecs::Entity parent, const std::string& text, float fontSize, math::Vec4 color) {
    const auto e = scene.createEntity(name, parent);
    auto& r = scene.registry().emplace<UIRectComponent>(e);
    r.anchorMin = {0.0f, 0.0f};
    r.anchorMax = {1.0f, 1.0f};
    r.size = {0.0f, 0.0f};
    auto& t = scene.registry().emplace<UITextComponent>(e);
    t.text = text;
    t.fontSize = fontSize;
    t.color = color;
    return e;
}

ecs::Entity uiPanel(Scene& scene, const std::string& name, ecs::Entity parent, math::Vec2 anchor, math::Vec2 position, math::Vec2 size) {
    const auto e = uiElement(scene, name, parent, anchor, position, size);
    scene.registry().emplace<UIImageComponent>(e).color = {0.05f, 0.06f, 0.08f, 0.62f};
    return e;
}

ecs::Entity uiButton(Scene& scene, const std::string& name, ecs::Entity parent, math::Vec2 anchor, math::Vec2 position, math::Vec2 size,
                     const std::string& label) {
    const auto e = uiElement(scene, name, parent, anchor, position, size);
    scene.registry().emplace<UIImageComponent>(e).color = {1.0f, 1.0f, 1.0f, 0.22f};
    auto& b = scene.registry().emplace<UIButtonComponent>(e);
    b.pressedColor = {2.2f, 1.6f, 0.6f, 2.2f};  // brighter, warmer while held
    uiLabel(scene, "Label", e, label, size.y * 0.42f, {1.0f, 1.0f, 1.0f, 0.9f});
    return e;
}

ecs::Entity audioSource(Scene& scene, const std::string& name, const std::string& clip, AudioBus bus, float volume) {
    const auto e = scene.createEntity(name);
    auto& a = scene.registry().emplace<AudioSourceComponent>(e);
    a.clip = clip;
    a.bus = bus;
    a.volume = volume;
    a.loop = true;
    a.stream = true;
    return e;
}

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
        case SceneTemplate::Showcase3D: return "Vitrine 3D";
    }
    return "?";
}

const char* templateDescription(SceneTemplate t) {
    switch (t) {
        case SceneTemplate::Empty2D: return "Caméra orthographique et un sprite : base d'un jeu 2D (plateforme, puzzle, cartes...).";
        case SceneTemplate::Empty25D: return "Caméra perspective inclinée, sol 3D et sprites : base d'un jeu 2.5D (stratégie, tower defense...).";
        case SceneTemplate::Empty3D: return "Caméra, soleil, ambiance et sol texturé : base d'un jeu 3D.";
        case SceneTemplate::Showcase2D:
            return "Petit jeu de plateforme jouable en pixel art : héros scripté (Lua) avec physique 2D, caisse à pousser, "
                   "champignons à ramasser, tilemap solide, parallaxe, lumière 2D, lucioles, musique et sons, animations (machine à "
                   "états) et interface (score, boutons tactiles). Flèches + Espace ou boutons à l'écran.";
        case SceneTemplate::Showcase3D:
            return "Matériaux PBR, modèles glTF importés, ombres, lumières, ciel et brume, avec physique 3D : en jeu, "
                   "Espace / clic / toucher lance des balles sur les caisses (script Lua), avec sons, ambiance, objets animés et "
                   "interface (compteur, curseur de vitesse), et un personnage animé (squelette glTF) qui danse quand une balle le touche.";
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
    if (mode == ProjectMode::Mode3D) return {SceneTemplate::Empty3D, SceneTemplate::Showcase3D};
    return {defaultTemplateFor(mode)};
}

std::vector<SceneTemplate> allTemplates() {
    return {SceneTemplate::Empty2D, SceneTemplate::Showcase2D, SceneTemplate::Empty25D, SceneTemplate::Empty3D, SceneTemplate::Showcase3D};
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
    // Two tilemaps: "Ground" is solid (TilemapCollider2D), "Decor" is drawn over it without collision.
    const auto ground = scene.createEntity("Ground");
    reg.get<TransformComponent>(ground).position = {-20.0f, -5.0f, 0.0f};
    {
        auto& tm = reg.emplace<TilemapComponent>(ground);
        tm.tileset = "assets://2d/forest_tiles.agrsheet";
        tm.width = 40;
        tm.height = 9;
        tm.lit = true;
        tilemapEnsureLayout(tm);
        tilemapFillRect(tm, 0, 0, 39, 0, DarkDirt);
        tilemapFillRect(tm, 0, 1, 39, 2, Dirt);
        tilemapFillRect(tm, 0, 3, 39, 3, GrassTop);
        tilemapFillRect(tm, 24, 0, 27, 3, -1);  // pond: falling in sends the hero back to the start
        tilemapSet(tm, 23, 3, GrassRight);
        tilemapSet(tm, 28, 3, GrassLeft);
        tilemapFillRect(tm, 30, 4, 33, 5, Stone);
        tilemapFillRect(tm, 30, 6, 33, 6, StoneTop);
        tilemapFillRect(tm, 9, 7, 13, 7, Plank);
    }
    reg.emplace<TilemapCollider2DComponent>(ground).friction = 0.8f;
    const auto decor = scene.createEntity("Decor");
    reg.get<TransformComponent>(decor).position = {-20.0f, -5.0f, 0.0f};
    {
        auto& tm = reg.emplace<TilemapComponent>(decor);
        tm.tileset = "assets://2d/forest_tiles.agrsheet";
        tm.width = 40;
        tm.height = 9;
        tm.lit = true;
        tm.orderInLayer = 1;
        tilemapEnsureLayout(tm);
        tilemapFillRect(tm, 24, 0, 27, 1, Water);
        tilemapFillRect(tm, 24, 2, 27, 2, WaterTop);
        tilemapFillRect(tm, 3, 6, 4, 7, Leaves);
        for (auto [x, tile] : {std::pair{1, Bush}, {6, Flowers}, {11, Rock}, {17, Flowers}, {29, Sign}, {35, Bush}, {38, Flowers}})
            tilemapSet(tm, x, 4, tile);
    }

    // Playable hero: Lua script + 2D physics (capsule whose bottom is at the feet).
    const auto hero = scene.createEntity("Hero");
    reg.get<TransformComponent>(hero).position = {-3.0f, -1.0f, 0.0f};
    {
        auto& hs = reg.emplace<SpriteRendererComponent>(hero);
        hs.sheet = "assets://2d/hero.agrsheet";
        hs.frame = 0;
        hs.size = {1.5f, 1.5f};
        hs.lit = true;
        hs.sortingLayer = 1;
    }
    reg.emplace<SpriteAnimatorComponent>(hero).clip = "idle";
    {
        auto& rb = reg.emplace<RigidBody2DComponent>(hero);
        rb.fixedRotation = true;
        rb.gravityScale = 2.0f;
    }
    {
        auto& col = reg.emplace<Collider2DComponent>(hero);
        col.shape = ColliderShape2D::Capsule;
        col.size = {0.7f, 1.3f};
        col.offset = {0.0f, 0.65f};
        col.friction = 0.0f;  // does not stick to walls
    }
    reg.emplace<ScriptComponent>(hero).script = "assets://scripts/platformer_hero.lua";
    reg.emplace<AnimatorComponent>(hero).controller = "assets://animations/hero.agrctrl";  // Idle / Run / Air
    reg.emplace<ScriptComponent>(cam).script = "assets://scripts/camera_follow.lua";

    // A crate to push (dynamic body) and mushrooms to collect (triggers).
    const auto crate = scene.createEntity("Crate");
    reg.get<TransformComponent>(crate).position = {-6.0f, 1.0f, 0.0f};
    {
        auto& cs = reg.emplace<SpriteRendererComponent>(crate);
        cs.sheet = "assets://2d/forest_tiles.agrsheet";
        cs.frame = Crate;
        cs.lit = true;
        cs.sortingLayer = 1;
    }
    reg.emplace<RigidBody2DComponent>(crate);
    reg.emplace<Collider2DComponent>(crate).density = 0.5f;
    const math::Vec2 mushrooms[] = {{-14.0f, -0.4f}, {-8.5f, 3.6f}, {12.0f, 2.6f}};
    for (int i = 0; i < 3; ++i) {
        const auto m = scene.createEntity("Mushroom " + std::to_string(i + 1));
        reg.get<TransformComponent>(m).position = {mushrooms[i].x, mushrooms[i].y, 0.0f};
        auto& ms = reg.emplace<SpriteRendererComponent>(m);
        ms.sheet = "assets://2d/forest_tiles.agrsheet";
        ms.frame = Mushroom;
        ms.lit = true;
        ms.sortingLayer = 1;
        auto& col = reg.emplace<Collider2DComponent>(m);
        col.shape = ColliderShape2D::Circle;
        col.radius = 0.35f;
        col.isTrigger = true;
        reg.emplace<ScriptComponent>(m).script = "assets://scripts/collectible.lua";
        reg.emplace<AnimatorComponent>(m).clip = "assets://animations/mushroom_pulse.agranim";
    }

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

    // Music, and the HUD: score, touch controls (held buttons read by the hero script).
    audioSource(scene, "Music", "assets://audio/music.wav", AudioBus::Music, 0.4f);
    const auto hud = uiCanvas(scene);
    const auto panel = uiPanel(scene, "Score Panel", hud, {0.0f, 0.0f}, {30.0f, 30.0f}, {520.0f, 84.0f});
    uiLabel(scene, "Score", panel, "Champignons : 0 / 3", 40.0f, {1.0f, 0.93f, 0.75f, 1.0f});
    uiButton(scene, "Left", hud, {0.0f, 1.0f}, {40.0f, -40.0f}, {190.0f, 170.0f}, "<");
    uiButton(scene, "Right", hud, {0.0f, 1.0f}, {260.0f, -40.0f}, {190.0f, 170.0f}, ">");
    const auto jump = uiButton(scene, "Jump", hud, {1.0f, 1.0f}, {-40.0f, -40.0f}, {260.0f, 170.0f}, "Saut");
    reg.get<UITextComponent>(scene.children(jump).front()).localizationKey = "hud.jump";
}

/// Daylight scene built from the original assets in assets/3d (tools/generate_template_3d_art.py).
void buildShowcase3D(Scene& scene) {
    auto& reg = scene.registry();
    const auto cam = scene.createEntity("Main Camera");
    reg.emplace<CameraComponent>(cam).fieldOfView = math::radians(50.0f);
    lookAt(scene, cam, {0.0f, 3.6f, 9.5f}, {0.0f, 0.5f, 0.0f});
    reg.emplace<ScriptComponent>(cam).script = "assets://scripts/ball_launcher.lua";  // Space / click / tap: throw balls
    {
        // HDR: the neon bar and the sun's highlights glow; slight vignette and contrast.
        auto& pp = reg.emplace<PostProcessComponent>(cam);
        pp.bloomIntensity = 0.5f;
        pp.bloomThreshold = 1.2f;
        pp.contrast = 1.05f;
        pp.saturation = 1.08f;
        pp.vignette = 0.25f;
    }

    const auto sunE = scene.createEntity("Sun");
    auto& sunL = reg.emplace<DirectionalLightComponent>(sunE);
    sunL.intensity = 1.7f;
    sunL.shadowDistance = 30.0f;
    reg.get<TransformComponent>(sunE).rotation = Quat::fromEuler({math::radians(-42.0f), math::radians(38.0f), 0.0f});
    auto& env = reg.emplace<EnvironmentComponent>(sunE);
    // Image sky (original procedural HDR): background, diffuse light and reflections. The
    // gradient colors stand in while it loads.
    env.sky = SkyMode::Image;
    env.skyTexture = "assets://sky/afternoon.hdr";
    env.ambientIntensity = 0.8f;
    env.fog = true;
    env.fogStart = 25.0f;
    env.fogEnd = 120.0f;
    env.fogColor = env.horizonColor;

    const auto ground = scene.createEntity("Ground");
    reg.get<TransformComponent>(ground).scale = {40.0f, 1.0f, 40.0f};
    {
        auto& gm = reg.emplace<MeshRendererComponent>(ground);
        gm.mesh = PrimitiveMesh::Plane;
        gm.texture = "assets://textures/grid.png";
        gm.uvScale = {20.0f, 20.0f};
        gm.roughness = 0.9f;
        auto& gc = reg.emplace<ColliderComponent>(ground);  // static: no RigidBody
        gc.size = {1.0f, 0.2f, 1.0f};
        gc.offset = {0.0f, -0.1f, 0.0f};
    }

    // Material spheres: metallic row (back) and dielectric row (front), roughness 0 -> 1.
    const auto spheres = scene.createEntity("Roughness Spheres");
    for (int row = 0; row < 2; ++row) {
        for (int i = 0; i < 5; ++i) {
            const auto s = scene.createEntity(row ? "Metal" : "Dielectric", spheres);
            auto& t = reg.get<TransformComponent>(s);
            t.position = {-3.0f + static_cast<float>(i) * 1.5f, 0.5f, row ? -2.6f : -1.2f};
            auto& m = reg.emplace<MeshRendererComponent>(s);
            m.mesh = PrimitiveMesh::Sphere;
            m.color = row ? math::Color{1.0f, 0.78f, 0.34f, 1.0f} : math::Color{0.85f, 0.12f, 0.10f, 1.0f};
            m.metallic = row ? 1.0f : 0.0f;
            m.roughness = 0.05f + 0.225f * static_cast<float>(i);
            reg.emplace<ColliderComponent>(s).shape = ColliderShape3D::Sphere;
        }
    }
    // Imported glTF models and .agrmat materials.
    struct Prop {
        const char* name;
        const char* model;
        Vec3 position;
        float yaw, scale;
    };
    const Prop props[] = {{"Crate", "assets://3d/crate.glb", {4.4f, 0.0f, 0.2f}, 20.0f, 1.0f},
                          {"Crate", "assets://3d/crate.glb", {5.1f, 0.0f, 1.5f}, -35.0f, 0.8f},
                          {"Crate", "assets://3d/crate.glb", {4.5f, 1.0f, 0.3f}, 60.0f, 0.7f},
                          {"Rock", "assets://3d/rock.glb", {-4.6f, 0.0f, 0.6f}, 0.0f, 1.1f},
                          {"Rock", "assets://3d/rock.glb", {-5.4f, 0.0f, -0.9f}, 90.0f, 0.7f}};
    const auto propsRoot = scene.createEntity("Props");
    for (const Prop& p : props) {
        const auto e = scene.createEntity(p.name, propsRoot);
        auto& t = reg.get<TransformComponent>(e);
        t.position = p.position;
        t.rotation = Quat::fromAxisAngle(Vec3::up(), math::radians(p.yaw));
        t.scale = Vec3{p.scale};
        reg.emplace<MeshRendererComponent>(e).model = p.model;
        auto& c = reg.emplace<ColliderComponent>(e);
        if (p.name[0] == 'C') {  // crates fall and get pushed; their model has its origin at the bottom
            c.offset = {0.0f, 0.5f, 0.0f};
            c.density = 300.0f;  // wood
            reg.emplace<RigidBodyComponent>(e);
        } else {                 // rocks: static, approximated by a box
            c.size = {1.6f, 0.8f, 1.3f};
            c.offset = {0.0f, 0.4f, 0.0f};
        }
    }
    const char* materials[] = {"gold", "copper", "plastic_red", "rubber", "ceramic"};
    const auto shelf = scene.createEntity("Materials");
    for (int i = 0; i < 5; ++i) {
        const auto e = scene.createEntity(materials[i], shelf);
        auto& t = reg.get<TransformComponent>(e);
        t.position = {-2.4f + static_cast<float>(i) * 1.2f, 0.4f, 0.6f};
        t.scale = Vec3{0.8f};
        auto& m = reg.emplace<MeshRendererComponent>(e);
        m.mesh = i % 2 ? PrimitiveMesh::Cube : PrimitiveMesh::Sphere;
        m.material = std::string("assets://3d/") + materials[i] + ".agrmat";
        reg.emplace<ColliderComponent>(e).shape = i % 2 ? ColliderShape3D::Box : ColliderShape3D::Sphere;
        if (i == 1) {  // the copper cube turns (animation) and pushes the balls (kinematic body)
            reg.emplace<AnimatorComponent>(e).clip = "assets://animations/spin.agranim";
            reg.emplace<RigidBodyComponent>(e).type = BodyType3D::Kinematic;
        }
    }

    // Local lights: warm lamp over the crates, cool spot on the spheres, neon bar.
    const auto lamp = scene.createEntity("Lamp");
    reg.get<TransformComponent>(lamp).position = {3.6f, 2.0f, 1.6f};
    auto& pl = reg.emplace<PointLightComponent>(lamp);
    pl.color = {1.0f, 0.72f, 0.4f, 1.0f};
    pl.intensity = 6.0f;
    pl.range = 6.0f;
    const auto spot = scene.createEntity("Spot");
    reg.get<TransformComponent>(spot).position = {0.0f, 4.0f, -1.9f};
    reg.get<TransformComponent>(spot).rotation = Quat::fromEuler({math::radians(-90.0f), 0.0f, 0.0f});
    auto& sl = reg.emplace<SpotLightComponent>(spot);
    sl.color = {0.55f, 0.75f, 1.0f, 1.0f};
    sl.intensity = 14.0f;
    sl.range = 8.0f;
    sl.innerAngle = math::radians(25.0f);
    sl.outerAngle = math::radians(40.0f);
    reg.emplace<AnimatorComponent>(lamp).clip = "assets://animations/lamp_flicker.agranim";

    // Animated character (skinned glTF): the hierarchy instantiateModel creates for sprout.glb,
    // its state machine (Idle / Dance) and a script that makes it dance when a ball hits it.
    const auto sproutRoot = scene.createEntity("Sprout");
    {
        auto& t = reg.get<TransformComponent>(sproutRoot);
        t.position = {3.3f, 0.0f, 2.8f};
        t.scale = Vec3{1.3f};
        reg.emplace<AnimatorComponent>(sproutRoot).controller = "assets://animations/sprout.agrctrl";
        reg.emplace<ScriptComponent>(sproutRoot).script = "assets://scripts/sprout_dance.lua";
        auto& c = reg.emplace<ColliderComponent>(sproutRoot);  // static: no RigidBody
        c.size = {0.6f, 1.5f, 0.6f};
        c.offset = {0.0f, 0.75f, 0.0f};
        const auto node = scene.createEntity("Sprout", sproutRoot);
        const auto body = scene.createEntity("Body", node);
        auto& mr = reg.emplace<MeshRendererComponent>(body);
        mr.model = "assets://3d/sprout.glb";
        mr.meshIndex = 0;
        mr.skin = 0;
        ecs::Entity parent = node;
        bool first = true;
        for (const char* joint : {"Root", "Stem1", "Stem2", "Top"}) {
            parent = scene.createEntity(joint, parent);
            reg.get<TransformComponent>(parent).position = {0.0f, first ? 0.0f : 0.35f, 0.0f};
            first = false;
        }
    }
    const auto neon = scene.createEntity("Neon");
    auto& nt = reg.get<TransformComponent>(neon);
    nt.position = {0.0f, 0.05f, 2.2f};
    nt.scale = {4.0f, 0.1f, 0.1f};
    {
        auto& nm = reg.emplace<MeshRendererComponent>(neon);
        nm.material = "assets://3d/neon.agrmat";
        nm.castShadows = false;
    }

    // Ambience, and the HUD: ball counter, launch speed slider (calls the camera script).
    audioSource(scene, "Ambience", "assets://audio/ambient.wav", AudioBus::Ambient, 0.35f);
    const auto hud = uiCanvas(scene);
    const auto panel = uiPanel(scene, "Counter Panel", hud, {0.0f, 0.0f}, {30.0f, 30.0f}, {760.0f, 80.0f});
    uiLabel(scene, "Counter", panel, "Balles : 0 / 25", 36.0f, {1.0f, 1.0f, 1.0f, 1.0f});
    const auto hint = uiElement(scene, "Hint", hud, {1.0f, 0.0f}, {-30.0f, 30.0f}, {700.0f, 80.0f});
    {
        auto& t = reg.emplace<UITextComponent>(hint);
        t.text = "Espace, clic ou toucher : lancer une balle";
        t.localizationKey = "hud.hint.launch";  // assets://localization.csv
        t.fontSize = 30.0f;
        t.hAlign = UIHAlign::Right;
        t.color = {1.0f, 1.0f, 1.0f, 0.8f};
    }
    const auto powerPanel = uiPanel(scene, "Power Panel", hud, {0.5f, 1.0f}, {0.0f, -30.0f}, {640.0f, 120.0f});
    const auto title = uiElement(scene, "Power Label", powerPanel, {0.5f, 0.0f}, {0.0f, 8.0f}, {600.0f, 44.0f});
    reg.emplace<UITextComponent>(title).text = "Vitesse de lancer";
    reg.get<UITextComponent>(title).localizationKey = "hud.speed";
    reg.get<UITextComponent>(title).fontSize = 30.0f;
    const auto power = uiElement(scene, "Power", powerPanel, {0.5f, 1.0f}, {0.0f, -16.0f}, {560.0f, 44.0f});
    auto& slider = reg.emplace<UISliderComponent>(power);
    slider.minValue = 6.0f;
    slider.maxValue = 30.0f;
    slider.value = 14.0f;
    slider.target = cam;
    slider.message = "setPower";
}

} // namespace

void buildSceneTemplate(Scene& scene, SceneTemplate tmpl) {
    registerRenderComponents();
    registerSpriteAnimatorComponent();
    registerPhysics2DComponents();
    registerPhysics3DComponents();
    registerScriptComponent();
    registerAudioComponents();
    registerUIComponents();
    registerAnimationComponents();
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
        case SceneTemplate::Showcase3D: buildShowcase3D(scene); break;
    }
    scene.updateTransforms();
}

} // namespace agr::editor
