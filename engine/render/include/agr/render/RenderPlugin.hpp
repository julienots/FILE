#pragma once
#include "agr/core/EventBus.hpp"
#include <memory>
#include <string>

#include "agr/assets/AssetManager.hpp"
#include "agr/render/GraphicsProfile.hpp"
#include "agr/render/Renderer.hpp"
#include "agr/runtime/Plugin.hpp"

namespace agr {

/// Connects rendering to the engine. Register it statically before Engine::initialize():
///
///     engine.plugins().addStatic(std::make_unique<RenderPlugin>());
///
/// It creates the GPU device on the platform's context, the AssetManager and the
/// Renderer, applies the graphics profile from EngineConfig::graphicsProfile, and
/// publishes them as engine services (engine.service<Renderer>() ...).
/// Without a graphics context (headless) only the AssetManager is created (CPU decoding).
class RenderPlugin final : public Plugin {
public:
    RenderPlugin();
    ~RenderPlugin() override;

    PluginInfo info() const override;
    Status onLoad(Engine& engine) override;
    void onUnload(Engine& engine) override;
    void onUpdate(Engine& engine, const FrameTime& time) override;
    void onRender(Engine& engine, const FrameTime& time) override;

    rhi::Device* device() const { return m_device.get(); }
    AssetManager* assets() const { return m_assets.get(); }
    Renderer* renderer() const { return m_renderer.get(); }
    SpriteSheetLibrary* spriteSheets() const { return m_sheets.get(); }
    ModelLibrary* models() const { return m_models.get(); }
    MaterialLibrary* materials() const { return m_materials.get(); }
    ShaderLibrary* shaders() const { return m_shaders.get(); }
    EnvironmentLibrary* environments() const { return m_environments.get(); }

    /// Switches quality at runtime (settings menu). Returns the applied profile.
    GraphicsProfile applyProfile(Engine& engine, GraphicsTier tier);
    /// Saves the next rendered frame as PNG (e.g. "user://screenshot.png").
    void requestScreenshot(std::string uri) { m_screenshotUri = std::move(uri); }
    /// When false, onRender does nothing: the application drives frames itself
    /// (the editor renders scene views into render targets, then its UI).
    void setAutoRender(bool enabled) { m_autoRender = enabled; }
    /// Polls asset files for changes (editor / development builds).
    void setHotReload(bool enabled, f64 intervalSeconds = 1.0);

private:
    SubscriptionId m_lifecycle{};
    std::unique_ptr<rhi::Device> m_device;
    std::unique_ptr<AssetManager> m_assets;
    std::unique_ptr<SpriteSheetLibrary> m_sheets;
    std::unique_ptr<ModelLibrary> m_models;
    std::unique_ptr<MaterialLibrary> m_materials;
    std::unique_ptr<ShaderLibrary> m_shaders;
    std::unique_ptr<EnvironmentLibrary> m_environments;
    std::unique_ptr<Renderer> m_renderer;
    std::string m_screenshotUri;
    bool m_hotReload = false;
    f64 m_clock = 0.0;
    bool m_autoRender = true;
    f64 m_hotReloadInterval = 1.0;
    f64 m_hotReloadTimer = 0.0;
    u32 m_appliedFpsCap = 0;
};

} // namespace agr
