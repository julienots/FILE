#pragma once
// Low-level sockets for games and tools (like Godot's TCPServer / StreamPeerTCP / PacketPeerUDP /
// WebSocket server): non-blocking, polled from the game loop, no thread.
//  * TcpServer / TcpConnection: byte streams (custom protocols, tools, a local dashboard).
//  * UdpSocket: datagrams to and from any address (LAN tools, custom game protocols).
//  * WebSocketServer: RFC 6455 server (browsers, phones, companion apps, a WebSocket client).
#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "agr/core/Result.hpp"
#include "agr/core/Types.hpp"

namespace agr::net {

/// This machine's IPv4 address on the route to `host` ("" if unknown): what a router or a LAN
/// peer sees.
std::string localAddressFor(const std::string& host);

class TcpConnection {
public:
    ~TcpConnection();
    TcpConnection(const TcpConnection&) = delete;
    TcpConnection& operator=(const TcpConnection&) = delete;
    /// Starts connecting to host:port (name or address). The connection opens in a later poll().
    static Result<std::unique_ptr<TcpConnection>> connect(const std::string& host, u16 port);
    /// Sends pending bytes, reads what arrived. Call every frame.
    void poll();
    /// Queues bytes (sent by poll()). False once closed.
    bool send(std::string_view bytes);
    /// Bytes received since the last call.
    std::string receive();
    bool connected() const { return m_state == State::Open; }
    bool closed() const { return m_state == State::Closed; }
    void close();
    std::string remoteAddress() const { return m_remote; }

private:
    friend class TcpServer;
    friend class WebSocketServer;
    enum class State : u8 { Connecting, Open, Closed };
    TcpConnection() = default;
    i64 m_socket = -1;
    State m_state = State::Closed;
    std::string m_out, m_in, m_remote;
};

class TcpServer {
public:
    ~TcpServer();
    /// Listens on `port` (0 = any free port, see port()), every interface unless `localOnly`.
    static Result<std::unique_ptr<TcpServer>> listen(u16 port, bool localOnly = false);
    /// Accepts a waiting connection (nullptr when none).
    std::unique_ptr<TcpConnection> accept();
    u16 port() const { return m_port; }

private:
    TcpServer() = default;
    i64 m_socket = -1;
    u16 m_port = 0;
};

class UdpSocket {
public:
    ~UdpSocket();
    /// Binds `port` (0 = any free port); broadcast allowed.
    static Result<std::unique_ptr<UdpSocket>> open(u16 port = 0);
    /// To host:port (name or IPv4 address, "255.255.255.255" for the LAN).
    bool sendTo(const std::string& host, u16 port, std::string_view bytes);
    struct Datagram {
        std::string host;
        u16 port = 0;
        std::string data;
    };
    /// Datagrams received since the last call.
    std::vector<Datagram> receive();
    u16 port() const { return m_port; }

private:
    UdpSocket() = default;
    i64 m_socket = -1;
    u16 m_port = 0;
};

/// WebSocket server: plain ws:// (put a TLS proxy in front for wss://). Polled every frame.
class WebSocketServer {
public:
    struct Event {
        enum class Type : u8 { Connected, Message, Disconnected } type = Type::Message;
        u32 client = 0;
        std::string data;
        bool binary = false;
    };
    ~WebSocketServer();
    static Result<std::unique_ptr<WebSocketServer>> listen(u16 port, bool localOnly = false);
    /// Accepts clients, does their handshake, reads their frames, answers pings, sends queued data.
    void poll();
    std::vector<Event> takeEvents();
    bool send(u32 client, std::string_view text, bool binary = false);
    void broadcast(std::string_view text, bool binary = false);
    void close(u32 client, u16 code = 1000);
    std::vector<u32> clients() const;
    u16 port() const { return m_server ? m_server->port() : 0; }
    usize maxMessageBytes = 16u << 20;

private:
    struct Client {
        std::unique_ptr<TcpConnection> tcp;
        bool open = false, closing = false;
        std::string buffer;      // received, not parsed yet
        std::string fragments;   // a fragmented message so far
        u8 fragmentOpcode = 0;
    };
    WebSocketServer() = default;
    void frame(Client& c, u8 opcode, std::string_view payload);
    bool parse(u32 id, Client& c);  // false: drop the client
    std::unique_ptr<TcpServer> m_server;
    std::unordered_map<u32, Client> m_clients;
    std::vector<Event> m_events;
    u32 m_nextId = 1;
};

} // namespace agr::net
