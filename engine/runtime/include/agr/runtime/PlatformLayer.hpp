#pragma once
#include <string>
#include <vector>

#include "agr/core/EventBus.hpp"
#include "agr/core/Result.hpp"
#include "agr/runtime/EngineConfig.hpp"
#include "agr/runtime/Input.hpp"

namespace agr {

/// Abstraction of the OS layer (window, events, presentation). The runtime
/// never includes SDL: the SDL3 implementation lives in agr::platform.
class PlatformLayer {
public:
    virtual ~PlatformLayer() = default;
    virtual Status initialize(const EngineConfig& config) = 0;
    virtual void shutdown() = 0;
    /// Pumps OS events into `input` and `events`. Returns false when the app must quit.
    virtual bool pollEvents(EventBus& events, Input& input) = 0;
    /// Presents the frame (swap buffers). No-op when headless.
    virtual void present() = 0;
    /// False while the app is in background (mobile): skip rendering.
    virtual bool canRender() const = 0;
    virtual math::Vec2 framebufferSize() const = 0;
    /// Window size in the units of mouse and touch positions (differs from the framebuffer on HiDPI).
    virtual math::Vec2 windowSize() const { return framebufferSize(); }
    virtual const char* name() const = 0;
    /// Screen areas hidden by notches, rounded corners or system bars: left, top, right,
    /// bottom insets in window units (0 on desktop).
    virtual math::Vec4 safeAreaInsets() const { return {0.0f, 0.0f, 0.0f, 0.0f}; }
    /// Device vibration (phones) for `seconds` at `strength` (0..1); no-op without a vibrator.
    virtual void vibrate(float seconds, float strength) { (void)seconds; (void)strength; }
    virtual bool canVibrate() const { return false; }
    /// User's languages, most preferred first ("fr-FR", "en"...); empty if unknown.
    virtual std::vector<std::string> preferredLanguages() const { return {}; }

    /// Graphics API of the context created by the platform (None when headless).
    virtual GraphicsApi graphicsApi() const { return GraphicsApi::None; }
    /// Resolves a graphics entry point (OpenGL functions) of the current context.
    virtual void* graphicsProcAddress(const char* name) const { (void)name; return nullptr; }

    // Vulkan (graphicsApi() == Vulkan)
    virtual std::vector<std::string> vulkanInstanceExtensions() const { return {}; }
    /// PFN_vkGetInstanceProcAddr of the Vulkan loader.
    virtual void* vulkanGetInstanceProcAddr() const { return nullptr; }
    /// Creates a VkSurfaceKHR for the window; returns 0 on failure.
    virtual u64 createVulkanSurface(void* vkInstance) const { (void)vkInstance; return 0; }

    /// Recreates the window for another graphics API (fallback when a device cannot
    /// be created). Every GPU object of the previous API must be destroyed first.
    virtual Status setGraphicsApi(GraphicsApi api) {
        (void)api;
        return makeError(Error::Code::Unsupported, "this platform cannot switch graphics API");
    }
};

/// No window, no input: used by tests, CI and dedicated servers.
class HeadlessPlatform final : public PlatformLayer {
public:
    Status initialize(const EngineConfig&) override { return {}; }
    void shutdown() override {}
    bool pollEvents(EventBus&, Input&) override { return true; }
    void present() override {}
    bool canRender() const override { return false; }
    math::Vec2 framebufferSize() const override { return {}; }
    const char* name() const override { return "Headless"; }
};

} // namespace agr
