#include "agr/net/Http.hpp"

#include <algorithm>
#include <cctype>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <format>
#include <optional>

#if defined(_WIN32)
#    define WIN32_LEAN_AND_MEAN
#    include <winsock2.h>
#    include <ws2tcpip.h>
#    include <wincrypt.h>
#    pragma comment(lib, "ws2_32.lib")
#    pragma comment(lib, "crypt32.lib")
#else
#    include <arpa/inet.h>
#    include <cerrno>
#    include <fcntl.h>
#    include <netdb.h>
#    include <netinet/in.h>
#    include <netinet/tcp.h>
#    include <poll.h>
#    include <sys/socket.h>
#    include <unistd.h>
#endif

#include <mbedtls/ctr_drbg.h>
#include <mbedtls/entropy.h>
#include <mbedtls/error.h>
#include <mbedtls/net_sockets.h>
#include <mbedtls/ssl.h>
#include <mbedtls/x509_crt.h>
#include <psa/crypto.h>

#include "agr/core/Log.hpp"

namespace agr::net {

namespace {

using Clock = std::chrono::steady_clock;

#if defined(_WIN32)
using SocketHandle = SOCKET;
constexpr SocketHandle kNoSocket = INVALID_SOCKET;
void closeSock(SocketHandle s) { closesocket(s); }
int lastError() { return WSAGetLastError(); }
bool interrupted(int e) { return e == WSAEWOULDBLOCK || e == WSAETIMEDOUT || e == WSAEINTR; }
struct WinsockInit {
    WinsockInit() {
        WSADATA d;
        WSAStartup(MAKEWORD(2, 2), &d);
    }
};
void ensureSockets() { static WinsockInit init; }
#else
using SocketHandle = int;
constexpr SocketHandle kNoSocket = -1;
void closeSock(SocketHandle s) { ::close(s); }
int lastError() { return errno; }
bool interrupted(int e) { return e == EAGAIN || e == EWOULDBLOCK || e == EINTR; }
void ensureSockets() {}
#endif

std::string lower(std::string_view s) {
    std::string out(s);
    for (char& c : out) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return out;
}
bool iequals(std::string_view a, std::string_view b) { return lower(a) == lower(b); }

/// Deadline-aware TCP connection (blocking socket with short receive timeouts so that the
/// cancel flag and the deadline are checked regularly).
struct Tcp {
    SocketHandle s = kNoSocket;
    ~Tcp() {
        if (s != kNoSocket) closeSock(s);
    }
};

std::string connectTcp(Tcp& tcp, const std::string& host, u16 port, Clock::time_point deadline, const std::atomic<bool>* cancel) {
    ensureSockets();
    addrinfo hints{};
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    addrinfo* res = nullptr;
    const std::string service = std::to_string(port);
    if (getaddrinfo(host.c_str(), service.c_str(), &hints, &res) != 0 || !res) return "unknown host '" + host + "'";
    std::string error = "cannot connect to " + host + ":" + service;
    for (addrinfo* r = res; r; r = r->ai_next) {
        SocketHandle s = ::socket(r->ai_family, r->ai_socktype, r->ai_protocol);
        if (s == kNoSocket) continue;
#if defined(_WIN32)
        u_long nb = 1;
        ioctlsocket(s, FIONBIO, &nb);
#else
        fcntl(s, F_SETFL, fcntl(s, F_GETFL, 0) | O_NONBLOCK);
#endif
        ::connect(s, r->ai_addr, static_cast<int>(r->ai_addrlen));
        bool connected = false;
        while (Clock::now() < deadline && !(cancel && cancel->load())) {
#if defined(_WIN32)
            fd_set w, e;
            FD_ZERO(&w);
            FD_ZERO(&e);
            FD_SET(s, &w);
            FD_SET(s, &e);
            timeval tv{0, 100000};
            const int n = select(0, nullptr, &w, &e, &tv);
            if (n > 0) {
                connected = FD_ISSET(s, &w) && !FD_ISSET(s, &e);
                break;
            }
#else
            pollfd p{s, POLLOUT, 0};
            const int n = ::poll(&p, 1, 100);
            if (n > 0) {
                int err = 0;
                socklen_t len = sizeof err;
                getsockopt(s, SOL_SOCKET, SO_ERROR, &err, &len);
                connected = err == 0;
                break;
            }
#endif
            if (n < 0) break;
        }
        if (!connected) {
            closeSock(s);
            continue;
        }
#if defined(_WIN32)
        u_long block = 0;
        ioctlsocket(s, FIONBIO, &block);
        DWORD tv = 200;
        setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, reinterpret_cast<const char*>(&tv), sizeof tv);
#else
        fcntl(s, F_SETFL, fcntl(s, F_GETFL, 0) & ~O_NONBLOCK);
        timeval tv{0, 200000};
        setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof tv);
#    if defined(SO_NOSIGPIPE)
        int one = 1;
        setsockopt(s, SOL_SOCKET, SO_NOSIGPIPE, &one, sizeof one);
#    endif
#endif
        int yes = 1;
        setsockopt(s, IPPROTO_TCP, TCP_NODELAY, reinterpret_cast<const char*>(&yes), sizeof yes);
        tcp.s = s;
        error.clear();
        break;
    }
    freeaddrinfo(res);
    if (!error.empty() && cancel && cancel->load()) return "cancelled";
    if (!error.empty() && Clock::now() >= deadline) return "timed out connecting to " + host;
    return error;
}

