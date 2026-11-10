#pragma once
// Shared between ScriptEngine.cpp (VM, classes, instances) and ScriptBindings.cpp (API).
#include <string>
#include <unordered_map>
#include <vector>

// Lua is compiled as C++ (see AGRDependencies.cmake): no extern "C", errors are C++ exceptions.
#include <lauxlib.h>
#include <lua.h>
#include <lualib.h>

#include "agr/core/Time.hpp"
#include "agr/script/ScriptEngine.hpp"

namespace agr {

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
    math::Vec2 screenSize{0.0f, 0.0f};
    Physics2DWorld* physics2d = nullptr;
    Physics3DWorld* physics3d = nullptr;
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
    void resetInstances();
    void dispatch(Scene& scene, ecs::Entity a, ecs::Entity b, bool begin, bool trigger);
    /// Calls target_script:function([sender], [value]) if the target has a started script defining it.
    void callOn(Scene& scene, ecs::Entity target, const std::string& function, ecs::Entity sender, bool withValue, double value);
};

/// Registry key of the Impl pointer (light userdata).
inline const char kImplKey = 0;
ScriptEngine::Impl& implOf(lua_State* L);

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

} // namespace agr
