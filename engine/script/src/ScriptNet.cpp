// agr.net: multiplayer sessions (NetPlugin). Messages are Lua tables sent as JSON.
#include "ScriptInternal.hpp"
#include "agr/net/Http.hpp"
#include "agr/net/NetSession.hpp"
#include "agr/net/Replication.hpp"

namespace agr {

namespace {

net::NetSession& netOf(lua_State* L) {
    net::NetSession* n = implOf(L).net;
    if (!n) luaL_error(L, "agr.net: networking is not available (NetPlugin not loaded)");
    return *n;
}

int pushStatus(lua_State* L, const Status& s) {
    if (s) {
        lua_pushboolean(L, 1);
        return 1;
    }
    lua_pushnil(L);
    lua_pushstring(L, s.error().message.c_str());
    return 2;
}

u16 checkPort(lua_State* L, int idx) {
    const lua_Integer port = luaL_checkinteger(L, idx);
    luaL_argcheck(L, port >= 0 && port <= 65535, idx, "port out of range (0..65535)");
    return static_cast<u16>(port);
}

/// agr.net.host(port [, maxPlayers]) -> true or nil, message
int n_host(lua_State* L) {
    const lua_Integer maxPlayers = luaL_optinteger(L, 2, 8);
    return pushStatus(L, netOf(L).host(checkPort(L, 1), static_cast<u32>(std::max<lua_Integer>(1, maxPlayers - 1))));
}
/// agr.net.join(address, port) -> true or nil, message (then a "connected" event)
int n_join(lua_State* L) { return pushStatus(L, netOf(L).join(luaL_checkstring(L, 1), checkPort(L, 2))); }
/// agr.net.setEncryption(on): host encrypts the session (needs a password); agr.net.encrypted([peer]).
int n_setEncryption(lua_State* L) {
    netOf(L).setEncryption(lua_toboolean(L, 1) != 0);
    return 0;
}
int n_encrypted(lua_State* L) {
    lua_pushboolean(L, netOf(L).encrypted(static_cast<u32>(luaL_optinteger(L, 1, net::NetSession::kHostId))));
    return 1;
}

/// agr.net.setPassword(text): the game password (host: required from players; client: sent).
int n_setPassword(lua_State* L) {
    netOf(L).setPassword(luaL_optstring(L, 1, ""));
    return 0;
}
/// agr.net.joinError() -> "full", "wrong password" or nil
int n_joinError(lua_State* L) {
    const std::string& e = netOf(L).joinError();
    if (e.empty()) lua_pushnil(L);
    else lua_pushstring(L, e.c_str());
    return 1;
}
/// agr.net.ping(peer) -> round trip in milliseconds (0 until measured)
int n_ping(lua_State* L) {
    lua_pushnumber(L, netOf(L).rtt(static_cast<u32>(luaL_checkinteger(L, 1))) * 1000.0);
    return 1;
}
/// agr.net.time() -> the host's clock in seconds (shared by every player), synced
int n_time(lua_State* L) {
    lua_pushnumber(L, netOf(L).networkTime());
    lua_pushboolean(L, netOf(L).clockSynced());
    return 2;
}
/// agr.net.stats() -> { bytesSent, bytesReceived, packetsSent, packetsReceived, resent }
int n_stats(lua_State* L) {
    const auto& st = netOf(L).stats();
    lua_createtable(L, 0, 5);
    for (auto [k, v] : {std::pair<const char*, u64>{"bytesSent", st.bytesSent}, {"bytesReceived", st.bytesReceived}, {"packetsSent", st.packetsSent},
                        {"packetsReceived", st.packetsReceived}, {"resent", st.resent}}) {
        lua_pushinteger(L, static_cast<lua_Integer>(v));
        lua_setfield(L, -2, k);
    }
    return 1;
}
int n_leave(lua_State* L) {
    netOf(L).close();
    return 0;
}
int n_role(lua_State* L) {
    const net::NetRole r = netOf(L).role();
    lua_pushstring(L, r == net::NetRole::Host ? "host" : r == net::NetRole::Client ? "client" : "none");
    return 1;
}
int n_id(lua_State* L) {
    lua_pushinteger(L, netOf(L).localId());
    return 1;
}
int n_port(lua_State* L) {
    lua_pushinteger(L, netOf(L).port());
    return 1;
}
int n_peers(lua_State* L) {
    const auto peers = netOf(L).peers();
    lua_createtable(L, static_cast<int>(peers.size()), 0);
    for (usize i = 0; i < peers.size(); ++i) {
        lua_pushinteger(L, peers[i]);
        lua_rawseti(L, -2, static_cast<lua_Integer>(i + 1));
    }
    return 1;
}
int sendTo(lua_State* L, u32 peer, int valueIdx, int reliableIdx) {
    const std::string data = toJson(L, valueIdx).dump();
    if (data.size() > net::NetSession::kMaxPayload) return luaL_error(L, "agr.net: message too large (%d bytes, at most %d)", int(data.size()), int(net::NetSession::kMaxPayload));
    lua_pushboolean(L, netOf(L).send(peer, data, lua_toboolean(L, reliableIdx) != 0));
    return 1;
}
/// agr.net.send(peer, value [, reliable]) -> sent
int n_send(lua_State* L) { return sendTo(L, static_cast<u32>(luaL_checkinteger(L, 1)), 2, 3); }
/// agr.net.broadcast(value [, reliable]): host -> every player, client -> host
int n_broadcast(lua_State* L) { return sendTo(L, net::NetSession::kBroadcast, 1, 2); }
/// agr.net.poll() -> { {type = "connected" | "disconnected" | "message", peer = id, data = value}, ... }
int n_poll(lua_State* L) {
    auto events = netOf(L).takeEvents();
    lua_createtable(L, static_cast<int>(events.size()), 0);
    lua_Integer i = 1;
    for (const net::NetEvent& e : events) {
        lua_createtable(L, 0, 3);
        lua_pushstring(L, e.type == net::NetEvent::Type::Connected ? "connected" : e.type == net::NetEvent::Type::Disconnected ? "disconnected" : "message");
        lua_setfield(L, -2, "type");
        lua_pushinteger(L, e.peer);
        lua_setfield(L, -2, "peer");
        if (e.type == net::NetEvent::Type::Message) {
            const nlohmann::json j = nlohmann::json::parse(e.data, nullptr, false);
            if (j.is_discarded()) {
                lua_pop(L, 1);  // not a message of agr.net: ignored
                continue;
            }
            pushJson(L, j);
            lua_setfield(L, -2, "data");
        }
        lua_rawseti(L, -2, i++);
    }
    return 1;
}

net::Replicator* replicatorOf(lua_State* L) { return implOf(L).replicator; }
/// agr.net.setVisible(entity, peer, visible) (host): shows / hides the entity's state to a player.
int n_setVisible(lua_State* L) {
    net::Replicator* r = replicatorOf(L);
    const ecs::Entity e = checkEntity(L, 1);
    if (r) r->setVisibleTo(sceneOf(L), e, static_cast<u32>(luaL_checkinteger(L, 2)), lua_toboolean(L, 3) != 0);
    return 0;
}
net::Replicator& needReplicator(lua_State* L) {
    net::Replicator* r = replicatorOf(L);
    if (!r) luaL_error(L, "agr.net: networking is not available (NetPlugin not loaded)");
    return *r;
}
nlohmann::json argsFrom(lua_State* L, int first) {
    nlohmann::json a = nlohmann::json::array();
    for (int i = first; i <= lua_gettop(L); ++i) a.push_back(toJson(L, i));
    return a;
}
/// Shared by entity:rpc / rpcTo: sends, and runs here too when the declaration ends with ",local".
int doRpc(lua_State* L, ecs::Entity e, u32 target, const std::string& fn, int firstArg) {
    auto& m = implOf(L);
    const nlohmann::json args = argsFrom(L, firstArg);
    if (args.dump().size() > 60000) return luaL_error(L, "agr.net: RPC arguments too large");
    const std::string mode = m.rpcMode(e, fn);
    if (mode.empty()) return luaL_error(L, "rpc '%s': declare it in the script's rpc table (rpc = { %s = \"any\" })", fn.c_str(), fn.c_str());
    bool sent = false;
    if (m.net && m.replicator && m.net->connected()) sent = m.replicator->rpc(sceneOf(L), *m.net, e, fn, args, target);
    const bool local = mode.find("local") != std::string::npos;
    if (local && (target == 0 || (m.net && target == m.net->localId()))) {
        const u32 self = m.net && m.net->connected() ? m.net->localId() : net::NetSession::kHostId;
        m.invokeRpc(sceneOf(L), e, fn, args, self, false);
    }
    lua_pushboolean(L, sent);
    return 1;
}
/// entity:rpc(fn, ...) -> sent: calls fn(...) of this entity's script on every other machine.
int e_rpc(lua_State* L) { return doRpc(L, checkEntity(L, 1), 0, luaL_checkstring(L, 2), 3); }
/// entity:rpcTo(peer, fn, ...): on one machine (1 = host).
int e_rpcTo(lua_State* L) { return doRpc(L, checkEntity(L, 1), static_cast<u32>(luaL_checkinteger(L, 2)), luaL_checkstring(L, 3), 4); }
int e_owner(lua_State* L) {
    const ecs::Entity e = checkEntity(L, 1);
    net::Replicator* r = replicatorOf(L);
    lua_pushinteger(L, r ? r->ownerOf(sceneOf(L), e) : net::NetSession::kHostId);
    return 1;
}
int e_isOwner(lua_State* L) {
    const ecs::Entity e = checkEntity(L, 1);
    auto& m = implOf(L);
    lua_pushboolean(L, !m.replicator || !m.net || m.replicator->isOwner(sceneOf(L), *m.net, e));
    return 1;
}
/// agr.net.spawn(prefab, x, y, z [, owner]) -> entity (host; offline: a plain instance)
int n_spawn(lua_State* L) {
    const std::string prefab = luaL_checkstring(L, 1);
    const math::Vec3 p{static_cast<float>(luaL_optnumber(L, 2, 0)), static_cast<float>(luaL_optnumber(L, 3, 0)), static_cast<float>(luaL_optnumber(L, 4, 0))};
    const u32 owner = static_cast<u32>(luaL_optinteger(L, 5, net::NetSession::kHostId));
    auto e = needReplicator(L).spawn(sceneOf(L), netOf(L), prefab, p, owner);
    if (!e) return luaL_error(L, "agr.net.spawn: %s", e.error().message.c_str());
    pushEntity(L, e.value());
    return 1;
}
int n_despawn(lua_State* L) {
    lua_pushboolean(L, needReplicator(L).despawn(sceneOf(L), netOf(L), checkEntity(L, 1)));
    return 1;
}
int n_setOwner(lua_State* L) {
    lua_pushboolean(L, needReplicator(L).setOwner(sceneOf(L), netOf(L), checkEntity(L, 1), static_cast<u32>(luaL_checkinteger(L, 2))));
    return 1;
}
/// agr.net.sender(): the player whose RPC is running (0 outside an RPC).
int n_sender(lua_State* L) {
    net::Replicator* r = replicatorOf(L);
    lua_pushinteger(L, r ? r->rpcSender() : 0);
    return 1;
}
int n_advertise(lua_State* L) {
    netOf(L).advertise(luaL_checkstring(L, 1));
    return 0;
}
int n_discover(lua_State* L) { return pushStatus(L, netOf(L).discover(checkPort(L, 1))); }
int n_stopDiscovery(lua_State* L) {
    netOf(L).stopDiscovery();
    return 0;
}
/// agr.net.games() -> { {address = "192.168.1.20", port = 7777, name = "...", players = 2, max = 4}, ... }
int n_games(lua_State* L) {
    const auto games = netOf(L).lanGames();
    lua_createtable(L, static_cast<int>(games.size()), 0);
    for (usize i = 0; i < games.size(); ++i) {
        const auto& g = games[i];
        lua_createtable(L, 0, 5);
        const std::string addr = g.address.toString();
        lua_pushstring(L, addr.substr(0, addr.rfind(':')).c_str());
        lua_setfield(L, -2, "address");
        lua_pushinteger(L, g.address.port);
        lua_setfield(L, -2, "port");
        lua_pushstring(L, g.name.c_str());
        lua_setfield(L, -2, "name");
        lua_pushinteger(L, g.players);
        lua_setfield(L, -2, "players");
        lua_pushinteger(L, g.maxPlayers);
        lua_setfield(L, -2, "max");
        lua_rawseti(L, -2, static_cast<lua_Integer>(i + 1));
    }
    return 1;
}

} // namespace

// ---------------------------------------------------------------- agr.http
namespace {

net::HttpClient& httpOf(lua_State* L) {
    auto& m = implOf(L);
    if (!m.http) m.http = std::make_shared<net::HttpClient>();
    return *m.http;
}

/// Starts a request; the callback (function at `cb`) is called with the response table.
int startHttp(lua_State* L, net::HttpRequest req, int cb) {
    auto& m = implOf(L);
    const u64 id = httpOf(L).send(std::move(req));
    if (cb && lua_isfunction(L, cb)) {
        lua_pushvalue(L, cb);
        m.httpCallbacks[id] = luaL_ref(L, LUA_REGISTRYINDEX);
    }
    lua_pushinteger(L, static_cast<lua_Integer>(id));
    return 1;
}

void readHeaders(lua_State* L, int t, net::HttpHeaders& out) {
    if (!lua_istable(L, t)) return;
    lua_pushnil(L);
    while (lua_next(L, t)) {
        if (lua_type(L, -2) == LUA_TSTRING) out.emplace_back(lua_tostring(L, -2), luaL_tolstring(L, -1, nullptr)), lua_pop(L, 1);
        lua_pop(L, 1);
    }
}

/// user://path -> a file of the user folder (downloads go only there).
std::string downloadTarget(lua_State* L, const char* uri) {
    auto parsed = VirtualFileSystem::parse(uri);
    if (!parsed || parsed.value().scheme != "user") luaL_error(L, "agr.http: downloads go to user:// (got '%s')", uri);
    auto* native = dynamic_cast<NativeFileProvider*>(implOf(L).vfs.provider("user"));
    if (!native) luaL_error(L, "agr.http: user:// is not a folder here");
    return native->root() + "/" + parsed.value().path;
}

/// agr.http.request{url=, method="GET", headers={...}, body="", timeout=30, download="user://f"} [, callback] -> id
int h_request(lua_State* L) {
    luaL_checktype(L, 1, LUA_TTABLE);
    net::HttpRequest req;
    lua_getfield(L, 1, "url");
    req.url = luaL_checkstring(L, -1);
    lua_pop(L, 1);
    if (lua_getfield(L, 1, "method") == LUA_TSTRING) req.method = lua_tostring(L, -1);
    lua_pop(L, 1);
    for (char& c : req.method) c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
    if (lua_getfield(L, 1, "body") == LUA_TSTRING) {
        size_t n = 0;
        const char* b = lua_tolstring(L, -1, &n);
        req.body.assign(b, n);
    }
    lua_pop(L, 1);
    if (lua_getfield(L, 1, "timeout") == LUA_TNUMBER) req.timeoutSeconds = lua_tonumber(L, -1);
    lua_pop(L, 1);
    lua_getfield(L, 1, "headers");
    readHeaders(L, lua_gettop(L), req.headers);
    lua_pop(L, 1);
    if (lua_getfield(L, 1, "download") == LUA_TSTRING) req.downloadPath = downloadTarget(L, lua_tostring(L, -1));
    lua_pop(L, 1);
    return startHttp(L, std::move(req), lua_isfunction(L, 2) ? 2 : 0);
}
/// agr.http.get(url, callback [, headers]) -> id
int h_get(lua_State* L) {
    net::HttpRequest req;
    req.url = luaL_checkstring(L, 1);
    readHeaders(L, 3, req.headers);
    return startHttp(L, std::move(req), lua_isfunction(L, 2) ? 2 : 0);
}
/// agr.http.post(url, body, callback [, headers]) -> id ; a table body is sent as JSON.
int h_post(lua_State* L) {
    net::HttpRequest req;
    req.method = "POST";
    req.url = luaL_checkstring(L, 1);
    if (lua_istable(L, 2)) {
        req.body = toJson(L, 2).dump();
        req.headers.emplace_back("Content-Type", "application/json");
    } else {
        size_t n = 0;
        const char* b = luaL_optlstring(L, 2, "", &n);
        req.body.assign(b, n);
    }
    readHeaders(L, 4, req.headers);
    return startHttp(L, std::move(req), lua_isfunction(L, 3) ? 3 : 0);
}
int h_cancel(lua_State* L) {
    httpOf(L).cancel(static_cast<u64>(luaL_checkinteger(L, 1)));
    return 0;
}
/// agr.http.progress(id) -> received, total (0 = unknown)
int h_progress(lua_State* L) {
    const auto [got, total] = httpOf(L).progress(static_cast<u64>(luaL_checkinteger(L, 1)));
    lua_pushinteger(L, static_cast<lua_Integer>(got));
    lua_pushinteger(L, static_cast<lua_Integer>(total));
    return 2;
}
int h_pending(lua_State* L) {
    auto& m = implOf(L);
    lua_pushinteger(L, m.http ? static_cast<lua_Integer>(m.http->pending()) : 0);
    return 1;
}

// ---------------------------------------------------------------- agr.ws
/// agr.ws.connect(url, {onOpen = f(), onMessage = f(data, binary), onClose = f(code, error)} [, headers]) -> id
int w_connect(lua_State* L) {
    auto& m = implOf(L);
    const char* url = luaL_checkstring(L, 1);
    ScriptEngine::Impl::WebSocketHandle h;
    h.ws = std::make_shared<net::WebSocket>();
    if (lua_istable(L, 2)) {
        lua_pushvalue(L, 2);
        h.handlers = luaL_ref(L, LUA_REGISTRYINDEX);
    }
    net::WebSocket::Options o;
    readHeaders(L, 3, o.headers);
    h.ws->connect(url, std::move(o));
    const u64 id = m.nextWebSocket++;
    m.webSockets[id] = std::move(h);
    lua_pushinteger(L, static_cast<lua_Integer>(id));
    return 1;
}
net::WebSocket* wsOf(lua_State* L) {
    auto& m = implOf(L);
    auto it = m.webSockets.find(static_cast<u64>(luaL_checkinteger(L, 1)));
    return it == m.webSockets.end() ? nullptr : it->second.ws.get();
}
/// agr.ws.send(id, text | table) -> true when queued (a table is sent as JSON text)
int w_send(lua_State* L) {
    net::WebSocket* ws = wsOf(L);
    bool ok = false;
    if (ws) {
        if (lua_istable(L, 2)) ok = ws->send(toJson(L, 2).dump());
        else {
            size_t n = 0;
            const char* t = luaL_checklstring(L, 2, &n);
            ok = lua_toboolean(L, 3) ? ws->sendBinary(std::string_view(t, n)) : ws->send(std::string_view(t, n));
        }
    }
    lua_pushboolean(L, ok);
    return 1;
}
int w_close(lua_State* L) {
    if (net::WebSocket* ws = wsOf(L)) ws->close(static_cast<u16>(luaL_optinteger(L, 2, 1000)), luaL_optstring(L, 3, ""));
    return 0;
}
/// agr.ws.state(id) -> "connecting" | "open" | "closing" | "closed"
int w_state(lua_State* L) {
    net::WebSocket* ws = wsOf(L);
    static const char* names[] = {"connecting", "open", "closing", "closed"};
    lua_pushstring(L, ws ? names[static_cast<int>(ws->state())] : "closed");
    return 1;
}

} // namespace

void dispatchHttp(ScriptEngine::Impl& m) {
    lua_State* L = m.L;
    // WebSockets: open, messages, close, in that order.
    for (auto it = m.webSockets.begin(); it != m.webSockets.end();) {
        auto& h = it->second;
        auto call = [&](const char* name, auto pushArgs, int nargs) {
            if (h.handlers == LUA_NOREF || h.handlers == -2) return;
            lua_rawgeti(L, LUA_REGISTRYINDEX, h.handlers);
            if (lua_getfield(L, -1, name) == LUA_TFUNCTION) {
                pushArgs();
                if (lua_pcall(L, nargs, 0, 0) != LUA_OK) {
                    m.reportError(std::string("agr.ws ") + name, lua_tostring(L, -1));
                    lua_pop(L, 1);
                }
            } else {
                lua_pop(L, 1);
            }
            lua_pop(L, 1);  // handlers table
        };
        const auto state = h.ws->state();
        if (!h.opened && (state == net::WebSocket::State::Open || state == net::WebSocket::State::Closing ||
                          (state == net::WebSocket::State::Closed && h.ws->error().empty()))) {
            h.opened = true;
            call("onOpen", [] {}, 0);
        }
        for (auto& msg : h.ws->receive())
            call("onMessage", [&] {
                lua_pushlstring(L, msg.data.data(), msg.data.size());
                lua_pushboolean(L, msg.binary);
            }, 2);
        if (state == net::WebSocket::State::Closed) {
            const std::string err = h.ws->error();
            const int code = h.ws->closeCode();
            call("onClose", [&] {
                lua_pushinteger(L, code);
                lua_pushstring(L, err.c_str());
            }, 2);
            if (h.handlers != LUA_NOREF && h.handlers != -2) luaL_unref(L, LUA_REGISTRYINDEX, h.handlers);
            it = m.webSockets.erase(it);
        } else {
            ++it;
        }
    }
    if (!m.http) return;
    for (auto& [id, r] : m.http->takeFinished()) {
        auto it = m.httpCallbacks.find(id);
        if (it == m.httpCallbacks.end()) continue;
        const int ref = it->second;
        m.httpCallbacks.erase(it);
        lua_rawgeti(L, LUA_REGISTRYINDEX, ref);
        luaL_unref(L, LUA_REGISTRYINDEX, ref);
        lua_createtable(L, 0, 7);
        lua_pushinteger(L, r.status);
        lua_setfield(L, -2, "status");
        lua_pushboolean(L, r.ok());
        lua_setfield(L, -2, "ok");
        lua_pushlstring(L, r.body.data(), r.body.size());
        lua_setfield(L, -2, "body");
        lua_pushstring(L, r.error.c_str());
        lua_setfield(L, -2, "error");
        lua_pushstring(L, r.finalUrl.c_str());
        lua_setfield(L, -2, "url");
        lua_pushinteger(L, static_cast<lua_Integer>(r.bytesReceived));
        lua_setfield(L, -2, "bytes");
        lua_createtable(L, 0, static_cast<int>(r.headers.size()));
        for (const auto& [k, v] : r.headers) {
            std::string key = k;
            for (char& c : key) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
            lua_pushstring(L, v.c_str());
            lua_setfield(L, -2, key.c_str());
        }
        lua_setfield(L, -2, "headers");
        if (lua_pcall(L, 1, 0, 0) != LUA_OK) {
            m.reportError("agr.http callback", lua_tostring(L, -1));
            lua_pop(L, 1);
        }
    }
}

void openNetBindings(lua_State* L) {
    static const luaL_Reg http[] = {{"request", h_request}, {"get", h_get},         {"post", h_post},     {"cancel", h_cancel},
                                    {"progress", h_progress}, {"pending", h_pending}, {nullptr, nullptr}};
    addAgrLibrary(L, "http", http);
    static const luaL_Reg ws[] = {{"connect", w_connect}, {"send", w_send}, {"close", w_close}, {"state", w_state}, {nullptr, nullptr}};
    addAgrLibrary(L, "ws", ws);
    static const luaL_Reg funcs[] = {{"host", n_host}, {"join", n_join}, {"leave", n_leave}, {"role", n_role}, {"id", n_id},
                                     {"port", n_port}, {"peers", n_peers}, {"send", n_send}, {"broadcast", n_broadcast},
                                     {"poll", n_poll}, {"spawn", n_spawn}, {"despawn", n_despawn}, {"setOwner", n_setOwner},
                                     {"sender", n_sender}, {"advertise", n_advertise}, {"discover", n_discover},
                                     {"stopDiscovery", n_stopDiscovery}, {"games", n_games}, {"setPassword", n_setPassword}, {"setEncryption", n_setEncryption}, {"encrypted", n_encrypted},
                                     {"joinError", n_joinError}, {"ping", n_ping}, {"time", n_time}, {"stats", n_stats}, {"setVisible", n_setVisible},
                                     {nullptr, nullptr}};
    addAgrLibrary(L, "net", funcs);
    static const luaL_Reg methods[] = {{"rpc", e_rpc}, {"rpcTo", e_rpcTo}, {"owner", e_owner}, {"isOwner", e_isOwner}, {nullptr, nullptr}};
    addEntityMethods(L, methods);
}

} // namespace agr
