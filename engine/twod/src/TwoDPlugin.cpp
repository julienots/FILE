#include "agr/twod/TwoDPlugin.hpp"

#include "agr/runtime/Engine.hpp"
#include "agr/twod/SpriteAnimator.hpp"

namespace agr {

PluginInfo TwoDPlugin::info() const {
    return {"2D", "0.4.0", "Sprite animation and 2D particles.", "Superessence Studio", {"Render"}};
}

Status TwoDPlugin::onLoad(Engine& engine) {
    registerRenderComponents();
    registerSpriteAnimatorComponent();
    engine.registerService<TwoDPlugin>(this);
    return {};
}

void TwoDPlugin::onUnload(Engine& engine) { engine.registerService<TwoDPlugin>(nullptr); }

void TwoDPlugin::onUpdate(Engine& engine, const FrameTime& time) {
    const float dt = static_cast<float>(time.deltaSeconds);
    if (auto* sheets = engine.service<SpriteSheetLibrary>()) updateSpriteAnimators(engine.scene(), *sheets, dt);
    const float particleDt = (dt == 0.0f && m_preview) ? static_cast<float>(time.unscaledDeltaSeconds) : dt;
    m_stats = simulateParticles2D(engine.scene(), particleDt);
}

} // namespace agr
