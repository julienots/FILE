#include "agr/net/NetSession.hpp"

#include <algorithm>
#include <cstring>
#include <format>
#include <random>

#if defined(_WIN32)
#    define WIN32_LEAN_AND_MEAN
#    include <winsock2.h>
#    include <ws2tcpip.h>
#    pragma comment(lib, "ws2_32.lib")
#else
#    include <arpa/inet.h>
#    include <cerrno>
#    include <fcntl.h>
#    include <netdb.h>
#    include <netinet/in.h>
#    include <sys/socket.h>
#    include <unistd.h>
#endif

#include "agr/core/Log.hpp"

namespace agr::net {

namespace {

constexpr u8 kMagic[4] = {'A', 'G', 'N', '1'};
constexpr usize kHeader = 4 + 1 + 4 + 4 + 4;  // magic, type, sender, seq, token
constexpr f64 kResendInterval = 0.2, kPingInterval = 0.5, kHelloInterval = 0.25;
constexpr usize kMaxEarly = 256, kMaxUnacked = 1024;

enum PacketType : u8 { Hello = 1, Welcome = 2, Data = 3, Reliable = 4, Ack = 5, Ping = 6, Bye = 7, Full = 8, Discover = 9, Info = 10, Denied = 11, Pong = 12 };

/// Proof of the password bound to the connection's token (FNV-1a 64, two rounds).
u64 passwordProof(const std::string& password, u32 token) {
    u64 h = 1469598103934665603ull;
    for (int round = 0; round < 2; ++round) {
        for (unsigned char c : password) h = (h ^ c) * 1099511628211ull;
        for (int i = 0; i < 4; ++i) h = (h ^ ((token >> (8 * i)) & 0xFF)) * 1099511628211ull;
    }
    return h;
}
// First byte of every reliable payload: a whole message, or a fragment of a large one.
constexpr char kWhole = 'M', kFragment = 'F', kLastFragment = 'L';

void putU32(std::string& out, u32 v) {
    for (int i = 0; i < 4; ++i) out.push_back(static_cast<char>((v >> (8 * i)) & 0xFF));
}
u32 getU32(const u8* p) { return u32(p[0]) | u32(p[1]) << 8 | u32(p[2]) << 16 | u32(p[3]) << 24; }

#if defined(_WIN32)
struct WinsockInit {
    WinsockInit() {
        WSADATA d;
        WSAStartup(MAKEWORD(2, 2), &d);
    }
    ~WinsockInit() { WSACleanup(); }
};
void ensureSockets() { static WinsockInit init; }
using SocketLen = int;
void closeSocket(std::intptr_t s) { closesocket(static_cast<SOCKET>(s)); }
bool wouldBlock() { return WSAGetLastError() == WSAEWOULDBLOCK; }
std::string lastSocketError() { return std::format("socket error {}", WSAGetLastError()); }
#else
void ensureSockets() {}
using SocketLen = socklen_t;
void closeSocket(std::intptr_t s) { ::close(static_cast<int>(s)); }
bool wouldBlock() { return errno == EAGAIN || errno == EWOULDBLOCK; }
std::string lastSocketError() { return std::strerror(errno); }
#endif

} // namespace

// ------------------------------------------------------------------ Address
std::string Address::toString() const {
    return std::format("{}.{}.{}.{}:{}", (ip >> 24) & 0xFF, (ip >> 16) & 0xFF, (ip >> 8) & 0xFF, ip & 0xFF, port);
}

Result<Address> Address::resolve(std::string_view host, u16 port) {
    ensureSockets();
    addrinfo hints{};
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_DGRAM;
    addrinfo* res = nullptr;
    const std::string name(host.empty() ? "127.0.0.1" : host);
    if (getaddrinfo(name.c_str(), nullptr, &hints, &res) != 0 || !res)
        return makeError(Error::Code::NotFound, "unknown host '" + name + "'");
    const auto* in = reinterpret_cast<const sockaddr_in*>(res->ai_addr);
    Address a{ntohl(in->sin_addr.s_addr), port};
    freeaddrinfo(res);
    return a;
}

// ------------------------------------------------------------------ UdpTransport
UdpTransport::UdpTransport() { ensureSockets(); }
UdpTransport::~UdpTransport() { close(); }

Status UdpTransport::open(u16 port) {
    close();
    const auto s = static_cast<std::intptr_t>(::socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP));
    if (s < 0) return makeError(Error::Code::IOError, "cannot create a UDP socket: " + lastSocketError());
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(port);
    if (::bind(static_cast<decltype(::socket(0, 0, 0))>(s), reinterpret_cast<sockaddr*>(&addr), sizeof addr) != 0) {
        const std::string err = lastSocketError();
        closeSocket(s);
        return makeError(Error::Code::IOError, std::format("cannot use UDP port {}: {}", port, err));
    }
    int yes = 1;  // LAN discovery sends to the broadcast address
    setsockopt(static_cast<decltype(::socket(0, 0, 0))>(s), SOL_SOCKET, SO_BROADCAST, reinterpret_cast<const char*>(&yes), sizeof yes);
#if defined(_WIN32)
    u_long nonBlocking = 1;
    ioctlsocket(static_cast<SOCKET>(s), FIONBIO, &nonBlocking);
#else
    fcntl(static_cast<int>(s), F_SETFL, fcntl(static_cast<int>(s), F_GETFL, 0) | O_NONBLOCK);
#endif
    sockaddr_in bound{};
    SocketLen len = sizeof bound;
    getsockname(static_cast<decltype(::socket(0, 0, 0))>(s), reinterpret_cast<sockaddr*>(&bound), &len);
    m_socket = s;
    m_port = ntohs(bound.sin_port);
    return {};
}

