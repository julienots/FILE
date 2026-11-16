// AGR Engine — 1.13.0 minimal UDP networking for local (Wi-Fi) multiplayer: non-blocking datagram socket,
// broadcast for discovery on the local network, IPv4 helpers. POSIX sockets: Linux desktop and Android alike
// (the APK declares android.permission.INTERNET).
#pragma once
#include <cstdint>
#include <string>
#include <vector>

namespace agr {

struct NetAddr {
    uint32_t ip = 0;     // host byte order
    uint16_t port = 0;
    bool valid() const { return ip != 0 && port != 0; }
    bool operator==(const NetAddr& o) const { return ip == o.ip && port == o.port; }
    bool operator!=(const NetAddr& o) const { return !(*this == o); }
    std::string str() const;
    static NetAddr make(const std::string& ip, uint16_t port);
};

class UdpSocket {
public:
    ~UdpSocket() { close(); }
    bool open(uint16_t port);           // 0 = any free port; broadcast enabled
    void close();
    bool isOpen() const { return fd >= 0; }
    bool send(const NetAddr& to, const void* data, int len);
    int recv(void* buf, int cap, NetAddr& from);   // < 0: nothing waiting
    uint16_t localPort() const { return port; }
private:
    int fd = -1;
    uint16_t port = 0;
};

bool parseIPv4(const std::string& s, uint32_t& out);
std::vector<std::string> localIPv4();       // this device's addresses on the local networks (not 127.0.0.1)
std::vector<uint32_t> broadcastAddrs();     // directed broadcast of each interface (+ 255.255.255.255)

// little binary writer / reader for messages
struct ByteWriter {
    std::vector<uint8_t> b;
    void u8(uint8_t v) { b.push_back(v); }
    void u16(uint16_t v) { b.push_back(v & 0xff); b.push_back(v >> 8); }
    void u32(uint32_t v) { for (int i = 0; i < 4; i++) b.push_back((v >> (8 * i)) & 0xff); }
    void i16(int16_t v) { u16((uint16_t)v); }
    void f32(float v) { uint32_t u; static_assert(sizeof u == sizeof v, ""); __builtin_memcpy(&u, &v, 4); u32(u); }
    void str(const std::string& s) { uint16_t n = (uint16_t)std::min<size_t>(s.size(), 4000); u16(n); b.insert(b.end(), s.begin(), s.begin() + n); }
};
struct ByteReader {
    const uint8_t* p; size_t n, i = 0; bool bad = false;
    ByteReader(const uint8_t* d, size_t len) : p(d), n(len) {}
    bool need(size_t k) { if (i + k > n) { bad = true; return false; } return true; }
    uint8_t u8() { return need(1) ? p[i++] : 0; }
    uint16_t u16() { if (!need(2)) return 0; uint16_t v = p[i] | (p[i + 1] << 8); i += 2; return v; }
    uint32_t u32() { if (!need(4)) return 0; uint32_t v = 0; for (int k = 0; k < 4; k++) v |= (uint32_t)p[i + k] << (8 * k); i += 4; return v; }
    int16_t i16() { return (int16_t)u16(); }
    float f32() { uint32_t u = u32(); float v; __builtin_memcpy(&v, &u, 4); return v; }
    std::string str() { uint16_t k = u16(); if (!need(k)) return ""; std::string s((const char*)p + i, k); i += k; return s; }
    bool done() const { return i >= n; }
};

}  // namespace agr
