#include "agr/physics3d/Physics3DPlugin.hpp"

#include "agr/runtime/Engine.hpp"

namespace agr {

PluginInfo Physics3DPlugin::info() const {
    return {"Physics3D", "1.0", "3D rigid bodies, colliders and triggers (Jolt Physics)", "Superessence Studio", {}};
}

Status Physics3DPlugin::onLoad(Engine& engine) {
    registerPhysics3DComponents();
    m_world = std::make_unique<Physics3DWorld>();
    engine.registerService<Physics3DPlugin>(this);
    return {};
}

void Physics3DPlugin::onUnload(Engine& engine) {
    engine.registerService<Physics3DPlugin>(nullptr);
    m_world.reset();
}

void Physics3DPlugin::onFixedUpdate(Engine& engine, const FrameTime& time) {
    m_world->step(engine.scene(), static_cast<float>(time.fixedDeltaSeconds));
    for (const Collision3DEvent& e : m_world->events()) engine.events().publish(e);
}

} // namespace agr
