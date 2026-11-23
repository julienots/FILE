#include "agr/net/Sockets.hpp"

#include <algorithm>
#include <cstdio>
#include <cstring>

#if defined(_WIN32)
#    define WIN32_LEAN_AND_MEAN
#    include <winsock2.h>
#    include <ws2tcpip.h>
#else
#    include <arpa/inet.h>
#    include <cerrno>
#    include <fcntl.h>
#    include <netdb.h>
#    include <netinet/in.h>
#    include <netinet/tcp.h>
#    include <sys/socket.h>
#    include <unistd.h>
#endif

#include "agr/net/Http.hpp"

namespace agr::net {

namespace {

#if defined(_WIN32)
using Sock = SOCKET;
const i64 kNone = -1;
Sock sock(i64 s) { return static_cast<Sock>(s); }
void closeSock(i64 s) { closesocket(sock(s)); }
bool wouldBlock() {
    const int e = WSAGetLastError();
    return e == WSAEWOULDBLOCK || e == WSAEINPROGRESS || e == WSAEALREADY;
}
void setNonBlocking(i64 s) {
    u_long on = 1;
    ioctlsocket(sock(s), FIONBIO, &on);
}
struct WinsockInit {
    WinsockInit() {
        WSADATA d;
        WSAStartup(MAKEWORD(2, 2), &d);
    }
};
void ensureSockets() { static WinsockInit init; }
using SockLen = int;
#else
using Sock = int;
const i64 kNone = -1;
Sock sock(i64 s) { return static_cast<Sock>(s); }
void closeSock(i64 s) { ::close(sock(s)); }
bool wouldBlock() { return errno == EAGAIN || errno == EWOULDBLOCK || errno == EINPROGRESS || errno == EINTR; }
void setNonBlocking(i64 s) { fcntl(sock(s), F_SETFL, fcntl(sock(s), F_GETFL, 0) | O_NONBLOCK); }
void ensureSockets() {}
using SockLen = socklen_t;
#endif

i64 asHandle(Sock s) {
#if defined(_WIN32)
    return s == INVALID_SOCKET ? kNone : static_cast<i64>(s);
#else
    return s < 0 ? kNone : static_cast<i64>(s);
#endif
}

bool resolve(const std::string& host, u16 port, int type, sockaddr_in& out) {
    std::memset(&out, 0, sizeof out);
    out.sin_family = AF_INET;
    out.sin_port = htons(port);
    if (inet_pton(AF_INET, host.c_str(), &out.sin_addr) == 1) return true;
    addrinfo hints{};
    hints.ai_family = AF_INET;
    hints.ai_socktype = type;
    addrinfo* res = nullptr;
    if (getaddrinfo(host.c_str(), nullptr, &hints, &res) != 0 || !res) return false;
    out.sin_addr = reinterpret_cast<sockaddr_in*>(res->ai_addr)->sin_addr;
    freeaddrinfo(res);
    return true;
}

std::string addressText(const sockaddr_in& a) {
    char buf[INET_ADDRSTRLEN] = {};
    inet_ntop(AF_INET, &a.sin_addr, buf, sizeof buf);
    return buf;
}

Result<std::pair<i64, u16>> bindSocket(int type, u16 port, bool localOnly) {
    ensureSockets();
    const i64 s = asHandle(::socket(AF_INET, type, type == SOCK_STREAM ? IPPROTO_TCP : IPPROTO_UDP));
    if (s == kNone) return makeError(Error::Code::IOError, "socket() failed");
    int on = 1;
    setsockopt(sock(s), SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<const char*>(&on), sizeof on);
    if (type == SOCK_DGRAM) setsockopt(sock(s), SOL_SOCKET, SO_BROADCAST, reinterpret_cast<const char*>(&on), sizeof on);
    sockaddr_in a{};
    a.sin_family = AF_INET;
    a.sin_port = htons(port);
    a.sin_addr.s_addr = htonl(localOnly ? INADDR_LOOPBACK : INADDR_ANY);
    if (::bind(sock(s), reinterpret_cast<sockaddr*>(&a), sizeof a) != 0) {
        closeSock(s);
        return makeError(Error::Code::IOError, "port " + std::to_string(port) + " is not available");
    }
    SockLen len = sizeof a;
    getsockname(sock(s), reinterpret_cast<sockaddr*>(&a), &len);
    setNonBlocking(s);
    return std::make_pair(s, static_cast<u16>(ntohs(a.sin_port)));
}

std::string lowerCase(std::string s) {
    for (char& c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return s;
}

} // namespace

std::string localAddressFor(const std::string& host) {
    ensureSockets();
    sockaddr_in a{};
    if (!resolve(host, 9, SOCK_DGRAM, a)) return {};
    const i64 s = asHandle(::socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP));
    if (s == kNone) return {};
    std::string out;
    if (::connect(sock(s), reinterpret_cast<sockaddr*>(&a), sizeof a) == 0) {  // no packet is sent
        sockaddr_in local{};
        SockLen len = sizeof local;
        if (getsockname(sock(s), reinterpret_cast<sockaddr*>(&local), &len) == 0) out = addressText(local);
    }
    closeSock(s);
    return out;
}

// ------------------------------------------------------------------ TCP
TcpConnection::~TcpConnection() { close(); }

Result<std::unique_ptr<TcpConnection>> TcpConnection::connect(const std::string& host, u16 port) {
    ensureSockets();
    sockaddr_in a{};
    if (!resolve(host, port, SOCK_STREAM, a)) return makeError(Error::Code::NotFound, "unknown host '" + host + "'");
    const i64 s = asHandle(::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP));
    if (s == kNone) return makeError(Error::Code::IOError, "socket() failed");
    setNonBlocking(s);
    int on = 1;
    setsockopt(sock(s), IPPROTO_TCP, TCP_NODELAY, reinterpret_cast<const char*>(&on), sizeof on);
    std::unique_ptr<TcpConnection> c(new TcpConnection());
    c->m_socket = s;
    c->m_remote = addressText(a) + ":" + std::to_string(port);
    if (::connect(sock(s), reinterpret_cast<sockaddr*>(&a), sizeof a) == 0) c->m_state = State::Open;
    else if (wouldBlock()) c->m_state = State::Connecting;
    else return makeError(Error::Code::IOError, "cannot connect to " + c->m_remote);
    return c;
}

