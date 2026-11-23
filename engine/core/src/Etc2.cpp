#include "agr/core/Etc2.hpp"

#include <algorithm>
#include <climits>
#include <cstring>

namespace agr::etc2 {

namespace {

constexpr int kEtc1Tables[8][4] = {{2, 8, -2, -8},     {5, 17, -5, -17},   {9, 29, -9, -29},   {13, 42, -13, -42},
                                   {18, 60, -18, -60}, {24, 80, -24, -80}, {33, 106, -33, -106}, {47, 183, -47, -183}};
constexpr int kEacTables[16][8] = {{-3, -6, -9, -15, 2, 5, 8, 14}, {-3, -7, -10, -13, 2, 6, 9, 12}, {-2, -5, -8, -13, 1, 4, 7, 12},
                                   {-2, -4, -6, -13, 1, 3, 5, 12}, {-3, -6, -8, -12, 2, 5, 7, 11}, {-3, -7, -9, -11, 2, 6, 8, 10},
                                   {-4, -7, -8, -11, 3, 6, 7, 10}, {-3, -5, -8, -11, 2, 4, 7, 10}, {-2, -6, -8, -10, 1, 5, 7, 9},
                                   {-2, -5, -8, -10, 1, 4, 7, 9},  {-2, -4, -8, -10, 1, 3, 7, 9},  {-2, -5, -7, -10, 1, 4, 6, 9},
                                   {-3, -4, -7, -10, 2, 3, 6, 9},  {-1, -2, -3, -10, 0, 1, 2, 9},  {-4, -6, -8, -9, 3, 5, 7, 8},
                                   {-3, -5, -7, -9, 2, 4, 6, 8}};
constexpr int kThDistances[8] = {3, 6, 11, 16, 23, 32, 41, 64};

int clamp255(int v) { return v < 0 ? 0 : v > 255 ? 255 : v; }
int expand4(int v) { return (v << 4) | v; }
int expand5(int v) { return (v << 3) | (v >> 2); }
int expand6(int v) { return (v << 2) | (v >> 4); }
int expand7(int v) { return (v << 1) | (v >> 6); }

// ------------------------------------------------------------------ decode
void decodeColorBlock(const u8* b, u8 px[16][4]) {  // writes rgb of px[i], i = x * 4 + y
    const u32 indices = static_cast<u32>(b[4]) << 24 | static_cast<u32>(b[5]) << 16 | static_cast<u32>(b[6]) << 8 | b[7];
    auto pixelIndex = [&](int i) { return static_cast<int>(((indices >> (16 + i)) & 1) << 1 | ((indices >> i) & 1)); };
    const bool diff = b[3] & 2;
    const bool flip = b[3] & 1;
    if (!diff) {  // individual
        const int c1[3] = {expand4(b[0] >> 4), expand4(b[1] >> 4), expand4(b[2] >> 4)};
        const int c2[3] = {expand4(b[0] & 15), expand4(b[1] & 15), expand4(b[2] & 15)};
        const int t1 = (b[3] >> 5) & 7, t2 = (b[3] >> 2) & 7;
        for (int i = 0; i < 16; ++i) {
            const int x = i / 4, y = i % 4;
            const bool second = flip ? y >= 2 : x >= 2;
            const int* c = second ? c2 : c1;
            const int m = kEtc1Tables[second ? t2 : t1][pixelIndex(i)];
            for (int k = 0; k < 3; ++k) px[i][k] = static_cast<u8>(clamp255(c[k] + m));
        }
        return;
    }
    const int r = b[0] >> 3, g = b[1] >> 3, bl = b[2] >> 3;
    auto signed3 = [](int v) { return v >= 4 ? v - 8 : v; };
    const int r2 = r + signed3(b[0] & 7), g2 = g + signed3(b[1] & 7), b2 = bl + signed3(b[2] & 7);
    if (r2 < 0 || r2 > 31) {  // T mode
        const int c1[3] = {expand4(((b[0] >> 3) & 3) << 2 | (b[0] & 3)), expand4(b[1] >> 4), expand4(b[1] & 15)};
        const int c2[3] = {expand4(b[2] >> 4), expand4(b[2] & 15), expand4(b[3] >> 4)};
        const int d = kThDistances[((b[3] >> 2) & 3) << 1 | (b[3] & 1)];
        int paint[4][3];
        for (int k = 0; k < 3; ++k) {
            paint[0][k] = c1[k];
            paint[1][k] = clamp255(c2[k] + d);
            paint[2][k] = c2[k];
            paint[3][k] = clamp255(c2[k] - d);
        }
        for (int i = 0; i < 16; ++i)
            for (int k = 0; k < 3; ++k) px[i][k] = static_cast<u8>(paint[pixelIndex(i)][k]);
        return;
    }
    if (g2 < 0 || g2 > 31) {  // H mode
        const int r1 = (b[0] >> 3) & 15, g1 = ((b[0] & 7) << 1) | ((b[1] >> 4) & 1);
        const int b1 = (b[1] & 8) | ((b[1] & 3) << 1) | (b[2] >> 7);
        const int rr2 = (b[2] >> 3) & 15, gg2 = ((b[2] & 7) << 1) | (b[3] >> 7), bb2 = (b[3] >> 3) & 15;
        const int v1 = r1 << 8 | g1 << 4 | b1, v2 = rr2 << 8 | gg2 << 4 | bb2;
        const int d = kThDistances[(b[3] & 4) | ((b[3] & 1) << 1) | (v1 >= v2 ? 1 : 0)];
        const int c1[3] = {expand4(r1), expand4(g1), expand4(b1)}, c2[3] = {expand4(rr2), expand4(gg2), expand4(bb2)};
        int paint[4][3];
        for (int k = 0; k < 3; ++k) {
            paint[0][k] = clamp255(c1[k] + d);
            paint[1][k] = clamp255(c1[k] - d);
            paint[2][k] = clamp255(c2[k] + d);
            paint[3][k] = clamp255(c2[k] - d);
        }
        for (int i = 0; i < 16; ++i)
            for (int k = 0; k < 3; ++k) px[i][k] = static_cast<u8>(paint[pixelIndex(i)][k]);
        return;
    }
    if (b2 < 0 || b2 > 31) {  // planar
        u64 v = 0;
        for (int k = 0; k < 8; ++k) v = v << 8 | b[k];
        auto bits = [&](int hi, int n) { return static_cast<int>((v >> (hi - n + 1)) & ((1u << n) - 1)); };
        const int ro = bits(62, 6), go = bits(56, 1) << 6 | bits(54, 6), bo = bits(48, 1) << 5 | bits(44, 2) << 3 | bits(41, 3);
        const int rh = bits(38, 5) << 1 | bits(32, 1), gh = bits(31, 7), bh = bits(24, 6);
        const int rv = bits(18, 6), gv = bits(12, 7), bv = bits(5, 6);
        const int o[3] = {expand6(ro), expand7(go), expand6(bo)}, h[3] = {expand6(rh), expand7(gh), expand6(bh)},
                  vv[3] = {expand6(rv), expand7(gv), expand6(bv)};
        for (int i = 0; i < 16; ++i) {
            const int x = i / 4, y = i % 4;
            for (int k = 0; k < 3; ++k) px[i][k] = static_cast<u8>(clamp255((x * (h[k] - o[k]) + y * (vv[k] - o[k]) + 4 * o[k] + 2) >> 2));
        }
        return;
    }
    // Differential.
    const int c1[3] = {expand5(r), expand5(g), expand5(bl)}, c2[3] = {expand5(r2), expand5(g2), expand5(b2)};
    const int t1 = (b[3] >> 5) & 7, t2 = (b[3] >> 2) & 7;
    for (int i = 0; i < 16; ++i) {
        const int x = i / 4, y = i % 4;
        const bool second = flip ? y >= 2 : x >= 2;
        const int* c = second ? c2 : c1;
        const int m = kEtc1Tables[second ? t2 : t1][pixelIndex(i)];
        for (int k = 0; k < 3; ++k) px[i][k] = static_cast<u8>(clamp255(c[k] + m));
    }
}

void decodeAlphaBlock(const u8* b, u8 px[16][4]) {
    const int base = b[0], mult = b[1] >> 4, table = b[1] & 15;
    u64 bits = 0;
    for (int k = 2; k < 8; ++k) bits = bits << 8 | b[k];
    for (int i = 0; i < 16; ++i) {
        const int idx = static_cast<int>((bits >> (45 - 3 * i)) & 7);
        px[i][3] = static_cast<u8>(clamp255(base + kEacTables[table][idx] * mult));
    }
}

// ------------------------------------------------------------------ encode
struct Px {
    int c[4];
};

/// Best table and indices for a set of 8 pixels around a base color; returns the error.
int fitSubblock(const Px* px, const int* subIdx, const int base[3], int& tableOut, int idxOut[8]) {
    int best = INT_MAX;
    for (int t = 0; t < 8; ++t) {
        int err = 0, idx[8];
        for (int n = 0; n < 8; ++n) {
            const Px& p = px[subIdx[n]];
            int be = INT_MAX, bi = 0;
            for (int m = 0; m < 4; ++m) {
                int e = 0;
                for (int k = 0; k < 3; ++k) {
                    const int d = clamp255(base[k] + kEtc1Tables[t][m]) - p.c[k];
                    e += d * d;
                }
                if (e < be) be = e, bi = m;
            }
            err += be;
            idx[n] = bi;
            if (err >= best) break;
        }
        if (err < best) {
            best = err;
            tableOut = t;
            std::memcpy(idxOut, idx, sizeof idx);
        }
    }
    return best;
}

void encodeColorBlock(const Px px[16], u8* out) {
    int bestErr = INT_MAX;
    u8 bestBlock[8]{};
    for (int flip = 0; flip < 2; ++flip) {
        int sub[2][8];
        int n0 = 0, n1 = 0;
        for (int i = 0; i < 16; ++i) {
            const int x = i / 4, y = i % 4;
            if (flip ? y < 2 : x < 2) sub[0][n0++] = i;
            else sub[1][n1++] = i;
        }
        float avg[2][3];
        for (int s = 0; s < 2; ++s)
            for (int k = 0; k < 3; ++k) {
                int sum = 0;
                for (int n = 0; n < 8; ++n) sum += px[sub[s][n]].c[k];
                avg[s][k] = sum / 8.0f;
            }
        for (int diff = 0; diff < 2; ++diff) {
            int q[2][3], base[2][3];
            bool ok = true;
            for (int s = 0; s < 2; ++s)
                for (int k = 0; k < 3; ++k) {
                    if (diff) {
                        q[s][k] = std::clamp(static_cast<int>(avg[s][k] * 31.0f / 255.0f + 0.5f), 0, 31);
                        base[s][k] = expand5(q[s][k]);
                    } else {
                        q[s][k] = std::clamp(static_cast<int>(avg[s][k] * 15.0f / 255.0f + 0.5f), 0, 15);
                        base[s][k] = expand4(q[s][k]);
                    }
                }
            if (diff)
                for (int k = 0; k < 3; ++k) {
                    const int d = q[1][k] - q[0][k];
                    if (d < -4 || d > 3) ok = false;
                }
            if (!ok) continue;
            int tables[2], idx[2][8];
            const int err = fitSubblock(px, sub[0], base[0], tables[0], idx[0]) + fitSubblock(px, sub[1], base[1], tables[1], idx[1]);
            if (err >= bestErr) continue;
            bestErr = err;
            u8 b[8]{};
            for (int k = 0; k < 3; ++k)
                b[k] = diff ? static_cast<u8>(q[0][k] << 3 | ((q[1][k] - q[0][k]) & 7)) : static_cast<u8>(q[0][k] << 4 | q[1][k]);
            b[3] = static_cast<u8>(tables[0] << 5 | tables[1] << 2 | diff << 1 | flip);
            u32 msb = 0, lsb = 0;
            for (int s = 0; s < 2; ++s)
                for (int n = 0; n < 8; ++n) {
                    const int i = sub[s][n], v = idx[s][n];  // table order a, b, -a, -b = (msb, lsb) 00, 01, 10, 11
                    msb |= static_cast<u32>(v >> 1) << i;
                    lsb |= static_cast<u32>(v & 1) << i;
                }
            const u32 bits = msb << 16 | lsb;
            b[4] = static_cast<u8>(bits >> 24), b[5] = static_cast<u8>(bits >> 16), b[6] = static_cast<u8>(bits >> 8), b[7] = static_cast<u8>(bits);
            std::memcpy(bestBlock, b, 8);
        }
    }
    std::memcpy(out, bestBlock, 8);
}

void encodeAlphaBlock(const Px px[16], u8* out) {
    int lo = 255, hi = 0, sum = 0;
    for (int i = 0; i < 16; ++i) lo = std::min(lo, px[i].c[3]), hi = std::max(hi, px[i].c[3]), sum += px[i].c[3];
    if (lo == hi) {  // flat: table 13 has a 0 offset (index 4)
        out[0] = static_cast<u8>(lo);
        out[1] = static_cast<u8>(1 << 4 | 13);
        const u64 bits = 0x924924924924ull;  // index 4 everywhere (100 repeated)
        for (int k = 0; k < 6; ++k) out[2 + k] = static_cast<u8>(bits >> (40 - 8 * k));
        return;
    }
    int bestErr = INT_MAX;
    for (int t = 0; t < 16; ++t) {
        const int range = kEacTables[t][7] - kEacTables[t][3];  // max - min of the table
        const int mid = (lo + hi + 1) / 2;
        for (int dm = -1; dm <= 1; ++dm) {
            const int mult = std::clamp((hi - lo + range / 2) / std::max(range, 1) + dm, 1, 15);
            for (int base : {mid, (sum + 8) / 16}) {
                int err = 0, idx[16];
                for (int i = 0; i < 16 && err < bestErr; ++i) {
                    int be = INT_MAX, bi = 0;
                    for (int m = 0; m < 8; ++m) {
                        const int d = clamp255(base + kEacTables[t][m] * mult) - px[i].c[3];
                        if (d * d < be) be = d * d, bi = m;
                    }
                    err += be;
                    idx[i] = bi;
                }
                if (err >= bestErr) continue;
                bestErr = err;
                out[0] = static_cast<u8>(base);
                out[1] = static_cast<u8>(mult << 4 | t);
                u64 bits = 0;
                for (int i = 0; i < 16; ++i) bits |= static_cast<u64>(idx[i]) << (45 - 3 * i);
                for (int k = 0; k < 6; ++k) out[2 + k] = static_cast<u8>(bits >> (40 - 8 * k));
            }
        }
    }
}

void readW32(const u8* p, u32& v) { std::memcpy(&v, p, 4); }

}  // namespace

std::vector<u8> encodeEtc2Rgba(const u8* rgba, u32 width, u32 height) {
    const u32 bw = (width + 3) / 4, bh = (height + 3) / 4;
    std::vector<u8> out(static_cast<usize>(bw) * bh * 16);
    for (u32 by = 0; by < bh; ++by)
        for (u32 bx = 0; bx < bw; ++bx) {
            Px px[16];
            for (int i = 0; i < 16; ++i) {
                const u32 x = std::min(bx * 4 + static_cast<u32>(i / 4), width - 1), y = std::min(by * 4 + static_cast<u32>(i % 4), height - 1);
                const u8* s = rgba + (static_cast<usize>(y) * width + x) * 4;
                for (int k = 0; k < 4; ++k) px[i].c[k] = s[k];
            }
            u8* block = out.data() + (static_cast<usize>(by) * bw + bx) * 16;
            encodeAlphaBlock(px, block);
            encodeColorBlock(px, block + 8);
        }
    return out;
}

void decodeEtc2Rgba(const u8* blocks, u32 width, u32 height, u8* rgba) {
    const u32 bw = (width + 3) / 4, bh = (height + 3) / 4;
    for (u32 by = 0; by < bh; ++by)
        for (u32 bx = 0; bx < bw; ++bx) {
            const u8* b = blocks + (static_cast<usize>(by) * bw + bx) * 16;
            u8 px[16][4];
            decodeAlphaBlock(b, px);
            decodeColorBlock(b + 8, px);
            for (int i = 0; i < 16; ++i) {
                const u32 x = bx * 4 + static_cast<u32>(i / 4), y = by * 4 + static_cast<u32>(i % 4);
                if (x < width && y < height) std::memcpy(rgba + (static_cast<usize>(y) * width + x) * 4, px[i], 4);
            }
        }
}

CompressedImage compressWithMips(const u8* rgba, u32 width, u32 height) {
    CompressedImage img;
    img.width = width, img.height = height;
    std::vector<u8> cur(rgba, rgba + static_cast<usize>(width) * height * 4);
    u32 w = width, h = height;
    for (;;) {
        img.levels.push_back(encodeEtc2Rgba(cur.data(), w, h));
        if (w == 1 && h == 1) break;
        const u32 nw = std::max(1u, w / 2), nh = std::max(1u, h / 2);
        std::vector<u8> next(static_cast<usize>(nw) * nh * 4);
        for (u32 y = 0; y < nh; ++y)
            for (u32 x = 0; x < nw; ++x)
                for (int k = 0; k < 4; ++k) {
                    int s = 0, n = 0;
                    for (u32 dy = 0; dy < 2; ++dy)
                        for (u32 dx = 0; dx < 2; ++dx) {
                            const u32 sx = std::min(x * 2 + dx, w - 1), sy = std::min(y * 2 + dy, h - 1);
                            s += cur[(static_cast<usize>(sy) * w + sx) * 4 + static_cast<usize>(k)];
                            ++n;
                        }
                    next[(static_cast<usize>(y) * nw + x) * 4 + static_cast<usize>(k)] = static_cast<u8>((s + n / 2) / n);
                }
        cur.swap(next);
        w = nw, h = nh;
    }
    return img;
}

namespace {
constexpr u8 kKtxId[12] = {0xAB, 'K', 'T', 'X', ' ', '1', '1', 0xBB, '\r', '\n', 0x1A, '\n'};
}

std::vector<u8> writeKtx(const CompressedImage& image) {
    std::vector<u8> out(kKtxId, kKtxId + 12);
    auto put = [&](u32 v) {
        const usize at = out.size();
        out.resize(at + 4);
        std::memcpy(out.data() + at, &v, 4);
    };
    put(0x04030201);  // endianness
    put(0);           // glType (compressed)
    put(1);           // glTypeSize
    put(0);           // glFormat
    put(image.glFormat);
    put(0x1908);  // GL_RGBA
    put(image.width);
    put(image.height);
    put(0);  // depth
    put(0);  // array elements
    put(1);  // faces
    put(static_cast<u32>(image.levels.size()));
    put(0);  // key/value bytes
    for (const auto& level : image.levels) {
        put(static_cast<u32>(level.size()));
        out.insert(out.end(), level.begin(), level.end());
        while (out.size() % 4) out.push_back(0);
    }
    return out;
}

bool isKtx(std::span<const u8> bytes) { return bytes.size() >= 64 && std::memcmp(bytes.data(), kKtxId, 12) == 0; }

Result<CompressedImage> readKtx(std::span<const u8> bytes) {
    if (!isKtx(bytes)) return makeError(Error::Code::ParseError, "not a KTX 1 file");
    u32 h[13];
    for (int i = 0; i < 13; ++i) readW32(bytes.data() + 12 + i * 4, h[i]);
    if (h[0] != 0x04030201) return makeError(Error::Code::Unsupported, "KTX: big-endian files are not supported");
    CompressedImage img;
    img.glFormat = h[4];
    img.width = h[6], img.height = h[7];
    if (img.glFormat != kGlRgba8Etc2Eac && img.glFormat != kGlSrgb8Alpha8Etc2Eac)
        return makeError(Error::Code::Unsupported, "KTX: only RGBA8 ETC2 + EAC is supported");
    if (h[10] != 1 || h[8] > 1 || h[9] != 0) return makeError(Error::Code::Unsupported, "KTX: 2D textures only");
    usize at = 64 + h[12];
    const u32 levels = std::max(1u, h[11]);
    u32 w = img.width, hh = img.height;
    for (u32 l = 0; l < levels; ++l) {
        if (at + 4 > bytes.size()) return makeError(Error::Code::ParseError, "KTX: truncated");
        u32 size;
        readW32(bytes.data() + at, size);
        at += 4;
        if (size != levelBytes(w, hh) || at + size > bytes.size()) return makeError(Error::Code::ParseError, "KTX: bad level size");
        img.levels.emplace_back(bytes.begin() + static_cast<std::ptrdiff_t>(at), bytes.begin() + static_cast<std::ptrdiff_t>(at + size));
        at += (size + 3) & ~3u;
        w = std::max(1u, w / 2), hh = std::max(1u, hh / 2);
    }
    return img;
}

} // namespace agr::etc2
