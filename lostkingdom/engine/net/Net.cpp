#include "Net.h"
#include <arpa/inet.h>
#include <fcntl.h>
#include <ifaddrs.h>
#include <net/if.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#include <cerrno>
#include <cstdio>
#include <cstring>

namespace agr {

std::string NetAddr::str() const {
    char b[32];
    snprintf(b, sizeof b, "%u.%u.%u.%u:%u", (ip >> 24) & 255, (ip >> 16) & 255, (ip >> 8) & 255, ip & 255, port);
    return b;
}

NetAddr NetAddr::make(const std::string& ip, uint16_t port) {
    NetAddr a; uint32_t v = 0;
    if (parseIPv4(ip, v)) { a.ip = v; a.port = port; }
    return a;
}

bool parseIPv4(const std::string& s, uint32_t& out) {
    unsigned a, b, c, d; char extra;
    if (sscanf(s.c_str(), "%u.%u.%u.%u%c", &a, &b, &c, &d, &extra) != 4 || a > 255 || b > 255 || c > 255 || d > 255) return false;
    out = (a << 24) | (b << 16) | (c << 8) | d;
    return true;
}

bool UdpSocket::open(uint16_t p) {
    close();
    fd = ::socket(AF_INET, SOCK_DGRAM, 0);
    if (fd < 0) return false;
    int one = 1;
    setsockopt(fd, SOL_SOCKET, SO_BROADCAST, &one, sizeof one);
    setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &one, sizeof one);
    fcntl(fd, F_SETFL, fcntl(fd, F_GETFL, 0) | O_NONBLOCK);
    sockaddr_in a{}; a.sin_family = AF_INET; a.sin_addr.s_addr = htonl(INADDR_ANY); a.sin_port = htons(p);
    if (::bind(fd, (sockaddr*)&a, sizeof a) != 0) { close(); return false; }
    socklen_t len = sizeof a;
    getsockname(fd, (sockaddr*)&a, &len);
    port = ntohs(a.sin_port);
    return true;
}

void UdpSocket::close() {
    if (fd >= 0) ::close(fd);
    fd = -1; port = 0;
}

bool UdpSocket::send(const NetAddr& to, const void* data, int len) {
    if (fd < 0 || !to.valid()) return false;
    sockaddr_in a{}; a.sin_family = AF_INET; a.sin_addr.s_addr = htonl(to.ip); a.sin_port = htons(to.port);
    return ::sendto(fd, data, len, 0, (sockaddr*)&a, sizeof a) == len;
}

int UdpSocket::recv(void* buf, int cap, NetAddr& from) {
    if (fd < 0) return -1;
    sockaddr_in a{}; socklen_t len = sizeof a;
    ssize_t n = ::recvfrom(fd, buf, cap, 0, (sockaddr*)&a, &len);
    if (n < 0) return -1;
    from.ip = ntohl(a.sin_addr.s_addr); from.port = ntohs(a.sin_port);
    return (int)n;
}

std::vector<std::string> localIPv4() {
    std::vector<std::string> out;
    ifaddrs* ifs = nullptr;
    if (getifaddrs(&ifs) != 0) return out;
    for (ifaddrs* i = ifs; i; i = i->ifa_next) {
        if (!i->ifa_addr || i->ifa_addr->sa_family != AF_INET || (i->ifa_flags & IFF_LOOPBACK)) continue;
        char b[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &((sockaddr_in*)i->ifa_addr)->sin_addr, b, sizeof b);
        out.push_back(b);
    }
    freeifaddrs(ifs);
    return out;
}

std::vector<uint32_t> broadcastAddrs() {
    std::vector<uint32_t> out{0xFFFFFFFFu};
    ifaddrs* ifs = nullptr;
    if (getifaddrs(&ifs) != 0) return out;
    for (ifaddrs* i = ifs; i; i = i->ifa_next) {
        if (!i->ifa_addr || i->ifa_addr->sa_family != AF_INET || !(i->ifa_flags & IFF_BROADCAST) || !i->ifa_broadaddr) continue;
        uint32_t b = ntohl(((sockaddr_in*)i->ifa_broadaddr)->sin_addr.s_addr);
        if (b && b != 0xFFFFFFFFu) out.push_back(b);
    }
    freeifaddrs(ifs);
    return out;
}

}  // namespace agr
