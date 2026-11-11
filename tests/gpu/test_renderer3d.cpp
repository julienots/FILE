// 3D rendering on every backend: shadows, point / spot lights, automatic
// instancing, imported models with .agrmat materials, sky, fog, alpha modes.
#include <filesystem>

#include "GpuFixture.hpp"
#include "agr/assets/AssetManager.hpp"
#include "agr/core/Uuid.hpp"
#include "agr/render/ModelLibrary.hpp"
#include "agr/assets/Image.hpp"
#include "agr/render/RenderComponents.hpp"
#include "agr/scene/Terrain.hpp"
#include "agr/twod/Particles2D.hpp"
#include "agr/render/Renderer.hpp"

using namespace agr;
using namespace agr::test;
using math::Vec3;
using math::Vec4;

namespace {

struct Render3DFixture {
    std::filesystem::path root = std::filesystem::temp_directory_path() / ("agr_gpu3d_" + Uuid::generate().toString());
    VirtualFileSystem vfs;
    JobSystem jobs{1};
    AssetManager assets{vfs, jobs, gpu().device.get()};
    ModelLibrary models{vfs, jobs, gpu().device.get()};
    MaterialLibrary materials{vfs};
    EnvironmentLibrary environments{vfs, jobs, gpu().device.get()};
    Renderer renderer{*gpu().device, assets};
    Scene scene;

    Render3DFixture() {
        std::filesystem::create_directories(root);
        vfs.mount("assets", std::make_unique<NativeFileProvider>(root.string(), false));
        registerRenderComponents();
        REQUIRE(renderer.initialize());
        GraphicsProfile p = graphicsProfile(GraphicsTier::High);
        p.renderScale = 1.0f;
        p.msaaSamples = 1;  // pixel-exact checks
        renderer.setProfile(p);
        renderer.setModels(&models);
        renderer.setMaterials(&materials);
        renderer.setEnvironments(&environments);
    }
    ~Render3DFixture() { std::filesystem::remove_all(root); }

    /// Perspective camera at `eye` looking at `target`, black background.
    void camera(Vec3 eye, Vec3 target) {
        const auto cam = scene.createEntity("Camera");
        auto& c = scene.registry().emplace<CameraComponent>(cam);
        c.clearColor = {0, 0, 0, 1};
        c.fieldOfView = math::radians(60.0f);
        auto& t = scene.registry().get<TransformComponent>(cam);
        Vec3 s;
        math::decompose(math::inverse(math::Mat4::lookAt(eye, target, std::abs(eye.x - target.x) + std::abs(eye.z - target.z) < 1e-3f
                                                                      ? Vec3{0, 0, -1}
                                                                      : Vec3{0, 1, 0})),
                        t.position, t.rotation, s);
    }
    ecs::Entity mesh(PrimitiveMesh type, Vec3 pos, Vec3 scale = Vec3{1.0f}, math::Color color = {1, 1, 1, 1}) {
        const auto e = scene.createEntity("Mesh");
        auto& t = scene.registry().get<TransformComponent>(e);
        t.position = pos;
        t.scale = scale;
        auto& m = scene.registry().emplace<MeshRendererComponent>(e);
        m.mesh = type;
        m.color = color;
        return e;
    }
    /// Environment without ambient light (only the lights under test).
    EnvironmentComponent& darkEnvironment() {
        const auto e = scene.createEntity("Env");
        auto& env = scene.registry().emplace<EnvironmentComponent>(e);
        env.ambientIntensity = 0.0f;
        return env;
    }
    ecs::Entity sun(Vec3 eulerDegrees, float intensity = 1.0f) {
        const auto e = scene.createEntity("Sun");
        auto& l = scene.registry().emplace<DirectionalLightComponent>(e);
        l.color = {1, 1, 1, 1};
        l.intensity = intensity;
        scene.registry().get<TransformComponent>(e).rotation =
            math::Quat::fromEuler({math::radians(eulerDegrees.x), math::radians(eulerDegrees.y), math::radians(eulerDegrees.z)});
        return e;
    }
    void frame() {
        scene.updateTransforms();
        models.update();
        assets.update();
        environments.update();
        renderer.render(scene, kWidth, kHeight);
    }
    void settle() {
        frame();
        models.waitAll();
        assets.waitAll();
        environments.waitAll();
        frame();
        assets.waitAll();
        frame();
    }
    /// Pixel position of a world point for the primary camera.
    Rgba at(Vec3 p) {
        const auto rv = RenderView::fromPrimaryCamera(scene);
        REQUIRE(rv);
        const math::Mat4 vp = rv->projectionMatrix(1.0f, renderer.glClipSpace()) * rv->view;
        const Vec4 c = vp * Vec4(p, 1.0f);
        const float x = (c.x / c.w * 0.5f + 0.5f) * static_cast<float>(kWidth);
        const float y = (0.5f - c.y / c.w * 0.5f) * static_cast<float>(kHeight);
        REQUIRE((x >= 0 && x < kWidth && y >= 0 && y < kHeight));
        return pixel(*gpu().device, static_cast<u32>(x), static_cast<u32>(y));
    }
};

int luminance(Rgba p) { return int(p.r) + int(p.g) + int(p.b); }

}  // namespace

