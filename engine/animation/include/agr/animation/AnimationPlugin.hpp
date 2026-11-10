#pragma once
#include "agr/animation/AnimationSystem.hpp"
#include "agr/runtime/Plugin.hpp"

namespace agr {

/// Runs Animator components (after scripts, before rendering) and publishes AnimationEvent.
///     engine.plugins().addStatic(std::make_unique<AnimationPlugin>());
/// Service: engine.service<AnimationPlugin>()->animation().
class AnimationPlugin final : public Plugin {
public:
    PluginInfo info() const override;
    Status onLoad(Engine& engine) override;
    void onUnload(Engine& engine) override;
    void onUpdate(Engine& engine, const FrameTime& time) override;

    AnimationSystem& animation() { return *m_animation; }
    /// Animators play while running (always in a game; in the editor, in Play mode).
    void setRunning(bool running);
    bool running() const { return m_running; }

private:
    std::unique_ptr<AnimationSystem> m_animation;
    bool m_running = true;
    f64 m_reloadTimer = 0.0;
};

} // namespace agr
