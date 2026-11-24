#pragma once
#include <atomic>
#include <thread>
#include <memory>
#include <map>
#include <set>
#include <functional>
#include "agr/runtime/InputActions.hpp"
#include "agr/script/Rewind.hpp"
#include "agr/runtime/Gestures.hpp"
#include "agr/runtime/Localization.hpp"
#include "agr/scene/PhysicsLayers.hpp"
// Shared between ScriptEngine.cpp (VM, classes, instances) and ScriptBindings.cpp (API).
#include <string>
#include <unordered_map>
#include <vector>

// Lua is compiled as C++ (see AGRDependencies.cmake): no extern "C", errors are C++ exceptions.
#include <lauxlib.h>
#include <lua.h>
#include <lualib.h>
#include <nlohmann/json.hpp>

#include "agr/core/Time.hpp"
#include "agr/script/ScriptEngine.hpp"
#include "agr/net/Sockets.hpp"

namespace agr {
namespace net {
class HttpClient;
class WebSocket;
}
namespace net {
class NetSession;
class Replicator;
}
class NavigationSystem;

/// Game-feel effects driven by the script engine (ScriptFx.cpp): tweens, particle bursts,
/// camera shake, floating texts, screen flash / fade, hit stop.
struct FxState {
    struct Tween {
        u64 id = 0;
        ecs::Entity entity;
        std::string component, field;  // reflected field ("Transform"/"position"...); field "rotation" of Transform = Euler degrees
        bool euler = false;
        u32 mask = 0xF;                // components written (alpha: 0x8)
        math::Vec4 from{}, to{};
        bool hasFrom = false, started = false, fresh = true;  // fresh: created this frame, runs from the next one
        float duration = 0.3f, delay = 0.0f, t = 0.0f;
        int ease = 0;
        int cycles = 1;                // remaining cycles (-1 = forever)
        bool yoyo = false, backward = false, unscaled = false;
        int onDone = LUA_NOREF;
    };
    struct Temporary {  // destroyed after a delay (particle bursts)
        ecs::Entity entity;
        float remaining = 0.0f;
    };
    struct Popup {
        ecs::Entity entity;
        math::Vec3 world;
        float age = 0.0f, duration = 0.9f, rise = 1.2f, size = 52.0f;
        math::Vec4 color{1.0f, 1.0f, 1.0f, 1.0f};
    };
    std::vector<Tween> tweens;
    std::vector<Temporary> temporaries;
    std::vector<Popup> popups;
    u64 nextTween = 1;
    ecs::Entity canvas, overlay;
    math::Vec4 overlayColor{0.0f, 0.0f, 0.0f, 0.0f};
    float fadeFrom = 0.0f, fadeTo = 0.0f, fadeTime = 0.0f, fadeDuration = 0.0f;  // persistent fade
    float flash = 0.0f, flashDuration = 0.0f;                                     // flash on top of the fade
    math::Vec4 flashColor{1.0f, 1.0f, 1.0f, 1.0f};
    float trauma = 0.0f, traumaDecay = 1.0f, shakeTime = 0.0f;
    ecs::Entity shakeCamera;
    math::Vec3 shakeOffset{0.0f, 0.0f, 0.0f};
    float hitStop = 0.0f;
    double resumeScale = 1.0;
    bool scaleTouched = false;  // the game changed the time scale: back to 1 when it stops
    u32 profileDepth = 0;       // agr.profile sections opened by scripts this frame
};

struct ScriptEngine::Impl {
    struct Class {
        int ref = LUA_NOREF;  // the table returned by the script
        i64 modifiedTime = 0;
        u32 version = 0;
        std::string error;
    };
    struct Instance {
        ecs::Entity entity;
        std::string uri;
        int ref = LUA_NOREF;  // `self`
        u32 classVersion = 0;
        bool started = false;
        bool faulted = false;
    };