TEST_SUITE("gpu.renderer3d") {

TEST_CASE("directional shadows: casters darken receivers, can be disabled") {
    Render3DFixture f;
    f.camera({0, 9, 3}, {0, 0, -0.5f});
    f.darkEnvironment().ambientIntensity = 0.2f;
    f.sun({-45, 0, 0});  // travels down and toward -Z: the shadow falls behind the box
    f.mesh(PrimitiveMesh::Plane, {0, 0, 0}, Vec3{20.0f});
    const auto box = f.mesh(PrimitiveMesh::Cube, {0, 0.5f, 0});
    f.frame();
    CHECK(f.renderer.stats().shadowCasters >= 2);
    REQUIRE(f.renderer.shadowMap());
    const int shadowed = luminance(f.at({0, 0, -1.3f}));
    const int lit = luminance(f.at({0, 0, 1.5f}));
    CHECK_MESSAGE(shadowed * 2 < lit, "shadowed " << shadowed << " lit " << lit);
    CHECK(shadowed > 0);  // ambient still lights the shadow

    f.scene.registry().get<MeshRendererComponent>(box).castShadows = false;
    f.frame();
    CHECK(std::abs(luminance(f.at({0, 0, -1.3f})) - lit) < 12);

    f.scene.registry().get<MeshRendererComponent>(box).castShadows = true;
    GraphicsProfile p = f.renderer.profile();
    p.shadowMapSize = 0;  // ultra-mobile: no shadows
    f.renderer.setProfile(p);
    f.frame();
    CHECK_FALSE(f.renderer.shadowMap());
    CHECK(std::abs(luminance(f.at({0, 0, -1.3f})) - lit) < 12);
    CHECK(gpu().device->validationErrorCount() == 0);
}

TEST_CASE("cascaded shadows: sharp near the camera, far casters, cascade count per profile") {
    Render3DFixture f;
    f.camera({0, 5, 2}, {0, 0, -10});
    f.darkEnvironment().ambientIntensity = 0.2f;
    const auto sunE = f.sun({-50, 90, 0});  // travels down and toward -X: shadows fall on the -X side
    f.scene.registry().get<DirectionalLightComponent>(sunE).shadowDistance = 60.0f;
    f.mesh(PrimitiveMesh::Plane, {0, 0, -30}, Vec3{200.0f});
    f.mesh(PrimitiveMesh::Cube, {0.3f, 0.5f, -3.0f}, {0.15f, 0.15f, 1.2f});  // thin slab near the camera
    f.mesh(PrimitiveMesh::Cube, {5.0f, 2.0f, -30.0f}, {2.0f, 4.0f, 12.0f});  // big block far away
    const Vec3 nearShadow{-0.12f, 0.0f, -3.0f}, nearLit{-0.9f, 0.0f, -3.0f};
    const Vec3 farShadow{2.3f, 0.0f, -30.0f}, farLit{-2.5f, 0.0f, -30.0f};
    auto measure = [&](u32 cascades, u32 mapSize) {
        GraphicsProfile p = f.renderer.profile();
        p.shadowCascades = cascades;
        p.shadowMapSize = mapSize;
        f.renderer.setProfile(p);
        f.frame();
        CHECK(f.renderer.stats().shadowCascades == cascades);
        struct R {
            int nearShadow, nearLit, farShadow, farLit;
        };
        const R r{luminance(f.at(nearShadow)), luminance(f.at(nearLit)), luminance(f.at(farShadow)), luminance(f.at(farLit))};
        MESSAGE(cascades << " cascade(s), " << mapSize << ": near " << r.nearShadow << "/" << r.nearLit << ", far " << r.farShadow << "/" << r.farLit);
        return r;
    };
    // A single 2048 map stretched over 60 m blurs the thin caster away; four 512 cascades (a
    // 1024 atlas, a quarter of the memory) resolve it, and the far block still casts its shadow.
    const auto single = measure(1, 2048);
    const auto four = measure(4, 512);
    CHECK(four.nearShadow * 2 < four.nearLit);
    CHECK(four.nearShadow + 150 < single.nearShadow);
    CHECK(single.farShadow * 2 < single.farLit);
    CHECK(four.farShadow * 2 < four.farLit);
    for (u32 n : {2u, 3u}) {
        const auto r = measure(n, 1024);
        CHECK(r.nearShadow * 2 < r.nearLit);
        CHECK(r.farShadow * 2 < r.farLit);
    }
    // Beyond the shadow distance nothing is shadowed.
    f.scene.registry().get<DirectionalLightComponent>(sunE).shadowDistance = 10.0f;
    const auto shortRange = measure(3, 1024);
    CHECK(shortRange.nearShadow * 2 < shortRange.nearLit);
    CHECK(std::abs(shortRange.farShadow - shortRange.farLit) < 12);
    CHECK(gpu().device->validationErrorCount() == 0);
}

TEST_CASE("image sky: background, diffuse and reflected light, rotation, loading fallback") {
    Render3DFixture f;
    // Equirect test sky: upper hemisphere red (HDR 2.0), lower blue; +X quarter of the horizon band green.
    const u32 w = 128, h = 64;
    std::vector<float> rgb(w * h * 3);
    for (u32 y = 0; y < h; ++y)
        for (u32 x = 0; x < w; ++x) {
            const Vec3 d = equirectDirection((x + 0.5f) / w, (y + 0.5f) / h);
            Vec3 c = d.y > 0.0f ? Vec3{2.0f, 0.0f, 0.0f} : Vec3{0.0f, 0.0f, 0.5f};
            if (std::abs(d.y) < 0.35f && d.x > 0.7f) c = {0.0f, 1.0f, 0.0f};
            rgb[(y * w + x) * 3] = c.x, rgb[(y * w + x) * 3 + 1] = c.y, rgb[(y * w + x) * 3 + 2] = c.z;
        }
    auto hdr = encodeHdr(w, h, rgb);
    REQUIRE(hdr);
    REQUIRE(f.vfs.writeBytes("assets://sky.hdr", hdr.value().data(), hdr.value().size()));

    f.camera({0, 0, 0}, {0, 0.2f, -1});
    auto& env = f.darkEnvironment();
    env.sky = SkyMode::Image;
    env.skyTexture = "assets://sky.hdr";
    env.ambientIntensity = 1.0f;
    MaterialData rough;
    rough.baseColor = {1, 1, 1, 1};
    rough.roughness = 1.0f;
    REQUIRE(f.materials.save("assets://rough.agrmat", rough));
    const auto ball = f.mesh(PrimitiveMesh::Sphere, {0, 0.2f, -4}, Vec3{1.6f});
    f.scene.registry().get<MeshRendererComponent>(ball).material = "assets://rough.agrmat";

    f.frame();  // first frame: the image is still loading, the gradient stands in
    CHECK(f.renderer.stats().environmentLoading);
    CHECK_FALSE(f.renderer.stats().imageSky);
    f.settle();
    REQUIRE(f.renderer.stats().imageSky);
    CHECK_FALSE(f.renderer.stats().environmentLoading);
    auto& d = *gpu().device;
    const Rgba skyUp = pixel(d, kWidth / 2, 2), skyDown = pixel(d, kWidth / 2, kHeight - 2);
    CHECK_MESSAGE((skyUp.r > 200 && skyUp.g < 40 && skyUp.b < 40), skyUp);
    CHECK_MESSAGE((skyDown.b > 100 && skyDown.r < 40), skyDown);
    // Diffuse (SH irradiance): the top of the rough ball is lit red, the bottom blue.
    const Rgba top = f.at({0, 0.2f + 0.65f, -4 + 0.3f}), bottom = f.at({0, 0.2f - 0.65f, -4 + 0.3f});
    CHECK_MESSAGE((top.r > 120 && top.r > top.b * 2), top);
    CHECK_MESSAGE((bottom.b > bottom.r), bottom);

    // A mirror ball reflects the sky: its right side shows the green band of +X.
    MaterialData mirror;
    mirror.baseColor = {1, 1, 1, 1};
    mirror.metallic = 1.0f;
    mirror.roughness = 0.05f;
    REQUIRE(f.materials.save("assets://mirror.agrmat", mirror));
    f.scene.registry().get<MeshRendererComponent>(ball).material = "assets://mirror.agrmat";
    f.frame();
    const Rgba right = f.at({0.62f, 0.2f, -4 + 0.5f});
    CHECK_MESSAGE((right.g > 100 && right.g > right.r), right);
    // Rotated by half a turn, +X now shows what was at -X (no green).
    f.scene.registry().get<EnvironmentComponent>(f.scene.findByName("Env")).skyRotation = math::radians(180.0f);
    f.frame();
    const Rgba rotated = f.at({0.62f, 0.2f, -4 + 0.5f});
    CHECK_MESSAGE(rotated.g < 60, rotated);
    // Intensity scales background and light.
    f.scene.registry().get<EnvironmentComponent>(f.scene.findByName("Env")).skyIntensity = 0.1f;
    f.frame();
    CHECK(pixel(d, kWidth / 2, 2).r < skyUp.r - 60);

    // A missing image falls back to the gradient without errors.
    f.scene.registry().get<EnvironmentComponent>(f.scene.findByName("Env")).skyTexture = "assets://none.hdr";
    f.settle();
    CHECK_FALSE(f.renderer.stats().imageSky);
    const EnvironmentAsset* missing = f.environments.get("assets://none.hdr");
    REQUIRE(missing);
    CHECK(missing->state == AssetState::Failed);
    CHECK(d.validationErrorCount() == 0);
}

TEST_CASE("terrain: heights lit by the sun, layers blended by the splat map, updates after sculpting") {
    Render3DFixture f;
    auto solid = [&](const char* uri, u8 r, u8 g, u8 b) {
        std::vector<u8> px(4 * 4 * 4);
        for (usize i = 0; i < px.size(); i += 4) px[i] = r, px[i + 1] = g, px[i + 2] = b, px[i + 3] = 255;
        auto png = encodePng(4, 4, px);
        REQUIRE(png);
        REQUIRE(f.vfs.writeBytes(uri, png.value().data(), png.value().size()));
    };
    solid("assets://red.png", 220, 40, 40);
    solid("assets://blue.png", 40, 60, 230);
    f.camera({0, 30, 0.01f}, {0, 0, 0});
    f.darkEnvironment().ambientIntensity = 0.3f;
    f.sun({-90, 0, 0});  // straight down
    const auto e = f.scene.createEntity("Terrain");
    auto& t = f.scene.registry().emplace<TerrainComponent>(e);
    t.size = 32.0f;
    t.resolution = 33;
    t.layer0 = "assets://red.png";
    t.layer1 = "assets://blue.png";
    t.tiling = {1, 1, 1, 1};
    terrain::ensureData(t);
    // Paint layer 1 (blue) on the +X half.
    for (u32 z = 0; z < 33; ++z)
        for (u32 x = 18; x < 33; ++x) t.splat[z * 33 + x] = 0x0000FF00;  // g = 255
    f.settle();
    CHECK(f.renderer.stats().terrains == 1);
    const Rgba left = f.at({-8, 0, 0}), right = f.at({8, 0, 0});
    CHECK_MESSAGE((left.r > left.b * 2 && left.r > 120), left);
    CHECK_MESSAGE((right.b > right.r * 2 && right.b > 120), right);

    // A hill: the flat top stays as bright, its slopes are darker (sun from above).
    terrain::BrushSettings raise;
    raise.radius = 8.0f;
    raise.strength = 6.0f;
    REQUIRE(terrain::applyBrush(t, {-8, 8}, raise));
    f.frame();
    const Rgba slope = f.at({-8 + 4.5f, terrain::heightAt(t, -3.5f, 8), 8});
    const Rgba flat = f.at({-8, 0, -8});
    CHECK_MESSAGE(luminance(slope) + 30 < luminance(flat), slope << " vs " << flat);
    CHECK(gpu().device->validationErrorCount() == 0);
}

TEST_CASE("water: transparent body over the ground, sky reflection at grazing angles, moving waves") {
    Render3DFixture f;
    auto& env = f.darkEnvironment();
    env.sky = SkyMode::Gradient;
    env.ambientIntensity = 0.5f;
    env.skyColor = {0.9f, 0.9f, 0.9f, 1.0f};
    env.horizonColor = {0.9f, 0.9f, 0.9f, 1.0f};
    f.sun({-60, 30, 0});
    f.camera({0, 12, 0.01f}, {0, 0, 0});
    const auto ground = f.mesh(PrimitiveMesh::Plane, {0, -1, 0}, Vec3{30.0f}, {1, 0, 0, 1});  // red floor below the water
    (void)ground;
    f.frame();
    const Rgba dry = f.at({0, -1, 0});
    const auto w = f.scene.createEntity("Water");
    auto& wc = f.scene.registry().emplace<WaterComponent>(w);
    wc.size = {30, 30};
    wc.shallowColor = {0.0f, 0.4f, 1.0f, 1.0f};
    wc.opacity = 0.7f;
    wc.waveHeight = 0.3f;
    f.renderer.setTime(0.0);
    f.frame();
    CHECK(f.renderer.stats().waterSurfaces == 1);
    const Rgba wet = f.at({0, 0, 0});
    CHECK_MESSAGE((wet.b > wet.r && dry.r > dry.b * 2), "dry " << dry << " wet " << wet);  // blue body over red
    CHECK(wet.r > 5);  // the floor shows through (transparent)

    // Waves move with time.
    std::vector<u8> a, b;
    REQUIRE(gpu().device->readBackbuffer(0, 0, kWidth, kHeight, a));
    f.renderer.setTime(1.3);
    f.frame();
    REQUIRE(gpu().device->readBackbuffer(0, 0, kWidth, kHeight, b));
    int changed = 0;
    for (usize i = 0; i < a.size(); i += 4) changed += std::abs(int(a[i + 2]) - int(b[i + 2])) > 2;
    CHECK(changed > 20);

    // Grazing view: Fresnel reflects the bright sky.
    f.renderer.setTime(0.0);
    auto& cam = f.scene.registry().get<TransformComponent>(f.scene.findByName("Camera"));
    cam.position = {0, 0.6f, 14};
    cam.rotation = math::Quat::fromAxisAngle({1, 0, 0}, math::radians(-4.0f));
    f.frame();
    const Rgba far = pixel(*gpu().device, kWidth / 2, kHeight / 2 + 4);
    CHECK_MESSAGE(luminance(far) > luminance(wet), far << " vs " << wet);
    CHECK(gpu().device->validationErrorCount() == 0);
}

TEST_CASE("3D particles: camera-facing billboards, additive brighter, hidden behind walls") {
    Render3DFixture f;
    f.camera({0, 0, 6}, {0, 0, 0});
    f.darkEnvironment();
    const auto fx = f.scene.createEntity("Fx");
    auto& em = f.scene.registry().emplace<ParticleEmitter3DComponent>(fx);
    em.rate = 0.0f;
    em.burst = 1;
    em.shape = EmitterShape3D::Point;
    em.speedMin = em.speedMax = 0.0f;
    em.lifetimeMin = em.lifetimeMax = 100.0f;
    em.startSize = em.endSize = 2.0f;
    em.startColor = em.endColor = {0.6f, 0.2f, 0.1f, 1.0f};
    f.scene.updateTransforms();
    simulateParticles3D(f.scene, 0.0f);
    f.settle();
    CHECK(f.renderer.stats().particlesVisible == 1);
    const Rgba alpha = f.at({0, 0, 0});
    CHECK_MESSAGE(alpha.r > 60, alpha);
    // Seen from the side, a billboard still faces the camera.
    const auto camera = f.scene.findByName("Camera");
    auto place = [&](Vec3 p, float yawDegrees) {  // looked up each time: creating entities moves components
        auto& t = f.scene.registry().get<TransformComponent>(camera);
        t.position = p;
        t.rotation = math::Quat::fromAxisAngle({0, 1, 0}, math::radians(yawDegrees));
    };
    place({6, 0, 0}, 90.0f);
    f.frame();
    CHECK(f.at({0, 0, 0}).r > 60);
    // Over a lit wall behind it: additive adds to the wall, alpha covers it.
    const auto wall = f.mesh(PrimitiveMesh::Cube, {-2, 0, 0}, {0.2f, 6, 6}, {0.5f, 0.5f, 0.5f, 1});
    (void)wall;
    f.sun({0, 90, 0});  // travels toward -X: lights the wall's face toward the camera
    f.frame();
    const Rgba covered = f.at({0, 0, 0});
    f.scene.registry().get<ParticleEmitter3DComponent>(fx).additive = true;
    f.frame();
    const Rgba add = f.at({0, 0, 0});
    CHECK_MESSAGE(add.g > covered.g + 20, add << " vs " << covered);  // the wall's light shows through
    // Behind the wall: hidden (depth test against opaque geometry).
    place({-6, 0, 0}, -90.0f);
    f.frame();
    const Rgba hiddenPx = f.at({-2.1f, 0, 0});
    CHECK_MESSAGE(hiddenPx.r < 40, hiddenPx);  // the unlit back of the wall, no particle
    CHECK(gpu().device->validationErrorCount() == 0);
}

TEST_CASE("point and spot lights: range, cone and per-profile limit") {
    Render3DFixture f;
    f.camera({0, 8, 0}, {0, 0, 0});
    f.darkEnvironment();
    f.mesh(PrimitiveMesh::Plane, {0, 0, 0}, Vec3{20.0f});
    const auto bulb = f.scene.createEntity("Bulb");
    f.scene.registry().get<TransformComponent>(bulb).position = {-2, 1, 0};
    auto& point = f.scene.registry().emplace<PointLightComponent>(bulb);
    point.color = {1, 0, 0, 1};
    point.intensity = 4.0f;
    point.range = 2.5f;
    const auto spot = f.scene.createEntity("Spot");
    auto& st = f.scene.registry().get<TransformComponent>(spot);
    st.position = {2, 2, 0};
    st.rotation = math::Quat::fromEuler({math::radians(-90.0f), 0, 0});  // points down
    auto& sp = f.scene.registry().emplace<SpotLightComponent>(spot);
    sp.color = {0, 0, 1, 1};
    sp.intensity = 8.0f;
    sp.range = 6.0f;
    sp.innerAngle = math::radians(10.0f);
    sp.outerAngle = math::radians(20.0f);
    f.frame();
    CHECK(f.renderer.stats().lights == 2);
    const Rgba underBulb = f.at({-2, 0, 0});
    CHECK_MESSAGE(underBulb.r > 120, underBulb);
    CHECK(underBulb.b < 10);
    CHECK_MESSAGE(near(f.at({-2, 0, 2.8f}), 0, 0, 0, 2), f.at({-2, 0, 2.8f}));  // beyond the range
    const Rgba underSpot = f.at({2, 0, 0});
    CHECK_MESSAGE(underSpot.b > 120, underSpot);
    CHECK_MESSAGE(near(f.at({2, 0, 1.5f}), 0, 0, 0, 2), f.at({2, 0, 1.5f}));  // outside the 20 degree cone (tan 20 * 2 = 0.73)

    GraphicsProfile p = f.renderer.profile();
    p.maxLights = 1;  // the nearest light to the camera wins
    f.renderer.setProfile(p);
    f.frame();
    CHECK(f.renderer.stats().lights == 1);
}

TEST_CASE("identical meshes are drawn as one instanced batch") {
    Render3DFixture f;
    f.camera({0, 12, 12}, {0, 0, 0});
    f.sun({-50, 30, 0});
    for (int i = 0; i < 100; ++i)  // per-instance tint: still one material
        f.mesh(PrimitiveMesh::Cube, {static_cast<float>(i % 10) - 4.5f, 0, static_cast<float>(i / 10) - 4.5f}, Vec3{0.5f},
               i % 2 ? math::Color{1, 0, 0, 1} : math::Color{0, 1, 0, 1});
    f.frame();
    CHECK(f.renderer.stats().meshesVisible == 100);
    CHECK(f.renderer.stats().meshBatches == 1);
    const Rgba c = f.at({-4.5f, 0.25f, -4.5f});  // first cube (green)
    CHECK_MESSAGE(c.g > c.r, c);
    // A different built-in material (roughness) splits the batch.
    f.scene.registry().view<MeshRendererComponent>().each([](ecs::Entity e, MeshRendererComponent& m) {
        if (e.index % 3 == 0) m.roughness = 0.9f;
    });
    f.frame();
    CHECK(f.renderer.stats().meshBatches == 2);
}

TEST_CASE("imported model with its material, .agrmat override, unlit") {
    Render3DFixture f;
    REQUIRE(f.vfs.writeText("assets://m/quad.mtl", "newmtl Green\nKd 0 1 0\n"));
    REQUIRE(f.vfs.writeText("assets://m/quad.obj", "mtllib quad.mtl\nv -1 -1 0\nv 1 -1 0\nv 1 1 0\nv -1 1 0\nusemtl Green\nf 1 2 3 4\n"));
    f.camera({0, 0, 3}, {0, 0, 0});
    f.sun({0, 0, 0});  // along -Z, onto the quad
    const ModelAsset* model = f.models.load("assets://m/quad.obj");
    REQUIRE(model);
    REQUIRE(model->state == AssetState::Ready);
    const ecs::Entity root = instantiateModel(f.scene, *model);
    REQUIRE_FALSE(root.isNull());
    CHECK(f.scene.nameOf(root) == "quad");
    f.settle();
    const Rgba green = f.at({0, 0, 0});
    CHECK_MESSAGE(green.g > 150, green);
    CHECK(green.r < 60);

    MaterialData blue;
    blue.baseColor = {0, 0, 1, 1};
    blue.unlit = true;
    REQUIRE(f.materials.save("assets://m/blue.agrmat", blue));
    f.scene.registry().view<MeshRendererComponent>().each([](ecs::Entity, MeshRendererComponent& m) { m.material = "assets://m/blue.agrmat"; });
    f.frame();
    CHECK_MESSAGE(near(f.at({0, 0, 0}), 0, 0, 255, 2), f.at({0, 0, 0}));
    // A missing model is skipped (not drawn, no crash).
    f.scene.registry().view<MeshRendererComponent>().each([](ecs::Entity, MeshRendererComponent& m) { m.model = "assets://m/none.glb"; });
    f.settle();
    CHECK(near(f.at({0, 0, 0}), 0, 0, 0));
}

TEST_CASE("gradient sky, fog and exposure") {
    Render3DFixture f;
    f.camera({0, 1, 0}, {0, 1, -10});  // looking at the horizon
    auto& env = f.darkEnvironment();
    env.sky = SkyMode::Gradient;
    env.skyColor = {0, 0, 1, 1};
    env.horizonColor = {0, 1, 0, 1};
    env.groundColor = {1, 0, 0, 1};
    env.sunDisc = false;
    f.frame();
    const Rgba top = pixel(*gpu().device, 32, 2), bottom = pixel(*gpu().device, 32, 62);
    CHECK_MESSAGE((top.b > top.r && top.b > 40), top);
    CHECK_MESSAGE((bottom.r > bottom.b && bottom.r > 40), bottom);

    // Fog: a white cube far away takes the fog color.
    env.sky = SkyMode::CameraColor;
    env.fog = true;
    env.fogColor = {1, 0, 1, 1};
    env.fogStart = 1.0f;
    env.fogEnd = 5.0f;
    f.sun({0, 0, 0});
    f.mesh(PrimitiveMesh::Cube, {0, 1, -20}, Vec3{4.0f});
    f.frame();
    const Rgba fogged = pixel(*gpu().device, 32, 32);
    CHECK_MESSAGE((fogged.r > 150 && fogged.b > 150 && fogged.g < 30), fogged);
    // Exposure scales the image before tone mapping.
    env.fog = false;
    f.frame();
    const int normal = luminance(pixel(*gpu().device, 32, 32));
    env.exposure = 0.05f;
    f.frame();
    const int dim = luminance(pixel(*gpu().device, 32, 32));
    CHECK_MESSAGE(dim * 2 < normal, "exposure 1: " << normal << ", exposure 0.05: " << dim);
}

TEST_CASE("alpha mask cuts, alpha blend mixes") {
    Render3DFixture f;
    // 2x1 texture: left opaque white, right transparent.
    Image img;
    img.width = 2;
    img.height = 1;
    img.pixels = {255, 255, 255, 255, 255, 255, 255, 0};
    auto png = encodePng(2, 1, img.pixels);
    REQUIRE(png);
    REQUIRE(f.vfs.writeBytes("assets://half.png", png.value().data(), png.value().size()));
    MaterialData mask;
    mask.baseColorTexture = "assets://half.png";
    mask.alphaMode = AlphaMode::Mask;
    mask.unlit = true;
    REQUIRE(f.materials.save("assets://mask.agrmat", mask));
    f.camera({0, 0, 3}, {0, 0, 0});
    const auto quad = f.mesh(PrimitiveMesh::Quad, {0, 0, 0}, Vec3{3.0f});
    f.scene.registry().get<MeshRendererComponent>(quad).material = "assets://mask.agrmat";
    // Pixel-art filtering is not exposed on materials: sample well inside each half.
    f.settle();
    CHECK(near(f.at({-0.9f, 0, 0}), 255, 255, 255, 3));
    CHECK(near(f.at({0.9f, 0, 0}), 0, 0, 0, 3));

    MaterialData glass;
    glass.baseColor = {1, 1, 1, 0.5f};
    glass.alphaMode = AlphaMode::Blend;
    glass.unlit = true;
    REQUIRE(f.materials.save("assets://glass.agrmat", glass));
    f.scene.registry().get<MeshRendererComponent>(quad).material = "assets://glass.agrmat";
    f.frame();
    CHECK_MESSAGE(near(f.at({0, 0, 0}), 128, 128, 128, 4), f.at({0, 0, 0}));
}

}  // TEST_SUITE

