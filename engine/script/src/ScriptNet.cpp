// agr.net: multiplayer sessions (NetPlugin). Messages are Lua tables sent as JSON.
#include "ScriptInternal.hpp"
#include "agr/net/NetSession.hpp"

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

} // namespace

void openNetBindings(lua_State* L) {
    static const luaL_Reg funcs[] = {{"host", n_host}, {"join", n_join}, {"leave", n_leave}, {"role", n_role}, {"id", n_id},
                                     {"port", n_port}, {"peers", n_peers}, {"send", n_send}, {"broadcast", n_broadcast},
                                     {"poll", n_poll}, {nullptr, nullptr}};
    addAgrLibrary(L, "net", funcs);
}

} // namespace agr
