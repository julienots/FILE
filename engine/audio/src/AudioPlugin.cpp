#include "agr/audio/AudioPlugin.hpp"

#include "agr/core/Log.hpp"
#include "agr/runtime/Engine.hpp"
#include "agr/runtime/Events.hpp"

namespace agr {

PluginInfo AudioPlugin::info() const {
    return {"Audio", "1.0", "Audio mixer, sound sources and spatial audio (miniaudio)", "Superessence Studio", {}};
}

Status AudioPlugin::onLoad(Engine& engine) {
    registerAudioComponents();
    m_audio = std::make_unique<AudioSystem>(engine.vfs());
    if (Status s = m_audio->initialize(m_config); !s) {
        // The game still runs without sound.
        AGR_LOG_ERROR("Audio", "audio disabled: {}", s.error().message);
    }
    engine.registerService<AudioPlugin>(this);
    m_lifecycle = engine.events().subscribe<AppLifecycleEvent>([this](const AppLifecycleEvent& e) {
        if (!m_audio) return;
        // Mobile: release the audio output in the background, take it back in the foreground.
        if (e.state == LifecycleState::WillEnterBackground) m_audio->suspend(true);
        else if (e.state == LifecycleState::DidEnterForeground) m_audio->suspend(false);
    });
    return {};
}

void AudioPlugin::onUnload(Engine& engine) {
    engine.events().unsubscribe(m_lifecycle);
    engine.registerService<AudioPlugin>(nullptr);
    m_audio.reset();
}

void AudioPlugin::setRunning(bool running) {
    if (m_running == running) return;
    m_running = running;
    if (!running && m_audio) m_audio->resetScene();
}

void AudioPlugin::setPaused(bool paused) {
    if (m_paused == paused) return;
    m_paused = paused;
    if (m_audio) m_audio->pauseSources(paused);
}

void AudioPlugin::onUpdate(Engine& engine, const FrameTime& time) {
    if (!m_audio) return;
    m_audio->updateScene(engine.scene(), m_running);
    m_audio->update(time.unscaledDeltaSeconds);
}

} // namespace agr
