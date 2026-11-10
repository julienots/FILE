// AGR Sandbox — runtime demonstration of phases 1 and 2.
//
// A lit 3D scene (built-in meshes, directional light, textured ground), a
// field of 225 cubes (frustum culling), a 2D sprite layer (batching, sorting),
// the dynamic Spin plugin animating the hierarchy, and scene save to user://.
//
// Usage: agr_sandbox [--headless] [--frames N] [--gles] [--profile NAME]
//                    [--screenshot user://shot.png] [--plugins DIR] [--sprites N]
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <format>
#include <string>

#include "agr/platform/SdlPlatform.hpp"
#include "agr/render/RenderComponents.hpp"
#include "agr/render/RenderPlugin.hpp"
#include "agr/runtime/Engine.hpp"
#include "agr/runtime/Events.hpp"

namespace {

using namespace agr;
using math::Quat;
using math::Vec3;

void lookAt(Scene& scene, ecs::Entity e, Vec3 eye, Vec3 target) {
    const math::Mat4 world = math::inverse(math::Mat4::lookAt(eye, target, Vec3::up()));
    auto& t = scene.registry().get<TransformComponent>(e);
    Vec3 s;
    math::decompose(world, t.position, t.rotation, s);
}

/// Adds the plugin-defined Spin component through reflection (the sandbox does not know its type).
void addSpin(Scene& scene, ecs::Entity e, float radiansPerSecond, Vec3 axis = Vec3::up()) {
    const ComponentInfo* spin = ComponentRegistry::instance().find("Spin");
    if (!spin) return;
    void* c = scene.registry().ensurePool(spin->id, spin->makePool).emplaceDefault(scene.registry(), e);
    spin->field("speed")->ref<float>(c) = radiansPerSecond;
    spin->field("axis")->ref<Vec3>(c) = axis;
}

class SandboxApp final : public Application {
public:
    SandboxApp(SdlPlatform* platform, std::string screenshot, u32 stressSprites)
        : m_platform(platform), m_screenshot(std::move(screenshot)), m_stressSprites(stressSprites) {}

