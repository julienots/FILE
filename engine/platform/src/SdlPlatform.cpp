#include "agr/platform/SdlPlatform.hpp"

#include <SDL3/SDL.h>

#include "agr/core/Log.hpp"
#include "agr/core/Platform.hpp"
#include "agr/runtime/Events.hpp"

namespace agr {
namespace {

MouseButton toMouseButton(u8 sdlButton) {
    switch (sdlButton) {
        case SDL_BUTTON_LEFT: return MouseButton::Left;
        case SDL_BUTTON_MIDDLE: return MouseButton::Middle;
        case SDL_BUTTON_RIGHT: return MouseButton::Right;
        case SDL_BUTTON_X1: return MouseButton::X1;
        case SDL_BUTTON_X2: return MouseButton::X2;
        default: return MouseButton::Count;
    }
}

// SDL scancodes are USB HID usage ids, exactly like agr::Key.
static_assert(static_cast<int>(Key::A) == SDL_SCANCODE_A && static_cast<int>(Key::Z) == SDL_SCANCODE_Z);
static_assert(static_cast<int>(Key::Num1) == SDL_SCANCODE_1 && static_cast<int>(Key::Num0) == SDL_SCANCODE_0);
static_assert(static_cast<int>(Key::Escape) == SDL_SCANCODE_ESCAPE && static_cast<int>(Key::Space) == SDL_SCANCODE_SPACE);
static_assert(static_cast<int>(Key::F12) == SDL_SCANCODE_F12 && static_cast<int>(Key::Up) == SDL_SCANCODE_UP);
static_assert(static_cast<int>(Key::LeftCtrl) == SDL_SCANCODE_LCTRL && static_cast<int>(Key::RightSuper) == SDL_SCANCODE_RGUI);
static_assert(static_cast<int>(Key::AndroidBack) == SDL_SCANCODE_AC_BACK);
static_assert(static_cast<int>(Key::Count) == SDL_SCANCODE_COUNT);

Key toKey(SDL_Scancode sc) {
    return static_cast<u32>(sc) < static_cast<u32>(Key::Count) ? static_cast<Key>(sc) : Key::Unknown;
}

} // namespace

SdlPlatform::~SdlPlatform() { shutdown(); }

Status SdlPlatform::initialize(const EngineConfig& config) {
    SDL_SetAppMetadata(config.appName.c_str(), nullptr, nullptr);
    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS))
        return makeError(Error::Code::Unsupported, std::string("SDL_Init failed: ") + SDL_GetError());
    m_sdlInitialized = true;

    const WindowConfig& wc = config.window;
    SDL_WindowFlags flags = 0;
    if (wc.resizable) flags |= SDL_WINDOW_RESIZABLE;
    if (wc.highDpi) flags |= SDL_WINDOW_HIGH_PIXEL_DENSITY;
#if defined(AGR_PLATFORM_MOBILE)
    flags |= SDL_WINDOW_FULLSCREEN;
#else
    if (wc.fullscreen) flags |= SDL_WINDOW_FULLSCREEN;
#endif

    if (wc.graphics == GraphicsApi::OpenGL) {
        flags |= SDL_WINDOW_OPENGL;
#if defined(AGR_PLATFORM_ANDROID)
        m_gles = true;
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
#else
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
#endif
        SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
        SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
        SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);
    }

    m_window = SDL_CreateWindow(wc.title.c_str(), wc.width, wc.height, flags);
    if (!m_window) return makeError(Error::Code::Unsupported, std::string("SDL_CreateWindow failed: ") + SDL_GetError());

    if (wc.graphics == GraphicsApi::OpenGL) {
        m_glContext = SDL_GL_CreateContext(m_window);
        if (!m_glContext)
            return makeError(Error::Code::Unsupported, std::string("OpenGL context creation failed: ") + SDL_GetError());
        SDL_GL_MakeCurrent(m_window, m_glContext);
        if (!SDL_GL_SetSwapInterval(wc.vsync ? 1 : 0))
            AGR_LOG_WARN("Platform", "could not set swap interval: {}", SDL_GetError());

        using GetStringFn = const unsigned char* (*)(unsigned int);
        if (auto getString = reinterpret_cast<GetStringFn>(SDL_GL_GetProcAddress("glGetString"))) {
            constexpr unsigned int kVendor = 0x1F00, kRenderer = 0x1F01, kVersion = 0x1F02;
            AGR_LOG_INFO("Platform", "{} context: {} | {} | {}", m_gles ? "OpenGL ES" : "OpenGL",
                         reinterpret_cast<const char*>(getString(kVersion)),
                         reinterpret_cast<const char*>(getString(kRenderer)),
                         reinterpret_cast<const char*>(getString(kVendor)));
        }
    }

    const math::Vec2 fb = framebufferSize();
    AGR_LOG_INFO("Platform", "window {}x{} (framebuffer {}x{}), video driver '{}'", wc.width, wc.height,
                 static_cast<int>(fb.x), static_cast<int>(fb.y), SDL_GetCurrentVideoDriver());
    return {};
}

void SdlPlatform::shutdown() {
    if (m_glContext) {
        SDL_GL_DestroyContext(m_glContext);
        m_glContext = nullptr;
    }
    if (m_window) {
        SDL_DestroyWindow(m_window);
        m_window = nullptr;
    }
    if (m_sdlInitialized) {
        SDL_Quit();
        m_sdlInitialized = false;
    }
}