void TcpConnection::poll() {
    if (m_state == State::Closed) return;
    if (m_state == State::Connecting) {  // writable = connected (or failed)
        fd_set w;
        FD_ZERO(&w);
        FD_SET(sock(m_socket), &w);
        timeval tv{0, 0};
        if (select(static_cast<int>(m_socket + 1), nullptr, &w, nullptr, &tv) <= 0) return;
        int err = 0;
        SockLen len = sizeof err;
        getsockopt(sock(m_socket), SOL_SOCKET, SO_ERROR, reinterpret_cast<char*>(&err), &len);
        if (err != 0) {
            close();
            return;
        }
        m_state = State::Open;
    }
    while (!m_out.empty()) {
        const auto n = ::send(sock(m_socket), m_out.data(), static_cast<int>(std::min<usize>(m_out.size(), 1 << 20)), 0);
        if (n > 0) {
            m_out.erase(0, static_cast<usize>(n));
            continue;
        }
        if (n < 0 && wouldBlock()) break;
        close();
        return;
    }
    char buf[16384];
    while (true) {
        const auto n = ::recv(sock(m_socket), buf, sizeof buf, 0);
        if (n > 0) {
            m_in.append(buf, static_cast<usize>(n));
            if (m_in.size() > (64u << 20)) {  // the reader stopped reading: protect memory
                close();
                return;
            }
            continue;
        }
        if (n < 0 && wouldBlock()) break;
        close();  // 0 = closed by the other side, < 0 = error
        return;
    }
}

bool TcpConnection::send(std::string_view bytes) {
    if (m_state == State::Closed) return false;
    m_out.append(bytes);
    return true;
}

std::string TcpConnection::receive() { return std::exchange(m_in, {}); }

void TcpConnection::close() {
    if (m_socket != kNone) closeSock(m_socket);
    m_socket = kNone;
    m_state = State::Closed;
}

TcpServer::~TcpServer() {
    if (m_socket != kNone) closeSock(m_socket);
}

