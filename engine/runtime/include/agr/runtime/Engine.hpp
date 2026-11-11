#pragma once
#include <functional>
#include <memory>
#include <string>
#include <unordered_map>

#include "agr/core/EventBus.hpp"
#include "agr/core/FileSystem.hpp"
#include "agr/core/JobSystem.hpp"
#include "agr/core/Log.hpp"
#include "agr/core/Time.hpp"
#include "agr/runtime/Application.hpp"
#include "agr/runtime/InputActions.hpp"
#include "agr/runtime/Localization.hpp"
#include "agr/runtime/SaveStore.hpp"
#include "agr/scene/Prefab.hpp"
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
    /// Automated tests and tools: starts `app` as run() does, without entering the loop;
    /// then each advance() runs one frame of `delta` seconds (fixed steps, updates, render)
    /// with the input set since the previous frame. The engine shuts down when destroyed.
    Status start(Application& app);
    void advance(Application& app, f64 delta);
    void requestQuit() { m_quitRequested = true; }
    /// Called when the user closes the window. Return false to keep running (e.g. the
    /// editor asks to save first, then calls requestQuit()).
    void setCloseRequestHandler(std::function<bool()> handler) { m_closeHandler = std::move(handler); }
    bool quitRequested() const { return m_quitRequested; }

    // ------------------------------------------------------------- services
    const EngineConfig& config() const { return m_config; }
    JobSystem& jobs() { return *m_jobs; }
    EventBus& events() { return m_events; }
    VirtualFileSystem& vfs() { return m_vfs; }
    /// Local save (user://save.json), written when the app goes to the background and at shutdown.
    SaveStore& save() { return *m_save; }
    /// Prefab files (.agrprefab); also the resolver scenes use to expand instances.
    PrefabLibrary& prefabs() { return *m_prefabs; }
    PluginManager& plugins() { return m_plugins; }
    Input& input() { return m_input; }
    /// Named actions ("jump", "move_x"), evaluated every frame from assets://input.agrinput.
    InputActions& actions() { return m_actions; }
    /// Translated texts (assets://localization.csv), language chosen at start from the saved
    /// choice or the system languages.
    Localization& localization() { return m_localization; }
    /// Changes the language and remembers the choice in the local save. False if unknown.
    bool setLanguage(std::string_view code);
    /// Reloads input actions and texts (after the asset folder changed: editor projects).
    void reloadGameSettings();
    PlatformLayer& platform() { return *m_platform; }
    Scene& scene() { return *m_scene; }
    const FrameTime& time() const { return m_time; }
    const FrameStats& stats() const { return m_stats; }
    const std::shared_ptr<RingBufferSink>& consoleLog() const { return m_consoleSink; }

    /// Frame cap (0 = uncapped / vsync). Graphics profiles lower it on weak devices.
    void setTargetFps(u32 fps) { m_config.targetFps = fps; }
    void setTimeScale(f64 scale) { m_time.timeScale = scale < 0.0 ? 0.0 : scale; }

    // -------------------------------------------------------------- services
    /// Publishes a subsystem owned elsewhere (renderer, asset manager, physics...)
    /// so gameplay code and plugins can find it. Keyed by type name: works across plugins.
    template <typename T>
    void registerService(T* service) {
        if (service) m_services[typeHash<T>()] = service;
        else m_services.erase(typeHash<T>());
    }
    template <typename T>
    T* service() const {
        auto it = m_services.find(typeHash<T>());
        return it == m_services.end() ? nullptr : static_cast<T*>(it->second);
    }

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
    std::unique_ptr<SaveStore> m_save;
    std::unique_ptr<PrefabLibrary> m_prefabs;
    Input m_input;
    InputActions m_actions;
    Localization m_localization;
    std::unique_ptr<PlatformLayer> m_platform;
    std::unique_ptr<Scene> m_scene;
    PluginManager m_plugins;
    FrameTime m_time;
    FrameStats m_stats;
    std::unordered_map<u64, void*> m_services;
    std::function<bool()> m_closeHandler;
    f64 m_accumulator = 0.0;
    f64 m_statsWindowTime = 0.0;
    u32 m_statsWindowFrames = 0;
    f64 m_statsWindowWorst = 0.0;
    bool m_initialized = false;
    bool m_quitRequested = false;
};

} // namespace agr
