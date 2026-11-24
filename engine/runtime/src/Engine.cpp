#include "agr/runtime/Engine.hpp"
#include "agr/core/Profiler.hpp"

#include <atomic>
#include <utility>

#include <algorithm>
#include <filesystem>
#include <thread>

#include "agr/core/Memory.hpp"
#include "agr/core/Platform.hpp"
#include "agr/runtime/Events.hpp"
#include "agr/scene/SceneSerializer.hpp"

namespace agr {

Engine::Engine(EngineConfig config) : m_config(std::move(config)) {}

Engine::~Engine() {
    m_asyncLoad.reset();  // joins a background scene load before the file system goes away
    shutdown(nullptr);
}

Status Engine::initialize(std::unique_ptr<PlatformLayer> platform) {
    if (m_initialized) return makeError(Error::Code::AlreadyExists, "engine already initialized");

    // 1. Logging: console (default) + in-memory ring for the editor console.
    m_consoleSink = std::make_shared<RingBufferSink>(4096);
    Log::addSink(std::make_shared<ConsoleSink>());
    Log::addSink(m_consoleSink);
    if (!m_config.logFile.empty()) {
        m_fileSink = std::make_shared<FileSink>(m_config.logFile);
        if (m_fileSink->isOpen()) Log::addSink(m_fileSink);
    }
    AGR_LOG_INFO("Engine", "{} {}.{}.{} - {} | {} | {}", kEngineName, AGR_VERSION_MAJOR, AGR_VERSION_MINOR,
                 AGR_VERSION_PATCH, kEngineSlogan, kStudioName, kPlatformName);

    // 2. Jobs.
    m_jobs = std::make_unique<JobSystem>(m_config.workerThreads);
    AGR_LOG_INFO("Engine", "job system: {} worker thread(s)", m_jobs->workerCount());

    // 3. Virtual file system.
    m_vfs.mount("assets", std::make_unique<NativeFileProvider>(m_config.assetsDirectory, true));
    {
        std::error_code ec;
        std::filesystem::create_directories(m_config.userDirectory, ec);
        m_vfs.mount("user", std::make_unique<NativeFileProvider>(m_config.userDirectory, false));
    }
    m_save = std::make_unique<SaveStore>(m_vfs, "user://save.json");
    m_prefabs = std::make_unique<PrefabLibrary>(m_vfs);
    m_prefabs->install();

    // 4. Platform.
    if (m_config.headless || !platform) platform = std::make_unique<HeadlessPlatform>();
    m_platform = std::move(platform);
    if (Status s = m_platform->initialize(m_config); !s) {
        AGR_LOG_ERROR("Engine", "platform '{}' failed to initialize: {}", m_platform->name(), s.error().message);
        m_platform.reset();
        return s;
    }
    AGR_LOG_INFO("Engine", "platform layer: {}", m_platform->name());
    reloadGameSettings();

    // 5. Scene.
    m_scene = std::make_unique<Scene>("Untitled");

    // 6. Engine-level event wiring.
    m_events.subscribe<WindowCloseRequestedEvent>([this](const WindowCloseRequestedEvent&) {
        if (!m_closeHandler || m_closeHandler()) requestQuit();
    });
    m_events.subscribe<AppLifecycleEvent>([this](const AppLifecycleEvent& e) {
        if (e.state == LifecycleState::DidEnterBackground) m_input.reset();
        // The OS may kill a background app without notice: write the save now.
        if (e.state == LifecycleState::WillEnterBackground || e.state == LifecycleState::Terminating)
            if (Status s = m_save->flush(); !s) AGR_LOG_ERROR("Engine", "save: {}", s.error().message);
        if (e.state == LifecycleState::Terminating) requestQuit();
    });

    // 7. Plugins (dynamic plugins next to the executable or in the configured folder).
    if (!m_config.pluginsDirectory.empty()) m_plugins.scanDirectory(m_config.pluginsDirectory);
    if (Status s = m_plugins.loadAll(*this); !s) AGR_LOG_ERROR("Engine", "plugin loading: {}", s.error().message);

    m_time.fixedDeltaSeconds = m_config.fixedTimestep;
    m_initialized = true;
    return {};
}

void Engine::setScene(std::unique_ptr<Scene> scene) {
    if (scene) m_scene = std::move(scene);
}

Status Engine::loadScene(std::string_view uri) {
    auto text = m_vfs.readText(uri);
    if (!text) return text.error();
    auto scene = std::make_unique<Scene>();
    // Text (.agrscene as saved by the editor) or binary (exported games): detected from the content.
    if (Status s = SceneSerializer::fromBytes(*scene, text.value()); !s) return s;
    AGR_LOG_INFO("Engine", "scene '{}' loaded from {} ({} entities)", scene->name(), uri, scene->entityCount());
    m_scene = std::move(scene);
    m_sceneUri = std::string(uri);
    return {};
}

struct Engine::AsyncSceneLoad {
    std::string uri;
    bool switchWhenReady = true;
    std::atomic<float> progress{0.0f};
    std::atomic<bool> done{false};
    std::unique_ptr<Scene> scene;  // written by the worker before `done`
    std::string error;
    bool reported = false;  // a failure is logged once
    std::thread worker;
    ~AsyncSceneLoad() {
        if (worker.joinable()) worker.join();
    }
};

void Engine::beginSceneLoad(std::string uri, bool switchWhenReady) {
    m_switchLoaded = false;
    auto load = std::make_shared<AsyncSceneLoad>();
    load->uri = std::move(uri);
    load->switchWhenReady = switchWhenReady;
    AsyncSceneLoad* l = load.get();
    VirtualFileSystem* vfs = &m_vfs;
    load->worker = std::thread([l, vfs] {
        l->progress = 0.1f;
        auto text = vfs->readText(l->uri);
        if (!text) {
            l->error = text.error().message;
        } else {
            l->progress = 0.5f;
            auto scene = std::make_unique<Scene>();
            if (Status s = SceneSerializer::fromBytes(*scene, text.value()); !s) l->error = s.error().message;
            else l->scene = std::move(scene);
        }
        l->progress = 1.0f;
        l->done = true;
    });
    m_asyncLoad = std::move(load);  // a previous load is joined and dropped
}

Engine::SceneLoadState Engine::sceneLoadState() const {
    SceneLoadState s;
    if (!m_asyncLoad) return s;
    s.uri = m_asyncLoad->uri;
    s.progress = m_asyncLoad->progress.load();
    const bool done = m_asyncLoad->done.load();
    s.loading = !done;
    s.ready = done && m_asyncLoad->scene != nullptr;
    s.failed = done && !m_asyncLoad->scene;
    if (s.failed) s.error = m_asyncLoad->error;
    return s;
}

void Engine::finishSceneLoad() {
    if (!m_asyncLoad || !m_asyncLoad->done.load()) return;
    if (!m_asyncLoad->scene) {  // failed: reported once, the state stays readable
        if (!m_asyncLoad->reported) {
            AGR_LOG_ERROR("Engine", "cannot load scene '{}': {}", m_asyncLoad->uri, m_asyncLoad->error);
            m_asyncLoad->reported = true;
        }
        return;
    }
    if (!m_asyncLoad->switchWhenReady && !m_switchLoaded) return;
    if (m_asyncLoad->worker.joinable()) m_asyncLoad->worker.join();
    AGR_LOG_INFO("Engine", "scene '{}' loaded in the background from {} ({} entities)", m_asyncLoad->scene->name(), m_asyncLoad->uri,
                 m_asyncLoad->scene->entityCount());
    m_scene = std::move(m_asyncLoad->scene);
    m_sceneUri = m_asyncLoad->uri;
    m_asyncLoad.reset();
    m_switchLoaded = false;
}

Status Engine::saveScene(std::string_view uri) {
    return m_vfs.writeText(uri, SceneSerializer::toString(*m_scene));
}

int Engine::run(Application& app) {
    if (!m_initialized) {
        AGR_LOG_ERROR("Engine", "run() called before a successful initialize()");
        return 1;
    }
    if (m_vfs.mountGeneration() != m_settingsMounts) reloadGameSettings();  // before the game's first scene
    if (Status s = app.onStart(*this); !s) {
        AGR_LOG_ERROR("Engine", "application failed to start: {}", s.error().message);
        shutdown(&app);
        return 1;
    }
    m_events.publish(EngineStartedEvent{});

    Stopwatch clock;
    f64 last = clock.elapsedSeconds();
    while (!m_quitRequested) {
        const f64 frameStart = clock.elapsedSeconds();
        const f64 rawDelta = frameStart - last;
        last = frameStart;

        Profiler::instance().endFrame();  // closes the previous frame (rendering and waits included)
        Profiler::instance().beginFrame();
        Stopwatch cpu;
        m_input.beginFrame();
        if (!m_platform->pollEvents(m_events, m_input)) m_quitRequested = true;
        m_gestures.update(m_input, rawDelta);
        m_actions.update(m_input, m_platform->windowSize());
        m_events.dispatchQueued();
        tick(app, rawDelta);
        m_stats.cpuMs = cpu.elapsedMilliseconds();

        if (m_platform->canRender()) {
            AGR_PROFILE_SCOPE("Rendu");
            m_plugins.render(*this, m_time);
            app.onRender(*this, m_time);
            AGR_PROFILE_SCOPE("Présentation (attente écran)");
            m_platform->present();
        } else if (!m_config.headless) {
            // Backgrounded on mobile: don't spin the CPU.
            std::this_thread::sleep_for(std::chrono::milliseconds(16));
        }

        if (m_config.targetFps > 0) {
            const f64 budget = 1.0 / m_config.targetFps;
            const f64 spent = clock.elapsedSeconds() - frameStart;
            if (spent < budget) std::this_thread::sleep_for(std::chrono::duration<f64>(budget - spent));
        }
        if (m_config.maxFrames > 0 && m_time.frameIndex >= m_config.maxFrames) m_quitRequested = true;
    }

    m_events.publish(EngineStoppingEvent{});
    shutdown(&app);
    return 0;
}

Status Engine::start(Application& app) {
    if (!m_initialized) return makeError(Error::Code::InvalidArgument, "start() called before a successful initialize()");
    if (Status s = app.onStart(*this); !s) return s;
    m_events.publish(EngineStartedEvent{});
    return {};
}

void Engine::advance(Application& app, f64 delta) {
    Profiler::instance().beginFrame();
    m_gestures.update(m_input, delta);
    m_actions.update(m_input, m_platform->windowSize());
    m_events.dispatchQueued();
    tick(app, delta);
    if (m_platform->canRender()) {
        AGR_PROFILE_SCOPE("Rendu");
        m_plugins.render(*this, m_time);
        app.onRender(*this, m_time);
        m_platform->present();
    }
    Profiler::instance().endFrame();
    m_input.beginFrame();  // this frame's presses become "held" for the next one
}

void Engine::tick(Application& app, f64 rawDelta) {
    // Files mounted (or unmounted) since the game settings were read: read them again
    // (a host that mounts the game after initialize(), a mod folder...).
    if (m_vfs.mountGeneration() != m_settingsMounts) reloadGameSettings();
    const f64 delta = std::clamp(rawDelta, 0.0, m_config.maxFrameDelta);
    m_time.unscaledDeltaSeconds = delta;
    m_time.deltaSeconds = delta * m_time.timeScale;
    m_time.totalSeconds += m_time.deltaSeconds;

    // Fixed timestep with a bounded number of catch-up steps.
    m_accumulator += m_time.deltaSeconds;
    u32 steps = 0;
    Profiler::instance().push("Pas fixes");
    while (m_accumulator >= m_config.fixedTimestep && steps < m_config.maxFixedStepsPerFrame) {
        m_plugins.fixedUpdate(*this, m_time);
        {
            AGR_PROFILE_SCOPE("Application");
            app.onFixedUpdate(*this, m_time);
        }
        m_accumulator -= m_config.fixedTimestep;
        ++steps;
    }
    Profiler::instance().pop();
    if (steps == m_config.maxFixedStepsPerFrame && m_accumulator >= m_config.fixedTimestep)
        m_accumulator = 0.0;  // drop the backlog instead of spiralling
    m_time.alpha = static_cast<f32>(m_accumulator / m_config.fixedTimestep);

    {
        AGR_PROFILE_SCOPE("Mise à jour");
        m_plugins.update(*this, m_time);
        AGR_PROFILE_SCOPE("Application");
        app.onUpdate(*this, m_time);
    }
    m_scene->flushDestroyed();
    if (!m_pendingScene.empty()) {
        const std::string uri = std::exchange(m_pendingScene, {});
        if (Status s = loadScene(uri); !s) AGR_LOG_ERROR("Engine", "cannot load scene '{}': {}", uri, s.error().message);
    }
    finishSceneLoad();
    {
        AGR_PROFILE_SCOPE("Transformations");
        m_scene->updateTransforms();
    }

    ++m_time.frameIndex;

    // Statistics.
    m_stats.frameMs = rawDelta * 1000.0;
    m_stats.fixedSteps = steps;
    m_stats.entityCount = m_scene->entityCount();
    m_stats.trackedMemoryBytes = MemoryStats::totalCurrentBytes();
    m_statsWindowTime += rawDelta;
    ++m_statsWindowFrames;
    m_statsWindowWorst = std::max(m_statsWindowWorst, m_stats.frameMs);
    if (m_statsWindowTime >= 0.5) {
        m_stats.fps = m_statsWindowFrames / m_statsWindowTime;
        m_stats.worstFrameMs = m_statsWindowWorst;
        m_statsWindowTime = 0.0;
        m_statsWindowFrames = 0;
        m_statsWindowWorst = 0.0;
    }
}

void Engine::reloadGameSettings() {
    m_settingsMounts = m_vfs.mountGeneration();
    if (Status s = m_actions.load(m_vfs); !s) {
        AGR_LOG_ERROR("Engine", "input actions: {} (defaults used)", s.error().message);
        m_actions.setMap(InputMap::defaults());
    }
    // The player's remapping (agr.input.saveBindings), kept in the save over the game's file.
    if (m_save && m_save->has("agr.input")) m_actions.applyOverrides(nlohmann::ordered_json::parse(m_save->get("agr.input").dump()));
    if (Status s = m_localization.load(m_vfs); !s) AGR_LOG_ERROR("Engine", "{}", s.error().message);
    std::vector<std::string> preferred;
    if (m_save && m_save->has("agr.language") && m_save->get("agr.language").is_string())
        preferred.push_back(m_save->get("agr.language").get<std::string>());
    if (m_platform)
        for (auto& l : m_platform->preferredLanguages()) preferred.push_back(std::move(l));
    m_localization.chooseLanguage(preferred);
    if (!m_localization.languages().empty())
        AGR_LOG_INFO("Engine", "language: {} ({} available)", m_localization.language(), m_localization.languages().size());
}

bool Engine::setLanguage(std::string_view code) {
    if (!m_localization.setLanguage(code)) return false;
    if (m_save) m_save->set("agr.language", m_localization.language());
    return true;
}

void Engine::shutdown(Application* app) {
    m_asyncLoad.reset();
    if (!m_initialized) return;
    if (app) app->onShutdown(*this);
    m_plugins.unloadAll(*this);
    if (m_save)
        if (Status s = m_save->flush(); !s) AGR_LOG_ERROR("Engine", "save: {}", s.error().message);
    m_scene.reset();
    if (m_prefabs) {
        setPrefabResolver(nullptr);
        m_prefabs.reset();
    }
    if (m_platform) m_platform->shutdown();
    m_platform.reset();
    m_jobs.reset();
    AGR_LOG_INFO("Engine", "shutdown complete");
    Log::flush();
    Log::clearSinks();
    m_initialized = false;
}

} // namespace agr
