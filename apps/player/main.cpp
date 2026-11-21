// AGR game player: runs an exported game (game.json + assets/ + scenes/).
//
// Desktop:  agr_player [--game DIR] [--vulkan|--opengl|--gles] [--validation] [--frames N]
//                      [--user DIR] [--screenshot user://shot.png] [--screenshot-frame N] [--autotest]
//           DIR defaults to the "game" folder next to the executable.
// Android:  built as libmain.so, started by the activity (SDL); the game data is in the
//           APK's assets/game folder.
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <string>
#include <algorithm>
#include <optional>
#include <vector>

#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

#include "agr/animation/AnimationPlugin.hpp"
#include "agr/audio/AudioPlugin.hpp"
#include "agr/core/Log.hpp"
#include "agr/core/Pack.hpp"
#include "agr/physics2d/Physics2DPlugin.hpp"
#include "agr/net/NetPlugin.hpp"
#include "agr/navigation/NavigationPlugin.hpp"
#include "agr/physics3d/Physics3DPlugin.hpp"
#include "agr/platform/SdlFileProvider.hpp"
#include "agr/platform/SdlPlatform.hpp"
#include "agr/render/RenderComponents.hpp"
#include "agr/render/RenderPlugin.hpp"
#include "agr/runtime/Engine.hpp"
#include "agr/runtime/Events.hpp"
#include "agr/runtime/GameManifest.hpp"
#include "agr/script/ScriptPlugin.hpp"
#include "agr/twod/TwoDPlugin.hpp"
#include "agr/ui/UIComponents.hpp"
#include "agr/ui/UIPlugin.hpp"

/// Data key slot: the exporter writes the game's key after the marker (byte 48 = 1 when set).
/// volatile: the compiler must read it at run time, not fold the placeholder.
extern "C" volatile unsigned char agr_pack_key_slot[64];
volatile unsigned char agr_pack_key_slot[64] = {'A', 'G', 'R', 'P', 'A', 'C', 'K', 'K', 'E', 'Y', 'S', 'L', 'O', 'T', '0', '1'};

namespace {

using namespace agr;

struct Options {
    std::string gameDir;
    std::string screenshot;
    bool autotest = false;
    u64 screenshotFrame = 60;  ///< Frame at which --screenshot is taken.
};

class PlayerApp final : public Application {
public:
    PlayerApp(GameManifest manifest, Options options) : m_manifest(std::move(manifest)), m_options(std::move(options)) {}

    Status onStart(Engine& engine) override {
        if (!m_manifest.splashImage.empty() && m_manifest.splashSeconds > 0.0f) {
            showSplash(engine);  // the startup scene follows in onUpdate
            return {};
        }
        return startGame(engine);
    }

    /// Splash: a camera clearing to the splash color and a full-screen canvas with the image
    /// (aspect kept, at most 60 % of the screen).
    void showSplash(Engine& engine) {
        Scene& scene = engine.scene();
        const auto cam = scene.createEntity("Splash Camera");
        auto& c = scene.registry().emplace<CameraComponent>(cam);
        c.projection = Projection::Orthographic;
        const auto& sc = m_manifest.splashColor;
        c.clearColor = {sc[0], sc[1], sc[2], sc[3]};
        const auto canvas = scene.createEntity("Splash");
        scene.registry().emplace<UICanvasComponent>(canvas);
        const auto image = scene.createEntity("Splash Image", canvas);
        auto& r = scene.registry().emplace<UIRectComponent>(image);
        r.anchorMin = {0.2f, 0.2f};
        r.anchorMax = {0.8f, 0.8f};
        r.size = {0.0f, 0.0f};
        auto& img = scene.registry().emplace<UIImageComponent>(image);
        img.texture = m_manifest.splashImage;
        img.preserveAspect = true;
        scene.updateTransforms();
        m_splash = true;
        AGR_LOG_INFO("Player", "splash screen: {} ({} s)", m_manifest.splashImage, m_manifest.splashSeconds);
    }

    Status startGame(Engine& engine) {
        const std::string uri = "game://" + m_manifest.startupScene;
        if (Status s = engine.loadScene(uri); !s) {
            AGR_LOG_ERROR("Player", "cannot load the startup scene '{}': {}", uri, s.error().message);
            return s;
        }
        AGR_LOG_INFO("Player", "'{}' {} started ({} entities)", m_manifest.name, m_manifest.version, engine.scene().entityCount());
        m_back = engine.events().subscribe<KeyEvent>([this, &engine](const KeyEvent& e) {
            if (!e.pressed || (e.key != Key::AndroidBack && e.key != Key::Escape)) return;
            const std::string startup = "game://" + m_manifest.startupScene;
            if (engine.sceneUri() != startup) engine.requestScene(startup);  // back to the menu / first scene
            else if (e.key == Key::AndroidBack && m_manifest.backQuits) engine.requestQuit();
        });
        return {};
    }

