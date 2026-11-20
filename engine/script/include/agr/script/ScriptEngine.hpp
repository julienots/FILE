#pragma once
#include <optional>
#include <functional>
#include <string_view>
#include <memory>
#include <string>
#include <vector>

#include "agr/core/FileSystem.hpp"
#include "agr/core/Time.hpp"
#include "agr/animation/AnimationSystem.hpp"
#include "agr/audio/AudioSystem.hpp"
#include "agr/runtime/PlatformLayer.hpp"
#include "agr/runtime/SaveStore.hpp"
#include "agr/ui/UISystem.hpp"
#include "agr/physics2d/Physics2DWorld.hpp"
#include "agr/physics3d/Physics3DWorld.hpp"
#include "agr/runtime/Input.hpp"
#include "agr/scene/Scene.hpp"

namespace agr {
class GestureRecognizer;
namespace net {
class NetSession;
class Replicator;
}
class InputActions;
class NavigationSystem;
class Localization;

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

/// A variable seen by the debugger: its value is a short readable text.
struct ScriptVariable {
    std::string name;
    std::string value;
    bool upvalue = false;  ///< Captured from an enclosing function (else a local).
};

/// One Lua function on the call stack of a break (innermost first).
struct ScriptStackFrame {
    std::string function;  ///< "onUpdate", "?" for anonymous functions, "main chunk".
    std::string source;    ///< Script uri (assets://scripts/player.lua) or chunk name.
    int line = 0;
    std::vector<ScriptVariable> variables;  ///< Locals then upvalues (no temporaries, no _ENV).
};

/// Why and where a script stopped in the debugger.
struct ScriptBreak {
    enum class Reason : u8 { Breakpoint, Step, Pause, Error };
    Reason reason = Reason::Breakpoint;
    std::string source;
    int line = 0;
    std::string message;  ///< The error message (Reason::Error).
    std::vector<ScriptStackFrame> stack;
};

/// What the debugger does when the break handler returns.
enum class ScriptDebugAction : u8 {
    Continue,  ///< Run until the next breakpoint.
    StepOver,  ///< Next line of this function (or of its caller once it returns).
    StepInto,  ///< Next line run, entering called functions.
    StepOut,   ///< Next line of the caller.
    Stop,      ///< Raise an error in the script (the instance stops) — e.g. the editor leaves play mode.
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
    /// Touch gestures for agr.input.gestures / drag / pinch / tapped (null = none).
    void setGestures(const GestureRecognizer* gestures);
    /// Named actions for agr.input.down / pressed / released / value.
    void setActions(const InputActions* actions);
    /// Texts for agr.tr / agr.locale; `setLanguage` changes and remembers the language
    /// (defaults to Localization::setLanguage).
    void setLocalization(Localization* localization, std::function<bool(std::string_view)> setLanguage = {});
    /// Window size in input units (agr.screen.size()); 0 x 0 when headless.
    void setScreenSize(math::Vec2 size);
    void setPhysics2D(Physics2DWorld* world);
    void setPhysics3D(Physics3DWorld* world);
    /// Paths and agents for entity:navigateTo and agr.nav.
    void setNavigation(NavigationSystem* navigation);
    /// Multiplayer session for agr.net (null: agr.net raises an error).
    void setNet(net::NetSession* session);
    /// Entity sync, network spawns and RPCs (entity:rpc, agr.net.spawn...); null = offline.
    void setReplicator(net::Replicator* replicator);
    /// Runs an RPC received from `sender` (called by the Replicator): the function must be
    /// declared in the script's `rpc` table ("any", or "authority": from the owner or the host).
    bool callRpc(Scene& scene, ecs::Entity e, const std::string& function, const std::string& argsJson, u32 sender);
    /// agr.loadScene(uri): asks the host to switch scenes at the end of the frame.
    void setSceneLoader(std::function<void(std::string)> loader);
    /// agr.time.setScale / scale and hit stop (Engine::setTimeScale / timeScale).
    void setTimeControl(std::function<void(double)> setScale, std::function<double()> scale);
    void setAudio(AudioSystem* audio);
    void setUI(UISystem* ui);
    void setAnimation(AnimationSystem* animation);
    /// Device services for agr.device (vibration, safe area); null = desktop defaults.
    void setPlatform(PlatformLayer* platform);
    /// Local save for agr.save (null = the functions fail with a message).
    void setSaveStore(SaveStore* save);
    /// Maximum run time of one callback before it is stopped (infinite loops).
    void setCallbackBudget(f64 seconds);

