#pragma once
#include "agr/runtime/Plugin.hpp"
#include "agr/twod/Particles2D.hpp"

namespace agr {

/// 2D gameplay systems: sprite animation and particles. Static plugin, after "Render":
///     engine.plugins().addStatic(std::make_unique<TwoDPlugin>());
class TwoDPlugin final : public Plugin {
public:
    PluginInfo info() const override;
    Status onLoad(Engine& engine) override;
    void onUnload(Engine& engine) override;
    void onUpdate(Engine& engine, const FrameTime& time) override;

    /// Editor preview: particles keep simulating (with real time) while the game
    /// time is stopped. Animations are not previewed: they write serialized frames.
    void setPreviewWhilePaused(bool enabled) { m_preview = enabled; }
    const ParticleStats2D& particleStats() const { return m_stats; }

private:
    bool m_preview = false;
    ParticleStats2D m_stats;
};

} // namespace agr