TEST_CASE("skinned model: the mesh follows its joints (GPU skinning)") {
    Render3DFixture f;
    std::filesystem::create_directories(f.root / "3d");
    std::filesystem::copy_file(std::filesystem::path(AGR_TEST_TEMPLATE_ASSETS) / "3d" / "sprout.glb", f.root / "3d" / "sprout.glb");
    MaterialData orange;
    orange.baseColor = {1.0f, 0.5f, 0.0f, 1.0f};
    orange.unlit = true;
    REQUIRE(f.materials.save("assets://orange.agrmat", orange));
    f.camera({0, 0.4f, 6}, {0, 0.4f, 0});
    const ModelAsset* model = f.models.load("assets://3d/sprout.glb");
    REQUIRE(model);
    REQUIRE(model->state == AssetState::Ready);
    REQUIRE(model->skins.size() == 1);
    CHECK(model->animationNames.size() == 2);
    const ecs::Entity root = instantiateModel(f.scene, *model);
    f.scene.registry().view<MeshRendererComponent>().each([](ecs::Entity, MeshRendererComponent& m) {
        CHECK(m.skin == 0);
        m.material = "assets://orange.agrmat";
    });
    f.settle();
    const Vec3 head{0.0f, 1.23f, 0.0f};
    CHECK_MESSAGE(near(f.at(head), 255, 128, 0, 6), f.at(head));       // bind pose: the head is up
    CHECK_MESSAGE(near(f.at({-1.23f, 0.0f, 0.0f}), 0, 0, 0), f.at({-1.23f, 0.0f, 0.0f}));

    // Bend the whole stem: rotating the root joint by 90 degrees moves the head to the left.
    const ecs::Entity joint = f.scene.findByName("Root");
    REQUIRE_FALSE(joint.isNull());
    CHECK(f.scene.parent(f.scene.parent(joint)) == root);
    f.scene.registry().get<TransformComponent>(joint).rotation = math::Quat::fromAxisAngle({0, 0, 1}, math::radians(90.0f));
    f.frame();
    CHECK_MESSAGE(near(f.at({-1.23f, 0.0f, 0.0f}), 255, 128, 0, 6), f.at({-1.23f, 0.0f, 0.0f}));
    CHECK_MESSAGE(near(f.at(head), 0, 0, 0), f.at(head));
    CHECK(gpu().device->validationErrorCount() == 0);
}

