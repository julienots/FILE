#pragma once
#include "agr/core/Result.hpp"
#include "agr/core/Time.hpp"

namespace agr {

class Engine;

/// Game / tool entry point. Override the hooks you need.
class Application {
public:
    virtual ~Application() = default;
    /// After every engine service is up and plugins are loaded.
    virtual Status onStart(Engine& engine) { (void)engine; return {}; }
    /// Fixed timestep (gameplay, physics). May run 0..N times per frame.
    virtual void onFixedUpdate(Engine& engine, const FrameTime& time) { (void)engine; (void)time; }
    /// Once per frame, variable timestep.
    virtual void onUpdate(Engine& engine, const FrameTime& time) { (void)engine; (void)time; }
    /// Once per frame after transforms are updated, only when rendering is possible.
    virtual void onRender(Engine& engine, const FrameTime& time) { (void)engine; (void)time; }
    /// Before plugins are unloaded.
    virtual void onShutdown(Engine& engine) { (void)engine; }
};

} // namespace agr