void UdpTransport::close() {
    if (m_socket >= 0) closeSocket(m_socket);
    m_socket = -1;
    m_port = 0;
}

Status UdpTransport::send(const Address& to, const void* data, usize size) {
    if (m_socket < 0) return makeError(Error::Code::InvalidArgument, "socket not open");
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(to.ip);
    addr.sin_port = htons(to.port);
    const auto sent = ::sendto(static_cast<decltype(::socket(0, 0, 0))>(m_socket), static_cast<const char*>(data), static_cast<int>(size), 0,
                               reinterpret_cast<sockaddr*>(&addr), sizeof addr);
    if (sent < 0) return makeError(Error::Code::IOError, "send failed: " + lastSocketError());
    return {};
}

bool UdpTransport::receive(Address& from, std::vector<u8>& data) {
    if (m_socket < 0) return false;
    data.resize(2048);
    sockaddr_in addr{};
    SocketLen len = sizeof addr;
    const auto n = ::recvfrom(static_cast<decltype(::socket(0, 0, 0))>(m_socket), reinterpret_cast<char*>(data.data()),
                              static_cast<int>(data.size()), 0, reinterpret_cast<sockaddr*>(&addr), &len);
    if (n < 0) {
        if (!wouldBlock()) AGR_LOG_DEBUG("Net", "receive: {}", lastSocketError());  // e.g. ICMP "port unreachable"
        return false;
    }
    data.resize(static_cast<usize>(n));
    from = {ntohl(addr.sin_addr.s_addr), ntohs(addr.sin_port)};
    return true;
}

// ------------------------------------------------------------------ NetSession
NetSession::NetSession() : m_transport(std::make_unique<UdpTransport>()) {}
NetSession::NetSession(std::unique_ptr<NetTransport> transport) : m_transport(std::move(transport)) {}
NetSession::~NetSession() { close(); }

Status NetSession::host(u16 port, u32 maxPeers) {
    close();
    stopDiscovery();
    if (Status s = m_transport->open(port); !s) return s;
    m_role = NetRole::Host;
    m_localId = kHostId;
    m_nextId = 2;
    m_maxPeers = std::max<u32>(1, maxPeers);
    AGR_LOG_INFO("Net", "hosting on UDP port {}", m_transport->localPort());
    return {};
}

f64 NetSession::rtt(u32 peer) const {
    auto it = m_peers.find(peer);
    return it == m_peers.end() ? 0.0 : it->second.rtt;
}

Status NetSession::join(std::string_view address, u16 port) {
    close();
    m_joinError.clear();
    stopDiscovery();
    auto addr = Address::resolve(address, port);
    if (!addr) return addr.error();
    if (Status s = m_transport->open(0); !s) return s;
    m_role = NetRole::Client;
    m_localId = 0;
    m_token = std::random_device{}() | 1u;
    Peer host;
    host.id = kHostId;
    host.address = addr.value();
    host.lastHeard = -1.0;  // set by the next update: the session's clock may be stale until then
    host.lastPing = -1.0;
    m_peers[kHostId] = std::move(host);
    m_joinStarted = m_now;
    AGR_LOG_INFO("Net", "joining {}", addr.value().toString());
    return {};
}