// ------------------------------------------------------------------ TLS
struct TlsGlobal {
    mbedtls_entropy_context entropy;
    mbedtls_ctr_drbg_context drbg;
    mbedtls_x509_crt system;  // system authorities
    bool ready = false;
    usize systemCount = 0;
    std::mutex mutex;  // the DRBG is shared by the workers
    TlsGlobal() {
        psa_crypto_init();
        mbedtls_entropy_init(&entropy);
        mbedtls_ctr_drbg_init(&drbg);
        mbedtls_x509_crt_init(&system);
        const char* pers = "agr-http";
        ready = mbedtls_ctr_drbg_seed(&drbg, mbedtls_entropy_func, &entropy, reinterpret_cast<const unsigned char*>(pers), std::strlen(pers)) == 0;
        loadSystemAuthorities();
    }
    void loadSystemAuthorities() {
        auto file = [&](const char* path) {
            std::error_code ec;
            if (!path || !*path || !std::filesystem::is_regular_file(path, ec)) return false;
            const int bad = mbedtls_x509_crt_parse_file(&system, path);  // >0: some certificates skipped
            return bad >= 0 || system.version != 0;
        };
        if (file(std::getenv("AGR_CA_BUNDLE")) || file(std::getenv("SSL_CERT_FILE"))) return;
#if defined(_WIN32)
        if (HCERTSTORE store = CertOpenSystemStoreA(0, "ROOT")) {
            PCCERT_CONTEXT c = nullptr;
            while ((c = CertEnumCertificatesInStore(store, c)) != nullptr)
                mbedtls_x509_crt_parse_der(&system, c->pbCertEncoded, c->cbCertEncoded);
            CertCloseStore(store, 0);
        }
#elif defined(__ANDROID__)
        mbedtls_x509_crt_parse_path(&system, "/system/etc/security/cacerts");
#else
        for (const char* p : {"/etc/ssl/certs/ca-certificates.crt", "/etc/pki/tls/certs/ca-bundle.crt", "/etc/ssl/cert.pem",
                              "/usr/local/etc/openssl/cert.pem", "/etc/ssl/ca-bundle.pem"})
            if (file(p)) break;
#endif
    }
    static TlsGlobal& get() {
        static TlsGlobal g;
        return g;
    }
};

int drbgRandom(void*, unsigned char* out, size_t len) {
    TlsGlobal& g = TlsGlobal::get();
    std::lock_guard lock(g.mutex);
    return mbedtls_ctr_drbg_random(&g.drbg, out, len);
}

