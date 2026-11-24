#include <set>
#include <regex>
#include <algorithm>
#include <cstring>
#include <unordered_set>
#include <cstdio>

#include <nlohmann/json.hpp>

#include "ScriptInternal.hpp"

#include "lstate.h"
#include "lobject.h"
#include "lopcodes.h"
#include "lfunc.h"
#include "ldebug.h"
#include "lstring.h"
#include "agr/script/LogicGraph.hpp"
#include "agr/core/Log.hpp"
#include "agr/core/Profiler.hpp"
#include "agr/script/ScriptComponent.hpp"
#include "agr/net/Replication.hpp"
#include "agr/net/NetSession.hpp"

namespace agr {

namespace {

int traceback(lua_State* L) {
    const char* msg = lua_tostring(L, 1);
    debugError(L, msg);  // the debugger sees the error where it happened
    luaL_traceback(L, L, msg ? msg : "(error object is not a string)", 1);
    return 1;
}

} // namespace

ScriptEngine::Impl& implOf(lua_State* L) {
    lua_rawgetp(L, LUA_REGISTRYINDEX, &kImplKey);
    auto* impl = static_cast<ScriptEngine::Impl*>(lua_touserdata(L, -1));
    lua_pop(L, 1);
    return *impl;
}

ScriptEngine::Impl::Impl(VirtualFileSystem& v) : vfs(v) {
    L = luaL_newstate();
    lua_pushlightuserdata(L, this);
    lua_rawsetp(L, LUA_REGISTRYINDEX, &kImplKey);
    openAgrLibrary(L);
    updateHook();
}

ScriptEngine::Impl::~Impl() {
    scene = nullptr;  // may already be destroyed: no onDestroy callbacks
    resetInstances();
    lua_close(L);
}

void ScriptEngine::Impl::callRuntime(const char* function, double arg) {
    lua_pushcfunction(L, traceback);
    lua_getglobal(L, "agr");
    if (lua_getfield(L, -1, function) != LUA_TFUNCTION) {
        lua_pop(L, 3);
        return;
    }
    lua_remove(L, -2);  // agr
    int nargs = 0;
    if (arg >= 0.0) {
        lua_pushnumber(L, arg);
        nargs = 1;
    }
    if (lua_pcall(L, nargs, 0, -2 - nargs) != LUA_OK) {
        reportError(std::string("agr.") + function, lua_tostring(L, -1));
        lua_pop(L, 1);
    }
    lua_pop(L, 1);  // traceback
}

void ScriptEngine::Impl::reportError(const std::string& where, const char* message) {
    ++stats.errors;
    AGR_LOG_ERROR("Script", "{}: {}", where, message ? message : "unknown error");
}

ScriptEngine::Impl::Class* ScriptEngine::Impl::loadClass(const std::string& uri) {
    Class& c = classes[uri];
    c.modifiedTime = vfs.modifiedTime(uri);
    auto text = vfs.readText(uri);
    std::string error;
    if (text && uri.size() > 9 && uri.compare(uri.size() - 9, 9, ".agrlogic") == 0) {
        // Visual logic graph: compiled to Lua here (the editor shows the same errors).
        auto graph = logic::Graph::fromJson(text.value());
        if (!graph) {
            text = Result<std::string>(graph.error());
        } else {
            logic::CompileResult compiled = logic::compile(graph.value(), uri);
            if (!compiled.ok()) {
                std::string all;
                for (const std::string& e : compiled.errors) all += (all.empty() ? "" : "; ") + e;
                text = Result<std::string>(makeError(Error::Code::ParseError, all));
            } else {
                text = Result<std::string>(std::move(compiled.lua));
            }
        }
    }
    if (!text) {
        error = text.error().message;
    } else {
        lua_pushcfunction(L, traceback);
        const std::string chunkName = "@" + uri;
        // Text only: precompiled bytecode could crash the VM.
        if (luaL_loadbufferx(L, text.value().data(), text.value().size(), chunkName.c_str(), "t") != LUA_OK) {
            error = lua_tostring(L, -1);
            lua_pop(L, 2);
        } else {
            callbackName = nullptr;
            inCall = true;
            callClock.restart();
            const int r = lua_pcall(L, 0, 1, -2);
            inCall = false;
            if (r != LUA_OK) {
                error = lua_tostring(L, -1);
                lua_pop(L, 2);
            } else if (!lua_istable(L, -1)) {
                error = "the script must return a table (e.g. local Player = {} ... return Player)";
                lua_pop(L, 2);
            } else {
                if (c.ref != LUA_NOREF) luaL_unref(L, LUA_REGISTRYINDEX, c.ref);
                c.ref = luaL_ref(L, LUA_REGISTRYINDEX);
                lua_pop(L, 1);  // traceback
                ++c.version;
                c.error.clear();
                return &c;
            }
        }
    }
    c.error = error;
    reportError(uri, error.c_str());
    return c.ref != LUA_NOREF ? &c : nullptr;  // keep the last good version when a reload fails
}

ScriptEngine::Impl::Class* ScriptEngine::Impl::classFor(const std::string& uri) {
    auto it = classes.find(uri);
    if (it != classes.end()) return it->second.ref != LUA_NOREF ? &it->second : nullptr;
    return loadClass(uri);
}

template <typename PushArgs>
bool ScriptEngine::Impl::call(Instance& inst, const char* name, PushArgs&& pushArgs) {
    if (inst.faulted || inst.ref == LUA_NOREF) return false;
    const int top = lua_gettop(L);
    lua_pushcfunction(L, traceback);
    lua_rawgeti(L, LUA_REGISTRYINDEX, inst.ref);
    lua_getfield(L, -1, name);
    if (!lua_isfunction(L, -1)) {
        lua_settop(L, top);
        return true;
    }
    lua_insert(L, -2);  // function, self
    const int nargs = pushArgs();
    callbackName = name;
    inCall = true;
    callClock.restart();
    const int r = lua_pcall(L, 1 + nargs, 0, top + 1);
    inCall = false;
    if (r != LUA_OK) {
        std::string who = "entity";
        if (scene && scene->valid(inst.entity)) who = "'" + scene->nameOf(inst.entity) + "'";
        reportError(inst.uri + " (" + std::string(name) + ", " + who + ")", lua_tostring(L, -1));
        inst.faulted = true;  // paused until the script file changes
    }
    lua_settop(L, top);
    return r == LUA_OK;
}

void ScriptEngine::Impl::destroyInstance(Instance& inst, bool callOnDestroy) {
    if (callOnDestroy && inst.started) call(inst, "onDestroy", [] { return 0; });
    if (inst.ref != LUA_NOREF) luaL_unref(L, LUA_REGISTRYINDEX, inst.ref);
    inst.ref = LUA_NOREF;
}

void ScriptEngine::Impl::resetInstances() {
    fxReset(*this);
    // onDestroy only while the instances' scene is still loaded (not after a reload in place).
    const bool alive = scene && scene->generation() == sceneGeneration;
    std::vector<u64> keys;
    for (auto& [k, inst] : instances) keys.push_back(k);
    std::sort(keys.begin(), keys.end());
    for (u64 k : keys) destroyInstance(instances[k], alive && scene->valid(instances[k].entity));
    instances.clear();
}

// ====================================================================== API
ScriptEngine::ScriptEngine(VirtualFileSystem& vfs) : m(std::make_unique<Impl>(vfs)) {}
ScriptEngine::~ScriptEngine() = default;

void ScriptEngine::setInput(const Input* input) { m->input = input; }
void ScriptEngine::setActions(InputActions* actions) { m->actions = actions; }
void ScriptEngine::setLocalization(Localization* localization, std::function<bool(std::string_view)> setLanguage) {
    m->localization = localization;
    m->setLanguage = setLanguage ? std::move(setLanguage)
                                 : std::function<bool(std::string_view)>([localization](std::string_view c) { return localization && localization->setLanguage(c); });
}
void ScriptEngine::setScreenSize(math::Vec2 size) { m->screenSize = size; }
void ScriptEngine::setPhysics2D(Physics2DWorld* world) { m->physics2d = world; }
void ScriptEngine::setPhysics3D(Physics3DWorld* world) { m->physics3d = world; }
void ScriptEngine::setNavigation(NavigationSystem* navigation) { m->navigation = navigation; }
void ScriptEngine::setNet(net::NetSession* session) { m->net = session; }
void ScriptEngine::setReplicator(net::Replicator* replicator) { m->replicator = replicator; }

std::string ScriptEngine::Impl::rpcMode(ecs::Entity e, const std::string& fn) {
    auto it = instances.find(e.packed());
    if (it == instances.end() || it->second.ref == LUA_NOREF) return {};
    const int top = lua_gettop(L);
    lua_rawgeti(L, LUA_REGISTRYINDEX, it->second.ref);
    lua_getfield(L, -1, "rpc");  // the script's declarations (through the class)
    std::string mode;
    if (lua_istable(L, -1)) {
        lua_getfield(L, -1, fn.c_str());
        if (lua_isstring(L, -1)) mode = lua_tostring(L, -1);
        else if (lua_toboolean(L, -1)) mode = "any";
    }
    lua_settop(L, top);
    return mode;
}

bool ScriptEngine::Impl::invokeRpc(Scene& sc, ecs::Entity e, const std::string& fn, const nlohmann::json& args, u32 sender, bool remote) {
    auto it = instances.find(e.packed());
    if (it == instances.end() || !it->second.started) return false;
    if (remote) {
        const std::string mode = rpcMode(e, fn);
        if (mode.empty()) {
            AGR_LOG_WARN("Script", "RPC '{}' refused: not declared in the rpc table of {}", fn, it->second.uri);
            return false;
        }
        if (mode.rfind("authority", 0) == 0 && replicator) {
            const u32 owner = replicator->ownerOf(sc, e);
            if (sender != owner && sender != net::NetSession::kHostId) {
                AGR_LOG_WARN("Script", "RPC '{}' refused: player {} is not the owner ({})", fn, sender, owner);
                return false;
            }
        }
    }
    scene = &sc;
    call(it->second, fn.c_str(), [&] {
        int n = 0;
        if (args.is_array())
            for (const auto& a : args) {
                pushJson(L, a);
                ++n;
            }
        return n;
    });
    return true;
}

bool ScriptEngine::callRpc(Scene& scene, ecs::Entity e, const std::string& function, const std::string& argsJson, u32 sender) {
    const nlohmann::json args = nlohmann::json::parse(argsJson, nullptr, false);
    return m->invokeRpc(scene, e, function, args.is_discarded() ? nlohmann::json::array() : args, sender, true);
}
bool ScriptEngine::callNetInput(Scene& scene, ecs::Entity e, const std::string& inputJson, float dt) {
    nlohmann::json args = nlohmann::json::array();
    const nlohmann::json input = nlohmann::json::parse(inputJson, nullptr, false);
    args.push_back(input.is_discarded() ? nlohmann::json::object() : input);
    args.push_back(dt);
    return m->invokeRpc(scene, e, "onNetInput", args, 0, false);
}
void ScriptEngine::setGestures(const GestureRecognizer* gestures) { m->gestures = gestures; }
void ScriptEngine::setSceneLoader(std::function<void(std::string)> loader) { m->sceneLoader = std::move(loader); }
void ScriptEngine::setDedicatedServer(bool server) { m->dedicatedServer = server; }
void ScriptEngine::setAsyncSceneLoading(AsyncSceneLoading loading) { m->asyncScenes = std::move(loading); }
void ScriptEngine::setTimeControl(std::function<void(double)> setScale, std::function<double()> scale) {
    m->setTimeScale = std::move(setScale);
    m->timeScale = std::move(scale);
}
void ScriptEngine::setAudio(AudioSystem* audio) { m->audio = audio; }
void ScriptEngine::setUI(UISystem* ui) { m->ui = ui; }
void ScriptEngine::setAnimation(AnimationSystem* animation) { m->animation = animation; }
void ScriptEngine::setPlatform(PlatformLayer* platform) { m->platform = platform; }
void ScriptEngine::setSaveStore(SaveStore* save) { m->save = save; }
void ScriptEngine::setCallbackBudget(f64 seconds) { m->budgetSeconds = std::max(seconds, 0.001); }

void ScriptEngine::reset() {
    m->resetInstances();
    m->sockets.clear();  // the game stopped: its listening ports are released
    m->sceneGeneration = 0;
}

void ScriptEngine::update(Scene& scene, const FrameTime& time) {
    Stopwatch sw;
    m->scene = &scene;
    m->time = time;
    lua_State* L = m->L;
    if (scene.generation() != m->sceneGeneration) {
        // The previous scene's entities are gone: drop their instances without onDestroy.
        for (auto& [key, inst] : m->instances)
            if (inst.ref != LUA_NOREF) luaL_unref(L, LUA_REGISTRYINDEX, inst.ref);
        m->instances.clear();
        m->sceneGeneration = scene.generation();
        fxReset(*m);
        m->callRuntime("_sceneChanged", -1.0);
    }
    fxBeginFrame(*m, scene);
    dispatchHttp(*m);
    // 1. Sync instances with the Script components.
    std::vector<u64> seen;
    std::vector<u64> created;
    scene.registry().view<ScriptComponent>().each([&](ecs::Entity e, ScriptComponent& sc) {
        if (!sc.enabled || sc.script.empty() || !scene.isActiveInHierarchy(e)) return;
        const u64 key = e.packed();
        seen.push_back(key);
        auto it = m->instances.find(key);
        if (it != m->instances.end() && it->second.uri == sc.script) return;
        if (it != m->instances.end()) {  // script replaced
            m->destroyInstance(it->second, true);
            m->instances.erase(it);
        }
        Impl::Instance inst;
        inst.entity = e;
        inst.uri = sc.script;
        Impl::Class* cls = m->classFor(sc.script);
        if (!cls) {
            inst.faulted = true;  // compile error, reported by loadClass
            m->instances.emplace(key, std::move(inst));
            return;
        }
        // self = setmetatable({entity = e, <properties>}, {__index = class})
        lua_newtable(L);
        lua_newtable(L);
        lua_rawgeti(L, LUA_REGISTRYINDEX, cls->ref);
        lua_setfield(L, -2, "__index");
        lua_setmetatable(L, -2);
        pushEntity(L, e);
        lua_setfield(L, -2, "entity");
        const nlohmann::json overrides = nlohmann::json::parse(sc.properties.empty() ? "{}" : sc.properties, nullptr, false);
        lua_rawgeti(L, LUA_REGISTRYINDEX, cls->ref);
        if (lua_getfield(L, -1, "properties") == LUA_TTABLE) {
            lua_pushnil(L);
            while (lua_next(L, -2)) {  // key at -2, default value at -1
                if (lua_type(L, -2) == LUA_TSTRING) {
                    const std::string name = lua_tostring(L, -2);
                    const auto o = overrides.is_object() ? overrides.find(name) : overrides.end();
                    const bool has = overrides.is_object() && o != overrides.end();
                    if (lua_type(L, -1) == LUA_TNUMBER && has && o->is_number()) lua_pushnumber(L, o->get<double>());
                    else if (lua_type(L, -1) == LUA_TBOOLEAN && has && o->is_boolean()) lua_pushboolean(L, o->get<bool>());
                    else if (lua_type(L, -1) == LUA_TSTRING && has && o->is_string()) lua_pushstring(L, o->get<std::string>().c_str());
                    else lua_pushvalue(L, -1);
                    lua_setfield(L, -6, name.c_str());  // self[name] = value
                }
                lua_pop(L, 1);
            }
        }
        lua_pop(L, 2);  // properties, class
        inst.ref = luaL_ref(L, LUA_REGISTRYINDEX);
        inst.classVersion = cls->version;
        m->instances.emplace(key, std::move(inst));
        created.push_back(key);
    });
    // 2. Instances whose component or entity disappeared.
    std::sort(seen.begin(), seen.end());
    std::vector<u64> removed;
    for (auto& [key, inst] : m->instances)
        if (!std::binary_search(seen.begin(), seen.end(), key)) removed.push_back(key);
    std::sort(removed.begin(), removed.end());
    for (u64 key : removed) {
        m->destroyInstance(m->instances[key], scene.valid(m->instances[key].entity));
        m->instances.erase(key);
    }
    // 3. onStart, then onUpdate (entity order: deterministic).
    std::sort(created.begin(), created.end(), [](u64 a, u64 b) { return static_cast<u32>(a) < static_cast<u32>(b); });
    for (u64 key : created) {
        auto it = m->instances.find(key);
        if (it == m->instances.end()) continue;
        it->second.started = true;
        m->call(it->second, "onStart", [] { return 0; });
    }
    std::vector<u64> order;
    order.reserve(m->instances.size());
    for (auto& [key, inst] : m->instances)
        if (inst.started && !inst.faulted) order.push_back(key);
    std::sort(order.begin(), order.end(), [](u64 a, u64 b) { return static_cast<u32>(a) < static_cast<u32>(b); });
    const double dt = time.deltaSeconds;
    for (u64 key : order) {
        auto it = m->instances.find(key);
        if (it == m->instances.end() || !scene.valid(it->second.entity)) continue;
        AGR_PROFILE_SCOPE(it->second.uri);  // one line per script class in the profiler
        m->call(it->second, "onUpdate", [&] {
            lua_pushnumber(L, dt);
            return 1;
        });
    }
    m->callRuntime("_tick", dt);  // timers, coroutines, tweens
    fxEndFrame(*m, scene, time);
    m->stats.updateMs = sw.elapsedMilliseconds();
}

void ScriptEngine::fixedUpdate(Scene& scene, const FrameTime& time) {
    if (scene.generation() != m->sceneGeneration) return;  // instances are created by update()
    m->scene = &scene;
    m->time = time;
    std::vector<u64> order;
    for (auto& [key, inst] : m->instances)
        if (inst.started && !inst.faulted) order.push_back(key);
    std::sort(order.begin(), order.end(), [](u64 a, u64 b) { return static_cast<u32>(a) < static_cast<u32>(b); });
    for (u64 key : order) {
        auto it = m->instances.find(key);
        if (it == m->instances.end() || !scene.valid(it->second.entity)) continue;
        m->call(it->second, "onFixedUpdate", [&] {
            lua_pushnumber(m->L, time.fixedDeltaSeconds);
            return 1;
        });
    }
}

void ScriptEngine::Impl::dispatch(Scene& s, ecs::Entity a, ecs::Entity b, bool begin, bool trigger) {
    if (s.generation() != sceneGeneration) return;
    scene = &s;
    const char* callback = trigger ? "onTrigger" : "onCollision";
    auto notify = [&](ecs::Entity self, ecs::Entity other) {
        auto it = instances.find(self.packed());
        if (it == instances.end() || !it->second.started) return;
        call(it->second, callback, [&] {
            pushEntity(L, other);
            lua_pushboolean(L, begin);
            return 2;
        });
    };
    notify(a, b);
    notify(b, a);
}

void ScriptEngine::Impl::callOn(Scene& s, ecs::Entity target, const std::string& function, ecs::Entity sender, bool withValue, double value) {
    if (s.generation() != sceneGeneration || function.empty() || target.isNull() || !s.valid(target)) return;
    scene = &s;
    auto it = instances.find(target.packed());
    if (it == instances.end() || !it->second.started) return;
    call(it->second, function.c_str(), [&] {
        int n = 0;
        if (!sender.isNull()) {
            pushEntity(L, sender);
            ++n;
        }
        if (withValue) {
            lua_pushnumber(L, value);
            ++n;
        }
        return n;
    });
}

void ScriptEngine::dispatchUI(Scene& scene, const UIClickEvent& e) {
    if (!scene.valid(e.entity)) return;
    m->callOn(scene, e.entity, "onClick", {}, false, 0.0);
    if (const auto* b = scene.registry().tryGet<UIButtonComponent>(e.entity)) m->callOn(scene, b->target, b->message, e.entity, false, 0.0);
}

void ScriptEngine::dispatchUI(Scene& scene, const UIValueChangedEvent& e) {
    if (!scene.valid(e.entity)) return;
    m->callOn(scene, e.entity, "onValueChanged", {}, true, e.value);
    if (const auto* sl = scene.registry().tryGet<UISliderComponent>(e.entity))
        m->callOn(scene, sl->target, sl->message, e.entity, true, e.value);
    if (const auto* t = scene.registry().tryGet<UIToggleComponent>(e.entity)) m->callOn(scene, t->target, t->message, e.entity, true, e.value);
    if (const auto* d = scene.registry().tryGet<UIDropdownComponent>(e.entity)) m->callOn(scene, d->target, d->message, e.entity, true, e.value);
    if (const auto* t = scene.registry().tryGet<UITabsComponent>(e.entity)) m->callOn(scene, t->target, t->message, e.entity, true, e.value);
    if (const auto* l = scene.registry().tryGet<UIItemListComponent>(e.entity)) m->callOn(scene, l->target, l->message, e.entity, true, e.value);
    if (const auto* d = scene.registry().tryGet<UIDialogComponent>(e.entity)) m->callOn(scene, d->target, d->message, e.entity, true, e.value);
    if (const auto* c = scene.registry().tryGet<UIColorPickerComponent>(e.entity)) m->callOn(scene, c->target, c->message, e.entity, true, e.value);
}

void ScriptEngine::dispatchUI(Scene& scene, const UITextEvent& e) {
    if (!scene.valid(e.entity) || scene.generation() != m->sceneGeneration) return;
    m->scene = &scene;
    auto callWithText = [&](ecs::Entity target, const std::string& fn, ecs::Entity sender) {
        if (fn.empty() || target.isNull() || !scene.valid(target)) return;
        auto it = m->instances.find(target.packed());
        if (it == m->instances.end() || !it->second.started) return;
        m->call(it->second, fn.c_str(), [&] {
            int n = 0;
            if (!sender.isNull()) {
                pushEntity(m->L, sender);
                ++n;
            }
            lua_pushlstring(m->L, e.text.data(), e.text.size());
            return n + 1;
        });
    };
    if (e.link) {  // a [url=id] of a rich text: the text's script, then its parents' (a panel handling its links)
        for (ecs::Entity t = e.entity; !t.isNull() && scene.valid(t); t = scene.parent(t))
            if (m->instances.count(t.packed())) {
                callWithText(t, "onLinkClicked", {});
                break;
            }
        return;
    }
    callWithText(e.entity, e.submitted ? "onSubmit" : "onTextChanged", {});
    if (e.submitted)
        if (const auto* f = scene.registry().tryGet<UIInputFieldComponent>(e.entity)) callWithText(f->target, f->message, e.entity);
}

void ScriptEngine::dispatchAnimation(Scene& scene, const AnimationEvent& e) {
    if (scene.generation() != m->sceneGeneration || !scene.valid(e.entity)) return;
    m->scene = &scene;
    auto it = m->instances.find(e.entity.packed());
    if (it == m->instances.end() || !it->second.started) return;
    const char* fn = e.kind == AnimationEvent::Kind::StateEnter ? "onStateEnter" : "onAnimationEvent";
    m->call(it->second, fn, [&] {
        lua_pushstring(m->L, e.name.c_str());
        if (e.layer.empty()) return 1;
        lua_pushstring(m->L, e.layer.c_str());  // second argument: the layer (nil for the base layer)
        return 2;
    });
}

void ScriptEngine::dispatchJointBreak(Scene& scene, ecs::Entity entity, float force) {
    if (scene.generation() != m->sceneGeneration || !scene.valid(entity)) return;
    m->scene = &scene;
    auto it = m->instances.find(entity.packed());
    if (it == m->instances.end() || !it->second.started) return;
    m->call(it->second, "onJointBreak", [&] {
        lua_pushnumber(m->L, force);
        return 1;
    });
}

void ScriptEngine::dispatchCollision(Scene& scene, const Collision2DEvent& event) { m->dispatch(scene, event.a, event.b, event.begin, event.trigger); }
void ScriptEngine::dispatchCollision(Scene& scene, const Collision3DEvent& event) { m->dispatch(scene, event.a, event.b, event.begin, event.trigger); }

u32 ScriptEngine::reloadChanged() {
    u32 n = 0;
    std::vector<std::string> changed;
    for (auto& [uri, c] : m->classes) {
        const i64 t = m->vfs.modifiedTime(uri);
        if (t != 0 && t != c.modifiedTime) changed.push_back(uri);
    }
    for (const std::string& uri : changed) {
        const u32 before = m->classes[uri].version;
        Impl::Class* c = m->loadClass(uri);
        if (!c || c->version == before) continue;  // failed: errors were reported, old code kept
        ++n;
        AGR_LOG_INFO("Script", "hot reload '{}'", uri);
        // Live instances keep their fields and switch to the new functions.
        for (auto& [key, inst] : m->instances) {
            if (inst.uri != uri) continue;
            inst.faulted = false;
            if (inst.ref == LUA_NOREF) continue;  // was never created (compile error): created on next sync
            lua_rawgeti(m->L, LUA_REGISTRYINDEX, inst.ref);
            lua_newtable(m->L);
            lua_rawgeti(m->L, LUA_REGISTRYINDEX, c->ref);
            lua_setfield(m->L, -2, "__index");
            lua_setmetatable(m->L, -2);
            lua_pop(m->L, 1);
            inst.classVersion = c->version;
        }
        // Instances that failed to compile are recreated (and started) on the next update.
        std::erase_if(m->instances, [&](const auto& kv) { return kv.second.uri == uri && kv.second.ref == LUA_NOREF; });
    }
    return n;
}

ScriptInfo ScriptEngine::describe(const std::string& uri) {
    ScriptInfo info;
    info.uri = uri;
    Impl::Class* c = m->classFor(uri);
    auto it = m->classes.find(uri);
    if (it != m->classes.end()) info.error = it->second.error;
    if (!c) return info;
    if (!info.error.empty()) return info;  // last good version exists but the file is broken
    info.valid = true;
    lua_State* L = m->L;
    lua_rawgeti(L, LUA_REGISTRYINDEX, c->ref);
    lua_pushnil(L);
    while (lua_next(L, -2)) {
        if (lua_type(L, -2) == LUA_TSTRING && lua_isfunction(L, -1)) info.callbacks.emplace_back(lua_tostring(L, -2));
        lua_pop(L, 1);
    }
    if (lua_getfield(L, -1, "properties") == LUA_TTABLE) {
        lua_pushnil(L);
        while (lua_next(L, -2)) {
            if (lua_type(L, -2) == LUA_TSTRING) {
                ScriptProperty p;
                p.name = lua_tostring(L, -2);
                const int t = lua_type(L, -1);
                if (t == LUA_TNUMBER) {
                    p.type = ScriptProperty::Type::Number;
                    p.number = lua_tonumber(L, -1);
                    info.properties.push_back(std::move(p));
                } else if (t == LUA_TBOOLEAN) {
                    p.type = ScriptProperty::Type::Boolean;
                    p.boolean = lua_toboolean(L, -1);
                    info.properties.push_back(std::move(p));
                } else if (t == LUA_TSTRING) {
                    p.type = ScriptProperty::Type::String;
                    p.string = lua_tostring(L, -1);
                    info.properties.push_back(std::move(p));
                }
            }
            lua_pop(L, 1);
        }
    }
    lua_pop(L, 2);
    std::sort(info.properties.begin(), info.properties.end(), [](const auto& a, const auto& b) { return a.name < b.name; });
    std::sort(info.callbacks.begin(), info.callbacks.end());
    return info;
}

Status ScriptEngine::run(std::string_view code, Scene* scene) {
    if (scene) {
        m->scene = scene;
        if (scene->generation() != m->sceneGeneration) {
            for (auto& [key, inst] : m->instances)
                if (inst.ref != LUA_NOREF) luaL_unref(m->L, LUA_REGISTRYINDEX, inst.ref);
            m->instances.clear();
            m->sceneGeneration = scene->generation();
        }
    }
    lua_State* L = m->L;
    lua_pushcfunction(L, traceback);
    if (luaL_loadbufferx(L, code.data(), code.size(), "=run", "t") != LUA_OK) {
        std::string err = lua_tostring(L, -1);
        lua_pop(L, 2);
        return makeError(Error::Code::ParseError, err);
    }
    m->callbackName = nullptr;
    m->inCall = true;
    m->callClock.restart();
    const int r = lua_pcall(L, 0, 0, -2);
    m->inCall = false;
    if (r != LUA_OK) {
        std::string err = lua_tostring(L, -1);
        lua_pop(L, 2);
        return makeError(Error::Code::Unknown, err);
    }
    lua_pop(L, 1);
    return {};
}

Result<std::string> ScriptEngine::field(ecs::Entity e, const std::string& name) {
    auto it = m->instances.find(e.packed());
    if (it == m->instances.end() || it->second.ref == LUA_NOREF) return makeError(Error::Code::NotFound, "no script instance");
    lua_State* L = m->L;
    lua_rawgeti(L, LUA_REGISTRYINDEX, it->second.ref);
    lua_getfield(L, -1, name.c_str());
    std::string out = luaL_tolstring(L, -1, nullptr);
    lua_pop(L, 3);
    return out;
}

ScriptStats ScriptEngine::stats() const {
    ScriptStats s = m->stats;
    s.instances = 0;
    s.faulted = 0;
    for (const auto& [k, inst] : m->instances) {
        if (inst.ref != LUA_NOREF) ++s.instances;
        if (inst.faulted) ++s.faulted;
    }
    s.memoryKb = static_cast<usize>(lua_gc(m->L, LUA_GCCOUNT, 0));
    return s;
}

std::vector<std::string> ScriptEngine::completions(std::string_view path) {
    lua_State* L = m->L;
    std::vector<std::string> out;
    const int top = lua_gettop(L);
    if (path == ":") {
        luaL_getmetatable(L, "agr.Entity");
        lua_getfield(L, -1, "__index");
    } else {
        lua_pushglobaltable(L);
        std::string_view rest = path;
        while (!rest.empty() && lua_istable(L, -1)) {
            const usize dot = rest.find('.');
            const std::string key(rest.substr(0, dot));
            lua_getfield(L, -1, key.c_str());
            lua_remove(L, -2);
            rest = dot == std::string_view::npos ? std::string_view{} : rest.substr(dot + 1);
        }
    }
    if (lua_istable(L, -1)) {
        lua_pushnil(L);
        while (lua_next(L, -2)) {
            if (lua_type(L, -2) == LUA_TSTRING) {
                std::string name = lua_tostring(L, -2);
                if (!name.empty() && name[0] != '_') out.push_back(std::move(name));
            }
            lua_pop(L, 1);
        }
    }
    lua_settop(L, top);
    std::sort(out.begin(), out.end());
    return out;
}

std::optional<ScriptEngine::SyntaxError> ScriptEngine::checkSyntax(std::string_view code, const std::string& name) {
    lua_State* L = m->L;
    const std::string chunk = "=" + name;
    if (luaL_loadbufferx(L, code.data(), code.size(), chunk.c_str(), "t") == LUA_OK) {
        lua_pop(L, 1);
        return std::nullopt;
    }
    SyntaxError e;
    e.message = lua_tostring(L, -1) ? lua_tostring(L, -1) : "syntax error";
    lua_pop(L, 1);
    // "name:LINE: message"
    const std::string prefix = name + ":";
    if (e.message.rfind(prefix, 0) == 0) {
        const usize colon = e.message.find(':', prefix.size());
        if (colon != std::string::npos) {
            e.line = std::atoi(e.message.substr(prefix.size(), colon - prefix.size()).c_str());
            e.message = e.message.substr(colon + 1);
            if (!e.message.empty() && e.message[0] == ' ') e.message.erase(0, 1);
        }
    }
    return e;
}

namespace {
/// Instructions of a function and its nested functions.
template <typename Fn>
void forEachProto(const Proto* p, Fn&& fn) {
    fn(p);
    for (int i = 0; i < p->sizep; ++i) forEachProto(p->p[i], fn);
}
std::string kString(const Proto* p, int k) {
    if (k < 0 || k >= p->sizek || !ttisstring(&p->k[k])) return {};
    return getstr(tsvalue(&p->k[k]));
}
bool isEnv(const Proto* p, int upvalue) {
    return upvalue >= 0 && upvalue < p->sizeupvalues && p->upvalues[upvalue].name && std::strcmp(getstr(p->upvalues[upvalue].name), "_ENV") == 0;
}
} // namespace

std::vector<ScriptEngine::SyntaxError> ScriptEngine::lint(std::string_view code, const std::string& name) {
    std::vector<SyntaxError> out;
    lua_State* L = m->L;
    const std::string chunk = "=" + name;
    if (luaL_loadbufferx(L, code.data(), code.size(), chunk.c_str(), "t") != LUA_OK) {
        lua_pop(L, 1);
        return out;
    }
    const LClosure* cl = clLvalue(s2v(L->top.p - 1));
    // Globals the script assigns somewhere count as defined.
    std::unordered_set<std::string> written;
    forEachProto(cl->p, [&](const Proto* p) {
        for (int pc = 0; pc < p->sizecode; ++pc) {
            const Instruction i = p->code[pc];
            if (GET_OPCODE(i) == OP_SETTABUP && isEnv(p, GETARG_A(i))) written.insert(kString(p, GETARG_B(i)));
        }
    });
    auto defined = [&](const std::string& global) {
        if (written.count(global)) return true;
        lua_getglobal(L, global.c_str());
        const bool ok = !lua_isnil(L, -1);
        lua_pop(L, 1);
        return ok;
    };
    std::unordered_set<std::string> reported;
    forEachProto(cl->p, [&](const Proto* p) {
        for (int pc = 0; pc < p->sizecode; ++pc) {
            const Instruction i = p->code[pc];
            if (GET_OPCODE(i) != OP_GETTABUP || !isEnv(p, GETARG_B(i))) continue;
            const std::string global = kString(p, GETARG_C(i));
            if (global.empty()) continue;
            const int line = luaG_getfuncline(p, pc);
            if (!defined(global)) {
                if (reported.insert(global + "@" + std::to_string(line)).second)
                    out.push_back({line, "'" + global + "' n'est défini nulle part (faute de frappe ? variable locale oubliée ?)"});
                continue;
            }
            if (global != "agr") continue;
            // agr.a.b...: the GETFIELD chain right after, checked against the live tables.
            std::string path = "agr";
            lua_getglobal(L, "agr");
            int reg = GETARG_A(i);
            for (int q = pc + 1; q < p->sizecode; ++q) {
                const Instruction n = p->code[q];
                if ((GET_OPCODE(n) != OP_GETFIELD && GET_OPCODE(n) != OP_SELF) || GETARG_B(n) != reg) break;
                const std::string key = GET_OPCODE(n) == OP_SELF ? (TESTARG_k(n) ? kString(p, GETARG_C(n)) : std::string()) : kString(p, GETARG_C(n));
                if (key.empty() || !lua_istable(L, -1)) break;
                lua_getfield(L, -1, key.c_str());
                lua_remove(L, -2);
                path += "." + key;
                if (path == "agr.globals") break;  // the game's own data, not API
                if (lua_isnil(L, -1)) {
                    const int l = luaG_getfuncline(p, q);
                    if (reported.insert(path + "@" + std::to_string(l)).second) out.push_back({l, "'" + path + "' n'existe pas dans l'API"});
                    break;
                }
                reg = GETARG_A(n);
                if (GET_OPCODE(n) == OP_SELF) break;
            }
            lua_pop(L, 1);
        }
    });

    // Methods: `function M:f(...)` has `self` in register 0. Fields read from it must be written
    // somewhere in the script (any table: properties, the module, self.x = ...) or be built in;
    // entity methods (`self.entity:m(...)`) must exist. Skipped when fields may come from
    // elsewhere (setmetatable / require / self[k] = v).
    auto isMethod = [](const Proto* p) {
        return p->numparams >= 1 && p->sizelocvars >= 1 && p->locvars[0].varname && std::strcmp(getstr(p->locvars[0].varname), "self") == 0;
    };
    std::unordered_set<std::string> known{"entity"};
    bool dynamicFields = false;
    forEachProto(cl->p, [&](const Proto* p) {
        for (int pc = 0; pc < p->sizecode; ++pc) {
            const Instruction i = p->code[pc];
            const OpCode op = GET_OPCODE(i);
            if (op == OP_SETFIELD) known.insert(kString(p, GETARG_B(i)));
            else if (op == OP_SETTABLE && isMethod(p) && GETARG_A(i) == 0) dynamicFields = true;
            else if (op == OP_GETTABUP && isEnv(p, GETARG_B(i))) {
                const std::string g = kString(p, GETARG_C(i));
                if (g == "setmetatable" || g == "require" || g == "rawset") dynamicFields = true;
            }
        }
    });
    luaL_getmetatable(L, "agr.Entity");
    lua_getfield(L, -1, "__index");
    lua_remove(L, -2);
    auto entityMethod = [&](const std::string& m) {
        if (!lua_istable(L, -1)) return true;
        lua_getfield(L, -1, m.c_str());
        const bool ok = !lua_isnil(L, -1);
        lua_pop(L, 1);
        return ok;
    };
    forEachProto(cl->p, [&](const Proto* p) {
        if (!isMethod(p)) return;
        // Registers holding self.entity (until overwritten).
        std::unordered_set<int> entityRegs;
        for (int pc = 0; pc < p->sizecode; ++pc) {
            const Instruction i = p->code[pc];
            const OpCode op = GET_OPCODE(i);
            const int line = luaG_getfuncline(p, pc);
            if ((op == OP_GETFIELD || op == OP_SELF) && GETARG_B(i) == 0) {
                const std::string key = op == OP_SELF ? (TESTARG_k(i) ? kString(p, GETARG_C(i)) : std::string()) : kString(p, GETARG_C(i));
                if (!key.empty() && !dynamicFields && !known.count(key) && reported.insert("self." + key + "@" + std::to_string(line)).second)
                    out.push_back({line, op == OP_SELF ? "'self:" + key + "' : aucune fonction '" + key + "' dans ce script"
                                                       : "'self." + key + "' n'est jamais défini dans ce script (faute de frappe ? propriété manquante ?)"});
                if (op == OP_GETFIELD && key == "entity") {
                    entityRegs.insert(GETARG_A(i));
                    continue;
                }
            } else if (op == OP_SELF && entityRegs.count(GETARG_B(i)) && TESTARG_k(i)) {
                const std::string m = kString(p, GETARG_C(i));
                if (!m.empty() && !entityMethod(m) && reported.insert("entity:" + m + "@" + std::to_string(line)).second)
                    out.push_back({line, "'" + m + "' n'est pas une méthode d'entité"});
            }
            // Any other write to a register forgets what it held (good enough for straight code).
            if (op != OP_SETFIELD && op != OP_SETTABLE && op != OP_SETI && op != OP_SETTABUP && op != OP_SETUPVAL && op != OP_SETLIST &&
                op != OP_JMP && op != OP_RETURN && op != OP_RETURN0 && op != OP_RETURN1 && op != OP_TEST && op != OP_EQ && op != OP_LT && op != OP_LE &&
                op != OP_EQK && op != OP_EQI && op != OP_LTI && op != OP_LEI && op != OP_GTI && op != OP_GEI)
                entityRegs.erase(GETARG_A(i));
        }
    });
    lua_pop(L, 1);  // entity methods
    lua_pop(L, 1);  // the chunk
    // Signals declared by the script (`X.signals = { a = {...}, b = {} }` or `signals = {...}` in its
    // table): the entity's own emit / connect / waitSignal must use one of them.
    {
        const std::string text(code);
        static const std::regex decl(R"((?:\.|\b)signals\s*=\s*\{)");
        std::smatch m;
        if (std::regex_search(text, m, decl)) {
            std::set<std::string> declared;
            usize i = static_cast<usize>(m.position(0) + m.length(0));
            int depth = 1;
            std::string word;
            for (; i < text.size() && depth > 0; ++i) {
                const char c = text[i];
                if (c == '{') ++depth;
                else if (c == '}') --depth;
                if (depth == 1 && (std::isalnum(static_cast<unsigned char>(c)) || c == '_')) word += c;
                else {
                    if (depth == 1 && c == '=' && !word.empty()) declared.insert(word);
                    if (!std::isspace(static_cast<unsigned char>(c))) word.clear();
                }
            }
            static const std::regex use(R"(self\.entity\s*:\s*(emit|connect)\s*\(\s*["']([A-Za-z_][A-Za-z0-9_]*)["']|agr\.waitSignal\s*\(\s*self\.entity\s*,\s*["']([A-Za-z_][A-Za-z0-9_]*)["'])");
            std::string list;
            for (const std::string& d : declared) list += (list.empty() ? "" : ", ") + d;
            for (auto it = std::sregex_iterator(text.begin(), text.end(), use); it != std::sregex_iterator(); ++it) {
                const std::string sig = (*it)[2].matched ? (*it)[2].str() : (*it)[3].str();
                if (declared.count(sig)) continue;
                const int line = 1 + static_cast<int>(std::count(text.begin(), text.begin() + it->position(0), '\n'));
                out.push_back({line, "signal '" + sig + "' non déclaré dans signals (" + (list.empty() ? "aucun" : list) + ")"});
            }
        }
    }
    std::sort(out.begin(), out.end(), [](const SyntaxError& a, const SyntaxError& b) { return a.line < b.line; });
    return out;
}

} // namespace agr
