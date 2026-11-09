#include "agr/core/Uuid.hpp"

#include <charconv>
#include <mutex>
#include <random>

namespace agr {

Uuid Uuid::generate() {
    static std::mutex mutex;
    static std::mt19937_64 engine{std::random_device{}() ^ (static_cast<u64>(std::random_device{}()) << 32)};
    std::lock_guard lock(mutex);
    u64 v = 0;
    while (v == 0) v = engine();
    return Uuid(v);
}

std::string Uuid::toString() const {
    char buf[17] = {};
    static constexpr char digits[] = "0123456789abcdef";
    for (int i = 15; i >= 0; --i) buf[15 - i] = digits[(m_value >> (i * 4)) & 0xF];
    return std::string(buf, 16);
}

Uuid Uuid::fromString(std::string_view s) {
    u64 v = 0;
    auto [ptr, ec] = std::from_chars(s.data(), s.data() + s.size(), v, 16);
    if (ec != std::errc{} || ptr != s.data() + s.size()) return Uuid{};
    return Uuid(v);
}

} // namespace agr
