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

#include <array>

#include "agr/core/Result.hpp"
#include "agr/core/Types.hpp"

namespace agr::net {

/// IPv4 or IPv6 address and port.
struct Address {
    u32 ip = 0;  ///< IPv4, host byte order (127.0.0.1 = 0x7F000001).
    u16 port = 0;
    std::array<u8, 16> ip6{};  ///< IPv6 bytes (network order) when v6.
    bool v6 = false;
    bool operator==(const Address&) const = default;
    /// "1.2.3.4:5000" or "[2001:db8::1]:5000".
    std::string toString() const;
    /// "192.168.1.20", "::1", "[2001:db8::1]", "localhost" or a host name (resolved through the
    /// system; IPv4 is preferred when a name has both).
    static Result<Address> resolve(std::string_view host, u16 port);
    static Address ipv6(const std::array<u8, 16>& bytes, u16 port) { return {0, port, bytes, true}; }
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
    std::intptr_t m_socket6 = -1;  // IPv6 on the same port (absent if the system has no IPv6)
    u16 m_port = 0;
};

enum class NetRole : u8 { None, Host, Client };

struct NetEvent {
    enum class Type : u8 { Connected, Disconnected, Message };
    Type type = Type::Message;
    u32 peer = 0;        ///< Who connected / left / sent (clients: always 1, the host).
    std::string data;    ///< Message payload.
};

/// A game found on the local network by NetSession::discover().
struct LanGame {
    Address address;
    std::string name;
    u32 players = 0, maxPlayers = 0;
    f64 lastSeen = 0.0;
};

struct NetStats {
    u64 packetsSent = 0, packetsReceived = 0, bytesSent = 0, bytesReceived = 0, resent = 0;
    u64 rejected = 0;
    u64 staleDropped = 0;
    u64 bytesBeforeCompression = 0, bytesAfterCompression = 0;  ///< Payloads that were compressed.  ///< Unreliable ordered messages dropped because a newer one had arrived.  ///< Encrypted links: forged, tampered, replayed or unencrypted packets dropped.
};

class NetSession {
public:
    static constexpr u32 kHostId = 1;
    static constexpr u32 kBroadcast = 0;
    static constexpr usize kMaxPayload = 1200;  ///< Unreliable messages: below common MTUs (no fragmentation).
    /// Reliable messages: split into fragments of kMaxPayload and put back together in order.
    static constexpr usize kMaxReliable = 256 * 1024;
    /// Messages starting with this byte go to the system channel (replication, RPC): takeSystemEvents().
    static constexpr char kSystemTag = '\x01';

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
    /// (unreliable: kMaxPayload, reliable: kMaxReliable) or not connected.
    bool send(u32 peer, std::string_view data, bool reliable);
    /// Unreliable but ordered (Godot's unreliable_ordered): a message older than the last one
    /// received on its channel (0..7) is dropped instead of delivered late — positions, aim,
    /// voice. At most kMaxPayload bytes.
    bool sendOrdered(u32 peer, std::string_view data, u8 channel = 0);
    static constexpr u8 kChannels = 8;
    std::vector<NetEvent> takeEvents();
    /// Messages of the system channel (first byte kSystemTag) and the connection events.
    std::vector<NetEvent> takeSystemEvents();

    // ---- LAN discovery
    /// Host: answers the discovery requests of the local network with this name.
    void advertise(std::string name) { m_advertisement = std::move(name); }
    /// Looks for games hosted on `port` on the local network (broadcast); see lanGames(). Works
    /// while not hosting or joined (join() and host() stop it).
    Status discover(u16 port);
    void stopDiscovery();
    bool discovering() const { return m_discoverPort != 0; }
    /// Games that answered (dropped after 5 s without an answer).
    std::vector<LanGame> lanGames() const;

