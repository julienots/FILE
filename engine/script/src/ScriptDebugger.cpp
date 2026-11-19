// Script debugger: breakpoints, stepping, call stack, variables and expression
// evaluation, on top of the Lua debug hooks (from C: the scripts' sandbox still has
// no `debug` library).
#include <algorithm>
#include <cstdio>

#include "ScriptInternal.hpp"

namespace agr {

namespace {

constexpr const char* kStopMessage = "stopped by the debugger";

int stackDepth(lua_State* L) {
    lua_Debug ar;
    int depth = 0;
    while (lua_getstack(L, depth, &ar)) ++depth;
    return depth;
}

std::string sourceUri(const char* source) {
    if (!source) return {};
    if (source[0] == '@' || source[0] == '=') return source + 1;
    return "(string)";
}

void appendValue(lua_State* L, int idx, std::string& out, int depth);

void appendKey(lua_State* L, int idx, std::string& out) {
    if (lua_type(L, idx) == LUA_TSTRING) {
        out += lua_tostring(L, idx);
    } else {
        out += '[';
        appendValue(L, idx, out, 1);
        out += ']';
    }
}

/// Readable text of a value without calling metamethods (no Lua code runs: safe in a hook).
void appendValue(lua_State* L, int idx, std::string& out, int depth) {
    idx = lua_absindex(L, idx);
    char buf[64];
    switch (lua_type(L, idx)) {
    case LUA_TNIL: out += "nil"; break;
    case LUA_TBOOLEAN: out += lua_toboolean(L, idx) ? "true" : "false"; break;
    case LUA_TNUMBER:
        if (lua_isinteger(L, idx)) {
            std::snprintf(buf, sizeof buf, "%lld", static_cast<long long>(lua_tointeger(L, idx)));
        } else {
            std::snprintf(buf, sizeof buf, "%.6g", lua_tonumber(L, idx));
        }
        out += buf;
        break;
    case LUA_TSTRING: {
        usize len = 0;
        const char* s = lua_tolstring(L, idx, &len);
        out += '"';
        out.append(s, std::min<usize>(len, 80));
        if (len > 80) out += "...";
        out += '"';
        break;
    }
    case LUA_TTABLE: {
        if (depth >= 2) {
            out += "{...}";
            break;
        }
        out += '{';
        int count = 0;
        lua_pushnil(L);
        while (lua_next(L, idx) != 0) {
            if (count == 8) {
                out += ", ...";
                lua_pop(L, 2);
                break;
            }
            if (count++ > 0) out += ", ";
            appendKey(L, -2, out);
            out += " = ";
            appendValue(L, -1, out, depth + 1);
            lua_pop(L, 1);
        }
        out += '}';
        break;
    }
    case LUA_TFUNCTION: out += lua_iscfunction(L, idx) ? "function (C)" : "function"; break;
    case LUA_TUSERDATA:
        if (const auto* e = static_cast<const LuaEntity*>(luaL_testudata(L, idx, "agr.Entity"))) {
            std::snprintf(buf, sizeof buf, "Entity #%u", static_cast<unsigned>(e->packed & 0xffffffffu));
            out += buf;
        } else {
            out += "userdata";
        }
        break;
    default: out += luaL_typename(L, idx); break;
    }
}

std::string valueText(lua_State* L, int idx) {
    std::string out;
    appendValue(L, idx, out, 0);
    return out;
}

} // namespace

void debugHook(lua_State* L, lua_Debug* ar) {
    auto& m = implOf(L);
    if (ar->event == LUA_HOOKCOUNT) {  // watchdog: stops callbacks that run longer than the budget
        if (m.inCall && !m.dbg.inBreak && m.callClock.elapsedSeconds() > m.budgetSeconds) {
            char msg[128];
            std::snprintf(msg, sizeof msg, "script stopped: a callback ran for more than %.2f s (infinite loop?)", m.budgetSeconds);
            luaL_error(L, "%s", msg);
        }
        return;
    }
    if (ar->event != LUA_HOOKLINE || m.dbg.inBreak || !m.dbg.handler) return;
    using Step = ScriptEngine::Impl::Debugger::Step;
    ScriptBreak::Reason reason = ScriptBreak::Reason::Breakpoint;
    bool stop = false;
    switch (m.dbg.step) {
    case Step::Into: stop = true; reason = ScriptBreak::Reason::Step; break;
    case Step::Pause: stop = true; reason = ScriptBreak::Reason::Pause; break;
    case Step::Over: stop = stackDepth(L) <= m.dbg.stepDepth; reason = ScriptBreak::Reason::Step; break;
    case Step::Out: stop = stackDepth(L) < m.dbg.stepDepth; reason = ScriptBreak::Reason::Step; break;
    case Step::None: break;
    }
    if (!stop) {
        if (!m.dbg.lineCount.contains(ar->currentline)) return;  // most lines: one hash lookup
        lua_getinfo(L, "S", ar);
        auto it = m.dbg.breakpoints.find(sourceUri(ar->source));
        if (it == m.dbg.breakpoints.end() || !it->second.contains(ar->currentline)) return;
        reason = ScriptBreak::Reason::Breakpoint;
    }
    if (m.debugBreak(reason, {}) == ScriptDebugAction::Stop) luaL_error(L, "%s", kStopMessage);
}

/// Error handler hook (called from the traceback message handler, the stack is still intact).
void debugError(lua_State* L, const char* message) {
    auto& m = implOf(L);
    if (!m.dbg.handler || !m.dbg.breakOnError || m.dbg.inBreak) return;
    if (message && std::string_view(message).find(kStopMessage) != std::string_view::npos) return;
    m.debugBreak(ScriptBreak::Reason::Error, message ? message : "(error object is not a string)");
}

void ScriptEngine::Impl::updateHook() {
    const bool lines = dbg.handler && (!dbg.breakpoints.empty() || dbg.step != Debugger::Step::None);
    lua_sethook(L, debugHook, LUA_MASKCOUNT | (lines ? LUA_MASKLINE : 0), 20000);
}

ScriptDebugAction ScriptEngine::Impl::debugBreak(ScriptBreak::Reason reason, const std::string& message) {
    ScriptBreak b;
    b.reason = reason;
    b.message = message;
    dbg.frameLevels.clear();
    lua_Debug ar;
    for (int level = 0; lua_getstack(L, level, &ar); ++level) {
        lua_getinfo(L, "Sln", &ar);
        if (ar.what && std::string_view(ar.what) == "C") continue;  // pcall, bindings
        lua_Debug caller;
        const bool outermost = !lua_getstack(L, level + 1, &caller) || (lua_getinfo(L, "S", &caller), std::string_view(caller.what) == "C");
        ScriptStackFrame f;
        if (ar.name) {
            f.function = ar.name;
        } else if (ar.what && std::string_view(ar.what) == "main") {
            f.function = "main chunk";
        } else if (outermost && inCall && callbackName) {
            f.function = callbackName;  // called by the engine (no name from Lua)
        } else {
            f.function = "function (line " + std::to_string(ar.linedefined) + ")";
        }
        f.source = sourceUri(ar.source);
        f.line = ar.currentline;
        for (int i = 1;; ++i) {
            const char* name = lua_getlocal(L, &ar, i);
            if (!name) break;
            if (name[0] != '(') f.variables.push_back({name, valueText(L, -1), false});
            lua_pop(L, 1);
        }
        lua_getinfo(L, "f", &ar);
        for (int i = 1;; ++i) {
            const char* name = lua_getupvalue(L, -1, i);
            if (!name) break;
            if (name[0] && std::string_view(name) != "_ENV") f.variables.push_back({name, valueText(L, -1), true});
            lua_pop(L, 1);
        }
        lua_pop(L, 1);  // function
        b.stack.push_back(std::move(f));
        dbg.frameLevels.push_back(level);
    }
    if (!b.stack.empty()) {
        b.source = b.stack.front().source;
        b.line = b.stack.front().line;
    }
    const int depth = stackDepth(L);
    dbg.inBreak = true;
    ScriptDebugAction action = ScriptDebugAction::Continue;
    try {
        action = dbg.handler(b);
    } catch (...) {
        dbg.inBreak = false;
        dbg.frameLevels.clear();
        throw;
    }
    dbg.inBreak = false;
    dbg.frameLevels.clear();
    callClock.restart();  // the time spent in the debugger does not count for the watchdog
    switch (action) {
    case ScriptDebugAction::StepInto: dbg.step = Debugger::Step::Into; break;
    case ScriptDebugAction::StepOver: dbg.step = Debugger::Step::Over; break;
    case ScriptDebugAction::StepOut: dbg.step = Debugger::Step::Out; break;
    default: dbg.step = Debugger::Step::None; break;
    }
    dbg.stepDepth = depth;
    if (reason == ScriptBreak::Reason::Error && action != ScriptDebugAction::Stop) dbg.step = Debugger::Step::None;  // the callback unwinds anyway
    updateHook();
    return action;
}

// ------------------------------------------------------------------ public API

void ScriptEngine::setBreakHandler(std::function<ScriptDebugAction(const ScriptBreak&)> handler) {
    m->dbg.handler = std::move(handler);
    if (!m->dbg.handler) m->dbg.step = Impl::Debugger::Step::None;
    m->updateHook();
}

void ScriptEngine::setBreakpoint(const std::string& uri, int line, bool enabled) {
    auto& lines = m->dbg.breakpoints[uri];
    if (enabled) {
        if (lines.insert(line).second) ++m->dbg.lineCount[line];
    } else if (lines.erase(line)) {
        if (--m->dbg.lineCount[line] <= 0) m->dbg.lineCount.erase(line);
    }
    if (lines.empty()) m->dbg.breakpoints.erase(uri);
    m->updateHook();
}

bool ScriptEngine::hasBreakpoint(const std::string& uri, int line) const {
    auto it = m->dbg.breakpoints.find(uri);
    return it != m->dbg.breakpoints.end() && it->second.contains(line);
}

void ScriptEngine::clearBreakpoints() {
    m->dbg.breakpoints.clear();
    m->dbg.lineCount.clear();
    m->updateHook();
}

std::vector<std::pair<std::string, int>> ScriptEngine::breakpoints() const {
    std::vector<std::pair<std::string, int>> out;
    for (const auto& [uri, lines] : m->dbg.breakpoints)
        for (int line : lines) out.emplace_back(uri, line);
    std::sort(out.begin(), out.end());
    return out;
}

void ScriptEngine::setBreakOnError(bool enabled) { m->dbg.breakOnError = enabled; }

void ScriptEngine::requestPause() {
    m->dbg.step = Impl::Debugger::Step::Pause;
    m->updateHook();
}

bool ScriptEngine::inBreak() const { return m->dbg.inBreak; }

Result<std::string> ScriptEngine::evaluate(std::string_view expression, usize frame) {
    if (!m->dbg.inBreak) return makeError(Error::Code::InvalidArgument, "the scripts are not stopped in the debugger");
    if (frame >= m->dbg.frameLevels.size()) return makeError(Error::Code::InvalidArgument, "no such stack frame");
    lua_State* L = m->L;
    const int top = lua_gettop(L);
    lua_Debug ar;
    if (!lua_getstack(L, m->dbg.frameLevels[frame], &ar)) return makeError(Error::Code::InvalidArgument, "no such stack frame");

    const std::string code = "return " + std::string(expression);
    if (luaL_loadbufferx(L, code.data(), code.size(), "=watch", "t") != LUA_OK) {
        std::string err = lua_tostring(L, -1);
        lua_settop(L, top);
        return makeError(Error::Code::ParseError, err);
    }
    // Environment: the frame's upvalues then locals (innermost wins), globals behind them.
    lua_newtable(L);
    const int env = lua_gettop(L);
    lua_getinfo(L, "f", &ar);
    for (int i = 1;; ++i) {
        const char* name = lua_getupvalue(L, -1, i);
        if (!name) break;
        if (name[0] && std::string_view(name) != "_ENV") {
            lua_setfield(L, env, name);
        } else {
            lua_pop(L, 1);
        }
    }
    lua_pop(L, 1);  // function
    for (int i = 1;; ++i) {
        const char* name = lua_getlocal(L, &ar, i);
        if (!name) break;
        if (name[0] != '(') {
            lua_setfield(L, env, name);
        } else {
            lua_pop(L, 1);
        }
    }
    lua_newtable(L);  // metatable: unknown names read the globals
    lua_rawgeti(L, LUA_REGISTRYINDEX, LUA_RIDX_GLOBALS);
    lua_setfield(L, -2, "__index");
    lua_setmetatable(L, env);
    if (!lua_setupvalue(L, env - 1, 1)) lua_pop(L, 1);  // _ENV of the chunk
    const int r = lua_pcall(L, 0, LUA_MULTRET, 0);
    if (r != LUA_OK) {
        std::string err = lua_tostring(L, -1) ? lua_tostring(L, -1) : "error";
        lua_settop(L, top);
        return makeError(Error::Code::InvalidArgument, err);
    }
    std::string out;
    for (int i = top + 1; i <= lua_gettop(L); ++i) {
        if (!out.empty()) out += ", ";
        out += valueText(L, i);
    }
    lua_settop(L, top);
    return out.empty() ? std::string("nil") : out;
}

} // namespace agr
