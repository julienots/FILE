#include "agr/render/RenderPlugin.hpp"

#include "agr/core/Platform.hpp"
#include "agr/render/RenderComponents.hpp"
#include "agr/runtime/Engine.hpp"

namespace agr {

RenderPlugin::RenderPlugin() = default;
RenderPlugin::~RenderPlugin() = default;

PluginInfo RenderPlugin::info() const {
    return {"Render", "0.2.0", "GPU device, asset manager and forward renderer.", "Superessence Studio", {}};
}

Status RenderPlugin::onLoad(Engine& engine) {
    registerRenderComponents();
    PlatformLayer& platform = engine.platform();
    const GraphicsApi api = platform.graphicsApi();

    if (api != GraphicsApi::None) {
        auto device = rhi::createOpenGLDevice([&platform](const char* name) { return platform.graphicsProcAddress(name); },
                                              api == GraphicsApi::OpenGLES);
        if (!device) return device.error();
        m_device = std::move(device.value());
    } else {
        AGR_LOG_INFO("Render", "no graphics context: assets are decoded on the CPU only");
    }

    m_assets = std::make_unique<AssetManager>(engine.vfs(), engine.jobs(), m_device.get());
    engine.registerService<AssetManager>(m_assets.get());

    if (m_device) {
        m_renderer = std::make_unique<Renderer>(*m_device, *m_assets);
        if (Status s = m_renderer->initialize(); !s) {
            m_renderer.reset();
            return s;
        }
        GraphicsTier tier;
        if (engine.config().graphicsProfile == "auto") {
#if defined(AGR_PLATFORM_MOBILE)
            constexpr bool mobile = true;
#else
            constexpr bool mobile = false;
#endif
            tier = recommendGraphicsTier(m_device->info(), mobile);
        } else if (auto parsed = parseGraphicsTier(engine.config().graphicsProfile)) {
            tier = parsed.value();
        } else {
            AGR_LOG_WARN("Render", "{}; using 'high'", parsed.error().message);
            tier = GraphicsTier::High;
        }
        applyProfile(engine, tier);
        engine.registerService<rhi::Device>(m_device.get());
        engine.registerService<Renderer>(m_renderer.get());
    }
    engine.registerService<RenderPlugin>(this);
    return {};
}

GraphicsProfile RenderPlugin::applyProfile(Engine& engine, GraphicsTier tier) {
    GraphicsProfile p = graphicsProfile(tier);
    if (m_device) p.maxTextureSize = std::min(p.maxTextureSize, m_device->info().maxTextureSize);
    if (m_renderer) m_renderer->setProfile(p);
    if (m_assets) {
        m_assets->setMaxTextureSize(p.maxTextureSize);
        m_assets->setUploadBudget(p.uploadBudgetBytes);
    }
    // The profile frame cap applies unless the project configured its own.
    if (engine.config().targetFps == 0 || engine.config().targetFps == m_appliedFpsCap) {
        engine.setTargetFps(p.targetFps);
        m_appliedFpsCap = p.targetFps;
    }
    AGR_LOG_INFO("Render", "graphics profile '{}': scale {:.2f}, MSAA x{}, max texture {}, fps cap {}", p.name,
                 p.renderScale, p.msaaSamples, p.maxTextureSize, p.targetFps ? std::to_string(p.targetFps) : "none");
    return p;
}

void RenderPlugin::setHotReload(bool enabled, f64 intervalSeconds) {
    m_hotReload = enabled;
    m_hotReloadInterval = intervalSeconds > 0.05 ? intervalSeconds : 0.05;
}

void RenderPlugin::onUpdate(Engine&, const FrameTime& time) {
    if (!m_assets) return;
    if (m_hotReload) {
        m_hotReloadTimer += time.unscaledDeltaSeconds;
        if (m_hotReloadTimer >= m_hotReloadInterval) {
            m_hotReloadTimer = 0.0;
            m_assets->reloadChanged();
        }
    }
    m_assets->update();
}

void RenderPlugin::onRender(Engine& engine, const FrameTime&) {
    if (!m_renderer) return;
    const math::Vec2 fb = engine.platform().framebufferSize();
    const u32 w = static_cast<u32>(fb.x), h = static_cast<u32>(fb.y);
    m_renderer->render(engine.scene(), w, h);

    if (!m_screenshotUri.empty() && w > 0 && h > 0) {
        std::vector<u8> pixels;
        if (Status s = m_device->readBackbuffer(0, 0, w, h, pixels); !s) {
            AGR_LOG_ERROR("Render", "screenshot failed: {}", s.error().message);
        } else if (auto png = encodePng(w, h, pixels); !png) {
            AGR_LOG_ERROR("Render", "screenshot encoding failed: {}", png.error().message);
        } else if (Status w2 = engine.vfs().writeBytes(m_screenshotUri, png.value().data(), png.value().size()); !w2) {
            AGR_LOG_ERROR("Render", "screenshot write failed: {}", w2.error().message);
        } else {
            AGR_LOG_INFO("Render", "screenshot saved to {} ({}x{})", m_screenshotUri, w, h);
        }
        m_screenshotUri.clear();
    }
}

void RenderPlugin::onUnload(Engine& engine) {
    engine.registerService<RenderPlugin>(nullptr);
    engine.registerService<Renderer>(nullptr);
    engine.registerService<rhi::Device>(nullptr);
    engine.registerService<AssetManager>(nullptr);
    m_renderer.reset();
    m_assets.reset();
    m_device.reset();
}

} // namespace agr