TEST_CASE("post-processing: HDR bloom, color grading, vignette, FXAA, UI untouched") {
    Render3DFixture f;
    f.camera({0, 0, 5}, {0, 0, 0});
    // A very bright (emissive) small sphere on black.
    MaterialData glow;
    glow.baseColor = {0, 0, 0, 1};
    glow.emissive = {1.0f, 0.6f, 0.2f, 1.0f};
    glow.emissiveIntensity = 8.0f;
    REQUIRE(f.materials.save("assets://glow.agrmat", glow));
    const auto ball = f.mesh(PrimitiveMesh::Sphere, {0, 0, 0}, Vec3{0.8f});
    f.scene.registry().get<MeshRendererComponent>(ball).material = "assets://glow.agrmat";
    f.settle();
    const Vec3 edgePoint{0.75f, 0.0f, 0.0f};  // just outside the sphere's silhouette
    CHECK_MESSAGE(near(f.at(edgePoint), 0, 0, 0, 2), f.at(edgePoint));
    CHECK_FALSE(f.renderer.stats().postProcessing);

    const auto cam = f.scene.findByName("Camera");
    auto& post = f.scene.registry().emplace<PostProcessComponent>(cam);
    post.vignette = 0.0f;
    post.fxaa = false;
    f.frame();
    CHECK(f.renderer.stats().postProcessing);
    CHECK(f.renderer.stats().hdr == gpu().device->info().halfFloatRenderTargets);
    CHECK(f.renderer.stats().bloomLevels >= 3);
    const Rgba halo = f.at(edgePoint);
    CHECK_MESSAGE(halo.r > 25, halo);  // the glow spreads past the edge
    CHECK(halo.r >= halo.b);
    CHECK_MESSAGE(f.at({0, 0, 0}).r > 240, f.at({0, 0, 0}));  // the core stays saturated

    // Grading: no saturation = gray; a red filter keeps only red.
    post.bloom = false;
    post.saturation = 0.0f;
    f.frame();
    const Rgba gray = f.at({0, 0, 0});
    CHECK_MESSAGE((std::abs(gray.r - gray.g) < 3 && std::abs(gray.g - gray.b) < 3 && gray.r > 100), gray);
    post.saturation = 1.0f;
    post.colorFilter = {1, 0, 0, 1};
    f.frame();
    const Rgba red = f.at({0, 0, 0});
    CHECK_MESSAGE((red.r > 200 && red.g < 5 && red.b < 5), red);

    // Vignette darkens the corners of a uniform background.
    post.colorFilter = {1, 1, 1, 1};
    f.scene.registry().get<CameraComponent>(cam).clearColor = {0.5f, 0.5f, 0.5f, 1.0f};
    post.vignette = 1.0f;
    post.filmicToneMapping = false;
    f.frame();
    auto& d = *gpu().device;
    CHECK(pixel(d, 1, 1).r + 30 < pixel(d, kWidth / 2, 2).r);

    // FXAA blends the jagged edge of a white quad on black (MSAA is off in this fixture).
    post.vignette = 0.0f;
    f.scene.registry().get<CameraComponent>(cam).clearColor = {0, 0, 0, 1};
    f.scene.registry().get<MeshRendererComponent>(ball).material.clear();
    f.scene.registry().get<MeshRendererComponent>(ball).color = {1, 1, 1, 1};
    MaterialData white;
    white.baseColor = {1, 1, 1, 1};
    white.unlit = true;
    REQUIRE(f.materials.save("assets://white.agrmat", white));
    f.scene.registry().get<MeshRendererComponent>(ball).material = "assets://white.agrmat";
    auto countMid = [&] {
        std::vector<u8> px;
        REQUIRE(d.readBackbuffer(0, 0, kWidth, kHeight, px));
        int mid = 0;
        for (usize i = 0; i < px.size(); i += 4)
            if (px[i] > 30 && px[i] < 225) ++mid;
        return mid;
    };
    post.fxaa = false;
    f.frame();
    const int hard = countMid();
    post.fxaa = true;
    f.frame();
    const int smooth = countMid();
    CHECK_MESSAGE(smooth > hard + 10, "edge pixels without FXAA: " << hard << ", with: " << smooth);
    CHECK(d.validationErrorCount() == 0);
}
