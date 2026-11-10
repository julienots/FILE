// AGR game player: runs an exported game (game.json + assets/ + scenes/).
//
// Desktop:  agr_player [--game DIR] [--vulkan|--opengl|--gles] [--validation] [--frames N]
//                      [--user DIR] [--screenshot user://shot.png] [--autotest]
//           DIR defaults to the "game" folder next to the executable.
// Android:  built as libmain.so, started by the activity (SDL); the game data is in the
//           APK's assets/game folder.
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <string>

#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

#include "agr/animation/AnimationPlugin.hpp"
#include "agr/audio/AudioPlugin.hpp"
#include "agr/core/Log.hpp"
#include "agr/physics2d/Physics2DPlugin.hpp"
#include "agr/physics3d/Physics3DPlugin.hpp"
#include "agr/platform/SdlFileProvider.hpp"
#include "agr/platform/SdlPlatform.hpp"
#include "agr/render/RenderPlugin.hpp"
#include "agr/runtime/Engine.hpp"
#include "agr/runtime/Events.hpp"
#include "agr/runtime/GameManifest.hpp"
#include "agr/script/ScriptPlugin.hpp"
#include "agr/twod/TwoDPlugin.hpp"
#include "agr/ui/UIComponents.hpp"
#include "agr/ui/UIPlugin.hpp"

namespace {

using namespace agr;

struct Options {
    std::string gameDir;
    std::string screenshot;
    bool autotest = false;
};

class PlayerApp final : public Application {
public:
    PlayerApp(GameManifest manifest, Options options) : m_manifest(std::move(manifest)), m_options(std::move(options)) {}

    Status onStart(Engine& engine) override {
        const std::string uri = "game://" + m_manifest.startupScene;
        if (Status s = engine.loadScene(uri); !s) {
            AGR_LOG_ERROR("Player", "cannot load the startup scene '{}': {}", uri, s.error().message);
            return s;
        }
        AGR_LOG_INFO("Player", "'{}' {} started ({} entities)", m_manifest.name, m_manifest.version, engine.scene().entityCount());
        m_back = engine.events().subscribe<KeyEvent>([this, &engine](const KeyEvent& e) {
            if (e.pressed && e.key == Key::AndroidBack && m_manifest.backQuits) engine.requestQuit();
        });
        return {};
    }

    void onUpdate(Engine& engine, const FrameTime& time) override {
        if (!m_options.screenshot.empty() && time.frameIndex == m_screenshotFrame)
            if (auto* rp = engine.service<RenderPlugin>()) rp->requestScreenshot(m_options.screenshot);
        if (m_options.autotest && engine.config().maxFrames > 0 && time.frameIndex + 2 == engine.config().maxFrames) autotest(engine);
        if (m_options.autotest && time.frameIndex == 30) m_rendered = renderedSomething(engine);
    }

    int exitCode() const { return m_failures; }

private:
    bool renderedSomething(Engine& engine) {
        auto* rp = engine.service<RenderPlugin>();
        return rp && rp->renderer() && rp->renderer()->stats().drawCalls > 0;
    }
    void check(bool ok, const char* what) {
        if (ok) AGR_LOG_INFO("Autotest", "ok: {}", what);
        else {
            AGR_LOG_ERROR("Autotest", "FAILED: {}", what);
            ++m_failures;
        }
    }
    void autotest(Engine& engine) {
        Scene& scene = engine.scene();
        check(scene.entityCount() > 0, "the startup scene is loaded");
        if (auto* sp = engine.service<ScriptPlugin>())
            check(sp->scripts()->stats().errors == 0 && sp->scripts()->stats().faulted == 0, "scripts run without errors");
        check(m_rendered || renderedSomething(engine), "frames are rendered");
        bool hasCanvas = false;
        scene.registry().view<UICanvasComponent>().each([&](ecs::Entity, UICanvasComponent&) { hasCanvas = true; });
        if (hasCanvas) {
            auto* rp = engine.service<RenderPlugin>();
            check(rp && rp->renderer() && rp->renderer()->stats().overlayQuads > 0, "the game UI is drawn");
        }
        if (auto* audio = engine.service<AudioPlugin>()) check(audio->audio().initialized(), "audio is running");
        AGR_LOG_INFO("Autotest", "{}", m_failures == 0 ? "PLAYER AUTOTEST PASSED" : "PLAYER AUTOTEST FAILED");
    }

