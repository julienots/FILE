#include "agr/runtime/Engine.hpp"

#include <algorithm>
#include <filesystem>
#include <thread>

#include "agr/core/Memory.hpp"
#include "agr/core/Platform.hpp"
#include "agr/runtime/Events.hpp"
#include "agr/scene/SceneSerializer.hpp"

namespace agr {

Engine::Engine(EngineConfig config) : m_config(std::move(config)) {}

Engine::~Engine() { shutdown(nullptr); }

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

    // 4. Platform.
    if (m_config.headless || !platform) platform = std::make_unique<HeadlessPlatform>();
    m_platform = std::move(platform);
    if (Status s = m_platform->initialize(m_config); !s) {
        AGR_LOG_ERROR("Engine", "platform '{}' failed to initialize: {}", m_platform->name(), s.error().message);
        m_platform.reset();
        return s;
    }
    AGR_LOG_INFO("Engine", "platform layer: {}", m_platform->name());

    // 5. Scene.
    m_scene = std::make_unique<Scene>("Untitled");

    // 6. Engine-level event wiring.
    m_events.subscribe<WindowCloseRequestedEvent>([this](const WindowCloseRequestedEvent&) { requestQuit(); });
    m_events.subscribe<AppLifecycleEvent>([this](const AppLifecycleEvent& e) {
        if (e.state == LifecycleState::DidEnterBackground) m_input.reset();
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
    if (Status s = SceneSerializer::fromString(*scene, text.value()); !s) return s;
    AGR_LOG_INFO("Engine", "scene '{}' loaded from {} ({} entities)", scene->name(), uri, scene->entityCount());
    m_scene = std::move(scene);
    return {};
}

Status Engine::saveScene(std::string_view uri) {
    return m_vfs.writeText(uri, SceneSerializer::toString(*m_scene));
}

int Engine::run(Application& app) {
    if (!m_initialized) {
        AGR_LOG_ERROR("Engine", "run() called before a successful initialize()");
        return 1;
    }
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

        Stopwatch cpu;
        m_input.beginFrame();
        if (!m_platform->pollEvents(m_events, m_input)) m_quitRequested = true;
        m_events.dispatchQueued();
        tick(app, rawDelta);
        m_stats.cpuMs = cpu.elapsedMilliseconds();

        if (m_platform->canRender()) {
            app.onRender(*this, m_time);
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

void Engine::tick(Application& app, f64 rawDelta) {
    const f64 delta = std::clamp(rawDelta, 0.0, m_config.maxFrameDelta);
    m_time.unscaledDeltaSeconds = delta;
    m_time.deltaSeconds = delta * m_time.timeScale;
    m_time.totalSeconds += m_time.deltaSeconds;

    // Fixed timestep with a bounded number of catch-up steps.
    m_accumulator += m_time.deltaSeconds;
    u32 steps = 0;
    while (m_accumulator >= m_config.fixedTimestep && steps < m_config.maxFixedStepsPerFrame) {
        m_plugins.fixedUpdate(*this, m_time);
        app.onFixedUpdate(*this, m_time);
        m_accumulator -= m_config.fixedTimestep;
        ++steps;
    }
    if (steps == m_config.maxFixedStepsPerFrame && m_accumulator >= m_config.fixedTimestep)
        m_accumulator = 0.0;  // drop the backlog instead of spiralling
    m_time.alpha = static_cast<f32>(m_accumulator / m_config.fixedTimestep);

    m_plugins.update(*this, m_time);
    app.onUpdate(*this, m_time);
    m_scene->flushDestroyed();
    m_scene->updateTransforms();

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

void Engine::shutdown(Application* app) {
    if (!m_initialized) return;
    if (app) app->onShutdown(*this);
    m_plugins.unloadAll(*this);
    m_scene.reset();
    if (m_platform) m_platform->shutdown();
    m_platform.reset();
    m_jobs.reset();
    AGR_LOG_INFO("Engine", "shutdown complete");
    Log::flush();
    Log::clearSinks();
    m_initialized = false;
}

} // namespace agr
