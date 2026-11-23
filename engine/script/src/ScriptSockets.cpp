// agr.tcp, agr.udp, agr.wsserver: low-level sockets for scripts (polled, non-blocking).
#include "ScriptInternal.hpp"

namespace agr {

namespace {

auto& socketsOf(lua_State* L) { return implOf(L).sockets; }

template <typename Map>
typename Map::mapped_type::element_type& checkHandle(lua_State* L, Map& map, const char* what) {
    auto it = map.find(static_cast<u64>(luaL_checkinteger(L, 1)));
    if (it == map.end()) luaL_error(L, "%s: unknown or closed handle", what);
    return *it->second;
}

int pushFailure(lua_State* L, const std::string& message) {
    lua_pushnil(L);
    lua_pushstring(L, message.c_str());
    return 2;
}

// ---- TCP
/// agr.tcp.listen(port [, localOnly]) -> server, port | nil, error
int t_listen(lua_State* L) {
    auto r = net::TcpServer::listen(static_cast<u16>(luaL_checkinteger(L, 1)), lua_toboolean(L, 2) != 0);
    if (!r) return pushFailure(L, r.error().message);
    auto& s = socketsOf(L);
    const u64 id = s.next++;
    const u16 port = r.value()->port();
    s.tcpServers[id] = std::move(r.value());
    lua_pushinteger(L, static_cast<lua_Integer>(id));
    lua_pushinteger(L, port);
    return 2;
}
/// agr.tcp.accept(server) -> connection | nil
int t_accept(lua_State* L) {
    auto& s = socketsOf(L);
    auto conn = checkHandle(L, s.tcpServers, "agr.tcp.accept").accept();
    if (!conn) return 0;
    const u64 id = s.next++;
    lua_pushinteger(L, static_cast<lua_Integer>(id));
    lua_pushstring(L, conn->remoteAddress().c_str());
    s.tcp[id] = std::move(conn);
    return 2;
}
/// agr.tcp.connect(host, port) -> connection | nil, error (opens in the following frames)
int t_connect(lua_State* L) {
    auto r = net::TcpConnection::connect(luaL_checkstring(L, 1), static_cast<u16>(luaL_checkinteger(L, 2)));
    if (!r) return pushFailure(L, r.error().message);
    auto& s = socketsOf(L);
    const u64 id = s.next++;
    s.tcp[id] = std::move(r.value());
    lua_pushinteger(L, static_cast<lua_Integer>(id));
    return 1;
}
/// agr.tcp.send(connection, text) -> queued
int t_send(lua_State* L) {
    size_t n = 0;
    const char* data = luaL_checklstring(L, 2, &n);
    auto& c = checkHandle(L, socketsOf(L).tcp, "agr.tcp.send");
    lua_pushboolean(L, c.send(std::string_view(data, n)));
    c.poll();
    return 1;
}
/// agr.tcp.receive(connection) -> bytes received since the last call ("" if none)
int t_receive(lua_State* L) {
    auto& c = checkHandle(L, socketsOf(L).tcp, "agr.tcp.receive");
    c.poll();
    const std::string data = c.receive();
    lua_pushlstring(L, data.data(), data.size());
    return 1;
}
/// agr.tcp.state(connection) -> "connecting" | "open" | "closed"
int t_state(lua_State* L) {
    auto& c = checkHandle(L, socketsOf(L).tcp, "agr.tcp.state");
    c.poll();
    lua_pushstring(L, c.connected() ? "open" : c.closed() ? "closed" : "connecting");
    return 1;
}
/// agr.tcp.close(handle): a connection or a server
int t_close(lua_State* L) {
    auto& s = socketsOf(L);
    const u64 id = static_cast<u64>(luaL_checkinteger(L, 1));
    if (auto it = s.tcp.find(id); it != s.tcp.end()) {
        it->second->poll();  // what is queued still goes out
        s.tcp.erase(it);
    }
    s.tcpServers.erase(id);
    return 0;
}

// ---- UDP
/// agr.udp.open([port]) -> socket, port | nil, error
int u_open(lua_State* L) {
    auto r = net::UdpSocket::open(static_cast<u16>(luaL_optinteger(L, 1, 0)));
    if (!r) return pushFailure(L, r.error().message);
    auto& s = socketsOf(L);
    const u64 id = s.next++;
    const u16 port = r.value()->port();
    s.udp[id] = std::move(r.value());
    lua_pushinteger(L, static_cast<lua_Integer>(id));
    lua_pushinteger(L, port);
    return 2;
}
/// agr.udp.send(socket, host, port, bytes) -> sent
int u_send(lua_State* L) {
    size_t n = 0;
    auto& u = checkHandle(L, socketsOf(L).udp, "agr.udp.send");
    const char* host = luaL_checkstring(L, 2);
    const u16 port = static_cast<u16>(luaL_checkinteger(L, 3));
    const char* data = luaL_checklstring(L, 4, &n);
    lua_pushboolean(L, u.sendTo(host, port, std::string_view(data, n)));
    return 1;
}
/// agr.udp.receive(socket) -> { {host =, port =, data =}, ... }
int u_receive(lua_State* L) {
    auto& u = checkHandle(L, socketsOf(L).udp, "agr.udp.receive");
    const auto list = u.receive();
    lua_createtable(L, static_cast<int>(list.size()), 0);
    for (usize i = 0; i < list.size(); ++i) {
        lua_createtable(L, 0, 3);
        lua_pushstring(L, list[i].host.c_str());
        lua_setfield(L, -2, "host");
        lua_pushinteger(L, list[i].port);
        lua_setfield(L, -2, "port");
        lua_pushlstring(L, list[i].data.data(), list[i].data.size());
        lua_setfield(L, -2, "data");
        lua_rawseti(L, -2, static_cast<lua_Integer>(i + 1));
    }
    return 1;
}
int u_close(lua_State* L) {
    socketsOf(L).udp.erase(static_cast<u64>(luaL_checkinteger(L, 1)));
    return 0;
}

// ---- WebSocket server
/// agr.wsserver.listen(port [, localOnly]) -> server, port | nil, error
int w_listen(lua_State* L) {
    auto r = net::WebSocketServer::listen(static_cast<u16>(luaL_checkinteger(L, 1)), lua_toboolean(L, 2) != 0);
    if (!r) return pushFailure(L, r.error().message);
    auto& s = socketsOf(L);
    const u64 id = s.next++;
    const u16 port = r.value()->port();
    s.wsServers[id] = std::move(r.value());
    lua_pushinteger(L, static_cast<lua_Integer>(id));
    lua_pushinteger(L, port);
    return 2;
}
/// agr.wsserver.poll(server) -> { {type = "connected" | "message" | "disconnected", client =, data =, binary =}, ... }
int w_poll(lua_State* L) {
    auto& w = checkHandle(L, socketsOf(L).wsServers, "agr.wsserver.poll");
    w.poll();
    const auto events = w.takeEvents();
    lua_createtable(L, static_cast<int>(events.size()), 0);
    for (usize i = 0; i < events.size(); ++i) {
        const auto& e = events[i];
        lua_createtable(L, 0, 4);
        lua_pushstring(L, e.type == net::WebSocketServer::Event::Type::Connected ? "connected"
                          : e.type == net::WebSocketServer::Event::Type::Message  ? "message"
                                                                                   : "disconnected");
        lua_setfield(L, -2, "type");
        lua_pushinteger(L, e.client);
        lua_setfield(L, -2, "client");
        if (e.type == net::WebSocketServer::Event::Type::Message) {
            lua_pushlstring(L, e.data.data(), e.data.size());
            lua_setfield(L, -2, "data");
            lua_pushboolean(L, e.binary);
            lua_setfield(L, -2, "binary");
        }
        lua_rawseti(L, -2, static_cast<lua_Integer>(i + 1));
    }
    return 1;
}
/// agr.wsserver.send(server, client, text [, binary]) -> sent
int w_send(lua_State* L) {
    size_t n = 0;
    auto& w = checkHandle(L, socketsOf(L).wsServers, "agr.wsserver.send");
    const u32 client = static_cast<u32>(luaL_checkinteger(L, 2));
    const char* data = luaL_checklstring(L, 3, &n);
    lua_pushboolean(L, w.send(client, std::string_view(data, n), lua_toboolean(L, 4) != 0));
    return 1;
}
/// agr.wsserver.broadcast(server, text [, binary])
int w_broadcast(lua_State* L) {
    size_t n = 0;
    auto& w = checkHandle(L, socketsOf(L).wsServers, "agr.wsserver.broadcast");
    const char* data = luaL_checklstring(L, 2, &n);
    w.broadcast(std::string_view(data, n), lua_toboolean(L, 3) != 0);
    return 0;
}
/// agr.wsserver.clients(server) -> { id, ... }
int w_clients(lua_State* L) {
    auto& w = checkHandle(L, socketsOf(L).wsServers, "agr.wsserver.clients");
    const auto ids = w.clients();
    lua_createtable(L, static_cast<int>(ids.size()), 0);
    for (usize i = 0; i < ids.size(); ++i) {
        lua_pushinteger(L, ids[i]);
        lua_rawseti(L, -2, static_cast<lua_Integer>(i + 1));
    }
    return 1;
}
/// agr.wsserver.close(server [, client]): one client, or the whole server
int w_close(lua_State* L) {
    auto& s = socketsOf(L);
    const u64 id = static_cast<u64>(luaL_checkinteger(L, 1));
    auto it = s.wsServers.find(id);
    if (it == s.wsServers.end()) return 0;
    if (lua_isnoneornil(L, 2)) s.wsServers.erase(it);
    else it->second->close(static_cast<u32>(luaL_checkinteger(L, 2)));
    return 0;
}

} // namespace

void openSocketBindings(lua_State* L) {
    static const luaL_Reg tcp[] = {{"listen", t_listen}, {"accept", t_accept}, {"connect", t_connect}, {"send", t_send},
                                   {"receive", t_receive}, {"state", t_state}, {"close", t_close}, {nullptr, nullptr}};
    addAgrLibrary(L, "tcp", tcp);
    static const luaL_Reg udp[] = {{"open", u_open}, {"send", u_send}, {"receive", u_receive}, {"close", u_close}, {nullptr, nullptr}};
    addAgrLibrary(L, "udp", udp);
    static const luaL_Reg ws[] = {{"listen", w_listen}, {"poll", w_poll}, {"send", w_send}, {"broadcast", w_broadcast},
                                  {"clients", w_clients}, {"close", w_close}, {nullptr, nullptr}};
    addAgrLibrary(L, "wsserver", ws);
}

} // namespace agr
