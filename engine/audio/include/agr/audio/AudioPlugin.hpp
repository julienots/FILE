#pragma once
#include "agr/audio/AudioSystem.hpp"
#include "agr/core/EventBus.hpp"
#include "agr/runtime/Plugin.hpp"

namespace agr {

/// Audio output and the AudioSource / AudioListener components. Static plugin:
///     engine.plugins().addStatic(std::make_unique<AudioPlugin>());
/// Service: engine.service<AudioPlugin>()->audio(). Sources play while the game
/// runs (always in a game; in the editor, in Play mode: setRunning).
class AudioPlugin final : public Plugin {
public:
    explicit AudioPlugin(AudioConfig config = {}) : m_config(config) {}
    PluginInfo info() const override;
    Status onLoad(Engine& engine) override;
    void onUnload(Engine& engine) override;
    void onUpdate(Engine& engine, const FrameTime& time) override;

    AudioSystem& audio() { return *m_audio; }
    void setRunning(bool running);
    void setPaused(bool paused);
    bool running() const { return m_running; }

private:
    AudioConfig m_config;
    std::unique_ptr<AudioSystem> m_audio;
    SubscriptionId m_lifecycle{};
    bool m_running = true;
    bool m_paused = false;
};

} // namespace agr