    Status onStart(Engine& engine) override {
        Scene& scene = engine.scene();
        scene.setName("Sandbox");
        auto& reg = scene.registry();

        const auto camera = scene.createEntity("Main Camera");
        reg.emplace<CameraComponent>(camera);
        lookAt(scene, camera, {0.0f, 4.5f, 11.0f}, {0.0f, 0.8f, 0.0f});

        const auto sun = scene.createEntity("Sun Light");
        reg.emplace<DirectionalLightComponent>(sun).intensity = 1.1f;
        reg.get<TransformComponent>(sun).rotation = Quat::fromEuler({math::radians(-50.0f), math::radians(35.0f), 0.0f});
        reg.emplace<EnvironmentComponent>(sun);

        const auto ground = scene.createEntity("Ground");
        auto& gt = reg.get<TransformComponent>(ground);
        gt.scale = {40.0f, 1.0f, 40.0f};
        auto& gm = reg.emplace<MeshRendererComponent>(ground);
        gm.mesh = PrimitiveMesh::Plane;
        gm.texture = "assets://textures/grid.png";
        gm.uvScale = {20.0f, 20.0f};

        // Centre piece: amber sphere with orbiting children (hierarchy + Spin plugin).
        const auto core = scene.createEntity("Core");
        reg.get<TransformComponent>(core).position = {0.0f, 1.5f, 0.0f};
        reg.get<TransformComponent>(core).scale = Vec3{1.6f};
        auto& cm = reg.emplace<MeshRendererComponent>(core);
        cm.mesh = PrimitiveMesh::Sphere;
        cm.color = {0.96f, 0.65f, 0.14f, 1.0f};
        const math::Color palette[] = {{0.90f, 0.89f, 0.87f, 1}, {0.24f, 0.55f, 0.99f, 1}, {0.25f, 0.73f, 0.31f, 1}, {0.97f, 0.32f, 0.29f, 1}};
        for (int i = 0; i < 4; ++i) {
            const auto pivot = scene.createEntity(std::format("Orbit {}", i));
            reg.get<TransformComponent>(pivot).position = {0.0f, 1.5f, 0.0f};
            reg.get<TransformComponent>(pivot).rotation = Quat::fromAxisAngle(Vec3::up(), static_cast<float>(i) * 1.6f);
            addSpin(scene, pivot, 0.9f / static_cast<float>(i + 1));
            const auto body = scene.createEntity(std::format("Satellite {}", i), pivot);
            auto& bt = reg.get<TransformComponent>(body);
            bt.position = {2.0f + 0.9f * static_cast<float>(i), 0.0f, 0.0f};
            bt.scale = Vec3{0.45f + 0.1f * static_cast<float>(i)};
            auto& bm = reg.emplace<MeshRendererComponent>(body);
            bm.mesh = i % 2 ? PrimitiveMesh::Sphere : PrimitiveMesh::Cube;
            bm.color = palette[i];
            addSpin(scene, body, 1.5f, {1.0f, 1.0f, 0.0f});
        }

        // Field of crates: many are outside the view and get culled.
        const auto field = scene.createEntity("Crate Field");
        for (int x = -7; x <= 7; ++x) {
            for (int z = -7; z <= 7; ++z) {
                if (std::abs(x) < 3 && std::abs(z) < 3) continue;
                const auto c = scene.createEntity("Crate", field);
                auto& t = reg.get<TransformComponent>(c);
                const float h = 0.3f + 0.25f * static_cast<float>((x * 7 + z * 13 + 100) % 4);
                t.position = {static_cast<float>(x) * 2.2f, h * 0.5f, static_cast<float>(z) * 2.2f};
                t.scale = {0.8f, h, 0.8f};
                auto& m = reg.emplace<MeshRendererComponent>(c);
                m.color = {0.35f, 0.38f, 0.43f, 1.0f};
            }
        }

        // 2D layer: AGR mark sprites (alpha blended, sorted, batched into one draw).
        const auto logo = scene.createEntity("AGR Mark");
        reg.get<TransformComponent>(logo).position = {0.0f, 4.2f, -6.0f};
        const std::string markTexture = "assets://textures/agr_mark.png";
        {
            // Scoped: component references are invalidated when the pool grows.
            auto& ls = reg.emplace<SpriteRendererComponent>(logo);
            ls.texture = markTexture;
            ls.size = {3.0f, 3.0f};
        }
        for (int i = 0; i < 6; ++i) {
            const auto s = scene.createEntity(std::format("Mark {}", i), logo);
            reg.get<TransformComponent>(s).position = {-5.0f + 2.0f * static_cast<float>(i) + (i >= 3 ? 2.0f : 0.0f), -0.6f, 0.0f};
            auto& sr = reg.emplace<SpriteRendererComponent>(s);
            sr.texture = markTexture;
            sr.size = {1.0f, 1.0f};
            sr.color = {1.0f, 1.0f, 1.0f, 0.35f + 0.1f * static_cast<float>(i)};
            sr.flipX = i % 2 == 1;
            sr.orderInLayer = i;
        }

        // Optional stress test: N small sprites with two textures, spread over a wall behind the scene.
        if (m_stressSprites > 0) {
            const auto wall = scene.createEntity("Sprite Wall");
            reg.get<TransformComponent>(wall).position = {0.0f, 6.0f, -12.0f};
            const u32 side = static_cast<u32>(std::ceil(std::sqrt(static_cast<float>(m_stressSprites))));
            for (u32 i = 0; i < m_stressSprites; ++i) {
                const auto e = scene.createEntity("S", wall);
                reg.get<TransformComponent>(e).position = {(static_cast<float>(i % side) - side * 0.5f) * 0.12f,
                                                           (static_cast<float>(i / side) - side * 0.5f) * 0.12f, 0.0f};
                auto& sr = reg.emplace<SpriteRendererComponent>(e);
                sr.texture = (i / 64) % 2 ? markTexture : std::string("assets://textures/grid.png");
                sr.size = {0.1f, 0.1f};
            }
        }

        AGR_LOG_INFO("Sandbox", "scene: {} entities, Spin plugin {}", scene.entityCount(),
                     ComponentRegistry::instance().find("Spin") ? "active" : "not found");
        if (Status s = engine.saveScene("user://sandbox.agrscene"); !s)
            AGR_LOG_WARN("Sandbox", "could not save scene: {}", s.error().message);

        m_keySub = engine.events().subscribe<KeyEvent>([&engine](const KeyEvent& e) {
            if (!e.pressed) return;
            if (e.key == Key::Escape || e.key == Key::AndroidBack) engine.requestQuit();
            auto* rp = engine.service<RenderPlugin>();
            if (!rp) return;
            // 1..5 switch graphics profiles at runtime; F12 takes a screenshot.
            if (e.key >= Key::Num1 && e.key <= Key::Num5)
                rp->applyProfile(engine, static_cast<GraphicsTier>(static_cast<int>(e.key) - static_cast<int>(Key::Num1)));
            if (e.key == Key::F12) rp->requestScreenshot("user://screenshot.png");
        });
        if (auto* rp = engine.service<RenderPlugin>()) rp->setHotReload(true);
        return {};
    }