    void onUpdate(Engine& engine, const FrameTime& time) override {
        // Screenshot from frame 60, once models and the sky image have loaded (frame 600 at most).
        if (!m_options.screenshot.empty() && !m_screenshotTaken && time.frameIndex >= m_screenshotFrame)
            if (auto* rp = engine.service<RenderPlugin>()) {
                const Renderer* r = rp->renderer();
                const bool loading = r && (r->stats().modelsLoading > 0 || r->stats().environmentLoading);
                if (!loading || time.frameIndex >= 600) {
                    rp->requestScreenshot(m_options.screenshot);
                    m_screenshotTaken = true;
                }
            }
        if (m_splash) {
            m_splashTime += time.unscaledDeltaSeconds;
            if (m_splashTime >= m_manifest.splashSeconds && time.frameIndex >= 2) {
                m_splash = false;
                if (Status s = startGame(engine); !s) engine.requestQuit();
            }
            return;
        }
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
    u64 m_screenshotFrame = m_options.screenshotFrame;
    bool m_screenshotTaken = false;
    bool m_rendered = false;
    bool m_splash = false;
    f64 m_splashTime = 0.0;
    int m_failures = 0;
};

std::unique_ptr<FileProvider> gameProvider(const std::string& root);

std::optional<crypto::AesKey> embeddedKey() {
    if (agr_pack_key_slot[48] != 1) return std::nullopt;
    crypto::AesKey k;
    for (int i = 0; i < 32; ++i) k[i] = agr_pack_key_slot[16 + i];
    return k;
}

/// game:// = the game folder (or game.agrpak) with patch packs layered on top.
std::shared_ptr<OverlayFileProvider> mountGame(Engine& engine, const std::string& gameDir) {
    namespace fs = std::filesystem;
    auto overlay = std::make_shared<OverlayFileProvider>();
    overlay->addLayer(std::shared_ptr<FileProvider>(gameProvider(gameDir)));
    const std::optional<crypto::AesKey> key = embeddedKey();
    setGamePackKey(key ? &*key : nullptr);
    auto addPack = [&](PackReader reader, const std::string& name) {
        if (!reader) return;
        auto pack = PackFileProvider::open(std::move(reader), key ? &*key : nullptr);
        if (!pack) {
            AGR_LOG_ERROR("Player", "{}: {}", name, pack.error().message);
            return;
        }
        AGR_LOG_INFO("Player", "data pack {}: {} files{}", name, pack.value()->entries().size(), pack.value()->encrypted() ? " (encrypted)" : "");
        overlay->addLayer(std::shared_ptr<FileProvider>(std::move(pack.value())));
    };
    addPack(sdlPackReader(gameDir + "/game.agrpak"), "game.agrpak");
    // Patches next to the game (desktop) and packs installed in user://packs (DLC, downloads).
    std::vector<fs::path> packs;
    std::error_code ec;
    for (const fs::path& dir : {fs::path(gameDir) / "patches", fs::path(engine.config().userDirectory) / "packs"})
        if (fs::is_directory(dir, ec))
            for (const auto& e : fs::directory_iterator(dir, ec))
                if (e.path().extension() == ".agrpak") packs.push_back(e.path());
    std::sort(packs.begin(), packs.end(), [](const fs::path& a, const fs::path& b) { return a.filename() < b.filename(); });
    for (const fs::path& p : packs) addPack(packFileReader(p), p.filename().string());
    engine.vfs().mount("game", std::make_unique<SubdirFileProvider>(overlay, ""));
    engine.vfs().mount("assets", std::make_unique<SubdirFileProvider>(overlay, "assets"));
    return overlay;
}

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
        else if (a == "--screenshot-frame" && i + 1 < argc) options.screenshotFrame = std::strtoull(argv[++i], nullptr, 10);
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
    engine.plugins().addStatic(std::make_unique<NetPlugin>());
    engine.plugins().addStatic(std::make_unique<NavigationPlugin>());
    engine.plugins().addStatic(std::make_unique<Physics2DPlugin>());
    engine.plugins().addStatic(std::make_unique<Physics3DPlugin>());
    engine.plugins().addStatic(std::make_unique<AudioPlugin>());
    if (Status s = engine.initialize(std::make_unique<SdlPlatform>()); !s) return 1;
    // Game data: read-only; on Android straight from the APK.
    const auto gameFiles = mountGame(engine, options.gameDir);
    if (m.keepScreenOn) SDL_DisableScreenSaver();

    PlayerApp app(m, options);
    const int code = engine.run(app);
    return code != 0 ? code : app.exitCode();
}
