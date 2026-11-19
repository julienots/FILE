#include <doctest/doctest.h>

#include <filesystem>

#include "agr/core/Uuid.hpp"
#include "agr/render/GraphicsProfile.hpp"
#include "agr/render/OcclusionBuffer.hpp"
#include "agr/render/Primitives.hpp"
#include "agr/render/RenderComponents.hpp"
#include "agr/render/RenderPlugin.hpp"
#include "agr/runtime/Engine.hpp"
#include "agr/scene/SceneSerializer.hpp"

using namespace agr;
using math::Vec3;

TEST_SUITE("render") {

TEST_CASE("primitives have outward counter-clockwise faces and correct bounds") {
    for (PrimitiveMesh type : {PrimitiveMesh::Cube, PrimitiveMesh::Sphere, PrimitiveMesh::Plane, PrimitiveMesh::Quad}) {
        const MeshData m = makePrimitive(type);
        REQUIRE(m.indices.size() % 3 == 0);
        int checked = 0;
        for (usize i = 0; i < m.indices.size(); i += 3) {
            const auto& a = m.vertices[m.indices[i]];
            const auto& b = m.vertices[m.indices[i + 1]];
            const auto& c = m.vertices[m.indices[i + 2]];
            const Vec3 faceNormal = math::cross(b.position - a.position, c.position - a.position);
            if (math::length(faceNormal) < 1e-7f) continue;  // degenerate sphere pole triangles
            CHECK(math::dot(faceNormal, a.normal + b.normal + c.normal) > 0.0f);
            ++checked;
        }
        CHECK(checked > 0);
        for (const auto& v : m.vertices) CHECK(math::nearlyEqual(math::length(v.normal), 1.0f, 1e-3f));
        CHECK(m.bounds.max.x == doctest::Approx(0.5f));
        CHECK(m.bounds.min.x == doctest::Approx(-0.5f));
    }
    CHECK(makeCube().vertices.size() == 24);
    CHECK(makeCube().indices.size() == 36);
    CHECK(makePlane().bounds.extents().y == doctest::Approx(0.0f));
}

TEST_CASE("graphics profiles") {
    CHECK(parseGraphicsTier("ultra-mobile").value() == GraphicsTier::UltraMobile);
    CHECK_FALSE(parseGraphicsTier("extreme"));
    // Profiles are ordered: cheaper tiers never cost more.
    for (int t = 1; t <= 4; ++t) {
        const auto lo = graphicsProfile(static_cast<GraphicsTier>(t - 1));
        const auto hi = graphicsProfile(static_cast<GraphicsTier>(t));
        CHECK(lo.renderScale <= hi.renderScale);
        CHECK(lo.msaaSamples <= hi.msaaSamples);
        CHECK(lo.maxTextureSize <= hi.maxTextureSize);
    }
    rhi::DeviceInfo d;
    d.backend = rhi::Backend::OpenGL;
    d.maxTextureSize = 16384;
    d.maxSamples = 8;
    CHECK(recommendGraphicsTier(d, false) == GraphicsTier::High);
    d.softwareRenderer = true;
    CHECK(recommendGraphicsTier(d, false) == GraphicsTier::Low);
    d.softwareRenderer = false;
    d.backend = rhi::Backend::OpenGLES;
    CHECK(recommendGraphicsTier(d, true) == GraphicsTier::Medium);
    d.maxTextureSize = 4096;
    CHECK(recommendGraphicsTier(d, true) == GraphicsTier::Low);
    d.maxTextureSize = 2048;
    CHECK(recommendGraphicsTier(d, true) == GraphicsTier::UltraMobile);
}

TEST_CASE("render components serialize through reflection") {
    registerRenderComponents();
    Scene s;
    const auto e = s.createEntity("Hero");
    auto& sr = s.registry().emplace<SpriteRendererComponent>(e);
    sr.texture = "assets://hero.png";
    sr.uvRect = {0.25f, 0.0f, 0.5f, 0.5f};
    sr.flipX = true;
    sr.orderInLayer = 4;
    auto& cam = s.registry().emplace<CameraComponent>(e);
    cam.projection = Projection::Orthographic;
    const std::string text = SceneSerializer::toString(s);
    CHECK(text.find("\"Orthographic\"") != std::string::npos);
    Scene loaded;
    REQUIRE(SceneSerializer::fromString(loaded, text));
    const auto le = loaded.findByName("Hero");
    const auto& lsr = loaded.registry().get<SpriteRendererComponent>(le);
    CHECK(lsr.texture == "assets://hero.png");
    CHECK(lsr.uvRect == math::Vec4{0.25f, 0.0f, 0.5f, 0.5f});
    CHECK(lsr.flipX);
    CHECK(lsr.orderInLayer == 4);
    CHECK(loaded.registry().get<CameraComponent>(le).projection == Projection::Orthographic);
    CHECK(ComponentRegistry::instance().find("MeshRenderer")->category == "Rendering");
}

TEST_CASE("RenderPlugin in headless mode provides CPU assets and services") {
    EngineConfig c;
    c.headless = true;
    c.maxFrames = 3;
    c.workerThreads = 1;
    c.userDirectory = (std::filesystem::temp_directory_path() / ("agr_rp_" + Uuid::generate().toString())).string();
    c.assetsDirectory = c.userDirectory;
    Engine engine(c);
    engine.plugins().addStatic(std::make_unique<RenderPlugin>());
    REQUIRE(engine.initialize(nullptr));
    CHECK(engine.service<AssetManager>() != nullptr);
    CHECK(engine.service<Renderer>() == nullptr);  // no GPU
    CHECK(engine.service<RenderPlugin>() != nullptr);
    struct App : Application {} app;
    engine.run(app);
    CHECK(engine.service<AssetManager>() == nullptr);  // unregistered on unload
    std::filesystem::remove_all(c.userDirectory);
}

TEST_CASE("occlusion buffer: hidden behind a wall, visible beside, in front, across the near plane") {
    using math::Mat4;
    using math::Vec3;
    for (bool zeroToOne : {true, false}) {
        CAPTURE(zeroToOne);
        const Mat4 proj = Mat4::perspective(math::radians(60.0f), 16.0f / 9.0f, 0.1f, 200.0f,
                                            zeroToOne ? Mat4::DepthRange::ZeroToOne : Mat4::DepthRange::NegativeOneToOne);
        const Mat4 view = Mat4::lookAt({0, 1, 10}, {0, 1, 0}, {0, 1, 0});
        OcclusionBuffer ob;
        ob.begin(proj * view, 256, 144, zeroToOne);
        CHECK(ob.empty());
        CHECK_FALSE(ob.occluded(math::Aabb::fromCenterExtents({0, 1, -5}, Vec3{0.5f})));  // no occluder
        ob.addBox(Mat4::translate({0, 1, 0}), {3.0f, 2.0f, 0.2f});  // wall 6 x 4 at z = 0
        CHECK(ob.triangles() == 12);
        CHECK(ob.coverage() > 0.05f);
        CHECK(ob.occluded(math::Aabb::fromCenterExtents({0, 1, -5}, Vec3{0.5f})));        // behind
        CHECK(ob.occluded(math::Aabb::fromCenterExtents({1.5f, 0.5f, -2}, Vec3{0.4f})));  // behind, off center
        CHECK_FALSE(ob.occluded(math::Aabb::fromCenterExtents({8, 1, -5}, Vec3{0.5f})));  // beside
        CHECK_FALSE(ob.occluded(math::Aabb::fromCenterExtents({0, 1, 4}, Vec3{0.5f})));   // in front
        CHECK_FALSE(ob.occluded(math::Aabb::fromCenterExtents({0, 4.5f, -5}, Vec3{1.5f}))); // sticks out above
        CHECK_FALSE(ob.occluded(math::Aabb::fromCenterExtents({0, 1, 10}, Vec3{1.0f})));  // around the camera
        CHECK_FALSE(ob.occluded(math::Aabb::fromCenterExtents({0, 1, -5}, Vec3{40.0f}))); // bigger than the wall
        // A quad occludes from both sides.
        ob.begin(proj * view, 256, 144, zeroToOne);
        ob.addQuad(Mat4::trs({0, 1, 0}, math::Quat::fromEuler({0, math::radians(180.0f), 0}), Vec3{1.0f}), {3.0f, 2.0f});
        CHECK(ob.occluded(math::Aabb::fromCenterExtents({0, 1, -5}, Vec3{0.5f})));
        // An occluder behind the camera hides nothing.
        ob.begin(proj * view, 256, 144, zeroToOne);
        ob.addBox(Mat4::translate({0, 1, 15}), {3.0f, 2.0f, 0.2f});
        CHECK_FALSE(ob.occluded(math::Aabb::fromCenterExtents({0, 1, -5}, Vec3{0.5f})));
    }
}

}  // TEST_SUITE

