#pragma once
#include <memory>

#include "agr/navigation/NavigationSystem.hpp"
#include "agr/runtime/Plugin.hpp"

namespace agr {

/// Keeps the scene's navigation data up to date and moves NavAgent entities (scaled time:
/// agents stand still while the editor is not playing). Static plugin:
///     engine.plugins().addStatic(std::make_unique<NavigationPlugin>());
class NavigationPlugin final : public Plugin {
public:
    PluginInfo info() const override;
    Status onLoad(Engine& engine) override;
    void onUnload(Engine& engine) override;
    void onUpdate(Engine& engine, const FrameTime& time) override;
    NavigationSystem& navigation() { return *m_system; }

private:
    std::unique_ptr<NavigationSystem> m_system;
};

} // namespace agr