    /// Creates instances for new Script components (onStart), destroys removed ones
    /// (onDestroy), then calls onUpdate.
    void update(Scene& scene, const FrameTime& time);
    void fixedUpdate(Scene& scene, const FrameTime& time);
    void dispatchCollision(Scene& scene, const Collision2DEvent& event);
    void dispatchCollision(Scene& scene, const Collision3DEvent& event);
    /// A joint broke: `onJointBreak(force)` on the script of the joint's entity.
    void dispatchJointBreak(Scene& scene, ecs::Entity entity, float force);
    /// UI: `onClick()` on the button's script, then `message(button)` on its target's script.
    void dispatchUI(Scene& scene, const UIClickEvent& event);
    /// UI: `onValueChanged(value)` on the slider's script, then `message(slider, value)` on its target's.
    void dispatchUI(Scene& scene, const UIValueChangedEvent& event);
    /// onTextChanged(text) / onSubmit(text) on the field; on submit, target:message(field, text).
    void dispatchUI(Scene& scene, const UITextEvent& event);
    /// Animation: onAnimationEvent(name) / onStateEnter(name) on the animated entity's script.
    void dispatchAnimation(Scene& scene, const AnimationEvent& event);
    /// Destroys every instance (onDestroy is called) — scene unloaded.
    void reset();
    u32 reloadChanged();

    /// Compiles a script without running instances (editor inspector, validation).
    ScriptInfo describe(const std::string& uri);
    /// Code completion from the live API: names in the table at `path` ("" = globals,
    /// "agr", "agr.physics2d", "string"...) or, for path ":", the entity methods. Sorted.
    std::vector<std::string> completions(std::string_view path);
    /// Syntax check of Lua code without running it: nullopt when it compiles, else the line
    /// (0 if unknown) and the message.
    struct SyntaxError {
        int line = 0;
        std::string message;
    };
    std::optional<SyntaxError> checkSyntax(std::string_view code, const std::string& name = "script");
    /// Runs a chunk in the sandbox (tests, console). Returns the error message on failure.
    Status run(std::string_view code, Scene* scene = nullptr);
    /// The `self` table of an entity's script as a debug string of its fields (tests, inspector).
    Result<std::string> field(ecs::Entity e, const std::string& name);

    ScriptStats stats() const;

    // ---- debugger
    /// Called (blocking, on the thread running the scripts) when a script stops on a
    /// breakpoint, a step, a pause request or an error; its answer resumes the script.
    /// Without a handler, breakpoints are ignored and scripts run at full speed.
    void setBreakHandler(std::function<ScriptDebugAction(const ScriptBreak&)> handler);
    /// Breakpoint on a line of a script (uri as in the Script component).
    void setBreakpoint(const std::string& uri, int line, bool enabled = true);
    bool hasBreakpoint(const std::string& uri, int line) const;
    void clearBreakpoints();
    /// All breakpoints, sorted by uri then line.
    std::vector<std::pair<std::string, int>> breakpoints() const;
    /// Stop in the handler when a callback raises an error, before it is reported (default on).
    void setBreakOnError(bool enabled);
    /// Stops at the next script line that runs.
    void requestPause();
    /// True while the break handler runs.
    bool inBreak() const;
    /// Evaluates a Lua expression in a frame of the current break (0 = innermost): its
    /// locals and upvalues are visible, assignments do not change them. Only while inBreak().
    Result<std::string> evaluate(std::string_view expression, usize frame = 0);

    struct Impl;

private:
    std::unique_ptr<Impl> m;
};

} // namespace agr
