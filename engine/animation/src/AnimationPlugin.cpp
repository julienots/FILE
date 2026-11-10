#include "agr/animation/AnimationPlugin.hpp"

#include "agr/runtime/Engine.hpp"

namespace agr {

PluginInfo AnimationPlugin::info() const {
    return {"Animation", "1.0", "Property animation clips, state machines and animation events", "Superessence Studio", {}};
}

Status AnimationPlugin::onLoad(Engine& engine) {
    registerAnimationComponents();
    m_animation = std::make_unique<AnimationSystem>(engine.vfs());
    engine.registerService<AnimationPlugin>(this);
    return {};
}

void AnimationPlugin::onUnload(Engine& engine) {
    engine.registerService<AnimationPlugin>(nullptr);
    m_animation.reset();
}

void AnimationPlugin::setRunning(bool running) {
    if (m_running && !running && m_animation) m_animation->reset();
    m_running = running;
}

void AnimationPlugin::onUpdate(Engine& engine, const FrameTime& time) {
    // Edited clips and state machines are picked up while the game runs.
    m_reloadTimer += time.unscaledDeltaSeconds;
    if (m_reloadTimer >= 0.5) {
        m_reloadTimer = 0.0;
        m_animation->library().reloadChanged();
    }
    if (!m_running) return;
    m_animation->update(engine.scene(), static_cast<float>(time.deltaSeconds));
    for (AnimationEvent& e : m_animation->takeEvents()) engine.events().publish(e);
}

} // namespace agr
