#include "Props.h"
#include <mutex>

namespace lk {

// ------------------------------------------------------------------------------------------ palette
namespace pal {
const vec3 bark = hexColor(0x5a4030), barkDark = hexColor(0x3b2b22), birch = hexColor(0xe2ddd0), birchMark = hexColor(0x2f2a26);
const vec3 leaf1 = hexColor(0x46692a), leaf2 = hexColor(0x6a8a36), leaf3 = hexColor(0x30522a), leafAut = hexColor(0xb8862e);
const vec3 pine = hexColor(0x2c5233), pine2 = hexColor(0x3a6a3d), snow = hexColor(0xeef3f8), ice = hexColor(0xa9d8f0);
const vec3 elder = hexColor(0x2b6f66), elder2 = hexColor(0x4f8f6a), elderGlow = hexColor(0x8ff0d8);
const vec3 stone = hexColor(0x8d8880), stoneDark = hexColor(0x5e5a55), stoneLight = hexColor(0xb3ada2), moss = hexColor(0x5d7a35);
const vec3 plaster = hexColor(0xe6dcc6), timber = hexColor(0x4a3322), thatch = hexColor(0xb08f4f), tile = hexColor(0x8e3b2a), slate = hexColor(0x4a5160);
const vec3 wood = hexColor(0x7a5636), woodLight = hexColor(0x9c7448), cloth1 = hexColor(0x8e2a24), cloth2 = hexColor(0x2a4f7a), cloth3 = hexColor(0xc9a24a);
const vec3 iron = hexColor(0x5d6066), gold = hexColor(0xc9a45c), obsidian = hexColor(0x1c1820), ember = hexColor(0xff7a26), marble = hexColor(0xe8e6df);
const vec3 window = hexColor(0xffc56b), rune = hexColor(0x6ff0ff), dark = hexColor(0x0d0b0a);
}  // namespace pal

vec3 regionGroundTint(int r) {
    switch (r) {
    case R_GREENHAVEN: return {1.0f, 1.0f, 0.95f};
    case R_FROSTHEIM: return {0.97f, 1.0f, 1.06f};
    case R_ASHEN: return {0.95f, 0.85f, 0.8f};
    case R_ELDERWOOD: return {0.86f, 0.98f, 0.94f};
    case R_SHADOWS: return {0.78f, 0.8f, 0.72f};
    default: return {1.0f, 0.97f, 0.92f};
    }
}

// Grass/foliage colour for a region; t in [0,1] walks from lush/deep to dry/sunlit (natural meadow range).
static vec3 ramp3(uint32_t a, uint32_t b, uint32_t c, float t) { t = clampf(t, 0.f, 1.f); return t < 0.5f ? lerp(hexColor(a), hexColor(b), t * 2) : lerp(hexColor(b), hexColor(c), t * 2 - 1); }
vec3 foliageColor(int r, float t) {
    switch (r) {
    case R_GREENHAVEN: return ramp3(0x4a6a2a, 0x6c8a38, 0xa29a58, t);
    case R_FROSTHEIM: return ramp3(0x6f7a62, 0x8f9480, 0xb2b29a, t);
    case R_ASHEN: return ramp3(0x4a4034, 0x6a5638, 0x8a7448, t);
    case R_ELDERWOOD: return ramp3(0x2f5a44, 0x4f7a56, 0x7a9a68, t);
    case R_SHADOWS: return ramp3(0x3a4632, 0x55603e, 0x76744e, t);
    default: return ramp3(0x5c6c3a, 0x7c8a4a, 0xa8a26a, t);
    }
}

// ------------------------------------------------------------------------------------------ helpers
static quat rotY(float a) { return quat::axisAngle({0, 1, 0}, a); }

// A canopy clump: lumpy ellipsoid with "puffy" normals (pointing out of the clump, like real foliage
// volumes are lit) plus a shell of small double-sided leaf cards that break the silhouette.
// Underside darker (self-shadow), wind weight rising with height.
static bool gLiteBuild = false;   // building the mid-distance (LOD1) templates: no leaf cards, coarser volume
static void clump(MeshBuilder& m, vec3 c, vec3 r, vec3 col, float wind, uint32_t seed) {
    bool f = m.flat; m.flat = false;
    Rng rng(seed);
    size_t from = m.v.size();
    float ph1 = rng.f() * 6.f, ph2 = rng.f() * 6.f;
    auto lump = [&](vec3 u) { return 1.f + 0.11f * std::sin(u.x * 3.1f + ph1) * std::sin(u.z * 2.7f + ph2) + 0.07f * std::sin(u.y * 4.3f + ph1 * 1.7f); };
    // core volume (slightly smaller: the leaf shell provides the outer silhouette)
    const int seg = gLiteBuild ? 7 : 9, rings = gLiteBuild ? 5 : 6;
    uint32_t base = (uint32_t)m.v.size();
    for (int j = 0; j <= rings; j++) {
        float th = PI * j / rings;
        for (int i = 0; i <= seg; i++) {
            float phi = TAU * i / seg;
            vec3 u{std::sin(th) * std::cos(phi), std::cos(th), std::sin(th) * std::sin(phi)};
            vec3 p = c + u * r * (0.86f * lump(u));
            vec3 n = normalize(normalize(vec3(u.x / r.x, u.y / r.y, u.z / r.z)) + vec3(0, 0.25f, 0));
            m.vert(p, n, Mat(col, 0.8f, 0, 0, wind));
        }
    }
    for (int j = 0; j < rings; j++)
        for (int i = 0; i < seg; i++) {
            uint32_t a = base + j * (seg + 1) + i, b = a + 1, d = a + seg + 1, e = d + 1;
            m.tri(a, d, b); m.tri(b, d, e);
        }
    // leaf cards on the surface, oriented in the tangent plane, sharing the volume normal
    int cards = gLiteBuild ? 0 : (int)clampf(6.f + (r.x + r.z) * 5.f, 8.f, 22.f);
    float avgR = (r.x + r.y + r.z) / 3.f;
    for (int k = 0; k < cards; k++) {
        vec3 u = rng.onSphere();
        if (u.y < -0.55f) u.y = -u.y * 0.5f;                 // few cards underneath
        u = normalize(u);
        vec3 p = c + u * r * rng.range(0.92f, 1.08f) * lump(u);
        vec3 n = normalize(normalize(vec3(u.x / r.x, u.y / r.y, u.z / r.z)) + vec3(0, 0.3f, 0));
        vec3 t = normalize(cross(n, std::fabs(n.y) > 0.9f ? vec3(1, 0, 0) : vec3(0, 1, 0)));
        float a = rng.f() * TAU;
        vec3 tx = t * std::cos(a) + cross(n, t) * std::sin(a), ty = cross(n, tx);
        float sz = avgR * rng.range(0.4f, 0.62f);
        vec3 lc = col * rng.range(0.85f, 1.18f);
        lc = lerp(lc, vec3(lc.x * 1.1f, lc.y * 1.08f, lc.z * 0.8f), rng.f() * 0.4f);  // sun-warmed leaves
        Mat lm(lc, 0.7f, 0, 0, wind);
        // irregular diamond (pointed leaf cluster), tilted outwards a bit
        vec3 q0 = p - tx * sz * 0.5f, q1 = p + ty * sz * 0.38f + n * sz * 0.12f, q2 = p + tx * sz * 0.55f, q3 = p - ty * sz * 0.42f - n * sz * 0.05f;
        uint32_t i0 = m.vert(q0, n, lm), i1 = m.vert(q1, n, lm), i2 = m.vert(q2, n, lm), i3 = m.vert(q3, n, lm);
        m.tri(i0, i1, i2); m.tri(i0, i2, i3);   // foliage is drawn two-sided: no back faces needed
    }
    m.applyColorJitter(from, 0.1f, seed);
    for (size_t i = from; i < m.v.size(); i++) {
        float k = clampf((m.v[i].py - (c.y - r.y)) / (2 * r.y), 0, 1);
        m.v[i].a = (uint8_t)(255 * (0.5f + 0.5f * k));
        m.v[i].wind = (uint8_t)clampf(wind * (0.5f + 0.5f * k) * 255, 0, 255);
    }
    m.flat = f;
}

static void branchTree(MeshBuilder& m, vec3 a, vec3 dir, float len, float r, int depth, Rng& rng, vec3 col, std::vector<vec3>* tips) {
    vec3 b = a + dir * len;
    m.tube(a, b, r, r * 0.65f, Mat(col, 0.9f, 0, 0, depth < 2 ? 0.f : 0.25f), 6, false);
    if (depth >= 3 || r < 0.03f) { if (tips) tips->push_back(b); return; }
    int n = depth == 0 ? 3 : 2;
    for (int i = 0; i < n; i++) {
        vec3 nd = normalize(dir + vec3(rng.range(-0.9f, 0.9f), rng.range(0.1f, 0.6f), rng.range(-0.9f, 0.9f)));
        branchTree(m, b, nd, len * rng.range(0.55f, 0.75f), r * 0.6f, depth + 1, rng, col, tips);
    }
}

static void ringOfRoots(MeshBuilder& m, float r, int n, vec3 col, Rng& rng) {
    for (int i = 0; i < n; i++) {
        float a = i * TAU / n + rng.range(-0.2f, 0.2f);
        vec3 d{std::cos(a), 0, std::sin(a)};
        m.tube(d * r * 0.4f + vec3(0, r * 0.6f, 0), d * r * 1.6f + vec3(0, -0.15f, 0), r * 0.32f, r * 0.08f, Mat(col, 0.9f), 6, false);
    }
}

static void gableRoof(MeshBuilder& m, float w, float l, float y, float h, float over, const Mat& mat) {
    float x = w / 2 + over, z = l / 2 + over;
    vec3 a{-x, y, -z}, b{x, y, -z}, c{x, y, z}, d{-x, y, z}, ra{0, y + h, -z}, rb{0, y + h, z};
    m.quadP(a, d, rb, ra, mat);   // left slope
    m.quadP(c, b, ra, rb, mat);   // right slope
    // gable ends (triangles) and thickness
    m.triP(a, ra, b, mat);
    m.triP(c, rb, d, mat);
    m.quadP(b, a, a - vec3(0, 0.12f, 0), b - vec3(0, 0.12f, 0), mat);
}

static void addBoxCollider(ChunkBuild& out, const Structure& s, float baseY, vec2 localC, float hx, float hz, float top, float localRot = 0) {
    Collider c;
    vec2 wc = rotate(localC * s.scale, -s.rot) + s.pos;
    c.c = wc; c.hx = hx * s.scale; c.hz = hz * s.scale; c.rot = -(s.rot + localRot); c.top = top * s.scale; c.base = baseY - 1.f;
    c.top += 1.f;
    out.colliders.push_back(c);
}

static void addCircleCollider(ChunkBuild& out, const Structure& s, float baseY, vec2 localC, float r, float top) {
    Collider c;
    c.c = rotate(localC * s.scale, -s.rot) + s.pos; c.r = r * s.scale; c.top = top * s.scale + 1.f; c.base = baseY - 1.f;
    out.colliders.push_back(c);
}

// ------------------------------------------------------------------------------------------ prop templates
static std::vector<std::vector<PropTemplate>> gTemplates, gTemplatesLite;
static std::once_flag gOnce;

static void buildProp(PropKind k, int var, PropTemplate& t) {
    MeshBuilder& S = t.solid;
    MeshBuilder& F = t.foliage;
    Rng rng(hash2i(k, var, 31337));
    t.radius = 0.4f; t.height = 2.f;
    switch (k) {
    case P_OAK: case P_BIRCH: {
        bool birch = k == P_BIRCH;
        float h = birch ? rng.range(4.5f, 6.f) : rng.range(3.0f, 4.0f);
        float r0 = birch ? 0.16f : 0.34f;
        vec3 lean{rng.range(-0.25f, 0.25f), 1, rng.range(-0.25f, 0.25f)};
        vec3 top = normalize(lean) * h;
        S.tube({0, -0.3f, 0}, top * 0.55f, r0 * 1.25f, r0 * 0.8f, Mat(birch ? pal::birch : pal::bark, 0.9f), 8, false);
        S.tube(top * 0.55f, top, r0 * 0.8f, r0 * 0.45f, Mat(birch ? pal::birch : pal::bark, 0.9f, 0, 0, 0.1f), 7, false);
        if (birch) for (auto& v : S.v) if (std::fmod(std::fabs(v.py * 3.7f + v.px * 5), 1.f) < 0.18f) { v.r = 50; v.g = 45; v.b = 40; }
        if (!birch) { Rng r2(var); ringOfRoots(S, r0 * 1.2f, 4, pal::barkDark, r2); }
        for (int i = 0; i < 3; i++) {
            float a = i * TAU / 3 + rng.f();
            vec3 bs = top * rng.range(0.55f, 0.8f);
            S.tube(bs, bs + vec3(std::cos(a) * 1.1f, 0.8f, std::sin(a) * 1.1f), r0 * 0.4f, r0 * 0.15f, Mat(birch ? pal::birch : pal::bark, 0.9f, 0, 0, 0.3f), 5, false);
        }
        int nc = birch ? 6 : 8;
        vec3 cc = top + vec3(0, birch ? 0.6f : 1.2f, 0);
        float cr = birch ? 1.3f : 2.2f;
        vec3 base = birch ? hexColor(0x7fa83c) : (var % 4 == 3 ? pal::leafAut : pal::leaf1);
        for (int i = 0; i < nc; i++) {
            vec3 o = rng.onSphere() * vec3(cr, cr * 0.55f, cr);
            vec3 col = lerp(base, i % 2 ? pal::leaf2 : pal::leaf3, rng.f() * 0.5f);
            clump(F, cc + o, vec3(cr * rng.range(0.6f, 0.85f), cr * rng.range(0.45f, 0.6f), cr * rng.range(0.6f, 0.85f)), col, 0.8f, rng.next());
        }
        clump(F, cc + vec3(0, cr * 0.4f, 0), vec3(cr * 0.8f, cr * 0.6f, cr * 0.8f), lerp(base, pal::leaf2, 0.4f), 1.f, rng.next());
        t.radius = r0 * 1.2f; t.height = h + cr * 2;
        break;
    }
    case P_PINE: case P_SNOWPINE: {
        float h = rng.range(6.f, 8.5f);
        S.tube({0, -0.3f, 0}, {0, h * 0.85f, 0}, 0.28f, 0.08f, Mat(pal::bark, 0.9f), 7, false);
        int layers = 5;
        for (int i = 0; i < layers; i++) {
            float y = 1.2f + i * (h - 1.6f) / layers;
            float r = (1.0f - (float)i / layers) * 2.3f + 0.45f;
            F.flat = true;
            size_t from = F.v.size();
            F.cone({0, y, 0}, {0, y + 1.9f, 0}, r, Mat(lerp(pal::pine, pal::pine2, rng.f()), 0.8f, 0, 0, 0.25f + 0.15f * i), 9);
            for (size_t q = from; q < F.v.size(); q++) {
                // droop at the rim and darker underside
                float dr = std::sqrt(F.v[q].px * F.v[q].px + F.v[q].pz * F.v[q].pz);
                F.v[q].py -= dr * 0.18f;
                if (F.v[q].ny < 0) F.v[q].a = 130;
            }
            if (k == P_SNOWPINE) {
                size_t sf = F.v.size();
                F.cone({0, y + 0.55f, 0}, {0, y + 1.95f, 0}, r * 0.78f, Mat(pal::snow, 0.6f, 0, 0, 0.2f + 0.15f * i), 9);
                for (size_t q = sf; q < F.v.size(); q++) F.v[q].py += 0.05f;
            }
            F.flat = false;
        }
        t.radius = 0.35f; t.height = h + 1.5f;
        break;
    }
    case P_DEADTREE: case P_CHARRED: case P_TWISTED: {
        vec3 col = k == P_CHARRED ? hexColor(0x1e1a18) : (k == P_TWISTED ? hexColor(0x3e3842) : hexColor(0x6b5a4a));
        std::vector<vec3> tips;
        Rng r2(var * 7 + k);
        if (k == P_TWISTED) {
            // spiral trunk
            vec3 prev{0, -0.3f, 0};
            for (int i = 1; i <= 6; i++) {
                float a = i * 0.9f;
                vec3 p{std::cos(a) * 0.35f, i * 0.75f, std::sin(a) * 0.35f};
                S.tube(prev, p, 0.42f - i * 0.04f, 0.38f - i * 0.04f, Mat(col, 0.9f), 7, false);
                prev = p;
            }
            branchTree(S, prev, normalize(vec3(0.4f, 1, 0.2f)), 1.4f, 0.16f, 1, r2, col, &tips);
            for (auto& tp : tips) if (r2.chance(0.5f)) clump(F, tp, vec3(0.5f, 0.35f, 0.5f), hexColor(0x5c4d6a), 0.9f, r2.next());
        } else {
            branchTree(S, {0, -0.3f, 0}, normalize(vec3(r2.range(-0.15f, 0.15f), 1, r2.range(-0.15f, 0.15f))), 2.6f, 0.3f, 0, r2, col, &tips);
        }
        if (k == P_CHARRED) {
            for (int i = 0; i < 5; i++) {
                vec3 p{r2.range(-0.25f, 0.25f), r2.range(0.2f, 2.2f), r2.range(-0.25f, 0.25f)};
                S.ellipsoid(p, vec3(0.1f, 0.18f, 0.1f), Mat(pal::ember, 0.6f, 0, 3.f), 5, 3);
            }
        }
        t.radius = 0.32f; t.height = 5;
        break;
    }
    case P_ELDERTREE: {
        float h = rng.range(9.f, 12.f);
        S.tube({0, -0.5f, 0}, {0, h * 0.6f, 0}, 1.25f, 0.8f, Mat(hexColor(0x4d3f36), 0.9f), 12, false);
        S.tube({0, h * 0.6f, 0}, {0.5f, h, 0.3f}, 0.8f, 0.4f, Mat(hexColor(0x4d3f36), 0.9f, 0, 0, 0.05f), 10, false);
        ringOfRoots(S, 1.4f, 7, hexColor(0x3f332b), rng);
        // moss and glowing runes on bark
        for (auto& v : S.v) if (v.ny > 60) { v.r = 70; v.g = 110; v.b = 60; }
        for (int i = 0; i < 4; i++) {
            float a = rng.range(0, TAU), y = rng.range(1.5f, 4.5f);
            S.ellipsoid({std::cos(a) * 1.15f, y, std::sin(a) * 1.15f}, vec3(0.08f, 0.25f, 0.08f), Mat(pal::rune, 0.4f, 0, 3.5f), 5, 3);
        }
        vec3 cc{0.4f, h + 1.5f, 0.2f};
        for (int i = 0; i < 11; i++) {
            vec3 o = rng.onSphere() * vec3(4.2f, 1.8f, 4.2f);
            clump(F, cc + o, vec3(rng.range(2.f, 2.8f), rng.range(1.3f, 1.8f), rng.range(2.f, 2.8f)), i % 4 == 3 ? hexColor(0x5d4f7a) : lerp(pal::elder, pal::elder2, rng.f()), 0.5f, rng.next());
        }
        // hanging glow fruits
        for (int i = 0; i < 9; i++) {
            vec3 o = rng.onSphere() * vec3(4.f, 0.5f, 4.f);
            F.ellipsoid(cc + o - vec3(0, 1.8f, 0), vec3(0.18f), Mat(pal::elderGlow, 0.3f, 0, 4.f, 0.8f), 6, 4);
        }
        t.radius = 1.3f; t.height = h + 5;
        break;
    }
    case P_GIANTSHROOM: {
        float h = rng.range(2.5f, 4.5f);
        vec3 capCol = var % 2 ? hexColor(0x6a4fd0) : hexColor(0x2fb0c8);
        S.tube({0, -0.2f, 0}, {0.2f, h, 0.1f}, 0.32f, 0.22f, Mat(hexColor(0xd8d0c0), 0.7f), 9, false);
        std::vector<vec2> prof = {{0.2f, h - 0.1f}, {1.6f, h - 0.15f}, {1.9f, h + 0.1f}, {1.6f, h + 0.55f}, {0.9f, h + 0.95f}, {0.01f, h + 1.05f}};
        S.lathe(prof, Mat(capCol, 0.5f, 0, 0.4f), 14, {0.2f, 0, 0.1f});
        // gills glowing underneath
        S.lathe({{0.05f, h - 0.05f}, {1.6f, h - 0.13f}}, Mat(lerp(capCol, vec3(1), 0.5f), 0.5f, 0, 2.5f), 14, {0.2f, 0, 0.1f}, rotY(0));
        for (int i = 0; i < 7; i++) {
            float a = rng.range(0, TAU), rr = rng.range(0.4f, 1.4f);
            S.ellipsoid({0.2f + std::cos(a) * rr, h + 0.95f - rr * 0.45f, 0.1f + std::sin(a) * rr}, vec3(0.13f, 0.07f, 0.13f), Mat(hexColor(0xf0f8ff), 0.5f, 0, 2.f), 6, 3);
        }
        t.radius = 0.35f; t.height = h + 1;
        break;
    }
    case P_WILLOW: {
        float h = 4.5f;
        S.tube({0, -0.3f, 0}, {0.3f, h, 0}, 0.45f, 0.3f, Mat(hexColor(0x4a4236), 0.9f), 8, false);
        vec3 cc{0.3f, h + 0.8f, 0};
        clump(F, cc, vec3(2.6f, 1.2f, 2.6f), hexColor(0x5c7a3a), 0.4f, rng.next());
        for (int i = 0; i < 28; i++) {
            float a = i * TAU / 28 + rng.range(-0.1f, 0.1f), r = rng.range(1.6f, 2.6f);
            vec3 root = cc + vec3(std::cos(a) * r, -0.3f, std::sin(a) * r);
            vec3 tip = root + vec3(std::cos(a) * 0.3f, -rng.range(2.f, 3.5f), std::sin(a) * 0.3f);
            F.blade(root, tip, 0.35f, {-std::sin(a), 0, std::cos(a)}, Mat(hexColor(0x5f7f3c), 0.8f, 0, 0, 0.3f), Mat(hexColor(0x7a9a4a), 0.8f, 0, 0, 1.f));
        }
        t.radius = 0.45f; t.height = h + 2;
        break;
    }
    case P_BUSH: {
        vec3 base = var % 3 == 2 ? hexColor(0x5d8a3a) : pal::leaf1;
        for (int i = 0; i < 4; i++) {
            vec3 o{rng.range(-0.5f, 0.5f), rng.range(0.3f, 0.6f), rng.range(-0.5f, 0.5f)};
            clump(F, o, vec3(rng.range(0.55f, 0.8f), rng.range(0.4f, 0.55f), rng.range(0.55f, 0.8f)), lerp(base, pal::leaf2, rng.f() * 0.6f), 0.35f, rng.next());
        }
        if (var % 3 == 1) for (int i = 0; i < 8; i++) F.ellipsoid(rng.onSphere() * vec3(0.7f, 0.4f, 0.7f) + vec3(0, 0.55f, 0), vec3(0.07f), Mat(hexColor(0xc0203a), 0.4f, 0, 0.3f, 0.3f), 5, 3);
        t.radius = 0; t.height = 1;
        break;
    }
    case P_FERN: case P_REEDS: case P_CACTUSFERN: {
        int n = k == P_REEDS ? 14 : 10;
        for (int i = 0; i < n; i++) {
            float a = i * TAU / n + rng.range(-0.2f, 0.2f);
            vec3 d{std::cos(a), 0, std::sin(a)};
            vec3 tip;
            if (k == P_REEDS) tip = d * rng.range(0.1f, 0.35f) + vec3(0, rng.range(1.2f, 1.9f), 0);
            else tip = d * rng.range(0.7f, 1.0f) + vec3(0, rng.range(0.4f, 0.7f), 0);
            vec3 c0 = k == P_REEDS ? hexColor(0x6a7a3a) : (k == P_CACTUSFERN ? hexColor(0x5a4630) : hexColor(0x3f7a32));
            vec3 c1 = k == P_REEDS ? hexColor(0xa59a5a) : (k == P_CACTUSFERN ? hexColor(0x9a6a3a) : hexColor(0x7ab84a));
            F.blade({0, 0, 0}, tip, k == P_REEDS ? 0.06f : 0.22f, {-d.z, 0, d.x}, Mat(c0, 0.8f, 0, 0, 0.f), Mat(c1, 0.8f, 0, k == P_CACTUSFERN ? 1.5f : 0, 1.f));
            if (k == P_REEDS && i % 4 == 0) F.ellipsoid(tip + vec3(0, 0.1f, 0), vec3(0.05f, 0.16f, 0.05f), Mat(hexColor(0x5a3a22), 0.9f, 0, 0, 1.f), 5, 3);
        }
        t.radius = 0; t.height = 1;
        break;
    }
    case P_ROCK: case P_BOULDER: case P_SNOWROCK: {
        float s = k == P_BOULDER ? 1.8f : 1.f;
        vec3 col = lerp(pal::stoneDark, pal::stone, rng.f());
        S.rock({0, 0.2f * s, 0}, vec3(rng.range(0.7f, 1.1f), rng.range(0.5f, 0.8f), rng.range(0.7f, 1.1f)) * s, Mat(col, 0.85f), rng.next(), 1);
        if (rng.chance(0.5f)) S.rock({0.7f * s, 0.05f, 0.4f * s}, vec3(0.4f, 0.3f, 0.35f) * s, Mat(col * 0.9f, 0.85f), rng.next(), 0);
        for (auto& v : S.v) if (v.ny > 85) {
            vec3 top = k == P_SNOWROCK ? pal::snow : pal::moss;
            Color8 c = toColor8(top, 1);
            if (k == P_SNOWROCK || rng.chance(0.6f)) { v.r = c.r; v.g = c.g; v.b = c.b; }
        }
        t.radius = 0.85f * s; t.height = 1.2f * s;
        break;
    }
    case P_ICECRYSTAL: case P_OBSIDIAN: case P_MAGICCRYSTAL: {
        S.flat = true;
        int n = rng.irange(4, 7);
        for (int i = 0; i < n; i++) {
            vec3 d = normalize(vec3(rng.range(-0.5f, 0.5f), 1, rng.range(-0.5f, 0.5f)));
            float l = rng.range(1.f, 2.4f) * (i == 0 ? 1.4f : 1.f);
            Mat m;
            if (k == P_ICECRYSTAL) m = Mat(lerp(pal::ice, vec3(1), rng.f() * 0.4f), 0.08f, 0.f, 0.5f);
            else if (k == P_OBSIDIAN) m = Mat(pal::obsidian, 0.12f, 0.f, 0.f);
            else m = Mat(var % 2 ? hexColor(0xb06cff) : hexColor(0x5ce0ff), 0.1f, 0.f, 2.2f);
            vec3 base{rng.range(-0.4f, 0.4f), -0.2f, rng.range(-0.4f, 0.4f)};
            S.cone(base, base + d * l, rng.range(0.18f, 0.35f), m, 5);
            if (k == P_OBSIDIAN && i % 2 == 0) S.cone(base + vec3(0.05f, 0, 0), base + d * l * 0.6f, 0.08f, Mat(pal::ember, 0.5f, 0, 3.f), 4);
        }
        S.flat = false;
        t.radius = 0.6f; t.height = 2.4f;
        break;
    }
    case P_GRAVESTONE: {
        vec3 col = lerp(pal::stoneDark, pal::stone, rng.f() * 0.6f);
        quat tilt = quat::euler(rng.range(-0.15f, 0.15f), 0, rng.range(-0.12f, 0.12f));
        if (var % 2 == 0) {
            S.bevelBox(tilt.rotate({0, 0.55f, 0}), {0.35f, 0.6f, 0.1f}, 0.05f, Mat(col, 0.85f), tilt);
            S.ellipsoid(tilt.rotate({0, 1.12f, 0}), vec3(0.35f, 0.18f, 0.1f), Mat(col, 0.85f), 10, 4, tilt);
        } else {
            S.box(tilt.rotate({0, 0.65f, 0}), {0.08f, 0.65f, 0.08f}, Mat(col, 0.85f), tilt);
            S.box(tilt.rotate({0, 0.95f, 0}), {0.32f, 0.08f, 0.08f}, Mat(col, 0.85f), tilt);
        }
        S.box({0, 0.03f, 0.4f}, {0.38f, 0.06f, 0.55f}, Mat(hexColor(0x3a3830), 0.95f));
        t.radius = 0.35f; t.height = 1.1f;
        break;
    }
    case P_RUINCOLUMN: {
        float h = rng.range(1.5f, 4.f);
        S.bevelBox({0, 0.25f, 0}, {0.6f, 0.25f, 0.6f}, 0.08f, Mat(pal::stoneLight, 0.8f));
        S.flat = true;
        S.lathe({{0.42f, 0.5f}, {0.4f, h * 0.5f}, {0.37f, h}}, Mat(pal::marble * 0.9f, 0.7f), 10);
        S.flat = false;
        // broken top: a few chunks
        S.rock({0.6f, 0.15f, 0.5f}, vec3(0.35f, 0.25f, 0.3f), Mat(pal::stoneLight, 0.8f), rng.next(), 0);
        for (auto& v : S.v) if (v.py > h * 0.5f && rng.chance(0.3f)) { v.r = 90; v.g = 120; v.b = 60; }
        t.radius = 0.55f; t.height = h;
        break;
    }
    case P_STUMP: {
        S.tube({0, -0.2f, 0}, {0, 0.55f, 0}, 0.45f, 0.4f, Mat(pal::bark, 0.9f), 9, true);
        for (auto& v : S.v) if (v.ny > 100) { v.r = 190; v.g = 160; v.b = 110; }
        Rng r2(var); ringOfRoots(S, 0.4f, 4, pal::barkDark, r2);
        t.radius = 0.45f; t.height = 0.5f;
        break;
    }
    case P_LOG: {
        S.tube({-1.4f, 0.3f, 0}, {1.4f, 0.32f, 0.1f}, 0.3f, 0.26f, Mat(pal::bark, 0.9f), 9, true);
        for (auto& v : S.v) if (std::fabs(v.nx) > 100) { v.r = 180; v.g = 150; v.b = 100; }
        clump(F, {0.2f, 0.55f, 0}, vec3(0.5f, 0.15f, 0.3f), pal::moss, 0.f, rng.next());
        t.radius = 0.5f; t.height = 0.6f;
        break;
    }
    case P_SMALLSHROOMS: {
        int n = rng.irange(3, 6);
        bool glow = var % 2 == 1;
        for (int i = 0; i < n; i++) {
            vec3 p{rng.range(-0.4f, 0.4f), 0, rng.range(-0.4f, 0.4f)};
            float h = rng.range(0.15f, 0.35f);
            S.tube(p, p + vec3(0, h, 0), 0.04f, 0.035f, Mat(hexColor(0xe8e0cc), 0.7f), 5, false);
            vec3 cap = glow ? hexColor(0x6ad0ff) : hexColor(0xc0302a);
            S.lathe({{0.02f, h}, {0.13f, h - 0.02f}, {0.1f, h + 0.06f}, {0.01f, h + 0.08f}}, Mat(cap, 0.5f, 0, glow ? 2.5f : 0.f), 8, p);
        }
        t.radius = 0; t.height = 0.4f;
        break;
    }
    case P_BONES: {
        Mat bm(hexColor(0xd8cfb8), 0.7f);
        for (int i = 0; i < 5; i++) {
            float z = -0.5f + i * 0.25f;
            for (int s = -1; s <= 1; s += 2) S.tube({0, 0.35f, z}, {s * 0.45f, 0.05f, z + 0.05f}, 0.05f, 0.03f, bm, 5, false);
        }
        S.tube({0, 0.35f, -0.6f}, {0, 0.32f, 0.6f}, 0.06f, 0.05f, bm, 6, false);
        S.ellipsoid({0, 0.2f, 0.9f}, vec3(0.22f, 0.18f, 0.28f), bm, 8, 6);
        t.radius = 0; t.height = 0.5f;
        break;
    }
    case P_LILYPADS: {
        for (int i = 0; i < 4; i++) {
            vec3 p{rng.range(-0.7f, 0.7f), 0, rng.range(-0.7f, 0.7f)};
            S.lathe({{0.01f, 0.0f}, {rng.range(0.25f, 0.4f), 0.0f}, {0.01f, 0.02f}}, Mat(hexColor(0x3f7a32), 0.5f), 10, p);
        }
        if (var == 1) S.ellipsoid({0, 0.08f, 0}, vec3(0.1f, 0.07f, 0.1f), Mat(hexColor(0xf3c0d8), 0.5f, 0, 0.6f), 8, 4);
        t.radius = 0; t.height = 0.1f;
        break;
    }
    case P_HAYBALE: S.tube({-0.7f, 0.6f, 0}, {0.7f, 0.6f, 0}, 0.6f, 0.6f, Mat(pal::thatch, 0.95f), 12, true); t.radius = 0.7f; t.height = 1.2f; break;
    case P_CRATE: S.bevelBox({0, 0.4f, 0}, vec3(0.4f), 0.04f, Mat(pal::woodLight, 0.85f)); S.box({0, 0.4f, 0.41f}, {0.42f, 0.06f, 0.01f}, Mat(pal::timber)); t.radius = 0.45f; t.height = 0.8f; break;
    case P_BARREL:
        S.lathe({{0.3f, 0}, {0.36f, 0.25f}, {0.38f, 0.45f}, {0.36f, 0.65f}, {0.3f, 0.9f}, {0.01f, 0.9f}}, Mat(pal::wood, 0.8f), 12);
        S.lathe({{0.37f, 0.18f}, {0.38f, 0.24f}}, Mat(pal::iron, 0.4f, 0.8f), 12);
        S.lathe({{0.37f, 0.66f}, {0.37f, 0.72f}}, Mat(pal::iron, 0.4f, 0.8f), 12);
        t.radius = 0.4f; t.height = 0.9f; break;
    // ---------------------------------------------------------------- world dressing
    case P_FLOWERS: {
        // a clump of wild flowers seen from above: daisies (0), lavender spikes (1), poppies (2).
        // Heads are flat discs facing up (2 triangles): readable from the game camera, cheap on mobile.
        static const uint32_t petals[3][3] = {{0xf6f2e4, 0xffffff, 0xf2e070}, {0x8a6ad8, 0x6a5ac0, 0xa88ae8}, {0xe0302a, 0xf0502a, 0xd02020}};
        int n = 20 + rng.irange(0, 8);
        for (int i = 0; i < n; i++) {
            vec2 o = rng.inDisk() * 0.6f;
            float h = rng.range(0.25f, 0.5f);
            vec3 base{o.x, 0, o.y}, top = base + vec3(rng.range(-0.05f, 0.05f), h, rng.range(-0.05f, 0.05f));
            F.blade(base, top, 0.03f, {1, 0, 0}, Mat(hexColor(0x3f6a2a), 0.9f, 0, 0, 0.f), Mat(hexColor(0x5a8a34), 0.9f, 0, 0, 0.8f));
            vec3 pc = hexColor(petals[var % 3][rng.irange(0, 2)]);
            Mat pm(pc, 0.6f, 0, 0.06f, 1.f);
            if (var % 3 == 1) F.blade(top - vec3(0, 0.02f, 0), top + vec3(0, 0.16f, 0), 0.06f, {1, 0, 0}, pm, pm);   // lavender spike
            else F.plane(top, vec2(rng.range(0.07f, 0.1f)), pm, quat::axisAngle({0, 1, 0}, rng.f() * TAU));
        }
        t.radius = 0; t.height = 0.5f;
        break;
    }
    case P_PEBBLES: {
        int n = rng.irange(5, 9);
        for (int i = 0; i < n; i++) {
            vec2 o = rng.inDisk() * 0.7f;
            float r = rng.range(0.05f, 0.16f);
            S.rock({o.x, r * 0.3f, o.y}, vec3(r * 1.2f, r * 0.7f, r), Mat(lerp(pal::stoneDark, pal::stoneLight, rng.f()), 0.85f), rng.next(), 0);
        }
        t.radius = 0; t.height = 0.15f;
        break;
    }
    case P_TALLGRASS: {
        int n = 16 + rng.irange(0, 8);
        vec3 c0 = hexColor(0x4a6a2a), c1 = var == 2 ? hexColor(0xc8b46a) : hexColor(0x9aa652);
        for (int i = 0; i < n; i++) {
            vec2 o = rng.inDisk() * 0.35f;
            vec3 base{o.x, 0, o.y};
            vec3 tip = base + vec3(o.x * 0.8f + rng.range(-0.15f, 0.15f), rng.range(0.6f, 1.05f), o.y * 0.8f + rng.range(-0.15f, 0.15f));
            F.blade(base, tip, 0.05f, normalize(vec3(-o.y + 0.01f, 0, o.x)), Mat(c0, 0.9f, 0, 0, 0.f), Mat(c1, 0.8f, 0, 0, 1.f));
            if (var == 2 && i % 3 == 0) F.ellipsoid(tip, vec3(0.02f, 0.07f, 0.02f), Mat(hexColor(0xd8c27a), 0.8f, 0, 0, 1.f), 4, 3);   // seed heads
        }
        t.radius = 0; t.height = 1.f;
        break;
    }
    case P_BRANCHES: {
        for (int i = 0; i < 3; i++) {
            float a = rng.f() * TAU, l = rng.range(0.6f, 1.2f);
            vec3 d{std::cos(a), 0, std::sin(a)};
            vec3 p0 = vec3(rng.range(-0.3f, 0.3f), 0.04f, rng.range(-0.3f, 0.3f));
            S.tube(p0 - d * l * 0.5f, p0 + d * l * 0.5f + vec3(0, 0.05f, 0), 0.04f, 0.015f, Mat(pal::bark, 0.9f), 5, false);
        }
        for (int i = 0; i < 6; i++) { vec2 o = rng.inDisk() * 0.6f; S.ellipsoid({o.x, 0.01f, o.y}, vec3(0.07f, 0.01f, 0.04f), Mat(rng.chance(0.5f) ? pal::leafAut : hexColor(0x8a6a3a), 0.9f), 5, 2); }
        t.radius = 0; t.height = 0.2f;
        break;
    }
    case P_FENCE: {
        Mat w(lerp(pal::wood, pal::timber, rng.f() * 0.5f), 0.9f);
        for (int s2 = -1; s2 <= 1; s2 += 2) S.bevelBox({s2 * 1.2f, 0.5f, 0}, {0.06f, 0.55f, 0.06f}, 0.015f, w);
        S.box({0, 0.75f, 0}, {1.3f, 0.04f, 0.025f}, w, quat::axisAngle({0, 0, 1}, rng.range(-0.04f, 0.04f)));
        S.box({0, 0.4f, 0}, {1.3f, 0.04f, 0.025f}, w, quat::axisAngle({0, 0, 1}, rng.range(-0.05f, 0.05f)));
        t.radius = 0; t.height = 1.1f;
        break;
    }
    case P_SIGNPOST: {
        Mat w(pal::wood, 0.9f);
        S.bevelBox({0, 1.1f, 0}, {0.06f, 1.1f, 0.06f}, 0.015f, w);
        for (int i = 0; i < 2; i++) {
            float a = (i ? 0.9f : -0.6f) + rng.range(-0.2f, 0.2f);
            quat q = quat::axisAngle({0, 1, 0}, a);
            vec3 c = q.rotate(vec3(0.45f, 0, 0)) + vec3(0, 1.75f - i * 0.32f, 0);
            S.box(c, {0.45f, 0.11f, 0.025f}, Mat(pal::woodLight, 0.85f), q);
            vec3 tip = q.rotate(vec3(0.95f, 0, 0)) + vec3(0, 1.75f - i * 0.32f, 0);
            S.triP(tip, q.rotate(vec3(0.88f, 0.11f, 0.03f)) + vec3(0, 1.75f - i * 0.32f, 0), q.rotate(vec3(0.88f, -0.11f, 0.03f)) + vec3(0, 1.75f - i * 0.32f, 0), Mat(pal::woodLight, 0.85f));
        }
        t.radius = 0.15f; t.height = 2.2f;
        break;
    }
    case P_CART: {
        Mat w(pal::wood, 0.85f), dark(pal::timber, 0.9f);
        S.bevelBox({0, 0.75f, 0}, {0.75f, 0.08f, 1.1f}, 0.02f, w);
        for (int s2 = -1; s2 <= 1; s2 += 2) {
            S.box({s2 * 0.72f, 1.0f, 0}, {0.04f, 0.2f, 1.1f}, w);
            S.lathe({{0.01f, -0.06f}, {0.48f, -0.06f}, {0.48f, 0.06f}, {0.01f, 0.06f}}, dark, 14, {s2 * 0.86f, 0.5f, -0.1f}, quat::axisAngle({0, 0, 1}, PI / 2));
            S.tube({s2 * 0.35f, 0.72f, 1.05f}, {s2 * 0.3f, 0.45f, 2.3f}, 0.04f, 0.035f, dark, 5, false);
        }
        S.box({0, 1.0f, -1.08f}, {0.72f, 0.2f, 0.04f}, w);
        if (var != 0) for (int i = 0; i < 3; i++) S.lathe({{0.22f, 0}, {0.26f, 0.2f}, {0.22f, 0.45f}, {0.01f, 0.45f}}, Mat(var == 1 ? pal::wood : pal::thatch, 0.85f), 10, {rng.range(-0.4f, 0.4f), 0.83f, rng.range(-0.7f, 0.5f)});
        t.radius = 1.0f; t.height = 1.4f;
        break;
    }
    case P_LANTERNPOST: {
        Mat w(pal::timber, 0.9f), iron(pal::iron, 0.5f, 0.8f);
        S.bevelBox({0, 1.25f, 0}, {0.07f, 1.25f, 0.07f}, 0.015f, w);
        S.box({0.28f, 2.45f, 0}, {0.3f, 0.04f, 0.04f}, w);
        S.box({0.52f, 2.15f, 0}, {0.11f, 0.15f, 0.11f}, iron);
        S.box({0.52f, 2.15f, 0}, {0.085f, 0.12f, 0.12f}, Mat(pal::window, 0.4f).nightGlow(3.f));
        S.cone({0.52f, 2.3f, 0}, {0.52f, 2.45f, 0}, 0.14f, iron, 6);
        t.radius = 0.15f; t.height = 2.6f;
        break;
    }
    case P_STANDSTONE: {
        vec3 col = lerp(pal::stoneDark, pal::stone, rng.f() * 0.6f);
        S.rock({0, 1.1f, 0}, vec3(rng.range(0.4f, 0.55f), rng.range(1.3f, 1.8f), rng.range(0.28f, 0.4f)), Mat(col, 0.85f), rng.next(), 1);
        for (auto& v : S.v) if (v.ny > 70 && rng.chance(0.7f)) { Color8 c = toColor8(pal::moss, 1); v.r = c.r; v.g = c.g; v.b = c.b; }
        if (var == 2) S.box({0, 1.3f, 0.36f}, {0.12f, 0.35f, 0.01f}, Mat(pal::rune, 0.4f, 0, 2.5f));   // faint rune
        t.radius = 0.55f; t.height = 3.f;
        break;
    }
    case P_CAMPFIRE: {
        for (int i = 0; i < 9; i++) { float a = i * TAU / 9; S.rock({std::cos(a) * 0.55f, 0.08f, std::sin(a) * 0.55f}, vec3(0.16f, 0.12f, 0.14f), Mat(pal::stoneDark, 0.9f), rng.next(), 0); }
        for (int i = 0; i < 4; i++) { float a = i * PI / 2 + 0.3f; S.tube({std::cos(a) * 0.4f, 0.05f, std::sin(a) * 0.4f}, {std::cos(a) * 0.05f, 0.35f, std::sin(a) * 0.05f}, 0.06f, 0.04f, Mat(pal::bark, 0.9f), 6, false); }
        S.ellipsoid({0, 0.04f, 0}, vec3(0.3f, 0.05f, 0.3f), Mat(pal::ember, 0.6f, 0, 2.5f));
        t.radius = 0.6f; t.height = 0.5f;
        break;
    }
    case P_TENT: {
        // A-frame canvas tent with poles
        vec3 cloth = var == 0 ? hexColor(0xc8b88a) : (var == 1 ? hexColor(0x8a3a2a) : hexColor(0x3a5a6a));
        Mat cm(cloth, 0.95f), pole(pal::timber, 0.9f);
        float L = 1.3f, W = 1.1f, Ht = 1.5f;
        vec3 a0{-W, 0, -L}, a1{W, 0, -L}, b0{-W, 0, L}, b1{W, 0, L}, r0{0, Ht, -L}, r1{0, Ht, L};
        S.quadP(a0, b0, r1, r0, cm); S.quadP(b1, a1, r0, r1, cm);
        S.quadP(r0, r1, b0, a0, cm); S.quadP(r1, r0, a1, b1, cm);   // inner faces
        S.triP(a1, a0, r0, cm);                                     // back wall; front stays open
        S.tube({0, 0, L + 0.05f}, {0, Ht + 0.2f, L + 0.05f}, 0.035f, 0.03f, pole, 5, false);
        S.tube({0, 0, -L - 0.05f}, {0, Ht + 0.2f, -L - 0.05f}, 0.035f, 0.03f, pole, 5, false);
        t.radius = 1.2f; t.height = 1.7f;
        break;
    }
    default: break;
    }
    S.bakeHeightAO(-0.2f, 1.2f, 0.65f);
}

int propVariants(PropKind k) {
    switch (k) {
    case P_OAK: case P_BIRCH: case P_PINE: case P_SNOWPINE: return 4;
    case P_HAYBALE: case P_CRATE: case P_BARREL: return 1;
    case P_SIGNPOST: case P_LANTERNPOST: case P_CAMPFIRE: return 2;
    default: return 3;
    }
}

void initPropTemplates() {
    std::call_once(gOnce, [] {
        gTemplates.resize(P_COUNT);
        for (int k = 0; k < P_COUNT; k++) {
            gTemplates[k].resize(propVariants((PropKind)k));
            for (int v = 0; v < (int)gTemplates[k].size(); v++) buildProp((PropKind)k, v, gTemplates[k][v]);
        }
        gLiteBuild = true;
        gTemplatesLite.resize(P_COUNT);
        for (int k = 0; k < P_COUNT; k++) {
            gTemplatesLite[k].resize(propVariants((PropKind)k));
            for (int v = 0; v < (int)gTemplatesLite[k].size(); v++) buildProp((PropKind)k, v, gTemplatesLite[k][v]);
        }
        gLiteBuild = false;
    });
}

const PropTemplate& propTemplate(PropKind k, int v, bool lite) { auto& T = lite ? gTemplatesLite : gTemplates; return T[k][v % T[k].size()]; }

static void appendXf(MeshBuilder& dst, const MeshBuilder& src, const mat4& m, vec3 tint) {
    uint32_t base = (uint32_t)dst.v.size();
    dst.v.reserve(dst.v.size() + src.v.size());
    for (Vertex v : src.v) {
        vec3 p = m.transformPoint({v.px, v.py, v.pz});
        vec3 n = normalize(m.transformDir(unpackNormal(v)));
        v.px = p.x; v.py = p.y; v.pz = p.z;
        v.nx = (int8_t)(n.x * 127); v.ny = (int8_t)(n.y * 127); v.nz = (int8_t)(n.z * 127);
        v.r = (uint8_t)clampf(v.r * tint.x, 0, 255); v.g = (uint8_t)clampf(v.g * tint.y, 0, 255); v.b = (uint8_t)clampf(v.b * tint.z, 0, 255);
        dst.v.push_back(v);
    }
    for (uint32_t i : src.idx) dst.idx.push_back(base + i);
}

void stampProp(ChunkBuild& out, PropKind k, int var, vec3 pos, float rotY_, float scale, vec3 tint, bool collide) {
    const PropTemplate& t = propTemplate(k, var, out.liteProps);
    mat4 m = mat4::trs(pos, rotY(rotY_), vec3(scale));
    if (!t.solid.v.empty()) appendXf(out.props, t.solid, m, tint);
    if (!t.foliage.v.empty()) appendXf(out.foliage, t.foliage, m, tint);
    if (collide && t.radius > 0) {
        Collider c; c.c = pos.xz(); c.r = t.radius * scale; c.top = t.height * scale; c.base = pos.y;
        out.colliders.push_back(c);
    }
}

// ------------------------------------------------------------------------------------------ structures
static void house(MeshBuilder& M, MeshBuilder& Fo, int variant, int region, bool inn) {
    Rng rng(variant * 13 + region);
    float w = inn ? 8.f : rng.range(5.f, 6.5f), l = inn ? 10.f : rng.range(6.f, 8.f), h = inn ? 5.2f : 3.0f;
    Mat found(pal::stoneDark, 0.9f), wall(lerp(pal::plaster, hexColor(0xd8c7a3), rng.f()), 0.9f), beam(pal::timber, 0.85f);
    Mat roof = variant % 2 ? Mat(pal::tile, 0.75f) : Mat(pal::thatch, 0.95f);
    M.bevelBox({0, -0.3f, 0}, {w / 2 + 0.2f, 0.8f, l / 2 + 0.2f}, 0.1f, found);
    M.box({0, 0.5f + h / 2, 0}, {w / 2, h / 2, l / 2}, wall);
    // timber framing
    for (float x : {-w / 2, w / 2}) for (float z : {-l / 2, l / 2}) M.box({x, 0.5f + h / 2, z}, {0.14f, h / 2 + 0.02f, 0.14f}, beam);
    for (float y : {0.6f, 0.5f + h}) { M.box({0, y, l / 2}, {w / 2 + 0.05f, 0.1f, 0.12f}, beam); M.box({0, y, -l / 2}, {w / 2 + 0.05f, 0.1f, 0.12f}, beam); M.box({w / 2, y, 0}, {0.12f, 0.1f, l / 2}, beam); M.box({-w / 2, y, 0}, {0.12f, 0.1f, l / 2}, beam); }
    for (int i = 0; i < 2; i++) {
        float s = i ? 1.f : -1.f;
        M.box({s * w / 4, 0.5f + h / 2, l / 2 + 0.02f}, {0.08f, h / 2, 0.1f}, beam);
        M.box({w / 2 + 0.02f, 0.5f + h / 2, s * l / 4}, {0.1f, h / 2, 0.08f}, beam);
        M.box({-w / 2 - 0.02f, 0.5f + h / 2, s * l / 4}, {0.1f, h / 2, 0.08f}, beam);
    }
    if (inn) M.box({0, 0.5f + h / 2, l / 2 + 0.03f}, {w / 2, 0.1f, 0.1f}, beam);
    // door & windows (glow at night)
    M.box({0, 1.5f, l / 2 + 0.06f}, {0.55f, 1.0f, 0.05f}, Mat(hexColor(0x5a3a22), 0.8f));
    M.box({0.3f, 1.5f, l / 2 + 0.12f}, {0.05f, 0.05f, 0.04f}, Mat(pal::iron, 0.3f, 1.f));
    Mat win = Mat(pal::window * 0.45f, 0.3f).nightGlow(3.5f);
    for (int i = 0; i < 2; i++) {
        float s = i ? 1.f : -1.f;
        M.box({s * (w / 2 + 0.04f), 2.1f, 0}, {0.03f, 0.45f, 0.4f}, win);
        M.box({s * (w / 2 + 0.07f), 2.1f, 0}, {0.03f, 0.5f, 0.06f}, beam);
        M.box({s * w * 0.3f, 2.2f, l / 2 + 0.04f}, {0.35f, 0.4f, 0.03f}, win);
        if (inn) M.box({s * w * 0.3f, 4.4f, l / 2 + 0.04f}, {0.35f, 0.4f, 0.03f}, win);
        M.box({s * (w / 2 + 0.12f), 2.1f, 0.5f}, {0.04f, 0.5f, 0.1f}, Mat(hexColor(0x3f5a3a), 0.8f));  // shutters
    }
    float ry = 0.5f + h;
    gableRoof(M, w, l, ry, inn ? 3.2f : 2.4f, 0.55f, roof);
    // chimney
    M.bevelBox({w * 0.25f, ry + 1.6f, -l * 0.25f}, {0.38f, 1.4f, 0.38f}, 0.05f, Mat(pal::stoneDark, 0.9f));
    if (inn) {
        // hanging sign
        M.box({w / 2 + 0.6f, 3.1f, l / 2 - 0.3f}, {0.6f, 0.05f, 0.05f}, beam);
        M.box({w / 2 + 0.9f, 2.6f, l / 2 - 0.3f}, {0.04f, 0.38f, 0.48f}, Mat(pal::cloth3, 0.7f));
    }
    // flower boxes & barrels
    Fo.ellipsoid({w * 0.3f, 1.65f, l / 2 + 0.3f}, vec3(0.38f, 0.15f, 0.12f), Mat(hexColor(0xd94b5f), 0.7f, 0, 0, 0.3f), 8, 4);
    M.bakeHeightAO(-0.2f, 2.f, 0.7f);
}

static void stoneHouse(MeshBuilder& M, int variant, int region) {
    Rng rng(variant * 17 + region);
    float w = rng.range(5.f, 6.5f), l = rng.range(6.f, 7.5f), h = 3.2f;
    vec3 sc = region == R_ASHEN ? hexColor(0x5a4f4a) : (region == R_HIGHCROWNS ? hexColor(0xa49c8e) : pal::stone);
    M.jitter = 0.08f; M.jitterSeed = variant + 5;
    M.bevelBox({0, h / 2 - 0.3f, 0}, {w / 2, h / 2 + 0.6f, l / 2}, 0.12f, Mat(sc, 0.9f));
    M.jitter = 0;
    for (int i = 0; i < 4; i++) {
        float y = 0.3f + i * 0.8f;
        M.box({w / 2 + 0.02f, y, 0}, {0.02f, 0.02f, l / 2}, Mat(sc * 0.7f, 0.9f));
        M.box({-w / 2 - 0.02f, y, 0}, {0.02f, 0.02f, l / 2}, Mat(sc * 0.7f, 0.9f));
    }
    M.box({0, 1.3f, l / 2 + 0.04f}, {0.6f, 1.0f, 0.05f}, Mat(hexColor(0x4a3020), 0.8f));
    M.box({0, 2.4f, l / 2 + 0.06f}, {0.75f, 0.12f, 0.08f}, Mat(sc * 0.8f, 0.9f));
    Mat win = Mat(pal::window * 0.45f, 0.3f).nightGlow(3.2f);
    for (int s = -1; s <= 1; s += 2) M.box({s * (w / 2 + 0.03f), 1.9f, 0}, {0.03f, 0.45f, 0.35f}, win);
    gableRoof(M, w, l, h + 0.3f, 2.2f, 0.45f, Mat(region == R_ASHEN ? hexColor(0x3a3230) : pal::slate, 0.65f));
    M.bevelBox({-w * 0.25f, h + 1.9f, l * 0.2f}, {0.35f, 1.2f, 0.35f}, 0.05f, Mat(sc * 0.8f, 0.9f));
    M.bakeHeightAO(-0.2f, 2.f, 0.7f);
}

static void longhouse(MeshBuilder& M, int variant) {
    float w = 6.f, l = 13.f, h = 2.4f;
    Mat logs(hexColor(0x6a4a30), 0.9f), roof(hexColor(0x5a4a3a), 0.9f);
    for (int i = 0; i < 6; i++) {
        float y = 0.2f + i * 0.4f;
        M.tube({-w / 2, y, -l / 2}, {-w / 2, y, l / 2}, 0.22f, 0.22f, logs, 6, true);
        M.tube({w / 2, y, -l / 2}, {w / 2, y, l / 2}, 0.22f, 0.22f, logs, 6, true);
        M.tube({-w / 2, y, l / 2}, {w / 2, y, l / 2}, 0.21f, 0.21f, logs, 6, true);
        M.tube({-w / 2, y, -l / 2}, {w / 2, y, -l / 2}, 0.21f, 0.21f, logs, 6, true);
    }
    M.box({0, 1.2f, l / 2 + 0.25f}, {0.7f, 1.1f, 0.05f}, Mat(hexColor(0x3a2818), 0.8f));
    M.box({0, 1.6f, -l / 2 - 0.25f}, {0.4f, 0.35f, 0.04f}, Mat(pal::window * 0.45f, 0.3f).nightGlow(3.2f));
    gableRoof(M, w, l, h + 0.2f, 3.4f, 0.7f, roof);
    // snow on the roof
    M.v.size();
    size_t from = M.v.size();
    gableRoof(M, w * 0.92f, l * 0.98f, h + 0.42f, 3.3f, 0.5f, Mat(pal::snow, 0.6f));
    (void)from;
    // crossed gable beams with carved heads
    for (int s = -1; s <= 1; s += 2) {
        M.tube({-1.2f, h + 2.9f, s * (l / 2 + 0.7f)}, {0.6f, h + 4.6f, s * (l / 2 + 0.75f)}, 0.1f, 0.08f, Mat(hexColor(0x3a2818)), 5, true);
        M.tube({1.2f, h + 2.9f, s * (l / 2 + 0.7f)}, {-0.6f, h + 4.6f, s * (l / 2 + 0.75f)}, 0.1f, 0.08f, Mat(hexColor(0x3a2818)), 5, true);
    }
    M.bakeHeightAO(-0.2f, 2.f, 0.7f);
    (void)variant;
}

static void stiltHut(MeshBuilder& M, int variant) {
    Mat post(hexColor(0x3e3226), 0.9f), plank(hexColor(0x5a4a3a), 0.9f), roof(hexColor(0x4e4a32), 0.95f);
    for (int i = -1; i <= 1; i += 2) for (int j = -1; j <= 1; j += 2) M.tube({i * 2.2f, -1.2f, j * 2.2f}, {i * 2.2f, 1.6f, j * 2.2f}, 0.15f, 0.12f, post, 6, false);
    M.box({0, 1.4f, 0}, {2.6f, 0.12f, 2.6f}, plank);
    M.box({0, 2.6f, 0}, {2.0f, 1.1f, 2.0f}, Mat(hexColor(0x6a5a44), 0.9f));
    M.box({0, 2.4f, 2.03f}, {0.45f, 0.85f, 0.04f}, Mat(hexColor(0x2a2018)));
    M.box({1.3f, 2.9f, 2.03f}, {0.3f, 0.3f, 0.03f}, Mat(hexColor(0x7aff9a) * 0.4f, 0.3f).nightGlow(3.f));  // eerie green lamplight
    gableRoof(M, 4.f, 4.f, 3.7f, 1.8f, 0.6f, roof);
    // ladder
    for (int i = 0; i < 5; i++) M.box({0, 0.2f + i * 0.3f, 3.0f}, {0.4f, 0.03f, 0.04f}, plank);
    M.bakeHeightAO(-0.5f, 2.5f, 0.6f);
    (void)variant;
}

static void tower(MeshBuilder& M, int variant, int region) {
    float h = variant == 1 ? 16.f : 11.f, r = 2.6f;
    vec3 sc = region == R_FROSTHEIM ? hexColor(0x9aa4ad) : pal::stone;
    M.jitter = 0.07f; M.jitterSeed = 99;
    M.lathe({{r + 0.3f, -1.f}, {r + 0.25f, 1.f}, {r, 1.4f}, {r, h}, {r + 0.35f, h + 0.3f}, {r + 0.35f, h + 0.9f}, {0.01f, h + 0.9f}}, Mat(sc, 0.9f), 14);
    M.jitter = 0;
    for (int i = 0; i < 4; i++) {
        float a = i * TAU / 4 + 0.4f;
        vec3 d{std::cos(a), 0, std::sin(a)};
        M.box(d * (r + 0.02f) + vec3(0, h * 0.55f + (i % 2) * 2.f, 0), {0.35f, 0.55f, 0.35f}, Mat(pal::window * 0.4f, 0.3f).nightGlow(3.f), quat::axisAngle({0, 1, 0}, -a));
    }
    M.box({0, 1.2f, r + 0.05f}, {0.6f, 1.1f, 0.12f}, Mat(hexColor(0x4a3020)));
    if (variant == 1) {
        M.ellipsoid({0, h + 0.9f, 0}, vec3(r * 0.9f, r * 0.8f, r * 0.9f), Mat(hexColor(0x3c5a7a), 0.35f, 0.6f), 14, 8);
        M.cone({0, h + 2.4f, 0}, {1.6f, h + 4.0f, 0}, 0.25f, Mat(pal::gold, 0.3f, 1.f), 8);  // telescope
    } else {
        M.flat = true;
        M.cone({0, h + 0.9f, 0}, {0, h + 5.f, 0}, r + 0.7f, Mat(pal::slate, 0.6f), 12);
        M.flat = false;
    }
}

static void windmill(MeshBuilder& M) {
    M.jitter = 0.05f;
    M.lathe({{3.f, -1.f}, {2.9f, 0.5f}, {2.1f, 8.5f}, {1.9f, 9.f}}, Mat(pal::plaster, 0.9f), 12);
    M.jitter = 0;
    M.flat = true;
    M.cone({0, 9.f, 0}, {0, 12.f, 0}, 2.5f, Mat(pal::thatch, 0.95f), 12);
    M.flat = false;
    M.box({0, 1.2f, 2.9f}, {0.6f, 1.1f, 0.1f}, Mat(hexColor(0x5a3a22)));
    M.box({0, 5.f, 2.55f}, {0.4f, 0.5f, 0.06f}, Mat(pal::window * 0.4f).nightGlow(3.f));
    M.tube({0, 9.6f, 1.5f}, {0, 9.6f, 3.0f}, 0.25f, 0.2f, Mat(pal::timber), 8, true);  // hub axle
}

void buildWindmillBlades(MeshBuilder& M) {
    Mat frame(pal::timber, 0.85f), sail(hexColor(0xe8dcc0), 0.95f, 0, 0, 0.f);
    for (int i = 0; i < 4; i++) {
        float a = i * TAU / 4;
        vec3 d{std::cos(a), std::sin(a), 0};
        vec3 s{-std::sin(a), std::cos(a), 0};
        M.box(d * 3.3f, {0.12f, 0.12f, 0.08f}, frame);
        M.tube({0, 0, 0}, d * 6.5f, 0.12f, 0.08f, frame, 5, false);
        vec3 a0 = d * 1.2f, a1 = d * 6.4f;
        M.quadP(a0, a1, a1 + s * 1.3f, a0 + s * 1.3f, sail);
        M.quadP(a0 + s * 1.3f, a1 + s * 1.3f, a1, a0, sail);
    }
    M.ellipsoid({0, 0, 0}, vec3(0.45f), Mat(pal::timber), 8, 6);
}

static void well(MeshBuilder& M) {
    M.jitter = 0.08f;
    M.lathe({{1.1f, -0.3f}, {1.15f, 0.9f}, {0.9f, 0.9f}, {0.85f, -0.2f}}, Mat(pal::stone, 0.9f), 12);
    M.jitter = 0;
    M.lathe({{0.85f, 0.4f}, {0.01f, 0.4f}}, Mat(hexColor(0x1a3040), 0.1f), 12);
    for (int s = -1; s <= 1; s += 2) M.box({s * 0.95f, 1.8f, 0}, {0.09f, 1.0f, 0.09f}, Mat(pal::timber));
    gableRoof(M, 1.6f, 1.6f, 2.7f, 0.9f, 0.3f, Mat(pal::tile, 0.8f));
    M.tube({-0.9f, 2.3f, 0}, {0.9f, 2.3f, 0}, 0.07f, 0.07f, Mat(pal::wood), 6, true);
    M.lathe({{0.18f, 1.5f}, {0.2f, 1.8f}, {0.01f, 1.8f}}, Mat(pal::wood, 0.8f), 8);
}

static void stall(MeshBuilder& M, MeshBuilder& Fo, int variant) {
    Mat post(pal::timber), table(pal::woodLight, 0.85f);
    vec3 cc = variant % 3 == 0 ? pal::cloth1 : (variant % 3 == 1 ? pal::cloth2 : pal::cloth3);
    for (int i = -1; i <= 1; i += 2) for (int j = -1; j <= 1; j += 2) M.box({i * 1.4f, 1.2f, j * 0.9f}, {0.07f, 1.2f, 0.07f}, post);
    M.box({0, 0.85f, 0}, {1.5f, 0.06f, 0.9f}, table);
    // striped awning, slightly sagging
    for (int i = 0; i < 6; i++) {
        float x0 = -1.6f + i * 3.2f / 6, x1 = x0 + 3.2f / 6;
        Mat m(i % 2 ? cc : hexColor(0xeee2c6), 0.9f, 0, 0, 0.15f);
        Fo.quadP({x0, 2.5f, -1.1f}, {x0, 2.2f, 1.2f}, {x1, 2.2f, 1.2f}, {x1, 2.5f, -1.1f}, m);
    }
    Rng rng(variant);
    for (int i = 0; i < 9; i++) {
        vec3 c{rng.range(-1.2f, 1.2f), 0.98f, rng.range(-0.6f, 0.6f)};
        static const uint32_t goods[] = {0xd94b2a, 0xe8b23a, 0x6aa83a, 0x8a4a8a, 0xc87a3a};
        M.ellipsoid(c, vec3(0.1f), Mat(hexColor(goods[rng.irange(0, 4)]), 0.5f), 6, 4);
    }
    M.bevelBox({-1.0f, 0.3f, -0.6f}, vec3(0.3f), 0.03f, Mat(pal::woodLight));
}

static void field(MeshBuilder& M, MeshBuilder& Fo, int variant) {
    Mat soil(hexColor(0x4a3524), 0.95f);
    for (int r = 0; r < 7; r++) {
        float z = -7.f + r * 2.2f;
        M.box({0, 0.0f, z}, {9.f, 0.18f, 0.45f}, soil);
        Rng rng(variant * 100 + r);
        for (int i = 0; i < 18; i++) {
            float x = -8.5f + i * 1.0f + rng.range(-0.15f, 0.15f);
            vec3 p{x, 0.15f, z};
            if (variant % 3 == 0) {  // wheat
                for (int k = 0; k < 3; k++) {
                    float a = k * 2.1f + rng.f();
                    Fo.blade(p + vec3(std::cos(a) * 0.15f, 0, std::sin(a) * 0.15f), p + vec3(std::cos(a) * 0.25f, 1.1f, std::sin(a) * 0.2f), 0.1f, {-std::sin(a), 0, std::cos(a)},
                             Mat(hexColor(0x9a8a3a), 0.9f, 0, 0, 0.f), Mat(hexColor(0xe8c860), 0.8f, 0, 0, 1.f));
                }
            } else if (variant % 3 == 1) {  // cabbages
                Fo.ellipsoid(p + vec3(0, 0.15f, 0), vec3(0.28f, 0.22f, 0.28f), Mat(hexColor(0x7ab84a), 0.7f, 0, 0, 0.15f), 7, 5);
            } else if (i % 2 == 0) {  // pumpkins
                M.ellipsoid(p + vec3(0, 0.2f, 0), vec3(0.32f, 0.25f, 0.32f), Mat(hexColor(0xe07a2a), 0.6f), 9, 6);
                Fo.blade(p, p + vec3(0.4f, 0.3f, 0.2f), 0.3f, {0, 0, 1}, Mat(hexColor(0x3f7a32)), Mat(hexColor(0x5a9a3a), 0.8f, 0, 0, 0.6f));
            }
        }
    }
    // fence around
    for (int i = -4; i <= 4; i++) {
        M.box({i * 2.2f, 0.5f, 8.2f}, {0.06f, 0.5f, 0.06f}, Mat(pal::wood));
        M.box({i * 2.2f, 0.5f, -8.6f}, {0.06f, 0.5f, 0.06f}, Mat(pal::wood));
    }
    M.box({0, 0.7f, 8.2f}, {9.f, 0.05f, 0.04f}, Mat(pal::wood));
    M.box({0, 0.7f, -8.6f}, {9.f, 0.05f, 0.04f}, Mat(pal::wood));
}

static vec3 castleStone(int variant) {
    return variant == 1 ? hexColor(0x4a4650) : (variant == 2 ? hexColor(0xa8b2ba) : hexColor(0x9a948a));
}

static void crenellations(MeshBuilder& M, float x0, float x1, float y, float z, float depth, const Mat& m) {
    int n = (int)((x1 - x0) / 1.0f);
    for (int i = 0; i < n; i += 2) M.box({x0 + (i + 0.5f) * (x1 - x0) / n, y + 0.4f, z}, {(x1 - x0) / n * 0.5f, 0.4f, depth}, m);
}

static void castleWall(MeshBuilder& M, int variant, float lengthScale) {
    // built in unit length (10 m) then scaled by the structure scale along X only through repetition
    (void)lengthScale;
    Mat s(castleStone(variant), 0.9f);
    M.jitter = 0.07f; M.jitterSeed = variant * 3 + 1;
    M.box({0, 3.f, 0}, {5.2f, 4.2f, 1.1f}, s);
    M.box({0, 7.3f, 0}, {5.2f, 0.15f, 1.3f}, Mat(castleStone(variant) * 0.85f, 0.9f));
    crenellations(M, -5.2f, 5.2f, 7.45f, 0.95f, 0.25f, s);
    crenellations(M, -5.2f, 5.2f, 7.45f, -0.95f, 0.25f, s);
    M.jitter = 0;
    if (variant == 1) {  // ruined: a breach
        M.v.size();
    }
}

static void castleTower(MeshBuilder& M, int variant) {
    Mat s(castleStone(variant), 0.9f);
    M.jitter = 0.07f; M.jitterSeed = variant + 77;
    M.lathe({{3.4f, -1.f}, {3.2f, 1.f}, {3.f, 9.f}, {3.5f, 9.4f}, {3.5f, 10.f}, {3.0f, 10.f}, {3.0f, 9.5f}, {0.01f, 9.5f}}, s, 14);
    M.jitter = 0;
    for (int i = 0; i < 7; i++) {
        float a = i * TAU / 7;
        M.box({std::cos(a) * 3.3f, 10.4f, std::sin(a) * 3.3f}, {0.45f, 0.45f, 0.3f}, s, quat::axisAngle({0, 1, 0}, -a + PI / 2));
    }
    for (int i = 0; i < 3; i++) {
        float a = i * 2.2f;
        M.box({std::cos(a) * 3.02f, 4.5f + i * 1.5f, std::sin(a) * 3.02f}, {0.12f, 0.5f, 0.12f}, Mat(pal::window * 0.35f, 0.3f).nightGlow(3.f), quat::axisAngle({0, 1, 0}, -a));
    }
    if (variant != 1) {
        M.flat = true;
        M.cone({0, 9.5f, 0}, {0, 15.f, 0}, 3.2f, Mat(variant == 2 ? hexColor(0x3c5468) : hexColor(0x6a2a26), 0.6f), 12);
        M.flat = false;
        M.tube({0, 15.f, 0}, {0, 17.f, 0}, 0.05f, 0.05f, Mat(pal::iron, 0.4f, 1.f), 4, false);
    }
}

static void keep(MeshBuilder& M, int variant) {
    Mat s(castleStone(variant), 0.9f);
    M.jitter = 0.06f; M.jitterSeed = 31;
    M.box({0, 6.f, 0}, {9.f, 7.f, 7.f}, s);
    M.jitter = 0;
    crenellations(M, -9.f, 9.f, 13.f, 6.8f, 0.3f, s);
    crenellations(M, -9.f, 9.f, 13.f, -6.8f, 0.3f, s);
    for (int i = -1; i <= 1; i += 2) for (int j = -1; j <= 1; j += 2) {
        M.lathe({{2.2f, -1.f}, {2.2f, 15.f}, {2.6f, 15.4f}, {0.01f, 15.4f}}, s, 10, {i * 9.f, 0, j * 7.f});
        M.flat = true;
        M.cone({i * 9.f, 15.4f, j * 7.f}, {i * 9.f, 20.f, j * 7.f}, 2.6f, Mat(hexColor(0x6a2a26), 0.6f), 10);
        M.flat = false;
    }
    M.box({0, 2.4f, 7.05f}, {1.6f, 2.4f, 0.1f}, Mat(hexColor(0x3a2818)));
    for (int i = -3; i <= 3; i++) M.box({i * 2.4f, 8.f, 7.05f}, {0.25f, 0.7f, 0.05f}, Mat(pal::window * 0.4f).nightGlow(3.f));
    // banners
    for (int i = -1; i <= 1; i += 2) M.box({i * 4.f, 9.f, 7.08f}, {0.8f, 2.2f, 0.03f}, Mat(pal::cloth2, 0.8f));
}

static void gate(MeshBuilder& M, int variant) {
    Mat s(castleStone(variant), 0.9f);
    M.box({-3.7f, 3.f, 0}, {1.5f, 4.2f, 1.3f}, s);
    M.box({3.7f, 3.f, 0}, {1.5f, 4.2f, 1.3f}, s);
    M.box({0, 6.3f, 0}, {2.3f, 1.1f, 1.3f}, s);
    crenellations(M, -5.2f, 5.2f, 7.4f, 1.1f, 0.3f, s);
    for (int i = -2; i <= 2; i++) M.box({i * 0.45f, 6.f, 0.9f}, {0.05f, 0.6f, 0.05f}, Mat(pal::iron, 0.4f, 1.f));  // raised portcullis
    M.lathe({{0.3f, 6.6f}, {0.3f, 8.2f}, {0.01f, 8.3f}}, Mat(pal::gold, 0.3f, 1.f), 6, {0, 0, 1.35f});
}

static void chapel(MeshBuilder& M, int variant) {
    Mat s(variant == 1 ? hexColor(0x6a6660) : pal::stoneLight, 0.9f);
    M.jitter = 0.06f;
    M.box({0, 2.6f, 0}, {3.5f, 3.4f, 6.f}, s);
    M.jitter = 0;
    gableRoof(M, 7.f, 12.f, 6.f, 3.f, 0.4f, Mat(pal::slate, 0.6f));
    M.lathe({{1.4f, 5.f}, {1.4f, 11.f}, {0.01f, 11.f}}, s, 4, {0, 0, 5.f}, quat::axisAngle({0, 1, 0}, PI / 4));
    M.flat = true;
    M.cone({0, 11.f, 5.f}, {0, 15.f, 5.f}, 1.6f, Mat(pal::slate, 0.6f), 4);
    M.flat = false;
    M.box({0, 15.6f, 5.f}, {0.06f, 0.6f, 0.06f}, Mat(pal::gold, 0.3f, 1.f));
    M.box({0, 15.8f, 5.f}, {0.3f, 0.06f, 0.06f}, Mat(pal::gold, 0.3f, 1.f));
    // stained glass rose window
    M.lathe({{1.1f, 0}, {0.01f, 0.f}}, Mat(hexColor(0xd060a0) * 0.5f, 0.2f).nightGlow(4.f), 12, {0, 7.f, 6.06f}, quat::axisAngle({1, 0, 0}, PI / 2));
    for (int i = -2; i <= 2; i++) M.box({3.53f, 3.f, i * 2.2f}, {0.03f, 1.1f, 0.35f}, Mat(hexColor(0x6080ff) * 0.4f).nightGlow(3.f));
    M.box({0, 1.6f, 6.05f}, {0.9f, 1.6f, 0.05f}, Mat(hexColor(0x3a2818)));
}

static void ruinWall(MeshBuilder& M, int variant, Rng& rng) {
    Mat s(variant == 1 ? pal::marble * 0.85f : pal::stone, 0.85f);
    int n = 6;
    for (int i = 0; i < n; i++) {
        float h = rng.range(0.6f, 3.5f) * (i == 0 || i == n - 1 ? 0.5f : 1.f);
        M.bevelBox({-3.f + i * 1.2f, h / 2 - 0.3f, 0}, {0.6f, h / 2 + 0.3f, 0.45f}, 0.08f, s);
    }
    for (int i = 0; i < 3; i++) M.rock({rng.range(-3, 3), 0, rng.range(0.8f, 1.6f)}, vec3(0.4f, 0.25f, 0.35f), s, rng.next(), 0);
    for (auto& v : M.v) if (v.ny > 90 && rng.chance(0.7f)) { v.r = 80; v.g = 110; v.b = 55; }
}

static void elvenArch(MeshBuilder& M, int variant) {
    Mat m(pal::marble, 0.4f, 0.f), r(pal::rune, 0.3f, 0, 3.5f);
    // two slender curved pillars meeting in a pointed arch
    std::vector<vec3> L, R;
    for (int i = 0; i <= 10; i++) {
        float t = i / 10.f;
        float x = 2.2f * (1 - t * t * 0.9f), y = t * 6.f;
        L.push_back({-x, y, 0}); R.push_back({x, y, 0});
    }
    for (int i = 0; i < 10; i++) {
        M.tube(L[i], L[i + 1], 0.32f - i * 0.02f, 0.3f - i * 0.02f, m, 8, false);
        M.tube(R[i], R[i + 1], 0.32f - i * 0.02f, 0.3f - i * 0.02f, m, 8, false);
    }
    M.ellipsoid({0, 6.3f, 0}, vec3(0.45f), r, 8, 6);
    for (int i = 1; i < 8; i += 2) { M.ellipsoid(L[i] + vec3(0, 0, 0.3f), vec3(0.08f, 0.2f, 0.05f), r, 5, 3); M.ellipsoid(R[i] + vec3(0, 0, 0.3f), vec3(0.08f, 0.2f, 0.05f), r, 5, 3); }
    M.bevelBox({-2.2f, 0.1f, 0}, {0.6f, 0.4f, 0.6f}, 0.08f, m);
    M.bevelBox({2.2f, 0.1f, 0}, {0.6f, 0.4f, 0.6f}, 0.08f, m);
    (void)variant;
}

static void obelisk(MeshBuilder& M, int variant) {
    Mat s(variant == 1 ? pal::marble : hexColor(0x6e6a70), 0.6f);
    M.bevelBox({0, 0.3f, 0}, {0.9f, 0.4f, 0.9f}, 0.1f, s);
    M.flat = true;
    M.lathe({{0.55f, 0.7f}, {0.4f, 5.f}, {0.01f, 5.8f}}, s, 4, {}, quat::axisAngle({0, 1, 0}, PI / 4));
    M.flat = false;
    for (int i = 0; i < 4; i++) M.box({0, 1.5f + i * 0.9f, 0.5f - i * 0.03f}, {0.12f, 0.2f, 0.02f}, Mat(variant == 1 ? pal::rune : hexColor(0xff5a3a), 0.3f, 0, 3.f));
}

static void crypt(MeshBuilder& M) {
    Mat s(hexColor(0x6a6670), 0.85f);
    M.jitter = 0.07f;
    M.box({0, 1.8f, 0}, {4.f, 2.6f, 4.5f}, s);
    M.jitter = 0;
    gableRoof(M, 8.f, 9.f, 4.4f, 2.f, 0.3f, Mat(hexColor(0x3e3c44), 0.7f));
    for (int i = -1; i <= 1; i += 2) M.lathe({{0.4f, 0}, {0.35f, 4.2f}, {0.5f, 4.4f}, {0.01f, 4.4f}}, Mat(pal::stoneLight, 0.8f), 8, {i * 2.2f, 0, 5.f});
    M.box({0, 1.6f, 4.55f}, {1.4f, 1.8f, 0.05f}, Mat(pal::dark, 1.f));  // dark doorway → catacombs
    for (int i = 0; i < 3; i++) M.box({0, -0.1f - i * 0.2f, 5.6f + i * 0.5f}, {1.6f, 0.15f, 0.3f}, s);
    M.ellipsoid({0, 5.7f, 4.6f}, vec3(0.5f, 0.5f, 0.1f), Mat(hexColor(0x9a3a3a), 0.4f, 0, 1.5f), 8, 6);
}

static void caveMouth(MeshBuilder& M, int variant) {
    Rng rng(variant + 5);
    Mat rock(variant == 1 ? hexColor(0x2a2430) : pal::stoneDark, 0.9f);
    for (int i = 0; i < 9; i++) {
        float a = PI * (i / 8.f);
        vec3 p{std::cos(a) * 4.2f, std::sin(a) * 4.f, 0};
        M.rock(p, vec3(rng.range(1.6f, 2.3f), rng.range(1.4f, 2.f), rng.range(1.8f, 2.6f)), rock, rng.next(), 1);
    }
    M.rock({0, 5.5f, -1.5f}, vec3(5.f, 2.5f, 3.5f), rock, rng.next(), 1);
    M.box({0, 1.8f, -0.4f}, {2.9f, 2.6f, 0.2f}, Mat(pal::dark, 1.f));  // darkness of the cave
    if (variant == 1) {  // the Abyss: violet glow
        M.lathe({{2.4f, 0}, {0.01f, 0}}, Mat(hexColor(0x8a3aff), 0.2f, 0, 5.f), 16, {0, 2.f, -0.2f}, quat::axisAngle({1, 0, 0}, PI / 2));
    }
    for (auto& v : M.v) if (v.ny > 80 && rng.chance(0.5f)) { v.r = 70; v.g = 95; v.b = 50; }
}

static void mine(MeshBuilder& M) {
    Rng rng(9);
    Mat rock(pal::stoneDark, 0.9f), wood(pal::timber, 0.85f);
    M.rock({0, 2.5f, -2.f}, vec3(5.f, 3.5f, 3.f), rock, 11, 1);
    M.box({0, 1.6f, 0.9f}, {1.8f, 1.9f, 0.3f}, Mat(pal::dark));
    for (int s = -1; s <= 1; s += 2) M.box({s * 1.9f, 1.7f, 1.2f}, {0.18f, 1.9f, 0.18f}, wood);
    M.box({0, 3.6f, 1.2f}, {2.3f, 0.2f, 0.25f}, wood);
    for (int i = 0; i < 8; i++) M.box({0, 0.05f, 1.5f + i * 0.8f}, {0.8f, 0.05f, 0.12f}, wood);
    for (int s = -1; s <= 1; s += 2) M.box({s * 0.5f, 0.12f, 4.5f}, {0.04f, 0.04f, 3.2f}, Mat(pal::iron, 0.4f, 1.f));
    M.bevelBox({0, 0.6f, 5.5f}, {0.7f, 0.45f, 1.f}, 0.08f, Mat(hexColor(0x5a4a3a), 0.6f, 0.4f));
    M.rock({0, 1.15f, 5.5f}, vec3(0.6f, 0.3f, 0.8f), Mat(hexColor(0x8a6a4a)), 5, 0);
}

static void waystone(MeshBuilder& M) {
    Mat s(hexColor(0x5e6470), 0.7f);
    M.flat = true;
    M.lathe({{0.7f, -0.3f}, {0.6f, 2.5f}, {0.35f, 3.4f}, {0.01f, 3.6f}}, s, 5);
    M.flat = false;
    M.box({0, 1.9f, 0.62f}, {0.15f, 0.6f, 0.03f}, Mat(pal::rune, 0.3f, 0, 4.f));
    M.box({0, 1.9f, 0.63f}, {0.4f, 0.08f, 0.03f}, Mat(pal::rune, 0.3f, 0, 4.f));
    for (int i = 0; i < 7; i++) {
        float a = i * TAU / 7;
        M.rock({std::cos(a) * 2.4f, 0, std::sin(a) * 2.4f}, vec3(0.35f, 0.3f, 0.35f), s, 70 + i, 0);
    }
}

static void campfire(MeshBuilder& M) {
    for (int i = 0; i < 9; i++) {
        float a = i * TAU / 9;
        M.rock({std::cos(a) * 0.8f, 0, std::sin(a) * 0.8f}, vec3(0.22f, 0.18f, 0.22f), Mat(pal::stoneDark), 90 + i, 0);
    }
    for (int i = 0; i < 4; i++) {
        float a = i * TAU / 4 + 0.3f;
        M.tube({std::cos(a) * 0.55f, 0.05f, std::sin(a) * 0.55f}, {std::cos(a) * 0.1f, 0.45f, std::sin(a) * 0.1f}, 0.08f, 0.06f, Mat(hexColor(0x2a2018)), 5, true);
    }
    M.ellipsoid({0, 0.08f, 0}, vec3(0.45f, 0.06f, 0.45f), Mat(pal::ember, 0.6f, 0, 2.f), 8, 3);
    for (int s = -1; s <= 1; s += 2) M.tube({s * 1.5f, 0.25f, -0.4f}, {s * 1.5f, 0.25f, 0.6f}, 0.2f, 0.2f, Mat(pal::bark), 7, true);  // log benches
}

static void bridge(MeshBuilder& M, float halfLen) {
    Mat plank(pal::woodLight, 0.85f), beam(pal::timber, 0.85f), stone(pal::stone, 0.9f);
    int n = (int)(halfLen * 2 / 0.6f);
    for (int i = 0; i <= n; i++) {
        float z = -halfLen + i * 0.6f;
        float t = z / halfLen;
        float y = 0.8f * (1 - t * t);
        M.box({0, y, z}, {2.4f, 0.08f, 0.27f}, Mat(lerp(pal::woodLight, pal::wood, (i % 3) / 3.f), 0.85f));
        if (i % 4 == 0) for (int s = -1; s <= 1; s += 2) {
            M.box({s * 2.35f, y + 0.55f, z}, {0.08f, 0.55f, 0.08f}, beam);
        }
    }
    for (int s = -1; s <= 1; s += 2) {
        for (int i = 0; i < n; i += 4) {
            float z0 = -halfLen + i * 0.6f, z1 = std::min(halfLen, z0 + 2.4f);
            float t0 = z0 / halfLen, t1 = z1 / halfLen;
            M.tube({s * 2.35f, 0.8f * (1 - t0 * t0) + 1.05f, z0}, {s * 2.35f, 0.8f * (1 - t1 * t1) + 1.05f, z1}, 0.07f, 0.07f, beam, 5, false);
        }
        M.tube({s * 2.2f, -3.f, -halfLen * 0.45f}, {s * 2.2f, 0.75f, -halfLen * 0.45f}, 0.2f, 0.18f, beam, 6, false);
        M.tube({s * 2.2f, -3.f, halfLen * 0.45f}, {s * 2.2f, 0.75f, halfLen * 0.45f}, 0.2f, 0.18f, beam, 6, false);
    }
    for (int e = -1; e <= 1; e += 2) M.bevelBox({0, -0.6f, e * (halfLen + 0.5f)}, {2.8f, 0.8f, 1.f}, 0.1f, stone);
}

static void tent(MeshBuilder& M, int variant) {
    vec3 c = variant % 3 == 0 ? hexColor(0xb8a07a) : (variant % 3 == 1 ? hexColor(0x8a5a3a) : hexColor(0x5a6a4a));
    Mat cl(c, 0.95f);
    float w = 2.2f, l = 3.2f, h = 2.4f;
    M.quadP({-w, 0, -l}, {-w, 0, l}, {0, h, l}, {0, h, -l}, cl);
    M.quadP({w, 0, l}, {w, 0, -l}, {0, h, -l}, {0, h, l}, cl);
    M.triP({-w, 0, -l}, {0, h, -l}, {w, 0, -l}, cl);
    M.triP({w, 0, l}, {0, h, l}, {-w, 0, l}, Mat(c * 0.5f, 0.95f));
    M.triP({-w * 0.5f, 0, l + 0.01f}, {0, h * 0.75f, l + 0.01f}, {w * 0.5f, 0, l + 0.01f}, Mat(pal::dark));
    M.tube({0, h, -l - 0.3f}, {0, h, l + 0.3f}, 0.05f, 0.05f, Mat(pal::timber), 5, true);
}

static void monastery(MeshBuilder& M) {
    Mat s(hexColor(0xc8bca6), 0.85f), roof(hexColor(0x3e5a7a), 0.55f, 0.2f);
    M.jitter = 0.05f;
    M.box({0, 4.f, 0}, {10.f, 5.f, 7.f}, s);
    M.jitter = 0;
    gableRoof(M, 20.f, 14.f, 9.f, 4.f, 0.6f, roof);
    M.ellipsoid({0, 13.f, 0}, vec3(3.5f, 3.f, 3.5f), Mat(pal::gold * 0.8f, 0.3f, 0.9f), 14, 8);
    for (int i = -1; i <= 1; i += 2) {
        M.lathe({{2.f, -1}, {2.f, 14.f}, {2.5f, 14.4f}, {0.01f, 14.4f}}, s, 10, {i * 11.f, 0, 6.f});
        M.flat = true;
        M.cone({i * 11.f, 14.4f, 6.f}, {i * 11.f, 19.f, 6.f}, 2.5f, roof, 10);
        M.flat = false;
    }
    for (int i = -3; i <= 3; i++) M.box({i * 2.6f, 5.f, 7.05f}, {0.4f, 1.3f, 0.05f}, Mat(pal::window * 0.4f).nightGlow(3.5f));
    M.box({0, 2.f, 7.05f}, {1.4f, 2.f, 0.06f}, Mat(hexColor(0x4a3020)));
}

static void statue(MeshBuilder& M) {
    Mat st(hexColor(0x9a9a92), 0.6f, 0.1f);
    M.bevelBox({0, 0.8f, 0}, {1.6f, 1.2f, 1.6f}, 0.15f, Mat(pal::stone, 0.9f));
    M.tube({-0.35f, 2.f, 0}, {-0.35f, 3.6f, 0}, 0.28f, 0.25f, st, 8, false);
    M.tube({0.35f, 2.f, 0}, {0.35f, 3.6f, 0}, 0.28f, 0.25f, st, 8, false);
    M.bevelBox({0, 4.4f, 0}, {0.7f, 0.9f, 0.4f}, 0.1f, st);
    M.ellipsoid({0, 5.7f, 0}, vec3(0.35f, 0.42f, 0.38f), st, 10, 8);
    M.tube({-0.8f, 5.0f, 0}, {-1.1f, 3.6f, 0.4f}, 0.2f, 0.16f, st, 7, true);
    M.tube({0.8f, 5.0f, 0}, {0.6f, 3.7f, 0.6f}, 0.2f, 0.16f, st, 7, true);
    M.box({0.1f, 3.f, 0.75f}, {0.08f, 1.8f, 0.05f}, st);  // greatsword planted
    M.box({0.1f, 4.8f, 0.75f}, {0.4f, 0.06f, 0.06f}, st);
    for (auto& v : M.v) if (v.ny > 90) { v.r = 90; v.g = 110; v.b = 70; }
}

static void forge(MeshBuilder& M) {
    Mat post(pal::timber), stone(pal::stoneDark, 0.9f);
    for (int i = -1; i <= 1; i += 2) for (int j = -1; j <= 1; j += 2) M.box({i * 2.5f, 1.6f, j * 2.f}, {0.12f, 1.6f, 0.12f}, post);
    gableRoof(M, 5.f, 4.f, 3.2f, 1.3f, 0.5f, Mat(pal::tile, 0.8f));
    M.bevelBox({-1.3f, 0.8f, -1.2f}, {1.f, 0.9f, 0.8f}, 0.1f, stone);
    M.box({-1.3f, 0.8f, -0.39f}, {0.5f, 0.35f, 0.02f}, Mat(pal::ember, 0.5f, 0, 4.f));
    M.bevelBox({-1.3f, 2.8f, -1.4f}, {0.4f, 1.5f, 0.4f}, 0.05f, stone);
    // anvil
    M.box({1.f, 0.4f, 0.2f}, {0.25f, 0.4f, 0.2f}, Mat(pal::wood));
    M.box({1.f, 0.9f, 0.2f}, {0.5f, 0.12f, 0.2f}, Mat(pal::iron, 0.35f, 1.f));
    M.cone({1.5f, 0.9f, 0.2f}, {1.85f, 0.95f, 0.2f}, 0.12f, Mat(pal::iron, 0.35f, 1.f), 6);
    M.lathe({{0.4f, 0}, {0.42f, 0.6f}, {0.01f, 0.6f}}, Mat(hexColor(0x3a4a5a), 0.2f, 0.3f), 10, {2.f, 0, -1.f});  // quench barrel
}

static void lantern(MeshBuilder& M) {
    M.tube({0, -0.3f, 0}, {0, 2.6f, 0}, 0.07f, 0.06f, Mat(pal::iron, 0.4f, 0.9f), 6, false);
    M.box({0.3f, 2.55f, 0}, {0.3f, 0.03f, 0.03f}, Mat(pal::iron, 0.4f, 0.9f));
    M.box({0.55f, 2.25f, 0}, {0.12f, 0.17f, 0.12f}, Mat(pal::window * 0.5f, 0.3f).nightGlow(6.f));
    M.cone({0.55f, 2.42f, 0}, {0.55f, 2.62f, 0}, 0.17f, Mat(pal::iron, 0.4f, 0.9f), 4);
}

static void palisade(MeshBuilder& M) {
    for (int i = -4; i <= 4; i++) {
        float h = 3.f + 0.3f * std::sin(i * 1.7f);
        M.tube({i * 0.42f, -0.5f, 0}, {i * 0.42f, h, 0}, 0.2f, 0.2f, Mat(hexColor(0x5a4430), 0.9f), 6, false);
        M.cone({i * 0.42f, h, 0}, {i * 0.42f, h + 0.5f, 0}, 0.2f, Mat(hexColor(0x6a5440), 0.9f), 6);
    }
    M.box({0, 2.f, -0.22f}, {1.9f, 0.08f, 0.04f}, Mat(pal::timber));
}

static void altar(MeshBuilder& M) {
    M.bevelBox({0, 0.5f, 0}, {1.5f, 0.6f, 0.8f}, 0.1f, Mat(pal::stoneLight, 0.8f));
    M.bevelBox({0, 1.15f, 0}, {1.7f, 0.08f, 1.f}, 0.04f, Mat(pal::stoneLight, 0.7f));
    for (int i = -1; i <= 1; i++) {
        M.tube({i * 1.f, 1.2f, 0.6f}, {i * 1.f, 1.5f, 0.6f}, 0.05f, 0.05f, Mat(hexColor(0xeee6d0), 0.6f), 6, true);
        M.ellipsoid({i * 1.f, 1.57f, 0.6f}, vec3(0.04f, 0.07f, 0.04f), Mat(pal::window, 0.3f, 0, 6.f), 5, 3);
    }
    M.ellipsoid({0, 1.4f, 0}, vec3(0.25f), Mat(pal::rune, 0.2f, 0, 3.f), 8, 6);
}

void buildStructure(ChunkBuild& out, const Structure& s, const World& w) {
    float gy = w.height(s.pos.x, s.pos.y);
    if (s.poi >= 0) gy = w.pois[s.poi].height;
    if (s.kind == S_BRIDGE) gy = s.variant / 100.f + 1.2f;
    MeshBuilder M, Fo;
    Rng rng(hash2i((int)s.pos.x, (int)s.pos.y, s.kind));
    int reg = s.region;
    vec2 L(0, 0);
    switch (s.kind) {
    case S_HOUSE: house(M, Fo, s.variant, reg, false); addBoxCollider(out, s, gy, L, 3.4f, 4.2f, 5.f); break;
    case S_INN: house(M, Fo, 1, reg, true); addBoxCollider(out, s, gy, L, 4.3f, 5.3f, 8.f); break;
    case S_STONEHOUSE: stoneHouse(M, s.variant, reg); addBoxCollider(out, s, gy, L, 3.4f, 4.0f, 5.f); break;
    case S_LONGHOUSE: longhouse(M, s.variant); addBoxCollider(out, s, gy, L, 3.3f, 6.8f, 5.f); break;
    case S_STILT_HUT: stiltHut(M, s.variant); addBoxCollider(out, s, gy, L, 2.4f, 2.4f, 4.f); break;
    case S_TOWER: tower(M, s.variant, reg); addCircleCollider(out, s, gy, L, 2.9f, 12.f); break;
    case S_WINDMILL: windmill(M); addCircleCollider(out, s, gy, L, 3.f, 12.f);
        { vec2 hub = rotate(vec2(0, 3.1f) * s.scale, -s.rot) + s.pos; out.emitters.push_back(vec4(hub.x, gy + 9.6f * s.scale, hub.y, EM_WINDMILL + s.rot * 0.001f)); }
        break;
    case S_WELL: well(M); addCircleCollider(out, s, gy, L, 1.2f, 1.f); break;
    case S_STALL:
        if (s.variant < 0) {  // village clutter: crates, barrels, hay, sacks
            int n = rng.irange(2, 5);
            for (int i = 0; i < n; i++) {
                PropKind pk = i % 3 == 0 ? P_BARREL : (i % 3 == 1 ? P_CRATE : (reg == R_GREENHAVEN ? P_HAYBALE : P_CRATE));
                vec2 o{rng.range(-1.2f, 1.2f), rng.range(-1.2f, 1.2f)};
                const PropTemplate& t = propTemplate(pk, 0);
                mat4 pm = mat4::trs({o.x, (pk == P_CRATE && i > 2) ? 0.8f : 0.f, o.y}, rotY(rng.range(0, TAU)), vec3(1));
                appendXf(M, t.solid, pm, vec3(1));
            }
            addCircleCollider(out, s, gy, L, 1.3f, 1.f);
            break;
        }
        stall(M, Fo, s.variant); addBoxCollider(out, s, gy, L, 1.5f, 0.9f, 1.f); break;
    case S_FIELD: field(M, Fo, s.variant); break;
    case S_CASTLE_WALL: {
        // repeat 10 m modules to cover the requested length
        int n = std::max(1, (int)std::round(s.scale));
        Structure u = s; u.scale = 1;
        for (int i = 0; i < n; i++) {
            MeshBuilder seg;
            castleWall(seg, s.variant, 1);
            float off = (i - (n - 1) * 0.5f) * 10.f;
            seg.xf = mat4();
            for (auto& v : seg.v) v.px += off;
            M.append(seg);
            addBoxCollider(out, u, gy, {off, 0}, 5.2f, 1.2f, 8.f);
        }
        Structure t = s; t.scale = 1;
        mat4 m = mat4::trs({s.pos.x, gy, s.pos.y}, rotY(s.rot), vec3(1));
        for (auto& v : M.v) { vec3 p = m.transformPoint({v.px, v.py, v.pz}); vec3 nn = m.transformDir(unpackNormal(v)); v.px = p.x; v.py = p.y; v.pz = p.z; v.nx = (int8_t)(nn.x * 127); v.ny = (int8_t)(nn.y * 127); v.nz = (int8_t)(nn.z * 127); }
        out.props.append(M);
        return;
    }
    case S_CASTLE_TOWER: castleTower(M, s.variant); addCircleCollider(out, s, gy, L, 3.4f, 12.f); out.emitters.push_back(vec4(s.pos.x, gy + 17.f, s.pos.y, EM_BANNER)); break;
    case S_KEEP: keep(M, s.variant); addBoxCollider(out, s, gy, L, 9.5f, 7.5f, 14.f); break;
    case S_GATE: gate(M, s.variant); addBoxCollider(out, s, gy, {-3.7f, 0}, 1.5f, 1.3f, 8.f); addBoxCollider(out, s, gy, {3.7f, 0}, 1.5f, 1.3f, 8.f); break;
    case S_CHAPEL: chapel(M, s.variant); addBoxCollider(out, s, gy, L, 3.6f, 6.2f, 8.f); break;
    case S_RUIN_WALL: ruinWall(M, s.variant, rng); addBoxCollider(out, s, gy, L, 3.6f, 0.5f, 1.2f); out.colliders.back().climb = 1; break;
    case S_ELVEN_ARCH: elvenArch(M, s.variant); addCircleCollider(out, s, gy, {-2.2f, 0}, 0.6f, 6.f); addCircleCollider(out, s, gy, {2.2f, 0}, 0.6f, 6.f); out.lights.push_back(vec4(s.pos.x, gy + 6.3f, s.pos.y, LI_MAGIC)); break;
    case S_OBELISK: obelisk(M, s.variant); addCircleCollider(out, s, gy, L, 0.9f, 5.f); break;
    case S_CRYPT: crypt(M); addBoxCollider(out, s, gy, L, 4.1f, 4.6f, 6.f); break;
    case S_CAVE: caveMouth(M, s.variant); addCircleCollider(out, s, gy, {-4.2f, 0}, 1.8f, 6.f); addCircleCollider(out, s, gy, {4.2f, 0}, 1.8f, 6.f); addBoxCollider(out, s, gy, {0, -2.5f}, 4.f, 2.f, 6.f);
        if (s.variant == 1) out.lights.push_back(vec4(s.pos.x, gy + 2.f, s.pos.y, LI_MAGIC));
        break;
    case S_MINE: mine(M); addBoxCollider(out, s, gy, {0, -2.f}, 4.5f, 2.6f, 6.f); out.lights.push_back(vec4(s.pos.x, gy + 3.f, s.pos.y, LI_LANTERN)); break;
    case S_WAYSTONE: waystone(M); addCircleCollider(out, s, gy, L, 0.7f, 3.f); out.lights.push_back(vec4(s.pos.x, gy + 2.2f, s.pos.y, LI_MAGIC)); out.emitters.push_back(vec4(s.pos.x, gy + 1.f, s.pos.y, EM_SPARKLES)); break;
    case S_CAMPFIRE: campfire(M); out.lights.push_back(vec4(s.pos.x, gy + 1.2f, s.pos.y, LI_FIRE)); out.emitters.push_back(vec4(s.pos.x, gy + 0.2f, s.pos.y, EM_FIRE)); break;
    case S_BRIDGE: bridge(M, 8.f * s.scale + 4.f); { Structure u = s; u.scale = 1; for (int sd = -1; sd <= 1; sd += 2) addBoxCollider(out, u, gy + 0.5f, {sd * 2.6f, 0}, 0.15f, 8.f * s.scale + 3.f, 1.6f); } break;
    case S_TENT: tent(M, s.variant); addBoxCollider(out, s, gy, L, 2.f, 3.f, 2.4f); break;
    case S_MONASTERY: monastery(M); addBoxCollider(out, s, gy, L, 10.3f, 7.3f, 12.f); break;
    case S_STATUE: statue(M); addBoxCollider(out, s, gy, L, 1.6f, 1.6f, 5.f); break;
    case S_FORGE: forge(M); addBoxCollider(out, s, gy, {-1.3f, -1.2f}, 1.f, 0.8f, 3.f); out.lights.push_back(vec4(s.pos.x, gy + 1.5f, s.pos.y, LI_FIRE)); { vec2 ch = rotate(vec2(-1.3f, -1.4f) * s.scale, -s.rot) + s.pos; out.emitters.push_back(vec4(ch.x, gy + 4.4f, ch.y, EM_SMOKE)); } break;
    case S_LANTERN: lantern(M); addCircleCollider(out, s, gy, L, 0.15f, 2.6f); { vec2 lp = rotate(vec2(0.55f, 0) * s.scale, -s.rot) + s.pos; out.lights.push_back(vec4(lp.x, gy + 2.2f, lp.y, LI_LANTERN)); } break;
    case S_PALISADE: palisade(M); addBoxCollider(out, s, gy, L, 1.9f, 0.25f, 3.f); break;
    case S_ALTAR: altar(M); addBoxCollider(out, s, gy, L, 1.6f, 0.9f, 1.2f); out.lights.push_back(vec4(s.pos.x, gy + 1.8f, s.pos.y, LI_MAGIC)); break;
    default: break;
    }
    // chimney smoke for houses
    if (s.kind == S_HOUSE || s.kind == S_INN || s.kind == S_STONEHOUSE || s.kind == S_LONGHOUSE) {
        vec2 c = s.kind == S_LONGHOUSE ? vec2(0, 0) : (s.kind == S_STONEHOUSE ? vec2(-1.5f, 1.4f) : vec2(1.6f, -1.8f));
        float hy = s.kind == S_INN ? 10.f : (s.kind == S_LONGHOUSE ? 6.5f : 7.2f);
        vec2 wc = rotate(c * s.scale, -s.rot) + s.pos;
        out.emitters.push_back(vec4(wc.x, gy + hy * s.scale, wc.y, EM_SMOKE));
    }
    mat4 m = mat4::trs({s.pos.x, gy, s.pos.y}, rotY(s.rot), vec3(s.kind == S_BRIDGE ? 1.f : s.scale));
    appendXf(out.props, M, m, vec3(1));
    if (!Fo.v.empty()) appendXf(out.foliage, Fo, m, vec3(1));
}

}  // namespace lk