void NetSession::close() {
    if (m_role != NetRole::None)
        for (auto& [id, p] : m_peers)
            for (int copy = 0; copy < 3; ++copy) sendPacket(p, Bye, 0, {});  // unreliable: a few copies (no wait for the timeout)
    m_peers.clear();
    m_role = NetRole::None;
    m_localId = 0;
    m_clockOffset = 0.0;
    m_clockSamples = 0;
    if (m_transport) m_transport->close();
}

void NetSession::sendPacket(Peer& p, u8 type, u32 seq, std::string_view payload) {
    std::string out;
    out.reserve(kHeader + payload.size());
    out.append(reinterpret_cast<const char*>(kMagic), 4);
    out.push_back(static_cast<char>(type));
    putU32(out, m_localId);
    putU32(out, seq);
    putU32(out, m_token);
    out.append(payload);
    if (m_transport->send(p.address, out.data(), out.size())) {
        ++m_stats.packetsSent;
        m_stats.bytesSent += out.size();
    }
}

NetSession::Peer* NetSession::findPeer(const Address& a) {
    for (auto& [id, p] : m_peers)
        if (p.address == a) return &p;
    return nullptr;
}

void NetSession::push(NetEvent e) {
    if (e.type != NetEvent::Type::Message) {  // both sides need to know who is there
        m_systemEvents.push_back(e);
        m_events.push_back(std::move(e));
    } else if (!e.data.empty() && e.data[0] == kSystemTag) {
        m_systemEvents.push_back(std::move(e));
    } else {
        m_events.push_back(std::move(e));
    }
}

void NetSession::deliver(Peer& p, std::string_view payload) {
    if (payload.empty()) return;
    const char kind = payload[0];
    payload.remove_prefix(1);
    if (kind == kWhole) {
        push({NetEvent::Type::Message, p.id, std::string(payload)});
    } else if (kind == kFragment || kind == kLastFragment) {
        if (p.assembly.size() + payload.size() > kMaxReliable) {  // corrupt or hostile: forget it
            p.assembly.clear();
            return;
        }
        p.assembly.append(payload);
        if (kind == kLastFragment) push({NetEvent::Type::Message, p.id, std::exchange(p.assembly, {})});
    }
}

void NetSession::drop(u32 id, bool notify) {
    if (m_peers.erase(id) == 0) return;
    if (notify) push({NetEvent::Type::Disconnected, id, {}});
    if (m_role == NetRole::Client && id == kHostId) {  // lost the host: the session is over
        m_localId = 0;
        m_role = NetRole::None;
        m_transport->close();
    }
}

bool NetSession::send(u32 peer, std::string_view data, bool reliable) {
    if (data.size() > (reliable ? kMaxReliable : kMaxPayload) || !connected()) return false;
    std::vector<u32> targets;
    if (peer == kBroadcast) {
        for (const auto& [id, p] : m_peers)
            if (p.welcomed) targets.push_back(id);
    } else if (auto it = m_peers.find(peer); it != m_peers.end() && it->second.welcomed) {
        targets.push_back(peer);
    }
    for (u32 id : targets) {
        Peer& p = m_peers[id];
        if (!reliable) {
            sendPacket(p, Data, 0, data);
            continue;
        }
        if (p.unacked.size() >= kMaxUnacked) {  // the peer stopped acknowledging: give up on it
            AGR_LOG_WARN("Net", "peer {} does not acknowledge messages: disconnected", id);
            drop(id, true);
            continue;
        }
        // Fragments of kMaxPayload - 1 bytes (one byte of header each), sent in order.
        constexpr usize kChunk = kMaxPayload - 1;
        usize at = 0;
        do {
            const usize n = std::min(kChunk, data.size() - at);
            const bool last = at + n >= data.size();
            std::string chunk(1, data.size() <= kChunk ? kWhole : last ? kLastFragment : kFragment);
            chunk.append(data.substr(at, n));
            Pending m{p.nextSendSeq++, std::move(chunk), m_now};
            sendPacket(p, Reliable, m.seq, m.data);
            p.unacked.push_back(std::move(m));
            at += n;
        } while (at < data.size());
    }
    return !targets.empty();
}