struct Stream {
    Tcp tcp;
    bool tls = false;
    mbedtls_ssl_context ssl;
    mbedtls_ssl_config conf;
    mbedtls_x509_crt extra;
    const std::atomic<bool>* cancel = nullptr;
    Clock::time_point deadline;
    Stream() {
        mbedtls_ssl_init(&ssl);
        mbedtls_ssl_config_init(&conf);
        mbedtls_x509_crt_init(&extra);
    }
    ~Stream() {
        if (tls) mbedtls_ssl_close_notify(&ssl);
        mbedtls_ssl_free(&ssl);
        mbedtls_ssl_config_free(&conf);
        mbedtls_x509_crt_free(&extra);
    }
    bool stopped() const { return (cancel && cancel->load()) || Clock::now() >= deadline; }

    static int bioSend(void* ctx, const unsigned char* buf, size_t len) {
        auto* self = static_cast<Stream*>(ctx);
#if defined(MSG_NOSIGNAL)
        const auto n = ::send(self->tcp.s, reinterpret_cast<const char*>(buf), static_cast<int>(len), MSG_NOSIGNAL);
#else
        const auto n = ::send(self->tcp.s, reinterpret_cast<const char*>(buf), static_cast<int>(len), 0);
#endif
        if (n < 0) return interrupted(lastError()) ? MBEDTLS_ERR_SSL_WANT_WRITE : MBEDTLS_ERR_NET_SEND_FAILED;
        return static_cast<int>(n);
    }
    static int bioRecv(void* ctx, unsigned char* buf, size_t len) {
        auto* self = static_cast<Stream*>(ctx);
        const auto n = ::recv(self->tcp.s, reinterpret_cast<char*>(buf), static_cast<int>(len), 0);
        if (n < 0) return interrupted(lastError()) ? MBEDTLS_ERR_SSL_WANT_READ : MBEDTLS_ERR_NET_RECV_FAILED;
        return static_cast<int>(n);
    }

    std::string startTls(const std::string& host, bool verify, const std::string& extraPem) {
        TlsGlobal& g = TlsGlobal::get();
        if (!g.ready) return "TLS: no random source";
        if (mbedtls_ssl_config_defaults(&conf, MBEDTLS_SSL_IS_CLIENT, MBEDTLS_SSL_TRANSPORT_STREAM, MBEDTLS_SSL_PRESET_DEFAULT) != 0)
            return "TLS: configuration failed";
        mbedtls_ssl_conf_rng(&conf, drbgRandom, nullptr);
        mbedtls_x509_crt* chain = &g.system;
        if (!extraPem.empty()) {
            if (mbedtls_x509_crt_parse(&extra, reinterpret_cast<const unsigned char*>(extraPem.c_str()), extraPem.size() + 1) != 0)
                return "TLS: invalid extra certificate";
            // Extra first, then the system list (a copy of the pointers is not possible: chain them).
            for (mbedtls_x509_crt* c = &g.system; c && c->version != 0; c = c->next) {
                mbedtls_x509_crt_parse_der(&extra, c->raw.p, c->raw.len);
                if (!c->next) break;
            }
            chain = &extra;
        }
        mbedtls_ssl_conf_ca_chain(&conf, chain, nullptr);
        mbedtls_ssl_conf_authmode(&conf, verify ? MBEDTLS_SSL_VERIFY_REQUIRED : MBEDTLS_SSL_VERIFY_NONE);
        if (mbedtls_ssl_setup(&ssl, &conf) != 0) return "TLS: setup failed";
        mbedtls_ssl_set_hostname(&ssl, host.c_str());
        mbedtls_ssl_set_bio(&ssl, this, bioSend, bioRecv, nullptr);
        for (;;) {
            const int r = mbedtls_ssl_handshake(&ssl);
            if (r == 0) break;
            if (r == MBEDTLS_ERR_SSL_WANT_READ || r == MBEDTLS_ERR_SSL_WANT_WRITE) {
                if (stopped()) return cancel && cancel->load() ? "cancelled" : "timed out (TLS handshake)";
                continue;
            }
            const u32 flags = mbedtls_ssl_get_verify_result(&ssl);
            if (flags != 0 && flags != static_cast<u32>(-1)) {
                char info[256];
                mbedtls_x509_crt_verify_info(info, sizeof info, "", flags);
                std::string why(info);
                while (!why.empty() && (why.back() == '\n' || why.back() == ' ')) why.pop_back();
                return "TLS: certificate of '" + host + "' not trusted (" + why + ")";
            }
            char buf[128];
            mbedtls_strerror(r, buf, sizeof buf);
            return std::string("TLS handshake failed: ") + buf;
        }
        tls = true;
        return {};
    }

