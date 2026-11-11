#include <algorithm>
#include <cstdio>

#include <nlohmann/json.hpp>

#include "ScriptInternal.hpp"
#include "agr/core/Log.hpp"
#include "agr/script/ScriptComponent.hpp"

namespace agr {

namespace {

int traceback(lua_State* L) {
    const char* msg = lua_tostring(L, 1);
    luaL_traceback(L, L, msg ? msg : "(error object is not a string)", 1);
    return 1;
}

/// Stops callbacks that run longer than the budget (infinite loops would freeze the game / editor).
void watchdog(lua_State* L, lua_Debug*) {
    auto& m = implOf(L);
    if (m.inCall && m.callClock.elapsedSeconds() > m.budgetSeconds) {
        char msg[128];
        std::snprintf(msg, sizeof msg, "script stopped: a callback ran for more than %.2f s (infinite loop?)", m.budgetSeconds);
        luaL_error(L, "%s", msg);
    }
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
    lua_sethook(L, watchdog, LUA_MASKCOUNT, 20000);
}

ScriptEngine::Impl::~Impl() {
    scene = nullptr;  // may already be destroyed: no onDestroy callbacks
    resetInstances();
    lua_close(L);
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
void ScriptEngine::setActions(const InputActions* actions) { m->actions = actions; }
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
void ScriptEngine::setSceneLoader(std::function<void(std::string)> loader) { m->sceneLoader = std::move(loader); }
void ScriptEngine::setAudio(AudioSystem* audio) { m->audio = audio; }
void ScriptEngine::setUI(UISystem* ui) { m->ui = ui; }
void ScriptEngine::setAnimation(AnimationSystem* animation) { m->animation = animation; }
void ScriptEngine::setPlatform(PlatformLayer* platform) { m->platform = platform; }
void ScriptEngine::setSaveStore(SaveStore* save) { m->save = save; }
void ScriptEngine::setCallbackBudget(f64 seconds) { m->budgetSeconds = std::max(seconds, 0.001); }

void ScriptEngine::reset() {
    m->resetInstances();
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
    }
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
        m->call(it->second, "onUpdate", [&] {
            lua_pushnumber(L, dt);
            return 1;
        });
    }
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
}

void ScriptEngine::dispatchAnimation(Scene& scene, const AnimationEvent& e) {
    if (scene.generation() != m->sceneGeneration || !scene.valid(e.entity)) return;
    m->scene = &scene;
    auto it = m->instances.find(e.entity.packed());
    if (it == m->instances.end() || !it->second.started) return;
    const char* fn = e.kind == AnimationEvent::Kind::StateEnter ? "onStateEnter" : "onAnimationEvent";
    m->call(it->second, fn, [&] {
        lua_pushstring(m->L, e.name.c_str());
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

} // namespace agr
