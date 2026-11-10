// AGR Editor — entry point.
//
// Usage: agr_editor [PROJECT_DIR] [--vulkan | --opengl | --gles] [--validation]
//                   [--smoke-test DIR] [--hub-screenshot FILE.png]
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <string>

#include <SDL3/SDL.h>

#include "agr/editor/ui/EditorApp.hpp"
#include "agr/platform/SdlPlatform.hpp"
#include "agr/animation/AnimationPlugin.hpp"
#include "agr/audio/AudioPlugin.hpp"
#include "agr/physics2d/Physics2DPlugin.hpp"
#include "agr/physics3d/Physics3DPlugin.hpp"
#include "agr/render/RenderPlugin.hpp"
#include "agr/script/ScriptPlugin.hpp"
#include "agr/ui/UIPlugin.hpp"
#include "agr/runtime/Engine.hpp"
#include "agr/editor/GameExport.hpp"

namespace {

/// Command-line tasks without a window (continuous integration, scripts):
///   agr_editor --create-project DIR [--template showcase2d|showcase3d|empty2d|empty25d|empty3d] [--name NAME]
///   agr_editor --project DIR --export desktop --output DIR
/// Returns -1 when no command-line task was given (the editor opens).
int runCommandLine(int argc, char** argv, const std::filesystem::path& exeDir) {
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
    Result<ExportReport> report = makeError(Error::Code::InvalidArgument, "unknown export '" + exportKind + "' (desktop)");
#if defined(_WIN32)
    const fs::path player = exeDir / "agr_player.exe";
#else
    const fs::path player = exeDir / "agr_player";
#endif
    if (exportKind == "desktop") report = exportDesktop(project.value(), output, player);
    if (!report) {
        AGR_LOG_ERROR("Export", "{}", report.error().message);
        return 1;
    }
    for (const std::string& w : report.value().warnings) AGR_LOG_WARN("Export", "{}", w);
    AGR_LOG_INFO("Export", "done: {}", report.value().output.string());
    return 0;
}

} // namespace

int main(int argc, char** argv) {
    using namespace agr;
    namespace fs = std::filesystem;
    const fs::path exeDir = fs::path(argc > 0 ? argv[0] : ".").parent_path();
    if (const int code = runCommandLine(argc, argv, exeDir); code >= 0) return code;

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
        else if (!a.empty() && a[0] != '-') options.projectPath = a;
    }
    // User data (settings, scratch) lives in the per-user preference folder.
    if (char* pref = SDL_GetPrefPath("Superessence Studio", "AGR Editor")) {
        config.userDirectory = std::string(pref) + "user";
        SDL_free(pref);
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