    std::string writeAll(const std::string& data) {
        usize done = 0;
        while (done < data.size()) {
            if (stopped()) return cancel && cancel->load() ? "cancelled" : "timed out sending";
            int n;
            if (tls) {
                n = mbedtls_ssl_write(&ssl, reinterpret_cast<const unsigned char*>(data.data() + done), data.size() - done);
                if (n == MBEDTLS_ERR_SSL_WANT_READ || n == MBEDTLS_ERR_SSL_WANT_WRITE) continue;
            } else {
                n = bioSend(this, reinterpret_cast<const unsigned char*>(data.data() + done), data.size() - done);
                if (n == MBEDTLS_ERR_SSL_WANT_WRITE) continue;
            }
            if (n <= 0) return "connection lost while sending";
            done += static_cast<usize>(n);
        }
        return {};
    }

    /// Reads some bytes: >0 count, 0 end of stream, <0 error (message in `error`).
    int readSome(char* out, usize len, std::string& error) {
        for (;;) {
            if (stopped()) {
                error = cancel && cancel->load() ? "cancelled" : "timed out";
                return -1;
            }
            int n;
            if (tls) {
                n = mbedtls_ssl_read(&ssl, reinterpret_cast<unsigned char*>(out), len);
                if (n == MBEDTLS_ERR_SSL_WANT_READ || n == MBEDTLS_ERR_SSL_WANT_WRITE) continue;
                if (n == MBEDTLS_ERR_SSL_RECEIVED_NEW_SESSION_TICKET) continue;  // TLS 1.3
                if (n == MBEDTLS_ERR_SSL_PEER_CLOSE_NOTIFY || n == 0) return 0;
            } else {
                n = bioRecv(this, reinterpret_cast<unsigned char*>(out), len);
                if (n == MBEDTLS_ERR_SSL_WANT_READ) continue;
                if (n == 0) return 0;
            }
            if (n < 0) {
                error = "connection lost while receiving";
                return -1;
            }
            return n;
        }
    }
};

/// Buffered reader over a Stream.
struct Reader {
    Stream& s;
    std::string buf;
    usize pos = 0;
    bool eof = false;
    std::string error;
    bool fill() {
        if (eof) return false;
        if (pos > 0 && pos == buf.size()) buf.clear(), pos = 0;
        char tmp[16384];
        const int n = s.readSome(tmp, sizeof tmp, error);
        if (n <= 0) {
            eof = true;
            return false;
        }
        buf.append(tmp, static_cast<usize>(n));
        return true;
    }
    bool line(std::string& out) {
        for (;;) {
            const auto nl = buf.find('\n', pos);
            if (nl != std::string::npos) {
                out = buf.substr(pos, nl - pos);
                if (!out.empty() && out.back() == '\r') out.pop_back();
                pos = nl + 1;
                return true;
            }
            if (buf.size() - pos > 65536) {
                error = "header line too long";
                return false;
            }
            if (!fill()) return false;
        }
    }
    /// Up to `max` bytes (at least 1 unless the stream ended).
    std::string_view chunk(usize max) {
        if (pos == buf.size() && !fill()) return {};
        const usize n = std::min(max, buf.size() - pos);
        std::string_view v(buf.data() + pos, n);
        pos += n;
        return v;
    }
};

