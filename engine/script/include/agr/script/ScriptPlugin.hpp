#pragma once
#include <memory>

#include "agr/core/EventBus.hpp"
#include "agr/runtime/Plugin.hpp"
#include "agr/script/ScriptEngine.hpp"

namespace agr {

/// Runs the Lua scripts of the active scene. Register it before the physics plugins
/// (scripts move bodies, then physics steps) and after "Render":
///     engine.plugins().addStatic(std::make_unique<ScriptPlugin>());
class ScriptPlugin final : public Plugin {
public:
    PluginInfo info() const override;
    Status onLoad(Engine& engine) override;
    void onUnload(Engine& engine) override;
    void onFixedUpdate(Engine& engine, const FrameTime& time) override;
    void onUpdate(Engine& engine, const FrameTime& time) override;

    ScriptEngine* scripts() { return m_scripts.get(); }
    /// Off: scripts are not run (editor edit mode); turning it off destroys the instances.
    void setRunning(bool running);
    bool running() const { return m_running; }
    /// Paused: instances are kept but no callback runs (editor pause).
    void setPaused(bool paused) { m_paused = paused; }
    /// Polls script files for changes (editor / development).
    void setHotReload(bool enabled) { m_hotReload = enabled; }

private:
    std::unique_ptr<ScriptEngine> m_scripts;
    void bindPhysics(Engine& engine);
    SubscriptionId m_collisions, m_collisions3D;
    Engine* m_engine = nullptr;
    bool m_running = true;
    bool m_paused = false;
    bool m_hotReload = false;
    f64 m_reloadTimer = 0.0;
};

} // namespace agr
