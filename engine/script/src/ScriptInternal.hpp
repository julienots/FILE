#pragma once
// Shared between ScriptEngine.cpp (VM, classes, instances) and ScriptBindings.cpp (API).
#include <string>
#include <unordered_map>
#include <vector>

extern "C" {
#include <lauxlib.h>
#include <lua.h>
#include <lualib.h>
}

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
    Physics2DWorld* physics2d = nullptr;
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

} // namespace agr
