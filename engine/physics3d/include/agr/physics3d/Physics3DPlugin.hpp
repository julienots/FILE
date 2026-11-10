#pragma once
#include "agr/physics3d/Physics3DWorld.hpp"
#include "agr/runtime/Plugin.hpp"

namespace agr {

/// Runs the 3D physics of the active scene at the fixed timestep and publishes
/// Collision3DEvent. Static plugin:
///     engine.plugins().addStatic(std::make_unique<Physics3DPlugin>());
class Physics3DPlugin final : public Plugin {
public:
    PluginInfo info() const override;
    Status onLoad(Engine& engine) override;
    void onUnload(Engine& engine) override;
    void onFixedUpdate(Engine& engine, const FrameTime& time) override;

    Physics3DWorld& world() { return *m_world; }

private:
    std::unique_ptr<Physics3DWorld> m_world;
};

} // namespace agr