Result<std::unique_ptr<TcpServer>> TcpServer::listen(u16 port, bool localOnly) {
    auto bound = bindSocket(SOCK_STREAM, port, localOnly);
    if (!bound) return bound.error();
    if (::listen(sock(bound.value().first), 16) != 0) {
        closeSock(bound.value().first);
        return makeError(Error::Code::IOError, "listen() failed");
    }
    std::unique_ptr<TcpServer> s(new TcpServer());
    s->m_socket = bound.value().first;
    s->m_port = bound.value().second;
    return s;
}

std::unique_ptr<TcpConnection> TcpServer::accept() {
    sockaddr_in a{};
    SockLen len = sizeof a;
    const i64 s = asHandle(::accept(sock(m_socket), reinterpret_cast<sockaddr*>(&a), &len));
    if (s == kNone) return nullptr;
    setNonBlocking(s);
    int on = 1;
    setsockopt(sock(s), IPPROTO_TCP, TCP_NODELAY, reinterpret_cast<const char*>(&on), sizeof on);
    std::unique_ptr<TcpConnection> c(new TcpConnection());
    c->m_socket = s;
    c->m_state = TcpConnection::State::Open;
    c->m_remote = addressText(a) + ":" + std::to_string(ntohs(a.sin_port));
    return c;
}

// ------------------------------------------------------------------ UDP
UdpSocket::~UdpSocket() {
    if (m_socket != kNone) closeSock(m_socket);
}

Result<std::unique_ptr<UdpSocket>> UdpSocket::open(u16 port) {
    auto bound = bindSocket(SOCK_DGRAM, port, false);
    if (!bound) return bound.error();
    std::unique_ptr<UdpSocket> u(new UdpSocket());
    u->m_socket = bound.value().first;
    u->m_port = bound.value().second;
    return u;
}

bool UdpSocket::sendTo(const std::string& host, u16 port, std::string_view bytes) {
    sockaddr_in a{};
    if (bytes.size() > 65000 || !resolve(host, port, SOCK_DGRAM, a)) return false;
    return ::sendto(sock(m_socket), bytes.data(), static_cast<int>(bytes.size()), 0, reinterpret_cast<sockaddr*>(&a), sizeof a) ==
           static_cast<decltype(::sendto(sock(m_socket), nullptr, 0, 0, nullptr, 0))>(bytes.size());
}

std::vector<UdpSocket::Datagram> UdpSocket::receive() {
    std::vector<Datagram> out;
    char buf[65536];
    for (int i = 0; i < 1024; ++i) {
        sockaddr_in a{};
        SockLen len = sizeof a;
        const auto n = ::recvfrom(sock(m_socket), buf, sizeof buf, 0, reinterpret_cast<sockaddr*>(&a), &len);
        if (n < 0) break;
        out.push_back({addressText(a), static_cast<u16>(ntohs(a.sin_port)), std::string(buf, static_cast<usize>(n))});
    }
    return out;
}

// ------------------------------------------------------------------ WebSocket server
WebSocketServer::~WebSocketServer() {
    for (auto& [id, c] : m_clients) {
        if (c.open) frame(c, 0x8, std::string("\x03\xe9", 2));  // 1001: going away
        c.tcp->poll();
    }
}

Result<std::unique_ptr<WebSocketServer>> WebSocketServer::listen(u16 port, bool localOnly) {
    auto tcp = TcpServer::listen(port, localOnly);
    if (!tcp) return tcp.error();
    std::unique_ptr<WebSocketServer> s(new WebSocketServer());
    s->m_server = std::move(tcp.value());
    return s;
}

void WebSocketServer::frame(Client& c, u8 opcode, std::string_view payload) {
    std::string f;
    f.push_back(static_cast<char>(0x80 | opcode));  // FIN, server frames are not masked
    if (payload.size() < 126) {
        f.push_back(static_cast<char>(payload.size()));
    } else if (payload.size() < 65536) {
        f.push_back(126);
        f.push_back(static_cast<char>(payload.size() >> 8));
        f.push_back(static_cast<char>(payload.size() & 0xFF));
    } else {
        f.push_back(127);
        for (int i = 7; i >= 0; --i) f.push_back(static_cast<char>((static_cast<u64>(payload.size()) >> (8 * i)) & 0xFF));
    }
    f.append(payload);
    c.tcp->send(f);
}