void NetSession::handle(const Address& from, const std::vector<u8>& packet, f64 now) {
    if (packet.size() < kHeader || std::memcmp(packet.data(), kMagic, 4) != 0) return;  // not ours
    const u8 type = packet[4];
    const u32 sender = getU32(packet.data() + 5), seq = getU32(packet.data() + 9), token = getU32(packet.data() + 13);
    const std::string_view payload(reinterpret_cast<const char*>(packet.data() + kHeader), packet.size() - kHeader);
    Peer* p = findPeer(from);

    // LAN discovery: anyone may ask a host; answers are recorded while discovering.
    if (type == Discover) {
        if (m_role == NetRole::Host) {
            Peer tmp;
            tmp.address = from;
            std::string body;
            putU32(body, static_cast<u32>(m_peers.size() + 1));
            putU32(body, m_maxPeers + 1);
            body += m_advertisement.substr(0, 200);
            sendPacket(tmp, Info, 0, body);
        }
        return;
    }
    if (type == Info) {
        if (m_discoverPort == 0 || payload.size() < 8) return;
        const auto* b = reinterpret_cast<const u8*>(payload.data());
        LanGame g{from, std::string(payload.substr(8)), getU32(b), getU32(b + 4), now};
        auto it = std::find_if(m_lanGames.begin(), m_lanGames.end(), [&](const LanGame& x) { return x.address == from; });
        if (it == m_lanGames.end()) m_lanGames.push_back(std::move(g));
        else *it = std::move(g);
        return;
    }

    if (m_role == NetRole::Host && type == Hello) {
        if (p) {  // same address: an answer to a repeated hello (our welcome was lost)
            p->lastHeard = now;
            std::string body;
            putU32(body, token);
            sendPacket(*p, Welcome, p->id, body);
            return;
        }
        if (m_peers.size() >= m_maxPeers) {
            Peer tmp;
            tmp.address = from;
            sendPacket(tmp, Full, 0, {});
            return;
        }
        {
            u64 proof = 0;
            if (payload.size() >= 8) std::memcpy(&proof, payload.data(), 8);
            if (proof != passwordProof(m_password, token)) {
                Peer tmp;
                tmp.address = from;
                sendPacket(tmp, Denied, 0, {});
                AGR_LOG_WARN("Net", "refused {}: wrong password", from.toString());
                return;
            }
        }
        Peer np;
        np.id = m_nextId++;
        np.address = from;
        np.lastHeard = now;
        np.lastPing = now;
        np.welcomed = true;
        std::string body;
        putU32(body, token);
        sendPacket(np, Welcome, np.id, body);
        AGR_LOG_INFO("Net", "player {} joined from {}", np.id, from.toString());
        push({NetEvent::Type::Connected, np.id, {}});
        m_peers[np.id] = std::move(np);
        return;
    }
    if (m_role == NetRole::Client && (type == Welcome || type == Full || type == Denied)) {
        if (!p || p->id != kHostId) return;
        if (type == Full || type == Denied) {
            m_joinError = type == Full ? "full" : "wrong password";
            AGR_LOG_WARN("Net", "join refused: {}", m_joinError);
            drop(kHostId, true);
            return;
        }
        if (payload.size() < 4 || getU32(reinterpret_cast<const u8*>(payload.data())) != m_token) return;  // stale welcome
        p->lastHeard = now;
        if (m_localId == 0) {
            m_localId = seq;
            p->welcomed = true;
            AGR_LOG_INFO("Net", "joined as player {}", m_localId);
            push({NetEvent::Type::Connected, kHostId, {}});
        }
        return;
    }
    if (!p || !p->welcomed) return;  // unknown sender
    if (m_role == NetRole::Host && sender != p->id) return;
    p->lastHeard = now;
    switch (type) {
        case Data: push({NetEvent::Type::Message, p->id, std::string(payload)}); break;
        case Reliable: {
            sendPacket(*p, Ack, seq, {});
            if (seq < p->nextRecvSeq) break;  // duplicate (our ack was lost)
            if (seq > p->nextRecvSeq) {
                if (p->early.size() < kMaxEarly) p->early.emplace(seq, std::string(payload));
                break;
            }
            deliver(*p, payload);
            ++p->nextRecvSeq;
            for (auto it = p->early.find(p->nextRecvSeq); it != p->early.end(); it = p->early.find(p->nextRecvSeq)) {
                deliver(*p, it->second);
                p->early.erase(it);
                ++p->nextRecvSeq;
            }
            break;
        }
        case Ack: {
            auto& q = p->unacked;
            q.erase(std::remove_if(q.begin(), q.end(), [&](const Pending& m) { return m.seq == seq; }), q.end());
            break;
        }
        case Ping: {  // the stamp back, plus our own clock (shared network time)
            std::string reply(payload.begin(), payload.end());
            reply.resize(8, '\0');
            reply.append(8, '\0');
            std::memcpy(reply.data() + 8, &now, 8);
            sendPacket(*p, Pong, 0, reply);
            break;
        }
        case Pong:
            if (payload.size() >= 8) {
                f64 sent = 0.0;
                std::memcpy(&sent, payload.data(), 8);
                const f64 sample = std::max(now - sent, 0.0);
                p->rtt = p->rtt <= 0.0 ? sample : p->rtt * 0.8 + sample * 0.2;
                if (payload.size() >= 16 && m_role == NetRole::Client && p->id == kHostId) {
                    f64 hostTime = 0.0;
                    std::memcpy(&hostTime, payload.data() + 8, 8);
                    const f64 offset = hostTime + sample * 0.5 - now;  // the host's clock when the pong arrives
                    m_clockOffset = m_clockSamples == 0 ? offset : m_clockOffset * 0.9 + offset * 0.1;
                    ++m_clockSamples;
                }
            }
            break;
        case Bye: drop(p->id, true); break;
        default: break;
    }
}

