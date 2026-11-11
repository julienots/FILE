#pragma once
#include <memory>

#include "agr/net/NetSession.hpp"
#include "agr/runtime/Plugin.hpp"

namespace agr {

/// Owns the game's network session (agr.net in scripts) and updates it every frame with
/// unscaled time (pings continue while the game is paused). Static plugin:
///     engine.plugins().addStatic(std::make_unique<NetPlugin>());
class NetPlugin final : public Plugin {
public:
    PluginInfo info() const override;
    Status onLoad(Engine& engine) override;
    void onUnload(Engine& engine) override;
    void onUpdate(Engine& engine, const FrameTime& time) override;
    net::NetSession& session() { return *m_session; }

private:
    std::unique_ptr<net::NetSession> m_session;
    f64 m_clock = 0.0;
};

} // namespace agr
