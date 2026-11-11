// AGR Engine — deterministic hashing, random numbers and coherent noise.
// Everything is seed-based so that the world can be regenerated identically
// when chunks are streamed back in (only *modifications* need to be saved).
#pragma once
#include "Math.h"
#include <cstdint>

namespace agr {

inline uint32_t hash32(uint32_t x) {
    x ^= x >> 16; x *= 0x7feb352dU; x ^= x >> 15; x *= 0x846ca68bU; x ^= x >> 16;
    return x;
}
inline uint32_t hash2i(int x, int y, uint32_t seed = 0) {
    return hash32((uint32_t)x * 0x8da6b343U ^ (uint32_t)y * 0xd8163841U ^ seed * 0xcb1ab31fU);
}
inline uint32_t hash3i(int x, int y, int z, uint32_t seed = 0) {
    return hash32((uint32_t)x * 0x8da6b343U ^ (uint32_t)y * 0xd8163841U ^ (uint32_t)z * 0xcb1ab31fU ^ seed * 0x165667b1U);
}
inline float hashf(uint32_t h) { return (h & 0xffffff) / 16777216.0f; }

// Small, fast PCG-style generator.
struct Rng {
    uint64_t s;
    explicit Rng(uint64_t seed = 0x853c49e6748fea9bULL) : s(seed * 6364136223846793005ULL + 1442695040888963407ULL) { next(); }
    uint32_t next() {
        uint64_t old = s; s = old * 6364136223846793005ULL + 1442695040888963407ULL;
        uint32_t xs = (uint32_t)(((old >> 18u) ^ old) >> 27u), rot = (uint32_t)(old >> 59u);
        return (xs >> rot) | (xs << ((-rot) & 31));
    }
    float f() { return (next() >> 8) * (1.0f / 16777216.0f); }
    float range(float a, float b) { return a + (b - a) * f(); }
    int irange(int a, int b) { return a + (int)(next() % (uint32_t)(b - a + 1)); }  // inclusive
    bool chance(float p) { return f() < p; }
    vec2 inDisk() { float a = f() * TAU, r = std::sqrt(f()); return {std::cos(a) * r, std::sin(a) * r}; }
    vec3 onSphere() { float z = range(-1, 1), a = f() * TAU, r = std::sqrt(1 - z * z); return {r * std::cos(a), z, r * std::sin(a)}; }
};

// Gradient (Perlin-style) noise in 2D, range ~[-1,1].
inline float gradNoise2(float x, float y, uint32_t seed = 0) {
    int xi = (int)std::floor(x), yi = (int)std::floor(y);
    float xf = x - xi, yf = y - yi;
    auto g = [&](int ix, int iy, float dx, float dy) {
        uint32_t h = hash2i(ix, iy, seed);
        float a = hashf(h) * TAU;
        return std::cos(a) * dx + std::sin(a) * dy;
    };
    float u = xf * xf * xf * (xf * (xf * 6 - 15) + 10), v = yf * yf * yf * (yf * (yf * 6 - 15) + 10);
    float n00 = g(xi, yi, xf, yf), n10 = g(xi + 1, yi, xf - 1, yf), n01 = g(xi, yi + 1, xf, yf - 1), n11 = g(xi + 1, yi + 1, xf - 1, yf - 1);
    return lerpf(lerpf(n00, n10, u), lerpf(n01, n11, u), v) * 1.41421f;
}

inline float valueNoise2(float x, float y, uint32_t seed = 0) {
    int xi = (int)std::floor(x), yi = (int)std::floor(y);
    float xf = x - xi, yf = y - yi;
    float u = xf * xf * (3 - 2 * xf), v = yf * yf * (3 - 2 * yf);
    float a = hashf(hash2i(xi, yi, seed)), b = hashf(hash2i(xi + 1, yi, seed));
    float c = hashf(hash2i(xi, yi + 1, seed)), d = hashf(hash2i(xi + 1, yi + 1, seed));
    return lerpf(lerpf(a, b, u), lerpf(c, d, u), v) * 2 - 1;
}

inline float fbm2(float x, float y, int octaves, uint32_t seed = 0, float lac = 2.03f, float gain = 0.5f) {
    float sum = 0, amp = 0.5f, norm = 0;
    for (int i = 0; i < octaves; i++) {
        sum += amp * gradNoise2(x, y, seed + i * 1013);
        norm += amp; x *= lac; y *= lac; amp *= gain;
        float nx = x * 0.8f - y * 0.6f, ny = x * 0.6f + y * 0.8f; x = nx; y = ny;  // rotate octaves to hide grid
    }
    return sum / norm;
}

// Ridged multifractal: sharp mountain crests.
inline float ridged2(float x, float y, int octaves, uint32_t seed = 0) {
    float sum = 0, amp = 0.5f, norm = 0, prev = 1;
    for (int i = 0; i < octaves; i++) {
        float n = 1 - std::fabs(gradNoise2(x, y, seed + i * 7919));
        n *= n;
        sum += n * amp * prev; prev = n; norm += amp;
        x *= 2.1f; y *= 2.1f; amp *= 0.5f;
    }
    return sum / norm;
}

// Cellular (Worley) distance to nearest feature point, in cell units.
inline float worley2(float x, float y, uint32_t seed = 0) {
    int xi = (int)std::floor(x), yi = (int)std::floor(y);
    float best = 9;
    for (int j = -1; j <= 1; j++)
        for (int i = -1; i <= 1; i++) {
            uint32_t h = hash2i(xi + i, yi + j, seed);
            float px = xi + i + hashf(h), py = yi + j + hashf(hash32(h));
            float dx = px - x, dy = py - y;
            best = std::min(best, dx * dx + dy * dy);
        }
    return std::sqrt(best);
}

}  // namespace agr