bool WebSocketServer::parse(u32 id, Client& c) {
    if (!c.open) {  // HTTP upgrade request
        const usize end = c.buffer.find("\r\n\r\n");
        if (end == std::string::npos) return c.buffer.size() < 16384;
        const std::string head = c.buffer.substr(0, end);
        c.buffer.erase(0, end + 4);
        std::string key;
        bool upgrade = false;
        usize at = head.find("\r\n");
        if (head.rfind("GET ", 0) != 0) return false;
        while (at != std::string::npos && at < head.size()) {
            const usize next = head.find("\r\n", at + 2);
            const std::string line = head.substr(at + 2, (next == std::string::npos ? head.size() : next) - at - 2);
            const usize colon = line.find(':');
            if (colon != std::string::npos) {
                const std::string name = lowerCase(line.substr(0, colon));
                std::string value = line.substr(colon + 1);
                while (!value.empty() && value.front() == ' ') value.erase(0, 1);
                if (name == "sec-websocket-key") key = value;
                if (name == "upgrade" && lowerCase(value) == "websocket") upgrade = true;
            }
            at = next;
        }
        if (!upgrade || key.empty()) {
            c.tcp->send("HTTP/1.1 400 Bad Request\r\nContent-Length: 0\r\nConnection: close\r\n\r\n");
            c.tcp->poll();
            return false;
        }
        c.tcp->send("HTTP/1.1 101 Switching Protocols\r\nUpgrade: websocket\r\nConnection: Upgrade\r\nSec-WebSocket-Accept: " + webSocketAccept(key) + "\r\n\r\n");
        c.open = true;
        m_events.push_back({Event::Type::Connected, id, {}, false});
    }
    // Frames: FIN/opcode, MASK/length (client frames must be masked), mask key, payload.
    while (c.buffer.size() >= 2) {
        const auto* b = reinterpret_cast<const u8*>(c.buffer.data());
        const bool fin = b[0] & 0x80;
        const u8 opcode = b[0] & 0x0F;
        if (!(b[1] & 0x80)) {  // unmasked client frame: protocol error
            frame(c, 0x8, std::string("\x03\xea", 2));
            return false;
        }
        u64 len = b[1] & 0x7F;
        usize pos = 2;
        if (len == 126) {
            if (c.buffer.size() < 4) return true;
            len = (static_cast<u64>(b[2]) << 8) | b[3];
            pos = 4;
        } else if (len == 127) {
            if (c.buffer.size() < 10) return true;
            len = 0;
            for (int i = 0; i < 8; ++i) len = (len << 8) | b[2 + i];
            pos = 10;
        }
        if (len > maxMessageBytes) {
            frame(c, 0x8, std::string("\x03\xf1", 2));  // 1009: too big
            return false;
        }
        if (c.buffer.size() < pos + 4 + len) return true;  // wait for the rest
        const u8* mask = b + pos;
        std::string payload(c.buffer.data() + pos + 4, static_cast<usize>(len));
        for (usize i = 0; i < payload.size(); ++i) payload[i] = static_cast<char>(payload[i] ^ mask[i & 3]);
        c.buffer.erase(0, pos + 4 + static_cast<usize>(len));
        switch (opcode) {
            case 0x0:  // continuation
                if (c.fragmentOpcode == 0) return false;
                c.fragments += payload;
                if (c.fragments.size() > maxMessageBytes) return false;
                if (fin) {
                    m_events.push_back({Event::Type::Message, id, std::exchange(c.fragments, {}), c.fragmentOpcode == 0x2});
                    c.fragmentOpcode = 0;
                }
                break;
            case 0x1:
            case 0x2:
                if (fin) m_events.push_back({Event::Type::Message, id, std::move(payload), opcode == 0x2});
                else c.fragmentOpcode = opcode, c.fragments = std::move(payload);
                break;
            case 0x8:  // close: answer and drop
                frame(c, 0x8, payload.substr(0, 2));
                c.tcp->poll();
                return false;
            case 0x9: frame(c, 0xA, payload); break;  // ping -> pong
            case 0xA: break;                          // pong
            default: return false;
        }
    }
    return true;
}

