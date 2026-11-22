#include "Game.h"
#include <algorithm>

namespace lk {

vec3 rotate3(vec3 v, float a);
static const float TILE = 4.f;

struct Theme { vec3 floor, floor2, wall, trim, accent; bool webs, crystals, bones, lava, ice; };
static Theme theme(int t) {
    switch (t) {
    case 0: return {hexColor(0x5a524a), hexColor(0x6a6058), hexColor(0x4a4440), hexColor(0x6a5a4a), hexColor(0x9aff6a), false, false, true, false, false};     // goblin cave
    case 1: return {hexColor(0x4a4850), hexColor(0x58565e), hexColor(0x3e3c44), hexColor(0x6a6670), hexColor(0x6ff0ff), true, false, true, false, false};    // catacombs
    case 2: return {hexColor(0x2e2a30), hexColor(0x3a3440), hexColor(0x241f28), hexColor(0x3a3040), hexColor(0xd060ff), true, true, true, false, false};     // spider den
    case 3: return {hexColor(0x3a2a24), hexColor(0x4a342a), hexColor(0x2a1e1a), hexColor(0x5a3a2a), hexColor(0xff7a26), false, false, false, true, false};   // ember mines
    case 4: return {hexColor(0x8aa0b8), hexColor(0x9ab4c8), hexColor(0x6a8aa8), hexColor(0xc0d8e8), hexColor(0x9ae6ff), false, true, true, false, true};    // ice caves
    case 6: return {hexColor(0xb89a64), hexColor(0xc8aa74), hexColor(0x8a6a3c), hexColor(0xe0b848), hexColor(0xffb040), false, false, true, false, false};   // sun temple: sandstone and gold
    case 8: return {hexColor(0x4f8a46), hexColor(0x5f9c50), hexColor(0x6a5a88), hexColor(0xa898e0), hexColor(0x5ab8e0), false, true, false, false, false};   // celestial realm: grass on floating rock, crystal
    case 7: return {hexColor(0x3a6a78), hexColor(0x4a7a88), hexColor(0x2a4a5a), hexColor(0x7ad0c8), hexColor(0x70ffe0), false, true, false, false, false};   // sea cave / sanctum: drowned stone, glowing coral
    default: return {hexColor(0x241a2a), hexColor(0x2e2236), hexColor(0x1a1220), hexColor(0x3a2a48), hexColor(0xb040ff), false, true, false, false, false}; // abyss
    }
}

bool Game::dungeonFloor(float x, float z, float& y) const {
    if (!dungeon.active || x < WORLD_SIZE) return false;
    int tx = (int)std::floor((x - dungeon.origin.x) / TILE), tz = (int)std::floor((z - dungeon.origin.z) / TILE);
    if (tx < 0 || tz < 0 || tx >= dungeon.W || tz >= dungeon.H) { y = dungeon.theme == 8 ? dungeon.origin.y - 30.f : dungeon.origin.y; return true; }
    TileType t = dungeon.tiles[tz * dungeon.W + tx];
    bool open = t == TileType::Pit || (dungeon.theme == 8 && t == TileType::Void);   // 2.2.1 the realm: no walls, the edge of an island is a drop into the sky
    y = open ? dungeon.origin.y - 30.f : dungeon.origin.y;
    return true;
}


// ============================================================================================ 3.1.0 dressing
// The dungeons were bare blocks: one flat slab of wall per tile, a few torches. Now every face of wall that looks onto
// a room or a corridor is dressed by theme (stone courses, a plinth and a cornice, pilasters; timber sets in the mines;
// rock and icicles in the caves), with sconces, banners, skull niches and cobwebs. Doorways get frames, the rooms get
// furniture along their walls (sarcophagi, statues, weapon racks, tables with candles, shelves, crates, ore carts,
// coral, crystals) and the boss hall becomes a throne room: dais, throne or altar, carpet, four great braziers.
namespace dpal { const vec3 gold = hexColor(0xc9a45c), timber = hexColor(0x4a3322), dark = hexColor(0x0d0b0a), ember = hexColor(0xff7a26); }
namespace {
struct Dresser {
    DungeonState& d; int theme; const Theme& th; MeshBuilder& wl; MeshBuilder& pp; Rng r;
    bool built() const { return theme == 1 || theme == 5 || theme == 6 || theme == 7; }
    bool cave() const { return theme == 0 || theme == 2 || theme == 4; }
    TileType T(int x, int z) const { return (x < 0 || z < 0 || x >= d.W || z >= d.H) ? TileType::Void : d.tiles[z * d.W + x]; }
    static bool walk(TileType t) { return t != TileType::Void && t != TileType::Pit; }
    int roomAt(int x, int z) const { for (size_t i = 0; i < d.rooms.size(); i++) { auto& m = d.rooms[i]; if (x >= m.x && x < m.x + m.w && z >= m.z && z < m.z + m.h) return (int)i; } return -1; }
    vec3 C(int x, int z) const { return d.origin + vec3((x + 0.5f) * TILE, 0, (z + 0.5f) * TILE); }
    vec3 banner() const { return theme == 1 ? hexColor(0x6e1c1c) : theme == 5 ? hexColor(0x3e1660) : theme == 6 ? hexColor(0x1c3f7a) : theme == 7 ? hexColor(0x165656) : theme == 3 ? hexColor(0x5a2a14) : hexColor(0x3a4a2a); }
    // a box on a wall face: n = outward normal of the face (into the room), t = along the face
    void fbox(MeshBuilder& M, vec3 fc, vec3 n, vec3 t, float on, float ot, float y, float hn, float ht, float hy, const Mat& m, float bevel = 0.f) {
        vec3 c = fc + n * on + t * ot + vec3(0, y, 0);
        vec3 h = vec3(std::fabs(n.x) * hn + std::fabs(t.x) * ht, hy, std::fabs(n.z) * hn + std::fabs(t.z) * ht);
        if (bevel > 0) M.bevelBox(c, h, bevel, m); else M.box(c, h, m);
    }
    void light(vec3 p, int kind) { d.lights.push_back(vec4(p, (float)kind)); }
    void sconce(vec3 fc, vec3 n, vec3 t, float ot) {
        Mat iron(hexColor(0x2a2624), 0.45f, 0.8f);
        fbox(wl, fc, n, t, 0.04f, ot, 2.5f, 0.04f, 0.12f, 0.2f, iron);
        wl.tube(fc + n * 0.06f + t * ot + vec3(0, 2.4f, 0), fc + n * 0.42f + t * ot + vec3(0, 2.75f, 0), 0.025f, 0.025f, iron, 5, false);
        wl.lathe({{0.01f, -0.18f}, {0.09f, 0.0f}, {0.11f, 0.08f}, {0.01f, 0.08f}}, iron, 7, fc + n * 0.45f + t * ot + vec3(0, 2.8f, 0));
        wl.ellipsoid(fc + n * 0.45f + t * ot + vec3(0, 3.0f, 0), vec3(0.1f, 0.2f, 0.1f), Mat(theme == 5 ? hexColor(0xb060ff) : (theme == 7 ? hexColor(0x60fff0) : hexColor(0xffa040)), 0.5f, 0, 4.f), 6, 4);
        vec3 lp = fc + n * 0.9f + t * ot + vec3(0, 3.1f, 0);
        light(lp, theme == 5 || theme == 7 ? LI_MAGIC : LI_FIRE);
        d.emitters.push_back(vec4(fc + n * 0.45f + t * ot + vec3(0, 3.05f, 0), theme == 5 || theme == 7 ? EM_SPARKLES : EM_FIRE));
    }
    void hangBanner(vec3 fc, vec3 n, vec3 t, float ot, float len) {
        Mat cloth(banner(), 0.95f), gold(dpal::gold, 0.4f, 0.8f);
        fbox(wl, fc, n, t, 0.12f, ot, 4.0f, 0.05f, 0.75f, 0.05f, Mat(dpal::timber, 0.8f));                  // rod
        fbox(wl, fc, n, t, 0.1f, ot, 4.0f - len * 0.5f, 0.02f, 0.62f, len * 0.5f, cloth);
        wl.triP(fc + n * 0.11f + t * (ot - 0.62f) + vec3(0, 4.f - len, 0), fc + n * 0.11f + t * (ot + 0.62f) + vec3(0, 4.f - len, 0), fc + n * 0.11f + t * ot + vec3(0, 3.6f - len, 0), cloth);
        wl.triP(fc + n * 0.09f + t * (ot + 0.62f) + vec3(0, 4.f - len, 0), fc + n * 0.09f + t * (ot - 0.62f) + vec3(0, 4.f - len, 0), fc + n * 0.09f + t * ot + vec3(0, 3.6f - len, 0), cloth);
        fbox(wl, fc, n, t, 0.125f, ot, 4.0f - len * 0.45f, 0.01f, 0.22f, 0.22f, gold);                        // emblem
        fbox(wl, fc, n, t, 0.125f, ot, 3.9f, 0.01f, 0.6f, 0.03f, gold);
    }
    // ---- one face of wall looking into a walkable tile
    void face(int x, int z, int dx, int dz, bool roomSide, bool boss) {
        vec3 n(dx, 0, dz), t(dz != 0 ? 1.f : 0.f, 0, dx != 0 ? 1.f : 0.f);
        vec3 fc = C(x, z) + n * (TILE * 0.5f);
        Mat stone(lerp(th.wall, th.trim, 0.25f), theme == 4 ? 0.35f : 0.88f), trim(th.trim, 0.8f);
        if (built()) {
            // plinth, three courses of dressed stone (staggered), a cornice
            fbox(wl, fc, n, t, 0.12f, 0, 0.25f, 0.12f, TILE * 0.5f, 0.25f, trim);
            for (int row = 0; row < 3; row++) {
                float y = 0.85f + row * 1.05f, off = (row % 2) * 0.66f;
                for (int b = -2; b <= 2; b++) {
                    float a = b * 1.33f + off - 0.66f, e = a + 1.26f;
                    a = std::max(a, -TILE * 0.5f + 0.02f); e = std::min(e, TILE * 0.5f - 0.02f);
                    if (e - a < 0.2f) continue;
                    fbox(wl, fc, n, t, 0.035f, (a + e) * 0.5f, y, 0.035f, (e - a) * 0.5f - 0.03f, 0.48f, stone.withColor(stone.color * r.range(0.86f, 1.1f)));
                }
            }
            fbox(wl, fc, n, t, 0.16f, 0, 4.25f, 0.16f, TILE * 0.5f, 0.12f, trim);
            if (((x + z) & 1) == 0) {   // pilaster
                fbox(wl, fc, n, t, 0.2f, TILE * 0.5f - 0.02f, 2.2f, 0.2f, 0.32f, 2.05f, trim, 0.05f);
                fbox(wl, fc, n, t, 0.26f, TILE * 0.5f - 0.02f, 4.0f, 0.26f, 0.4f, 0.14f, trim, 0.04f);
            }
        } else if (theme == 3) {
            // mine gallery: timber sets against raw rock, a plank lagging between them
            Mat wood(hexColor(0x4a3322), 0.85f);
            if (((x + z) & 1) == 0) {
                for (int s = -1; s <= 1; s += 2) wl.tube(fc + n * 0.2f + t * (s * 1.6f) + vec3(0, -0.1f, 0), fc + n * 0.2f + t * (s * 1.5f) + vec3(0, 3.8f, 0), 0.14f, 0.12f, wood, 6, false);
                fbox(wl, fc, n, t, 0.22f, 0, 3.85f, 0.16f, 1.9f, 0.14f, wood, 0.03f);
            }
            for (int i = 0; i < 4; i++) fbox(wl, fc, n, t, 0.06f, 0, 0.5f + i * 0.85f, 0.04f, 1.45f, 0.36f, wood.withColor(hexColor(0x5a4030) * r.range(0.8f, 1.1f)));
        } else {
            // natural cave wall: lumps of rock, ice gets icicles hanging from the brow
            for (int i = 0; i < 3; i++) wl.rock(fc + n * r.range(0.1f, 0.4f) + t * r.range(-1.6f, 1.6f) + vec3(0, r.range(0.3f, 3.8f), 0), vec3(r.range(0.5f, 0.9f), r.range(0.5f, 1.1f), r.range(0.4f, 0.7f)), stone, r.next(), 0);
            if (theme == 4) for (int i = 0; i < 4; i++) { vec3 b = fc + n * r.range(0.2f, 0.6f) + t * r.range(-1.8f, 1.8f) + vec3(0, 4.4f, 0); wl.cone(b, b - vec3(0, r.range(0.6f, 1.6f), 0), r.range(0.08f, 0.18f), Mat(hexColor(0xc8ecff), 0.15f, 0, 0.4f), 5); }
            if (theme == 2 && r.chance(0.35f)) {   // cobweb across the corner
                Mat web(hexColor(0xd8d8e0), 0.9f, 0, 0.15f);
                vec3 a = fc + n * 0.05f + t * (TILE * 0.5f) + vec3(0, 4.2f, 0);
                for (int k = 0; k < 3; k++) wl.triP(a, a - t * (1.2f + k * 0.4f) + n * 0.02f * k, a + n * (1.0f + k * 0.3f) - vec3(0, 0.8f + k * 0.5f, 0), web);
            }
            if (theme == 0 && r.chance(0.08f)) { light(fc + n * 0.6f + vec3(0, 1.f, 0), LI_MAGIC); wl.rock(fc + n * 0.3f + vec3(0, 0.4f, 0), vec3(0.3f), Mat(th.accent, 0.3f, 0, 2.f), r.next(), 0); }   // glowing moss stone
        }
        // furniture of the wall
        float roll = r.f();
        if (roll < (boss ? 0.3f : 0.17f)) sconce(fc, n, t, r.range(-0.8f, 0.8f));
        else if (roomSide && (built() || theme == 3) && roll < (boss ? 0.62f : 0.27f)) hangBanner(fc, n, t, 0, r.range(1.6f, 2.4f));
        else if (theme == 1 && roll < 0.45f) {   // burial niche with a skull
            fbox(wl, fc, n, t, 0.015f, 0, 1.6f, 0.02f, 0.65f, 0.35f, Mat(dpal::dark, 1.f));
            wl.ellipsoid(fc + n * 0.12f + vec3(0, 1.45f, 0), vec3(0.13f, 0.12f, 0.14f), Mat(hexColor(0xd8d0b8), 0.7f), 7, 5);
            wl.ellipsoid(fc + n * 0.22f + t * 0.04f + vec3(0, 1.45f, 0), vec3(0.025f), Mat(dpal::dark), 4, 3);
            wl.ellipsoid(fc + n * 0.22f - t * 0.04f + vec3(0, 1.45f, 0), vec3(0.025f), Mat(dpal::dark), 4, 3);
        } else if ((theme == 5 || theme == 1) && roll < 0.55f) {   // chains
            for (int s = -1; s <= 1; s += 2) for (int k = 0; k < 6; k++) wl.ellipsoid(fc + n * 0.08f + t * (s * 0.5f) + vec3(0, 3.6f - k * 0.18f, 0), vec3(0.03f, 0.08f, 0.03f), Mat(hexColor(0x3a3634), 0.4f, 0.9f), 5, 3);
        }
    }
    // ---- door frames where a corridor meets a room
    void doorway(int x, int z, int dx, int dz) {
        vec3 n(dx, 0, dz), t(dz != 0 ? 1.f : 0.f, 0, dx != 0 ? 1.f : 0.f);
        vec3 b = C(x, z) + n * (TILE * 0.5f);   // the boundary between the corridor tile and the room tile
        if (theme == 3) {
            Mat wood(hexColor(0x4a3322), 0.85f);
            for (int s = -1; s <= 1; s += 2) wl.tube(b + t * (s * 1.75f), b + t * (s * 1.7f) + vec3(0, 3.6f, 0), 0.17f, 0.15f, wood, 6, true);
            wl.bevelBox(b + vec3(0, 3.7f, 0), t * 2.2f + vec3(0, 0.17f, 0) + n * 0.2f, 0.03f, wood);
            return;
        }
        if (!built()) return;
        Mat trim(th.trim, 0.75f), dk(th.trim * 0.7f, 0.8f);
        for (int s = -1; s <= 1; s += 2) {
            wl.bevelBox(b + t * (s * 1.78f) + vec3(0, 1.9f, 0), t * 0.3f + n * 0.38f + vec3(0, 1.9f, 0), 0.05f, trim);
            wl.bevelBox(b + t * (s * 1.78f) + vec3(0, 0.15f, 0), t * 0.38f + n * 0.46f + vec3(0, 0.15f, 0), 0.04f, dk);
        }
        // a flattened arch: lintel, two haunches and a keystone in the accent colour
        wl.bevelBox(b + vec3(0, 4.0f, 0), t * 2.1f + n * 0.42f + vec3(0, 0.22f, 0), 0.05f, trim);
        for (int s = -1; s <= 1; s += 2) wl.triP(b + t * (s * 1.5f) + vec3(0, 3.78f, 0) + n * 0.4f, b + t * (s * 0.6f) + vec3(0, 3.78f, 0) + n * 0.4f, b + t * (s * 1.5f) + vec3(0, 3.2f, 0) + n * 0.4f, trim);
        wl.bevelBox(b + vec3(0, 4.05f, 0) + n * 0.05f, t * 0.28f + n * 0.46f + vec3(0, 0.32f, 0), 0.04f, Mat(theme == 6 ? dpal::gold : th.accent * 0.7f, 0.4f, theme == 6 ? 0.8f : 0.f, theme == 6 ? 0.f : 1.2f));
    }
    // ---- furniture in a room tile against a wall (n points from the wall into the room)
    void furnish(vec3 at, vec3 n, int roomKind) {
        vec3 t(std::fabs(n.z), 0, std::fabs(n.x));
        vec3 p = at - n * 1.05f;   // close to the wall
        float pick = r.f();
        Mat stone(th.trim, 0.8f), wood(hexColor(0x5a3e28), 0.85f), dark(hexColor(0x3a2a1c), 0.85f), iron(hexColor(0x4a4c52), 0.4f, 0.85f);
        auto collider = [&](vec3 c, float rad) { Collider col; col.c = c.xz(); col.r = rad; col.base = -1; col.top = 2.f; d.colliders.push_back(col); };
        auto candles = [&](vec3 base, int k) { for (int i = 0; i < k; i++) { vec3 q = base + t * ((i - (k - 1) * 0.5f) * 0.16f) + n * r.range(-0.08f, 0.08f); pp.tube(q, q + vec3(0, r.range(0.12f, 0.26f), 0), 0.03f, 0.03f, Mat(hexColor(0xe8dcc0), 0.6f), 5, true); pp.ellipsoid(q + vec3(0, 0.3f, 0), vec3(0.025f, 0.05f, 0.025f), Mat(hexColor(0xffc060), 0.5f, 0, 5.f), 4, 3); } light(base + vec3(0, 0.9f, 0) + n * 0.4f, LI_FIRE); };
        if (theme == 1 || theme == 5) {
            if (pick < 0.35f) {   // sarcophagus with an effigy, candles at its head
                vec3 c = p + n * 0.25f;
                pp.bevelBox(c + vec3(0, 0.38f, 0), t * 1.05f + n * 0.5f + vec3(0, 0.38f, 0), 0.06f, stone);
                pp.bevelBox(c + vec3(0, 0.82f, 0), t * 1.12f + n * 0.56f + vec3(0, 0.08f, 0), 0.04f, stone.withColor(th.trim * 1.12f));
                pp.ellipsoid(c + vec3(0, 0.98f, 0), t * 0.75f + n * 0.2f + vec3(0, 0.1f, 0), stone.withColor(th.trim * 1.2f), 8, 5);
                pp.ellipsoid(c + t * 0.82f + vec3(0, 1.02f, 0), vec3(0.14f), stone.withColor(th.trim * 1.2f), 6, 4);
                candles(c - t * 1.45f + vec3(0, 0, 0), 3);
                collider(c, 1.0f);
            } else if (pick < 0.6f) {   // robed statue on a plinth holding a sword
                pp.bevelBox(p + vec3(0, 0.3f, 0), vec3(0.55f, 0.3f, 0.55f), 0.05f, stone);
                pp.lathe({{0.42f, 0.6f}, {0.36f, 1.4f}, {0.24f, 2.2f}, {0.2f, 2.35f}, {0.01f, 2.4f}}, stone.withColor(th.trim * 1.1f), 10, p);
                pp.ellipsoid(p + vec3(0, 2.6f, 0), vec3(0.17f, 0.21f, 0.17f), stone.withColor(th.trim * 1.1f), 8, 6);
                pp.tube(p + n * 0.3f + vec3(0, 0.75f, 0), p + n * 0.32f + vec3(0, 2.1f, 0), 0.04f, 0.03f, stone.withColor(th.trim * 0.95f), 5, true);
                pp.box(p + n * 0.32f + vec3(0, 1.9f, 0), t * 0.25f + n * 0.04f + vec3(0, 0.04f, 0), stone.withColor(th.trim * 0.95f));
                collider(p, 0.6f);
            } else if (pick < 0.8f) {   // shelves of urns and books
                pp.bevelBox(p + vec3(0, 1.2f, 0), t * 0.95f + n * 0.25f + vec3(0, 1.2f, 0), 0.03f, dark);
                for (int sh = 0; sh < 3; sh++) for (int i = 0; i < 5; i++) {
                    vec3 q = p + t * (-0.7f + i * 0.35f) + n * 0.12f + vec3(0, 0.35f + sh * 0.75f, 0);
                    if (r.chance(0.5f)) pp.lathe({{0.08f, 0}, {0.12f, 0.15f}, {0.06f, 0.32f}, {0.01f, 0.32f}}, Mat(hexColor(r.chance(0.5f) ? 0x8a5a3a : 0x6a6a7a), 0.6f), 6, q);
                    else pp.box(q + vec3(0, 0.17f, 0), t * 0.13f + n * 0.12f + vec3(0, 0.17f, 0), Mat(hexColor(r.chance(0.5f) ? 0x5a1e1e : 0x2a3a5a), 0.8f));
                }
                collider(p, 0.7f);
            } else { const PropTemplate& tp = propTemplate(P_BONES, 0); MeshBuilder tmp; tmp.append(tp.solid); for (auto& v : tmp.v) { v.px += p.x; v.py += p.y; v.pz += p.z; } pp.append(tmp); candles(p + t * 0.6f, 2); }
        } else if (theme == 6 || theme == 7) {
            if (pick < 0.45f) {   // great urn / coral bloom on a plinth
                pp.bevelBox(p + vec3(0, 0.2f, 0), vec3(0.5f, 0.2f, 0.5f), 0.04f, stone);
                if (theme == 6) pp.lathe({{0.2f, 0.4f}, {0.42f, 0.8f}, {0.38f, 1.3f}, {0.18f, 1.55f}, {0.24f, 1.7f}, {0.01f, 1.7f}}, Mat(hexColor(0xb8763a), 0.5f, 0.3f), 10, p);
                else for (int i = 0; i < 6; i++) { vec3 q = p + vec3(r.range(-0.3f, 0.3f), 0.4f, r.range(-0.3f, 0.3f)); pp.cone(q, q + vec3(r.range(-0.3f, 0.3f), r.range(0.6f, 1.3f), r.range(-0.3f, 0.3f)), r.range(0.06f, 0.12f), Mat(th.accent * 0.8f, 0.3f, 0, 1.5f), 5); }
                if (theme == 7) light(p + vec3(0, 1.2f, 0), LI_MAGIC);
                collider(p, 0.55f);
            } else if (pick < 0.75f) {   // guardian statue
                pp.bevelBox(p + vec3(0, 0.3f, 0), vec3(0.6f, 0.3f, 0.6f), 0.05f, stone);
                pp.lathe({{0.35f, 0.6f}, {0.4f, 1.6f}, {0.3f, 2.3f}, {0.01f, 2.35f}}, stone.withColor(th.trim * 1.15f), 8, p);
                pp.ellipsoid(p + vec3(0, 2.55f, 0) + n * 0.05f, vec3(0.2f, 0.24f, 0.24f), stone.withColor(th.trim * 1.15f), 8, 6);
                for (int s = -1; s <= 1; s += 2) pp.cone(p + t * (s * 0.1f) + vec3(0, 2.7f, 0), p + t * (s * 0.18f) + vec3(0, 3.0f, 0), 0.07f, stone.withColor(th.trim * 1.15f), 4);   // ears / crown
                pp.tube(p + t * 0.45f + vec3(0, 0.6f, 0), p + t * 0.45f + vec3(0, 3.1f, 0), 0.04f, 0.04f, Mat(dpal::gold, 0.4f, 0.8f), 6, true);
                collider(p, 0.65f);
            } else { pp.box(p + vec3(0, 0.75f, 0), t * 0.9f + n * 0.4f + vec3(0, 0.05f, 0), wood); for (int k = 0; k < 4; k++) pp.box(p + t * ((k % 2 ? 0.8f : -0.8f)) + n * ((k / 2 ? 0.3f : -0.3f)) + vec3(0, 0.36f, 0), vec3(0.05f, 0.36f, 0.05f), wood); candles(p + vec3(0, 0.8f, 0), 3); collider(p, 0.8f); }
        } else if (theme == 3) {
            if (pick < 0.4f) {   // ore cart on a stub of rails
                for (int s = -1; s <= 1; s += 2) pp.box(p + t * (s * 0.4f) + vec3(0, 0.05f, 0), t * 0.03f + n * 1.4f + vec3(0, 0.04f, 0), iron);
                pp.bevelBox(p + vec3(0, 0.6f, 0), t * 0.55f + n * 0.75f + vec3(0, 0.35f, 0), 0.05f, Mat(hexColor(0x4a3c30), 0.6f, 0.4f));
                for (int i = 0; i < 5; i++) pp.rock(p + vec3(r.range(-0.3f, 0.3f), 1.0f, r.range(-0.3f, 0.3f)), vec3(r.range(0.15f, 0.25f)), Mat(i % 2 ? hexColor(0xff7a26) : hexColor(0x3a2a24), 0.6f, 0, i % 2 ? 2.f : 0.f), r.next(), 0);
                collider(p, 0.8f);
            } else { for (int i = 0; i < 3; i++) pp.bevelBox(p + t * (i - 1) * 0.7f + vec3(0, 0.33f + (i == 1 ? 0.66f : 0.f), 0), vec3(0.33f), 0.04f, wood.withColor(hexColor(0x6a4a2e) * r.range(0.85f, 1.1f))); pp.lathe({{0.3f, 0}, {0.36f, 0.45f}, {0.3f, 0.9f}, {0.01f, 0.9f}}, wood, 10, p + n * 0.6f + t * 1.3f); collider(p, 0.9f); }
        } else if (theme == 4) {
            const PropTemplate& tp = propTemplate(P_ICECRYSTAL, r.irange(0, 2)); MeshBuilder tmp; tmp.append(tp.solid);
            for (auto& v : tmp.v) { v.px = v.px * 1.3f + p.x; v.py = v.py * 1.3f + p.y; v.pz = v.pz * 1.3f + p.z; } pp.append(tmp); light(p + vec3(0, 1.f, 0), LI_CRYSTAL); collider(p, 0.6f);
        } else {
            if (pick < 0.5f) { for (int i = 0; i < 2; i++) pp.bevelBox(p + t * (i ? 0.45f : -0.4f) + vec3(0, 0.33f, 0), vec3(0.33f), 0.04f, wood); pp.lathe({{0.3f, 0}, {0.36f, 0.45f}, {0.3f, 0.9f}, {0.01f, 0.9f}}, wood, 10, p + t * 1.2f); collider(p, 0.8f); }
            else if (theme == 2) { for (int i = 0; i < 4; i++) pp.ellipsoid(p + vec3(r.range(-0.5f, 0.5f), 0.25f, r.range(-0.5f, 0.5f)), vec3(0.22f, 0.28f, 0.22f), Mat(hexColor(0xd8d0c0), 0.5f, 0, 0.3f), 7, 5); }   // egg sacs
            else { // goblin weapon rack and a cooking pot
                for (int s = -1; s <= 1; s += 2) pp.tube(p + t * (s * 0.7f), p + t * (s * 0.7f) + vec3(0, 1.5f, 0), 0.05f, 0.05f, wood, 5, true);
                pp.tube(p - t * 0.75f + vec3(0, 1.2f, 0), p + t * 0.75f + vec3(0, 1.2f, 0), 0.04f, 0.04f, wood, 5, true);
                for (int k = 0; k < 3; k++) pp.tube(p + t * (-0.4f + k * 0.4f) + n * 0.1f, p + t * (-0.4f + k * 0.4f) + vec3(0, 1.8f, 0), 0.025f, 0.02f, wood, 4, true);
                pp.lathe({{0.01f, 0}, {0.35f, 0.1f}, {0.4f, 0.45f}, {0.32f, 0.5f}}, iron, 10, p + n * 1.2f);
                collider(p, 0.7f);
            }
        }
        (void)roomKind;
    }
    // ---- the boss hall: dais, throne or altar, carpet, four great braziers
    void throneRoom() {
        const DungeonRoom& rm = d.rooms.back();
        vec3 ctr = d.origin + vec3((rm.x + rm.w * 0.5f) * TILE, 0, (rm.z + rm.h * 0.5f) * TILE);
        vec3 toGate = d.gatePos - ctr; toGate.y = 0;
        vec3 n = std::fabs(toGate.x) > std::fabs(toGate.z) ? vec3(toGate.x > 0 ? 1.f : -1.f, 0, 0) : vec3(0, 0, toGate.z > 0 ? 1.f : -1.f);   // towards the way in
        vec3 t(std::fabs(n.z), 0, std::fabs(n.x));
        float half = (std::fabs(n.x) > 0 ? rm.w : rm.h) * TILE * 0.5f, side = (std::fabs(n.x) > 0 ? rm.h : rm.w) * TILE * 0.5f;
        vec3 back = ctr - n * (half - 2.2f);
        Mat stone(lerp(th.trim, th.floor2, 0.4f) * 1.1f, 0.6f), dk(lerp(th.floor, th.trim, 0.5f), 0.7f), gold(dpal::gold, 0.35f, 0.85f), cloth(banner(), 0.95f);
        // two-step dais
        pp.bevelBox(back + vec3(0, 0.05f, 0), t * 4.2f + n * 2.0f + vec3(0, 0.05f, 0), 0.05f, dk);
        pp.bevelBox(back - n * 0.4f + vec3(0, 0.13f, 0), t * 3.2f + n * 1.5f + vec3(0, 0.04f, 0), 0.04f, stone);
        // carpet from the way in to the dais, with a gold edge
        float len = half * 2.f - 4.4f;
        vec3 cm = ctr + n * (half - len * 0.5f - 0.2f) - n * 2.2f;
        pp.box(cm + vec3(0, 0.015f, 0), t * 1.25f + n * (len * 0.5f) + vec3(0, 0.012f, 0), cloth);
        for (int s = -1; s <= 1; s += 2) pp.box(cm + t * (s * 1.2f) + vec3(0, 0.03f, 0), t * 0.07f + n * (len * 0.5f) + vec3(0, 0.01f, 0), gold);
        vec3 seat = back - n * 0.6f + vec3(0, 0.17f, 0);
        if (built() || theme == 3) {
            // throne: seat, tall back with a crest, armrests, gems
            pp.bevelBox(seat + vec3(0, 0.45f, 0), t * 0.85f + n * 0.7f + vec3(0, 0.45f, 0), 0.06f, stone);
            pp.bevelBox(seat - n * 0.6f + vec3(0, 1.9f, 0), t * 0.85f + n * 0.14f + vec3(0, 1.9f, 0), 0.06f, stone);
            pp.cone(seat - n * 0.6f + vec3(0, 3.8f, 0), seat - n * 0.6f + vec3(0, 4.7f, 0), 0.55f, gold, 4);
            for (int s = -1; s <= 1; s += 2) {
                pp.bevelBox(seat + t * (s * 0.9f) + vec3(0, 1.05f, 0), t * 0.14f + n * 0.7f + vec3(0, 0.12f, 0), 0.04f, gold);
                pp.lathe({{0.15f, 0}, {0.12f, 1.0f}, {0.01f, 1.05f}}, stone, 8, seat + t * (s * 0.9f) + n * 0.6f);
            }
            pp.box(seat - n * 0.45f + vec3(0, 1.0f, 0), t * 0.6f + n * 0.02f + vec3(0, 0.08f, 0), cloth);   // cushion back
            pp.ellipsoid(seat - n * 0.45f + vec3(0, 3.2f, 0), vec3(0.18f), Mat(th.accent, 0.2f, 0, 3.f), 8, 6);
            light(seat + vec3(0, 3.3f, 0) + n * 0.5f, LI_MAGIC);
        } else {
            // altar of rough stone with a glowing relic
            pp.rock(seat + vec3(0, 0.5f, 0), t * 1.3f + n * 0.8f + vec3(0, 0.9f, 0), stone, r.next(), 1);
            pp.ellipsoid(seat + vec3(0, 1.45f, 0), vec3(0.25f), Mat(th.accent, 0.2f, 0, 3.f), 8, 6);
            light(seat + vec3(0, 2.f, 0), LI_MAGIC); d.emitters.push_back(vec4(seat + vec3(0, 1.5f, 0), EM_SPARKLES));
        }
        { Collider col; col.c = seat.xz(); col.r = 1.1f; col.base = -1; col.top = 4.f; d.colliders.push_back(col); }
        // four great braziers on pedestals
        for (int k = 0; k < 4; k++) {
            vec3 q = ctr + t * ((k % 2 ? 1.f : -1.f) * (side - 2.6f)) + n * ((k / 2 ? 1.f : -1.f) * (half - 5.0f));
            pp.lathe({{0.45f, 0}, {0.3f, 0.2f}, {0.2f, 0.9f}, {0.35f, 1.05f}, {0.7f, 1.35f}, {0.6f, 1.4f}, {0.01f, 1.25f}}, Mat(hexColor(0x3a3430), 0.4f, 0.8f), 10, q);
            pp.ellipsoid(q + vec3(0, 1.38f, 0), vec3(0.5f, 0.08f, 0.5f), Mat(theme == 5 || theme == 7 ? th.accent : dpal::ember, 0.5f, 0, 3.f), 8, 3);
            light(q + vec3(0, 2.2f, 0), theme == 5 || theme == 7 ? LI_MAGIC : LI_FIRE);
            d.emitters.push_back(vec4(q + vec3(0, 1.45f, 0), theme == 5 || theme == 7 ? EM_SPARKLES : EM_FIRE));
            Collider col; col.c = q.xz(); col.r = 0.6f; col.base = -1; col.top = 2.f; d.colliders.push_back(col);
        }
    }
};
}

static void dressDungeon(DungeonState& d, int theme, const Theme& th, MeshBuilder& wl, MeshBuilder& pp, uint32_t seed) {
    Dresser D{d, theme, th, wl, pp, Rng(seed * 7 + 3)};
    const int bossRoom = (int)d.rooms.size() - 1;
    // wall faces and door frames
    for (int z = 0; z < d.H; z++) for (int x = 0; x < d.W; x++) {
        TileType t = D.T(x, z);
        for (int k = 0; k < 4; k++) {
            int dx = k == 0 ? 1 : k == 1 ? -1 : 0, dz = k == 2 ? 1 : k == 3 ? -1 : 0;
            TileType nb = D.T(x + dx, z + dz);
            if (!Dresser::walk(t) && t != TileType::Pit && Dresser::walk(nb)) {
                int ri = D.roomAt(x + dx, z + dz);
                D.face(x, z, dx, dz, ri >= 0, ri == bossRoom);
            } else if (Dresser::walk(t) && Dresser::walk(nb) && D.roomAt(x, z) < 0 && D.roomAt(x + dx, z + dz) >= 0) {
                D.doorway(x, z, dx, dz);
            }
        }
    }
    // furniture along the walls of the ordinary rooms (not the entrance hall, the puzzle room or the boss hall)
    for (size_t i = 1; i + 1 < d.rooms.size(); i++) {
        const DungeonRoom& rm = d.rooms[i];
        if (rm.kind == 3) continue;
        for (int z = rm.z; z < rm.z + rm.h; z++) for (int x = rm.x; x < rm.x + rm.w; x++) {
            bool jet = false;
            for (const vec3& f : d.fireJets) if (distXZ(f, D.C(x, z)) < 2.5f) jet = true;
            if (jet) continue;
            for (int k = 0; k < 4; k++) {
                int dx = k == 0 ? 1 : k == 1 ? -1 : 0, dz = k == 2 ? 1 : k == 3 ? -1 : 0;
                if (D.T(x + dx, z + dz) != TileType::Void) continue;   // only against a solid wall (never in a doorway)
                bool corner = D.T(x - dx, z - dz) == TileType::Void;   // narrow room: skip
                if (corner || !D.r.chance(0.34f)) continue;
                D.furnish(D.C(x, z), vec3(-dx, 0, -dz), rm.kind);
                break;
            }
        }
    }
    if (bossRoom >= 1) D.throneRoom();
}

void Game::buildDungeon(int idx) {
    const DungeonDef& D = dungeons()[idx];
    DungeonState& d = dungeon;
    bool rebuildOnly = d.def == idx && !d.tiles.empty();
    if (!rebuildOnly) {
        d = DungeonState();
        d.def = idx; d.theme = D.theme;
        d.W = 26; d.H = 26;
        d.origin = vec3(WORLD_SIZE + 400.f + idx * 160.f, 0.f, 400.f);
        d.tiles.assign(d.W * d.H, TileType::Void);
        Rng r(D.seed);
        // ---- rooms placed along a winding path so the dungeon reads as a journey
        int cx = 2, cz = 2;
        for (int i = 0; i < D.rooms; i++) {
            bool boss = i == D.rooms - 1;
            int w = boss ? 7 : r.irange(3, 5), h = boss ? 7 : r.irange(3, 5);
            int x = std::min(d.W - w - 1, std::max(1, cx)), z = std::min(d.H - h - 1, std::max(1, cz));
            DungeonRoom room{x, z, w, h, boss ? 2 : (i == 0 ? 0 : (i == D.rooms - 2 ? 3 : 1))};
            d.rooms.push_back(room);
            for (int zz = z; zz < z + h; zz++) for (int xx = x; xx < x + w; xx++) d.tiles[zz * d.W + xx] = TileType::Floor;
            // next room position: snake through the grid
            int dir = r.irange(0, 3);
            int nx = cx, nz = cz;
            for (int tries = 0; tries < 12; tries++) {
                int dd = (dir + tries) % 4;
                nx = cx + (dd == 0 ? 7 : dd == 1 ? -7 : r.irange(-2, 2));
                nz = cz + (dd == 2 ? 7 : dd == 3 ? -7 : r.irange(-2, 2));
                if (nx >= 1 && nz >= 1 && nx < d.W - 8 && nz < d.H - 8) {
                    bool overlap = false;
                    for (auto& o : d.rooms) if (nx < o.x + o.w + 1 && nx + 6 > o.x - 1 && nz < o.z + o.h + 1 && nz + 6 > o.z - 1) overlap = true;
                    if (!overlap) break;
                }
            }
            cx = nx; cz = nz;
        }
        // ---- corridors (L-shaped, 1 tile wide) between consecutive rooms
        for (size_t i = 0; i + 1 < d.rooms.size(); i++) {
            auto& a = d.rooms[i]; auto& b = d.rooms[i + 1];
            int ax = a.x + a.w / 2, az = a.z + a.h / 2, bx = b.x + b.w / 2, bz = b.z + b.h / 2;
            int x = ax, z = az;
            while (x != bx) { if (d.tiles[z * d.W + x] == TileType::Void) d.tiles[z * d.W + x] = TileType::Floor; x += x < bx ? 1 : -1; }
            while (z != bz) { if (d.tiles[z * d.W + x] == TileType::Void) d.tiles[z * d.W + x] = TileType::Floor; z += z < bz ? 1 : -1; }
            // gate before the final room (opened by the puzzle)
            if (i + 2 == d.rooms.size()) {
                d.gateRoom = (int)i + 1;
                // find the corridor tile just outside the boss room
                int gx = bx, gz = bz;
                int px = ax, pz = az;
                (void)px; (void)pz;
                // walk back from the boss room centre until leaving the room
                x = bx; z = bz;
                int sx = (ax > bx) ? 1 : (ax < bx ? -1 : 0);
                if (z != az) { int szz = az > bz ? 1 : -1; while (z >= b.z && z < b.z + b.h) z += szz; gx = x; gz = z; d.gateRot = 0; }
                else { while (x >= b.x && x < b.x + b.w) x += sx; gx = x; gz = z; d.gateRot = PI / 2; }
                d.gatePos = d.origin + vec3((gx + 0.5f) * TILE, 0, (gz + 0.5f) * TILE);
            }
        }
        // ---- traps along corridors, puzzle braziers in the penultimate room
        for (int z = 0; z < d.H; z++)
            for (int x = 0; x < d.W; x++) {
                if (d.tiles[z * d.W + x] != TileType::Floor) continue;
                bool inRoom = false;
                for (auto& rm : d.rooms) if (x >= rm.x && x < rm.x + rm.w && z >= rm.z && z < rm.z + rm.h) inRoom = true;
                if (!inRoom && r.chance(0.18f)) { d.tiles[z * d.W + x] = TileType::Spikes; d.traps.push_back({d.origin + vec3((x + 0.5f) * TILE, 0, (z + 0.5f) * TILE), r.range(0, 3)}); }
            }
        for (size_t i = 1; i + 2 < d.rooms.size(); i++) {
            auto& rm = d.rooms[i];
            if (r.chance(0.35f)) d.blades.push_back(d.origin + vec3((rm.x + rm.w * 0.5f) * TILE, 0, (rm.z + rm.h * 0.5f) * TILE));
            if (D.theme == 3 || D.theme == 5 || r.chance(0.25f)) d.fireJets.push_back(d.origin + vec3((rm.x + 1.f) * TILE, 0, (rm.z + rm.h - 1.f) * TILE));
        }
        auto& pr = d.rooms[d.rooms.size() - 2];
        int nb = 4;
        for (int i = 0; i < nb; i++) {
            float a = i * TAU / nb + 0.4f;
            vec3 c = d.origin + vec3((pr.x + pr.w * 0.5f) * TILE, 0, (pr.z + pr.h * 0.5f) * TILE);
            d.braziers.push_back(c + vec3(std::cos(a), 0, std::sin(a)) * (std::min(pr.w, pr.h) * TILE * 0.32f));
            d.brazierLit.push_back(false);
            d.puzzleOrder.push_back(i);
        }
        for (int i = nb - 1; i > 0; i--) std::swap(d.puzzleOrder[i], d.puzzleOrder[r.irange(0, i)]);
        auto& r0 = d.rooms[0];
        d.entrance = d.origin + vec3((r0.x + r0.w * 0.5f) * TILE, 0, (r0.z + r0.h * 0.5f) * TILE);
        auto& rb = d.rooms.back();
        d.bossPos = d.origin + vec3((rb.x + rb.w * 0.5f) * TILE, 0, (rb.z + rb.h * 0.5f) * TILE);
        d.exitPos = d.entrance + vec3(-TILE * 0.6f, 0, -TILE * 0.6f);
    }
    // ---- meshes & colliders
    Theme th = theme(D.theme);
    MeshBuilder fl, wl, pp;
    d.colliders.clear(); d.lights.clear(); d.emitters.clear();
    Rng r(D.seed + 99);
    auto T = [&](int x, int z) { return (x < 0 || z < 0 || x >= d.W || z >= d.H) ? TileType::Void : d.tiles[z * d.W + x]; };
    fl.jitter = 0.1f; wl.jitter = 0.08f;
    for (int z = 0; z < d.H; z++)
        for (int x = 0; x < d.W; x++) {
            TileType t = T(x, z);
            vec3 c = d.origin + vec3((x + 0.5f) * TILE, 0, (z + 0.5f) * TILE);
            if (D.theme == 8) {
                // 2.2.1 floating islands: rooms are grassy rock with a hanging root of stone; corridors are bridges of light
                if (t == TileType::Void || t == TileType::Pit) continue;
                bool inRoom = false;
                for (auto& rm : d.rooms) if (x >= rm.x && x < rm.x + rm.w && z >= rm.z && z < rm.z + rm.h) inRoom = true;
                if (inRoom) {
                    // 2.2.2 the island itself is built below as one organic, terrain-shaded mesh (see buildRealm)
                } else {
                    // bridge: translucent-looking glowing planks between two rails of light
                    for (int k = 0; k < 4; k++) fl.box(c + vec3(0, -0.06f, (k - 1.5f) * TILE * 0.25f), {TILE * 0.32f, 0.05f, TILE * 0.11f}, Mat(hexColor(0x8ae8ff) * 0.6f, 0.2f, 0, 1.2f));
                    fl.box(c + vec3(TILE * 0.36f, 0.35f, 0), {0.04f, 0.04f, TILE * 0.5f}, Mat(hexColor(0xc8b8ff), 0.2f, 0, 3.f));
                    fl.box(c + vec3(-TILE * 0.36f, 0.35f, 0), {0.04f, 0.04f, TILE * 0.5f}, Mat(hexColor(0xc8b8ff), 0.2f, 0, 3.f));
                    if ((x + z) % 3 == 0) d.lights.push_back(vec4(c + vec3(0, 1.f, 0), LI_CRYSTAL));
                }
                continue;
            }
            if (t != TileType::Void && t != TileType::Pit) {
                // 4 slabs per tile, slightly bevelled for a hand-laid look
                for (int s = 0; s < 4; s++) {
                    vec3 o{(s % 2 - 0.5f) * TILE * 0.5f, 0, (s / 2 - 0.5f) * TILE * 0.5f};
                    vec3 col = lerp(th.floor, th.floor2, r.f());
                    fl.bevelBox(c + o + vec3(0, -0.12f + r.range(-0.03f, 0.03f), 0), {TILE * 0.24f, 0.12f, TILE * 0.24f}, 0.05f, Mat(col, D.theme == 4 ? 0.3f : 0.85f));
                }
                if (t == TileType::Floor && r.chance(0.12f)) {
                    // debris by theme
                    vec3 p = c + vec3(r.range(-1.4f, 1.4f), 0, r.range(-1.4f, 1.4f));
                    if (th.bones && r.chance(0.4f)) { const PropTemplate& tp = propTemplate(P_BONES, 0); MeshBuilder tmp; tmp.append(tp.solid); for (auto& v : tmp.v) { v.px += p.x; v.py += p.y; v.pz += p.z; } pp.append(tmp); }
                    else if (th.crystals && r.chance(0.5f)) { const PropTemplate& tp = propTemplate(D.theme == 4 ? P_ICECRYSTAL : P_MAGICCRYSTAL, r.irange(0, 2)); MeshBuilder tmp; tmp.append(tp.solid); for (auto& v : tmp.v) { v.px = v.px * 0.7f + p.x; v.py = v.py * 0.7f + p.y; v.pz = v.pz * 0.7f + p.z; } pp.append(tmp); }
                    else pp.rock(p + vec3(0, 0.1f, 0), vec3(r.range(0.3f, 0.6f)), Mat(th.wall, 0.9f), r.next(), 0);
                }
            } else {
                // walls where void touches floor
                bool edge = false;
                for (int dz = -1; dz <= 1; dz++) for (int dx = -1; dx <= 1; dx++) { TileType n = T(x + dx, z + dz); if (n != TileType::Void && n != TileType::Pit) edge = true; }
                if (!edge) continue;
                float h = 4.5f + r.range(-0.4f, 0.6f);
                wl.bevelBox(c + vec3(0, h * 0.5f - 0.3f, 0), {TILE * 0.5f, h * 0.5f + 0.3f, TILE * 0.5f}, 0.25f, Mat(lerp(th.wall, th.wall * 1.15f, r.f()), D.theme == 4 ? 0.35f : 0.9f));
                wl.box(c + vec3(0, h + 0.05f, 0), {TILE * 0.52f, 0.12f, TILE * 0.52f}, Mat(th.trim, 0.8f));
                if (D.theme == 0 || D.theme == 2 || D.theme == 4) wl.rock(c + vec3(r.range(-1, 1), h, r.range(-1, 1)), vec3(1.6f, 0.8f, 1.6f), Mat(th.wall, 0.9f), r.next(), 0);
                Collider col; col.c = c.xz(); col.hx = TILE * 0.5f; col.hz = TILE * 0.5f; col.base = -1; col.top = 8;
                d.colliders.push_back(col);
            }
        }
    if (D.theme == 8) {
        // 2.2.2 celestial islands built like a world chunk: an organic island top with the world's grass and rock
        // textures (terrain shader), cliffs that drop away under a hanging root of stone, real grass, flowers, trees
        ChunkBuild cb; cb.liteProps = false; cb.region = R_ELDERWOOD;
        Rng ir(D.seed + 31);
        MeshBuilder& T = cb.terrain;
        for (size_t ri = 0; ri < d.rooms.size(); ri++) {
            const DungeonRoom& rm = d.rooms[ri];
            vec2 c0{d.origin.x + rm.x * TILE, d.origin.z + rm.z * TILE};
            vec2 ctr = c0 + vec2(rm.w * TILE * 0.5f, rm.h * TILE * 0.5f);
            float hx = rm.w * TILE * 0.5f, hz = rm.h * TILE * 0.5f, rr = 1.0f;
            const float M = 3.f, st = 1.f;
            int nx = (int)((hx * 2 + 2 * M) / st) + 1, nz = (int)((hz * 2 + 2 * M) / st) + 1;
            std::vector<float> H((size_t)nx * nz);
            auto edgeAt = [&](float x, float z) {
                vec2 q{std::fabs(x - ctr.x) - (hx - rr), std::fabs(z - ctr.y) - (hz - rr)};
                float sd = length(vec2(std::max(q.x, 0.f), std::max(q.y, 0.f))) + std::min(std::max(q.x, q.y), 0.f) - rr;
                float n = fbm2(x * 0.25f, z * 0.25f, 2, 4100 + (int)ri);
                return sd - std::max(n, 0.f) * 0.9f;   // ragged outline, never inside the walkable area
            };
            for (int j = 0; j < nz; j++) for (int i = 0; i < nx; i++) {
                float x = ctr.x - hx - M + i * st, z = ctr.y - hz - M + j * st;
                float e = edgeAt(x, z);
                H[(size_t)j * nx + i] = e <= 0.f ? 0.f : -std::min(9.f, e * 3.5f + e * e * 1.5f);
            }
            uint32_t base = (uint32_t)T.v.size();
            for (int j = 0; j < nz; j++) for (int i = 0; i < nx; i++) {
                float x = ctr.x - hx - M + i * st, z = ctr.y - hz - M + j * st, y = H[(size_t)j * nx + i];
                auto Hc = [&](int a, int b) { a = std::max(0, std::min(nx - 1, a)); b = std::max(0, std::min(nz - 1, b)); return H[(size_t)b * nx + a]; };
                vec3 nn = normalize(vec3(Hc(i - 1, j) - Hc(i + 1, j), 2.f * st, Hc(i, j - 1) - Hc(i, j + 1)));
                float n1 = fbm2(x * 0.09f, z * 0.09f, 2, 51) * 0.5f + 0.5f;
                Vertex v{};
                v.px = x; v.py = d.origin.y + y; v.pz = z;
                v.nx = (int8_t)(nn.x * 127); v.ny = (int8_t)(nn.y * 127); v.nz = (int8_t)(nn.z * 127);
                vec3 tint = vec3(0.92f, 1.04f, 0.98f) * (0.94f + 0.12f * n1);
                Color8 cc = toColor8(tint * 0.5f, 1.f);
                v.r = cc.r; v.g = cc.g; v.b = cc.b; v.a = 255;
                auto q8 = [](float f) { return (uint8_t)clampf(f * 255.f + 0.5f, 0, 255); };
                v.rough = q8(y < -0.15f ? 1.f : 0.f);
                v.metal = q8(y >= -0.15f ? smoothstepf(0.62f, 0.8f, n1) * 0.55f : 0.f);
                T.v.push_back(v);
            }
            for (int j = 0; j + 1 < nz; j++) for (int i = 0; i + 1 < nx; i++) {
                uint32_t a = base + j * nx + i, b = a + 1, cI = a + nx, dI = cI + 1;
                T.tri(a, cI, dI); T.tri(a, dI, b);
            }
            // the hanging root of the island: a big stone mass and a few long spires below
            wl.rock(vec3(ctr.x, d.origin.y - 6.f, ctr.y), vec3(hx * 0.95f, 5.f, hz * 0.95f), Mat(th.wall * 0.85f, 0.9f), ir.next(), 1);
            for (int k = 0; k < 5; k++) {
                vec3 tp(ctr.x + ir.range(-hx, hx) * 0.6f, d.origin.y - 8.f, ctr.y + ir.range(-hz, hz) * 0.6f);
                wl.cone(tp, tp - vec3(ir.range(-1.f, 1.f), ir.range(6.f, 14.f), ir.range(-1.f, 1.f)), ir.range(1.5f, 3.f), Mat(th.wall * 0.7f, 0.9f), 7);
            }
            d.emitters.push_back(vec4(ctr.x, d.origin.y - 7.f, ctr.y, EM_MIST));
            // trees and bushes along the island's rim (not where a bridge leaves, not in the boss arena)
            bool arena = ri + 1 == d.rooms.size();
            int nTrees = arena ? 0 : ir.irange(2, 4);
            for (int k = 0; k < nTrees * 3 && nTrees > 0; k++) {
                float ang = ir.range(0, TAU);
                vec2 pp2 = ctr + vec2(std::cos(ang) * (hx - 1.3f), std::sin(ang) * (hz - 1.3f));
                int tx = (int)std::floor((pp2.x - d.origin.x) / TILE), tz = (int)std::floor((pp2.y - d.origin.z) / TILE);
                bool nearBridge = false;
                for (int dz = -1; dz <= 1; dz++) for (int dx = -1; dx <= 1; dx++) {
                    int xx = tx + dx, zz = tz + dz;
                    if (xx < 0 || zz < 0 || xx >= d.W || zz >= d.H) continue;
                    bool inR = xx >= rm.x && xx < rm.x + rm.w && zz >= rm.z && zz < rm.z + rm.h;
                    if (!inR && d.tiles[zz * d.W + xx] != TileType::Void && d.tiles[zz * d.W + xx] != TileType::Pit) nearBridge = true;
                }
                if (nearBridge) continue;
                static const PropKind trees[] = {P_ELDERTREE, P_BIRCH, P_OAK, P_GIANTSHROOM};
                PropKind pk = trees[ir.irange(0, 3)];
                stampProp(cb, pk, ir.irange(0, propVariants(pk) - 1), {pp2.x, d.origin.y - 0.05f, pp2.y}, ir.range(0, TAU), ir.range(0.75f, 1.05f), vec3(1), true);
                if (pk == P_GIANTSHROOM || pk == P_ELDERTREE) cb.lights.push_back(vec4(pp2.x, d.origin.y + 2.5f, pp2.y, LI_MAGIC));
                if (--nTrees <= 0) break;
            }
            for (int k = 0; k < 8; k++) {
                float ang = ir.range(0, TAU), rad = ir.range(0.55f, 0.92f);
                vec2 q = ctr + vec2(std::cos(ang) * hx * rad, std::sin(ang) * hz * rad);
                PropKind pk = k % 3 == 0 ? P_BUSH : (k % 3 == 1 ? P_FERN : P_FLOWERS);
                stampProp(cb, pk, ir.irange(0, 2), {q.x, d.origin.y - 0.03f, q.y}, ir.range(0, TAU), ir.range(0.8f, 1.2f), vec3(1), false);
            }
            // grass (and a few flowers) over the island top
            for (float z = ctr.y - hz + 0.3f; z < ctr.y + hz - 0.2f; z += 0.62f)
                for (float x = ctr.x - hx + 0.3f; x < ctr.x + hx - 0.2f; x += 0.62f) {
                    float gx = x + ir.range(-0.25f, 0.25f), gz = z + ir.range(-0.25f, 0.25f);
                    if (edgeAt(gx, gz) > -0.2f) continue;
                    float patch = fbm2(gx * 0.05f, gz * 0.05f, 2, 77) * 0.5f + 0.5f;
                    if (ir.f() > 0.55f + 0.45f * patch) continue;
                    InstanceData g;
                    g.x = gx; g.y = d.origin.y - 0.03f; g.z = gz; g.rotY = ir.range(0, TAU); g.scale = ir.range(0.8f, 1.4f);
                    vec3 col = lerp(foliageColor(R_ELDERWOOD, patch), hexColor(0x6ac8a8), 0.15f);
                    bool flower = ir.f() < 0.045f;
                    if (flower) { static const uint32_t fc[] = {0xf0e0ff, 0x9af0ff, 0xf6c0e8, 0xffe08a}; col = hexColor(fc[ir.irange(0, 3)]); }
                    Color8 c8 = toColor8(col * 0.5f, 1);
                    g.r = c8.r; g.g = c8.g; g.b = c8.b; g.a = 255;
                    (flower ? cb.flowers : cb.grass).push_back(g);
                }
        }
        if (d.realm.ready) d.realm.destroy();
        World::uploadBuild(cb, d.realm);
        for (const Collider& c : d.realm.colliders) d.colliders.push_back(c);
        for (const vec4& l : d.realm.lights) d.lights.push_back(l);
    }
    // pillars & accent glows in rooms, cobwebs, lava cracks
    for (size_t i = 0; i < d.rooms.size(); i++) {
        auto& rm = d.rooms[i];
        if (D.theme == 8 && rm.w >= 4 && rm.h >= 4) for (int k = 0; k < 4; k++) {
            vec3 p = d.origin + vec3((rm.x + (k % 2 ? rm.w - 1 : 1)) * TILE, 0, (rm.z + (k / 2 ? rm.h - 1 : 1)) * TILE);
            wl.lathe({{0.7f, 0}, {0.55f, 0.5f}, {0.01f, 0.6f}}, Mat(th.trim, 0.6f), 8, p);
            pp.lathe({{0.01f, 1.2f}, {0.45f, 2.2f}, {0.01f, 4.2f}}, Mat(th.accent * 0.8f, 0.15f, 0, 2.5f), 6, p);
            d.lights.push_back(vec4(p + vec3(0, 2.6f, 0), LI_CRYSTAL));
            Collider col; col.c = p.xz(); col.r = 0.7f; col.base = -1; col.top = 5; d.colliders.push_back(col);
        }
        else if (rm.w >= 4 && rm.h >= 4) for (int k = 0; k < 4; k++) {
            vec3 p = d.origin + vec3((rm.x + (k % 2 ? rm.w - 1 : 1)) * TILE, 0, (rm.z + (k / 2 ? rm.h - 1 : 1)) * TILE);
            wl.lathe({{0.6f, 0}, {0.5f, 0.4f}, {0.45f, 4.f}, {0.6f, 4.3f}, {0.6f, 4.6f}, {0.01f, 4.6f}}, Mat(th.trim, 0.8f), 10, p);
            Collider col; col.c = p.xz(); col.r = 0.6f; col.base = -1; col.top = 8; d.colliders.push_back(col);
        }
        vec3 rc = d.origin + vec3((rm.x + rm.w * 0.5f) * TILE, 0, (rm.z + rm.h * 0.5f) * TILE);
        if (rm.kind == 2 || i == 0) { pp.lathe({{2.6f, 0.02f}, {0.01f, 0.03f}}, Mat(th.accent, 0.3f, 0, D.theme == 8 ? 0.6f : 2.f), 24, rc); d.lights.push_back(vec4(rc + vec3(0, 3, 0), LI_MAGIC)); d.emitters.push_back(vec4(rc, EM_SPARKLES)); }
    }
    if (th.lava) for (int i = 0; i < 12; i++) { auto& rm = d.rooms[r.irange(0, (int)d.rooms.size() - 1)]; vec3 p = d.origin + vec3((rm.x + r.f() * rm.w) * TILE, 0.01f, (rm.z + r.f() * rm.h) * TILE); pp.box(p, {r.range(0.8f, 2.f), 0.02f, 0.06f}, Mat(hexColor(0xff6a1a), 0.5f, 0, 5.f), quat::axisAngle({0, 1, 0}, r.range(0, PI))); }
    if (D.theme == 8) {
        // far islands drifting in the sky around the archipelago
        Rng fr(D.seed + 7);
        vec3 mid = d.origin + vec3(d.W * TILE * 0.5f, 0, d.H * TILE * 0.5f);
        for (int i = 0; i < 18; i++) {
            float a = i * TAU / 18 + fr.range(-0.2f, 0.2f), dist = fr.range(70.f, 140.f);
            vec3 c = mid + vec3(std::cos(a) * dist, fr.range(-25.f, 18.f), std::sin(a) * dist);
            float sc = fr.range(4.f, 11.f);
            wl.rock(c, vec3(sc, sc * 0.35f, sc * 0.9f), Mat(th.floor, 0.9f), fr.next(), 1);
            wl.cone(c - vec3(0, sc * 0.2f, 0), c - vec3(fr.range(-1.f, 1.f), sc * fr.range(1.2f, 2.f), fr.range(-1.f, 1.f)), sc * 0.8f, Mat(th.wall * 0.8f, 0.9f), 7);
            if (fr.chance(0.5f)) pp.lathe({{0.01f, 0}, {sc * 0.12f, sc * 0.4f}, {0.01f, sc * 0.9f}}, Mat(th.accent, 0.2f, 0, 2.5f), 6, c + vec3(0, sc * 0.3f, 0));
        }
    }
    if (D.theme != 8) dressDungeon(d, D.theme, th, wl, pp, (uint32_t)D.seed);   // 3.1.0
    // exit portal (the way back up)
    pp.lathe({{1.2f, 0.02f}, {0.01f, 0.04f}}, Mat(hexColor(0x8ad8ff), 0.2f, 0, 3.f), 20, d.exitPos);
    for (int i = 0; i < 6; i++) { float a = i * TAU / 6; pp.rock(d.exitPos + vec3(std::cos(a) * 1.5f, 0, std::sin(a) * 1.5f), vec3(0.3f, 0.5f, 0.3f), Mat(th.trim), 300 + i, 0); }
    d.lights.push_back(vec4(d.exitPos + vec3(0, 1.5f, 0), LI_MAGIC));
    if (d.floor.vao) d.floor.destroy();
    if (d.walls.vao) d.walls.destroy();
    if (d.props.vao) d.props.destroy();
    fl.upload(d.floor); wl.upload(d.walls); pp.upload(d.props);
}

void Game::enterDungeon(int idx) {
    const DungeonDef& D = dungeons()[idx];
    Character& p = pc();
    vec3 back = p.pos;
    if (riding) toggleMount();
    fadeTransition([this, idx, back, D] {
        buildDungeon(idx);
        dungeon.levelOverride = D.id == "domaine" ? std::max(5, base.dunLevel > 0 ? base.dunLevel : level) : 0;   // 3.0.0 the domain's dungeon
        dungeon.active = true;
        dungeon.returnPos = back;
        Character& c = pc();
        c.pos = dungeon.entrance; c.vel = vec3(0); c.yaw = 0;
        camFocus = c.pos;
        respawnPos = back;
        // populate rooms (except entrance and puzzle room)
        Rng r(D.seed + day);
        // 3.0.0 the dungeon's level is that of its strongest dweller: weaker kinds (a draugr in the Titans' abyss) are
        // raised to it, so a late dungeon stays a late dungeon whatever creatures it borrows from older lands
        int dunLevel = 0;
        for (auto& id : D.enemies) { int ei = enemyIndex(id); if (ei >= 0) dunLevel = std::max(dunLevel, enemies()[ei].level); }
        if (dungeon.levelOverride > 0) dunLevel = dungeon.levelOverride;   // the domain's own dungeon (Base.cpp)
        auto raise = [&](Entity e) {
            Character* c = reg.get<Character>(e);
            if (!c || c->level >= dunLevel) return;
            float dl = (float)(dunLevel - c->level);
            c->maxHp *= 1.f + dl * 0.25f; c->hp = c->maxHp; c->atk *= 1.f + dl * 0.12f; c->def += dl; c->level = dunLevel;
        };
        for (size_t i = 1; i < dungeon.rooms.size(); i++) {
            auto& rm = dungeon.rooms[i];
            if (rm.kind == 2 || rm.kind == 3) continue;
            int n = r.irange(2, 4);
            for (int k = 0; k < n; k++) {
                vec3 sp = dungeon.origin + vec3((rm.x + r.range(0.5f, rm.w - 0.5f)) * TILE, 0, (rm.z + r.range(0.5f, rm.h - 0.5f)) * TILE);
                int ei = enemyIndex(D.enemies[r.irange(0, (int)D.enemies.size() - 1)]);
                if (ei >= 0) { Entity e = spawnEnemy(ei, sp, r.chance(0.1f), 0); reg.get<AIBrain>(e)->home = sp; raise(e); }
            }
        }
        // a reward chest in the final room for dungeons without a boss, and one in the puzzle room
        auto& last = dungeon.rooms.back();
        NodeSpawn chest; chest.type = NodeType::Chest; chest.tier = 2;
        chest.pos = dungeon.origin + vec3((last.x + last.w * 0.5f) * TILE, 0, (last.z + last.h - 1.f) * TILE);
        chest.uid = hash32(9000 + idx * 17 + day); chest.rot = 0;
        if (D.boss < 0) spawnNode(chest);
        notify(D.name, vec3(0.85f, 0.8f, 1.f), "i_cave");
        regionBanner = D.name; regionBannerSub = "Donjon"; regionBannerT = 4.f;
        play("door");
    });
}

void Game::exitDungeon() {
    fadeTransition([this] {
        // remove dungeon inhabitants
        reg.each<Character>([&](Entity e, Character& c) { if (c.pos.x > WORLD_SIZE && e != player) reg.destroyLater(e); });
        reg.each<NodeComp>([&](Entity e, NodeComp& n) { if (n.node.pos.x > WORLD_SIZE) { nodeEntities.erase(n.node.uid); reg.destroyLater(e); } });
        if (bossEnt != NullEntity) bossEnt = NullEntity;
        Character& c = pc();
        vec3 rp = dungeon.returnPos;
        c.pos = rp; c.pos.y = world.groundHeight(rp.x, rp.z); c.vel = vec3(0);
        camFocus = c.pos;
        dungeon.active = false;
        dungeon.floor.destroy(); dungeon.walls.destroy(); dungeon.props.destroy(); dungeon.realm.destroy();
        dungeon.tiles.clear(); dungeon.def = -1;
        dungeon.interior = false; dungeon.hasBed = dungeon.hasForge = false;   // 2.4.1
        play("door");
    });
}

void Game::updateDungeon(float dt) {
    dungeonPrompt.clear();
    nearDungeon = -1;
    if (player == NullEntity) return;
    Character& p = pc();
    if (updateInterior(dt)) return;   // 2.4.1 inside a building
    // ---- overworld: entrances
    if (!dungeon.active) {
        findDoor();   // 2.4.1 building doors
        if (nearDoor >= 0) {
            const Structure& s = world.structures[nearDoor];
            static const char* nm[] = {"la maison"};
            (void)nm;
            dungeonPrompt = std::string("Entrer : ") + (s.kind == S_INN ? "l'auberge" : s.kind == S_CHAPEL ? "la chapelle" : s.kind == S_FORGE ? "la forge" : s.kind == S_STILT_HUT || s.kind == S_BEACHHUT ? "la cabane" : "la maison");
            if (input.interact && transitionT < 0) { enterInterior(nearDoor); return; }
        }
        // 3.0.0 the domain's own dungeon: down the stairs of the crypt the player built
        if (base.founded && dungeonPrompt.empty()) {
            int di = -1; for (int i = 0; i < (int)dungeons().size(); i++) if (dungeons()[i].id == "domaine") di = i;
            for (const BaseBuilt& b : base.built)
                if (di >= 0 && baseBuildings()[b.def].id == "crypt" && length(b.pos - p.pos.xz()) < 6.5f) {
                    int lv = std::max(5, base.dunLevel > 0 ? base.dunLevel : level);
                    nearDungeon = di;
                    dungeonPrompt = "Descendre : Donjon du domaine (niveau " + std::to_string(lv) + ")";
                    if (input.interact && transitionT < 0) enterDungeon(di);
                }
        }
        for (int i = 0; i < (int)dungeons().size(); i++) {
            const Poi* poi = dungeons()[i].poi.empty() ? nullptr : world.poiById(dungeons()[i].poi);
            if (!poi) continue;
            float d = length(poi->pos - p.pos.xz());
            if (d < 9.f) {
                const std::string& did = dungeons()[i].id;
                auto opened = [&](const char* q) { return questActive(q) || questsDone.count(q) > 0; };
                bool locked = (did == "abyss" && !opened("m7")) || (did == "sun_temple" && !opened("n6")) || (did == "silence" && !opened("n8"));
                const char* lockMsg = did == "abyss" ? "Le sceau de Morneval est encore intact. (Quête principale)"
                                    : (did == "sun_temple" ? "Les portes du temple sont scellées par une clé d'ambre. (Quête principale)" : "Le sanctuaire reste fermé. Maître Hanzo en garde la porte. (Quête principale)");
                nearDungeon = i;
                dungeonPrompt = locked ? (did == "abyss" ? "Un sceau ancien bloque le passage" : "Une porte scellée bloque le passage") : "Entrer : " + dungeons()[i].name;
                if (input.interact && !locked && transitionT < 0) enterDungeon(i);
                if (input.interact && locked) notify(lockMsg, vec3(0.8f, 0.6f, 1.f), "i_lock");
            }
        }
        return;
    }
    if (p.pos.x < WORLD_SIZE) return;
    DungeonState& d = dungeon;
    const DungeonDef& D = dungeons()[d.def];
    // ---- exit portal
    if (distXZ(p.pos, d.exitPos) < 2.2f) {
        dungeonPrompt = "Sortir du donjon";
        if (input.interact && transitionT < 0) { exitDungeon(); return; }
    }
    // ---- puzzle braziers
    if (!d.puzzleSolved) {
        for (size_t i = 0; i < d.braziers.size(); i++) {
            if (d.brazierLit[i] || distXZ(p.pos, d.braziers[i]) > 2.4f) continue;
            dungeonPrompt = "Allumer le brasero";
            if (input.interact) {
                int expected = d.puzzleOrder[d.puzzleProgress];
                if ((int)i == expected) {
                    d.brazierLit[i] = true; d.puzzleProgress++;
                    particles.magicBurst(d.braziers[i] + vec3(0, 1.3f, 0), elementColor((Element)(1 + i % 7)), 1.f, 20);
                    play("fireball", 0.6f, 1.3f);
                    if (d.puzzleProgress >= (int)d.braziers.size()) {
                        d.puzzleSolved = true; d.gateOpen = true;
                        play("lever"); play("door", 1.f, 0.7f);
                        notify("Un mécanisme s'enclenche : la herse se lève !", vec3(1, 0.9f, 0.6f), "i_lock");
                        addShake(0.3f);
                        gainXp(80);
                    }
                } else {
                    // wrong order: braziers go out and a trap fires
                    for (size_t k = 0; k < d.brazierLit.size(); k++) d.brazierLit[k] = false;
                    d.puzzleProgress = 0;
                    particles.explosion(d.braziers[i] + vec3(0, 1, 0), 0.5f);
                    dealDamage(NullEntity, player, p.maxHp * 0.08f, Element::Fire, vec3(0, 0, 1), 0, false);
                    notify("Mauvais ordre… Observez les gemmes de la herse.", vec3(1, 0.6f, 0.4f), "i_info");
                    play("trap");
                }
            }
        }
    }
    // gate collider while closed
    if (!d.gateOpen && distXZ(p.pos, d.gatePos) < TILE * 0.9f) {
        vec2 l = rotate(p.pos.xz() - d.gatePos.xz(), d.gateRot);
        if (std::fabs(l.y) < 0.8f) { p.pos.x -= (p.pos.x - d.gatePos.x) > 0 ? -dt * 0 : 0; vec2 push = rotate(vec2(0, l.y > 0 ? 0.8f - l.y : -0.8f - l.y), -d.gateRot); p.pos.x += push.x; p.pos.z += push.y; }
        if (distXZ(p.pos, d.gatePos) < 3.f && hintT <= 0) { toastHint = "La herse est fermée. Allumez les braseros dans l'ordre des gemmes."; hintT = 8; }
    }
    hintT = std::max(0.f, hintT - dt);
    // ---- traps
    float t = (float)gameTime;
    for (auto& tr : d.traps) {
        float ph = std::fmod(t * 0.5f + tr.second, 2.f);
        bool up = ph > 1.3f && ph < 1.9f;
        if (up && distXZ(p.pos, tr.first) < TILE * 0.5f && p.invuln <= 0) { dealDamage(NullEntity, player, p.maxHp * 0.12f, Element::Physical, vec3(0, 1, 0), 30, false); p.invuln = 0.8f; play("trap"); }
    }
    for (auto& b : d.blades) {
        float ang = std::sin(t * 1.8f) * 1.2f;
        vec3 tip = b + vec3(std::sin(ang) * 2.7f, 4.6f - std::cos(ang) * 2.7f, 0);
        if (length(tip - (p.pos + vec3(0, 1, 0))) < 1.1f && p.invuln <= 0) { dealDamage(NullEntity, player, p.maxHp * 0.15f, Element::Physical, vec3(std::cos(ang), 0, 0), 50, false); p.invuln = 0.8f; }
    }
    for (auto& f : d.fireJets) {
        float ph = std::fmod(t * 0.4f, 3.f);
        if (ph > 2.2f) {
            if (particles.rng.f() < dt * 40) { Particle q; q.pos = f + vec3(particles.rng.range(-0.3f, 0.3f), 0.1f, particles.rng.range(-0.3f, 0.3f)); q.vel = vec3(0, particles.rng.range(5, 8), 0); q.maxLife = 0.5f; q.size0 = 0.6f; q.size1 = 0.2f; q.c0 = vec4(3, 1.2f, 0.3f, 1); q.c1 = vec4(1, 0.2f, 0, 0); q.frame = PF_FLAME; particles.spawn(q); }
            if (distXZ(p.pos, f) < 1.3f && p.invuln <= 0) { dealDamage(NullEntity, player, p.maxHp * 0.1f, Element::Fire, vec3(0, 1, 0), 20, false); p.invuln = 0.8f; }
        } else if (ph > 1.8f && particles.rng.f() < dt * 10) particles.sparks(f + vec3(0, 0.1f, 0), {0, 1, 0}, 2, vec3(1, 0.5f, 0.2f));
    }
    // ---- pits: falling sends you back to the entrance
    if (p.pos.y < d.origin.y - 6.f) { p.pos = d.entrance; p.vel = vec3(0); dealDamage(NullEntity, player, p.maxHp * 0.15f, Element::Physical, vec3(0), 0, false); }
    // ---- boss room
    auto& br = d.rooms.back();
    vec3 brc = d.bossPos;
    bool inBossRoom = p.pos.x > d.origin.x + br.x * TILE && p.pos.x < d.origin.x + (br.x + br.w) * TILE && p.pos.z > d.origin.z + br.z * TILE && p.pos.z < d.origin.z + (br.z + br.h) * TILE;
    if (inBossRoom && D.boss >= 0 && !defeatedBosses.count(bosses()[D.boss].id) && bossEnt == NullEntity) spawnBoss(D.boss, brc);
    // ---- completion
    bool done = D.boss >= 0 ? defeatedBosses.count(bosses()[D.boss].id) > 0 : inBossRoom;
    if (done && !d.completed) {
        d.completed = true;
        if (!dungeonsDone.count(D.id)) {
            dungeonsDone.insert(D.id);
            if (!D.reward.empty()) addItem(D.reward, 1);
            gainXp(250 + d.def * 120);
            notify("Donjon terminé : " + D.name, vec3(0.6f, 1.f, 0.6f), "i_check");
        }
        advanceQuests(StepKind::Dungeon, D.id);
        if (D.id == "catacombs") advanceQuests(StepKind::Dungeon, "graveyard");
    }
}

void Game::renderDungeon() {
    DungeonState& d = dungeon;
    if (!d.active || !d.floor.valid()) return;
    DrawItem it;
    it.mesh = &d.floor; renderer.submit(it);
    it.mesh = &d.walls; renderer.submit(it);
    if (d.theme == 8 && d.realm.ready) {   // 2.2.2 the islands, drawn like a world chunk
        Chunk& ch = d.realm;
        DrawItem r2; r2.tint = vec4(1, 1, 1, 0);
        if (ch.terrain.valid()) { r2.mesh = &ch.terrain; r2.flags = LIT_TERRAIN; renderer.submit(r2); }
        if (ch.props.valid()) { r2.mesh = &ch.props; r2.flags = 0; renderer.submit(r2); }
        if (ch.foliage.valid()) { r2.mesh = &ch.foliage; r2.flags = LIT_WIND; renderer.submit(r2); }
        r2.castShadow = false;
        if (ch.grass.valid()) { r2.mesh = &ch.grass; r2.flags = LIT_INSTANCED | LIT_WIND | LIT_GRASS; renderer.submit(r2); }
        if (ch.flowers.valid()) { r2.mesh = &ch.flowers; r2.flags = LIT_INSTANCED | LIT_WIND | LIT_GRASS; renderer.submit(r2); }
    }
    if (d.props.valid()) { it.mesh = &d.props; renderer.submit(it); }
    float t = (float)gameTime;
    for (size_t i = 0; i < d.braziers.size(); i++) {
        DrawItem b; b.mesh = brazierMesh.get(); b.model = mat4::translate(d.braziers[i]); renderer.submit(b);
        if (d.brazierLit[i]) {
            vec3 col = elementColor((Element)(1 + i % 7));
            if (particles.rng.f() < 0.5f) { Particle q; q.pos = d.braziers[i] + vec3(particles.rng.range(-0.2f, 0.2f), 1.2f, particles.rng.range(-0.2f, 0.2f)); q.vel = vec3(0, 1.4f, 0); q.maxLife = 0.6f; q.size0 = 0.5f; q.size1 = 0.1f; q.c0 = vec4(col * 3.f, 1); q.c1 = vec4(col, 0); q.frame = PF_FLAME; particles.spawn(q); }
            PointLight pl; pl.pos = d.braziers[i] + vec3(0, 1.6f, 0); pl.radius = 8; pl.color = col * 3.f; pl.flicker = 1; renderer.addLight(pl);
        }
    }
    // gate with coloured gems showing the order
    if (d.gateRoom >= 0) {
        static float lift = 0;
        lift = d.gateOpen ? std::min(4.2f, lift + 0.05f) : 0.f;
        DrawItem g; g.mesh = gateMesh.get(); g.model = mat4::trs(d.gatePos + vec3(0, lift, 0), quat::axisAngle({0, 1, 0}, d.gateRot), vec3(1)); renderer.submit(g);
        if (!d.gateOpen) for (size_t k = 0; k < d.puzzleOrder.size(); k++) {
            DrawItem o; o.mesh = orbMesh.get();
            vec3 off = rotate3(vec3((k - 1.5f) * 0.7f, 3.6f, 0), -d.gateRot);
            o.model = mat4::trs(d.gatePos + off, quat(), vec3(0.17f));
            o.tint = vec4(elementColor((Element)(1 + d.puzzleOrder[k] % 7)), 0); o.fx.w = 2.f; o.castShadow = false;
            renderer.submit(o);
        }
    }
    for (auto& tr : d.traps) {
        float ph = std::fmod(t * 0.5f + tr.second, 2.f);
        float h = ph > 1.3f && ph < 1.9f ? 0.f : (ph > 1.15f && ph <= 1.3f ? -0.6f : -0.95f);
        DrawItem s; s.mesh = spikeMesh.get(); s.model = mat4::translate(tr.first + vec3(0, h, 0)); renderer.submit(s);
    }
    for (auto& b : d.blades) {
        float ang = std::sin(t * 1.8f) * 1.2f;
        DrawItem s; s.mesh = bladeMesh.get(); s.model = mat4::trs(b + vec3(0, 4.6f, 0), quat::axisAngle({0, 0, 1}, ang), vec3(1)); renderer.submit(s);
    }
}

}  // namespace lk
