#include "agr/core/Compression.hpp"

#include <cstring>

namespace agr::lz {

// Sequence: token (high nibble: literal count, low nibble: match length - 4; 15 = more bytes
// follow, each added until one is < 255), literals, then a 2-byte offset (1..65535). The last
// sequence has literals only (it ends the input).
namespace {
constexpr usize kMinMatch = 4;
constexpr int kHashBits = 12;

u32 read32(const u8* p) {
    u32 v;
    std::memcpy(&v, p, 4);
    return v;
}
u32 hash(u32 v) { return (v * 2654435761u) >> (32 - kHashBits); }
void putLength(std::vector<u8>& out, usize n) {  // the part beyond 15
    while (n >= 255) {
        out.push_back(255);
        n -= 255;
    }
    out.push_back(static_cast<u8>(n));
}
} // namespace

std::vector<u8> compress(std::span<const u8> in) {
    std::vector<u8> out;
    out.reserve(in.size() + in.size() / 64 + 16);
    const usize n = in.size();
    u32 table[1 << kHashBits];
    std::memset(table, 0xFF, sizeof table);
    usize anchor = 0, i = 0;
    auto emit = [&](usize literalEnd, usize matchLen, usize offset) {
        const usize lits = literalEnd - anchor;
        const usize m = matchLen ? matchLen - kMinMatch : 0;
        out.push_back(static_cast<u8>(((lits >= 15 ? 15 : lits) << 4) | (matchLen ? (m >= 15 ? 15 : m) : 0)));
        if (lits >= 15) putLength(out, lits - 15);
        out.insert(out.end(), in.begin() + static_cast<std::ptrdiff_t>(anchor), in.begin() + static_cast<std::ptrdiff_t>(literalEnd));
        if (matchLen == 0) return;  // last sequence: literals only
        out.push_back(static_cast<u8>(offset & 0xFF));
        out.push_back(static_cast<u8>(offset >> 8));
        if (m >= 15) putLength(out, m - 15);
    };
    while (n >= kMinMatch && i + kMinMatch <= n) {
        const u32 v = read32(&in[i]);
        const u32 h = hash(v);
        const u32 candidate = table[h];
        table[h] = static_cast<u32>(i);
        if (candidate != 0xFFFFFFFFu && i - candidate <= 65535 && read32(&in[candidate]) == v) {
            usize len = kMinMatch;
            while (i + len < n && in[candidate + len] == in[i + len]) ++len;
            emit(i, len, i - candidate);
            i += len;
            anchor = i;
            continue;
        }
        ++i;
    }
    emit(n, 0, 0);
    return out;
}

std::optional<std::vector<u8>> decompress(std::span<const u8> in, usize maxSize) {
    std::vector<u8> out;
    usize i = 0;
    auto length = [&](usize base, usize& value) {
        value = base;
        if (base < 15) return true;
        while (true) {
            if (i >= in.size()) return false;
            const u8 b = in[i++];
            value += b;
            if (value > maxSize) return false;
            if (b < 255) return true;
        }
    };
    while (i < in.size()) {
        const u8 token = in[i++];
        usize lits = 0;
        if (!length(token >> 4, lits) || lits > in.size() - i || out.size() + lits > maxSize) return std::nullopt;
        out.insert(out.end(), in.begin() + static_cast<std::ptrdiff_t>(i), in.begin() + static_cast<std::ptrdiff_t>(i + lits));
        i += lits;
        if (i == in.size()) return out;  // last sequence: literals only
        if (in.size() - i < 2) return std::nullopt;
        const usize offset = in[i] | (static_cast<usize>(in[i + 1]) << 8);
        i += 2;
        usize m = 0;
        if (!length(token & 15, m)) return std::nullopt;
        const usize len = m + kMinMatch;
        if (offset == 0 || offset > out.size() || out.size() + len > maxSize) return std::nullopt;
        const usize from = out.size() - offset;
        for (usize k = 0; k < len; ++k) out.push_back(out[from + k]);  // may overlap (runs)
    }
    return out;  // empty input
}

} // namespace agr::lz
