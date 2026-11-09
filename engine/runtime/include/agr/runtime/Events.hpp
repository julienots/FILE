#pragma once
// Platform-independent engine events, published on Engine::events().
#include <string>

#include "agr/runtime/Input.hpp"

namespace agr {

struct WindowResizedEvent {
    i32 width, height;            ///< Logical size.
    i32 pixelWidth, pixelHeight;  ///< Framebuffer size (high DPI).
};
struct WindowCloseRequestedEvent {};
struct WindowFocusEvent { bool focused; };

/// Mobile lifecycle. On Android, rendering must stop between
/// WillEnterBackground and DidEnterForeground (the GL surface is destroyed).
enum class LifecycleState : u8 { WillEnterBackground, DidEnterBackground, WillEnterForeground, DidEnterForeground, LowMemory, Terminating };
struct AppLifecycleEvent { LifecycleState state; };

struct KeyEvent {
    Key key;
    bool pressed;
    bool repeat;
};
struct MouseButtonEvent {
    MouseButton button;
    bool pressed;
    math::Vec2 position;
};
struct TouchEvent {
    u64 id;
    TouchPhase phase;
    math::Vec2 position;
};
struct FileDroppedEvent { std::string path; };

/// Sent by Engine before the first frame and before shutdown.
struct EngineStartedEvent {};
struct EngineStoppingEvent {};

} // namespace agr
