#include <doctest/doctest.h>

#include <filesystem>

#include "agr/core/Uuid.hpp"
#include "agr/render/GraphicsProfile.hpp"
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

}  // TEST_SUITE
