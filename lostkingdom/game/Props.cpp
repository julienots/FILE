#include "Props.h"
#include <unordered_map>
#include <unordered_set>
#include <mutex>
#include "../engine/core/Platform.h"

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
    case R_AMBER: return {1.1f, 0.94f, 0.72f};
    case R_CORAL: return {0.98f, 1.04f, 0.9f};
    case R_STEPPE: return {1.08f, 1.0f, 0.78f};    // 2.4.0 golden grassland
    case R_JUNGLE: return {0.82f, 1.02f, 0.84f};   // deep, wet green
    case R_AURORA: return {0.95f, 1.0f, 0.93f};   // 2.8.0 cool moss
    case R_TITANS: return {0.93f, 1.0f, 0.9f};    // 3.0.0 alpine pasture
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
    case R_AMBER: return ramp3(0x6a5a30, 0x8a7a44, 0xb59a58, t);
    case R_CORAL: return ramp3(0x2f7a3a, 0x4f9a40, 0x9ab84a, t);
    case R_STEPPE: return ramp3(0x8a8a40, 0xb8a452, 0xd8c070, t);   // 2.4.0 straw and gold
    case R_JUNGLE: return ramp3(0x1f5a2a, 0x2f7a32, 0x5a9a3a, t);   // 2.4.0 emerald
    case R_AURORA: return ramp3(0x3f6236, 0x667f40, 0xa29c62, t);   // 2.8.0 northern moss and dry golden grass
    case R_TITANS: return ramp3(0x3e6a2e, 0x5f8a3c, 0x9aa25a, t);   // 3.0.0 lush alpine green, dry tips
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
    // 2.7.0 lumpier: leaf masses bulge out of the clump instead of a smooth ball
    auto lump = [&](vec3 u) { return 1.f + 0.17f * std::sin(u.x * 3.1f + ph1) * std::sin(u.z * 2.7f + ph2) + 0.1f * std::sin(u.y * 4.3f + ph1 * 1.7f) + 0.08f * std::sin(u.x * 7.3f + u.z * 6.1f + ph2 * 2.3f); };
    // core volume (slightly smaller: the leaf shell provides the outer silhouette)
    bool small = (r.x + r.y + r.z) / 3.f < 1.0f;   // 2.7.0 small clumps get a coarser core (the leaf cards carry the detail)
    const int seg = gLiteBuild || small ? 7 : 9, rings = gLiteBuild || small ? 4 : 6;
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
    // 2.7.0 twice as many, smaller leaf sprays that stand out of the volume: a broken, leafy silhouette
    int cards = gLiteBuild ? 0 : (int)clampf(6.f + (r.x + r.z) * 6.f, 10.f, 24.f);
    float avgR = (r.x + r.y + r.z) / 3.f;
    for (int k = 0; k < cards; k++) {
        vec3 u = rng.onSphere();
        if (u.y < -0.55f) u.y = -u.y * 0.5f;                 // few cards underneath
        u = normalize(u);
        vec3 p = c + u * r * rng.range(0.95f, 1.16f) * lump(u);
        vec3 n = normalize(normalize(vec3(u.x / r.x, u.y / r.y, u.z / r.z)) + vec3(0, 0.3f, 0));
        vec3 t = normalize(cross(n, std::fabs(n.y) > 0.9f ? vec3(1, 0, 0) : vec3(0, 1, 0)));
        float a = rng.f() * TAU;
        vec3 tx = t * std::cos(a) + cross(n, t) * std::sin(a), ty = cross(n, tx);
        float sz = avgR * rng.range(0.26f, 0.42f);
        vec3 lc = col * rng.range(0.78f, 1.3f) * (0.85f + 0.3f * clampf(u.y * 0.5f + 0.5f, 0.f, 1.f));   // sunlit on top
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

// 2.7.0 one tier of a conifer: a jagged star of boughs around the trunk. Each bough arches out from the trunk and
// droops at its tip; notches between them sit higher and closer. Dark and dense near the trunk, lighter needles at
// the tips, a dark underside. (Replaces the smooth stacked cones.)
static void pineTier(MeshBuilder& F, float y, float r, float rise, int pts, vec3 col, Rng& rng, bool mid, bool snowCap) {
    const int n = pts * 2;
    float rot = rng.f() * TAU;
    vec3 top{0, y + rise, 0}, under{0, y - r * 0.08f, 0};
    std::vector<vec3> rim(n), mr(n);
    for (int i = 0; i < n; i++) {
        float a = rot + TAU * i / n + rng.range(-0.1f, 0.1f);
        bool tip = (i & 1) == 0;
        float rr = tip ? r * rng.range(0.82f, 1.1f) : r * rng.range(0.42f, 0.58f);
        float droop = tip ? r * rng.range(0.34f, 0.5f) : r * 0.1f;
        rim[i] = vec3(std::cos(a) * rr, y - droop + (tip ? 0.f : rise * 0.2f), std::sin(a) * rr);
        mr[i] = lerp(top, rim[i], 0.55f) + vec3(0, r * (tip ? 0.12f : 0.05f), 0);   // the bough arches before it droops
    }
    auto V = [&](vec3 p, vec3 nrm, vec3 c, float ao) { uint32_t k = F.vert(p, normalize(nrm), Mat(c, 0.85f)); F.v[k].a = (uint8_t)clampf(ao * 255.f, 0, 255); return k; };
    vec3 deep = col * 0.55f, light = lerp(col * 1.12f, vec3(col.x * 1.15f, col.y * 1.2f, col.z * 0.85f), 0.4f);
    if (snowCap) { deep = pal::snow * 0.82f; light = pal::snow; }
    uint32_t it = V(top, {0, 1, 0}, deep, 0.6f);
    for (int i = 0; i < n; i++) {
        int j = (i + 1) % n;
        auto out = [&](vec3 p, float up) { vec3 h{p.x, 0, p.z}; float l = length(h); return (l > 1e-4f ? h / l : vec3(0)) * 0.65f + vec3(0, up, 0); };
        vec3 ci = lerp(deep, light, (i & 1) ? 0.55f : 1.f), cj = lerp(deep, light, (j & 1) ? 0.55f : 1.f);
        uint32_t ri = V(rim[i], out(rim[i], 0.55f), ci, (i & 1) ? 0.75f : 1.f), rj = V(rim[j], out(rim[j], 0.55f), cj, (j & 1) ? 0.75f : 1.f);
        if (mid) {
            uint32_t mi = V(mr[i], out(mr[i], 0.9f), lerp(deep, light, 0.45f), 0.8f), mj = V(mr[j], out(mr[j], 0.9f), lerp(deep, light, 0.45f), 0.8f);
            F.tri(it, mj, mi);
            F.tri(mi, mj, rj); F.tri(mi, rj, ri);
        } else F.tri(it, rj, ri);
        if (!snowCap) {   // underside: needles in the shade of the tier
            uint32_t u = V(under, {0, -1, 0}, col * 0.3f, 0.35f);
            uint32_t ui = V(rim[i], {0, -1, 0}, col * 0.45f, 0.5f), uj = V(rim[j], {0, -1, 0}, col * 0.45f, 0.5f);
            F.tri(u, ui, uj);
        }
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
        // 2.7.0 a crown made of the branches: each limb leaves the trunk, forks once and carries its own leaf mass,
        // so the crown is irregular with gaps where light passes, and the wood shows between the clumps
        vec3 cc = top + vec3(0, birch ? 0.6f : 1.1f, 0);
        float cr = birch ? 1.3f : 2.2f;
        vec3 base = birch ? hexColor(0x7fa83c) : (var % 4 == 3 ? pal::leafAut : pal::leaf1);
        Mat wood(birch ? pal::birch : pal::bark, 0.9f);
        int nb = birch ? 4 : 5;
        float a0 = rng.f() * TAU;
        for (int i = 0; i < nb; i++) {
            float a = a0 + i * TAU / nb + rng.range(-0.35f, 0.35f);
            vec3 out{std::cos(a), 0, std::sin(a)};
            vec3 bs = top * rng.range(0.5f, 0.85f);
            float reach = cr * rng.range(0.75f, 1.05f);
            vec3 be = bs + out * reach + vec3(0, rng.range(0.9f, 1.6f) * (birch ? 1.2f : 1.f), 0);
            S.tube(bs, be, r0 * 0.42f, r0 * 0.16f, wood, 5, false);
            vec3 fork = be + vec3(-out.z, 0.5f, out.x) * rng.range(0.4f, 0.7f) * (rng.chance(0.5f) ? 1.f : -1.f);
            S.tube(lerp(bs, be, 0.6f), fork, r0 * 0.2f, r0 * 0.08f, wood, 4, false);
            vec3 col = lerp(base, i % 2 ? pal::leaf2 : pal::leaf3, rng.f() * 0.55f) * rng.range(0.88f, 1.1f);
            float s = cr * rng.range(0.5f, 0.66f);
            clump(F, be + vec3(0, s * 0.25f, 0), vec3(s, s * 0.72f, s), col, 0.8f, rng.next());
            uint32_t fs = rng.next();
            if (!gLiteBuild) clump(F, fork + vec3(0, s * 0.15f, 0), vec3(s * 0.7f, s * 0.55f, s * 0.7f), lerp(col, base, 0.3f), 0.85f, fs);   // mid distance: no small clumps
        }
        clump(F, cc + vec3(0, cr * 0.35f, 0), vec3(cr * 0.72f, cr * 0.55f, cr * 0.72f), lerp(base, pal::leaf2, 0.4f), 1.f, rng.next());
        uint32_t is = rng.next();
        if (!gLiteBuild) clump(F, cc + vec3(rng.range(-0.4f, 0.4f), -cr * 0.05f, rng.range(-0.4f, 0.4f)), vec3(cr * 0.6f, cr * 0.45f, cr * 0.6f), base * 0.85f, 0.7f, is);
        t.radius = r0 * 1.2f; t.height = h + cr * 2;
        break;
    }
    case P_PINE: case P_SNOWPINE: {
        // 2.7.0 a real conifer: a tapering trunk with a root flare, 8 tiers of drooping boughs (5 at mid distance),
        // each tier turned at random, darker and denser towards the trunk, a spire at the top
        float h = rng.range(6.5f, 9.5f);
        S.tube({0, -0.3f, 0}, {0, h * 0.35f, 0}, 0.3f, 0.2f, Mat(pal::bark, 0.95f), 8, false);
        S.tube({0, h * 0.35f, 0}, {0, h * 0.94f, 0}, 0.2f, 0.05f, Mat(pal::bark, 0.95f), 6, false);
        if (!gLiteBuild) { Rng r2(var * 13 + 5); ringOfRoots(S, 0.26f, 5, pal::barkDark, r2); }
        int layers = gLiteBuild ? 4 : 7;
        float y0 = rng.range(1.1f, 1.6f), y1 = h - 0.9f;
        vec3 base = lerp(pal::pine, pal::pine2, rng.f() * 0.6f);
        for (int i = 0; i < layers; i++) {
            float f = (float)i / (layers - 1);
            float y = y0 + (y1 - y0) * f;
            float r = (1.f - f) * 2.5f * rng.range(0.92f, 1.08f) + 0.38f;
            float rise = (y1 - y0) / (layers - 1) * 1.35f + 0.25f;
            vec3 col = lerp(base, pal::pine2, rng.f() * 0.35f) * rng.range(0.9f, 1.08f);
            pineTier(F, y, r, rise, gLiteBuild ? 5 : (r > 1.4f ? 6 : 5), col, rng, !gLiteBuild, false);
            if (k == P_SNOWPINE) pineTier(F, y + 0.12f, r * 0.82f, rise * 0.85f, 5, col, rng, false, true);
        }
        F.flat = true;
        F.cone({0, y1 + 0.3f, 0}, {0, h + 0.5f, 0}, 0.32f, Mat(k == P_SNOWPINE ? pal::snow : base * 0.9f, 0.85f), 6);   // the spire
        F.flat = false;
        // 2.6.2 the wind moves the branch tips, not the whole layer: weight grows with the distance to the trunk
        // (0 on the axis, where the cone is fixed to the trunk) and a little with the height. Each cone used to
        // slide as one block and the crown came apart from its trunk in strong wind.
        for (auto& v : F.v) {
            float dr = std::sqrt(v.px * v.px + v.pz * v.pz);
            float w = clampf(dr / 2.8f, 0.f, 1.f) * (0.55f + 0.45f * clampf(v.py / h, 0.f, 1.f)) + (v.py > h - 1.f ? 0.15f : 0.f);
            v.wind = (uint8_t)clampf(w * 0.75f * 255.f, 0, 255);
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
            if (gLiteBuild) {   // mid distance: the cheap flat heads
                if (var % 3 == 1) F.blade(top - vec3(0, 0.02f, 0), top + vec3(0, 0.16f, 0), 0.06f, {1, 0, 0}, pm, pm);
                else F.plane(top, vec2(rng.range(0.07f, 0.1f)), pm, quat::axisAngle({0, 1, 0}, rng.f() * TAU));
                continue;
            }
            // 2.7.0 real flower heads: a lavender spike of small florets, or a cup of petals turned up around a heart
            if (var % 3 == 1) {
                for (int q = 0; q < 4; q++) {
                    float a = rng.f() * TAU; vec3 d{std::cos(a), 0, std::sin(a)};
                    vec3 p0 = top + vec3(0, q * 0.04f, 0);
                    F.blade(p0, p0 + d * 0.03f + vec3(0, 0.05f, 0), 0.045f - q * 0.007f, vec3(-d.z, 0, d.x), pm, pm);
                }
            } else {
                int np = var % 3 == 0 ? 6 : 4;
                float pr = var % 3 == 0 ? rng.range(0.055f, 0.075f) : rng.range(0.06f, 0.085f);
                float rot = rng.f() * TAU;
                for (int q = 0; q < np; q++) {
                    float a = rot + q * TAU / np; vec3 d{std::cos(a), var % 3 == 0 ? 0.3f : 0.9f, std::sin(a)};
                    F.blade(top, top + normalize(d) * pr, pr * (var % 3 == 0 ? 0.55f : 1.0f), vec3(-d.z, 0, d.x), pm, pm);
                }
                F.ellipsoid(top + vec3(0, 0.01f, 0), vec3(0.018f, 0.014f, 0.018f), Mat(var % 3 == 0 ? hexColor(0xf0c030) : hexColor(0x2a1a14), 0.7f, 0, 0, 1.f), 5, 3);
            }
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
    // ---------------------------------------------------------------- 2.0.0 Amber Sands and Coral Coast
    case P_PALM: case P_DATEPALM: {
        // curved, ringed trunk; a crown of arched fronds (three-section blades that droop at the tips); fruit
        bool date = k == P_DATEPALM;
        float h = date ? rng.range(6.5f, 8.5f) : rng.range(5.5f, 8.f);
        float lean = rng.range(0.7f, 2.2f) * (rng.chance(0.5f) ? 1.f : -1.f), ang = rng.range(0, TAU);
        vec3 dir{std::cos(ang), 0, std::sin(ang)};
        vec3 prev{0, -0.3f, 0};
        int segs = 7;
        Mat trunk(date ? hexColor(0x6b5438) : hexColor(0x8a7050), 0.92f);
        vec3 top = prev;
        for (int i = 1; i <= segs; i++) {
            float u = (float)i / segs;
            vec3 p = dir * (lean * u * u) + vec3(0, -0.3f + h * u, 0);
            S.tube(prev, p, 0.3f - u * 0.12f, 0.3f - (u + 1.f / segs) * 0.12f, trunk.withColor(trunk.color * (0.9f + 0.1f * (i % 2))), 7, i == segs);
            prev = p; top = p;
        }
        int nf = date ? 12 : rng.irange(9, 12);
        vec3 gl = date ? hexColor(0x3f7a38) : hexColor(0x2f8a3a), gt = date ? hexColor(0x7ab048) : hexColor(0x7cc24a);
        for (int i = 0; i < nf; i++) {
            float a = i * TAU / nf + rng.range(-0.15f, 0.15f);
            vec3 d{std::cos(a), 0, std::sin(a)};
            float len = rng.range(2.7f, 3.5f) * (date ? 0.9f : 1.f), up = rng.range(0.35f, 1.15f);
            vec3 p0 = top, p1 = top + d * (len * 0.38f) + vec3(0, up, 0), p2 = top + d * (len * 0.72f) + vec3(0, up * 0.9f - 0.5f, 0), p3 = top + d * len + vec3(0, up * 0.5f - 1.5f, 0);
            vec3 sd{-d.z, 0, d.x};
            Mat m0(lerp(gl, gt, 0.15f), 0.8f, 0, 0, 0.5f), m1(lerp(gl, gt, 0.55f), 0.8f, 0, 0, 0.8f), m2(gt, 0.8f, 0, 0, 1.f);
            if (gLiteBuild) {   // mid distance: three wide blades
                F.blade(p0, p1, 0.55f, sd, m0, m1);
                F.blade(p1, p2, 0.62f, sd, m1, m1);
                F.blade(p2, p3, 0.46f, sd, m1, m2);
                continue;
            }
            // 2.8.0 a real frond: a thin midrib along the arched curve, and pairs of narrow leaflets that hang from it
            auto curve = [&](float t) { float u = 1.f - t; return p0 * (u * u * u) + p1 * (3 * u * u * t) + p2 * (3 * u * t * t) + p3 * (t * t * t); };
            F.blade(p0, curve(0.5f), 0.07f, sd, m0, m1);
            F.blade(curve(0.5f), p3, 0.05f, sd, m1, m2);
            const int nl = 8;
            for (int q = 1; q <= nl; q++) {
                float t = 0.12f + 0.86f * q / nl;
                vec3 P = curve(t), T = normalize(curve(std::min(1.f, t + 0.05f)) - curve(std::max(0.f, t - 0.05f)));
                vec3 side = normalize(cross(T, vec3(0, 1, 0)));
                float ll = len * (0.26f - 0.13f * t) * rng.range(0.85f, 1.1f);
                Mat ml(lerp(gl, gt, t * 0.8f + rng.f() * 0.15f), 0.8f, 0, 0, 0.6f + 0.4f * t);
                for (int sgn = -1; sgn <= 1; sgn += 2) {
                    vec3 tip = P + side * (ll * sgn) + vec3(0, -ll * 0.55f, 0) + T * (ll * 0.35f);
                    F.blade(P, tip, 0.16f * (1.f - 0.4f * t), T, ml, ml.withWind(ml.wind + 0.15f));
                }
            }
        }
        if (date) for (int i = 0; i < 3; i++) F.ellipsoid(top + vec3(std::cos(i * 2.1f) * 0.3f, -0.45f - i * 0.05f, std::sin(i * 2.1f) * 0.3f), vec3(0.14f, 0.3f, 0.14f), Mat(hexColor(0xc0702a), 0.7f, 0, 0, 0.3f), 6, 4);
        else for (int i = 0; i < 4; i++) F.ellipsoid(top + vec3(std::cos(i * 1.6f) * 0.28f, -0.3f, std::sin(i * 1.6f) * 0.28f), vec3(0.16f), Mat(hexColor(0x6a4a22), 0.8f, 0, 0, 0.2f), 6, 4);
        t.radius = 0.4f; t.height = h;
        break;
    }
    case P_BANANA: {
        for (int s2 = 0; s2 < 3; s2++) {
            vec3 o{rng.range(-0.5f, 0.5f), 0, rng.range(-0.5f, 0.5f)};
            float h = rng.range(2.2f, 3.2f);
            S.tube(o + vec3(0, -0.2f, 0), o + vec3(rng.range(-0.1f, 0.1f), h, rng.range(-0.1f, 0.1f)), 0.2f, 0.12f, Mat(hexColor(0x6a8a3a), 0.85f), 7, false);
            int nl = 6;
            for (int i = 0; i < nl; i++) {
                float a = i * TAU / nl + s2;
                vec3 d{std::cos(a), 0, std::sin(a)};
                vec3 r0 = o + vec3(0, h, 0), r1 = r0 + d * 1.1f + vec3(0, 0.5f, 0), r2 = r0 + d * 2.1f + vec3(0, -0.15f, 0);
                vec3 sd{-d.z, 0, d.x};
                F.blade(r0, r1, 0.9f, sd, Mat(hexColor(0x3d8a30), 0.8f, 0, 0, 0.5f), Mat(hexColor(0x5aa83a), 0.8f, 0, 0, 0.9f));
                F.blade(r1, r2, 0.85f, sd, Mat(hexColor(0x5aa83a), 0.8f, 0, 0, 0.9f), Mat(hexColor(0x78c04a), 0.8f, 0, 0, 1.f));
            }
            if (s2 == 0) { for (int i = 0; i < 7; i++) S.tube({o.x + 0.2f, h - 0.6f - i * 0.04f, o.z}, {o.x + 0.35f, h - 0.9f - i * 0.04f, o.z + 0.08f * i - 0.3f}, 0.05f, 0.05f, Mat(hexColor(0xe4c33a), 0.6f), 5, true); }
        }
        t.radius = 0.55f; t.height = 3.f;
        break;
    }
    case P_TROPBUSH: {
        vec3 fl = var == 0 ? hexColor(0xe8364a) : (var == 1 ? hexColor(0xf5b23a) : hexColor(0xf070b8));
        for (int i = 0; i < 4; i++) {
            vec3 o{rng.range(-0.5f, 0.5f), rng.range(0.35f, 0.6f), rng.range(-0.5f, 0.5f)};
            clump(F, o, vec3(rng.range(0.55f, 0.8f), rng.range(0.4f, 0.55f), rng.range(0.55f, 0.8f)), lerp(hexColor(0x2f7a38), hexColor(0x4f9a40), rng.f()), 0.35f, rng.next());
        }
        for (int i = 0; i < 9; i++) {
            vec3 p = rng.onSphere() * vec3(0.75f, 0.45f, 0.75f) + vec3(0, 0.6f, 0);
            F.ellipsoid(p, vec3(0.13f, 0.06f, 0.13f), Mat(fl, 0.5f, 0, 0.25f, 0.3f), 6, 3);
            F.ellipsoid(p + vec3(0, 0.05f, 0), vec3(0.04f), Mat(hexColor(0xffe070), 0.5f, 0, 0.3f, 0.3f), 4, 3);
        }
        t.radius = 0; t.height = 1.1f;
        break;
    }
    case P_ACACIA: {
        // flat umbrella canopy on a forked trunk
        float h = rng.range(3.4f, 4.8f);
        Mat bark(hexColor(0x6a5238), 0.92f);
        vec3 top{rng.range(-0.5f, 0.5f), h, rng.range(-0.5f, 0.5f)};
        S.tube({0, -0.3f, 0}, top * 0.5f, 0.3f, 0.22f, bark, 7, false);
        for (int i = 0; i < 3; i++) {
            float a = i * TAU / 3 + rng.f();
            vec3 b = top * 0.5f, e = top + vec3(std::cos(a) * 1.6f, 0.1f * i, std::sin(a) * 1.6f);
            S.tube(b, e, 0.18f, 0.08f, bark, 6, false);
            clump(F, e + vec3(0, 0.35f, 0), vec3(2.0f, 0.42f, 2.0f), lerp(hexColor(0x6a8a30), hexColor(0x9aa040), rng.f() * 0.6f), 0.7f, rng.next());
        }
        clump(F, top + vec3(0, 0.5f, 0), vec3(2.4f, 0.5f, 2.4f), hexColor(0x7a9a34), 0.7f, rng.next());
        t.radius = 0.35f; t.height = h + 1.f;
        break;
    }
    case P_CACTUS: {
        // saguaro with up-turned arms; a pink flower crowns some
        float h = rng.range(2.6f, 4.2f);
        Mat c(hexColor(0x4f8a4a), 0.85f), rib(hexColor(0x3a6e3a), 0.85f);
        S.tube({0, -0.2f, 0}, {0, h, 0}, 0.3f, 0.27f, c, 10, true);
        S.ellipsoid({0, h, 0}, vec3(0.27f, 0.22f, 0.27f), c, 8, 4);
        for (int i = 0; i < 10; i++) { float a = i * TAU / 10; S.box({std::cos(a) * 0.29f, h * 0.5f, std::sin(a) * 0.29f}, {0.015f, h * 0.5f - 0.1f, 0.015f}, rib); }
        int arms = var == 0 ? 0 : (var == 1 ? 1 : 2);
        for (int i = 0; i < arms; i++) {
            float a = i * PI + rng.range(0, 1.f), y = h * rng.range(0.4f, 0.6f);
            vec3 d{std::cos(a), 0, std::sin(a)};
            vec3 p1 = d * 0.9f + vec3(0, y, 0), p2 = d * 1.05f + vec3(0, y + h * 0.28f, 0);
            S.tube(vec3(0, y, 0) + d * 0.1f, p1, 0.17f, 0.16f, c, 8, false);
            S.tube(p1, p2, 0.16f, 0.15f, c, 8, true);
            S.ellipsoid(p2, vec3(0.15f), c, 7, 4);
        }
        for (int i = 0; i < 24; i++) { float a = rng.range(0, TAU), y = rng.range(0.3f, h); S.box({std::cos(a) * 0.31f, y, std::sin(a) * 0.31f}, {0.012f, 0.05f, 0.012f}, Mat(hexColor(0xe8e2c0), 0.8f)); }
        if (var != 1) S.ellipsoid({0, h + 0.2f, 0}, vec3(0.16f, 0.09f, 0.16f), Mat(hexColor(0xf070a0), 0.5f, 0, 0.3f), 7, 3);
        t.radius = 0.4f; t.height = h + 0.3f;
        break;
    }
    case P_AGAVE: {
        int n = 13;
        for (int i = 0; i < n; i++) {
            float a = i * TAU / n * 1.62f + rng.range(-0.1f, 0.1f), tilt = 0.3f + 0.8f * (float)(i % 4) / 3.f;
            vec3 d{std::cos(a), 0, std::sin(a)};
            vec3 root{0, 0, 0}, tip = d * (0.4f + tilt * 1.0f) + vec3(0, 1.3f - tilt * 0.5f, 0);
            F.blade(root, tip, 0.26f, {-d.z, 0, d.x}, Mat(hexColor(0x4f7a62), 0.8f, 0, 0, 0.1f), Mat(hexColor(0x8ab8a0), 0.8f, 0, 0, 0.35f));
        }
        if (var == 2) { S.tube({0, 0, 0}, {0.05f, 3.2f, 0}, 0.07f, 0.04f, Mat(hexColor(0x8a7a40), 0.9f), 6, false); for (int i = 0; i < 6; i++) S.ellipsoid({0.05f, 2.2f + i * 0.17f, 0}, vec3(0.22f, 0.09f, 0.22f), Mat(hexColor(0xe8c050), 0.6f), 6, 3); }
        t.radius = 0; t.height = 1.2f;
        break;
    }
    case P_SANDROCK: {
        // wind-carved sandstone: a layered pillar on a narrow neck (a hoodoo) or a sloping slab
        vec3 c0 = lerp(hexColor(0xb8743a), hexColor(0xd69a58), rng.f());
        if (var != 2) {
            int layers = rng.irange(4, 6);
            float y = 0;
            for (int i = 0; i < layers; i++) {
                float r = 0.5f + 0.55f * std::fabs(std::sin(i * 1.4f + var));
                float hh = rng.range(0.45f, 0.8f);
                S.rock({rng.range(-0.08f, 0.08f), y + hh * 0.5f, rng.range(-0.08f, 0.08f)}, vec3(r * 1.1f, hh * 0.65f, r), Mat(lerp(c0, hexColor(0x8a5230), (float)(i % 2) * 0.5f), 0.9f), rng.next(), 1);
                y += hh * 0.85f;
            }
            t.height = y + 0.5f; t.radius = 0.9f;
        } else {
            S.rock({0, 0.5f, 0}, vec3(1.7f, 0.9f, 1.2f), Mat(c0, 0.9f), rng.next(), 1);
            S.rock({1.5f, 0.2f, 0.5f}, vec3(0.7f, 0.45f, 0.6f), Mat(c0 * 0.9f, 0.9f), rng.next(), 0);
            t.height = 1.6f; t.radius = 1.6f;
        }
        break;
    }
    case P_TUMBLEWEED: {
        for (int i = 0; i < 26; i++) {
            vec3 d = rng.onSphere();
            F.blade({0, 0.5f, 0}, d * 0.5f + vec3(0, 0.5f, 0), 0.05f, normalize(cross(d, vec3(0, 1, 0.1f))), Mat(hexColor(0x6a5232), 0.9f, 0, 0, 0.6f), Mat(hexColor(0x9a7a4a), 0.9f, 0, 0, 0.9f));
        }
        t.radius = 0; t.height = 1.f;
        break;
    }
    case P_DRYBUSH: {
        for (int i = 0; i < 7; i++) {
            float a = i * TAU / 7 + rng.range(-0.3f, 0.3f);
            vec3 tip = vec3(std::cos(a) * rng.range(0.3f, 0.7f), rng.range(0.7f, 1.2f), std::sin(a) * rng.range(0.3f, 0.7f));
            S.tube({0, 0, 0}, tip, 0.04f, 0.015f, Mat(hexColor(0x7a6040), 0.95f, 0, 0, 0.5f), 4, false);
            if (rng.chance(0.6f)) S.tube(tip * 0.6f, tip * 0.6f + vec3(rng.range(-0.3f, 0.3f), 0.35f, rng.range(-0.3f, 0.3f)), 0.025f, 0.01f, Mat(hexColor(0x8a6a48), 0.95f, 0, 0, 0.6f), 4, false);
            if (var == 1 && rng.chance(0.5f)) F.ellipsoid(tip, vec3(0.08f), Mat(hexColor(0xb8a050), 0.9f, 0, 0, 0.6f), 4, 3);
        }
        t.radius = 0; t.height = 1.f;
        break;
    }
    case P_SEAROCK: {
        vec3 col = lerp(hexColor(0x4a4a50), hexColor(0x6a6a70), rng.f());
        S.rock({0, 0.2f, 0}, vec3(rng.range(0.9f, 1.4f), rng.range(0.6f, 1.0f), rng.range(0.9f, 1.3f)), Mat(col, 0.8f), rng.next(), 1);
        S.rock({0.9f, 0.05f, 0.5f}, vec3(0.5f, 0.35f, 0.45f), Mat(col * 0.9f, 0.8f), rng.next(), 0);
        for (auto& v : S.v) if (v.ny < 60 && v.py < 0.3f) { Color8 c = toColor8(hexColor(0x3f7a52), 1); v.r = c.r; v.g = c.g; v.b = c.b; }   // weed at the waterline
        for (int i = 0; i < 4; i++) S.cone({rng.range(-0.8f, 0.8f), 0.2f, rng.range(-0.6f, 0.6f)}, {rng.range(-0.8f, 0.8f), 0.55f, rng.range(-0.6f, 0.6f)}, 0.1f, Mat(hexColor(0xe8a0a0), 0.8f), 6);
        t.radius = 1.1f; t.height = 1.3f;
        break;
    }
    case P_DRIFTWOOD: {
        Mat w(hexColor(0xb8aa94), 0.95f);
        vec3 a{-1.1f, 0.12f, 0}, b{0.1f, 0.35f, 0.2f}, c{1.2f, 0.18f, -0.1f};
        S.tube(a, b, 0.14f, 0.1f, w, 6, true); S.tube(b, c, 0.1f, 0.12f, w, 6, true);
        S.tube(b, b + vec3(-0.2f, 0.5f, 0.3f), 0.05f, 0.02f, w, 5, false);
        t.radius = 0.9f; t.height = 0.5f;
        break;
    }
    case P_SHELLS: {
        for (int i = 0; i < 5; i++) {
            vec3 p{rng.range(-0.6f, 0.6f), 0.03f, rng.range(-0.6f, 0.6f)};
            vec3 col = i % 3 == 0 ? hexColor(0xf4d8d0) : (i % 3 == 1 ? hexColor(0xf8f0e0) : hexColor(0xe8b8a0));
            if (i % 2) S.ellipsoid(p, vec3(0.12f, 0.05f, 0.1f), Mat(col, 0.4f), 7, 4);
            else S.cone(p, p + vec3(0.02f, 0.2f, 0), 0.08f, Mat(col, 0.45f), 7);
        }
        t.radius = 0; t.height = 0.2f;
        break;
    }
    case P_HAMMOCK: {
        Mat w(pal::wood, 0.88f);
        for (int s2 = -1; s2 <= 1; s2 += 2) S.tube({s2 * 1.5f, -0.2f, 0}, {s2 * 1.45f, 2.2f, 0}, 0.1f, 0.08f, w, 6, true);
        for (int i = 0; i < 10; i++) {
            float u = (float)i / 9.f, x = -1.4f + 2.8f * u, y = 1.9f - 0.9f * std::sin(u * PI);
            S.box({x, y, 0}, {0.16f, 0.025f, 0.34f}, Mat(i % 2 ? hexColor(0xd9d0b8) : hexColor(0xc85a40), 0.9f));
        }
        t.radius = 0.2f; t.height = 2.2f;
        break;
    }
    case P_NETRACK: {
        Mat w(pal::woodLight, 0.88f);
        for (int s2 = -1; s2 <= 1; s2 += 2) { S.tube({s2 * 1.6f, -0.2f, 0}, {s2 * 1.55f, 2.2f, 0.1f}, 0.07f, 0.06f, w, 6, true); S.tube({s2 * 1.6f, 0.2f, 0}, {s2 * 1.1f, 1.2f, 0.9f}, 0.04f, 0.04f, w, 5, false); }
        S.tube({-1.6f, 2.15f, 0.05f}, {1.6f, 2.15f, 0.05f}, 0.06f, 0.06f, w, 6, true);
        for (int i = 0; i < 12; i++) { float x = -1.45f + i * 0.26f; S.tube({x, 2.15f, 0.05f}, {x + 0.03f, 0.9f - 0.2f * std::sin(i * 0.5f), 0.05f}, 0.012f, 0.012f, Mat(hexColor(0xa89870), 0.95f), 4, false); }
        for (int j = 0; j < 4; j++) S.tube({-1.5f, 1.85f - j * 0.3f, 0.06f}, {1.5f, 1.8f - j * 0.3f, 0.06f}, 0.01f, 0.01f, Mat(hexColor(0xa89870), 0.95f), 4, false);
        for (int i = 0; i < 3; i++) S.ellipsoid({-0.9f + i * 0.9f, 0.5f, 0.08f}, vec3(0.28f, 0.06f, 0.1f), Mat(hexColor(0xaab4bc), 0.35f, 0.3f), 7, 4);   // fish
        t.radius = 0.2f; t.height = 2.2f;
        break;
    }
    case P_BOAT: {
        // a fishing boat: hull of planks, a mast with a furled lateen sail, oars, a net heap
        Mat hull(hexColor(0x7a5636), 0.88f), plank(hexColor(0x9c7448), 0.88f), paint(var == 1 ? hexColor(0x2a5a8a) : (var == 2 ? hexColor(0xb83a2a) : hexColor(0x2f7a6a)), 0.8f);
        for (int i = 0; i < 14; i++) {
            float u = (float)i / 13.f, z = -2.4f + 4.8f * u;
            float wdt = 0.95f * std::sin(u * PI * 0.92f + 0.12f) + 0.1f;
            float rise = 0.28f + 0.5f * std::pow(std::fabs(u * 2.f - 1.f), 2.f);
            S.box({-wdt, 0.45f + rise * 0.5f, z}, {0.05f, 0.3f + rise * 0.3f, 0.2f}, hull);
            S.box({wdt, 0.45f + rise * 0.5f, z}, {0.05f, 0.3f + rise * 0.3f, 0.2f}, hull);
            S.box({0, 0.22f, z}, {wdt * 0.85f, 0.05f, 0.2f}, plank);
        }
        S.box({-0.9f, 0.9f, 0}, {0.03f, 0.04f, 2.3f}, paint); S.box({0.9f, 0.9f, 0}, {0.03f, 0.04f, 2.3f}, paint);
        S.box({0, 0.55f, 0.9f}, {0.85f, 0.04f, 0.09f}, plank); S.box({0, 0.55f, -0.9f}, {0.85f, 0.04f, 0.09f}, plank);
        if (var != 0) {
            S.tube({0, 0.3f, 0.3f}, {0, 4.2f, 0.3f}, 0.07f, 0.05f, plank, 6, true);
            S.tube({-1.4f, 3.6f, 0.3f}, {1.6f, 1.6f, 0.3f}, 0.04f, 0.035f, plank, 5, true);
            S.triP({-1.4f, 3.6f, 0.3f}, {1.6f, 1.6f, 0.3f}, {-0.1f, 1.5f, 0.3f}, Mat(hexColor(0xe8dcc0), 0.95f));
            S.triP({1.6f, 1.6f, 0.3f}, {-1.4f, 3.6f, 0.3f}, {-0.1f, 1.5f, 0.3f}, Mat(hexColor(0xe8dcc0), 0.95f));
        } else {
            S.tube({-0.7f, 0.9f, 0.5f}, {-2.0f, 0.5f, 0.1f}, 0.03f, 0.03f, plank, 4, false);
            S.tube({0.7f, 0.9f, 0.5f}, {2.0f, 0.5f, 0.1f}, 0.03f, 0.03f, plank, 4, false);
            S.ellipsoid({0, 0.4f, -1.1f}, vec3(0.5f, 0.18f, 0.4f), Mat(hexColor(0xa89870), 0.95f), 7, 4);
        }
        t.radius = 1.4f; t.height = 1.3f;
        break;
    }
    case P_AMPHORA: {
        Mat tc(var == 1 ? hexColor(0xb86a3a) : hexColor(0xc87a48), 0.8f), tc2(hexColor(0x8a4a2a), 0.8f);
        auto jar = [&](vec3 o, float s, bool two) {
            S.lathe({{0.01f, 0}, {0.2f * s, 0.03f * s}, {0.34f * s, 0.3f * s}, {0.3f * s, 0.62f * s}, {0.16f * s, 0.85f * s}, {0.13f * s, 1.0f * s}, {0.19f * s, 1.05f * s}, {0.12f * s, 1.05f * s}}, tc, 10, o);
            S.lathe({{0.2f * s, 0.55f * s}, {0.31f * s, 0.57f * s}}, tc2, 10, o);
            if (two) for (int sd = -1; sd <= 1; sd += 2) S.tube(o + vec3(sd * 0.13f * s, 0.95f * s, 0), o + vec3(sd * 0.3f * s, 0.6f * s, 0), 0.025f * s, 0.025f * s, tc2, 4, false);
        };
        jar({0, 0, 0}, 1.2f, true); jar({0.55f, 0, 0.2f}, 0.9f, false);
        if (var != 2) jar({-0.4f, 0, 0.5f}, 0.75f, true);
        t.radius = 0.8f; t.height = 1.3f;
        break;
    }
    case P_CARPET: {
        vec3 a = var == 0 ? hexColor(0x9a2a2a) : (var == 1 ? hexColor(0x2a5a8a) : hexColor(0x2f7a5a)), b = hexColor(0xe8c860), c = hexColor(0xe8dcc0);
        S.box({0, 0.025f, 0}, {1.15f, 0.025f, 1.7f}, Mat(a, 0.95f));
        S.box({0, 0.052f, 0}, {1.0f, 0.004f, 1.55f}, Mat(b, 0.95f));
        S.box({0, 0.056f, 0}, {0.88f, 0.004f, 1.43f}, Mat(a * 0.85f, 0.95f));
        for (int i = 0; i < 3; i++) S.ellipsoid({0, 0.062f, -0.9f + i * 0.9f}, vec3(0.34f, 0.006f, 0.34f), Mat(i % 2 ? c : b, 0.9f), 8, 2);
        for (int i = 0; i < 8; i++) { float x = -1.0f + i * 0.28f; S.box({x, 0.025f, 1.78f}, {0.03f, 0.015f, 0.1f}, Mat(c, 0.95f)); S.box({x, 0.025f, -1.78f}, {0.03f, 0.015f, 0.1f}, Mat(c, 0.95f)); }
        t.radius = 0; t.height = 0.1f;
        break;
    }
    case P_BRAZIER: {
        Mat br(hexColor(0x8a5a30), 0.45f, 0.9f);
        for (int i = 0; i < 3; i++) { float a = i * TAU / 3; S.tube({std::cos(a) * 0.45f, 0, std::sin(a) * 0.45f}, {std::cos(a) * 0.12f, 0.85f, std::sin(a) * 0.12f}, 0.04f, 0.035f, br, 5, false); }
        S.lathe({{0.01f, 0.8f}, {0.28f, 0.88f}, {0.46f, 1.15f}, {0.42f, 1.18f}, {0.01f, 0.9f}}, br, 12);
        S.ellipsoid({0, 1.12f, 0}, vec3(0.34f, 0.09f, 0.34f), Mat(pal::ember, 0.6f, 0, 2.8f), 8, 3);
        t.radius = 0.5f; t.height = 1.2f;
        break;
    }
    case P_CRYSTALTREE: {
        // 2.8.0 (reworked) the aurora birch of the northern island: a slender silver-white trunk marked with dark
        // lenticels, leaning a little, thin drooping limbs and light, airy foliage in natural greens and gold
        float h = rng.range(5.5f, 7.5f);
        Mat bark(hexColor(0xe4e0d8), 0.75f), barkB(hexColor(0xd0ccc2), 0.75f);
        vec3 lean{rng.range(-0.35f, 0.35f), 0, rng.range(-0.35f, 0.35f)};
        vec3 p0{0, -0.3f, 0}, p1 = vec3(0, h * 0.5f, 0) + lean * 0.5f, p2 = vec3(0, h, 0) + lean;
        size_t sb = S.v.size();
        S.tube(p0, p1, 0.2f, 0.14f, bark, 8, false);
        S.tube(p1, p2, 0.14f, 0.05f, barkB, 7, false);
        for (size_t i = sb; i < S.v.size(); i++)   // dark horizontal lenticels and a darker, rougher foot
            if (std::fmod(std::fabs(S.v[i].py * 4.3f + S.v[i].px * 3.f), 1.f) < 0.16f || S.v[i].py < 0.5f) { S.v[i].r = 58; S.v[i].g = 54; S.v[i].b = 50; }
        vec3 leafA = hexColor(0x4a6e30), leafB = hexColor(0x6a8238), leafC = hexColor(0x9a8a40);
        vec3 base = lerp(leafA, var % 3 == 2 ? leafC : leafB, rng.f() * 0.6f);
        int nb = 5; float a0 = rng.f() * TAU;
        for (int i = 0; i < nb; i++) {
            float a = a0 + i * TAU / nb + rng.range(-0.3f, 0.3f);
            vec3 out{std::cos(a), 0, std::sin(a)};
            vec3 bs = lerp(p1, p2, rng.range(0.1f, 0.75f));
            vec3 be = bs + out * rng.range(0.9f, 1.4f) + vec3(0, rng.range(0.3f, 0.8f), 0);
            S.tube(bs, be, 0.06f, 0.025f, barkB, 5, false);
            float sz = rng.range(0.7f, 1.0f);
            clump(F, be + vec3(0, -sz * 0.1f, 0), vec3(sz, sz * 0.75f, sz), base * rng.range(0.88f, 1.12f), 0.9f, rng.next());
        }
        clump(F, p2 + vec3(0, 0.3f, 0), vec3(1.0f, 0.9f, 1.0f), lerp(base, leafB, 0.3f), 1.f, rng.next());
        clump(F, lerp(p1, p2, 0.55f) + vec3(0, 0.2f, 0), vec3(1.15f, 0.8f, 1.15f), base * 0.9f, 0.8f, rng.next());
        t.radius = 0.25f; t.height = h + 1.f;
        break;
    }
    case P_AURORASPIRE: {
        // 2.8.0 (reworked) an outcrop of quartz: milky and smoky crystals breaking out of a mossy rock, a faint inner glow
        S.flat = true;
        S.rock({0, 0.2f, 0}, vec3(1.2f, 0.55f, 1.1f), Mat(hexColor(0x6a6660), 0.9f), rng.next(), 1);
        S.rock({0.3f, 0.5f, -0.2f}, vec3(0.8f, 0.35f, 0.7f), Mat(hexColor(0x5a6a3a), 0.95f), rng.next(), 0);   // moss on top
        int n = gLiteBuild ? 3 : 6;
        for (int i = 0; i < n; i++) {
            vec3 d = normalize(vec3(rng.range(-0.6f, 0.6f), 1, rng.range(-0.6f, 0.6f)));
            float l = i == 0 ? rng.range(1.6f, 2.4f) : rng.range(0.6f, 1.4f);
            float r = i == 0 ? 0.3f : rng.range(0.12f, 0.22f);
            vec3 b{rng.range(-0.6f, 0.6f), 0.3f, rng.range(-0.5f, 0.5f)};
            vec3 col = lerp(hexColor(0xe8ecee), hexColor(0x8a8a92), rng.f() * 0.6f);
            if (rng.chance(0.25f)) col = lerp(col, hexColor(0xb8a0d8), 0.35f);   // a touch of amethyst
            S.tube(b, b + d * l * 0.8f, r, r * 0.92f, Mat(col, 0.12f, 0.f, 0.25f), 6, false);
            S.cone(b + d * l * 0.8f, b + d * l, r * 0.92f, Mat(col * 1.05f, 0.1f, 0.f, 0.35f), 6);
        }
        S.flat = false;
        t.radius = 0.9f; t.height = 2.5f;
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
    case P_DATEPALM: case P_PALM: return 4;
    case P_SHELLS: case P_NETRACK: case P_HAMMOCK: case P_BRAZIER: case P_TUMBLEWEED: case P_DRIFTWOOD: return 1;
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
    return variant == 1 ? hexColor(0x4a4650) : (variant == 2 ? hexColor(0xa8b2ba) : (variant == 3 ? hexColor(0xd2b078) : hexColor(0x9a948a)));
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

// 3.1.0 mine entrance rebuilt. Before, it was one 10 m rock whose flat bottom stood 1.3 m above the ground: a giant
// faceted pebble floating next to a lone door frame. Now a rocky knoll sunk into the ground (every rock's flat base is
// below the terrain), a timbered adit (sets of posts and caps receding into the dark), a plank hood, rails with a
// loaded ore cart, a winch, an ore pile, a pickaxe, a hanging lantern and a sign.
static void mine(MeshBuilder& M) {
    Rng rng(9);
    Mat rock(pal::stoneDark, 0.9f), rockL(lerp(pal::stoneDark, pal::stone, 0.55f), 0.9f), wood(pal::timber, 0.85f), plank(pal::wood, 0.85f);
    Mat iron(pal::iron, 0.45f, 0.9f), dark(pal::dark, 1.f);
    auto sunk = [&](vec3 c, vec3 r, const Mat& m, uint32_t seed, int det) { c.y = std::min(c.y, r.y * 0.35f - 0.4f); M.rock(c, r, m, seed, det); };
    size_t r0 = M.v.size();
    // the knoll: a long back ridge, two shoulders framing the adit, crown rocks and scree
    sunk({0, 1.0f, -5.2f}, {7.0f, 4.6f, 4.4f}, rock, 11, 2);
    sunk({-2.5f, 2.2f, -7.f}, {4.5f, 6.0f, 3.6f}, rockL, 12, 2);
    sunk({3.0f, 1.6f, -6.5f}, {4.2f, 5.0f, 3.8f}, rock, 13, 2);
    for (int sd = -1; sd <= 1; sd += 2) {
        sunk({sd * 3.6f, 0.5f, -1.0f}, {2.3f, 3.3f, 2.6f}, sd < 0 ? rockL : rock, 20 + sd, 1);
        sunk({sd * 4.9f, 0.3f, 0.6f}, {1.4f, 1.8f, 1.5f}, rock, 30 + sd, 1);
        for (int i = 0; i < 4; i++) sunk({sd * rng.range(2.6f, 5.6f), 0.f, rng.range(1.f, 3.2f)}, vec3(rng.range(0.25f, 0.55f)), rockL, 40 + i + sd * 7, 0);
    }
    sunk({0, 4.2f, -2.6f}, {3.4f, 1.6f, 2.0f}, rockL, 50, 1);   // the brow over the adit
    for (size_t i = r0; i < M.v.size(); i += 3) if (M.v[i].ny > 90 && rng.chance(0.45f)) for (size_t j = i; j < i + 3 && j < M.v.size(); j++) { Vertex& q = M.v[j]; Color8 c = toColor8(lerp(pal::moss, pal::stoneDark, 0.45f), 1); q.r = (uint8_t)((q.r + c.r) / 2); q.g = (uint8_t)((q.g + c.g) / 2); q.b = (uint8_t)((q.b + c.b) / 2); }
    // the adit: a dark tunnel, then three timber sets receding into it
    M.box({0, 1.55f, -1.6f}, {1.55f, 1.7f, 2.2f}, dark);
    for (int k = 0; k < 3; k++) {
        float z = 1.0f - k * 1.15f, h = 3.15f - k * 0.08f, x = 1.62f - k * 0.04f;
        Mat w = k == 0 ? wood : wood.withColor(pal::timber * (0.8f - k * 0.2f));
        for (int sd = -1; sd <= 1; sd += 2) M.tube({sd * (x + 0.12f), -0.3f, z}, {sd * x, h, z}, 0.16f, 0.14f, w, 6, true);
        M.bevelBox({0, h + 0.1f, z}, {x + 0.4f, 0.16f, 0.18f}, 0.03f, w);
    }
    for (int sd = -1; sd <= 1; sd += 2) M.tube({sd * 1.6f, 2.2f, 1.0f}, {sd * 0.95f, 3.2f, 1.0f}, 0.07f, 0.07f, wood, 5, false);   // knee braces
    // plank hood over the mouth
    for (int i = 0; i < 6; i++) {
        float x = -1.85f + i * 0.74f;
        M.box({x, 3.62f, 1.35f}, {0.36f, 0.05f, 0.75f}, plank.withColor(pal::wood * (0.85f + 0.12f * (i % 2))), quat::axisAngle({1, 0, 0}, 0.22f));
    }
    M.box({0, 3.85f, 0.55f}, {2.2f, 0.12f, 0.14f}, wood);
    // the sign: a board on the lintel with a crossed pick and hammer
    M.bevelBox({0, 4.15f, 1.32f}, {0.9f, 0.26f, 0.05f}, 0.02f, plank);
    M.box({0, 4.15f, 1.38f}, {0.55f, 0.04f, 0.01f}, Mat(hexColor(0x2a1d12)), quat::axisAngle({0, 0, 1}, 0.6f));
    M.box({0, 4.15f, 1.38f}, {0.55f, 0.04f, 0.01f}, Mat(hexColor(0x2a1d12)), quat::axisAngle({0, 0, 1}, -0.6f));
    // the hanging lantern under the lintel (the structure's lantern light sits right here)
    M.tube({0, 3.15f, 1.15f}, {0, 2.75f, 1.15f}, 0.015f, 0.015f, iron, 4, false);
    M.box({0, 2.6f, 1.15f}, {0.11f, 0.15f, 0.11f}, iron);
    M.box({0, 2.6f, 1.15f}, {0.085f, 0.12f, 0.12f}, Mat(pal::window, 0.4f).withEmis(2.5f));
    // rails out of the tunnel and down the spoil track
    for (int i = 0; i < 12; i++) M.box({0, 0.04f, -1.6f + i * 0.75f}, {0.75f, 0.06f, 0.11f}, wood.withColor(pal::timber * (0.85f + 0.2f * rng.f())));
    for (int sd = -1; sd <= 1; sd += 2) M.box({sd * 0.48f, 0.13f, 2.5f}, {0.035f, 0.045f, 4.3f}, iron);
    // the ore cart on the rails, loaded
    M.bevelBox({0, 0.7f, 4.6f}, {0.62f, 0.38f, 0.85f}, 0.06f, Mat(hexColor(0x4a3c30), 0.7f, 0.3f));
    for (int sd = -1; sd <= 1; sd += 2) for (int e = -1; e <= 1; e += 2) M.lathe({{0.01f, -0.04f}, {0.2f, -0.04f}, {0.2f, 0.04f}, {0.01f, 0.04f}}, iron, 10, {sd * 0.5f, 0.24f, 4.6f + e * 0.5f}, quat::axisAngle({0, 0, 1}, PI / 2));
    M.box({0, 1.1f, 4.6f}, {0.65f, 0.04f, 0.9f}, iron);
    for (int i = 0; i < 6; i++) M.rock({rng.range(-0.35f, 0.35f), 1.12f, 4.6f + rng.range(-0.5f, 0.5f)}, vec3(rng.range(0.18f, 0.28f)), Mat(i % 3 == 0 ? hexColor(0x9a6a40) : hexColor(0x6a625a), 0.7f, i % 3 == 0 ? 0.5f : 0.f), 60 + i, 0);
    // ore pile and tools beside the track
    for (int i = 0; i < 9; i++) M.rock({2.6f + rng.range(-0.8f, 0.8f), 0.05f, 3.4f + rng.range(-0.7f, 0.7f)}, vec3(rng.range(0.2f, 0.4f)), Mat(i % 3 == 0 ? hexColor(0xa07848) : hexColor(0x625c55), 0.75f, i % 3 == 0 ? 0.4f : 0.f), 70 + i, 0);
    M.tube({2.1f, 0.0f, 1.9f}, {2.2f, 1.15f, 1.6f}, 0.035f, 0.03f, plank, 5, true);   // pickaxe against the rock
    M.box({2.2f, 1.17f, 1.6f}, {0.32f, 0.04f, 0.04f}, iron, quat::axisAngle({0, 0, 1}, 0.25f));
    // the winch on the left: two posts, a drum and a rope down the track
    for (int e = -1; e <= 1; e += 2) M.bevelBox({-2.4f, 0.55f, 2.6f + e * 0.55f}, {0.09f, 0.6f, 0.09f}, 0.02f, wood);
    M.lathe({{0.01f, -0.5f}, {0.22f, -0.5f}, {0.22f, 0.5f}, {0.01f, 0.5f}}, plank, 10, {-2.4f, 0.95f, 2.6f}, quat::axisAngle({1, 0, 0}, PI / 2));
    M.lathe({{0.01f, -0.42f}, {0.25f, -0.42f}, {0.25f, 0.42f}, {0.01f, 0.42f}}, Mat(hexColor(0x8a7a58), 0.95f), 10, {-2.4f, 0.95f, 2.6f}, quat::axisAngle({1, 0, 0}, PI / 2));
    M.tube({-2.4f, 1.0f, 3.15f}, {-2.4f, 1.0f, 3.6f}, 0.05f, 0.05f, wood, 4, false);   // crank
    // two crates and a barrel by the cart
    M.bevelBox({-1.7f, 0.35f, 5.2f}, {0.35f, 0.35f, 0.35f}, 0.04f, plank);
    M.bevelBox({-1.6f, 0.95f, 5.15f}, {0.25f, 0.25f, 0.25f}, 0.03f, plank.withColor(pal::woodLight));
    M.lathe({{0.3f, 0}, {0.36f, 0.45f}, {0.3f, 0.9f}, {0.01f, 0.9f}}, plank, 10, {1.6f, 0, 5.4f});
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

// 1.11.2 timber bridge built on the shared deck profile (World::bridgeDeck): planks, stringers, posts and rails that
// follow the deck, trestle piers down to the river bed with cross bracing, stone abutments and lanterns at both ends.
static void bridgeV2(MeshBuilder& M, ChunkBuild& out, const Structure& s, const World& w, float gy) {
    float L = World::bridgeHalfLen(s);
    auto deck = [&](float z) { return w.bridgeDeck(s, z) - gy; };
    auto ground = [&](float x, float z) { vec2 p = rotate(vec2(x, z), -s.rot) + s.pos; return w.height(p.x, p.y) - gy; };
    Mat beam(pal::timber, 0.85f), dark(pal::timber * 0.7f, 0.9f), stone(pal::stone, 0.9f), iron(pal::iron, 0.4f, 0.9f);
    Rng rng(hash2i((int)s.pos.x, (int)s.pos.y, 77));
    // planks (top = deck), slightly uneven in colour, height and angle
    int n = (int)(L * 2 / 0.55f);
    for (int i = 0; i <= n; i++) {
        float z = -L + i * (2 * L / n);
        float y = deck(z);
        vec3 c = lerp(pal::woodLight, pal::wood, rng.f() * 0.8f) * (rng.chance(0.08f) ? 0.75f : 1.f);
        M.box({rng.range(-0.04f, 0.04f), y - 0.05f + rng.range(-0.012f, 0.012f), z}, {2.4f, 0.05f, 0.25f}, Mat(c, 0.85f),
              quat::axisAngle({0, 1, 0}, rng.range(-0.025f, 0.025f)));
    }
    // stringers under the planks and rails, in 1.1 m segments following the profile
    int segs = (int)(L * 2 / 1.1f);
    for (int i = 0; i < segs; i++) {
        float z0 = -L + i * (2 * L / segs), z1 = z0 + 2 * L / segs;
        float y0 = deck(z0), y1 = deck(z1);
        for (float x : {-2.05f, -0.7f, 0.7f, 2.05f}) M.tube({x, y0 - 0.2f, z0}, {x, y1 - 0.2f, z1}, 0.13f, 0.13f, dark, 5, false);
        for (int sd = -1; sd <= 1; sd += 2) {
            M.tube({sd * 2.3f, y0 + 1.05f, z0}, {sd * 2.3f, y1 + 1.05f, z1}, 0.07f, 0.07f, beam, 5, false);   // hand rail
            M.tube({sd * 2.3f, y0 + 0.55f, z0}, {sd * 2.3f, y1 + 0.55f, z1}, 0.045f, 0.045f, beam, 5, false); // mid rail
        }
    }
    // posts every 2.2 m
    int np = std::max(2, (int)(L * 2 / 2.2f));
    for (int i = 0; i <= np; i++) {
        float z = -L + 0.4f + i * ((2 * L - 0.8f) / np), y = deck(z);
        for (int sd = -1; sd <= 1; sd += 2) {
            M.box({sd * 2.3f, y + 0.55f, z}, {0.09f, 0.6f, 0.09f}, beam);
            M.box({sd * 2.3f, y + 1.18f, z}, {0.11f, 0.04f, 0.11f}, dark);
        }
    }
    // trestle piers where the deck is clear of the ground
    for (float z = -L + 3.2f; z <= L - 3.2f; z += 4.4f) {
        float y = deck(z);
        float g = std::min(ground(-2.f, z), ground(2.f, z));
        if (y - g < 1.1f) continue;
        float bot = g - 1.2f;
        for (int sd = -1; sd <= 1; sd += 2) M.tube({sd * 2.f, bot, z}, {sd * 1.95f, y - 0.25f, z}, 0.22f, 0.19f, dark, 7, false);
        M.box({0, y - 0.32f, z}, {2.35f, 0.14f, 0.18f}, beam);   // cap beam
        if (y - g > 2.2f) {                                        // X bracing
            float top = y - 0.5f, lo = std::max(g + 0.3f, top - 3.5f);
            M.tube({-1.95f, lo, z}, {1.95f, top, z}, 0.08f, 0.08f, beam, 5, false);
            M.tube({1.95f, lo, z}, {-1.95f, top, z}, 0.08f, 0.08f, beam, 5, false);
        }
    }
    // stone abutments (from below the bank up to the deck) and a lantern at each end
    for (int e = -1; e <= 1; e += 2) {
        float z = e * (L - 0.6f), y = deck(z);
        float g = std::min({ground(-2.8f, z), ground(2.8f, z), ground(0, z)}) - 0.8f;
        float hh = std::max(0.3f, (y - 0.12f - g) * 0.5f);
        M.bevelBox({0, g + hh, z}, {2.9f, hh, 1.3f}, 0.12f, stone);
        for (int sd = -1; sd <= 1; sd += 2) M.bevelBox({sd * 2.75f, y + 0.35f, z}, {0.28f, 0.5f, 0.4f}, 0.06f, stone);   // end pillars
        float lz = z + e * 0.1f, ly = y + 0.85f;
        M.tube({2.75f, ly, lz}, {2.75f, ly + 1.4f, lz}, 0.05f, 0.045f, iron, 6, false);
        M.box({2.75f, ly + 1.55f, lz}, {0.12f, 0.16f, 0.12f}, Mat(pal::window * 0.5f, 0.3f).nightGlow(6.f));
        M.cone({2.75f, ly + 1.71f, lz}, {2.75f, ly + 1.9f, lz}, 0.16f, iron, 4);
        vec2 lp = rotate(vec2(2.75f, lz), -s.rot) + s.pos;
        out.lights.push_back(vec4(lp.x, gy + ly + 1.55f, lp.y, LI_LANTERN));
    }
    // railings: colliders in 4 m pieces at the deck height
    Structure u = s; u.scale = 1;
    int nc = std::max(1, (int)std::ceil(2 * L / 4.f));
    for (int i = 0; i < nc; i++) {
        float hz = L / nc, zc = -L + hz * (2 * i + 1);
        for (int sd = -1; sd <= 1; sd += 2) addBoxCollider(out, u, gy + deck(zc), {sd * 2.6f, zc}, 0.15f, hz, 1.6f);
    }
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

// ---------------------------------------------------------------------------- imported building models
static std::vector<std::unique_ptr<PropModel>> gPropModels;
static std::vector<int> gKindModels[S_COUNT];

const char* structureModelName(int kind) {
    switch (kind) {
    case S_HOUSE: return "house"; case S_INN: return "inn"; case S_STONEHOUSE: return "stonehouse"; case S_LONGHOUSE: return "longhouse";
    case S_FORGE: return "forge"; case S_CHAPEL: return "chapel"; case S_TOWER: return "tower"; case S_WELL: return "well";
    case S_STALL: return "stall"; case S_TENT: return "tent"; default: return nullptr;
    }
}

// 2.3.6 vertex-clustering decimation: snap every vertex to a grid cell, keep one representative per cell, drop the
// triangles that collapse. Cheap, robust, and fine for a building seen from 30 m and more.
void clusterDecimate(const std::vector<Vertex>& V, const std::vector<uint32_t>& I, vec3 bmin, vec3 bmax, int cells,
                            std::vector<Vertex>& outV, std::vector<uint32_t>& outI) {
    vec3 ext = vmax(bmax - bmin, vec3(1e-3f));
    float cs = std::max(ext.x, std::max(ext.y, ext.z)) / cells;
    std::unordered_map<uint64_t, uint32_t> cellOf;
    std::vector<uint32_t> remap(V.size());
    outV.clear(); outI.clear();
    for (size_t i = 0; i < V.size(); i++) {
        int cx = (int)((V[i].px - bmin.x) / cs), cy = (int)((V[i].py - bmin.y) / cs), cz = (int)((V[i].pz - bmin.z) / cs);
        uint64_t key = ((uint64_t)(uint32_t)cx << 42) ^ ((uint64_t)(uint32_t)cy << 21) ^ (uint64_t)(uint32_t)cz;
        auto it = cellOf.find(key);
        if (it == cellOf.end()) { cellOf[key] = (uint32_t)outV.size(); remap[i] = (uint32_t)outV.size(); outV.push_back(V[i]); }
        else remap[i] = it->second;
    }
    std::unordered_set<uint64_t> seen;
    for (size_t t = 0; t + 2 < I.size(); t += 3) {
        uint32_t a = remap[I[t]], b = remap[I[t + 1]], c = remap[I[t + 2]];
        if (a == b || b == c || a == c) continue;
        uint32_t lo = std::min(a, std::min(b, c)), hi = std::max(a, std::max(b, c)), mid = a + b + c - lo - hi;
        uint64_t key = ((uint64_t)lo << 42) ^ ((uint64_t)mid << 21) ^ hi;
        if (!seen.insert(key).second) continue;
        outI.push_back(a); outI.push_back(b); outI.push_back(c);
    }
}

int loadPropModels() {
    gPropModels.clear();
    for (auto& v : gKindModels) v.clear();
    for (int k = 0; k < S_COUNT; k++) {
        const char* name = structureModelName(k);
        if (!name) continue;
        for (int var = 1; var <= 3; var++) {
            std::string id = std::string(name) + (var > 1 ? "_" + std::to_string(var) : "");
            std::vector<uint8_t> mb, tb;
            if (!platform::readAsset("models/props/" + id + ".lkm", mb)) continue;
            auto m = std::make_unique<PropModel>();
            m->id = id;
            if (!m->data.loadLKM(mb) || !platform::readAsset("models/props/" + id + ".jpg", tb) || !m->data.loadTexture(tb)) { AGR_WARN("building model '%s' is invalid, built in code instead", id.c_str()); continue; }
            m->bmin = vec3(1e9f); m->bmax = vec3(-1e9f);
            for (const Vertex& v : m->data.verts) { vec3 p(v.px, v.py, v.pz); m->bmin = vmin(m->bmin, p); m->bmax = vmax(m->bmax, p); }
            clusterDecimate(m->data.verts, m->data.idx, m->bmin, m->bmax, 28, m->lodVerts, m->lodIdx);
            AGR_LOG("building model '%s': %zu triangles (far: %zu), %.1f x %.1f x %.1f m", id.c_str(), m->data.idx.size() / 3, m->lodIdx.size() / 3, m->bmax.x - m->bmin.x, m->bmax.y - m->bmin.y, m->bmax.z - m->bmin.z);
            gKindModels[k].push_back((int)gPropModels.size());
            gPropModels.push_back(std::move(m));
        }
    }
    return (int)gPropModels.size();
}

int propModelFor(int kind, int variant) {
    if (kind < 0 || kind >= S_COUNT || gKindModels[kind].empty()) return -1;
    if (kind == S_STALL && variant < 0) return -1;   // village clutter (crates, barrels), not a stall
    const auto& v = gKindModels[kind];
    return v[(size_t)std::abs(variant) % v.size()];
}

PropModel* propModel(int i) { return i >= 0 && i < (int)gPropModels.size() ? gPropModels[i].get() : nullptr; }

// footprint (half extents, metres) the model is fitted into: the code-built structure's collider
vec2 structureFootprint(int kind) {
    switch (kind) {
    case S_HOUSE: return {3.4f, 4.2f}; case S_INN: return {4.3f, 5.3f}; case S_STONEHOUSE: return {3.4f, 4.0f}; case S_LONGHOUSE: return {3.3f, 6.8f};
    case S_FORGE: return {2.7f, 2.2f}; case S_CHAPEL: return {3.6f, 6.2f}; case S_TOWER: return {2.9f, 2.9f}; case S_WELL: return {1.2f, 1.2f};
    case S_STALL: return {1.5f, 0.9f}; case S_TENT: return {2.f, 3.f};
    case S_KEEP: return {9.5f, 7.5f}; case S_MONASTERY: return {10.3f, 7.3f}; case S_CRYPT: return {4.1f, 4.6f}; case S_WINDMILL: return {3.f, 3.f};
    case S_CASTLE_TOWER: return {3.4f, 3.4f}; case S_STILT_HUT: return {2.4f, 2.4f}; case S_STATUE: return {1.6f, 1.6f}; case S_GATE: return {5.2f, 1.3f};
    case S_PALISADE: return {1.9f, 0.25f}; case S_FIELD: return {9.f, 7.5f}; case S_PEN: return {5.f, 5.f}; case S_BANNER: return {1.5f, 0.7f};
    case S_DOMEHOUSE: return {3.4f, 3.5f}; case S_BAZAAR: return {3.3f, 2.5f}; case S_MINARET: return {1.8f, 1.8f}; case S_ZIGGURAT: return {11.f, 11.f};
    case S_LIGHTHOUSE: return {3.2f, 3.2f}; case S_PAGODA: return {5.4f, 11.f}; case S_SPHINX: return {3.2f, 7.3f}; case S_PIER: return {1.4f, 7.f};
    case S_PORTAL: return {6.f, 1.4f};
    case S_BEACHHUT: return {2.7f, 3.4f}; case S_COLONNADE: return {6.4f, 1.f}; case S_ARCHWAY: return {5.f, 1.2f};
    case S_GEYSER: return {1.6f, 1.6f}; case S_SKYISLAND: return {0.5f, 0.5f};   // 2.8.0 (the island floats: the ground under it stays free)
    case S_OBELISK: return {0.9f, 0.9f}; case S_ALTAR: return {1.6f, 0.9f}; case S_WAYSTONE: return {0.7f, 0.7f}; case S_CAMPFIRE: return {0.8f, 0.8f}; case S_LANTERN: return {0.6f, 0.3f};
    default: return {2.f, 2.f};
    }
}

// 1.10.1 buildings sit on the LOWEST ground under their footprint: on a slope (or at the edge of a levelled
// village) the uphill side is embedded in the ground instead of the downhill side floating in the air.
static bool restsOnFootprint(int kind) {
    switch (kind) {
    case S_HOUSE: case S_INN: case S_STONEHOUSE: case S_LONGHOUSE: case S_STILT_HUT: case S_TOWER: case S_WINDMILL: case S_WELL: case S_STALL:
    case S_FIELD: case S_CHAPEL: case S_CRYPT: case S_FORGE: case S_TENT: case S_PEN: case S_BANNER: case S_MONASTERY: case S_STATUE: case S_KEEP:
    case S_CASTLE_TOWER: case S_OBELISK: case S_ALTAR: case S_LANTERN: case S_PALISADE: case S_GATE: case S_WAYSTONE: case S_CAMPFIRE:
    case S_DOMEHOUSE: case S_BAZAAR: case S_MINARET: case S_ZIGGURAT: case S_LIGHTHOUSE: case S_PAGODA: case S_SPHINX: case S_BEACHHUT: case S_COLONNADE: case S_ARCHWAY: case S_PORTAL: return true;
    default: return false;
    }
}

// buildings that get a stone foundation on a slope (instead of being half buried uphill)
static bool getsPlinth(int kind) {
    switch (kind) {
    case S_HOUSE: case S_INN: case S_STONEHOUSE: case S_LONGHOUSE: case S_CHAPEL: case S_FORGE: case S_TOWER: case S_WINDMILL:
    case S_CRYPT: case S_MONASTERY: case S_KEEP: case S_CASTLE_TOWER:
    case S_DOMEHOUSE: case S_MINARET: case S_ZIGGURAT: case S_LIGHTHOUSE: case S_PAGODA: case S_SPHINX: case S_ARCHWAY: return true;
    default: return false;
    }
}

static void groundUnder(const Structure& s, const World& w, float& lo, float& hi) {
    vec2 fp = structureFootprint(s.kind) * std::max(0.5f, s.scale);
    lo = 1e9f; hi = -1e9f;
    for (int j = -2; j <= 2; j++)
        for (int i = -2; i <= 2; i++) {
            vec2 p = rotate(vec2(fp.x * i * 0.5f, fp.y * j * 0.5f), -s.rot) + s.pos;
            float h = w.height(p.x, p.y);
            lo = std::min(lo, h); hi = std::max(hi, h);
        }
}

float structureGroundY(const Structure& s, const World& w) {
    float gy = w.height(s.pos.x, s.pos.y);
    if (s.poi >= 0) gy = w.pois[s.poi].height;
    if (s.kind == S_BRIDGE) return s.variant / 100.f + 1.2f;
    if (s.baseY > -1e8f) return s.baseY - 0.04f;   // 2.2.1 levelled pad
    if (!restsOnFootprint(s.kind)) return gy;
    float lo, hi;
    groundUnder(s, w, lo, hi);
    float slope = hi - lo;
    if (getsPlinth(s.kind) && slope > 0.8f) return lo + std::min(slope * 0.5f, 2.2f);   // on its foundation
    return lo - 0.04f;
}

float structurePlinth(const Structure& s, const World& w) {
    if (s.baseY > -1e8f) return 0.f;   // 2.2.1 the ground is levelled: no foundation needed
    if (!restsOnFootprint(s.kind) || !getsPlinth(s.kind)) return 0.f;
    float lo, hi;
    groundUnder(s, w, lo, hi);
    return std::max(0.f, structureGroundY(s, w) - lo);
}

// 1.9.2 livestock enclosure: 10 x 10 m rail fence with a gate on +Z, a trough and hay
static void pen(MeshBuilder& M, Rng& rng) {
    Mat w(lerp(pal::wood, pal::timber, 0.3f), 0.9f), hay(hexColor(0xc8a850), 0.95f), water(hexColor(0x3a6a8a), 0.15f);
    auto rail = [&](vec2 a, vec2 b) {
        vec2 d = b - a; float L = length(d); vec2 n = d / L;
        int posts = std::max(1, (int)std::round(L / 2.5f));
        quat q = quat::axisAngle({0, 1, 0}, std::atan2(n.x, n.y) - PI * 0.5f);
        for (int i = 0; i <= posts; i++) { vec2 p = a + d * (i / (float)posts); M.bevelBox({p.x, 0.55f, p.y}, {0.08f, 0.6f, 0.08f}, 0.02f, w); }
        vec2 m = (a + b) * 0.5f;
        M.box({m.x, 0.85f, m.y}, {L * 0.5f, 0.05f, 0.035f}, w, q);
        M.box({m.x, 0.45f, m.y}, {L * 0.5f, 0.05f, 0.035f}, w, q);
    };
    float h = 5.f;
    rail({-h, -h}, {h, -h}); rail({h, -h}, {h, h}); rail({-h, h}, {-h, -h});
    rail({-h, h}, {-1.4f, h}); rail({1.4f, h}, {h, h});   // gate gap
    M.box({-3.2f, 0.35f, -3.6f}, {1.1f, 0.25f, 0.35f}, w);
    M.box({-3.2f, 0.56f, -3.6f}, {1.0f, 0.03f, 0.28f}, water);
    for (int i = 0; i < 3; i++) M.bevelBox({3.f + rng.range(-0.4f, 0.4f), 0.4f, -3.f + i * 1.1f}, {0.55f, 0.4f, 0.45f}, 0.12f, hay);
}

// 1.9.2 the domain's banner: a tall pole with a cloth banner (wind-animated) and a brazier
static void bannerPole(MeshBuilder& M, MeshBuilder& Fo) {
    Mat pole(pal::timber, 0.8f), iron(pal::iron, 0.4f, 0.9f), cloth = Mat(hexColor(0x8a1f1f), 0.9f), gold(hexColor(0xd8b040), 0.4f, 0.8f);
    M.tube({0, -0.3f, 0}, {0, 6.2f, 0}, 0.12f, 0.09f, pole, 8, true);
    M.box({0.9f, 6.0f, 0}, {0.95f, 0.05f, 0.05f}, pole);
    M.ellipsoid({0, 6.3f, 0}, vec3(0.16f), gold, 8, 6);
    for (int i = 0; i < 6; i++) {   // hanging cloth in strips so the wind can move its lower part
        float y0 = 6.0f - i * 0.55f, y1 = y0 - 0.55f;
        Mat c0 = cloth.withWind(i * 0.18f), c1 = cloth.withWind((i + 1) * 0.18f);
        Fo.blade({0.9f, y0, 0}, {0.9f, y1, 0}, 1.7f, {1, 0, 0}, c0, c1);
    }
    M.lathe({{0.45f, 0}, {0.5f, 0.5f}, {0.35f, 0.9f}, {0.01f, 0.9f}}, Mat(pal::stone, 0.8f), 10, {-1.4f, 0, 0.6f});
    M.lathe({{0.38f, 0.9f}, {0.42f, 1.15f}, {0.01f, 1.15f}}, iron, 10, {-1.4f, 0, 0.6f});
}

// ------------------------------------------------------------------------------------------ 2.0.0 new buildings
// Sandstone of the Amber Sands, lime-washed harbour towns of the Coral Coast, the temple of Shirokawa.
namespace sand {
const vec3 wall = hexColor(0xdcbf8a), wallDark = hexColor(0xb98d58), wallLight = hexColor(0xecd6a8), trim = hexColor(0x2fa6a0), trim2 = hexColor(0xc4543a);
const vec3 gold = hexColor(0xd9a93c), lime = hexColor(0xf2ece0), terracotta = hexColor(0xc0683a), indigo = hexColor(0x2a4a8a);
}  // namespace sand

static void striped(MeshBuilder& M, vec3 a0, vec3 a1, vec3 b0, vec3 b1, int n, const Mat& m1, const Mat& m2) {
    // a cloth sloping from edge a (a0..a1) to edge b (b0..b1), cut into n alternating strips
    for (int i = 0; i < n; i++) {
        float u0 = (float)i / n, u1 = (float)(i + 1) / n;
        vec3 p00 = lerp(a0, a1, u0), p01 = lerp(a0, a1, u1), p10 = lerp(b0, b1, u0), p11 = lerp(b0, b1, u1);
        const Mat& m = i % 2 ? m2 : m1;
        M.quadP(p00, p10, p11, p01, m); M.quadP(p00, p01, p11, p10, m);
    }
}

static void domeHouse(MeshBuilder& M, MeshBuilder& Fo, int variant, int region) {
    Rng rng(variant * 29 + region);
    float w = rng.range(5.6f, 6.6f), l = rng.range(5.8f, 6.8f), h = 3.5f + 0.5f * (variant % 2);
    bool coast = region == R_CORAL;
    vec3 wc = coast ? sand::lime : lerp(sand::wall, sand::wallLight, rng.f() * 0.6f);
    Mat wall(wc, 0.92f), dark(sand::wallDark, 0.92f), trim(coast ? sand::indigo : sand::trim, 0.7f), wood(hexColor(0x6a4426), 0.85f);
    M.jitter = 0.04f; M.jitterSeed = variant + 31;
    M.bevelBox({0, h / 2 - 0.2f, 0}, {w / 2, h / 2 + 0.4f, l / 2}, 0.16f, wall);
    M.jitter = 0;
    // parapet and a coloured band under it
    M.box({0, h + 0.28f, l / 2 - 0.1f}, {w / 2 + 0.08f, 0.28f, 0.14f}, wall); M.box({0, h + 0.28f, -l / 2 + 0.1f}, {w / 2 + 0.08f, 0.28f, 0.14f}, wall);
    M.box({w / 2 - 0.1f, h + 0.28f, 0}, {0.14f, 0.28f, l / 2}, wall); M.box({-w / 2 + 0.1f, h + 0.28f, 0}, {0.14f, 0.28f, l / 2}, wall);
    M.box({0, h - 0.35f, l / 2 + 0.03f}, {w / 2 + 0.02f, 0.12f, 0.03f}, trim); M.box({0, h - 0.35f, -l / 2 - 0.03f}, {w / 2 + 0.02f, 0.12f, 0.03f}, trim);
    M.box({w / 2 + 0.03f, h - 0.35f, 0}, {0.03f, 0.12f, l / 2}, trim); M.box({-w / 2 - 0.03f, h - 0.35f, 0}, {0.03f, 0.12f, l / 2}, trim);
    // horizontal mud-brick courses
    for (int i = 0; i < 3; i++) { float y = 0.8f + i * 0.95f; M.box({0, y, l / 2 + 0.02f}, {w / 2, 0.015f, 0.015f}, dark); M.box({w / 2 + 0.02f, y, 0}, {0.015f, 0.015f, l / 2}, dark); M.box({-w / 2 - 0.02f, y, 0}, {0.015f, 0.015f, l / 2}, dark); }
    // arched door with a heavy lintel
    M.box({0, 1.25f, l / 2 + 0.05f}, {0.62f, 1.15f, 0.05f}, wood);
    M.ellipsoid({0, 2.35f, l / 2 + 0.05f}, vec3(0.62f, 0.4f, 0.05f), wood, 10, 4);
    M.box({-0.78f, 1.4f, l / 2 + 0.08f}, {0.1f, 1.4f, 0.1f}, dark); M.box({0.78f, 1.4f, l / 2 + 0.08f}, {0.1f, 1.4f, 0.1f}, dark);
    M.box({0, 2.85f, l / 2 + 0.08f}, {0.9f, 0.1f, 0.1f}, dark);
    M.box({0.35f, 1.3f, l / 2 + 0.12f}, {0.04f, 0.04f, 0.03f}, Mat(sand::gold, 0.35f, 1.f));
    // lattice windows (glow at night)
    Mat win = Mat(hexColor(0xffb860) * 0.5f, 0.3f).nightGlow(3.4f);
    for (int s = -1; s <= 1; s += 2) {
        M.box({s * w * 0.32f, 2.3f, l / 2 + 0.04f}, {0.32f, 0.38f, 0.03f}, win);
        for (int i = -1; i <= 1; i++) { M.box({s * w * 0.32f + i * 0.18f, 2.3f, l / 2 + 0.07f}, {0.015f, 0.4f, 0.02f}, wood); M.box({s * w * 0.32f, 2.3f + i * 0.2f, l / 2 + 0.07f}, {0.34f, 0.015f, 0.02f}, wood); }
        M.box({s * (w / 2 + 0.04f), 2.2f, 0}, {0.03f, 0.36f, 0.3f}, win);
    }
    // striped awning over the door (poles at the front)
    Mat a1(coast ? hexColor(0x2a7aa0) : sand::trim2, 0.9f), a2(hexColor(0xf0e6d0), 0.9f);
    striped(M, {-1.5f, 3.0f, l / 2 + 0.1f}, {1.5f, 3.0f, l / 2 + 0.1f}, {-1.5f, 2.4f, l / 2 + 1.7f}, {1.5f, 2.4f, l / 2 + 1.7f}, 6, a1, a2);
    for (int s = -1; s <= 1; s += 2) M.tube({s * 1.45f, 0, l / 2 + 1.6f}, {s * 1.45f, 2.4f, l / 2 + 1.6f}, 0.06f, 0.05f, wood, 5, false);
    // roof: a small dome on a drum for variants 1 and 2, a shade pergola for variant 0, a stair up the side
    if (variant % 3 != 0) {
        Mat dome(variant % 3 == 1 ? (coast ? hexColor(0xe8e6e0) : sand::terracotta) : sand::indigo, 0.55f, 0.1f);
        M.lathe({{1.5f, 0}, {1.5f, 0.45f}, {1.45f, 0.5f}, {1.15f, 1.05f}, {0.75f, 1.55f}, {0.3f, 1.9f}, {0.05f, 2.1f}, {0.01f, 2.3f}}, dome, 14, {-w * 0.15f, h + 0.55f, -l * 0.12f});
        M.tube({-w * 0.15f, h + 2.8f, -l * 0.12f}, {-w * 0.15f, h + 3.35f, -l * 0.12f}, 0.04f, 0.015f, Mat(sand::gold, 0.35f, 1.f), 5, true);
    } else {
        for (int i = -1; i <= 1; i += 2) for (int j = -1; j <= 1; j += 2) M.tube({i * 1.8f, h + 0.5f, j * 1.8f - 0.6f}, {i * 1.8f, h + 2.3f, j * 1.8f - 0.6f}, 0.07f, 0.06f, wood, 5, false);
        for (int i = 0; i < 6; i++) M.box({0, h + 2.32f, -2.4f + i * 0.5f - 0.6f}, {2.0f, 0.03f, 0.05f}, wood);
        striped(M, {-2.0f, h + 2.4f, -2.4f}, {-2.0f, h + 2.4f, 1.0f}, {2.0f, h + 2.4f, -2.4f}, {2.0f, h + 2.4f, 1.0f}, 5, a1, a2);
    }
    for (int i = 0; i < 6; i++) M.box({w / 2 + 0.55f, 0.3f + i * (h / 6.f), -l * 0.28f + 0.0f - i * 0.0f}, {0.5f, 0.3f + i * (h / 12.f), 0.5f - 0.0f}, dark);   // outside stair, rising towards the back
    // vigas: roof beams sticking out of the wall, a drying rack, a clay pot
    for (int i = -2; i <= 2; i++) M.tube({i * 1.0f, h + 0.0f, l / 2 + 0.02f}, {i * 1.0f, h + 0.0f, l / 2 + 0.55f}, 0.07f, 0.07f, wood, 5, true);
    M.bakeHeightAO(-0.2f, 2.f, 0.7f);
    Fo.ellipsoid({w * 0.28f, 1.1f, l / 2 + 0.35f}, vec3(0.3f, 0.2f, 0.2f), Mat(hexColor(0x3fa04a), 0.7f, 0, 0, 0.3f), 8, 4);   // potted plant
}

static void bazaarHall(MeshBuilder& M, MeshBuilder& Fo, int variant, int region) {
    Rng rng(variant * 41 + region);
    Mat wood(hexColor(0x6a4426), 0.85f), dark(hexColor(0x3a2616), 0.9f);
    float W = 6.f, D = 4.3f;
    for (int i = -1; i <= 1; i++) for (int j = -1; j <= 1; j += 2) M.tube({i * W * 0.45f, -0.1f, j * D * 0.45f}, {i * W * 0.45f, 3.0f + 0.1f * i * j, j * D * 0.45f}, 0.1f, 0.08f, wood, 6, true);
    Mat c1(variant % 2 ? hexColor(0x2a7aa0) : sand::trim2, 0.9f), c2(hexColor(0xf0e6d0), 0.9f), c3(sand::gold, 0.9f);
    striped(M, {-W * 0.55f, 3.25f, -D * 0.5f}, {W * 0.55f, 3.25f, -D * 0.5f}, {-W * 0.55f, 3.9f, 0.f}, {W * 0.55f, 3.9f, 0.f}, 9, c1, c2);
    striped(M, {-W * 0.55f, 3.9f, 0.f}, {W * 0.55f, 3.9f, 0.f}, {-W * 0.55f, 3.25f, D * 0.5f}, {W * 0.55f, 3.25f, D * 0.5f}, 9, c2, c1);
    for (int i = 0; i < 10; i++) { float x = -W * 0.55f + (i + 0.5f) * (W * 1.1f / 10); Fo.blade({x, 3.25f, D * 0.5f}, {x, 2.85f, D * 0.5f + 0.05f}, 0.5f, {1, 0, 0}, c3.withWind(0.2f), (i % 2 ? c1 : c2).withWind(0.6f)); }
    // counters with goods: fruit pyramids, spice cones, jars, folded cloth
    for (int s = -1; s <= 1; s += 2) {
        M.bevelBox({s * 1.6f, 0.55f, D * 0.3f}, {1.2f, 0.55f, 0.5f}, 0.05f, wood);
        M.box({s * 1.6f, 1.12f, D * 0.3f}, {1.3f, 0.04f, 0.6f}, dark);
    }
    Mat fr[] = {Mat(hexColor(0xe86a1a), 0.6f), Mat(hexColor(0xd23a2a), 0.6f), Mat(hexColor(0xf0c030), 0.6f), Mat(hexColor(0x5aa030), 0.6f)};
    for (int i = 0; i < 4; i++) for (int k = 0; k < 6; k++) Fo.ellipsoid({-2.5f + i * 0.5f + rng.range(-0.08f, 0.08f), 1.25f + (k > 3 ? 0.14f : 0.f), D * 0.3f + (k % 3 - 1) * 0.17f}, vec3(0.12f), fr[i], 6, 4);
    Mat sp[] = {Mat(hexColor(0xd88a1a), 0.9f), Mat(hexColor(0xa02a1a), 0.9f), Mat(hexColor(0xd8c040), 0.9f), Mat(hexColor(0x6a3a1a), 0.9f)};
    for (int i = 0; i < 4; i++) M.cone({0.4f + i * 0.55f, 1.14f, D * 0.3f}, {0.4f + i * 0.55f, 1.55f, D * 0.3f}, 0.22f, sp[i], 10);
    M.box({1.6f, 1.4f, -D * 0.3f}, {0.9f, 0.04f, 0.45f}, dark);
    for (int i = 0; i < 4; i++) M.box({0.9f + i * 0.4f, 1.2f + 0.1f * (i % 2), -D * 0.3f}, {0.18f, 0.1f + 0.05f * (i % 2), 0.28f}, Mat(i % 2 ? sand::indigo : sand::trim2, 0.95f));
    M.lathe({{0.01f, 0}, {0.3f, 0.05f}, {0.42f, 0.4f}, {0.2f, 0.85f}, {0.2f, 1.0f}}, Mat(sand::terracotta, 0.8f), 10, {-2.4f, 0, -D * 0.35f});
    M.lathe({{0.01f, 0}, {0.26f, 0.05f}, {0.36f, 0.35f}, {0.18f, 0.75f}, {0.18f, 0.9f}}, Mat(sand::terracotta * 0.85f, 0.8f), 10, {-1.7f, 0, -D * 0.4f});
    // hanging lanterns
    for (int i = -1; i <= 1; i += 2) { M.tube({i * 2.0f, 3.2f, 0.2f}, {i * 2.0f, 2.7f, 0.2f}, 0.01f, 0.01f, dark, 3, false); M.ellipsoid({i * 2.0f, 2.55f, 0.2f}, vec3(0.15f, 0.2f, 0.15f), Mat(hexColor(0xffb050) * 0.5f, 0.4f).nightGlow(3.6f), 8, 5); }
    M.bakeHeightAO(-0.2f, 2.f, 0.72f);
}

static void minaret(MeshBuilder& M, int variant) {
    Mat wall(lerp(sand::wallLight, sand::wall, 0.3f), 0.85f), trim(sand::trim, 0.6f), gold(sand::gold, 0.35f, 0.9f);
    M.bevelBox({0, 0.5f, 0}, {1.8f, 0.7f, 1.8f}, 0.1f, Mat(sand::wallDark, 0.9f));
    M.tube({0, 1.0f, 0}, {0, 8.5f, 0}, 1.25f, 1.0f, wall, 8, false);
    M.lathe({{1.35f, 0}, {1.35f, 0.18f}, {1.0f, 0.3f}}, trim, 8, {0, 8.4f, 0});
    M.tube({0, 8.8f, 0}, {0, 11.8f, 0}, 0.9f, 0.8f, wall, 8, false);
    M.lathe({{1.6f, 0}, {1.6f, 0.18f}, {1.2f, 0.28f}, {0.9f, 0.28f}}, Mat(sand::wallDark, 0.9f), 8, {0, 11.4f, 0});
    for (int i = 0; i < 8; i++) { float a = i * TAU / 8; M.box({std::cos(a) * 1.5f, 12.0f, std::sin(a) * 1.5f}, {0.04f, 0.3f, 0.04f}, trim); }   // balcony rail
    M.tube({0, 11.7f, 0}, {0, 14.2f, 0}, 0.75f, 0.7f, wall, 8, false);
    Mat win = Mat(hexColor(0xffb860) * 0.5f, 0.3f).nightGlow(3.6f);
    for (int i = 0; i < 4; i++) { float a = i * PI / 2; M.box({std::cos(a) * 0.74f, 13.0f, std::sin(a) * 0.74f}, {0.06f, 0.45f, 0.06f}, win); }
    for (int i = 0; i < 4; i++) { float a = i * PI / 2 + PI / 4, y = 3.5f + i * 1.3f; M.box({std::cos(a) * 1.12f, y, std::sin(a) * 1.12f}, {0.05f, 0.35f, 0.05f}, Mat(hexColor(0x201810))); }
    M.lathe({{0.9f, 0}, {0.85f, 0.4f}, {0.55f, 0.95f}, {0.25f, 1.35f}, {0.01f, 1.55f}}, Mat(variant % 2 ? sand::indigo : sand::terracotta, 0.55f, 0.1f), 12, {0, 14.2f, 0});
    M.tube({0, 15.6f, 0}, {0, 17.2f, 0}, 0.04f, 0.015f, gold, 5, true);
    M.ellipsoid({0, 16.1f, 0}, vec3(0.17f), gold, 8, 6);
    M.bakeHeightAO(-0.2f, 3.f, 0.75f);
}

static void ziggurat(MeshBuilder& M, MeshBuilder& Fo, int variant) {
    // five terraces; a wide stair on the +Z face; a shrine with columns and a brazier at the top
    Mat st(sand::wall, 0.9f), st2(sand::wallDark, 0.9f), gold(sand::gold, 0.35f, 0.9f), glow(hexColor(0xffb050) * 0.6f, 0.4f);
    float sz = 11.f;
    float y = 0;
    for (int i = 0; i < 5; i++) {
        float hh = 1.55f, r = sz - i * 1.9f;
        M.jitter = 0.05f; M.jitterSeed = 7 + i;
        M.bevelBox({0, y + hh / 2 - 0.2f, 0}, {r, hh / 2 + 0.2f, r * 0.94f}, 0.12f, i % 2 ? st : st2);
        M.jitter = 0;
        M.box({0, y + hh + 0.02f, r * 0.94f - 0.0f}, {r + 0.04f, 0.06f, 0.06f}, gold);
        // carved band: dark niches along the face
        for (int k = -3; k <= 3; k++) { M.box({k * r * 0.26f, y + hh * 0.5f, r * 0.94f + 0.14f}, {0.35f, 0.4f, 0.03f}, Mat(hexColor(0x241810), 0.9f)); }
        y += hh;
    }
    // stair
    for (int i = 0; i < 14; i++) { float yy = 0.12f + i * (y / 14.f), zz = sz * 0.94f + 0.25f - i * (sz * 0.94f - 2.2f) / 14.f; M.box({0, yy * 0.5f + 0.0f, zz - 0.3f}, {2.1f, yy * 0.5f + 0.05f, 0.5f}, i % 2 ? st : Mat(sand::wallLight, 0.9f)); }
    for (int s = -1; s <= 1; s += 2) M.bevelBox({s * 2.6f, y * 0.5f, sz * 0.5f}, {0.5f, y * 0.5f + 0.2f, sz * 0.5f - 1.0f}, 0.1f, st2);
    // shrine
    float t = 1.7f;
    M.bevelBox({0, y + 0.15f, 0}, {t * 1.6f, 0.2f, t * 1.5f}, 0.05f, Mat(sand::wallLight, 0.85f));
    for (int i = -1; i <= 1; i += 2) for (int j = -1; j <= 1; j += 2) M.lathe({{0.38f, 0}, {0.3f, 0.2f}, {0.26f, 3.0f}, {0.34f, 3.2f}, {0.4f, 3.4f}}, Mat(sand::wallLight, 0.8f), 10, {i * t * 1.3f, y + 0.3f, j * t * 1.2f});
    M.bevelBox({0, y + 3.95f, 0}, {t * 1.65f, 0.28f, t * 1.55f}, 0.06f, st);
    M.box({0, y + 4.27f, t * 1.55f}, {t * 1.65f, 0.06f, 0.06f}, gold);
    M.cone({0, y + 4.2f, 0}, {0, y + 6.2f, 0}, t * 1.2f, st2, 4);
    M.ellipsoid({0, y + 6.4f, 0}, vec3(0.22f), gold, 8, 6);
    M.lathe({{0.01f, 0}, {0.55f, 0.3f}, {0.35f, 0.7f}, {0.5f, 1.2f}, {0.01f, 1.2f}}, Mat(sand::gold * 0.8f, 0.45f, 0.8f), 10, {0, y + 0.35f, 0});   // great brazier
    M.ellipsoid({0, y + 1.65f, 0}, vec3(0.45f, 0.14f, 0.45f), Mat(hexColor(0xff9a30), 0.6f, 0, 3.f), 8, 3);
    // winged sun disc over the base doorway
    M.lathe({{0.01f, 0}, {0.8f, 0.05f}, {0.8f, 0.12f}, {0.01f, 0.15f}}, gold, 14, {0, 2.3f, sz * 0.94f + 0.2f}, quat::axisAngle({1, 0, 0}, PI / 2));
    for (int s = -1; s <= 1; s += 2) for (int i = 0; i < 4; i++) M.box({s * (1.2f + i * 0.28f), 2.3f - i * 0.04f, sz * 0.94f + 0.2f}, {0.14f, 0.05f, 0.03f}, gold);
    M.bakeHeightAO(-0.2f, 4.f, 0.7f);
    (void)Fo; (void)variant;
}

static void lighthouse(MeshBuilder& M, MeshBuilder& Fo) {
    Mat white(hexColor(0xf4f0e8), 0.85f), red(hexColor(0xb8302a), 0.8f), stone(sand::wallDark * 0.8f, 0.9f), iron(pal::iron, 0.4f, 0.9f);
    M.bevelBox({0, 0.5f, 0}, {3.2f, 0.8f, 3.2f}, 0.15f, stone);
    for (int i = 0; i < 6; i++) {
        float y0 = 1.1f + i * 2.5f, y1 = y0 + 2.5f, r0 = 2.4f - i * 0.16f, r1 = 2.4f - (i + 1) * 0.16f;
        M.tube({0, y0, 0}, {0, y1, 0}, r0, r1, i % 2 ? red : white, 12, false);
    }
    M.lathe({{2.6f, 0}, {2.6f, 0.15f}, {1.5f, 0.3f}}, Mat(sand::wallDark, 0.7f), 12, {0, 16.1f, 0});
    for (int i = 0; i < 16; i++) { float a = i * TAU / 16; M.tube({std::cos(a) * 2.45f, 16.4f, std::sin(a) * 2.45f}, {std::cos(a) * 2.45f, 17.3f, std::sin(a) * 2.45f}, 0.03f, 0.03f, iron, 4, false); }
    M.tube({0, 17.3f, 0}, {0, 17.3f, 0.001f}, 0.01f, 0.01f, iron, 3, false);
    Mat glass = Mat(hexColor(0xfff2c0) * 0.55f, 0.15f).nightGlow(5.f);
    M.tube({0, 16.4f, 0}, {0, 18.8f, 0}, 1.25f, 1.15f, glass, 10, true);
    for (int i = 0; i < 8; i++) { float a = i * TAU / 8; M.box({std::cos(a) * 1.22f, 17.6f, std::sin(a) * 1.22f}, {0.05f, 1.25f, 0.05f}, iron); }
    M.lathe({{1.5f, 0}, {1.4f, 0.2f}, {0.8f, 0.9f}, {0.01f, 1.5f}}, red, 12, {0, 18.8f, 0});
    M.tube({0, 20.2f, 0}, {0, 21.2f, 0}, 0.04f, 0.02f, iron, 4, true);
    M.box({0, 1.5f, 2.35f}, {0.55f, 1.0f, 0.06f}, Mat(hexColor(0x3a2616), 0.9f));
    for (int i = 0; i < 4; i++) { float a = i * 1.7f + 0.6f, y = 4.f + i * 3.3f; M.box({std::cos(a) * (2.2f - y * 0.0f), y, std::sin(a) * (2.2f - y * 0.0f)}, {0.05f, 0.3f, 0.05f}, Mat(hexColor(0x1a1410))); }
    M.bakeHeightAO(-0.2f, 3.f, 0.78f);
    (void)Fo;
}

static void pagoda(MeshBuilder& M, MeshBuilder& Fo, int variant) {
    // Shirokawa: a three-tier pagoda on a stone platform, lacquered red posts, white plaster, teal tiles with upturned corners
    Mat stone(pal::stone, 0.9f), red(hexColor(0xa8281e), 0.55f), white(hexColor(0xf0ece0), 0.85f), tile(hexColor(0x2f5a5a), 0.55f, 0.1f), gold(sand::gold, 0.35f, 0.9f), wood(hexColor(0x3a2616), 0.85f);
    M.bevelBox({0, 0.4f, 0}, {5.4f, 0.55f, 5.4f}, 0.1f, stone);
    for (int i = 0; i < 4; i++) M.box({0, 0.1f + i * 0.14f, 5.5f + 0.5f - i * 0.18f}, {1.6f, 0.07f, 0.3f}, stone);
    float y = 0.95f;
    float half[3] = {3.4f, 2.7f, 2.0f};
    for (int t = 0; t < 3; t++) {
        float r = half[t], hh = 2.9f - 0.2f * t;
        M.box({0, y + hh / 2, 0}, {r - 0.35f, hh / 2, r - 0.35f}, white);
        for (int i = -1; i <= 1; i += 2) for (int j = -1; j <= 1; j += 2) M.tube({i * (r - 0.3f), y, j * (r - 0.3f)}, {i * (r - 0.3f), y + hh, j * (r - 0.3f)}, 0.17f, 0.15f, red, 8, true);
        for (int i = -1; i <= 1; i++) M.tube({i * (r - 0.3f), y + 0.1f, r - 0.3f}, {i * (r - 0.3f), y + hh, r - 0.3f}, 0.1f, 0.09f, red, 6, false);
        M.box({0, y + hh * 0.5f, r - 0.33f}, {r * 0.28f, hh * 0.38f, 0.04f}, Mat(hexColor(0x201810), 0.9f));   // sliding door
        Mat win = Mat(hexColor(0xffc070) * 0.5f, 0.3f).nightGlow(3.6f);
        M.box({r * 0.62f, y + hh * 0.55f, r - 0.33f}, {0.28f, 0.34f, 0.03f}, win); M.box({-r * 0.62f, y + hh * 0.55f, r - 0.33f}, {0.28f, 0.34f, 0.03f}, win);
        // roof: a low pyramid with upswept corners (eaves overshoot the walls by 1.4 m)
        float e = r + 1.5f, rh = 1.15f, ry = y + hh;
        vec3 c0{-e, ry, -e}, c1{e, ry, -e}, c2{e, ry, e}, c3{-e, ry, e}, top{0, ry + rh, 0};
        vec3 u0 = c0 + vec3(-0.5f, 0.55f, -0.5f), u1 = c1 + vec3(0.5f, 0.55f, -0.5f), u2 = c2 + vec3(0.5f, 0.55f, 0.5f), u3 = c3 + vec3(-0.5f, 0.55f, 0.5f);
        vec3 m01 = (c0 + c1) * 0.5f - vec3(0, 0.12f, 0), m12 = (c1 + c2) * 0.5f - vec3(0, 0.12f, 0), m23 = (c2 + c3) * 0.5f - vec3(0, 0.12f, 0), m30 = (c3 + c0) * 0.5f - vec3(0, 0.12f, 0);
        M.triP(u0, m01, top, tile); M.triP(m01, u1, top, tile); M.triP(u1, m12, top, tile); M.triP(m12, u2, top, tile);
        M.triP(u2, m23, top, tile); M.triP(m23, u3, top, tile); M.triP(u3, m30, top, tile); M.triP(m30, u0, top, tile);
        M.bevelBox({0, ry - 0.08f, 0}, {e - 0.3f, 0.12f, e - 0.3f}, 0.04f, wood);
        // hanging bell at each corner
        for (vec3 u : {u0, u1, u2, u3}) M.cone(u + vec3(0, -0.02f, 0), u + vec3(0, -0.5f, 0), 0.1f, gold, 6);
        y += hh + 0.55f;
    }
    M.tube({0, y - 0.1f, 0}, {0, y + 2.6f, 0}, 0.07f, 0.03f, gold, 6, true);
    for (int i = 0; i < 6; i++) M.lathe({{0.34f - i * 0.04f, 0}, {0.36f - i * 0.04f, 0.05f}}, gold, 10, {0, y + 0.3f + i * 0.34f, 0});
    // stone lanterns before the stair and a vermilion gate
    for (int s = -1; s <= 1; s += 2) {
        M.bevelBox({s * 2.4f, 0.5f, 7.6f}, {0.28f, 0.5f, 0.28f}, 0.05f, stone); M.bevelBox({s * 2.4f, 1.2f, 7.6f}, {0.38f, 0.24f, 0.38f}, 0.05f, Mat(pal::stoneLight, 0.8f));
        M.box({s * 2.4f, 1.2f, 7.6f}, {0.2f, 0.16f, 0.2f}, Mat(hexColor(0xffb860) * 0.5f, 0.3f).nightGlow(3.5f)); M.cone({s * 2.4f, 1.44f, 7.6f}, {s * 2.4f, 1.9f, 7.6f}, 0.5f, stone, 4);
        M.tube({s * 3.0f, 0, 10.2f}, {s * 3.0f, 4.6f, 10.2f}, 0.22f, 0.2f, red, 8, true);
    }
    M.box({0, 4.3f, 10.2f}, {3.9f, 0.14f, 0.17f}, red); M.box({0, 3.5f, 10.2f}, {3.2f, 0.1f, 0.12f}, red);
    for (int i = 0; i < 2; i++) M.quadP({-4.2f, 4.5f + i * 0.0f, 10.0f}, {4.2f, 4.5f, 10.0f}, {4.5f, 4.85f, 10.0f}, {-4.5f, 4.85f, 10.0f}, red);
    M.bakeHeightAO(-0.2f, 3.f, 0.75f);
    (void)Fo; (void)variant;
}

static void sphinx(MeshBuilder& M, int variant) {
    // a crouching guardian of weathered sandstone, nose long since eroded; facing +Z
    Mat st(lerp(sand::wall, sand::wallDark, 0.35f), 0.92f), dk(sand::wallDark, 0.92f), gold(sand::gold, 0.4f, 0.8f);
    M.jitter = 0.06f; M.jitterSeed = 91;
    M.bevelBox({0, 0.6f, 0}, {3.2f, 0.7f, 6.4f}, 0.25f, dk);                       // plinth
    M.ellipsoid({0, 2.3f, -1.6f}, vec3(2.0f, 1.7f, 3.8f), st, 12, 8);              // haunch and back
    M.ellipsoid({0, 2.7f, 1.8f}, vec3(1.45f, 1.6f, 1.7f), st, 12, 8);              // chest
    for (int s = -1; s <= 1; s += 2) {
        M.bevelBox({s * 1.15f, 1.55f, 4.4f}, {0.6f, 0.65f, 2.3f}, 0.15f, st);      // forelegs
        for (int k = 0; k < 3; k++) M.ellipsoid({s * (1.15f + (k - 1) * 0.28f), 1.0f, 6.9f}, vec3(0.2f, 0.18f, 0.32f), dk, 7, 4);
        M.ellipsoid({s * 1.7f, 1.6f, -4.1f}, vec3(0.9f, 0.9f, 1.4f), st, 9, 6);    // hind paws
    }
    M.tube({0, 3.4f, 3.0f}, {0, 5.4f, 3.5f}, 0.95f, 0.85f, st, 10, false);         // neck
    M.ellipsoid({0, 5.9f, 3.65f}, vec3(0.88f, 1.05f, 0.95f), st, 12, 9);            // head
    M.ellipsoid({0, 5.5f, 4.45f}, vec3(0.25f, 0.36f, 0.2f), dk, 6, 4);              // eroded nose
    for (int s = -1; s <= 1; s += 2) {
        M.ellipsoid({s * 0.38f, 6.05f, 4.45f}, vec3(0.15f, 0.12f, 0.06f), Mat(hexColor(0x241810), 0.9f), 6, 4);
        M.ellipsoid({s * 1.0f, 5.5f, 3.2f}, vec3(0.45f, 1.35f, 0.7f), Mat(hexColor(0x3a7a8a), 0.7f), 8, 5);   // striped headdress lappets
        M.box({s * 1.16f, 5.4f, 3.4f}, {0.04f, 1.2f, 0.48f}, gold);
    }
    M.box({0, 6.85f, 3.55f}, {0.9f, 0.1f, 0.95f}, gold);                            // diadem band
    M.ellipsoid({0, 7.0f, 3.7f}, vec3(0.1f, 0.2f, 0.08f), gold, 6, 4);
    M.jitter = 0;
    (void)variant;
    M.bakeHeightAO(-0.2f, 4.f, 0.72f);
}

static void pier(MeshBuilder& M, MeshBuilder& Fo, float deckY, int variant) {
    // a boardwalk running along local +Z (14 m), posts down to the seabed, mooring bollards, crates and lamps
    Mat plank(hexColor(0x8a6a44), 0.9f), plank2(hexColor(0x6e5236), 0.9f), post(hexColor(0x4a3626), 0.92f), rope(hexColor(0xb8a070), 0.95f);
    float L = 14.f, W = 1.2f;
    for (int i = 0; i < 28; i++) { float z = -L * 0.5f + 0.25f + i * 0.5f; M.box({0, deckY, z}, {W, 0.06f, 0.235f}, (i + variant) % 3 ? plank : plank2); }
    for (int s = -1; s <= 1; s += 2) M.box({s * (W - 0.05f), deckY - 0.12f, 0}, {0.07f, 0.1f, L * 0.5f}, post);
    for (int i = 0; i < 8; i++) {
        float z = -L * 0.5f + 0.5f + i * 2.f;
        for (int s = -1; s <= 1; s += 2) {
            M.tube({s * W, deckY - 3.2f, z}, {s * W, deckY + (i % 2 ? 0.6f : 0.3f), z}, 0.12f, 0.1f, post, 6, true);
            if (i % 2) M.tube({s * W, deckY + 0.5f, z}, {s * W, deckY + 0.5f, z + 2.f}, 0.03f, 0.03f, rope, 4, false);
        }
    }
    for (int i = 0; i < 3; i++) M.lathe({{0.12f, 0}, {0.1f, 0.3f}, {0.16f, 0.4f}, {0.01f, 0.42f}}, Mat(pal::iron, 0.5f, 0.8f), 7, {W + 0.15f, deckY + 0.06f, -3.f + i * 3.2f});
    for (int i = 0; i < 2; i++) M.bevelBox({-W + 0.4f, deckY + 0.4f + i * 0.45f * (variant % 2), -4.f + i * 0.0f}, {0.4f, 0.35f, 0.4f}, 0.04f, Mat(pal::woodLight, 0.85f));
    M.tube({0.0f, deckY, L * 0.5f - 0.6f}, {0.0f, deckY + 3.0f, L * 0.5f - 0.6f}, 0.06f, 0.05f, post, 5, true);
    M.box({0.35f, deckY + 2.85f, L * 0.5f - 0.6f}, {0.35f, 0.03f, 0.03f}, post);
    M.box({0.62f, deckY + 2.6f, L * 0.5f - 0.6f}, {0.1f, 0.14f, 0.1f}, Mat(hexColor(0xffc070) * 0.5f, 0.3f).nightGlow(3.6f));
    (void)Fo;
}

static void beachHut(MeshBuilder& M, MeshBuilder& Fo, int variant) {
    // palm-thatched hut on short posts with a veranda, hammock hooks and shell chimes
    Rng rng(variant * 53 + 5);
    Mat post(hexColor(0x6a5030), 0.92f), plank(hexColor(0xb09060), 0.9f), wall(hexColor(0xe4d4b0), 0.92f), thatch(hexColor(0xb89a50), 0.97f), thatch2(hexColor(0x9a7e3c), 0.97f);
    for (int i = -1; i <= 1; i += 2) for (int j = -1; j <= 1; j += 2) M.tube({i * 2.2f, -0.6f, j * 2.2f}, {i * 2.2f, 1.2f, j * 2.2f}, 0.12f, 0.1f, post, 6, false);
    M.box({0, 1.2f, 0.4f}, {2.5f, 0.07f, 2.9f}, plank);
    M.box({0, 2.2f, -0.2f}, {1.9f, 1.0f, 1.9f}, wall);
    M.box({0, 1.9f, 1.72f}, {0.45f, 0.8f, 0.04f}, Mat(hexColor(0x3a2616), 0.9f));
    Mat win = Mat(hexColor(0xffc070) * 0.5f, 0.3f).nightGlow(3.4f);
    M.box({1.4f, 2.4f, 1.72f}, {0.3f, 0.3f, 0.03f}, win); M.box({-1.4f, 2.4f, 1.72f}, {0.3f, 0.3f, 0.03f}, win);
    // veranda rail and posts
    for (int i = -2; i <= 2; i++) M.tube({i * 1.1f, 1.2f, 3.1f}, {i * 1.1f, 2.2f, 3.1f}, 0.04f, 0.04f, post, 4, false);
    M.box({0, 2.2f, 3.1f}, {2.4f, 0.04f, 0.04f}, post);
    for (int s = -1; s <= 1; s += 2) M.tube({s * 2.3f, 1.2f, 3.2f}, {s * 2.3f, 3.4f, 3.2f}, 0.1f, 0.08f, post, 6, true);
    // steep thatched roof (layered fringes), ridge pole
    float ry = 3.1f;
    vec3 a{-3.2f, ry, -2.4f}, b{3.2f, ry, -2.4f}, c{3.2f, ry, 3.6f}, d{-3.2f, ry, 3.6f}, ra{0, ry + 2.1f, -2.4f}, rb{0, ry + 2.1f, 3.6f};
    M.quadP(a, d, rb, ra, thatch); M.quadP(c, b, ra, rb, thatch); M.triP(a, ra, b, thatch2); M.triP(c, rb, d, thatch2);
    for (int i = 0; i < 9; i++) { float z = -2.4f + i * 0.7f; Fo.blade({-3.2f + 0.2f, ry - 0.1f, z}, {-3.5f, ry - 0.55f, z}, 0.7f, {0, 0, 1}, thatch.withWind(0.1f), thatch2.withWind(0.5f)); Fo.blade({3.2f - 0.2f, ry - 0.1f, z}, {3.5f, ry - 0.55f, z}, 0.7f, {0, 0, 1}, thatch.withWind(0.1f), thatch2.withWind(0.5f)); }
    M.tube({0, ry + 2.15f, -2.5f}, {0, ry + 2.15f, 3.7f}, 0.07f, 0.07f, post, 5, true);
    // shell chime
    for (int i = 0; i < 4; i++) { M.tube({-0.6f + i * 0.4f, ry - 0.2f, 3.4f}, {-0.6f + i * 0.4f, ry - 0.9f - 0.1f * (i % 2), 3.4f}, 0.008f, 0.008f, Mat(hexColor(0xe8e0d0), 0.9f), 3, false); M.cone({-0.6f + i * 0.4f, ry - 0.9f - 0.1f * (i % 2), 3.4f}, {-0.6f + i * 0.4f, ry - 1.1f - 0.1f * (i % 2), 3.4f}, 0.06f, Mat(hexColor(0xf4d8d0), 0.5f), 6); }
    // ladder
    for (int i = 0; i < 4; i++) M.box({0, -0.4f + i * 0.5f, 3.9f - i * 0.2f}, {0.5f, 0.03f, 0.05f}, post);
    M.bakeHeightAO(-0.6f, 2.f, 0.7f);
}

static void colonnade(MeshBuilder& M, int variant, Rng& rng) {
    // a row of fluted sandstone columns, some broken, a fallen drum, a lintel stretch still standing
    Mat st(lerp(sand::wallLight, sand::wall, 0.4f), 0.9f), dk(sand::wallDark, 0.92f);
    int n = 5;
    for (int i = 0; i < n; i++) {
        float x = (i - (n - 1) * 0.5f) * 2.5f;
        float h = (i % 3 == 1) ? rng.range(1.2f, 2.4f) : rng.range(3.6f, 4.4f);
        M.bevelBox({x, 0.2f, 0}, {0.6f, 0.25f, 0.6f}, 0.04f, dk);
        M.lathe({{0.5f, 0}, {0.42f, 0.2f}, {0.38f, h}, {0.45f, h + 0.05f}}, st, 10, {x, 0.4f, 0});
        for (int k = 0; k < 8; k++) { float a = k * TAU / 8; M.box({x + std::cos(a) * 0.4f, 0.4f + h * 0.5f, std::sin(a) * 0.4f}, {0.015f, h * 0.5f, 0.015f}, dk); }
        if (h > 3.f) M.bevelBox({x, 0.4f + h + 0.2f, 0}, {0.62f, 0.2f, 0.62f}, 0.04f, st);
    }
    M.bevelBox({-1.25f, 4.95f, 0}, {3.6f, 0.38f, 0.55f}, 0.06f, st);   // surviving lintel
    M.box({3.2f, 0.4f, 1.0f}, {0.8f, 0.4f, 0.4f}, st, quat::axisAngle({0, 1, 0}, 0.4f));   // fallen drum
    M.bakeHeightAO(-0.2f, 3.f, 0.75f);
    (void)variant;
}

static void archway(MeshBuilder& M, int variant) {
    Mat st(lerp(sand::wallLight, sand::wall, 0.5f), 0.9f), dk(sand::wallDark, 0.92f), gold(sand::gold, 0.35f, 0.9f), trim(sand::trim, 0.6f);
    for (int s = -1; s <= 1; s += 2) {
        M.bevelBox({s * 3.4f, 3.2f, 0}, {1.0f, 3.4f, 1.1f}, 0.12f, st);
        M.box({s * 3.4f, 5.4f, 1.15f}, {1.1f, 0.12f, 0.06f}, trim);
        for (int k = 0; k < 4; k++) M.box({s * 3.4f, 1.1f + k * 1.4f, 1.13f}, {0.7f, 0.025f, 0.02f}, dk);
    }
    M.bevelBox({0, 7.0f, 0}, {4.6f, 0.55f, 1.1f}, 0.1f, st);
    // the opening: a true semicircular arch of wedge-shaped stones (voussoirs) springing from the piers
    for (int i = 0; i < 9; i++) {
        float a = (i + 0.5f) * PI / 9.f;
        vec3 c{std::cos(a) * 2.5f, 4.1f + std::sin(a) * 2.5f, 0};
        M.bevelBox(c, {0.46f, 0.5f, 1.0f}, 0.06f, i % 2 ? st : Mat(sand::wallLight, 0.9f), quat::axisAngle({0, 0, 1}, a - PI * 0.5f));
    }
    M.box({0, 6.3f, 0}, {2.3f, 0.35f, 0.9f}, st);   // spandrel fill above the keystone
    for (int i = 0; i < 7; i++) M.box({-3.6f + i * 1.2f, 7.85f, 0}, {0.3f, 0.3f, 0.5f}, st);   // merlons
    M.lathe({{0.01f, 0}, {0.9f, 0.05f}, {0.9f, 0.12f}, {0.01f, 0.16f}}, gold, 14, {0, 6.9f, 1.18f}, quat::axisAngle({1, 0, 0}, PI / 2));
    M.box({0, 8.3f, 0}, {4.8f, 0.14f, 1.2f}, dk);
    (void)variant;
}


// 2.0.0 village dressing: every dwelling gets its yard (barrels and crates, hay, jars, nets, boats, palms, carpets...), chosen by
// region and placed in the building's own frame (door on +Z). Deterministic per structure; skips roads, water, steep ground
// and other buildings. Props only (no new structures), so the placement audits are unaffected.
// 2.2.1 the celestial portal: a stone dais, a ring of rune-cut blocks with a burning gold inner rim, a vortex of light
// (concentric glowing discs and spiral arms), two pylons crowned with floating crystals, rune stones orbiting. The ring
// is open at its foot so the hero walks straight in. Facing +Z. (The swirling sparks are particles, Game.cpp.)
static void portalGate(MeshBuilder& M, MeshBuilder& Fo) {
    Mat stone(hexColor(0x5a5468), 0.8f), stone2(hexColor(0x6e6880), 0.8f), gold(hexColor(0xe8c060), 0.35f, 0.9f, 1.4f);
    M.lathe({{6.0f, 0}, {6.0f, 0.25f}, {5.4f, 0.3f}, {5.4f, 0.55f}, {4.8f, 0.6f}, {4.8f, 0.85f}, {0.01f, 0.86f}}, stone, 24);
    for (int i = 0; i < 24; i++) { float a = i * TAU / 24; M.box({std::cos(a) * 4.8f, 0.87f, std::sin(a) * 4.8f}, {0.25f, 0.02f, 0.08f}, Mat(hexColor(0x9af0ff), 0.2f, 0, 2.5f), quat::axisAngle({0, 1, 0}, -a)); }
    const float R = 4.3f, cy = 0.86f + 4.4f;
    int n = 26;
    for (int i = 0; i < n; i++) {
        float a = (i + 0.5f) * TAU / n;   // angle in the ring plane, 0 = right
        if (std::sin(a) < -0.86f) continue;   // the foot of the ring stays open
        vec3 c{std::cos(a) * R, cy + std::sin(a) * R, 0};
        quat q = quat::axisAngle({0, 0, 1}, a);
        M.bevelBox(c, {0.62f, 0.5f, 0.55f}, 0.08f, i % 2 ? stone : stone2, q);
        M.box(c + q.rotate(vec3(0.3f, 0, 0.56f)), {0.12f, 0.22f, 0.01f}, Mat(hexColor(0x9af0ff), 0.2f, 0, 3.f), q);   // rune
        vec3 ci{std::cos(a) * (R - 0.62f), cy + std::sin(a) * (R - 0.62f), 0};
        M.box(ci, {0.08f, 0.52f, 0.36f}, gold, q);
    }
    // keystone with a crown of crystal
    M.bevelBox({0, cy + R + 0.6f, 0}, {0.8f, 0.6f, 0.7f}, 0.1f, stone2);
    M.lathe({{0.01f, 0}, {0.35f, 0.5f}, {0.01f, 1.6f}}, Mat(hexColor(0x9af0ff), 0.15f, 0, 3.f), 6, {0, cy + R + 1.2f, 0});
    // the vortex: layered discs facing +Z, hottest in the centre, with six spiral arms
    for (int k = 0; k < 5; k++) {
        float rr = (R - 0.7f) * (1.f - k * 0.18f);
        vec3 col = lerp(hexColor(0x3a6aff), hexColor(0xf0d8ff), k / 4.f);
        M.lathe({{rr, 0}, {0.01f, 0}}, Mat(col * 0.7f, 0.2f, 0, 1.2f + k * 0.7f), 32, {0, cy, -0.06f + k * 0.03f}, quat::axisAngle({1, 0, 0}, PI / 2));
        M.lathe({{rr, 0}, {0.01f, 0}}, Mat(col * 0.7f, 0.2f, 0, 1.2f + k * 0.7f), 32, {0, cy, 0.06f - k * 0.03f}, quat::axisAngle({1, 0, 0}, -PI / 2));
    }
    for (int arm = 0; arm < 6; arm++)
        for (int j = 0; j < 9; j++) {
            float u = j / 8.f, a = arm * TAU / 6 + u * 2.6f, rr = (R - 0.9f) * (1.f - u * 0.9f);
            M.box({std::cos(a) * rr, cy + std::sin(a) * rr, 0.1f}, {0.28f - u * 0.2f, 0.05f, 0.02f}, Mat(hexColor(0xffffff), 0.2f, 0, 3.5f), quat::axisAngle({0, 0, 1}, a + PI / 2));
        }
    // pylons with floating crystals
    for (int sd = -1; sd <= 1; sd += 2) {
        vec3 b{sd * 6.6f, 0, 0.5f};
        M.lathe({{0.9f, 0}, {0.75f, 0.4f}, {0.55f, 4.4f}, {0.8f, 4.7f}, {0.8f, 5.0f}, {0.01f, 5.0f}}, stone, 8, b);
        for (int k = 0; k < 4; k++) M.box(b + vec3(0, 1.2f + k * 0.9f, 0.62f), {0.18f, 0.3f, 0.01f}, Mat(hexColor(0x9af0ff), 0.2f, 0, 2.5f));
        M.lathe({{0.01f, 0}, {0.5f, 0.7f}, {0.01f, 2.2f}}, Mat(hexColor(0xc8a8ff), 0.15f, 0, 2.8f), 6, b + vec3(0, 5.8f, 0));
        M.lathe({{0.01f, 0}, {0.22f, 0.3f}, {0.01f, 0.9f}}, Mat(hexColor(0x9af0ff), 0.15f, 0, 3.f), 6, b + vec3(0.9f * sd, 6.4f, 0.4f));
    }
    // rune stones orbiting the ring (frozen in mid-air)
    for (int i = 0; i < 8; i++) { float a = i * TAU / 8 + 0.2f; M.rock({std::cos(a) * (R + 1.8f), cy + std::sin(a) * (R + 1.4f), 0.4f * (i % 2 ? 1 : -1)}, vec3(0.35f, 0.45f, 0.3f), stone2, 700 + i, 0); }
    M.bakeHeightAO(-0.2f, 2.f, 0.8f);
    (void)Fo;
}

void dressStructure(ChunkBuild& out, const Structure& s, const World& w) {
    bool house = s.kind == S_HOUSE || s.kind == S_STONEHOUSE || s.kind == S_LONGHOUSE || s.kind == S_DOMEHOUSE || s.kind == S_BEACHHUT ||
                 s.kind == S_STILT_HUT || s.kind == S_INN || s.kind == S_FORGE || s.kind == S_CHAPEL || s.kind == S_BAZAAR || s.kind == S_TENT;
    if (!house || s.poi < 0) return;
    const Poi& P = w.pois[s.poi];
    if (P.type != PoiType::Village && P.type != PoiType::Monastery && P.type != PoiType::Mill && P.type != PoiType::Farm && P.type != PoiType::Camp && P.type != PoiType::Temple) return;
    Rng r(hash2i((int)(s.pos.x * 3.f), (int)(s.pos.y * 3.f), 8811 + s.kind));
    vec2 fp = structureFootprint(s.kind) * s.scale;
    int reg = s.region;
    auto okAt = [&](vec2 wp, float margin) {
        if (w.roadInfluence(wp.x, wp.y) > 0.25f) return false;
        float y = w.height(wp.x, wp.y);
        float wl = w.waterLevel(wp.x, wp.y);
        if (wl > -1e8f && wl > y - 0.25f) return false;
        if (w.normal(wp.x, wp.y).y < 0.86f) return false;
        ChunkKey k = chunkOf(wp.x, wp.y);
        for (int dz = -1; dz <= 1; dz++) for (int dx = -1; dx <= 1; dx++) {
            int cj = chunkIndex(k.x + dx, k.z + dz);
            if (cj < 0) continue;
            for (int si : w.chunkStructures[cj]) {
                const Structure& o = w.structures[si];
                if (o.kind == S_FIELD || o.kind == S_LANTERN || o.kind == S_STALL) { if (o.kind != S_FIELD && length(o.pos - wp) < (o.kind == S_STALL ? 2.6f : 1.6f)) return false; continue; }   // 3.0.0 a stall's corners reach 1.8 m
                if (o.kind == S_PEN) continue;
                vec2 f2 = structureFootprint(o.kind) * o.scale;
                vec2 l = rotate(wp - o.pos, o.rot);
                if (o.kind == S_PIER) { if (std::fabs(l.x) < 2.5f && std::fabs(l.y) < 8.f) return false; continue; }
                if (std::fabs(l.x) < f2.x + margin && std::fabs(l.y) < f2.y + margin) return false;
            }
        }
        return true;
    };
    auto put = [&](PropKind k, int var, vec2 local, float yaw, float sc, bool collide) {
        vec2 wp = rotate(local, -s.rot) + s.pos;
        if (!okAt(wp, 0.7f)) return false;
        float y = w.height(wp.x, wp.y);
        stampProp(out, k, var, {wp.x, y - 0.04f, wp.y}, s.rot + yaw, sc, vec3(1) * r.range(0.92f, 1.06f), collide);
        return true;
    };
    float fx = fp.x, fz = fp.y;
    int sd = r.chance(0.5f) ? 1 : -1;
    if (s.kind == S_BAZAAR || s.kind == S_TENT) {
        if (reg == R_AMBER) { put(P_CARPET, r.irange(0, 2), {0, fz + 1.8f}, r.range(-0.2f, 0.2f), 1.f, false); put(P_AMPHORA, r.irange(0, 2), {sd * (fx + 1.2f), fz * 0.4f}, r.range(0, TAU), 1.f, true); }
        else { put(P_BARREL, 0, {sd * (fx + 1.f), fz * 0.5f}, 0, 1.f, true); put(P_CRATE, 0, {-sd * (fx + 1.1f), fz * 0.3f}, r.range(0, TAU), 1.f, true); }
        return;
    }
    switch (reg) {
    case R_AMBER: {
        put(P_AMPHORA, r.irange(0, 2), {sd * (fx * 0.6f), fz + 1.1f}, r.range(0, TAU), 1.f, true);
        if (r.chance(0.6f)) put(P_CARPET, r.irange(0, 2), {-sd * 0.3f, fz + 1.6f + (s.kind == S_DOMEHOUSE ? 1.0f : 0.f)}, r.range(-0.15f, 0.15f), 0.9f, false);
        if (r.chance(0.5f)) put(P_AGAVE, r.irange(0, 2), {-sd * (fx + 1.3f), r.range(-fz, fz)}, r.range(0, TAU), 1.f, false);
        if (r.chance(0.35f)) put(P_DATEPALM, r.irange(0, 3), {sd * (fx + 2.6f), r.range(-fz, fz * 0.6f)}, r.range(0, TAU), 1.f, true);
        if (r.chance(0.3f)) put(P_BRAZIER, 0, {-sd * (fx * 0.7f), fz + 1.6f}, 0, 1.f, true);
        if (r.chance(0.35f)) put(P_CRATE, 0, {-sd * (fx * 0.5f), -fz - 1.2f}, r.range(0, TAU), 1.f, true);
        break;
    }
    case R_CORAL: {
        put(P_BARREL, 0, {sd * (fx * 0.7f), fz + 1.0f}, 0, 1.f, true);
        if (r.chance(0.5f)) put(P_NETRACK, 0, {-sd * (fx + 2.4f), r.range(-fz * 0.5f, fz * 0.5f)}, PI / 2, 1.f, true);
        if (r.chance(0.35f)) put(P_BOAT, r.irange(0, 2), {sd * (fx + 3.2f), r.range(-fz, fz * 0.4f)}, PI / 2 + r.range(-0.3f, 0.3f), 1.f, true);
        if (r.chance(0.5f)) put(P_PALM, r.irange(0, 3), {-sd * (fx + 2.0f), -fz * 0.2f + r.range(-1.f, 1.f)}, r.range(0, TAU), r.range(0.85f, 1.15f), true);
        if (r.chance(0.5f)) put(P_TROPBUSH, r.irange(0, 2), {sd * (fx * 0.3f), fz + 1.6f}, r.range(0, TAU), 1.f, false);
        if (r.chance(0.12f)) put(P_HAMMOCK, 0, {-sd * (fx + 3.2f), fz * 0.6f}, PI / 2, 1.f, true);
        if (r.chance(0.4f)) put(P_SHELLS, 0, {r.range(-fx, fx), fz + 2.2f}, r.range(0, TAU), 1.f, false);
        break;
    }
    case R_FROSTHEIM: {
        put(P_BARREL, 0, {sd * (fx * 0.7f), fz + 1.0f}, 0, 1.f, true);
        put(P_CRATE, 0, {-sd * (fx * 0.6f), fz + 1.2f}, r.range(0, TAU), 1.f, true);
        if (r.chance(0.5f)) put(P_SNOWROCK, r.irange(0, 2), {sd * (fx + 1.8f), r.range(-fz, fz)}, r.range(0, TAU), r.range(0.4f, 0.7f), false);
        if (r.chance(0.4f)) put(P_SNOWPINE, r.irange(0, 3), {-sd * (fx + 3.0f), r.range(-fz, fz)}, r.range(0, TAU), r.range(0.7f, 1.f), true);
        break;
    }
    case R_ELDERWOOD: {
        put(P_SMALLSHROOMS, r.irange(0, 2), {sd * (fx + 0.8f), fz * 0.2f}, r.range(0, TAU), 1.2f, false);
        put(P_FERN, r.irange(0, 2), {-sd * (fx + 0.9f), -fz * 0.3f}, r.range(0, TAU), 1.2f, false);
        if (r.chance(0.5f)) put(P_BARREL, 0, {sd * (fx * 0.6f), fz + 1.0f}, 0, 1.f, true);
        if (r.chance(0.3f)) put(P_ELDERTREE, r.irange(0, 2), {-sd * (fx + 4.f), r.range(-fz, fz)}, r.range(0, TAU), r.range(0.5f, 0.7f), true);
        break;
    }
    case R_SHADOWS: case R_ASHEN: {
        put(P_BARREL, 0, {sd * (fx * 0.7f), fz + 1.0f}, 0, 1.f, true);
        if (r.chance(0.5f)) put(P_CRATE, 0, {-sd * (fx * 0.6f), fz + 1.1f}, r.range(0, TAU), 1.f, true);
        if (r.chance(0.35f)) put(reg == R_SHADOWS ? P_DEADTREE : P_CHARRED, r.irange(0, 2), {-sd * (fx + 3.f), r.range(-fz, fz)}, r.range(0, TAU), r.range(0.6f, 0.9f), true);
        break;
    }
    default: {   // Greenhaven, High Crowns
        put(P_BARREL, 0, {sd * (fx * 0.7f), fz + 1.0f}, 0, 1.f, true);
        if (r.chance(0.55f)) put(P_CRATE, 0, {-sd * (fx * 0.55f), fz + 1.2f}, r.range(0, TAU), 1.f, true);
        if (r.chance(0.45f)) put(P_HAYBALE, 0, {sd * (fx + 1.3f), r.range(-fz, fz * 0.5f)}, r.range(0, TAU), 1.f, true);
        if (r.chance(0.5f)) put(P_FLOWERS, r.irange(0, 2), {-sd * (fx * 0.4f), fz + 1.9f}, r.range(0, TAU), 1.3f, false);
        if (r.chance(0.3f)) put(P_FENCE, 0, {sd * (fx + 2.2f), 0}, PI / 2, 1.f, false);
        if (r.chance(0.35f)) put(reg == R_HIGHCROWNS ? P_PINE : P_OAK, r.irange(0, 3), {-sd * (fx + 3.5f), r.range(-fz, fz)}, r.range(0, TAU), r.range(0.65f, 0.95f), true);
        if (r.chance(0.12f)) put(P_CART, r.irange(0, 2), {sd * (fx + 2.6f), fz * 0.2f}, r.range(0, TAU), 1.f, true);
        break;
    }
    }
}

void buildStructure(ChunkBuild& out, const Structure& s, const World& w) {
    float gy = structureGroundY(s, w);
    MeshBuilder M, Fo;
    // 1.10.1 stone foundation under a building raised on a slope (dry stone, slightly wider than the walls)
    if (float pl = structurePlinth(s, w); pl > 0.05f) {
        vec2 fp = structureFootprint(s.kind) * 1.04f;
        float bot = -pl - 0.4f;
        Mat st(lerp(pal::stoneDark, pal::stone, 0.65f), 0.9f);
        M.bevelBox({0, (bot + 0.06f) * 0.5f, 0}, {fp.x, (0.06f - bot) * 0.5f, fp.y}, 0.08f, st);
        M.box({0, 0.04f, 0}, {fp.x + 0.08f, 0.05f, fp.y + 0.08f}, Mat(pal::stone, 0.85f));   // coping course
    }
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
    case S_MINE: { mine(M); addBoxCollider(out, s, gy, {0, -4.2f}, 6.5f, 3.4f, 6.f); addCircleCollider(out, s, gy, {-3.6f, -1.f}, 2.1f, 4.f); addCircleCollider(out, s, gy, {3.6f, -1.f}, 2.1f, 4.f);
        vec2 lp = s.pos + rotate(vec2(0, 1.15f), -s.rot); out.lights.push_back(vec4(lp.x, gy + 2.6f, lp.y, LI_LANTERN)); break; }
    case S_WAYSTONE: waystone(M); addCircleCollider(out, s, gy, L, 0.7f, 3.f); out.lights.push_back(vec4(s.pos.x, gy + 2.2f, s.pos.y, LI_MAGIC)); out.emitters.push_back(vec4(s.pos.x, gy + 1.f, s.pos.y, EM_SPARKLES)); break;
    case S_CAMPFIRE: campfire(M); out.lights.push_back(vec4(s.pos.x, gy + 1.2f, s.pos.y, LI_FIRE)); out.emitters.push_back(vec4(s.pos.x, gy + 0.2f, s.pos.y, EM_FIRE)); break;
    case S_BRIDGE: bridgeV2(M, out, s, w, gy); break;
    case S_TENT: tent(M, s.variant); addBoxCollider(out, s, gy, L, 2.f, 3.f, 2.4f); break;
    case S_MONASTERY: monastery(M); addBoxCollider(out, s, gy, L, 10.3f, 7.3f, 12.f); break;
    case S_STATUE: statue(M); addBoxCollider(out, s, gy, L, 1.6f, 1.6f, 5.f); break;
    case S_FORGE: forge(M); addBoxCollider(out, s, gy, {-1.3f, -1.2f}, 1.f, 0.8f, 3.f); out.lights.push_back(vec4(s.pos.x, gy + 1.5f, s.pos.y, LI_FIRE)); { vec2 ch = rotate(vec2(-1.3f, -1.4f) * s.scale, -s.rot) + s.pos; out.emitters.push_back(vec4(ch.x, gy + 4.4f, ch.y, EM_SMOKE)); } break;
    case S_LANTERN: lantern(M); addCircleCollider(out, s, gy, L, 0.15f, 2.6f); { vec2 lp = rotate(vec2(0.55f, 0) * s.scale, -s.rot) + s.pos; out.lights.push_back(vec4(lp.x, gy + 2.2f, lp.y, LI_LANTERN)); } break;
    case S_PALISADE: palisade(M); addBoxCollider(out, s, gy, L, 1.9f, 0.25f, 3.f); break;
    case S_PEN: pen(M, rng);
        addBoxCollider(out, s, gy, {0, -5.f}, 5.f, 0.12f, 1.2f); addBoxCollider(out, s, gy, {5.f, 0}, 0.12f, 5.f, 1.2f); addBoxCollider(out, s, gy, {-5.f, 0}, 0.12f, 5.f, 1.2f);
        addBoxCollider(out, s, gy, {-3.2f, 5.f}, 1.8f, 0.12f, 1.2f); addBoxCollider(out, s, gy, {3.2f, 5.f}, 1.8f, 0.12f, 1.2f); break;
    case S_BANNER: bannerPole(M, Fo); addCircleCollider(out, s, gy, L, 0.25f, 6.f);
        out.lights.push_back(vec4(s.pos.x, gy + 1.6f, s.pos.y, LI_FIRE)); { vec2 bp = rotate(vec2(-1.4f, 0.6f) * s.scale, -s.rot) + s.pos; out.emitters.push_back(vec4(bp.x, gy + 1.1f, bp.y, EM_FIRE)); } break;
    case S_ALTAR: altar(M); addBoxCollider(out, s, gy, L, 1.6f, 0.9f, 1.2f); out.lights.push_back(vec4(s.pos.x, gy + 1.8f, s.pos.y, LI_MAGIC)); break;
    // ---- 2.0.0
    case S_DOMEHOUSE: domeHouse(M, Fo, s.variant, reg); addBoxCollider(out, s, gy, L, 3.4f, 3.5f, 5.f); break;
    case S_BAZAAR: bazaarHall(M, Fo, s.variant, reg);
        addBoxCollider(out, s, gy, {-1.6f, 1.3f}, 1.3f, 0.6f, 1.4f); addBoxCollider(out, s, gy, {1.6f, 1.3f}, 1.3f, 0.6f, 1.4f);
        out.lights.push_back(vec4(s.pos.x, gy + 2.6f, s.pos.y, LI_LANTERN)); break;
    case S_MINARET: minaret(M, s.variant); addCircleCollider(out, s, gy, L, 1.6f, 17.f); out.lights.push_back(vec4(s.pos.x, gy + 13.2f, s.pos.y, LI_LANTERN)); break;
    case S_ZIGGURAT: ziggurat(M, Fo, s.variant);
        addBoxCollider(out, s, gy, {-7.2f, 0}, 3.8f, 10.f, 8.f); addBoxCollider(out, s, gy, {7.2f, 0}, 3.8f, 10.f, 8.f); addBoxCollider(out, s, gy, {0, -7.2f}, 5.f, 3.8f, 8.f); addBoxCollider(out, s, gy, {0, 3.f}, 5.f, 6.5f, 8.f);
        out.lights.push_back(vec4(s.pos.x, gy + 9.4f * s.scale, s.pos.y, LI_FIRE)); out.emitters.push_back(vec4(s.pos.x, gy + 9.2f * s.scale, s.pos.y, EM_FIRE)); break;
    case S_LIGHTHOUSE: lighthouse(M, Fo); addCircleCollider(out, s, gy, L, 2.9f, 22.f); out.lights.push_back(vec4(s.pos.x, gy + 18.f * s.scale, s.pos.y, LI_FIRE)); out.emitters.push_back(vec4(s.pos.x, gy + 18.2f * s.scale, s.pos.y, EM_SPARKLES)); break;
    case S_PAGODA: pagoda(M, Fo, s.variant); addBoxCollider(out, s, gy, L, 5.4f, 5.4f, 14.f); addCircleCollider(out, s, gy, {-3.0f, 10.2f}, 0.3f, 5.f); addCircleCollider(out, s, gy, {3.0f, 10.2f}, 0.3f, 5.f);
        out.lights.push_back(vec4(s.pos.x, gy + 2.2f, s.pos.y, LI_MAGIC)); out.emitters.push_back(vec4(s.pos.x, gy + 1.2f, s.pos.y, EM_SPARKLES)); break;
    case S_SPHINX: sphinx(M, s.variant); addBoxCollider(out, s, gy, {0, -1.f}, 2.6f, 5.6f, 6.f); addBoxCollider(out, s, gy, {0, 3.6f}, 2.4f, 3.f, 7.f); out.lights.push_back(vec4(s.pos.x, gy + 6.2f, s.pos.y, LI_MAGIC)); break;
    case S_PIER: { float lv = w.waterLevel(s.pos.x, s.pos.y); float deck = (lv > -1e8f ? lv : gy) + 0.75f; pier(M, Fo, deck - gy, s.variant);
        out.lights.push_back(vec4(s.pos.x, deck + 2.7f, s.pos.y, LI_LANTERN)); break; }
    case S_BEACHHUT: beachHut(M, Fo, s.variant); addBoxCollider(out, s, gy, {0, 0.4f}, 2.5f, 2.9f, 3.f); break;
    case S_COLONNADE: colonnade(M, s.variant, rng); for (int i = 0; i < 5; i++) addCircleCollider(out, s, gy, {(i - 2) * 2.5f, 0}, 0.5f, 4.f); break;
    case S_ARCHWAY: archway(M, s.variant); addBoxCollider(out, s, gy, {-3.4f, 0}, 1.0f, 1.1f, 7.f); addBoxCollider(out, s, gy, {3.4f, 0}, 1.0f, 1.1f, 7.f); addBoxCollider(out, s, gy, {0, 0}, 3.0f, 1.0f, 3.f); out.colliders.back().base = gy + 6.f; out.colliders.back().top = 3.f; break;
    case S_GEYSER: {
        // 2.8.0 (reworked) a hot spring geyser: terraces of cream and ochre sinter round a clear blue-green pool,
        // a pale throat in the middle, wet dark stones, and a column of steam
        Mat sinter(hexColor(0xd8ccb0), 0.7f), ochre(hexColor(0xb88a50), 0.8f), wet(hexColor(0x4a4844), 0.4f);
        M.lathe({{2.6f, -0.05f}, {2.2f, 0.08f}, {1.75f, 0.12f}, {1.5f, 0.22f}, {1.3f, 0.2f}}, sinter, 20);   // outer terrace
        M.lathe({{1.3f, 0.2f}, {1.22f, 0.12f}}, ochre, 20);                                                     // ochre lip
        M.flat = true;
        for (int i = 0; i < 7; i++) {
            float a = i * TAU / 7 + rng.range(-0.2f, 0.2f), rr = rng.range(1.9f, 2.6f);
            M.rock({std::cos(a) * rr, 0.05f, std::sin(a) * rr}, vec3(rng.range(0.25f, 0.45f), rng.range(0.15f, 0.3f), rng.range(0.25f, 0.4f)), i % 2 ? wet : Mat(hexColor(0x6a665e), 0.8f), rng.next(), 0);
        }
        M.flat = false;
        M.ellipsoid({0, 0.1f, 0}, vec3(1.22f, 0.03f, 1.22f), Mat(hexColor(0x3a8a90), 0.05f, 0.f, 0.35f), 16, 3);    // the pool (clear, a faint glow)
        M.ellipsoid({0, 0.12f, 0}, vec3(0.42f, 0.03f, 0.42f), Mat(hexColor(0xbfe8e0), 0.05f, 0.f, 0.6f), 10, 3);   // the throat
        out.emitters.push_back(vec4(s.pos.x, gy + 0.4f, s.pos.y, EM_MIST));
        out.emitters.push_back(vec4(s.pos.x, gy + 1.4f, s.pos.y, EM_MIST));
        break;
    }
    case S_SKYISLAND: {
        // 2.8.0 (reworked) a floating island torn from the ground: an uneven grassy top with patches of earth and
        // moss, a band of dark soil, then rock in strata narrowing to a crag, roots hanging from the edges, a few
        // quartz crystals under it. On top: a birch, boulders, bushes, a chest, a vein of aurora crystal.
        float off = s.variant / 100.f - gy;
        const float R = SKY_ISLAND_R;
        float ph1 = rng.range(0, TAU), ph2 = rng.range(0, TAU);
        auto outline = [&](float a) { return 1.f + 0.09f * std::sin(3.f * a + ph1) + 0.06f * std::sin(5.f * a + ph2) + 0.03f; };   // >= 0.95
        size_t a0 = M.v.size();
        M.lathe({{R * 1.02f, off - 0.2f}, {R * 0.92f, off + 0.12f}, {R * 0.6f, off + 0.3f}, {R * 0.25f, off + 0.38f}, {0.f, off + 0.4f}}, Mat(hexColor(0x5f7f3e), 0.95f), 24);
        size_t a1 = M.v.size();
        M.lathe({{0.05f, off - 13.f}, {R * 0.15f, off - 11.5f}, {R * 0.3f, off - 9.f}, {R * 0.5f, off - 6.5f}, {R * 0.7f, off - 4.f}, {R * 0.86f, off - 2.2f}, {R * 0.97f, off - 0.9f}, {R * 1.02f, off - 0.2f}}, Mat(hexColor(0x7a746a), 0.95f), 24);
        size_t a2 = M.v.size();
        for (size_t i = a0; i < a2; i++) {
            Vertex& v = M.v[i];
            float ang = std::atan2(v.pz, v.px);
            float f = outline(ang);
            bool top = i < a1;
            float n = fbm2(v.px * 0.35f + s.pos.x, v.pz * 0.35f + s.pos.y + v.py * 0.3f, 3, 991);
            if (!top) f *= 1.f + n * 0.16f;
            v.px *= f; v.pz *= f;
            if (top) {
                v.py += n * 0.25f * std::min(1.f, std::sqrt(v.px * v.px + v.pz * v.pz) / R);   // gentle hummocks
                float m = fbm2(v.px * 0.25f + 11.f + s.pos.x, v.pz * 0.25f + s.pos.y, 2, 992) * 0.5f + 0.5f;
                vec3 c = lerp(hexColor(0x3c5628), hexColor(0x6e6c3a), smoothstepf(0.4f, 0.75f, m)) * 0.85f;   // moss -> dry grass
                if (m < 0.22f) c = lerp(c, hexColor(0x5a4a36), 0.7f);                                    // bare earth
                v.r = (uint8_t)clampf(c.x * 255, 0, 255); v.g = (uint8_t)clampf(c.y * 255, 0, 255); v.b = (uint8_t)clampf(c.z * 255, 0, 255);
            } else {
                float depth = off - v.py;
                vec3 c = depth < 1.2f ? hexColor(0x6a5038) : (depth < 2.f ? hexColor(0x847058) : hexColor(0xa09888));   // soil, subsoil, rock (lit from below only by the sky: kept light)
                if (depth >= 2.f) { float band = 0.82f + 0.2f * std::sin(v.py * 2.7f + n * 3.f); c *= band; if (std::fmod(depth, 3.1f) < 0.5f) c = lerp(c, hexColor(0x8a7a62), 0.5f); }
                v.r = (uint8_t)clampf(c.x * 255, 0, 255); v.g = (uint8_t)clampf(c.y * 255, 0, 255); v.b = (uint8_t)clampf(c.z * 255, 0, 255);
            }
        }
        Mat root(hexColor(0x3a2c20), 0.9f);
        for (int i = 0; i < 16; i++) {   // roots hanging from the torn edge
            float a = rng.range(0, TAU); float rr = R * outline(a) * rng.range(0.85f, 0.97f);
            vec3 b{std::cos(a) * rr, off - rng.range(0.4f, 1.2f), std::sin(a) * rr};
            vec3 m1 = b + vec3(rng.range(-0.3f, 0.3f), -rng.range(0.8f, 1.6f), rng.range(-0.3f, 0.3f));
            vec3 e = m1 + vec3(rng.range(-0.4f, 0.4f), -rng.range(0.6f, 2.f), rng.range(-0.4f, 0.4f));
            M.tube(b, m1, 0.07f, 0.045f, root, 4, false); M.tube(m1, e, 0.045f, 0.012f, root, 4, false);
        }
        M.flat = true;
        for (int i = 0; i < 4; i++) {   // a few quartz crystals in the crag
            float a = rng.range(0, TAU), rr = rng.range(R * 0.2f, R * 0.5f);
            vec3 b{std::cos(a) * rr, off - 6.f - rng.range(0.f, 3.f), std::sin(a) * rr};
            M.cone(b, b + normalize(vec3(std::cos(a), -0.6f, std::sin(a))) * rng.range(0.6f, 1.2f), 0.18f, Mat(lerp(hexColor(0xe8ecee), hexColor(0xa898c8), rng.f() * 0.4f), 0.12f, 0, 0.3f), 6);
        }
        for (int i = 0; i < 4; i++) {   // boulders on the deck
            float a = rng.range(0, TAU), rr = rng.range(R * 0.45f, R * 0.8f);
            M.rock({std::cos(a) * rr, off + 0.25f, std::sin(a) * rr}, vec3(rng.range(0.4f, 0.8f), rng.range(0.3f, 0.5f), rng.range(0.4f, 0.7f)), Mat(hexColor(0x6e6a62), 0.9f), rng.next(), 0);
        }
        M.flat = false;
        {   // a birch and two bushes
            const PropTemplate& t = propTemplate(P_CRYSTALTREE, rng.irange(0, 2));
            mat4 pm = mat4::trs({-2.5f, off + 0.3f, 1.5f}, rotY(rng.range(0, TAU)), vec3(1.0f));
            appendXf(M, t.solid, pm, vec3(1)); appendXf(Fo, t.foliage, pm, vec3(1));
            addCircleCollider(out, s, gy, {-2.5f, 1.5f}, 0.25f, off + 5.f); out.colliders.back().base = s.variant / 100.f - 0.2f; out.colliders.back().top = 5.f;
            for (int q = 0; q < 2; q++) {
                const PropTemplate& bt = propTemplate(P_BUSH, q);
                float a = rng.range(0, TAU);
                mat4 bm = mat4::trs({std::cos(a) * R * 0.55f, off + 0.3f, std::sin(a) * R * 0.55f}, rotY(rng.range(0, TAU)), vec3(0.9f));
                appendXf(M, bt.solid, bm, vec3(1)); appendXf(Fo, bt.foliage, bm, vec3(1));
            }
        }
        auto wpos = [&](vec2 l, float y) { vec2 q = rotate(l, -s.rot) + s.pos; return vec3(q.x, y, q.y); };
        NodeSpawn ch; ch.type = NodeType::Chest; ch.pos = wpos({2.4f, -1.2f}, s.variant / 100.f + 0.35f); ch.rot = s.rot + 0.6f; ch.tier = 3;
        ch.uid = hash32(0x5C1E0000u + (uint32_t)((int)s.pos.x * 7919 + (int)s.pos.y)); out.nodes.push_back(ch);
        NodeSpawn cr; cr.type = NodeType::AuroraCrystal; cr.pos = wpos({0.5f, 3.2f}, s.variant / 100.f + 0.3f); cr.rot = s.rot; cr.tier = 3;
        cr.uid = hash32(0x5C1F0000u + (uint32_t)((int)s.pos.x * 7919 + (int)s.pos.y)); out.nodes.push_back(cr);
        break;
    }
    case S_PORTAL: portalGate(M, Fo);
        addCircleCollider(out, s, gy, {-4.3f, 0}, 0.7f, 9.f); addCircleCollider(out, s, gy, {4.3f, 0}, 0.7f, 9.f);
        addCircleCollider(out, s, gy, {-6.6f, 0.5f}, 0.9f, 7.f); addCircleCollider(out, s, gy, {6.6f, 0.5f}, 0.9f, 7.f);
        out.lights.push_back(vec4(s.pos.x, gy + 5.2f, s.pos.y, LI_MAGIC)); out.lights.push_back(vec4(s.pos.x, gy + 1.2f, s.pos.y + 2.f, LI_CRYSTAL));
        out.emitters.push_back(vec4(s.pos.x, gy + 5.2f, s.pos.y, EM_SPARKLES)); break;
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
    // 1.10.1 fields and livestock pens drape over the terrain (rows and fence posts follow the slope)
    if (s.kind == S_FIELD || s.kind == S_PEN) {
        for (MeshBuilder* mb : {&M, &Fo})
            for (Vertex& v : mb->v) {
                vec2 wp = rotate(vec2(v.px, v.pz) * s.scale, -s.rot) + s.pos;
                v.py += (w.height(wp.x, wp.y) - gy) / std::max(0.01f, s.scale);
            }
    }
    int pm = propModelFor(s.kind, s.variant);
    if (PropModel* P = propModel(pm)) {
        // fitted inside the footprint, centred, base slightly sunk so it never floats on a slope
        vec2 fp = structureFootprint(s.kind);
        vec3 ext = P->bmax - P->bmin;
        float k = std::min(2.f * fp.x / std::max(0.01f, ext.x), 2.f * fp.y / std::max(0.01f, ext.z));
        // 2.2.1 fitted to the footprint alone, some models came out at doll size next to a 1.8 m hero (a forge below the
        // hero's shoulder, a knee-high well, a tower of 5 m): each kind has a believable height, the model grows to it
        // (at most 1.7 x the footprint fit, so neighbours are not swallowed)
        float wantH = 0.f;
        switch (s.kind) {
        case S_HOUSE: wantH = 7.f; break; case S_INN: wantH = 10.f; break; case S_STONEHOUSE: wantH = 6.8f; break; case S_LONGHOUSE: wantH = 6.5f; break;
        case S_CHAPEL: wantH = 11.f; break; case S_FORGE: wantH = 5.2f; break; case S_TOWER: wantH = 13.f; break; case S_WELL: wantH = 2.8f; break;
        case S_STALL: wantH = 2.9f; break; case S_TENT: wantH = 2.7f; break; default: break;
        }
        if (wantH > 0.f && ext.y > 0.01f) k = clampf(wantH / ext.y, k, k * 1.7f);
        vec3 c = (P->bmin + P->bmax) * 0.5f;
        mat4 local = mat4::trs(vec3(-c.x * k, -P->bmin.y * k - 0.12f, -c.z * k), quat(), vec3(k));
        out.models.push_back({pm, m * local});
        return;
    }
    appendXf(out.props, M, m, vec3(1));
    if (!Fo.v.empty()) appendXf(out.foliage, Fo, m, vec3(1));
}

}  // namespace lk
