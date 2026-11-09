#pragma once
#include "agr/runtime/PlatformLayer.hpp"

struct SDL_Window;
struct SDL_GLContextState;

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
    const char* name() const override { return "SDL3"; }

    SDL_Window* window() const { return m_window; }
    /// Resolves an OpenGL entry point of the current context.
    void* glProcAddress(const char* name) const;
    bool isGLES() const { return m_gles; }
    void setTitle(const std::string& title);
    /// Shows the on-screen keyboard on mobile.
    void setTextInput(bool enabled);

private:
    SDL_Window* m_window = nullptr;
    SDL_GLContextState* m_glContext = nullptr;
    bool m_gles = false;
    bool m_backgrounded = false;
    bool m_minimized = false;
    bool m_sdlInitialized = false;
};

} // namespace agr