void WebSocketServer::poll() {
    while (auto conn = m_server->accept()) {
        Client c;
        c.tcp = std::move(conn);
        m_clients.emplace(m_nextId++, std::move(c));
    }
    std::vector<u32> gone;
    for (auto& [id, c] : m_clients) {
        c.tcp->poll();
        c.buffer += c.tcp->receive();
        const bool keep = !c.closing && parse(id, c) && !c.tcp->closed();
        if (!keep) gone.push_back(id);
        c.tcp->poll();  // what parse() queued (handshake answer, pong)
    }
    for (u32 id : gone) {
        auto it = m_clients.find(id);
        if (it->second.open) m_events.push_back({Event::Type::Disconnected, id, {}, false});
        it->second.tcp->poll();
        m_clients.erase(it);
    }
}

std::vector<WebSocketServer::Event> WebSocketServer::takeEvents() { return std::exchange(m_events, {}); }

bool WebSocketServer::send(u32 client, std::string_view text, bool binary) {
    auto it = m_clients.find(client);
    if (it == m_clients.end() || !it->second.open) return false;
    frame(it->second, binary ? 0x2 : 0x1, text);
    return true;
}

void WebSocketServer::broadcast(std::string_view text, bool binary) {
    for (auto& [id, c] : m_clients)
        if (c.open) frame(c, binary ? 0x2 : 0x1, text);
}

void WebSocketServer::close(u32 client, u16 code) {
    auto it = m_clients.find(client);
    if (it == m_clients.end()) return;
    const char payload[2] = {static_cast<char>(code >> 8), static_cast<char>(code & 0xFF)};
    if (it->second.open) frame(it->second, 0x8, std::string_view(payload, 2));
    it->second.closing = true;
}

std::vector<u32> WebSocketServer::clients() const {
    std::vector<u32> out;
    for (const auto& [id, c] : m_clients)
        if (c.open) out.push_back(id);
    return out;
}

// ------------------------------------------------------------------ WebSocket transport
namespace {
Address clientAddress(u32 client) { return Address{0x7F000000u | (client & 0xFFFFFFu), 1}; }  // stands for a WebSocket client
} // namespace

WebSocketTransport::WebSocketTransport() = default;
WebSocketTransport::~WebSocketTransport() { close(); }

Status WebSocketTransport::open(u16 port) {
    close();
    if (!m_hosting) return {};  // joining: connects on the first message (the host's address)
    auto s = WebSocketServer::listen(port, false);
    if (!s) return s.error();
    m_server = std::move(s.value());
    return {};
}

void WebSocketTransport::close() {
    m_server.reset();
    if (m_client) m_client->close();
    m_client.reset();
    m_inbox.clear();
}

u16 WebSocketTransport::localPort() const { return m_server ? m_server->port() : 0; }

Status WebSocketTransport::send(const Address& to, const void* data, usize size) {
    const std::string_view bytes(static_cast<const char*>(data), size);
    if (m_server) {
        if ((to.ip & 0xFF000000u) != 0x7F000000u || to.port != 1) return makeError(Error::Code::NotFound, "not a WebSocket client");
        if (!m_server->send(to.ip & 0xFFFFFFu, bytes, true)) return makeError(Error::Code::NotFound, "WebSocket client gone");
        return {};
    }
    if (!m_client) {
        m_host = to;
        m_client = std::make_unique<WebSocket>();
        char ip[32];
        std::snprintf(ip, sizeof ip, "%u.%u.%u.%u", (to.ip >> 24) & 255, (to.ip >> 16) & 255, (to.ip >> 8) & 255, to.ip & 255);
        m_client->connect("ws://" + std::string(ip) + ":" + std::to_string(to.port) + "/agr");
    }
    if (!m_client->sendBinary(bytes)) return makeError(Error::Code::IOError, "WebSocket closed");
    return {};
}

bool WebSocketTransport::receive(Address& from, std::vector<u8>& data) {
    if (m_inbox.empty()) {
        if (m_server) {
            m_server->poll();
            for (auto& e : m_server->takeEvents())
                if (e.type == WebSocketServer::Event::Type::Message) m_inbox.emplace_back(clientAddress(e.client), std::move(e.data));
        } else if (m_client) {
            for (auto& m : m_client->receive()) m_inbox.emplace_back(m_host, std::move(m.data));
        }
        std::reverse(m_inbox.begin(), m_inbox.end());  // taken from the back, oldest first
    }
    if (m_inbox.empty()) return false;
    from = m_inbox.back().first;
    data.assign(m_inbox.back().second.begin(), m_inbox.back().second.end());
    m_inbox.pop_back();
    return true;
}

} // namespace agr::net
