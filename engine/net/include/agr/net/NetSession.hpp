#pragma once
// Small UDP networking layer for multiplayer games: one host, up to `maxPeers` clients.
//
//  * connection handshake, keep-alive pings and timeouts;
//  * unreliable messages (positions sent every frame) and reliable ordered messages
//    (scores, events: resent until acknowledged, delivered once and in order);
//  * a transport interface so other transports (relay servers, platform services) can be
//    added without changing games: `NetTransport`, implemented here over UDP sockets.
//
// The host is peer 1; clients get ids 2, 3... Clients only talk to the host (the game's
// host script relays what others must know). Payloads are bytes (Lua scripts send tables,
// encoded as JSON by agr.net). See docs/NETWORK.md.
#include <deque>
#include <map>
#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "agr/core/Result.hpp"
#include "agr/core/Types.hpp"

namespace agr::net {

/// IPv4 address and port.
struct Address {
    u32 ip = 0;  ///< Host byte order (127.0.0.1 = 0x7F000001).
    u16 port = 0;
    bool operator==(const Address&) const = default;
    std::string toString() const;
    /// "192.168.1.20", "localhost" or a host name (resolved through the system).
    static Result<Address> resolve(std::string_view host, u16 port);
};

/// Datagram transport. UdpTransport is the default; tests and relays may provide others.
class NetTransport {
public:
    virtual ~NetTransport() = default;
    /// Opens the local endpoint (port 0 = any free port).
    virtual Status open(u16 port) = 0;
    virtual void close() = 0;
    virtual u16 localPort() const = 0;
    virtual Status send(const Address& to, const void* data, usize size) = 0;
    /// Next datagram, false when none is waiting (non-blocking).
    virtual bool receive(Address& from, std::vector<u8>& data) = 0;
};

class UdpTransport final : public NetTransport {
public:
    UdpTransport();
    ~UdpTransport() override;
    Status open(u16 port) override;
    void close() override;
    u16 localPort() const override { return m_port; }
    Status send(const Address& to, const void* data, usize size) override;
    bool receive(Address& from, std::vector<u8>& data) override;

private:
    std::intptr_t m_socket = -1;
    u16 m_port = 0;
};

enum class NetRole : u8 { None, Host, Client };

struct NetEvent {
    enum class Type : u8 { Connected, Disconnected, Message };
    Type type = Type::Message;
    u32 peer = 0;        ///< Who connected / left / sent (clients: always 1, the host).
    std::string data;    ///< Message payload.
};

struct NetStats {
    u64 packetsSent = 0, packetsReceived = 0, bytesSent = 0, bytesReceived = 0, resent = 0;
};

class NetSession {
public:
    static constexpr u32 kHostId = 1;
    static constexpr u32 kBroadcast = 0;
    static constexpr usize kMaxPayload = 1200;  ///< Below common MTUs (no fragmentation).

    NetSession();
    explicit NetSession(std::unique_ptr<NetTransport> transport);
    ~NetSession();

    /// Starts hosting on `port` (0 = any; see port()).
    Status host(u16 port, u32 maxPeers = 8);
    /// Connects to a host; Connected (peer 1) arrives once the host accepted.
    Status join(std::string_view address, u16 port);
    /// Tells the other side and closes (Disconnected events are not produced for oneself).
    void close();

    /// Receives, resends unacknowledged reliable messages, pings, drops silent peers.
    /// `now` in seconds (any monotonic clock).
    void update(f64 now);
    /// To one peer, or kBroadcast (host: every client; client: the host). False if too large
    /// or not connected.
    bool send(u32 peer, std::string_view data, bool reliable);
    std::vector<NetEvent> takeEvents();

    NetRole role() const { return m_role; }
    /// 1 for the host, the assigned id for a connected client, 0 otherwise.
    u32 localId() const { return m_localId; }
    bool connected() const { return m_role == NetRole::Host || (m_role == NetRole::Client && m_localId != 0); }
    std::vector<u32> peers() const;
    u16 port() const { return m_transport ? m_transport->localPort() : 0; }
    const NetStats& stats() const { return m_stats; }
    /// Seconds without any packet before a peer is dropped.
    void setTimeout(f64 seconds) { m_timeout = seconds; }

private:
    struct Pending {
        u32 seq = 0;
        std::string data;
        f64 lastSent = -1.0;
    };
    struct Peer {
        u32 id = 0;
        Address address;
        f64 lastHeard = 0.0, lastPing = 0.0;
        u32 nextSendSeq = 1, nextRecvSeq = 1;
        std::deque<Pending> unacked;
        std::map<u32, std::string> early;  ///< Reliable messages received ahead of order.
        bool welcomed = false;
    };
    void sendPacket(Peer& p, u8 type, u32 seq, std::string_view payload);
    void handle(const Address& from, const std::vector<u8>& packet, f64 now);
    Peer* findPeer(const Address& a);
    void drop(u32 id, bool notify);

    std::unique_ptr<NetTransport> m_transport;
    NetRole m_role = NetRole::None;
    u32 m_localId = 0, m_nextId = 2, m_maxPeers = 8;
    u32 m_token = 0;  ///< Random per join: tells a new connection from a stale one.
    std::unordered_map<u32, Peer> m_peers;
    std::vector<NetEvent> m_events;
    NetStats m_stats;
    f64 m_now = 0.0, m_timeout = 5.0, m_joinStarted = 0.0;
    std::vector<u8> m_buffer;
};

} // namespace agr::net
