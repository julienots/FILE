// AGR Sandbox — Phase 1 runtime smoke application.
//
// Exercises the whole foundation: engine loop, SDL3 window + GL context, input,
// events, ECS scene hierarchy, dynamic plugin (Spin), scene save / load via VFS.
// Geometry rendering arrives with the renderer (Phase 2); this app clears the
// framebuffer through the real GL context to prove the platform path.
//
// Usage: agr_sandbox [--headless] [--frames N] [--plugins DIR]
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <format>
#include <string>

#include "agr/platform/SdlPlatform.hpp"
#include "agr/runtime/Engine.hpp"
#include "agr/runtime/Events.hpp"
#include "agr/scene/SceneSerializer.hpp"

namespace {

using namespace agr;

class SandboxApp final : public Application {
public:
    explicit SandboxApp(SdlPlatform* platform) : m_platform(platform) {}

    Status onStart(Engine& engine) override {
        Scene& scene = engine.scene();
        scene.setName("Sandbox");

        // A small solar system: hierarchy + transforms + plugin component.
        const auto sun = scene.createEntity("Sun");
        const auto* spinInfo = ComponentRegistry::instance().find("Spin");
        for (int i = 0; i < 4; ++i) {
            const auto pivot = scene.createEntity(std::format("Orbit {}", i), sun);
            const auto planet = scene.createEntity(std::format("Planet {}", i), pivot);
            scene.registry().get<TransformComponent>(planet).position = {2.0f + 1.5f * static_cast<float>(i), 0.0f, 0.0f};
            if (spinInfo) {
                // Added through reflection: the sandbox does not know the Spin type.
                auto& pool = scene.registry().ensurePool(spinInfo->id, spinInfo->makePool);
                void* spin = pool.emplaceDefault(scene.registry(), pivot);
                spinInfo->field("speed")->ref<float>(spin) = 1.0f / static_cast<float>(i + 1);
            }
        }
        AGR_LOG_INFO("Sandbox", "scene created: {} entities, Spin plugin {}", scene.entityCount(),
                     spinInfo ? "active" : "not found");

        if (Status s = engine.saveScene("user://sandbox.agrscene"); !s)
            AGR_LOG_WARN("Sandbox", "could not save scene: {}", s.error().message);
        else
            AGR_LOG_INFO("Sandbox", "scene saved to user://sandbox.agrscene");

        m_keySub = engine.events().subscribe<KeyEvent>([&engine](const KeyEvent& e) {
            if (e.pressed && (e.key == Key::Escape || e.key == Key::AndroidBack)) engine.requestQuit();
        });
        engine.events().subscribe<WindowResizedEvent>([](const WindowResizedEvent& e) {
            AGR_LOG_INFO("Sandbox", "resized to {}x{} ({}x{} px)", e.width, e.height, e.pixelWidth, e.pixelHeight);
        });

        if (m_platform) {
            m_glClearColor = reinterpret_cast<ClearColorFn>(m_platform->glProcAddress("glClearColor"));
            m_glClear = reinterpret_cast<ClearFn>(m_platform->glProcAddress("glClear"));
            m_glViewport = reinterpret_cast<ViewportFn>(m_platform->glProcAddress("glViewport"));
        }
        return {};
    }

    void onUpdate(Engine& engine, const FrameTime& time) override {
        m_titleTimer += time.unscaledDeltaSeconds;
        if (m_titleTimer >= 1.0) {
            m_titleTimer = 0.0;
            const auto& s = engine.stats();
            const auto planet = engine.scene().findByName("Planet 0");
            const auto p = engine.scene().worldMatrix(planet).translation();
            AGR_LOG_INFO("Sandbox", "fps {:.1f} | cpu {:.3f} ms | entities {} | planet0 world ({:.2f}, {:.2f}, {:.2f})",
                         s.fps, s.cpuMs, s.entityCount, p.x, p.y, p.z);
            if (m_platform) m_platform->setTitle(std::format("AGR ENGINE Sandbox - {:.0f} FPS", s.fps));
        }
    }

    void onRender(Engine& engine, const FrameTime& time) override {
        if (!m_glClear) return;
        const auto fb = engine.platform().framebufferSize();
        m_glViewport(0, 0, static_cast<int>(fb.x), static_cast<int>(fb.y));
        const float t = static_cast<float>(time.totalSeconds);
        // AGR brand background (deep indigo) with a slow pulse.
        m_glClearColor(0.06f + 0.02f * std::sin(t), 0.06f, 0.11f + 0.03f * std::sin(t * 0.7f), 1.0f);
        m_glClear(0x00004000 | 0x00000100);  // COLOR | DEPTH
    }

    void onShutdown(Engine& engine) override { engine.events().unsubscribe(m_keySub); }

private:
    using ClearColorFn = void (*)(float, float, float, float);
    using ClearFn = void (*)(unsigned int);
    using ViewportFn = void (*)(int, int, int, int);

    SdlPlatform* m_platform;
    ClearColorFn m_glClearColor = nullptr;
    ClearFn m_glClear = nullptr;
    ViewportFn m_glViewport = nullptr;
    SubscriptionId m_keySub;
    f64 m_titleTimer = 0.0;
};

std::string executableDirectory(const char* argv0) {
    std::string p = argv0 ? argv0 : "";
    const auto slash = p.find_last_of("/\\");
    return slash == std::string::npos ? "." : p.substr(0, slash);
}

} // namespace

int main(int argc, char** argv) {
    EngineConfig config;
    config.appName = "AGR Sandbox";
    config.window.title = "AGR ENGINE Sandbox";
    config.pluginsDirectory = executableDirectory(argc > 0 ? argv[0] : nullptr);
    for (int i = 1; i < argc; ++i) {
        if (!std::strcmp(argv[i], "--headless")) config.headless = true;
        else if (!std::strcmp(argv[i], "--frames") && i + 1 < argc) config.maxFrames = std::strtoull(argv[++i], nullptr, 10);
        else if (!std::strcmp(argv[i], "--plugins") && i + 1 < argc) config.pluginsDirectory = argv[++i];
    }

    Engine engine(config);
    std::unique_ptr<SdlPlatform> sdl;
    SdlPlatform* sdlRaw = nullptr;
    if (!config.headless) {
        sdl = std::make_unique<SdlPlatform>();
        sdlRaw = sdl.get();
    }
    if (Status s = engine.initialize(std::move(sdl)); !s) return 1;

    SandboxApp app(sdlRaw);
    return engine.run(app);
}
