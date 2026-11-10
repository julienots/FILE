#pragma once
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
    virtual const char* name() const = 0;

    /// Graphics API of the context created by the platform (None when headless).
    virtual GraphicsApi graphicsApi() const { return GraphicsApi::None; }
    /// Resolves a graphics entry point (OpenGL functions) of the current context.
    virtual void* graphicsProcAddress(const char* name) const { (void)name; return nullptr; }
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