namespace {
/// Claims a Vulkan window whose device creation will fail, and records API switches.
class FakeVulkanPlatform final : public PlatformLayer {
public:
    GraphicsApi current = GraphicsApi::Vulkan;
    std::vector<GraphicsApi> switches;
    Status initialize(const EngineConfig&) override { return {}; }
    void shutdown() override {}
    bool pollEvents(EventBus&, Input&) override { return true; }
    void present() override {}
    bool canRender() const override { return false; }
    math::Vec2 framebufferSize() const override { return {64, 64}; }
    const char* name() const override { return "FakeVulkan"; }
    GraphicsApi graphicsApi() const override { return current; }
    Status setGraphicsApi(GraphicsApi api) override {
        switches.push_back(api);
        current = api;
        return {};
    }
};
}  // namespace

TEST_CASE("RenderPlugin falls back from Vulkan to OpenGL when no device can be created") {
    EngineConfig c;
    c.workerThreads = 1;
    c.maxFrames = 1;
    c.userDirectory = (std::filesystem::temp_directory_path() / ("agr_fb_" + Uuid::generate().toString())).string();
    c.assetsDirectory = c.userDirectory;
    Engine engine(c);
    engine.plugins().addStatic(std::make_unique<RenderPlugin>());
    auto platform = std::make_unique<FakeVulkanPlatform>();
    FakeVulkanPlatform* fake = platform.get();
    REQUIRE(engine.initialize(std::move(platform)));
    // Vulkan failed (no loader entry point) -> the window was switched to OpenGL...
    REQUIRE(fake->switches.size() == 1);
    CHECK(fake->switches[0] == GraphicsApi::OpenGL);
    // ...and the OpenGL attempt also failed here (fake platform has no GL functions),
    // so the plugin reports an error instead of running without a device.
    CHECK_FALSE(engine.plugins().isLoaded("Render"));
    bool reported = false;
    for (const auto& e : engine.plugins().entries())
        if (e.info.name == "Render") reported = e.error.find("OpenGL functions missing") != std::string::npos;
    CHECK(reported);
    std::filesystem::remove_all(c.userDirectory);
}