    VirtualFileSystem& vfs;
    lua_State* L = nullptr;
    const Input* input = nullptr;
    const GestureRecognizer* gestures = nullptr;
    InputActions* actions = nullptr;
    Localization* localization = nullptr;
    std::function<bool(std::string_view)> setLanguage;  ///< Engine::setLanguage (remembers the choice)
    math::Vec2 screenSize{0.0f, 0.0f};
    Physics2DWorld* physics2d = nullptr;
    RewindRecorder rewind;  // agr.rewind: history of Rewind entities, ghosts
    Physics3DWorld* physics3d = nullptr;
    PhysicsLayers physicsLayers;  ///< Names of assets://physics_layers.json (agr.physics*.layer / mask), refreshed on use.
    NavigationSystem* navigation = nullptr;
    net::NetSession* net = nullptr;
    std::shared_ptr<net::HttpClient> http;         ///< agr.http (created on first use)
    std::unordered_map<u64, int> httpCallbacks;    ///< request id -> Lua callback ref
    struct WebSocketHandle {
        std::shared_ptr<net::WebSocket> ws;
        int handlers = -2;  ///< LUA_NOREF: table {onOpen, onMessage, onClose}
        bool opened = false;
    };
    std::unordered_map<u64, WebSocketHandle> webSockets;  ///< agr.ws handles
    /// agr.tcp / agr.udp / agr.wsserver handles (closed when the game stops).
    struct Sockets {
        std::unordered_map<u64, std::unique_ptr<net::TcpServer>> tcpServers;
        std::unordered_map<u64, std::unique_ptr<net::TcpConnection>> tcp;
        std::unordered_map<u64, std::unique_ptr<net::UdpSocket>> udp;
        std::unordered_map<u64, std::unique_ptr<net::WebSocketServer>> wsServers;
        u64 next = 1;
        // agr.upnp: one router job at a time, on a worker thread.
        struct UpnpJob {
            std::atomic<bool> done{false};
            bool ok = false;
            std::string externalAddress, error;
            std::thread worker;
            ~UpnpJob() {
                if (worker.joinable()) worker.join();
            }
        };
        std::shared_ptr<UpnpJob> upnp;
        void clear() {
            upnp.reset();
            tcpServers.clear();
            tcp.clear();
            udp.clear();
            wsServers.clear();
        }
    } sockets;
    u64 nextWebSocket = 1;
    net::Replicator* replicator = nullptr;
    /// Runs `fn` of the entity's script for an RPC. `remote`: checks the script's rpc table
    /// ("any" / "authority"); returns false when refused or missing.
    bool invokeRpc(Scene& scene, ecs::Entity e, const std::string& fn, const nlohmann::json& args, u32 sender, bool remote);
    /// The rpc declaration of a function of the entity's script ("" when none).
    std::string rpcMode(ecs::Entity e, const std::string& fn);
    std::function<void(std::string)> sceneLoader;
    bool dedicatedServer = false;
    ScriptEngine::AsyncSceneLoading asyncScenes;
    std::function<void(double)> setTimeScale;  ///< Engine::setTimeScale (agr.time.setScale, hit stop)
    std::function<double()> timeScale;
    FxState fx;
    AudioSystem* audio = nullptr;
    UISystem* ui = nullptr;
    AnimationSystem* animation = nullptr;
    PlatformLayer* platform = nullptr;
    SaveStore* save = nullptr;
    Scene* scene = nullptr;          // valid during update / fixedUpdate / dispatch / run
    FrameTime time;
    u64 sceneGeneration = 0;
    f64 budgetSeconds = 2.0;
    Stopwatch callClock;
    bool inCall = false;
    const char* callbackName = nullptr;  ///< Callback being called (names the outermost frame in the debugger).
    struct Debugger {
        std::function<ScriptDebugAction(const ScriptBreak&)> handler;
        std::unordered_map<std::string, std::set<int>> breakpoints;  // uri -> lines
        std::unordered_map<int, int> lineCount;  // line -> breakpoints on it (fast reject in the hook)
        enum class Step : u8 { None, Into, Over, Out, Pause } step = Step::None;
        int stepDepth = 0;
        bool breakOnError = true;
        bool inBreak = false;
        std::vector<int> frameLevels;  // lua_getstack levels of the frames of the current break
        struct Options {
            std::string condition;  // stops only when true
            int hitCount = 0;       // stops from the Nth time the line runs
            std::string log;        // logpoint: writes this ({expressions} filled in) and does not stop
            int hits = 0;
        };
        std::map<std::pair<std::string, int>, Options> options;
        bool inCondition = false;
    } dbg;
    std::unordered_map<std::string, Class> classes;
    std::unordered_map<u64, Instance> instances;
    std::unordered_map<std::string, int> modules;  // require() cache (registry refs)
    ScriptStats stats;