    void onUpdate(Engine& engine, const FrameTime& time) override {
        // Slow camera orbit.
        Scene& scene = engine.scene();
        const auto cam = scene.findByName("Main Camera");
        const float a = static_cast<float>(time.totalSeconds) * 0.15f;
        lookAt(scene, cam, {std::sin(a) * 11.0f, 4.5f, std::cos(a) * 11.0f}, {0.0f, 0.8f, 0.0f});

        if (!m_screenshot.empty() && time.frameIndex == m_screenshotFrame) {
            if (auto* rp = engine.service<RenderPlugin>()) rp->requestScreenshot(m_screenshot);
        }

        m_statsTimer += time.unscaledDeltaSeconds;
        if (m_statsTimer < 1.0) return;
        m_statsTimer = 0.0;
        const auto& s = engine.stats();
        const auto* renderer = engine.service<Renderer>();
        if (!renderer) {
            AGR_LOG_INFO("Sandbox", "fps {:.1f} | cpu {:.2f} ms | entities {} (no renderer)", s.fps, s.cpuMs, s.entityCount);
            return;
        }
        const auto& r = renderer->stats();
        const std::string gpu = r.gpuTimeMs >= 0.0 ? std::format("{:.2f} ms", r.gpuTimeMs) : std::string("n/a");
        AGR_LOG_INFO("Sandbox",
                     "fps {:.1f} | cpu {:.2f} ms (render {:.2f}) | gpu {} | draws {} | tris {} | meshes {} (+{} culled) | sprites {} in {} batch(es) | {}x{} MSAA x{} | VRAM {:.1f} MB",
                     s.fps, s.cpuMs, r.cpuMs, gpu, r.drawCalls, r.triangles, r.meshesVisible, r.meshesCulled, r.spritesVisible,
                     r.spriteBatches, r.renderWidth, r.renderHeight, r.msaaSamples, static_cast<double>(r.gpuMemoryBytes) / (1024.0 * 1024.0));
        if (m_platform) m_platform->setTitle(std::format("AGR ENGINE Sandbox - {:.0f} FPS - {} draws", s.fps, r.drawCalls));
    }

    void onShutdown(Engine& engine) override { engine.events().unsubscribe(m_keySub); }

    u64 m_screenshotFrame = 60;

private:
    SdlPlatform* m_platform;
    std::string m_screenshot;
    u32 m_stressSprites = 0;
    SubscriptionId m_keySub;
    f64 m_statsTimer = 0.0;
};

std::string executableDirectory(const char* argv0) {
    std::string p = argv0 ? argv0 : "";
    const auto slash = p.find_last_of("/\\");
    return slash == std::string::npos ? "." : p.substr(0, slash);
}

} // namespace

int main(int argc, char** argv) {
    const std::string exeDir = executableDirectory(argc > 0 ? argv[0] : nullptr);
    EngineConfig config;
    config.appName = "AGR Sandbox";
    config.window.title = "AGR ENGINE Sandbox";
    config.pluginsDirectory = exeDir;
    config.assetsDirectory = exeDir + "/assets";
    std::string screenshot;
    u32 stressSprites = 0;
    for (int i = 1; i < argc; ++i) {
        if (!std::strcmp(argv[i], "--headless")) config.headless = true;
        else if (!std::strcmp(argv[i], "--gles")) config.window.graphics = GraphicsApi::OpenGLES;
        else if (!std::strcmp(argv[i], "--frames") && i + 1 < argc) config.maxFrames = std::strtoull(argv[++i], nullptr, 10);
        else if (!std::strcmp(argv[i], "--profile") && i + 1 < argc) config.graphicsProfile = argv[++i];
        else if (!std::strcmp(argv[i], "--screenshot") && i + 1 < argc) screenshot = argv[++i];
        else if (!std::strcmp(argv[i], "--plugins") && i + 1 < argc) config.pluginsDirectory = argv[++i];
        else if (!std::strcmp(argv[i], "--sprites") && i + 1 < argc) stressSprites = static_cast<u32>(std::strtoul(argv[++i], nullptr, 10));
    }

    Engine engine(config);
    engine.plugins().addStatic(std::make_unique<RenderPlugin>());
    std::unique_ptr<SdlPlatform> sdl;
    SdlPlatform* sdlRaw = nullptr;
    if (!config.headless) {
        sdl = std::make_unique<SdlPlatform>();
        sdlRaw = sdl.get();
    }
    if (Status s = engine.initialize(std::move(sdl)); !s) return 1;

    SandboxApp app(sdlRaw, screenshot, stressSprites);
    if (config.maxFrames > 0 && config.maxFrames <= app.m_screenshotFrame) app.m_screenshotFrame = config.maxFrames / 2;
    return engine.run(app);
}
