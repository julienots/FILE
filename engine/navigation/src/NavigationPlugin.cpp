#include "agr/navigation/NavigationPlugin.hpp"

#include "agr/runtime/Engine.hpp"

namespace agr {

PluginInfo NavigationPlugin::info() const {
    return {"Navigation", "1.0", "2D grid pathfinding, 3D navigation meshes (Recast & Detour), agents", "Superessence Studio", {}};
}

Status NavigationPlugin::onLoad(Engine& engine) {
    registerNavigationComponents();
    m_system = std::make_unique<NavigationSystem>();
    engine.registerService<NavigationPlugin>(this);
    return {};
}

void NavigationPlugin::onUnload(Engine& engine) {
    engine.registerService<NavigationPlugin>(nullptr);
    m_system.reset();
}

void NavigationPlugin::onUpdate(Engine& engine, const FrameTime& time) {
    if (time.deltaSeconds <= 0.0) return;  // editing or paused: nothing moves (the editor rebuilds on demand)
    m_system->update(engine.scene(), static_cast<float>(time.deltaSeconds));
}

} // namespace agr