    explicit Impl(VirtualFileSystem& v);
    ~Impl();

    Class* loadClass(const std::string& uri);
    Class* classFor(const std::string& uri);
    void destroyInstance(Instance& inst, bool callOnDestroy);
    /// Calls self:name(args...) where `pushArgs` pushes the arguments and returns their count.
    template <typename PushArgs>
    bool call(Instance& inst, const char* name, PushArgs&& pushArgs);
    void reportError(const std::string& where, const char* message);
    /// Calls agr.<function>(arg) of the Lua runtime (arg < 0: no argument).
    void callRuntime(const char* function, double arg);
    /// Line hook only while the debugger needs it (breakpoints or stepping): the count hook stays on.
    void updateHook();
    /// Builds the break description, calls the handler (blocking) and applies its answer.
    ScriptDebugAction debugBreak(ScriptBreak::Reason reason, const std::string& message);
    void resetInstances();
    void dispatch(Scene& scene, ecs::Entity a, ecs::Entity b, bool begin, bool trigger);
    /// Calls target_script:function([sender], [value]) if the target has a started script defining it.
    void callOn(Scene& scene, ecs::Entity target, const std::string& function, ecs::Entity sender, bool withValue, double value);
};

/// Registry key of the Impl pointer (light userdata).
inline const char kImplKey = 0;
ScriptEngine::Impl& implOf(lua_State* L);
/// Count hook (watchdog) + line hook (debugger), ScriptDebugger.cpp.
void debugHook(lua_State* L, lua_Debug* ar);
/// A callback raised an error: break in the debugger if it is attached.
void debugError(lua_State* L, const char* message);

/// Entity userdata: the entity plus the scene generation it belongs to.
struct LuaEntity {
    u64 packed;
    u64 generation;
};
void pushEntity(lua_State* L, ecs::Entity e);
/// The entity at `idx`; raises a Lua error if it is not an entity or no longer exists.
ecs::Entity checkEntity(lua_State* L, int idx);
/// Installs the `agr` API, the Entity type and the sandboxed standard library.
void openAgrLibrary(lua_State* L);
/// The running scene (raises a Lua error outside of a scene update).
Scene& sceneOf(lua_State* L);
/// Adds methods to the Entity type / a sub-table agr.<name> (called after openAgrLibrary's core setup).
void addEntityMethods(lua_State* L, const luaL_Reg* methods);
void addAgrLibrary(lua_State* L, const char* name, const luaL_Reg* funcs);
/// Adds functions to an existing table agr.<name> (e.g. "input").
void extendAgrLibrary(lua_State* L, const char* name, const luaL_Reg* funcs);
/// API extensions, one file each: agr.audio and Entity:playSound()...
void openAudioBindings(lua_State* L);
void openUIBindings(lua_State* L);
void openAnimationBindings(lua_State* L);
void openMobileBindings(lua_State* L);
void openNavigationBindings(lua_State* L);
void openGameBindings(lua_State* L);
/// Calls the callbacks of finished agr.http requests (once per frame).
void dispatchHttp(ScriptEngine::Impl& m);
void openNetBindings(lua_State* L);
void openSocketBindings(lua_State* L);
void openFxBindings(lua_State* L);
void openRuntimeBindings(lua_State* L);
/// Effects frame hooks (ScriptFx.cpp): before the scripts' onUpdate (removes last frame's camera
/// shake), after it (tweens, effects, new shake), and when the scene changes.
void fxBeginFrame(ScriptEngine::Impl& m, Scene& scene);
void fxEndFrame(ScriptEngine::Impl& m, Scene& scene, const FrameTime& time);
void fxReset(ScriptEngine::Impl& m);
/// Lua value <-> JSON (agr.save, agr.net): numbers, strings, booleans, arrays, string-keyed tables.
nlohmann::json toJson(lua_State* L, int idx, int depth = 0);
void pushJson(lua_State* L, const nlohmann::json& j);

} // namespace agr
