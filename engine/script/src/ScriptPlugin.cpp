#include "agr/script/ScriptPlugin.hpp"

#include "agr/physics2d/Physics2DPlugin.hpp"
#include "agr/runtime/Engine.hpp"
#include "agr/script/ScriptComponent.hpp"

namespace agr {

PluginInfo ScriptPlugin::info() const {
    return {"Script", "1.0", "Lua 5.4 gameplay scripts", "Superessence Studio", {}};
}

Status ScriptPlugin::onLoad(Engine& engine) {
    registerScriptComponent();
    m_engine = &engine;
    m_scripts = std::make_unique<ScriptEngine>(engine.vfs());
    m_scripts->setInput(&engine.input());
    engine.registerService<ScriptPlugin>(this);
    m_collisions = engine.events().subscribe<Collision2DEvent>([this](const Collision2DEvent& e) {
        if (m_running && m_engine) m_scripts->dispatchCollision(m_engine->scene(), e);
    });
    return {};
}

void ScriptPlugin::onUnload(Engine& engine) {
    engine.events().unsubscribe(m_collisions);
    engine.registerService<ScriptPlugin>(nullptr);
    m_scripts.reset();
    m_engine = nullptr;
}

void ScriptPlugin::setRunning(bool running) {
    if (m_running && !running && m_scripts) m_scripts->reset();
    m_running = running;
}

void ScriptPlugin::onFixedUpdate(Engine& engine, const FrameTime& time) {
    if (!m_running) return;
    // Physics may have been registered after this plugin: look it up lazily.
    if (auto* p = engine.service<Physics2DPlugin>()) m_scripts->setPhysics2D(&p->world());
    m_scripts->fixedUpdate(engine.scene(), time);
}

void ScriptPlugin::onUpdate(Engine& engine, const FrameTime& time) {
    if (m_hotReload) {
        m_reloadTimer += time.unscaledDeltaSeconds;
        if (m_reloadTimer >= 0.5) {
            m_reloadTimer = 0.0;
            m_scripts->reloadChanged();
        }
    }
    if (!m_running) return;
    if (auto* p = engine.service<Physics2DPlugin>()) m_scripts->setPhysics2D(&p->world());
    m_scripts->update(engine.scene(), time);
}

} // namespace agr