    GameManifest m_manifest;
    Options m_options;
    SubscriptionId m_back{};
    u64 m_screenshotFrame = 60;
    bool m_rendered = false;
    int m_failures = 0;
};

std::unique_ptr<FileProvider> gameProvider(const std::string& root) {
#if defined(AGR_PLATFORM_ANDROID)
    return std::make_unique<SdlFileProvider>(root);  // APK assets
#else
    return std::make_unique<NativeFileProvider>(root, true);
#endif
}

} // namespace

int main(int argc, char** argv) {
    namespace fs = std::filesystem;
    Options options;
    EngineConfig config;
    bool userDirSet = false;
#if defined(AGR_PLATFORM_ANDROID)
    options.gameDir = "game";
#else
    const fs::path exeDir = fs::path(argc > 0 ? argv[0] : ".").parent_path();
    options.gameDir = (exeDir / "game").string();
#endif
    for (int i = 1; i < argc; ++i) {
        const std::string a = argv[i];
        if (a == "--game" && i + 1 < argc) options.gameDir = argv[++i];
        else if (a == "--vulkan") config.window.graphics = GraphicsApi::Vulkan;
        else if (a == "--opengl") config.window.graphics = GraphicsApi::OpenGL;
        else if (a == "--gles") config.window.graphics = GraphicsApi::OpenGLES;
        else if (a == "--validation") config.window.graphicsValidation = true;
        else if (a == "--frames" && i + 1 < argc) config.maxFrames = std::strtoull(argv[++i], nullptr, 10);
        else if (a == "--screenshot" && i + 1 < argc) options.screenshot = argv[++i];
        else if (a == "--autotest") options.autotest = true;
        else if (a == "--user" && i + 1 < argc) {
            config.userDirectory = argv[++i];
            userDirSet = true;
        }
    }

    // The manifest decides the window, the profile and the orientation.
    const auto provider = gameProvider(options.gameDir);
    auto manifestBytes = provider->read("game.json");
    if (!manifestBytes) {
        SDL_Log("AGR player: no game found (%s)", manifestBytes.error().message.c_str());
        return 2;
    }
    auto manifest = GameManifest::fromJson(std::string(manifestBytes.value().begin(), manifestBytes.value().end()));
    if (!manifest) {
        SDL_Log("AGR player: %s", manifest.error().message.c_str());
        return 2;
    }
    const GameManifest& m = manifest.value();
    config.appName = m.name;
    config.organization = m.organization;
    config.window.title = m.name;
    config.graphicsProfile = m.graphicsProfile;
    config.targetFps = m.targetFps;
    config.pluginsDirectory = "";
    config.assetsDirectory = options.gameDir + "/assets";
    if (!userDirSet) {
        if (char* pref = SDL_GetPrefPath(m.organization.c_str(), GameManifest::safeName(m.name).c_str())) {
            config.userDirectory = pref;  // saves: internal storage on Android
            SDL_free(pref);
        }
    }
    switch (m.orientation) {
        case ScreenOrientation::Landscape: SDL_SetHint(SDL_HINT_ORIENTATIONS, "LandscapeLeft LandscapeRight"); break;
        case ScreenOrientation::Portrait: SDL_SetHint(SDL_HINT_ORIENTATIONS, "Portrait"); break;
        case ScreenOrientation::Sensor: SDL_SetHint(SDL_HINT_ORIENTATIONS, "LandscapeLeft LandscapeRight Portrait"); break;
    }

    Engine engine(config);
    engine.plugins().addStatic(std::make_unique<RenderPlugin>());
    engine.plugins().addStatic(std::make_unique<TwoDPlugin>());
    engine.plugins().addStatic(std::make_unique<UIPlugin>());
    engine.plugins().addStatic(std::make_unique<ScriptPlugin>());
    engine.plugins().addStatic(std::make_unique<AnimationPlugin>());
    engine.plugins().addStatic(std::make_unique<Physics2DPlugin>());
    engine.plugins().addStatic(std::make_unique<Physics3DPlugin>());
    engine.plugins().addStatic(std::make_unique<AudioPlugin>());
    if (Status s = engine.initialize(std::make_unique<SdlPlatform>()); !s) return 1;
    // Game data: read-only; on Android straight from the APK.
    engine.vfs().mount("game", gameProvider(options.gameDir));
    engine.vfs().mount("assets", gameProvider(options.gameDir + "/assets"));
    if (m.keepScreenOn) SDL_DisableScreenSaver();

    PlayerApp app(m, options);
    const int code = engine.run(app);
    return code != 0 ? code : app.exitCode();
}
