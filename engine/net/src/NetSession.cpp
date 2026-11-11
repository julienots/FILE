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

enum PacketType : u8 { Hello = 1, Welcome = 2, Data = 3, Reliable = 4, Ack = 5, Ping = 6, Bye = 7, Full = 8 };

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
    if (Status s = m_transport->open(port); !s) return s;
    m_role = NetRole::Host;
    m_localId = kHostId;
    m_nextId = 2;
    m_maxPeers = std::max<u32>(1, maxPeers);
    AGR_LOG_INFO("Net", "hosting on UDP port {}", m_transport->localPort());
    return {};
}

Status NetSession::join(std::string_view address, u16 port) {
    close();
    auto addr = Address::resolve(address, port);
    if (!addr) return addr.error();
    if (Status s = m_transport->open(0); !s) return s;
    m_role = NetRole::Client;
    m_localId = 0;
    m_token = std::random_device{}() | 1u;
    Peer host;
    host.id = kHostId;
    host.address = addr.value();
    host.lastHeard = m_now;
    host.lastPing = -1.0;
    m_peers[kHostId] = std::move(host);
    m_joinStarted = m_now;
    AGR_LOG_INFO("Net", "joining {}", addr.value().toString());
    return {};
}

void NetSession::close() {
    if (m_role != NetRole::None)
        for (auto& [id, p] : m_peers) sendPacket(p, Bye, 0, {});
    m_peers.clear();
    m_role = NetRole::None;
    m_localId = 0;
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

void NetSession::drop(u32 id, bool notify) {
    if (m_peers.erase(id) == 0) return;
    if (notify) m_events.push_back({NetEvent::Type::Disconnected, id, {}});
    if (m_role == NetRole::Client && id == kHostId) {  // lost the host: the session is over
        m_localId = 0;
        m_role = NetRole::None;
        m_transport->close();
    }
}

bool NetSession::send(u32 peer, std::string_view data, bool reliable) {
    if (data.size() > kMaxPayload || !connected()) return false;
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
        Pending m{p.nextSendSeq++, std::string(data), m_now};
        sendPacket(p, Reliable, m.seq, m.data);
        p.unacked.push_back(std::move(m));
    }
    return !targets.empty();
}

void NetSession::handle(const Address& from, const std::vector<u8>& packet, f64 now) {
    if (packet.size() < kHeader || std::memcmp(packet.data(), kMagic, 4) != 0) return;  // not ours
    const u8 type = packet[4];
    const u32 sender = getU32(packet.data() + 5), seq = getU32(packet.data() + 9), token = getU32(packet.data() + 13);
    const std::string_view payload(reinterpret_cast<const char*>(packet.data() + kHeader), packet.size() - kHeader);
    Peer* p = findPeer(from);

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
        m_events.push_back({NetEvent::Type::Connected, np.id, {}});
        m_peers[np.id] = std::move(np);
        return;
    }
    if (m_role == NetRole::Client && (type == Welcome || type == Full)) {
        if (!p || p->id != kHostId) return;
        if (type == Full) {
            AGR_LOG_WARN("Net", "the host is full");
            drop(kHostId, true);
            return;
        }
        if (payload.size() < 4 || getU32(reinterpret_cast<const u8*>(payload.data())) != m_token) return;  // stale welcome
        p->lastHeard = now;
        if (m_localId == 0) {
            m_localId = seq;
            p->welcomed = true;
            AGR_LOG_INFO("Net", "joined as player {}", m_localId);
            m_events.push_back({NetEvent::Type::Connected, kHostId, {}});
        }
        return;
    }
    if (!p || !p->welcomed) return;  // unknown sender
    if (m_role == NetRole::Host && sender != p->id) return;
    p->lastHeard = now;
    switch (type) {
        case Data: m_events.push_back({NetEvent::Type::Message, p->id, std::string(payload)}); break;
        case Reliable: {
            sendPacket(*p, Ack, seq, {});
            if (seq < p->nextRecvSeq) break;  // duplicate (our ack was lost)
            if (seq > p->nextRecvSeq) {
                if (p->early.size() < kMaxEarly) p->early.emplace(seq, std::string(payload));
                break;
            }
            m_events.push_back({NetEvent::Type::Message, p->id, std::string(payload)});
            ++p->nextRecvSeq;
            for (auto it = p->early.find(p->nextRecvSeq); it != p->early.end(); it = p->early.find(p->nextRecvSeq)) {
                m_events.push_back({NetEvent::Type::Message, p->id, std::move(it->second)});
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
        case Ping: break;
        case Bye: drop(p->id, true); break;
        default: break;
    }
}

void NetSession::update(f64 now) {
    m_now = now;
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
        if (now - p.lastHeard > m_timeout) {
            silent.push_back(id);
            continue;
        }
        if (m_role == NetRole::Client && !p.welcomed) {  // still joining: say hello again
            if (now - p.lastPing >= kHelloInterval) {
                sendPacket(p, Hello, 0, {});
                p.lastPing = now;
            }
            continue;
        }
        if (now - p.lastPing >= kPingInterval) {
            sendPacket(p, Ping, 0, {});
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

std::vector<u32> NetSession::peers() const {
    std::vector<u32> out;
    for (const auto& [id, p] : m_peers)
        if (p.welcomed) out.push_back(id);
    std::sort(out.begin(), out.end());
    return out;
}

} // namespace agr::net
