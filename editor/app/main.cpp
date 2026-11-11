// AGR Editor — entry point.
//
// Usage: agr_editor [PROJECT_DIR] [--vulkan | --opengl | --gles] [--validation]
//                   [--smoke-test DIR] [--hub-screenshot FILE.png]
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <string>

#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <fstream>

#include "agr/core/Platform.hpp"
#include "agr/editor/ui/EditorApp.hpp"
#include "agr/platform/SdlPlatform.hpp"
#include "agr/animation/AnimationPlugin.hpp"
#include "agr/audio/AudioPlugin.hpp"
#include "agr/net/NetPlugin.hpp"
#include "agr/navigation/NavigationPlugin.hpp"
#include "agr/physics2d/Physics2DPlugin.hpp"
#include "agr/physics3d/Physics3DPlugin.hpp"
#include "agr/render/RenderPlugin.hpp"
#include "agr/script/ScriptPlugin.hpp"
#include "agr/ui/UIPlugin.hpp"
#include "agr/runtime/Engine.hpp"
#include "agr/editor/AndroidExport.hpp"
#include "agr/editor/GameExport.hpp"

namespace {

/// Command-line tasks without a window (continuous integration, scripts):
///   agr_editor --create-project DIR [--template showcase2d|showcase3d|empty2d|empty25d|empty3d] [--name NAME]
///   agr_editor --project DIR --export desktop|android-apk|android-release-apk|android-aab --output DIR
///   (release builds read the keystore passwords from AGR_STORE_PASSWORD / AGR_KEY_PASSWORD)
/// Returns -1 when no command-line task was given (the editor opens).
[[maybe_unused]] int runCommandLine(int argc, char** argv, const std::filesystem::path& exeDir) {  // desktop only
    using namespace agr;
    using namespace agr::editor;
    namespace fs = std::filesystem;
    std::string create, tmplName = "showcase2d", name, projectDir, exportKind, output;
    for (int i = 1; i < argc; ++i) {
        const std::string a = argv[i];
        auto next = [&]() -> std::string { return i + 1 < argc ? argv[++i] : std::string(); };
        if (a == "--create-project") create = next();
        else if (a == "--template") tmplName = next();
        else if (a == "--name") name = next();
        else if (a == "--project") projectDir = next();
        else if (a == "--export") exportKind = next();
        else if (a == "--output") output = next();
    }
    if (create.empty() && exportKind.empty()) return -1;
    Log::addSink(std::make_shared<ConsoleSink>());
    if (!create.empty()) {
        struct Named {
            const char* name;
            SceneTemplate tmpl;
            ProjectMode mode;
        };
        static const Named templates[] = {{"empty2d", SceneTemplate::Empty2D, ProjectMode::Mode2D},
                                          {"empty25d", SceneTemplate::Empty25D, ProjectMode::Mode25D},
                                          {"empty3d", SceneTemplate::Empty3D, ProjectMode::Mode3D},
                                          {"showcase2d", SceneTemplate::Showcase2D, ProjectMode::Mode2D},
                                          {"showcase3d", SceneTemplate::Showcase3D, ProjectMode::Mode3D}};
        const Named* chosen = nullptr;
        for (const Named& t : templates)
            if (tmplName == t.name) chosen = &t;
        if (!chosen) {
            AGR_LOG_ERROR("Editor", "unknown template '{}'", tmplName);
            return 2;
        }
        ProjectSettings settings;
        settings.name = name.empty() ? fs::path(create).filename().string() : name;
        settings.mode = chosen->mode;
        auto project = Project::create(create, settings, chosen->tmpl, exeDir / "editor_resources" / "templates" / "assets");
        if (!project) {
            AGR_LOG_ERROR("Editor", "{}", project.error().message);
            return 1;
        }
        if (projectDir.empty()) projectDir = create;
    }
    if (exportKind.empty()) return 0;
    auto project = Project::open(projectDir);
    if (!project) {
        AGR_LOG_ERROR("Editor", "{}", project.error().message);
        return 1;
    }
    if (output.empty()) output = (fs::path(projectDir) / "build" / exportKind).string();
    Result<ExportReport> report = makeError(Error::Code::InvalidArgument,
                                            "unknown export '" + exportKind + "' (desktop, android-apk, android-release-apk, android-aab)");
#if defined(_WIN32)
    const fs::path player = exeDir / "agr_player.exe";
#else
    const fs::path player = exeDir / "agr_player";
#endif
    if (exportKind == "desktop") report = exportDesktop(project.value(), output, player);
    if (exportKind.rfind("android", 0) == 0) {
        AndroidBuildOptions o;
        o.kind = exportKind == "android-aab" ? AndroidBuildKind::ReleaseBundle
                 : exportKind == "android-release-apk" ? AndroidBuildKind::ReleaseApk
                                                       : AndroidBuildKind::DebugApk;
        if (const char* p = std::getenv("AGR_STORE_PASSWORD")) o.storePassword = p;
        if (const char* p = std::getenv("AGR_KEY_PASSWORD")) o.keyPassword = p;
        const AndroidToolchain tools = findAndroidToolchain(exeDir);
        report = buildAndroid(project.value(), tools, output, o, [](const std::string& line) { std::printf("%s\n", line.c_str()); });
    }
    if (!report) {
        AGR_LOG_ERROR("Export", "{}", report.error().message);
        return 1;
    }
    for (const std::string& w : report.value().warnings) AGR_LOG_WARN("Export", "{}", w);
    AGR_LOG_INFO("Export", "done: {}", report.value().output.string());
    return 0;
}

#if defined(AGR_PLATFORM_ANDROID)
/// The editor's resources (fonts, project templates) travel in the APK assets under
/// game/assets/editor_resources, listed in files.txt (first line: content version). The editor
/// works with real files: they are copied to internal storage once per version.
bool extractAndroidResources(const std::filesystem::path& dest) {
    namespace fs = std::filesystem;
    const std::string root = "game/assets/editor_resources/";
    size_t size = 0;
    void* data = SDL_LoadFile((root + "files.txt").c_str(), &size);
    if (!data) {
        SDL_Log("AGR editor: no packed resources (%s)", SDL_GetError());
        return fs::exists(dest / "fonts");
    }
    const std::string list(static_cast<const char*>(data), size);
    SDL_free(data);
    const std::size_t firstEnd = list.find('\n');
    const std::string version = list.substr(0, firstEnd);
    std::error_code ec;
    {
        std::ifstream stamp(dest / ".version");
        std::string current;
        std::getline(stamp, current);
        if (current == version && fs::exists(dest / "fonts", ec)) return true;
    }
    std::size_t pos = firstEnd == std::string::npos ? list.size() : firstEnd + 1, copied = 0;
    while (pos < list.size()) {
        std::size_t end = list.find('\n', pos);
        if (end == std::string::npos) end = list.size();
        const std::string rel = list.substr(pos, end - pos);
        pos = end + 1;
        if (rel.empty()) continue;
        void* bytes = SDL_LoadFile((root + rel).c_str(), &size);
        if (!bytes) {
            SDL_Log("AGR editor: missing packed resource %s", rel.c_str());
            continue;
        }
        const fs::path out = dest / rel;
        fs::create_directories(out.parent_path(), ec);
        std::ofstream(out, std::ios::binary).write(static_cast<const char*>(bytes), static_cast<std::streamsize>(size));
        SDL_free(bytes);
        ++copied;
    }
    std::ofstream(dest / ".version") << version << "\n";
    SDL_Log("AGR editor: %zu resource files extracted", copied);
    return copied > 0;
}
#endif

} // namespace

