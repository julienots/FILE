#pragma once
#include "agr/physics2d/Physics2DWorld.hpp"
#include "agr/runtime/Plugin.hpp"

namespace agr {

/// Runs the 2D physics of the active scene at the engine's fixed timestep and
/// publishes Collision2DEvent on the engine event bus. Static plugin:
///     engine.plugins().addStatic(std::make_unique<Physics2DPlugin>());
/// The world is available as a service: engine.service<Physics2DPlugin>()->world().
class Physics2DPlugin final : public Plugin {
public:
    PluginInfo info() const override;
    Status onLoad(Engine& engine) override;
    void onUnload(Engine& engine) override;
    void onFixedUpdate(Engine& engine, const FrameTime& time) override;

    Physics2DWorld& world() { return m_world; }

private:
    Physics2DWorld m_world;
};

} // namespace agr