    NetRole role() const { return m_role; }
    /// 1 for the host, the assigned id for a connected client, 0 otherwise.
    u32 localId() const { return m_localId; }
    bool connected() const { return m_role == NetRole::Host || (m_role == NetRole::Client && m_localId != 0); }
    std::vector<u32> peers() const;
    u16 port() const { return m_transport ? m_transport->localPort() : 0; }
    const NetStats& stats() const { return m_stats; }
    /// Seconds without any packet before a peer is dropped.
    void setTimeout(f64 seconds) { m_timeout = seconds; }
    /// Game password: the host refuses players whose password differs (set it before host() /
    /// join()). Checked with a hash bound to each connection attempt; it does not encrypt.
    void setPassword(std::string password) { m_password = std::move(password); }
    /// Host: encrypts every message of the session (AES-256-GCM, a key per connection derived from
    /// the password and the connection's random token with PBKDF2-SHA-256). Needs a password: it is
    /// the shared secret. Clients follow the host (told in its welcome). Tampered, replayed or
    /// unencrypted packets from an encrypted peer are dropped.
    void setEncryption(bool on) { m_encrypt = on; }
    /// Compresses the messages this machine sends (LZ, only when it saves bytes; each packet says
    /// so, the other side needs no setting). JSON game states often shrink 2 to 5 times.
    void setCompression(bool on) { m_compress = on; }
    bool compression() const { return m_compress; }
    /// Bytes sent and received per second over the last minute (oldest first): network profiler.
    struct Bandwidth {
        u64 sent = 0, received = 0;
    };
    const std::deque<Bandwidth>& bandwidthHistory() const { return m_bandwidth; }
    bool encryption() const { return m_encrypt; }
    /// True when the link to this peer is encrypted.
    bool encrypted(u32 peer) const {
        auto it = m_peers.find(peer);
        return it != m_peers.end() && it->second.encrypted;
    }
    /// Last reason a join failed ("full", "wrong password"), empty otherwise.
    const std::string& joinError() const { return m_joinError; }
    /// Round-trip time to a peer in seconds (smoothed; 0 until measured).
    f64 rtt(u32 peer) const;
    /// Shared clock: the host's time (seconds, the `now` its update() receives), estimated on
    /// clients from the pings (half the round trip); the local time when not in a session.
    f64 networkTime() const { return m_now + (m_role == NetRole::Client ? m_clockOffset : 0.0); }
    bool clockSynced() const { return m_role != NetRole::Client || m_clockSamples > 0; }

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
        u32 orderedOut[8]{}, orderedIn[8]{};    ///< Unreliable ordered: last counter sent / delivered per channel.
        std::string assembly;  ///< Fragments of a large reliable message received so far.
        bool welcomed = false;
        f64 rtt = 0.0;
        // Encryption (AES-256-GCM, key from the password and this connection's token).
        bool encrypted = false;
        std::array<u8, 32> key{};
        u64 sendCounter = 0, recvHighest = 0, recvWindow = 0;
    };
    bool seal(Peer& p, std::string& packet);                     ///< Encrypts after the header.
    bool open(Peer& p, const std::vector<u8>& packet, std::string& payload);  ///< Checks and decrypts.
    void sendPacket(Peer& p, u8 type, u32 seq, std::string_view payload);
    void handle(const Address& from, const std::vector<u8>& packet, f64 now);
    Peer* findPeer(const Address& a);
    void drop(u32 id, bool notify);
    void deliver(Peer& p, std::string_view payload);  ///< Reliable payload in order (fragment header).
    void push(NetEvent e);                             ///< To the user or the system queue.

    std::unique_ptr<NetTransport> m_transport;
    NetRole m_role = NetRole::None;
    u32 m_localId = 0, m_nextId = 2, m_maxPeers = 8;
    u32 m_token = 0;  ///< Random per join: tells a new connection from a stale one.
    std::unordered_map<u32, Peer> m_peers;
    std::vector<NetEvent> m_events, m_systemEvents;
    std::string m_advertisement = "AGR game";
    u16 m_discoverPort = 0;
    f64 m_lastDiscover = -1.0;
    std::vector<LanGame> m_lanGames;
    NetStats m_stats;
    f64 m_now = 0.0, m_timeout = 5.0, m_joinStarted = 0.0;
    f64 m_clockOffset = 0.0;  ///< Client: host time - local time.
    u32 m_clockSamples = 0;
    std::vector<u8> m_buffer;
    std::string m_password, m_joinError;
    bool m_encrypt = false;
    bool m_compress = false;
    std::deque<Bandwidth> m_bandwidth;
    f64 m_bandwidthStart = -1.0;
    u64 m_bandwidthSent = 0, m_bandwidthReceived = 0;
};

} // namespace agr::net
