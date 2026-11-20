// Gameplay runtime for scripts (the GDScript conveniences): signals between entities and a
// global event bus, timers, coroutines that wait (seconds, frames, a condition, a signal),
// groups of entities (saved with the scene) and agr.globals, a table that
// lives as long as the game (kept across agr.loadScene, like an autoload).
// The scheduling part is Lua (the prelude below), ticked by ScriptEngine::update.
#include <algorithm>
#include <sstream>

#include "ScriptInternal.hpp"
#include "agr/core/Log.hpp"
#include "agr/script/ScriptComponent.hpp"

namespace agr {

namespace {

std::vector<std::string> splitGroups(const std::string& s) {
    std::vector<std::string> out;
    std::string cur;
    std::stringstream ss(s);
    while (std::getline(ss, cur, ',')) {
        const auto b = cur.find_first_not_of(" \t");
        const auto e = cur.find_last_not_of(" \t");
        if (b != std::string::npos) out.push_back(cur.substr(b, e - b + 1));
    }
    return out;
}

std::string joinGroups(const std::vector<std::string>& g) {
    std::string s;
    for (const auto& x : g) s += (s.empty() ? "" : ",") + x;
    return s;
}

int e_key(lua_State* L) {
    auto* ud = static_cast<LuaEntity*>(luaL_checkudata(L, 1, "agr.Entity"));
    lua_pushfstring(L, "%I:%I", static_cast<lua_Integer>(ud->packed), static_cast<lua_Integer>(ud->generation));
    return 1;
}

int e_addToGroup(lua_State* L) {
    const ecs::Entity e = checkEntity(L, 1);
    const std::string name = luaL_checkstring(L, 2);
    auto& reg = sceneOf(L).registry();
    auto* g = reg.tryGet<GroupsComponent>(e);
    if (!g) g = &reg.emplace<GroupsComponent>(e);
    auto list = splitGroups(g->groups);
    if (std::find(list.begin(), list.end(), name) == list.end()) list.push_back(name);
    g->groups = joinGroups(list);
    return 0;
}

int e_removeFromGroup(lua_State* L) {
    const ecs::Entity e = checkEntity(L, 1);
    const std::string name = luaL_checkstring(L, 2);
    auto* g = sceneOf(L).registry().tryGet<GroupsComponent>(e);
    if (!g) return 0;
    auto list = splitGroups(g->groups);
    std::erase(list, name);
    g->groups = joinGroups(list);
    return 0;
}

int e_inGroup(lua_State* L) {
    const ecs::Entity e = checkEntity(L, 1);
    const std::string name = luaL_checkstring(L, 2);
    const auto* g = sceneOf(L).registry().tryGet<GroupsComponent>(e);
    bool in = false;
    if (g)
        for (const auto& x : splitGroups(g->groups)) in |= x == name;
    lua_pushboolean(L, in);
    return 1;
}

int e_groups(lua_State* L) {
    const ecs::Entity e = checkEntity(L, 1);
    const auto* g = sceneOf(L).registry().tryGet<GroupsComponent>(e);
    const auto list = g ? splitGroups(g->groups) : std::vector<std::string>{};
    lua_createtable(L, static_cast<int>(list.size()), 0);
    for (usize i = 0; i < list.size(); ++i) {
        lua_pushstring(L, list[i].c_str());
        lua_rawseti(L, -2, static_cast<lua_Integer>(i + 1));
    }
    return 1;
}

// agr.group(name) -> active entities of the group, in creation order.
int a_group(lua_State* L) {
    const std::string name = luaL_checkstring(L, 1);
    Scene& s = sceneOf(L);
    std::vector<ecs::Entity> found;
    s.registry().view<GroupsComponent>().each([&](ecs::Entity e, GroupsComponent& g) {
        if (!s.isActiveInHierarchy(e)) return;
        for (const auto& x : splitGroups(g.groups))
            if (x == name) {
                found.push_back(e);
                break;
            }
    });
    std::sort(found.begin(), found.end(), [](ecs::Entity a, ecs::Entity b) { return a.index < b.index; });
    lua_createtable(L, static_cast<int>(found.size()), 0);
    for (usize i = 0; i < found.size(); ++i) {
        pushEntity(L, found[i]);
        lua_rawseti(L, -2, static_cast<lua_Integer>(i + 1));
    }
    return 1;
}

constexpr const char* kPrelude = R"lua(
local agr = agr
local E = ...  -- entity methods table
local rt = { timers = {}, tasks = {}, signals = {}, events = {} }
agr.globals = agr.globals or {}

local function alive(owner) return owner == nil or owner:isValid() end
local function report(where, err) agr.error(where .. ": " .. tostring(err)) end

local Handle = {}
Handle.__index = Handle
function Handle:cancel() self.cancelled = true end
function Handle:isDone() return self.done == true or self.cancelled == true end
function Handle:onDone(fn) self.doneFn = fn return self end
local function finish(h)
  h.done = true
  if h.doneFn then
    local ok, err = pcall(h.doneFn, h)
    if not ok then report("onDone", err) end
  end
end

-- Global event bus -------------------------------------------------------------------------
agr.events = {}
function agr.events.on(name, fn, owner)
  local list = rt.events[name] or {}
  rt.events[name] = list
  local h = setmetatable({ fn = fn, owner = owner, name = name }, Handle)
  list[#list + 1] = h
  return h
end
function agr.events.off(h) if h then h.cancelled = true end end
function agr.events.emit(name, ...)
  local list = rt.events[name]
  if not list then return 0 end
  local n, keep = 0, {}
  for _, h in ipairs(list) do
    if not h.cancelled and alive(h.owner) then
      keep[#keep + 1] = h
      n = n + 1
      local ok, err = pcall(h.fn, ...)
      if not ok then report("event '" .. name .. "'", err) end
    end
  end
  rt.events[name] = keep
  return n
end

-- Entity signals ---------------------------------------------------------------------------
function E:connect(signal, fn, owner)
  local k = self:key()
  local s = rt.signals[k] or {}
  rt.signals[k] = s
  local list = s[signal] or {}
  s[signal] = list
  local h = setmetatable({ fn = fn, owner = owner }, Handle)
  list[#list + 1] = h
  return h
end
function E:disconnect(h) if h then h.cancelled = true end end
function E:emit(signal, ...)
  local s = rt.signals[self:key()]
  local list = s and s[signal]
  if not list then return 0 end
  local n, keep = 0, {}
  for _, h in ipairs(list) do
    if not h.cancelled and alive(h.owner) then
      keep[#keep + 1] = h
      n = n + 1
      local ok, err = pcall(h.fn, ...)
      if not ok then report("signal '" .. signal .. "'", err) end
    end
  end
  s[signal] = keep
  return n
end

-- Timers -----------------------------------------------------------------------------------
function agr.after(seconds, fn, owner)
  local h = setmetatable({ left = seconds or 0, fn = fn, owner = owner }, Handle)
  rt.timers[#rt.timers + 1] = h
  return h
end
function agr.every(seconds, fn, owner)
  local period = math.max(seconds or 0, 1e-4)
  local h = setmetatable({ left = period, period = period, fn = fn, owner = owner }, Handle)
  rt.timers[#rt.timers + 1] = h
  return h
end

-- Coroutines -------------------------------------------------------------------------------
local function resume(h, ...)
  local ok, req = coroutine.resume(h.co, ...)
  if not ok then
    report("agr.start", req)
    h.cancelled = true
    return
  end
  if coroutine.status(h.co) == "dead" then
    finish(h)
    return
  end
  h.waitTime, h.waitFrames, h.waitCond = nil, nil, nil
  if type(req) == "table" then
    h.waitTime, h.waitFrames, h.waitCond = req.time, req.frames, req.cond
  else
    h.waitFrames = 1
  end
end
function agr.start(fn, owner, ...)
  local h = setmetatable({ co = coroutine.create(fn), owner = owner }, Handle)
  rt.tasks[#rt.tasks + 1] = h
  resume(h, ...)
  return h
end
local function yieldFor(req, what)
  if not coroutine.isyieldable() then error(what .. " must be called inside agr.start(function() ... end)", 3) end
  return coroutine.yield(req)
end
function agr.wait(seconds) return yieldFor({ time = seconds or 0 }, "agr.wait") end
function agr.waitFrames(n) return yieldFor({ frames = n or 1 }, "agr.waitFrames") end
function agr.waitUntil(cond) return yieldFor({ cond = cond }, "agr.waitUntil") end
function agr.waitSignal(entity, signal)
  local got, args = false, nil
  local c = entity:connect(signal, function(...) got, args = true, table.pack(...) end)
  yieldFor({ cond = function() return got or not entity:isValid() end }, "agr.waitSignal")
  c:cancel()
  if args then return table.unpack(args, 1, args.n) end
end

-- Scheduler --------------------------------------------------------------------------------
local function sweep(list, step)
  local keep = {}
  for _, h in ipairs(list) do
    if not h.cancelled and not h.done then
      if not alive(h.owner) then
        h.cancelled = true
      else
        local ok, err = pcall(step, h)
        if not ok then report("agr runtime", err) h.cancelled = true end
        if not h.cancelled and not h.done then keep[#keep + 1] = h end
      end
    end
  end
  return keep
end

function agr._tick(dt)
  -- New entries added while ticking land in the fresh lists and run next frame.
  local timers, tasks = rt.timers, rt.tasks
  rt.timers, rt.tasks = {}, {}
  timers = sweep(timers, function(h)
    h.left = h.left - dt
    if h.left > 0 then return end
    local result = h.fn(h)
    if h.period and result ~= false and not h.cancelled then
      h.left = h.left + h.period
      if h.left <= 0 then h.left = h.period end
    else
      finish(h)
    end
  end)
  tasks = sweep(tasks, function(h)
    if h.waitCond then
      if h.waitCond() then resume(h) end
    elseif h.waitFrames then
      h.waitFrames = h.waitFrames - 1
      if h.waitFrames <= 0 then resume(h) end
    else
      h.waitTime = (h.waitTime or 0) - dt
      if h.waitTime <= 0 then resume(h) end
    end
  end)
  for _, h in ipairs(rt.timers) do timers[#timers + 1] = h end
  for _, h in ipairs(rt.tasks) do tasks[#tasks + 1] = h end
  rt.timers, rt.tasks = timers, tasks
end

-- Another scene: what belonged to its entities stops; global timers, events and agr.globals stay.
function agr._sceneChanged()
  local function global(list)
    local keep = {}
    for _, h in ipairs(list) do if h.owner == nil then keep[#keep + 1] = h end end
    return keep
  end
  rt.timers, rt.tasks = global(rt.timers), global(rt.tasks)
  rt.signals = {}
  for name, list in pairs(rt.events) do rt.events[name] = global(list) end
end

function agr._runtimeStats()
  local n = 0
  for _ in pairs(rt.signals) do n = n + 1 end
  return #rt.timers, #rt.tasks, n
end
)lua";

} // namespace

void openRuntimeBindings(lua_State* L) {
    static const luaL_Reg methods[] = {{"key", e_key},         {"addToGroup", e_addToGroup}, {"removeFromGroup", e_removeFromGroup},
                                       {"inGroup", e_inGroup}, {"groups", e_groups},         {nullptr, nullptr}};
    addEntityMethods(L, methods);
    static const luaL_Reg funcs[] = {{"group", a_group}, {nullptr, nullptr}};
    lua_getglobal(L, "agr");
    luaL_setfuncs(L, funcs, 0);
    lua_pop(L, 1);
    if (luaL_loadbufferx(L, kPrelude, std::char_traits<char>::length(kPrelude), "=agr.runtime", "t") != LUA_OK) {
        AGR_LOG_ERROR("Script", "runtime prelude: {}", lua_tostring(L, -1));
        lua_pop(L, 1);
        return;
    }
    luaL_getmetatable(L, "agr.Entity");
    lua_getfield(L, -1, "__index");
    lua_remove(L, -2);
    if (lua_pcall(L, 1, 0, 0) != LUA_OK) {
        AGR_LOG_ERROR("Script", "runtime prelude: {}", lua_tostring(L, -1));
        lua_pop(L, 1);
    }
}

} // namespace agr