bool noProxyFor(const std::string& host) {
    const char* np = std::getenv("NO_PROXY");
    if (!np) np = std::getenv("no_proxy");
    if (!np) return host == "localhost" || host == "127.0.0.1" || host == "::1";
    std::string list = np;
    usize start = 0;
    while (start <= list.size()) {
        usize end = list.find(',', start);
        if (end == std::string::npos) end = list.size();
        std::string item = lower(list.substr(start, end - start));
        item.erase(0, item.find_first_not_of(' '));
        while (!item.empty() && item.back() == ' ') item.pop_back();
        if (item == "*") return true;
        if (!item.empty() && item[0] == '.') item.erase(0, 1);
        if (item.starts_with("*.")) item.erase(0, 2);
        const std::string h = lower(host);
        if (!item.empty() && (h == item || (h.size() > item.size() && h.ends_with("." + item)))) return true;
        // a.b.c.d/nn ranges: only the exact common ones (loopback / private) are handled by prefix
        if (item.find('/') != std::string::npos) {
            const std::string prefix = item.substr(0, item.find('/'));
            if (prefix == "127.0.0.0" && h.starts_with("127.")) return true;
            if (prefix == "10.0.0.0" && h.starts_with("10.")) return true;
            if (prefix == "192.168.0.0" && h.starts_with("192.168.")) return true;
        }
        start = end + 1;
    }
    return false;
}

std::optional<Url> proxyFor(const Url& url) {
    if (noProxyFor(url.host)) return std::nullopt;
    const char* names[] = {url.scheme == "https" ? "HTTPS_PROXY" : "HTTP_PROXY", url.scheme == "https" ? "https_proxy" : "http_proxy", "ALL_PROXY", "all_proxy"};
    for (const char* n : names)
        if (const char* v = std::getenv(n); v && *v) {
            std::string text = v;
            if (text.find("://") == std::string::npos) text = "http://" + text;
            if (auto p = Url::parse(text)) return p.value();
        }
    return std::nullopt;
}

struct Parsed {
    int status = 0;
    HttpHeaders headers;
};

std::string readHead(Reader& r, Parsed& out) {
    std::string line;
    if (!r.line(line)) return r.error.empty() ? "connection closed before the response" : r.error;
    // "HTTP/1.1 200 OK"
    if (!line.starts_with("HTTP/")) return "not an HTTP response";
    const auto sp = line.find(' ');
    if (sp == std::string::npos) return "bad status line";
    out.status = std::atoi(line.c_str() + sp + 1);
    for (;;) {
        if (!r.line(line)) return r.error.empty() ? "connection closed in the headers" : r.error;
        if (line.empty()) break;
        const auto colon = line.find(':');
        if (colon == std::string::npos) continue;
        std::string value = line.substr(colon + 1);
        value.erase(0, value.find_first_not_of(" \t"));
        while (!value.empty() && (value.back() == ' ' || value.back() == '\t')) value.pop_back();
        out.headers.emplace_back(line.substr(0, colon), std::move(value));
    }
    return {};
}

std::string headerOf(const HttpHeaders& h, std::string_view name) {
    for (const auto& [k, v] : h)
        if (iequals(k, name)) return v;
    return {};
}

