#include "agr/net/NetPlugin.hpp"

#include "agr/runtime/Engine.hpp"

namespace agr {

PluginInfo NetPlugin::info() const {
    return {"Network", "1.0", "UDP sessions for multiplayer games (host / join, reliable and unreliable messages)", "Superessence Studio", {}};
}

Status NetPlugin::onLoad(Engine& engine) {
    m_session = std::make_unique<net::NetSession>();
    engine.registerService<NetPlugin>(this);
    return {};
}

void NetPlugin::onUnload(Engine& engine) {
    engine.registerService<NetPlugin>(nullptr);
    m_session.reset();  // says goodbye to the peers
}

void NetPlugin::onUpdate(Engine&, const FrameTime& time) {
    m_clock += time.unscaledDeltaSeconds;
    m_session->update(m_clock);
}

} // namespace agr
