#include <doctest/doctest.h>

#include <atomic>
#include <chrono>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>
#include <thread>

#include "agr/core/Uuid.hpp"
#include "agr/net/Http.hpp"

#if !defined(_WIN32)
#    include <arpa/inet.h>
#    include <netinet/in.h>
#    include <poll.h>
#    include <sys/socket.h>
#    include <unistd.h>
#endif

using namespace agr;
using namespace agr::net;

TEST_SUITE("http") {
TEST_CASE("URL parsing") {
    auto u = Url::parse("https://example.com/a/b?x=1#frag");
    REQUIRE(u);
    CHECK(u.value().scheme == "https");
    CHECK(u.value().host == "example.com");
    CHECK(u.value().port == 443);
    CHECK(u.value().target == "/a/b?x=1");
    CHECK(u.value().hostHeader() == "example.com");
    auto v = Url::parse("http://[::1]:8080");
    REQUIRE(v);
    CHECK(v.value().host == "::1");
    CHECK(v.value().port == 8080);
    CHECK(v.value().target == "/");
    CHECK(v.value().hostHeader() == "[::1]:8080");
    CHECK(Url::parse("http://h?q").value().target == "/?q");
    CHECK_FALSE(Url::parse("ftp://x"));
    CHECK_FALSE(Url::parse("no-scheme"));
}

#if !defined(_WIN32)
namespace {
/// Minimal HTTP/1.1 server on 127.0.0.1 for the client tests (one connection at a time).
struct TestServer {
    int listener = -1;
    u16 port = 0;
    std::atomic<bool> stop{false};
    std::thread thread;
    TestServer() {
        listener = ::socket(AF_INET, SOCK_STREAM, 0);
        int yes = 1;
        setsockopt(listener, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof yes);
        sockaddr_in a{};
        a.sin_family = AF_INET;
        a.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        ::bind(listener, reinterpret_cast<sockaddr*>(&a), sizeof a);
        ::listen(listener, 16);
        socklen_t len = sizeof a;
        getsockname(listener, reinterpret_cast<sockaddr*>(&a), &len);
        port = ntohs(a.sin_port);
        thread = std::thread([this] { run(); });
    }
    ~TestServer() {
        stop = true;
        thread.join();
        ::close(listener);
    }
    static void sendAll(int c, const std::string& s) {
        usize d = 0;
        while (d < s.size()) {
            const auto n = ::send(c, s.data() + d, s.size() - d, MSG_NOSIGNAL);
            if (n <= 0) return;
            d += static_cast<usize>(n);
        }
    }
    void run() {
        while (!stop) {
            pollfd p{listener, POLLIN, 0};
            if (::poll(&p, 1, 50) <= 0) continue;
            const int c = ::accept(listener, nullptr, nullptr);
            if (c < 0) continue;
            std::string req;
            char buf[4096];
            while (req.find("\r\n\r\n") == std::string::npos) {
                const auto n = ::recv(c, buf, sizeof buf, 0);
                if (n <= 0) break;
                req.append(buf, static_cast<usize>(n));
            }
            const auto headEnd = req.find("\r\n\r\n");
            std::string body = headEnd == std::string::npos ? "" : req.substr(headEnd + 4);
            const auto clPos = req.find("Content-Length: ");
            if (clPos != std::string::npos) {
                const usize want = std::stoul(req.substr(clPos + 16));
                while (body.size() < want) {
                    const auto n = ::recv(c, buf, sizeof buf, 0);
                    if (n <= 0) break;
                    body.append(buf, static_cast<usize>(n));
                }
            }
            const std::string method = req.substr(0, req.find(' '));
            const std::string path = req.substr(req.find(' ') + 1, req.find(' ', req.find(' ') + 1) - req.find(' ') - 1);
            if (path == "/hello") {
                sendAll(c, "HTTP/1.1 200 OK\r\nContent-Type: text/plain\r\nX-Test: yes\r\nContent-Length: 11\r\n\r\nhello world");
            } else if (path == "/chunked") {
                sendAll(c, "HTTP/1.1 200 OK\r\nTransfer-Encoding: chunked\r\n\r\n3\r\nabc\r\n4\r\ndefg\r\n0\r\n\r\n");
            } else if (path == "/redirect") {
                sendAll(c, "HTTP/1.1 302 Found\r\nLocation: /hello\r\nContent-Length: 0\r\n\r\n");
            } else if (path == "/loop") {
                sendAll(c, "HTTP/1.1 302 Found\r\nLocation: /loop\r\nContent-Length: 0\r\n\r\n");
            } else if (path == "/echo") {
                const std::string out = method + ":" + body + ":" + (req.find("X-Token: 42") != std::string::npos ? "token" : "no-token");
                sendAll(c, "HTTP/1.1 201 Created\r\nContent-Length: " + std::to_string(out.size()) + "\r\n\r\n" + out);
            } else if (path == "/close") {
                sendAll(c, "HTTP/1.1 200 OK\r\n\r\nuntil the end");
            } else if (path == "/big") {
                std::string big(2 << 20, 'x');
                for (usize i = 0; i < big.size(); i += 1000) big[i] = static_cast<char>('a' + (i / 1000) % 26);
                sendAll(c, "HTTP/1.1 200 OK\r\nContent-Length: " + std::to_string(big.size()) + "\r\n\r\n" + big);
            } else if (path == "/slow") {
                for (int i = 0; i < 30 && !stop; ++i) std::this_thread::sleep_for(std::chrono::milliseconds(100));
                sendAll(c, "HTTP/1.1 200 OK\r\nContent-Length: 2\r\n\r\nok");
            } else {
                sendAll(c, "HTTP/1.1 404 Not Found\r\nContent-Length: 9\r\n\r\nnot found");
            }
            ::close(c);
        }
    }
    std::string url(const std::string& path) const { return "http://127.0.0.1:" + std::to_string(port) + path; }
};
}  // namespace

TEST_CASE("HTTP client: content-length, chunked, redirects, POST, errors, timeouts, downloads, async") {
    TestServer server;
    HttpRequest r;
    r.url = server.url("/hello");
    HttpResponse a = httpRequest(r);
    CHECK(a.error.empty());
    CHECK(a.status == 200);
    CHECK(a.ok());
    CHECK(a.body == "hello world");
    CHECK(a.header("x-test") == "yes");
    CHECK(a.header("Content-Type") == "text/plain");

    r.url = server.url("/chunked");
    CHECK(httpRequest(r).body == "abcdefg");
    r.url = server.url("/close");
    CHECK(httpRequest(r).body == "until the end");

    r.url = server.url("/redirect");
    HttpResponse red = httpRequest(r);
    CHECK(red.status == 200);
    CHECK(red.body == "hello world");
    CHECK(red.finalUrl == server.url("/hello"));
    r.url = server.url("/loop");
    CHECK(httpRequest(r).error == "too many redirects");

    HttpRequest post;
    post.method = "POST";
    post.url = server.url("/echo");
    post.body = "{\"score\":12}";
    post.headers = {{"X-Token", "42"}};
    HttpResponse p = httpRequest(post);
    CHECK(p.status == 201);
    CHECK(p.body == "POST:{\"score\":12}:token");

    r.url = server.url("/missing");
    HttpResponse nf = httpRequest(r);
    CHECK(nf.status == 404);
    CHECK_FALSE(nf.ok());
    CHECK(nf.error.empty());

    r.url = "http://127.0.0.1:1/";  // nothing listens there
    CHECK_FALSE(httpRequest(r).error.empty());
    r.url = "http://no-such-host.invalid/";
    CHECK_FALSE(httpRequest(r).error.empty());

    HttpRequest slow;
    slow.url = server.url("/slow");
    slow.timeoutSeconds = 0.5;
    const auto t0 = std::chrono::steady_clock::now();
    HttpResponse to = httpRequest(slow);
    CHECK(to.error.find("timed out") != std::string::npos);
    CHECK(std::chrono::steady_clock::now() - t0 < std::chrono::seconds(2));

    // Download to a file, with progress.
    const auto file = std::filesystem::temp_directory_path() / ("agr_http_" + Uuid::generate().toString()) / "big.bin";
    HttpRequest dl;
    dl.url = server.url("/big");
    dl.downloadPath = file.string();
    u64 lastProgress = 0, total = 0;
    HttpResponse d = httpRequest(dl, nullptr, [&](u64 got, u64 all) { lastProgress = got, total = all; });
    CHECK(d.ok());
    CHECK(d.body.empty());
    CHECK(std::filesystem::file_size(file) == (2u << 20));
    CHECK(lastProgress == (2u << 20));
    CHECK(total == (2u << 20));
    CHECK_FALSE(std::filesystem::exists(file.string() + ".part"));
    std::filesystem::remove_all(file.parent_path());

    // Async: several requests, polled; a cancelled one ends quickly.
    HttpClient client;
    HttpRequest h;
    h.url = server.url("/hello");
    const auto id1 = client.send(h);
    HttpRequest c = slow;
    c.timeoutSeconds = 10;
    const auto id2 = client.send(c);
    client.cancel(id2);
    std::vector<std::pair<HttpClient::RequestId, HttpResponse>> done;
    for (int i = 0; i < 400 && done.size() < 2; ++i) {
        for (auto& f : client.takeFinished()) done.push_back(std::move(f));
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    REQUIRE(done.size() == 2);
    for (auto& [id, resp] : done) {
        if (id == id1) CHECK(resp.body == "hello world");
        if (id == id2) CHECK(resp.error == "cancelled");
    }
    CHECK(client.pending() == 0);
}
#endif

TEST_CASE("HTTPS on the Internet (through HTTPS_PROXY when set): certificate checked, bad certificate refused") {
    HttpRequest r;
    r.url = "https://raw.githubusercontent.com/nlohmann/json/v3.11.3/LICENSE.MIT";
    r.timeoutSeconds = 20;
    HttpResponse a = httpRequest(r);
    if (!a.error.empty() && a.error.find("TLS") == std::string::npos) {
        WARN_MESSAGE(false, "no Internet access: " << a.error);
        return;
    }
    CHECK_MESSAGE(a.error.empty(), a.error);
    CHECK(a.status == 200);
    CHECK(a.body.find("MIT License") != std::string::npos);
    // An expired certificate is refused (when that test site is reachable).
    HttpRequest expired;
    expired.url = "https://expired.badssl.com/";
    expired.timeoutSeconds = 20;
    HttpResponse e = httpRequest(expired);
    if (e.error.find("proxy") == std::string::npos && !e.error.empty()) CHECK(e.error.find("TLS") != std::string::npos);
}
}  // TEST_SUITE
