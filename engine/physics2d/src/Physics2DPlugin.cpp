#include "agr/physics2d/Physics2DPlugin.hpp"

#include "agr/runtime/Engine.hpp"

namespace agr {

PluginInfo Physics2DPlugin::info() const {
    return {"Physics2D", "1.0", "2D rigid bodies, colliders and triggers (Box2D v3)", "Superessence Studio", {}};
}

Status Physics2DPlugin::onLoad(Engine& engine) {
    registerPhysics2DComponents();
    engine.registerService<Physics2DPlugin>(this);
    return {};
}

void Physics2DPlugin::onUnload(Engine& engine) {
    engine.registerService<Physics2DPlugin>(nullptr);
    m_world.reset();
}

void Physics2DPlugin::onFixedUpdate(Engine& engine, const FrameTime& time) {
    m_world.step(engine.scene(), static_cast<float>(time.fixedDeltaSeconds));
    for (const Collision2DEvent& e : m_world.events()) engine.events().publish(e);
}

} // namespace agr
