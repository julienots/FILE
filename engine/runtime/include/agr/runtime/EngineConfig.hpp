#pragma once
#include <string>

#include "agr/core/Result.hpp"
#include "agr/core/Types.hpp"

namespace agr {

enum class GraphicsApi : u8 { None, OpenGL };

struct WindowConfig {
    std::string title = "AGR ENGINE";
    i32 width = 1280;
    i32 height = 720;
    bool resizable = true;
    bool fullscreen = false;   ///< Always true on Android.
    bool vsync = true;
    bool highDpi = true;
    GraphicsApi graphics = GraphicsApi::OpenGL;
};

/// Startup configuration. Loaded from "engine.json" in the project, or built in code.
struct EngineConfig {
    std::string appName = "AGR Application";
    std::string organization = "Superessence Studio";
    WindowConfig window;
    bool headless = false;           ///< No window: servers, tests, CI.
    f64 fixedTimestep = 1.0 / 60.0;  ///< Physics / gameplay tick.
    u32 maxFixedStepsPerFrame = 5;   ///< Avoids the "spiral of death" on slow devices.
    f64 maxFrameDelta = 0.25;        ///< Clamp after hitches (debugger, app resume).
    u32 targetFps = 0;               ///< 0 = uncapped / vsync. 30 saves battery on mobile.
    u32 workerThreads = 0;           ///< 0 = automatic.
    u64 maxFrames = 0;               ///< Quit after N frames (0 = run until closed).
    std::string assetsDirectory = "assets";  ///< Mounted as assets:// (read-only).
    std::string userDirectory = "user";      ///< Mounted as user:// (saves, settings).
    std::string pluginsDirectory = "";       ///< Dynamic plugins folder ("" = next to the executable).
    std::string logFile = "";                ///< Optional log file path.

    static Result<EngineConfig> fromJson(std::string_view text);
    std::string toJson() const;
};

} // namespace agr
