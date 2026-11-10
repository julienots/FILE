#pragma once
#include <functional>

#include "agr/runtime/PlatformLayer.hpp"

struct SDL_Window;
struct SDL_GLContextState;

struct SDL_Sensor;
struct SDL_Haptic;

namespace agr {

/// SDL3 implementation of the platform layer: window, OpenGL / OpenGL ES
/// context, keyboard, mouse, multi-touch and mobile lifecycle events.
///
/// Context selection:
///   desktop -> OpenGL 3.3 core
///   Android -> OpenGL ES 3.0
/// (the Vulkan path is added with the renderer in Phase 2.)
class SdlPlatform final : public PlatformLayer {
public:
    SdlPlatform() = default;
    ~SdlPlatform() override;

    Status initialize(const EngineConfig& config) override;
    void shutdown() override;
    bool pollEvents(EventBus& events, Input& input) override;
    void present() override;
    bool canRender() const override { return m_window && !m_backgrounded && !m_minimized; }
    math::Vec2 framebufferSize() const override;
    math::Vec2 windowSize() const override;
    const char* name() const override { return "SDL3"; }
    math::Vec4 safeAreaInsets() const override;
    void vibrate(float seconds, float strength) override;
    bool canVibrate() const override { return m_haptic != nullptr; }
    GraphicsApi graphicsApi() const override;
    void* graphicsProcAddress(const char* name) const override { return glProcAddress(name); }
    std::vector<std::string> vulkanInstanceExtensions() const override;
    void* vulkanGetInstanceProcAddr() const override;
    u64 createVulkanSurface(void* vkInstance) const override;
    Status setGraphicsApi(GraphicsApi api) override;

    SDL_Window* window() const { return m_window; }
    /// Resolves an OpenGL entry point of the current context.
    void* glProcAddress(const char* name) const;
    bool isGLES() const { return m_gles; }
    void setTitle(const std::string& title);
    /// Shows the on-screen keyboard on mobile.
    void setTextInput(bool enabled);
    /// Called with every raw SDL_Event* before the engine translates it (UI toolkits).
    using RawEventHook = std::function<void(const void* sdlEvent)>;
    void setRawEventHook(RawEventHook hook) { m_rawHook = std::move(hook); }

private:
    Status createWindow(GraphicsApi api);
    void destroyWindow();
    void openDevices();  ///< Motion sensors and vibrator (phones, tablets).

    EngineConfig m_config;
    RawEventHook m_rawHook;
    GraphicsApi m_api = GraphicsApi::None;
    bool m_vulkanLoaded = false;
    SDL_Window* m_window = nullptr;
    SDL_GLContextState* m_glContext = nullptr;
    bool m_gles = false;
    bool m_backgrounded = false;
    bool m_minimized = false;
    bool m_sdlInitialized = false;
    ::SDL_Sensor* m_accel = nullptr;
    ::SDL_Sensor* m_gyro = nullptr;
    ::SDL_Haptic* m_haptic = nullptr;
    u32 m_accelId = 0, m_gyroId = 0;
};

} // namespace agr
