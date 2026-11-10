#pragma once
#include <memory>
#include <string>
#include <vector>

#include "agr/core/FileSystem.hpp"
#include "agr/core/Time.hpp"
#include "agr/animation/AnimationSystem.hpp"
#include "agr/audio/AudioSystem.hpp"
#include "agr/ui/UISystem.hpp"
#include "agr/physics2d/Physics2DWorld.hpp"
#include "agr/physics3d/Physics3DWorld.hpp"
#include "agr/runtime/Input.hpp"
#include "agr/scene/Scene.hpp"

namespace agr {

/// A property declared by a script (`properties = { speed = 6 }`), shown in the inspector.
struct ScriptProperty {
    enum class Type : u8 { Number, Boolean, String };
    std::string name;
    Type type = Type::Number;
    double number = 0.0;
    bool boolean = false;
    std::string string;
};

/// What the editor needs to know about a script file.
struct ScriptInfo {
    std::string uri;
    bool valid = false;
    std::string error;                    ///< Compile / load error (with line number).
    std::vector<ScriptProperty> properties;  ///< Sorted by name.
    std::vector<std::string> callbacks;   ///< onStart, onUpdate... defined by the script.
};

struct ScriptStats {
    u32 instances = 0;
    u32 faulted = 0;      ///< Instances stopped after an error (until their script is fixed).
    u32 errors = 0;       ///< Errors reported since the last reset.
    f64 updateMs = 0.0;   ///< Last update (all onUpdate calls).
    usize memoryKb = 0;   ///< Lua heap.
};

/// Lua 5.4 runtime for gameplay scripts (one VM per engine).
///
/// A script is a .lua asset returning a table: its functions are callbacks
/// (onStart, onUpdate(dt), onFixedUpdate(dt), onCollision(other, began),
/// onTrigger(other, began), onDestroy) called with `self`, an instance table
/// per entity whose `entity` field is the entity. The sandbox has no file, OS,
/// debug or bytecode access; a callback running too long is stopped. A script
/// that raises an error is reported once (file:line and entity) and paused until
/// its file changes. Scripts reload live when their file changes: instances keep
/// their fields and use the new functions.
class ScriptEngine {
public:
    explicit ScriptEngine(VirtualFileSystem& vfs);
    ~ScriptEngine();
    ScriptEngine(const ScriptEngine&) = delete;
    ScriptEngine& operator=(const ScriptEngine&) = delete;

    void setInput(const Input* input);
    /// Window size in input units (agr.screen.size()); 0 x 0 when headless.
    void setScreenSize(math::Vec2 size);
    void setPhysics2D(Physics2DWorld* world);
    void setPhysics3D(Physics3DWorld* world);
    void setAudio(AudioSystem* audio);
    void setUI(UISystem* ui);
    void setAnimation(AnimationSystem* animation);
    /// Maximum run time of one callback before it is stopped (infinite loops).
    void setCallbackBudget(f64 seconds);

    /// Creates instances for new Script components (onStart), destroys removed ones
    /// (onDestroy), then calls onUpdate.
    void update(Scene& scene, const FrameTime& time);
    void fixedUpdate(Scene& scene, const FrameTime& time);
    void dispatchCollision(Scene& scene, const Collision2DEvent& event);
    void dispatchCollision(Scene& scene, const Collision3DEvent& event);
    /// UI: `onClick()` on the button's script, then `message(button)` on its target's script.
    void dispatchUI(Scene& scene, const UIClickEvent& event);
    /// UI: `onValueChanged(value)` on the slider's script, then `message(slider, value)` on its target's.
    void dispatchUI(Scene& scene, const UIValueChangedEvent& event);
    /// Animation: onAnimationEvent(name) / onStateEnter(name) on the animated entity's script.
    void dispatchAnimation(Scene& scene, const AnimationEvent& event);
    /// Destroys every instance (onDestroy is called) — scene unloaded.
    void reset();
    u32 reloadChanged();

    /// Compiles a script without running instances (editor inspector, validation).
    ScriptInfo describe(const std::string& uri);
    /// Runs a chunk in the sandbox (tests, console). Returns the error message on failure.
    Status run(std::string_view code, Scene* scene = nullptr);
    /// The `self` table of an entity's script as a debug string of its fields (tests, inspector).
    Result<std::string> field(ecs::Entity e, const std::string& name);

    ScriptStats stats() const;

    struct Impl;

private:
    std::unique_ptr<Impl> m;
};

} // namespace agr