int main(int argc, char** argv) {
    using namespace agr;
    namespace fs = std::filesystem;
#if defined(AGR_PLATFORM_ANDROID)
    // Internal storage: resources extracted from the APK, projects, settings.
    fs::path exeDir;
    if (char* pref = SDL_GetPrefPath("Superessence Studio", "AGR Editor")) {
        exeDir = pref;
        SDL_free(pref);
    }
    if (!extractAndroidResources(exeDir / "editor_resources")) SDL_Log("AGR editor: resources could not be prepared");
    SDL_SetHint(SDL_HINT_ORIENTATIONS, "LandscapeLeft LandscapeRight");
    (void)argc;
    (void)argv;
#else
    const fs::path exeDir = fs::path(argc > 0 ? argv[0] : ".").parent_path();
    if (const int code = runCommandLine(argc, argv, exeDir); code >= 0) return code;
#endif

    EngineConfig config;
    config.appName = "AGR Editor";
    config.window.title = "AGR ENGINE";
    config.window.width = 1600;
    config.window.height = 900;
    config.pluginsDirectory = exeDir.string();
    config.assetsDirectory = (exeDir / "editor_resources" / "templates" / "assets").string();
    editor::EditorOptions options;
    options.resourcesDir = (exeDir / "editor_resources").string();
    for (int i = 1; i < argc; ++i) {
        const std::string a = argv[i];
        if (a == "--vulkan") config.window.graphics = GraphicsApi::Vulkan;
        else if (a == "--opengl") config.window.graphics = GraphicsApi::OpenGL;
        else if (a == "--gles") config.window.graphics = GraphicsApi::OpenGLES;
        else if (a == "--validation") config.window.graphicsValidation = true;
        else if (a == "--smoke-test" && i + 1 < argc) options.smokeTestDir = argv[++i];
        else if (a == "--hub-screenshot" && i + 1 < argc) options.hubScreenshot = argv[++i];
        else if (a == "--template-gallery" && i + 1 < argc) options.galleryDir = argv[++i];
        else if (!a.empty() && a[0] != '-') options.projectPath = a;
    }
    // User data (settings, scratch) lives in the per-user preference folder.
    if (char* pref = SDL_GetPrefPath("Superessence Studio", "AGR Editor")) {
        config.userDirectory = std::string(pref) + "user";
        SDL_free(pref);
    }
    if (!options.galleryDir.empty()) {
        const fs::path previous = fs::path(options.galleryDir) / "GalleryProject";
        std::error_code ec;
        if (fs::exists(previous / "project.agrproject", ec)) fs::remove_all(previous, ec);
        fs::remove_all(fs::path(options.galleryDir) / "settings", ec);  // default window layout
        config.userDirectory = (fs::path(options.galleryDir) / "user").string();
    }
    if (!options.smokeTestDir.empty()) {
        // Re-runs start clean, but only a folder this test created is ever removed.
        const fs::path previous = fs::path(options.smokeTestDir) / "SmokeProject";
        std::error_code ec;
        if (fs::exists(previous / "project.agrproject", ec)) fs::remove_all(previous, ec);
        fs::remove_all(fs::path(options.smokeTestDir) / "settings", ec);
        config.userDirectory = (fs::path(options.smokeTestDir) / "user").string();
        config.window.graphicsValidation = true;
    }

    Engine engine(config);
    engine.plugins().addStatic(std::make_unique<RenderPlugin>());
    engine.plugins().addStatic(std::make_unique<TwoDPlugin>());
    engine.plugins().addStatic(std::make_unique<UIPlugin>());  // after RenderPlugin (draws over its views), before scripts (input first)
    engine.plugins().addStatic(std::make_unique<ScriptPlugin>());  // before physics: scripts move bodies, then physics steps
    engine.plugins().addStatic(std::make_unique<AnimationPlugin>());  // after scripts: they set animator parameters
    engine.plugins().addStatic(std::make_unique<NetPlugin>());
    engine.plugins().addStatic(std::make_unique<NavigationPlugin>());  // after scripts (destinations), before physics
    engine.plugins().addStatic(std::make_unique<Physics2DPlugin>());
    engine.plugins().addStatic(std::make_unique<Physics3DPlugin>());
    engine.plugins().addStatic(std::make_unique<AudioPlugin>());
    auto platform = std::make_unique<SdlPlatform>();
    SdlPlatform* window = platform.get();
    if (Status s = engine.initialize(std::move(platform)); !s) {
        // No display: the scripted test is "skipped" for CTest unless CI requires GPUs.
        if (!options.smokeTestDir.empty() && !std::getenv("AGR_REQUIRE_GPU_TESTS")) return 77;
        return 1;
    }

    editor::EditorApp app(window, options);
    const int code = engine.run(app);
    return code != 0 ? code : app.exitCode();
}