/// One request / response exchange (no redirect handling).
HttpResponse exchange(const HttpRequest& req, const Url& url, Clock::time_point deadline, const std::atomic<bool>* cancel,
                      const std::function<void(u64, u64)>& progress) {
    HttpResponse resp;
    resp.finalUrl = req.url;
    Stream s;
    s.cancel = cancel;
    s.deadline = deadline;
    const std::optional<Url> proxy = proxyFor(url);
    const bool viaProxy = proxy.has_value();
    if (std::string e = connectTcp(s.tcp, viaProxy ? proxy->host : url.host, viaProxy ? proxy->port : url.port, deadline, cancel); !e.empty()) {
        resp.error = viaProxy ? "proxy: " + e : e;
        return resp;
    }
    if (viaProxy && url.scheme == "https") {  // tunnel: CONNECT host:443
        const std::string target = (url.host.find(':') != std::string::npos ? "[" + url.host + "]" : url.host) + ":" + std::to_string(url.port);
        if (std::string e = s.writeAll("CONNECT " + target + " HTTP/1.1\r\nHost: " + target + "\r\n\r\n"); !e.empty()) {
            resp.error = "proxy: " + e;
            return resp;
        }
        Reader r{s};
        Parsed head;
        if (std::string e = readHead(r, head); !e.empty()) {
            resp.error = "proxy: " + e;
            return resp;
        }
        if (head.status != 200) {
            resp.error = std::format("proxy refused the connection to {} (HTTP {})", target, head.status);
            return resp;
        }
    }
    if (url.scheme == "https")
        if (std::string e = s.startTls(url.host, req.verifyCertificates, req.extraCaPem); !e.empty()) {
            resp.error = e;
            return resp;
        }

    std::string head = req.method + " " + (viaProxy && url.scheme == "http" ? req.url : url.target) + " HTTP/1.1\r\n";
    bool hasHost = false, hasAgent = false, hasLength = false;
    for (const auto& [k, v] : req.headers) {
        hasHost |= iequals(k, "host"), hasAgent |= iequals(k, "user-agent"), hasLength |= iequals(k, "content-length");
        head += k + ": " + v + "\r\n";
    }
    if (!hasHost) head += "Host: " + url.hostHeader() + "\r\n";
    if (!hasAgent) head += "User-Agent: AGR-Engine\r\n";
    if (!hasLength && (!req.body.empty() || req.method == "POST" || req.method == "PUT" || req.method == "PATCH"))
        head += "Content-Length: " + std::to_string(req.body.size()) + "\r\n";
    head += "Accept-Encoding: identity\r\nConnection: close\r\n\r\n";
    if (std::string e = s.writeAll(head + req.body); !e.empty()) {
        resp.error = e;
        return resp;
    }

    Reader r{s};
    Parsed parsed;
    if (std::string e = readHead(r, parsed); !e.empty()) {
        resp.error = e;
        return resp;
    }
    while (parsed.status == 100) {  // 100 Continue
        parsed = {};
        if (std::string e = readHead(r, parsed); !e.empty()) {
            resp.error = e;
            return resp;
        }
    }
    resp.status = parsed.status;
    resp.headers = std::move(parsed.headers);

    FILE* file = nullptr;
    if (!req.downloadPath.empty() && resp.status >= 200 && resp.status < 300) {
        std::error_code ec;
        std::filesystem::create_directories(std::filesystem::path(req.downloadPath).parent_path(), ec);
        file = std::fopen((req.downloadPath + ".part").c_str(), "wb");
        if (!file) {
            resp.error = "cannot write " + req.downloadPath;
            return resp;
        }
    }
    auto sink = [&](std::string_view data) {
        resp.bytesReceived += data.size();
        if (file) return std::fwrite(data.data(), 1, data.size(), file) == data.size();
        if (resp.body.size() + data.size() > req.maxBodyBytes) return false;
        resp.body.append(data);
        return true;
    };
    const bool noBody = req.method == "HEAD" || resp.status == 204 || resp.status == 304;
    const std::string te = lower(headerOf(resp.headers, "transfer-encoding"));
    const std::string cl = headerOf(resp.headers, "content-length");
    const u64 total = cl.empty() ? 0 : std::strtoull(cl.c_str(), nullptr, 10);
    bool ok = true;
    if (noBody) {
    } else if (te.find("chunked") != std::string::npos) {
        for (;;) {
            std::string line;
            if (!r.line(line)) {
                ok = false;
                break;
            }
            const u64 size = std::strtoull(line.c_str(), nullptr, 16);
            if (size == 0) {
                while (r.line(line) && !line.empty()) {}  // trailers
                break;
            }
            u64 left = size;
            while (left > 0) {
                const std::string_view part = r.chunk(static_cast<usize>(std::min<u64>(left, 1 << 20)));
                if (part.empty() || !sink(part)) {
                    ok = false;
                    break;
                }
                left -= part.size();
                if (progress) progress(resp.bytesReceived, 0);
            }
            if (!ok) break;
            r.line(line);  // CRLF after the chunk
        }
    } else if (!cl.empty()) {
        u64 left = total;
        while (left > 0) {
            const std::string_view part = r.chunk(static_cast<usize>(std::min<u64>(left, 1 << 20)));
            if (part.empty() || !sink(part)) {
                ok = false;
                break;
            }
            left -= part.size();
            if (progress) progress(resp.bytesReceived, total);
        }
    } else {  // until the server closes
        for (;;) {
            const std::string_view part = r.chunk(1 << 20);
            if (part.empty()) break;
            if (!sink(part)) {
                ok = false;
                break;
            }
            if (progress) progress(resp.bytesReceived, 0);
        }
        if (!r.error.empty() && r.error != "connection lost while receiving") ok = false;
    }
    if (file) {
        ok = std::fclose(file) == 0 && ok;
        std::error_code ec;
        if (ok) std::filesystem::rename(req.downloadPath + ".part", req.downloadPath, ec);
        else std::filesystem::remove(req.downloadPath + ".part", ec);
        if (ec) ok = false;
    }
    if (!ok) resp.error = !r.error.empty() ? r.error : resp.body.size() >= req.maxBodyBytes ? "response too large" : "incomplete response";
    return resp;
}

} // namespace

