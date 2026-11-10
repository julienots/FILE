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
#include "agr/render/RenderPlugin.hpp"
#include "agr/runtime/Engine.hpp"

int main(int argc, char** argv) {
    using namespace agr;
    namespace fs = std::filesystem;
    const fs::path exeDir = fs::path(argc > 0 ? argv[0] : ".").parent_path();

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
