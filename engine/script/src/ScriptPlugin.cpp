#include "agr/script/ScriptPlugin.hpp"

#include "agr/audio/AudioPlugin.hpp"
#include "agr/physics2d/Physics2DPlugin.hpp"
#include "agr/physics3d/Physics3DPlugin.hpp"
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
        if (m_running && !m_paused && m_engine) m_scripts->dispatchCollision(m_engine->scene(), e);
    });
    m_collisions3D = engine.events().subscribe<Collision3DEvent>([this](const Collision3DEvent& e) {
        if (m_running && !m_paused && m_engine) m_scripts->dispatchCollision(m_engine->scene(), e);
    });
    return {};
}

void ScriptPlugin::onUnload(Engine& engine) {
    engine.events().unsubscribe(m_collisions);
    engine.events().unsubscribe(m_collisions3D);
    engine.registerService<ScriptPlugin>(nullptr);
    m_scripts.reset();
    m_engine = nullptr;
}

void ScriptPlugin::bindPhysics(Engine& engine) {
    // The physics plugins may be registered after this one: looked up lazily.
    auto* p2 = engine.service<Physics2DPlugin>();
    auto* p3 = engine.service<Physics3DPlugin>();
    m_scripts->setPhysics2D(p2 ? &p2->world() : nullptr);
    m_scripts->setPhysics3D(p3 ? &p3->world() : nullptr);
    auto* audio = engine.service<AudioPlugin>();
    m_scripts->setAudio(audio ? &audio->audio() : nullptr);
}

void ScriptPlugin::setRunning(bool running) {
    if (m_running && !running && m_scripts) m_scripts->reset();
    m_running = running;
}

void ScriptPlugin::onFixedUpdate(Engine& engine, const FrameTime& time) {
    if (!m_running || m_paused) return;
    // Physics may have been registered after this plugin: look it up lazily.
    bindPhysics(engine);
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
    if (!m_running || m_paused) return;
    bindPhysics(engine);
    m_scripts->setScreenSize(engine.platform().windowSize());
    m_scripts->update(engine.scene(), time);
}

} // namespace agr