// ------------------------------------------------------------------ Url
Result<Url> Url::parse(std::string_view text) {
    Url u;
    const auto sep = text.find("://");
    if (sep == std::string_view::npos) return makeError(Error::Code::InvalidArgument, "URL without scheme: '" + std::string(text) + "'");
    u.scheme = lower(text.substr(0, sep));
    if (u.scheme != "http" && u.scheme != "https") return makeError(Error::Code::Unsupported, "only http:// and https:// URLs are supported");
    std::string_view rest = text.substr(sep + 3);
    const auto slash = rest.find_first_of("/?#");
    std::string_view authority = rest.substr(0, slash);
    u.target = slash == std::string_view::npos ? "/" : std::string(rest.substr(slash));
    if (const auto hash = u.target.find('#'); hash != std::string::npos) u.target.erase(hash);
    if (u.target.empty() || u.target[0] == '?') u.target = "/" + u.target;
    if (const auto at = authority.rfind('@'); at != std::string_view::npos) authority = authority.substr(at + 1);  // user:pass@ ignored
    u.port = u.scheme == "https" ? 443 : 80;
    if (!authority.empty() && authority[0] == '[') {
        const auto close = authority.find(']');
        if (close == std::string_view::npos) return makeError(Error::Code::InvalidArgument, "bad IPv6 address in URL");
        u.host = std::string(authority.substr(1, close - 1));
        if (close + 1 < authority.size() && authority[close + 1] == ':') u.port = static_cast<u16>(std::atoi(std::string(authority.substr(close + 2)).c_str()));
    } else {
        const auto colon = authority.rfind(':');
        u.host = std::string(authority.substr(0, colon));
        if (colon != std::string_view::npos) u.port = static_cast<u16>(std::atoi(std::string(authority.substr(colon + 1)).c_str()));
    }
    if (u.host.empty() || u.port == 0) return makeError(Error::Code::InvalidArgument, "bad URL: '" + std::string(text) + "'");
    return u;
}

std::string Url::hostHeader() const {
    const std::string h = host.find(':') != std::string::npos ? "[" + host + "]" : host;
    const bool defaultPort = (scheme == "https" && port == 443) || (scheme == "http" && port == 80);
    return defaultPort ? h : h + ":" + std::to_string(port);
}

std::string HttpResponse::header(std::string_view name) const { return headerOf(headers, name); }

