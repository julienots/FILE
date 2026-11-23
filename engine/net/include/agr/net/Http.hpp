#pragma once
// HTTP / HTTPS client (HTTP/1.1, TLS 1.2 / 1.3 through mbedTLS): REST APIs, leaderboards,
// downloads of DLC packs, the editor's online package repository. Requests run on worker
// threads (HttpClient) or blocking (httpRequest). Follows redirects, decodes chunked bodies,
// goes through HTTPS_PROXY / HTTP_PROXY when set (NO_PROXY honoured) and checks certificates
// against the system's authorities (SSL_CERT_FILE / AGR_CA_BUNDLE override them).
#include <atomic>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_map>
#include <utility>
#include <vector>

#include "agr/core/Result.hpp"
#include "agr/core/Types.hpp"

namespace agr::net {

struct Url {
    std::string scheme;  ///< "http" or "https"
    std::string host;    ///< Without brackets for IPv6 literals.
    u16 port = 0;
    std::string target = "/";  ///< Path and query.
    static Result<Url> parse(std::string_view text);
    std::string hostHeader() const;  ///< "host" or "host:port" (non-default port).
};

using HttpHeaders = std::vector<std::pair<std::string, std::string>>;

struct HttpRequest {
    std::string method = "GET";
    std::string url;
    HttpHeaders headers;
    std::string body;
    f64 timeoutSeconds = 30.0;  ///< Whole request (connection, sending, receiving).
    u32 maxRedirects = 5;
    u64 maxBodyBytes = 512ull << 20;
    /// Non-empty: the body is written to this file (big downloads) instead of HttpResponse::body.
    std::string downloadPath;
    bool verifyCertificates = true;  ///< Turn off only for local tests with self-signed certificates.
    /// Extra trusted certificates (PEM), e.g. a self-signed server's.
    std::string extraCaPem;
    bool direct = false;  ///< Never through a proxy (devices on the local network: routers, consoles).
};

struct HttpResponse {
    int status = 0;  ///< 0 when the request failed (see error).
    HttpHeaders headers;
    std::string body;
    std::string finalUrl;  ///< After redirects.
    std::string error;     ///< Non-empty on failure (DNS, connection, TLS, timeout, cancelled).
    u64 bytesReceived = 0;
    bool ok() const { return error.empty() && status >= 200 && status < 300; }
    /// First header of that name (case-insensitive), "" if absent.
    std::string header(std::string_view name) const;
};

/// Blocking request. `cancel` (optional) aborts it when it becomes true; `progress` gets
/// (received, total or 0 when unknown).
HttpResponse httpRequest(const HttpRequest& request, const std::atomic<bool>* cancel = nullptr,
                         const std::function<void(u64, u64)>& progress = {});

/// Asynchronous requests on worker threads; poll() from the main thread.
class HttpClient {
public:
    using RequestId = u64;
    HttpClient() = default;
    ~HttpClient();
    HttpClient(const HttpClient&) = delete;
    HttpClient& operator=(const HttpClient&) = delete;

    RequestId send(HttpRequest request);
    /// True once the request is finished (the response is moved out and the id forgotten).
    bool poll(RequestId id, HttpResponse& out);
    /// Finished requests since the last call (id, response), in completion order.
    std::vector<std::pair<RequestId, HttpResponse>> takeFinished();
    void cancel(RequestId id);
    /// Bytes received / expected (0 = unknown) of a running request.
    std::pair<u64, u64> progress(RequestId id) const;
    usize pending() const;

private:
    struct Job {
        std::thread thread;
        std::atomic<bool> cancel{false}, done{false};
        std::atomic<u64> received{0}, total{0};
        HttpResponse response;
    };
    mutable std::mutex m_mutex;
    std::unordered_map<RequestId, std::unique_ptr<Job>> m_jobs;
    std::vector<RequestId> m_order;
    RequestId m_next = 1;
};

struct WebSocketOptions {
    HttpHeaders headers;                 ///< Extra handshake headers (Authorization, Origin...).
    std::vector<std::string> protocols;  ///< Sec-WebSocket-Protocol offers.
    f64 connectTimeoutSeconds = 15.0;
    bool verifyCertificates = true;
    std::string extraCaPem;
    u64 maxMessageBytes = 16ull << 20;
};

/// WebSocket client (RFC 6455) over ws:// or wss:// (same TLS, proxy and certificate rules as
/// HTTPS). It runs on its own thread: send() queues, receive() drains what arrived. Pings are
/// answered, fragmented messages reassembled, the close handshake done.
class WebSocket {
public:
    enum class State : u8 { Connecting, Open, Closing, Closed };
    struct Message {
        bool binary = false;
        std::string data;
    };
    using Options = WebSocketOptions;
    WebSocket() = default;
    ~WebSocket();
    WebSocket(const WebSocket&) = delete;
    WebSocket& operator=(const WebSocket&) = delete;

    /// Starts connecting (State::Connecting, then Open or Closed with error()).
    void connect(std::string url, Options options = {});
    /// Queues a message; false unless connecting or open.
    bool send(std::string_view text);
    bool sendBinary(std::string_view bytes);
    /// Messages received since the last call.
    std::vector<Message> receive();
    /// Starts the close handshake (code 1000 = normal).
    void close(u16 code = 1000, std::string reason = {});
    State state() const { return m_state.load(); }
    std::string error() const;
    u16 closeCode() const { return m_closeCode.load(); }
    std::string protocol() const;  ///< Sub-protocol chosen by the server.

private:
    void run(std::string url, Options options);
    std::thread m_thread;
    std::atomic<State> m_state{State::Closed};
    std::atomic<bool> m_stop{false};
    std::atomic<u16> m_closeCode{0};
    mutable std::mutex m_mutex;
    std::vector<std::pair<u8, std::string>> m_outgoing;  // opcode, payload
    std::vector<Message> m_incoming;
    std::string m_error, m_protocol;
    bool m_closeRequested = false;
    u16 m_closeSendCode = 1000;
    std::string m_closeReason;
};

/// Sec-WebSocket-Accept value for a handshake key (base64 of SHA-1 of key + RFC GUID).
std::string webSocketAccept(std::string_view key);

} // namespace agr::net
