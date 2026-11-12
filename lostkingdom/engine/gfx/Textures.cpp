#include "Textures.h"
#include "../core/Noise.h"
#include <cmath>
#include <algorithm>

namespace agr {
namespace texgen {

static inline int pmod(int a, int p) { int r = a % p; return r < 0 ? r + p : r; }

float tileNoise(float x, float y, int P, uint32_t seed) {
    int xi = (int)std::floor(x), yi = (int)std::floor(y);
    float xf = x - xi, yf = y - yi;
    auto g = [&](int ix, int iy, float dx, float dy) {
        float a = hashf(hash2i(pmod(ix, P), pmod(iy, P), seed)) * TAU;
        return std::cos(a) * dx + std::sin(a) * dy;
    };
    float u = xf * xf * xf * (xf * (xf * 6 - 15) + 10), v = yf * yf * yf * (yf * (yf * 6 - 15) + 10);
    return lerpf(lerpf(g(xi, yi, xf, yf), g(xi + 1, yi, xf - 1, yf), u), lerpf(g(xi, yi + 1, xf, yf - 1), g(xi + 1, yi + 1, xf - 1, yf - 1), u), v) * 1.41421f;
}

float tileFbm(float x, float y, int P, int oct, uint32_t seed) {
    float s = 0, a = 0.5f, n = 0;
    for (int i = 0; i < oct; i++) {
        s += a * tileNoise(x, y, P, seed + i * 31);
        n += a; x *= 2; y *= 2; P *= 2; a *= 0.5f;
    }
    return s / n;
}

float tileWorley(float x, float y, int P, uint32_t seed, float* second, uint32_t* cellId) {
    int xi = (int)std::floor(x), yi = (int)std::floor(y);
    float b1 = 9, b2 = 9; uint32_t id = 0;
    for (int j = -1; j <= 1; j++)
        for (int i = -1; i <= 1; i++) {
            uint32_t h = hash2i(pmod(xi + i, P), pmod(yi + j, P), seed);
            float px = xi + i + 0.1f + 0.8f * hashf(h), py = yi + j + 0.1f + 0.8f * hashf(hash32(h));
            float d = std::sqrt((px - x) * (px - x) + (py - y) * (py - y));
            if (d < b1) { b2 = b1; b1 = d; id = h; } else if (d < b2) b2 = d;
        }
    if (second) *second = b2;
    if (cellId) *cellId = id;
    return b1;
}

static uint8_t u8(float f) { return (uint8_t)clampf(f * 255.f + 0.5f, 0, 255); }

ImageRGBA noiseTexture(int S) {
    ImageRGBA img; img.alloc(S, S);
    for (int y = 0; y < S; y++)
        for (int x = 0; x < S; x++) {
            float u = (float)x / S, v = (float)y / S;
            float r = tileFbm(u * 16, v * 16, 16, 4, 11) * 0.5f + 0.5f;
            float g = tileFbm(u * 6, v * 6, 6, 5, 23) * 0.5f + 0.5f;
            float b = tileFbm(u * 4, v * 4, 4, 6, 37) * 0.5f + 0.5f;
            float w = tileWorley(u * 10, v * 10, 10, 41);
            float a = clampf(0.6f * (tileFbm(u * 8, v * 8, 8, 4, 53) * 0.5f + 0.5f) + 0.4f * w, 0, 1);
            uint8_t* p = img.at(x, y);
            p[0] = u8(r); p[1] = u8(g); p[2] = u8(b); p[3] = u8(a);
        }
    return img;
}

void terrainLayers(int S, std::vector<std::vector<uint8_t>>& alb, std::vector<std::vector<uint8_t>>& nrm) {
    const int L = 8;
    alb.assign(L, std::vector<uint8_t>((size_t)S * S * 4));
    nrm.assign(L, std::vector<uint8_t>((size_t)S * S * 4));
    std::vector<float> H((size_t)S * S);
    for (int l = 0; l < L; l++) {
        for (int y = 0; y < S; y++)
            for (int x = 0; x < S; x++) {
                float u = (float)x / S, v = (float)y / S;
                vec3 c; float h = 0.5f;
                switch (l) {
                case 0: {  // grass: layered clumps + blade streaks + small flowers of colour
                    float n1 = tileFbm(u * 8, v * 8, 8, 4, 101) * 0.5f + 0.5f;
                    float n2 = tileFbm(u * 32, v * 32, 32, 2, 103) * 0.5f + 0.5f;
                    float streak = tileNoise(u * 64, v * 12, 64, 105) * 0.5f + 0.5f;
                    c = lerp(hexColor(0x3f6b25), hexColor(0x79a33a), n1);
                    c = lerp(c, hexColor(0x5d8a2c), streak * 0.4f);
                    c = c * (0.85f + n2 * 0.3f);
                    float dry = smoothstepf(0.62f, 0.8f, tileFbm(u * 4, v * 4, 4, 3, 107) * 0.5f + 0.5f);
                    c = lerp(c, hexColor(0x9a9a4a), dry * 0.45f);
                    h = 0.35f + n2 * 0.35f + streak * 0.3f;
                    break;
                }
                case 1: {  // dirt path: packed earth with pebbles
                    float n1 = tileFbm(u * 6, v * 6, 6, 5, 201) * 0.5f + 0.5f;
                    float sd; uint32_t id; float w = tileWorley(u * 24, v * 24, 24, 203, &sd, &id);
                    float peb = smoothstepf(0.32f, 0.18f, w) * (hashf(id) > 0.55f ? 1.f : 0.f);
                    c = lerp(hexColor(0x6b4e33), hexColor(0x8f6d47), n1);
                    c = lerp(c, lerp(hexColor(0x8a8478), hexColor(0xa9a194), hashf(id >> 3)), peb);
                    h = 0.3f + n1 * 0.3f + peb * 0.5f;
                    break;
                }
                case 2: {  // rock: strata + cracks + lichen
                    float n1 = tileFbm(u * 5, v * 5, 5, 6, 301) * 0.5f + 0.5f;
                    float sd; float w = tileWorley(u * 11, v * 11, 11, 303, &sd);
                    float crack = smoothstepf(0.045f, 0.0f, sd - w) * (0.4f + 0.6f * smoothstepf(0.35f, 0.7f, n1));
                    float strata = std::sin((v * 22 + n1 * 3.f) * TAU) * 0.5f + 0.5f;
                    c = lerp(hexColor(0x5d5b58), hexColor(0x8d8984), n1 * 0.7f + strata * 0.3f);
                    float lichen = smoothstepf(0.6f, 0.75f, tileFbm(u * 9, v * 9, 9, 3, 305) * 0.5f + 0.5f);
                    c = lerp(c, hexColor(0x7d8a4c), lichen * 0.35f);
                    c = c * (1.f - crack * 0.32f);
                    h = 0.55f + n1 * 0.35f - crack * 0.25f;
                    break;
                }
                case 3: {  // snow: soft drifts, cold blue in hollows
                    float n1 = tileFbm(u * 4, v * 4, 4, 5, 401) * 0.5f + 0.5f;
                    float n2 = tileNoise(u * 48, v * 48, 48, 403) * 0.5f + 0.5f;
                    c = lerp(hexColor(0x95a2b8), hexColor(0xc2cad8), n1);
                    c = c * (0.88f + n2 * 0.18f);
                    h = 0.4f + n1 * 0.5f;
                    break;
                }
                case 4: {  // sand: wind ripples
                    float n1 = tileFbm(u * 6, v * 6, 6, 4, 501) * 0.5f + 0.5f;
                    // soft wind ripples, broken up by noise so they never read as regular stripes
                    float rip = std::sin((u * 16 + tileNoise(u * 4, v * 4, 4, 503) * 2.5f) * TAU) * 0.5f + 0.5f;
                    float ripMask = smoothstepf(0.35f, 0.75f, tileFbm(u * 3, v * 3, 3, 3, 507) * 0.5f + 0.5f);
                    c = lerp(hexColor(0xb59a6a), hexColor(0xd2bd8e), n1 * 0.8f + rip * 0.12f * ripMask);
                    h = 0.4f + rip * 0.15f * ripMask + n1 * 0.35f;
                    break;
                }
                case 5: {  // mud: dark, wet, puddle dimples
                    float n1 = tileFbm(u * 5, v * 5, 5, 5, 601) * 0.5f + 0.5f;
                    float w = tileWorley(u * 12, v * 12, 12, 603);
                    c = lerp(hexColor(0x2d241a), hexColor(0x4d3d2b), n1);
                    h = 0.25f + n1 * 0.4f + w * 0.2f;
                    break;
                }
                case 6: {  // cobblestone: rounded stones with mortar
                    float sd; uint32_t id; float w = tileWorley(u * 10, v * 10, 10, 701, &sd, &id);
                    float edge = sd - w;
                    float stone = smoothstepf(0.02f, 0.14f, edge);
                    float n1 = tileFbm(u * 20, v * 20, 20, 3, 703) * 0.5f + 0.5f;
                    vec3 sc = lerp(hexColor(0x6e6a64), hexColor(0x9a948a), hashf(id));
                    c = lerp(hexColor(0x3e3a33), sc * (0.9f + n1 * 0.2f), stone);
                    h = stone * (0.6f + 0.4f * smoothstepf(0.0f, 0.3f, edge)) * 0.9f + n1 * 0.1f;
                    break;
                }
                default: {  // ash / volcanic soil with cinders
                    float n1 = tileFbm(u * 7, v * 7, 7, 5, 801) * 0.5f + 0.5f;
                    float sd; uint32_t id; float w = tileWorley(u * 30, v * 30, 30, 803, &sd, &id);
                    float cinder = smoothstepf(0.25f, 0.1f, w) * (hashf(id) > 0.6f ? 1.f : 0.f);
                    c = lerp(hexColor(0x26221f), hexColor(0x4a4440), n1);
                    c = lerp(c, hexColor(0x15110f), cinder);
                    h = 0.35f + n1 * 0.3f + cinder * 0.35f;
                    break;
                }
                }
                uint8_t* p = &alb[l][((size_t)y * S + x) * 4];
                p[0] = u8(c.x); p[1] = u8(c.y); p[2] = u8(c.z); p[3] = u8(clampf(h, 0, 1));
                H[(size_t)y * S + x] = clampf(h, 0, 1);
            }
        // normals from height (wrap-around Sobel)
        float strength = (l == 6) ? 5.f : (l == 2 ? 4.f : (l == 3 ? 2.2f : 2.5f));
        for (int y = 0; y < S; y++)
            for (int x = 0; x < S; x++) {
                auto hh = [&](int xx, int yy) { return H[(size_t)pmod(yy, S) * S + pmod(xx, S)]; };
                float dx = (hh(x + 1, y - 1) + 2 * hh(x + 1, y) + hh(x + 1, y + 1)) - (hh(x - 1, y - 1) + 2 * hh(x - 1, y) + hh(x - 1, y + 1));
                float dy = (hh(x - 1, y + 1) + 2 * hh(x, y + 1) + hh(x + 1, y + 1)) - (hh(x - 1, y - 1) + 2 * hh(x, y - 1) + hh(x + 1, y - 1));
                vec3 n = normalize(vec3(-dx * strength, -dy * strength, 1.f));
                uint8_t* p = &nrm[l][((size_t)y * S + x) * 4];
                p[0] = u8(n.x * 0.5f + 0.5f); p[1] = u8(n.y * 0.5f + 0.5f); p[2] = u8(n.z * 0.5f + 0.5f); p[3] = 255;
            }
    }
}

ImageRGBA particleAtlas(int C) {
    ImageRGBA img; img.alloc(C * 4, C * 4);
    for (int f = 0; f < 16; f++) {
        int ox = (f % 4) * C, oy = (f / 4) * C;
        for (int y = 0; y < C; y++)
            for (int x = 0; x < C; x++) {
                float u = (x + 0.5f) / C * 2 - 1, v = (y + 0.5f) / C * 2 - 1;  // -1..1
                float r = std::sqrt(u * u + v * v), a = std::atan2(v, u);
                float val = 0; vec3 col{1, 1, 1};
                switch (f) {
                case 0: val = std::pow(saturate(1 - r), 2.f); break;                                   // soft dot
                case 1: {                                                                               // smoke puff
                    float n = tileFbm((u + 1) * 2, (v + 1) * 2, 4, 4, 911) * 0.5f + 0.5f;
                    val = saturate(1 - r * (1.1f - n * 0.5f)) * (0.5f + n * 0.7f);
                    val = smoothstepf(0.f, 0.7f, val);
                    break;
                }
                case 2: val = std::pow(saturate(1 - std::fabs(u) * 6), 2.f) * std::pow(saturate(1 - std::fabs(v)), 1.5f); break;  // spark streak (vertical)
                case 3: {                                                                               // flame tongue
                    float vv = (v + 1) * 0.5f;  // 0 top .. 1 bottom
                    float w = 0.55f * std::pow(vv, 0.7f);
                    float n = tileNoise((u + 1) * 3, (v + 1) * 3, 6, 913);
                    val = saturate(1 - std::fabs(u + n * 0.15f * (1 - vv)) / std::max(w, 0.01f)) * smoothstepf(0.f, 0.3f, vv) * smoothstepf(1.f, 0.75f, vv);
                    break;
                }
                case 4: {                                                                               // leaf
                    float lu = u * 1.7f, lv = v;
                    float shape = 1 - (lu * lu) / std::max(0.01f, (1 - lv * lv) * 0.8f);
                    float vein = smoothstepf(0.08f, 0.f, std::fabs(lu)) * 0.3f;
                    val = smoothstepf(0.f, 0.15f, shape) * (std::fabs(lv) < 0.95f ? 1.f : 0.f);
                    col = vec3(1.f - vein, 1.f - vein, 1.f - vein);
                    break;
                }
                case 5: {                                                                               // snowflake (6 arms)
                    float arm = std::fabs(std::sin(a * 3)) ;
                    val = smoothstepf(0.12f, 0.f, arm * r) * saturate(1 - r) + std::pow(saturate(1 - r * 1.4f), 3.f);
                    break;
                }
                case 6: {                                                                               // star glint
                    val = std::pow(saturate(1 - std::fabs(u) * 8) * saturate(1 - std::fabs(v)), 1.f) + std::pow(saturate(1 - std::fabs(v) * 8) * saturate(1 - std::fabs(u)), 1.f);
                    val = saturate(val + std::pow(saturate(1 - r * 2.5f), 2.f));
                    break;
                }
                case 7: val = std::pow(saturate(1 - std::fabs(r - 0.78f) * 8), 2.f); break;            // ring (shockwave)
                case 8: {                                                                               // rune circle
                    float ring = saturate(1 - std::fabs(r - 0.85f) * 25) + saturate(1 - std::fabs(r - 0.62f) * 30);
                    float glyph = (std::fabs(std::sin(a * 8)) > 0.85f && r > 0.64f && r < 0.84f) ? 1.f : 0.f;
                    float tri = 0;
                    for (int k = 0; k < 3; k++) {
                        float ang = k * TAU / 3 + PI / 2;
                        vec2 p0{std::cos(ang) * 0.6f, std::sin(ang) * 0.6f}, p1{std::cos(ang + TAU / 3) * 0.6f, std::sin(ang + TAU / 3) * 0.6f};
                        vec2 pa = vec2(u, v) - p0, ba = p1 - p0;
                        float hcl = saturate(dot(pa, ba) / dot(ba, ba));
                        tri = std::max(tri, saturate(1 - length(pa - ba * hcl) * 40));
                    }
                    val = saturate(ring + glyph * 0.8f + tri);
                    break;
                }
                case 9: val = saturate(1 - std::fabs(u) * 10) * saturate(1 - std::fabs(v) * 1.05f); break;  // rain streak
                case 10: val = std::pow(saturate(1 - r), 4.f) * 1.5f; break;                                // ember (hot core)
                case 11: {                                                                                   // ice shard
                    float d = std::fabs(u) * 2.2f + std::fabs(v) * 0.9f;
                    val = smoothstepf(1.f, 0.85f, d);
                    col = vec3(0.8f + 0.2f * (u > 0), 0.9f, 1.f);
                    break;
                }
                case 12: {                                                                                   // healing cross / plus
                    float cr = std::max(saturate(1 - std::fabs(u) * 4) * saturate(1 - std::fabs(v) * 1.3f), saturate(1 - std::fabs(v) * 4) * saturate(1 - std::fabs(u) * 1.3f));
                    val = smoothstepf(0.f, 0.3f, cr);
                    break;
                }
                case 13: {                                                                                   // bird silhouette (gull-wing V)
                    float wing = std::fabs(v + 0.25f - std::fabs(u) * 0.55f + u * u * 0.4f);
                    val = smoothstepf(0.12f, 0.04f, wing) * (std::fabs(u) < 0.95f ? 1.f : 0.f);
                    break;
                }
                case 14: {                                                                                   // dust / dirt clump
                    float n = tileFbm((u + 1) * 3, (v + 1) * 3, 6, 3, 917) * 0.5f + 0.5f;
                    val = saturate(1 - r) * smoothstepf(0.35f, 0.7f, n) * 1.4f;
                    break;
                }
                default: {                                                                                   // lightning arc fragment
                    float n = tileNoise((v + 1) * 4, 0.5f, 8, 919);
                    val = std::pow(saturate(1 - std::fabs(u - n * 0.4f) * 9), 2.f) * saturate(1 - std::fabs(v) * 1.02f);
                    break;
                }
                }
                uint8_t* p = img.at(ox + x, oy + y);
                p[0] = u8(col.x); p[1] = u8(col.y); p[2] = u8(col.z); p[3] = u8(saturate(val));
            }
    }
    return img;
}

}  // namespace texgen
}  // namespace agr