// ------------------------------------------------------------------ requests
HttpResponse httpRequest(const HttpRequest& request, const std::atomic<bool>* cancel, const std::function<void(u64, u64)>& progress) {
    const auto deadline = Clock::now() + std::chrono::milliseconds(static_cast<i64>(std::max(0.1, request.timeoutSeconds) * 1000.0));
    HttpRequest req = request;
    for (u32 hop = 0;; ++hop) {
        auto url = Url::parse(req.url);
        if (!url) {
            HttpResponse r;
            r.error = url.error().message;
            return r;
        }
        HttpResponse resp = exchange(req, url.value(), deadline, cancel, progress);
        resp.finalUrl = req.url;
        const bool redirect = resp.error.empty() && (resp.status == 301 || resp.status == 302 || resp.status == 303 || resp.status == 307 || resp.status == 308);
        if (!redirect || hop >= req.maxRedirects) {
            if (redirect) resp.error = "too many redirects";
            return resp;
        }
        std::string location = resp.header("location");
        if (location.empty()) return resp;
        if (location.find("://") == std::string::npos) {  // relative
            const Url& u = url.value();
            const std::string origin = u.scheme + "://" + u.hostHeader();
            if (location[0] == '/') location = origin + location;
            else location = origin + u.target.substr(0, u.target.rfind('/') + 1) + location;
        }
        if (resp.status == 303 || ((resp.status == 301 || resp.status == 302) && req.method == "POST")) {
            req.method = "GET";
            req.body.clear();
        }
        req.url = location;
    }
}

HttpClient::~HttpClient() {
    std::lock_guard lock(m_mutex);
    for (auto& [id, job] : m_jobs) job->cancel = true;
    for (auto& [id, job] : m_jobs)
        if (job->thread.joinable()) job->thread.join();
}

HttpClient::RequestId HttpClient::send(HttpRequest request) {
    auto job = std::make_unique<Job>();
    Job* j = job.get();
    std::lock_guard lock(m_mutex);
    const RequestId id = m_next++;
    j->thread = std::thread([j, req = std::move(request)] {
        j->response = httpRequest(req, &j->cancel, [j](u64 got, u64 total) {
            j->received = got;
            j->total = total;
        });
        j->done = true;
    });
    m_jobs.emplace(id, std::move(job));
    m_order.push_back(id);
    return id;
}

bool HttpClient::poll(RequestId id, HttpResponse& out) {
    std::unique_ptr<Job> job;
    {
        std::lock_guard lock(m_mutex);
        auto it = m_jobs.find(id);
        if (it == m_jobs.end() || !it->second->done) return false;
        job = std::move(it->second);
        m_jobs.erase(it);
        m_order.erase(std::remove(m_order.begin(), m_order.end(), id), m_order.end());
    }
    job->thread.join();
    out = std::move(job->response);
    return true;
}

std::vector<std::pair<HttpClient::RequestId, HttpResponse>> HttpClient::takeFinished() {
    std::vector<RequestId> ready;
    {
        std::lock_guard lock(m_mutex);
        for (RequestId id : m_order)
            if (m_jobs.at(id)->done) ready.push_back(id);
    }
    std::vector<std::pair<RequestId, HttpResponse>> out;
    for (RequestId id : ready) {
        HttpResponse r;
        if (poll(id, r)) out.emplace_back(id, std::move(r));
    }
    return out;
}

void HttpClient::cancel(RequestId id) {
    std::lock_guard lock(m_mutex);
    if (auto it = m_jobs.find(id); it != m_jobs.end()) it->second->cancel = true;
}

std::pair<u64, u64> HttpClient::progress(RequestId id) const {
    std::lock_guard lock(m_mutex);
    auto it = m_jobs.find(id);
    if (it == m_jobs.end()) return {0, 0};
    return {it->second->received.load(), it->second->total.load()};
}

usize HttpClient::pending() const {
    std::lock_guard lock(m_mutex);
    return m_jobs.size();
}

} // namespace agr::net