void NetSession::update(f64 now) {
    m_now = now;
    if (m_discoverPort != 0 && m_role == NetRole::None) {
        if (m_lastDiscover < 0.0 || now - m_lastDiscover >= 1.0) {  // ask again every second
            Peer everyone;
            everyone.address = {0xFFFFFFFFu, m_discoverPort};
            sendPacket(everyone, Discover, 0, {});
            m_lastDiscover = now;
        }
        Address from;
        while (m_transport && m_transport->receive(from, m_buffer)) handle(from, m_buffer, now);
        std::erase_if(m_lanGames, [&](const LanGame& g) { return now - g.lastSeen > 5.0; });
        return;
    }
    if (m_role == NetRole::None) return;
    Address from;
    while (m_transport && m_transport->receive(from, m_buffer)) {
        ++m_stats.packetsReceived;
        m_stats.bytesReceived += m_buffer.size();
        handle(from, m_buffer, now);
        if (m_role == NetRole::None) return;
    }
    std::vector<u32> silent;
    for (auto& [id, p] : m_peers) {
        if (p.lastHeard < 0.0) p.lastHeard = now;
        if (now - p.lastHeard > m_timeout) {
            silent.push_back(id);
            continue;
        }
        if (m_role == NetRole::Client && !p.welcomed) {  // still joining: say hello again
            if (now - p.lastPing >= kHelloInterval) {
                std::string proof;
                const u64 h = passwordProof(m_password, m_token);
                proof.append(reinterpret_cast<const char*>(&h), 8);
                sendPacket(p, Hello, 0, proof);
                p.lastPing = now;
            }
            continue;
        }
        if (now - p.lastPing >= kPingInterval) {
            std::string stamp(8, '\0');
            std::memcpy(stamp.data(), &now, 8);
            sendPacket(p, Ping, 0, stamp);
            p.lastPing = now;
        }
        for (Pending& m : p.unacked)
            if (now - m.lastSent >= kResendInterval) {
                sendPacket(p, Reliable, m.seq, m.data);
                m.lastSent = now;
                ++m_stats.resent;
            }
    }
    for (u32 id : silent) {
        AGR_LOG_INFO("Net", "player {} timed out", id);
        drop(id, true);
    }
}

std::vector<NetEvent> NetSession::takeEvents() { return std::exchange(m_events, {}); }
std::vector<NetEvent> NetSession::takeSystemEvents() { return std::exchange(m_systemEvents, {}); }

Status NetSession::discover(u16 port) {
    if (m_role != NetRole::None) return makeError(Error::Code::InvalidArgument, "discovery needs a session that is not hosting or joined");
    if (m_discoverPort == 0)
        if (Status s = m_transport->open(0); !s) return s;
    m_discoverPort = port;
    m_lastDiscover = -1.0;
    m_lanGames.clear();
    return {};
}

void NetSession::stopDiscovery() {
    if (m_discoverPort == 0) return;
    m_discoverPort = 0;
    m_lanGames.clear();
    if (m_role == NetRole::None && m_transport) m_transport->close();
}

std::vector<LanGame> NetSession::lanGames() const { return m_lanGames; }

std::vector<u32> NetSession::peers() const {
    std::vector<u32> out;
    for (const auto& [id, p] : m_peers)
        if (p.welcomed) out.push_back(id);
    std::sort(out.begin(), out.end());
    return out;
}

} // namespace agr::net
