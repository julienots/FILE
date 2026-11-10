// 3D rendering on every backend: shadows, point / spot lights, automatic
// instancing, imported models with .agrmat materials, sky, fog, alpha modes.
#include <filesystem>

#include "GpuFixture.hpp"
#include "agr/assets/AssetManager.hpp"
#include "agr/core/Uuid.hpp"
#include "agr/render/ModelLibrary.hpp"
#include "agr/render/RenderComponents.hpp"
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
        renderer.render(scene, kWidth, kHeight);
    }
    void settle() {
        frame();
        models.waitAll();
        assets.waitAll();
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