bool SdlPlatform::pollEvents(EventBus& events, Input& input) {
    bool keepRunning = true;
    SDL_Event e;
    while (SDL_PollEvent(&e)) {
        switch (e.type) {
            case SDL_EVENT_QUIT:
                keepRunning = false;
                break;
            case SDL_EVENT_WINDOW_CLOSE_REQUESTED:
                events.publish(WindowCloseRequestedEvent{});
                break;
            case SDL_EVENT_WINDOW_RESIZED:
            case SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED: {
                int w = 0, h = 0, pw = 0, ph = 0;
                SDL_GetWindowSize(m_window, &w, &h);
                SDL_GetWindowSizeInPixels(m_window, &pw, &ph);
                events.publish(WindowResizedEvent{w, h, pw, ph});
                break;
            }
            case SDL_EVENT_WINDOW_MINIMIZED: m_minimized = true; break;
            case SDL_EVENT_WINDOW_RESTORED:
            case SDL_EVENT_WINDOW_MAXIMIZED: m_minimized = false; break;
            case SDL_EVENT_WINDOW_FOCUS_GAINED: events.publish(WindowFocusEvent{true}); break;
            case SDL_EVENT_WINDOW_FOCUS_LOST:
                input.reset();
                events.publish(WindowFocusEvent{false});
                break;

            case SDL_EVENT_KEY_DOWN:
            case SDL_EVENT_KEY_UP: {
                const Key key = toKey(e.key.scancode);
                input.setKey(key, e.key.down);
                events.publish(KeyEvent{key, e.key.down, e.key.repeat});
                break;
            }
            case SDL_EVENT_TEXT_INPUT: input.appendText(e.text.text); break;

            case SDL_EVENT_MOUSE_MOTION: input.setMousePosition({e.motion.x, e.motion.y}); break;
            case SDL_EVENT_MOUSE_BUTTON_DOWN:
            case SDL_EVENT_MOUSE_BUTTON_UP: {
                const MouseButton b = toMouseButton(e.button.button);
                input.setMouseButton(b, e.button.down);
                input.setMousePosition({e.button.x, e.button.y});
                if (b != MouseButton::Count) events.publish(MouseButtonEvent{b, e.button.down, {e.button.x, e.button.y}});
                break;
            }
            case SDL_EVENT_MOUSE_WHEEL: input.addWheel({e.wheel.x, e.wheel.y}); break;

            case SDL_EVENT_FINGER_DOWN:
            case SDL_EVENT_FINGER_MOTION:
            case SDL_EVENT_FINGER_UP: {
                // SDL reports normalized coordinates; convert to window pixels.
                int w = 0, h = 0;
                SDL_GetWindowSize(m_window, &w, &h);
                const math::Vec2 pos{e.tfinger.x * static_cast<float>(w), e.tfinger.y * static_cast<float>(h)};
                const u64 id = static_cast<u64>(e.tfinger.fingerID);
                TouchPhase phase = TouchPhase::Moved;
                if (e.type == SDL_EVENT_FINGER_DOWN) { input.touchDown(id, pos, e.tfinger.pressure); phase = TouchPhase::Began; }
                else if (e.type == SDL_EVENT_FINGER_UP) { input.touchUp(id, pos); phase = TouchPhase::Ended; }
                else input.touchMove(id, pos, e.tfinger.pressure);
                events.publish(TouchEvent{id, phase, pos});
                break;
            }

            case SDL_EVENT_DROP_FILE:
                if (e.drop.data) events.publish(FileDroppedEvent{e.drop.data});
                break;

            // Mobile lifecycle.
            case SDL_EVENT_WILL_ENTER_BACKGROUND:
                m_backgrounded = true;
                events.publish(AppLifecycleEvent{LifecycleState::WillEnterBackground});
                break;
            case SDL_EVENT_DID_ENTER_BACKGROUND:
                m_backgrounded = true;
                events.publish(AppLifecycleEvent{LifecycleState::DidEnterBackground});
                break;
            case SDL_EVENT_WILL_ENTER_FOREGROUND: events.publish(AppLifecycleEvent{LifecycleState::WillEnterForeground}); break;
            case SDL_EVENT_DID_ENTER_FOREGROUND:
                m_backgrounded = false;
                events.publish(AppLifecycleEvent{LifecycleState::DidEnterForeground});
                break;
            case SDL_EVENT_LOW_MEMORY: events.publish(AppLifecycleEvent{LifecycleState::LowMemory}); break;
            case SDL_EVENT_TERMINATING:
                events.publish(AppLifecycleEvent{LifecycleState::Terminating});
                keepRunning = false;
                break;
            default: break;
        }
    }
    return keepRunning;
}

void SdlPlatform::present() {
    if (m_glContext) SDL_GL_SwapWindow(m_window);
}

math::Vec2 SdlPlatform::framebufferSize() const {
    int w = 0, h = 0;
    if (m_window) SDL_GetWindowSizeInPixels(m_window, &w, &h);
    return {static_cast<float>(w), static_cast<float>(h)};
}

void* SdlPlatform::glProcAddress(const char* name) const {
    return reinterpret_cast<void*>(SDL_GL_GetProcAddress(name));
}

void SdlPlatform::setTitle(const std::string& title) {
    if (m_window) SDL_SetWindowTitle(m_window, title.c_str());
}

void SdlPlatform::setTextInput(bool enabled) {
    if (!m_window) return;
    if (enabled) SDL_StartTextInput(m_window);
    else SDL_StopTextInput(m_window);
}

} // namespace agr
