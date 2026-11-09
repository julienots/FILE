#pragma once
#include <memory>
#include <string>

#include "agr/core/EventBus.hpp"
#include "agr/core/FileSystem.hpp"
#include "agr/core/JobSystem.hpp"
#include "agr/core/Log.hpp"
#include "agr/core/Time.hpp"
#include "agr/runtime/Application.hpp"
#include "agr/runtime/EngineConfig.hpp"
#include "agr/runtime/Input.hpp"
#include "agr/runtime/PlatformLayer.hpp"
#include "agr/runtime/PluginManager.hpp"
#include "agr/scene/Scene.hpp"

namespace agr {

/// Rolling performance numbers (fed to the profiler overlay in later phases).
struct FrameStats {
    f64 fps = 0.0;            ///< Smoothed over ~0.5 s.
    f64 frameMs = 0.0;        ///< Wall time of the last frame.
    f64 cpuMs = 0.0;          ///< Engine work of the last frame (excludes vsync / limiter wait).
    f64 worstFrameMs = 0.0;   ///< Worst frame of the last sampling window.
    u32 fixedSteps = 0;       ///< Fixed updates executed during the last frame.
    usize entityCount = 0;
    usize trackedMemoryBytes = 0;
};

/// Owns every engine service and drives the main loop.
///
/// Startup order : log -> jobs -> VFS -> platform -> scene -> plugins -> app
/// Shutdown order: app -> plugins -> scene -> platform -> jobs -> log
class Engine {
public:
    explicit Engine(EngineConfig config);
    ~Engine();
    Engine(const Engine&) = delete;
    Engine& operator=(const Engine&) = delete;

    /// Brings up services. `platform` = nullptr selects HeadlessPlatform.
    Status initialize(std::unique_ptr<PlatformLayer> platform);
    /// Runs the main loop until quit. Returns the process exit code.
    int run(Application& app);
    void requestQuit() { m_quitRequested = true; }
    bool quitRequested() const { return m_quitRequested; }

    // ------------------------------------------------------------- services
    const EngineConfig& config() const { return m_config; }
    JobSystem& jobs() { return *m_jobs; }
    EventBus& events() { return m_events; }
    VirtualFileSystem& vfs() { return m_vfs; }
    PluginManager& plugins() { return m_plugins; }
    Input& input() { return m_input; }
    PlatformLayer& platform() { return *m_platform; }
    Scene& scene() { return *m_scene; }
    const FrameTime& time() const { return m_time; }
    const FrameStats& stats() const { return m_stats; }
    const std::shared_ptr<RingBufferSink>& consoleLog() const { return m_consoleSink; }

    void setTimeScale(f64 scale) { m_time.timeScale = scale < 0.0 ? 0.0 : scale; }

    // ---------------------------------------------------------------- scenes
    /// Replaces the active scene.
    void setScene(std::unique_ptr<Scene> scene);
    Status loadScene(std::string_view uri);
    Status saveScene(std::string_view uri);

private:
    void tick(Application& app, f64 rawDelta);
    void shutdown(Application* app);

    EngineConfig m_config;
    std::shared_ptr<RingBufferSink> m_consoleSink;
    std::shared_ptr<FileSink> m_fileSink;
    std::unique_ptr<JobSystem> m_jobs;
    EventBus m_events;
    VirtualFileSystem m_vfs;
    Input m_input;
    std::unique_ptr<PlatformLayer> m_platform;
    std::unique_ptr<Scene> m_scene;
    PluginManager m_plugins;
    FrameTime m_time;
    FrameStats m_stats;
    f64 m_accumulator = 0.0;
    f64 m_statsWindowTime = 0.0;
    u32 m_statsWindowFrames = 0;
    f64 m_statsWindowWorst = 0.0;
    bool m_initialized = false;
    bool m_quitRequested = false;
};

} // namespace agr
