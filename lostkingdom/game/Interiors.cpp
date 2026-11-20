// LOST KINGDOM 2.4.1 — you can walk into the buildings. Houses, the inn, the chapel, the forge, sandstone and stilt
// houses: a door prompt in front of them, a fade, and a furnished room built off the map (the dungeon machinery:
// floor, walls, colliders, lights; theme 9, no ceiling so the camera can look in). Sleep in the bed, work at the
// forge, open the house chest once, and walk out through the door.
#include "Game.h"
#include <algorithm>

namespace lk {

vec3 rotate3(vec3 v, float a);
static const float ITILE = 4.f;

bool Game::enterableKind(int k) const {
    return k == S_HOUSE || k == S_STONEHOUSE || k == S_LONGHOUSE || k == S_INN || k == S_CHAPEL || k == S_FORGE || k == S_DOMEHOUSE || k == S_STILT_HUT || k == S_BEACHHUT;
}

static const char* interiorName(int k) {
    switch (k) {
    case S_INN: return "Auberge"; case S_CHAPEL: return "Chapelle"; case S_FORGE: return "Forge"; case S_LONGHOUSE: return "Longère";
    case S_STONEHOUSE: return "Maison de pierre"; case S_DOMEHOUSE: return "Maison de grès"; case S_STILT_HUT: case S_BEACHHUT: return "Cabane";
    default: return "Maison";
    }
}

vec3 Game::doorOf(const Structure& s) const {
    vec2 fp = structureFootprint(s.kind) * s.scale;
    vec2 fwd{std::sin(s.rot), std::cos(s.rot)};
    vec2 d = s.pos + fwd * (fp.y + 1.1f);
    return vec3(d.x, world.groundHeight(d.x, d.y), d.y);
}

// the door the hero stands in front of (overworld)
void Game::findDoor() {
    nearDoor = -1;
    if (player == NullEntity || dungeon.active || driving >= 0 || riding || interactTarget != NullEntity) return;   // a villager / a chest nearby keeps the button
    Character& p = pc();
    int cx = (int)std::floor(p.pos.x / CHUNK), cz = (int)std::floor(p.pos.z / CHUNK);
    float best = 2.6f;
    for (int dz = -1; dz <= 1; dz++)
        for (int dx = -1; dx <= 1; dx++) {
            int ci = chunkIndex(cx + dx, cz + dz);
            if (ci < 0 || ci >= (int)world.chunkStructures.size()) continue;
            for (int si : world.chunkStructures[ci]) {
                const Structure& s = world.structures[si];
                if (!enterableKind(s.kind)) continue;
                vec3 dpos = doorOf(s);
                float d = distXZ(dpos, p.pos);
                if (d < best && std::fabs(dpos.y - p.pos.y) < 2.5f) { best = d; nearDoor = si; }
            }
        }
}

// ---------------------------------------------------------------------------------------------- room building
void Game::buildInterior(int si) {
    const Structure& S = world.structures[si];
    DungeonState& d = dungeon;
    d = DungeonState();
    d.interior = true; d.interiorKind = S.kind; d.interiorStruct = si; d.theme = 9;
    d.interiorRegion = world.regionId(S.pos.x, S.pos.y); d.interiorName = interiorName(S.kind);
    // room size (metres) by building kind
    float rw = 8.f, rd = 7.f, wallH = 3.4f;
    switch (S.kind) {
    case S_INN: rw = 14.f; rd = 11.f; wallH = 4.2f; break;
    case S_CHAPEL: rw = 9.f; rd = 15.f; wallH = 6.f; break;
    case S_LONGHOUSE: rw = 7.f; rd = 15.f; break;
    case S_FORGE: rw = 9.f; rd = 8.f; wallH = 4.f; break;
    case S_STILT_HUT: case S_BEACHHUT: rw = 6.f; rd = 6.f; wallH = 3.f; break;
    default: break;
    }
    d.W = (int)std::ceil(rw / ITILE) + 2; d.H = (int)std::ceil(rd / ITILE) + 2;
    d.origin = vec3(WORLD_SIZE + 4200.f, 0.f, 600.f);
    d.tiles.assign(d.W * d.H, TileType::Floor);
    vec3 c0 = d.origin + vec3(ITILE, 0, ITILE);              // room corner (x0, z0)
    vec3 C = c0 + vec3(rw * 0.5f, 0, rd * 0.5f);              // centre
    Rng r(hash32((uint32_t)si * 7919u + 17u));
    bool desert = S.kind == S_DOMEHOUSE, hut = S.kind == S_STILT_HUT || S.kind == S_BEACHHUT, stone = S.kind == S_CHAPEL || S.kind == S_STONEHOUSE || S.kind == S_FORGE;
    vec3 plaster = desert ? hexColor(0xd8b888) : (stone ? hexColor(0x9a948a) : hexColor(0xe6dcc6));
    vec3 timber = hut ? hexColor(0x8a6a44) : hexColor(0x4a3220);
    Mat wall(plaster, 0.95f), beam(timber, 0.85f), plank(hut ? hexColor(0xa88458) : hexColor(0x7a5434), 0.8f), plankDark(hexColor(0x5a3a22), 0.8f);
    Mat stoneM(hexColor(0x6e6a64), 0.9f), cloth(hexColor(0x8e2a24), 0.9f), clothB(hexColor(0x2a4f7a), 0.9f), iron(hexColor(0x3a3a3e), 0.4f, 0.8f);
    Mat glowWin(hexColor(0xfff0c8), 0.3f, 0, 1.6f), fireM(hexColor(0xff8a2a), 0.5f, 0, 3.f), gold(hexColor(0xd8b050), 0.3f, 1.f);
    MeshBuilder F, Wm, P;
    // ---- floor: planks (wood buildings) or flagstones (stone ones)
    if (stone || desert) {
        for (float z = 0; z < rd; z += 1.f) for (float x = 0; x < rw; x += 1.f) {
            float t = r.range(0.85f, 1.1f);
            F.bevelBox(c0 + vec3(x + 0.5f, -0.05f, z + 0.5f), vec3(0.47f, 0.06f, 0.47f), 0.03f, Mat((desert ? hexColor(0xc8a878) : hexColor(0x7a746c)) * t, 0.9f));
        }
    } else {
        for (float x = 0; x < rw; x += 0.5f) {
            float t = r.range(0.8f, 1.15f);
            F.box(c0 + vec3(x + 0.25f, -0.05f, rd * 0.5f), vec3(0.24f, 0.06f, rd * 0.5f), Mat(plank.color * t, 0.85f));
        }
    }
    // ---- walls (0.3 m), timber posts, top beam; the door gap in the middle of the front wall (z = rd)
    float th = 0.3f;
    auto wallSeg = [&](vec3 a, vec3 b) {   // straight wall from a to b along x or z
        vec3 m = (a + b) * 0.5f; vec3 h = (b - a) * 0.5f;
        vec3 half(std::fabs(h.x) + th * 0.5f, wallH * 0.5f, std::fabs(h.z) + th * 0.5f);
        if (std::fabs(h.x) > std::fabs(h.z)) half.z = th * 0.5f; else half.x = th * 0.5f;
        Wm.box(m + vec3(0, wallH * 0.5f, 0), half, wall);
        Collider col; col.c = m.xz(); col.hx = half.x; col.hz = half.z; col.top = wallH; col.base = 0; d.colliders.push_back(col);
    };
    wallSeg(c0, c0 + vec3(rw, 0, 0));                                  // back wall
    wallSeg(c0, c0 + vec3(0, 0, rd));                                  // left
    wallSeg(c0 + vec3(rw, 0, 0), c0 + vec3(rw, 0, rd));                // right
    float doorW = 1.6f;
    wallSeg(c0 + vec3(0, 0, rd), c0 + vec3(rw * 0.5f - doorW * 0.5f, 0, rd));
    wallSeg(c0 + vec3(rw * 0.5f + doorW * 0.5f, 0, rd), c0 + vec3(rw, 0, rd));
    Wm.box(c0 + vec3(rw * 0.5f, wallH - (wallH - 2.3f) * 0.5f, rd), vec3(doorW * 0.5f + 0.1f, (wallH - 2.3f) * 0.5f, th * 0.5f), wall);   // lintel
    Wm.box(c0 + vec3(rw * 0.5f, 1.15f, rd + 0.05f), vec3(doorW * 0.5f, 1.15f, 0.06f), plankDark);                                          // the door itself (closed behind you)
    if (!desert) {
        for (float x = 0; x <= rw + 0.01f; x += 2.f) { Wm.box(c0 + vec3(x, wallH * 0.5f, 0.17f), vec3(0.11f, wallH * 0.5f, 0.06f), beam); Wm.box(c0 + vec3(x, wallH * 0.5f, rd - 0.17f), vec3(0.11f, wallH * 0.5f, 0.06f), beam); }
        for (float z = 0; z <= rd + 0.01f; z += 2.f) { Wm.box(c0 + vec3(0.17f, wallH * 0.5f, z), vec3(0.06f, wallH * 0.5f, 0.11f), beam); Wm.box(c0 + vec3(rw - 0.17f, wallH * 0.5f, z), vec3(0.06f, wallH * 0.5f, 0.11f), beam); }
        Wm.box(c0 + vec3(rw * 0.5f, wallH - 0.15f, 0.2f), vec3(rw * 0.5f, 0.12f, 0.1f), beam);
        Wm.box(c0 + vec3(0.2f, wallH - 0.15f, rd * 0.5f), vec3(0.1f, 0.12f, rd * 0.5f), beam);
        Wm.box(c0 + vec3(rw - 0.2f, wallH - 0.15f, rd * 0.5f), vec3(0.1f, 0.12f, rd * 0.5f), beam);
    }
    // windows: warm daylight panes on the side walls
    for (float z = 2.f; z < rd - 1.5f; z += 3.5f) {
        Wm.box(c0 + vec3(0.16f, 1.9f, z), vec3(0.02f, 0.55f, 0.5f), S.kind == S_CHAPEL ? Mat(hexColor(0x8ab0ff), 0.3f, 0, 1.4f) : glowWin);
        Wm.box(c0 + vec3(rw - 0.16f, 1.9f, z), vec3(0.02f, 0.55f, 0.5f), S.kind == S_CHAPEL ? Mat(hexColor(0xffb070), 0.3f, 0, 1.4f) : glowWin);
    }
    // ---- furniture
    auto solid = [&](vec3 p, float hx, float hz, float top) { Collider col; col.c = p.xz(); col.hx = hx; col.hz = hz; col.top = top; col.base = 0; d.colliders.push_back(col); };
    auto table = [&](vec3 p, float w, float l) {
        P.bevelBox(p + vec3(0, 0.78f, 0), vec3(w * 0.5f, 0.05f, l * 0.5f), 0.02f, plank);
        for (int k = 0; k < 4; k++) P.box(p + vec3((k & 1 ? 1 : -1) * (w * 0.5f - 0.1f), 0.38f, (k & 2 ? 1 : -1) * (l * 0.5f - 0.1f)), vec3(0.05f, 0.38f, 0.05f), plankDark);
        solid(p, w * 0.5f, l * 0.5f, 0.82f);
    };
    auto stool = [&](vec3 p) { P.lathe({{0.2f, 0.45f}, {0.2f, 0.5f}, {0.f, 0.5f}}, plank, 8, p); P.tube(p, p + vec3(0, 0.45f, 0), 0.04f, 0.04f, plankDark, 5, false); };
    auto barrel = [&](vec3 p) { P.lathe({{0.28f, 0}, {0.34f, 0.45f}, {0.28f, 0.9f}, {0.f, 0.9f}}, Mat(hexColor(0x7a5432), 0.85f), 10, p); P.lathe({{0.345f, 0.25f}, {0.345f, 0.3f}}, iron, 10, p); P.lathe({{0.345f, 0.6f}, {0.345f, 0.65f}}, iron, 10, p); solid(p, 0.32f, 0.32f, 0.9f); };
    auto shelf = [&](vec3 p, float w, bool alongX) {
        for (int k = 0; k < 3; k++) P.box(p + vec3(0, 0.6f + k * 0.55f, 0), alongX ? vec3(w * 0.5f, 0.03f, 0.18f) : vec3(0.18f, 0.03f, w * 0.5f), plank);
        for (int k = 0; k < 7; k++) {
            float o = r.range(-w * 0.45f, w * 0.45f); int lvl = r.irange(0, 2);
            vec3 q = p + (alongX ? vec3(o, 0.63f + lvl * 0.55f, 0) : vec3(0, 0.63f + lvl * 0.55f, o));
            vec3 jc = k % 3 == 0 ? hexColor(0x6a8a4a) : (k % 3 == 1 ? hexColor(0xb88a5a) : hexColor(0x4a6a9a));
            P.lathe({{0.07f, 0}, {0.09f, 0.12f}, {0.05f, 0.22f}, {0.f, 0.24f}}, Mat(jc, 0.4f), 8, q);
        }
    };
    auto hearth = [&](vec3 p, float rot) {   // stone fireplace against a wall, fire and light
        quat q = quat::axisAngle({0, 1, 0}, rot);
        P.bevelBox(p + rotate3(vec3(0, 0.9f, 0), rot), vec3(1.0f, 0.9f, 0.45f), 0.08f, stoneM, q);
        P.box(p + rotate3(vec3(0, 0.55f, 0.32f), rot), vec3(0.6f, 0.45f, 0.2f), Mat(hexColor(0x1a1410), 0.9f), q);
        P.box(p + rotate3(vec3(0, 2.6f, -0.1f), rot), vec3(0.45f, 1.0f, 0.3f), stoneM, q);
        for (int k = 0; k < 3; k++) P.tube(p + rotate3(vec3(-0.3f + k * 0.3f, 0.18f, 0.35f), rot), p + rotate3(vec3(0.3f - k * 0.3f, 0.22f, 0.5f), rot), 0.06f, 0.05f, plankDark, 6, true);
        P.ellipsoid(p + rotate3(vec3(0, 0.38f, 0.42f), rot), vec3(0.28f, 0.22f, 0.18f), fireM, 8, 5);
        d.lights.push_back(vec4(p + rotate3(vec3(0, 0.8f, 0.9f), rot), LI_FIRE));
        d.emitters.push_back(vec4(p + rotate3(vec3(0, 0.35f, 0.45f), rot), EM_FIRE));
        solid(p, 1.0f, 0.45f, 1.8f);
    };
    auto bed = [&](vec3 p, Mat blanket) {
        P.bevelBox(p + vec3(0, 0.25f, 0), vec3(0.55f, 0.25f, 1.05f), 0.04f, plankDark);
        P.bevelBox(p + vec3(0, 0.55f, 0.05f), vec3(0.5f, 0.1f, 0.98f), 0.06f, Mat(hexColor(0xe8e0d0), 0.95f));
        P.bevelBox(p + vec3(0, 0.62f, 0.25f), vec3(0.52f, 0.06f, 0.75f), 0.05f, blanket);
        P.ellipsoid(p + vec3(0, 0.7f, -0.75f), vec3(0.32f, 0.08f, 0.17f), Mat(hexColor(0xf4f0e8), 0.95f), 8, 5);
        P.box(p + vec3(0, 0.75f, -1.05f), vec3(0.58f, 0.75f, 0.05f), plankDark);
        solid(p, 0.55f, 1.05f, 0.7f);
        d.bedPos = p; d.hasBed = true;
    };
    auto rug = [&](vec3 p, float w, float l, Mat m) { P.box(p + vec3(0, 0.01f, 0), vec3(w * 0.5f, 0.01f, l * 0.5f), m); P.box(p + vec3(0, 0.012f, 0), vec3(w * 0.5f - 0.15f, 0.01f, l * 0.5f - 0.15f), Mat(m.color * 1.35f, 0.95f)); };
    auto lantern = [&](vec3 p) { P.tube(p + vec3(0, 0.4f, 0), p + vec3(0, 1.2f, 0), 0.015f, 0.015f, iron, 4, false); P.box(p, vec3(0.12f, 0.16f, 0.12f), Mat(hexColor(0xffd890), 0.3f, 0, 2.2f)); d.lights.push_back(vec4(p, LI_LANTERN)); };
    vec3 B = c0 + vec3(0, 0, 0);   // back-left corner shorthand
    switch (S.kind) {
    case S_INN: {
        // the bar along the back, bottles, tables with stools, a big hearth, barrels, lanterns
        P.bevelBox(c0 + vec3(rw * 0.5f, 0.55f, 2.2f), vec3(rw * 0.32f, 0.55f, 0.35f), 0.05f, plank); solid(c0 + vec3(rw * 0.5f, 0, 2.2f), rw * 0.32f, 0.35f, 1.1f);
        P.box(c0 + vec3(rw * 0.5f, 1.12f, 2.2f), vec3(rw * 0.33f, 0.04f, 0.42f), plankDark);
        shelf(c0 + vec3(rw * 0.5f, 0, 0.45f), rw * 0.55f, true);
        for (int k = 0; k < 4; k++) barrel(c0 + vec3(0.7f, 0, 1.0f + k * 0.8f));
        for (int k = 0; k < 4; k++) { vec3 tp = c0 + vec3(3.5f + (k % 2) * 6.f, 0, 5.5f + (k / 2) * 3.2f); table(tp, 1.4f, 1.0f); for (int s2 = 0; s2 < 4; s2++) stool(tp + vec3((s2 & 1 ? 1 : -1) * 0.95f, 0, (s2 & 2 ? 1 : -1) * 0.5f)); }
        hearth(c0 + vec3(rw - 0.6f, 0, rd * 0.55f), -PI * 0.5f);
        for (int k = 0; k < 3; k++) lantern(c0 + vec3(3.f + k * 4.f, wallH - 1.3f, rd * 0.6f));
        bed(c0 + vec3(rw - 1.4f, 0, 1.6f), cloth);   // a room to rent behind the bar
        rug(c0 + vec3(rw * 0.5f, 0, rd - 2.f), 2.4f, 1.6f, cloth);
        break;
    }
    case S_CHAPEL: {
        // pews in two rows, an aisle carpet, the altar with candles, coloured windows
        for (int k = 0; k < 5; k++) for (int s2 = -1; s2 <= 1; s2 += 2) {
            vec3 p = C + vec3(s2 * 2.3f, 0, 1.2f + k * 1.5f - rd * 0.15f);
            P.box(p + vec3(0, 0.45f, 0), vec3(1.4f, 0.05f, 0.25f), plank); P.box(p + vec3(0, 0.8f, 0.24f), vec3(1.4f, 0.35f, 0.04f), plank);
            solid(p, 1.4f, 0.3f, 0.9f);
        }
        rug(C + vec3(0, 0, 1.f), 1.4f, rd - 3.f, cloth);
        P.bevelBox(c0 + vec3(rw * 0.5f, 0.55f, 1.4f), vec3(1.3f, 0.55f, 0.55f), 0.05f, Mat(hexColor(0xd8d0c0), 0.6f)); solid(c0 + vec3(rw * 0.5f, 0, 1.4f), 1.3f, 0.55f, 1.1f);
        P.box(c0 + vec3(rw * 0.5f, 1.12f, 1.4f), vec3(1.35f, 0.02f, 0.6f), cloth);
        for (int k = 0; k < 5; k++) { vec3 cp = c0 + vec3(rw * 0.5f - 1.f + k * 0.5f, 1.14f, 1.2f); P.tube(cp, cp + vec3(0, 0.25f, 0), 0.03f, 0.03f, Mat(hexColor(0xf4f0e0), 0.6f), 5, true); P.ellipsoid(cp + vec3(0, 0.3f, 0), vec3(0.025f, 0.05f, 0.025f), fireM, 5, 4); }
        d.lights.push_back(vec4(c0 + vec3(rw * 0.5f, 1.6f, 1.4f), LI_FIRE));
        P.box(c0 + vec3(rw * 0.5f, 3.6f, 0.2f), vec3(0.08f, 1.0f, 0.05f), gold); P.box(c0 + vec3(rw * 0.5f, 4.0f, 0.2f), vec3(0.5f, 0.08f, 0.05f), gold);   // the sun-cross
        Wm.box(c0 + vec3(rw * 0.5f, 4.4f, 0.17f), vec3(1.0f, 1.2f, 0.02f), Mat(hexColor(0xc070ff), 0.3f, 0, 1.6f));
        for (int k = 0; k < 2; k++) lantern(c0 + vec3(rw * (0.3f + k * 0.4f), wallH - 2.f, rd * 0.5f));
        break;
    }
    case S_FORGE: {
        // forge hearth with bellows, anvil, quench barrel, tool rack, grindstone
        P.bevelBox(c0 + vec3(1.4f, 0.6f, 1.2f), vec3(1.1f, 0.6f, 0.9f), 0.08f, stoneM); solid(c0 + vec3(1.4f, 0, 1.2f), 1.1f, 0.9f, 1.2f);
        P.ellipsoid(c0 + vec3(1.4f, 1.25f, 1.2f), vec3(0.6f, 0.15f, 0.45f), fireM, 10, 5);
        P.box(c0 + vec3(1.4f, 2.6f, 0.9f), vec3(0.7f, 1.4f, 0.5f), stoneM);
        d.lights.push_back(vec4(c0 + vec3(1.4f, 1.8f, 1.8f), LI_FIRE)); d.emitters.push_back(vec4(c0 + vec3(1.4f, 1.3f, 1.2f), EM_FIRE));
        vec3 an = c0 + vec3(rw * 0.5f, 0, rd * 0.45f);
        P.box(an + vec3(0, 0.35f, 0), vec3(0.25f, 0.35f, 0.25f), plankDark);
        P.bevelBox(an + vec3(0, 0.82f, 0), vec3(0.22f, 0.12f, 0.5f), 0.03f, iron); P.cone(an + vec3(0, 0.82f, 0.48f), an + vec3(0, 0.84f, 0.8f), 0.1f, iron, 6);
        solid(an, 0.3f, 0.5f, 0.95f);
        d.forgePos = an; d.hasForge = true;
        barrel(c0 + vec3(rw - 1.f, 0, 1.2f)); barrel(c0 + vec3(rw - 1.f, 0, 2.1f));
        for (int k = 0; k < 4; k++) P.tube(c0 + vec3(rw - 0.3f, 1.2f, 3.5f + k * 0.4f), c0 + vec3(rw - 0.3f, 2.0f, 3.5f + k * 0.4f), 0.03f, 0.03f, plankDark, 5, true);
        P.box(c0 + vec3(rw - 0.25f, 1.9f, 4.1f), vec3(0.05f, 0.04f, 0.9f), plank);
        P.lathe({{0.45f, 0.f}, {0.45f, 0.12f}, {0.f, 0.12f}}, stoneM, 14, c0 + vec3(rw * 0.75f, 0.7f, rd - 1.6f), quat::axisAngle({0, 0, 1}, PI * 0.5f));
        lantern(c0 + vec3(rw * 0.5f, wallH - 1.2f, rd * 0.6f));
        break;
    }
    default: {
        // a home: bed, hearth, table and stools, shelves, a chest, barrels, rug, a lantern
        bool big = S.kind == S_LONGHOUSE;
        bed(c0 + vec3(1.0f, 0, 1.5f), r.chance(0.5f) ? cloth : clothB);
        if (big) bed(c0 + vec3(2.4f, 0, 1.5f), clothB);
        hearth(c0 + vec3(rw - 0.6f, 0, big ? rd * 0.4f : rd * 0.45f), -PI * 0.5f);
        vec3 tp = c0 + vec3(rw * 0.45f, 0, rd * (big ? 0.6f : 0.62f));
        table(tp, 1.4f, 0.9f); stool(tp + vec3(-0.95f, 0, 0)); stool(tp + vec3(0.95f, 0, 0)); stool(tp + vec3(0, 0, 0.75f));
        P.lathe({{0.1f, 0}, {0.12f, 0.1f}, {0.06f, 0.2f}, {0.f, 0.22f}}, Mat(hexColor(0xb07a4a), 0.5f), 8, tp + vec3(0.2f, 0.83f, 0));   // a jug
        P.ellipsoid(tp + vec3(-0.3f, 0.86f, 0.1f), vec3(0.15f, 0.04f, 0.15f), Mat(hexColor(0xd8c8a0), 0.6f), 8, 4);                   // a bowl
        shelf(c0 + vec3(rw * 0.5f, 0, 0.35f), 2.4f, true);
        barrel(c0 + vec3(rw - 0.6f, 0, rd - 1.0f));
        rug(c0 + vec3(rw * 0.45f, 0, rd * 0.62f), 2.6f, 1.8f, desert ? Mat(hexColor(0xb8402a), 0.95f) : clothB);
        lantern(c0 + vec3(rw * 0.5f, wallH - 1.1f, rd * 0.5f));
        if (big) { table(c0 + vec3(rw * 0.5f, 0, rd - 3.f), 1.2f, 2.2f); barrel(c0 + vec3(0.7f, 0, rd - 2.f)); }
        break;
    }
    }
    (void)B;
    // the house chest: once per building (its uid is saved with the opened chests)
    if (S.kind != S_CHAPEL) {
        NodeSpawn ch; ch.type = NodeType::Chest; ch.tier = S.kind == S_INN ? 1 : 0; ch.rot = PI;
        ch.pos = c0 + vec3(S.kind == S_FORGE ? rw - 1.f : 2.6f, 0, S.kind == S_FORGE ? rd - 1.f : 0.7f);
        ch.uid = hash32(0x1A7E0000u + (uint32_t)si);
        if (!openedChests.count(ch.uid)) spawnNode(ch);
    }
    F.bakeHeightAO(0, 0.3f, 0.85f);
    P.bakeHeightAO(0, 1.2f, 0.7f);
    F.upload(d.floor); Wm.upload(d.walls); P.upload(d.props);
    d.entrance = c0 + vec3(rw * 0.5f, 0, rd - 1.3f);
    d.exitPos = c0 + vec3(rw * 0.5f, 0, rd - 0.45f);
    d.bossPos = C;
    d.rooms.push_back(DungeonRoom{1, 1, d.W - 2, d.H - 2, 0});
}

void Game::enterInterior(int si) {
    if (si < 0 || si >= (int)world.structures.size()) return;
    const Structure s = world.structures[si];
    vec3 out = doorOf(s);
    if (riding) toggleMount();
    digMode = false;
    fadeTransition([this, si, out, s] {
        buildInterior(si);
        dungeon.active = true;
        dungeon.returnPos = out;
        Character& c = pc();
        c.pos = dungeon.entrance; c.vel = vec3(0); c.yaw = PI;   // facing into the room
        camFocus = c.pos; camYaw = 0.f; camPitch = std::max(camPitch, 0.75f); camDist = camDistTarget = 7.f;
        regionBanner = dungeon.interiorName; regionBannerSub = ""; regionBannerT = 2.5f;
        play("door");
    });
}

// inside a building: the door, the bed, the forge. Returns true when inside (the dungeon logic is skipped).
bool Game::updateInterior(float dt) {
    (void)dt;
    if (!dungeon.active || !dungeon.interior) return false;
    if (player == NullEntity) return true;
    Character& p = pc();
    DungeonState& d = dungeon;
    if (distXZ(p.pos, d.exitPos) < 1.6f) {
        dungeonPrompt = "Sortir";
        if (input.interact && transitionT < 0) { exitDungeon(); return true; }
    } else if (d.hasBed && distXZ(p.pos, d.bedPos) < 1.9f) {
        dungeonPrompt = "Dormir jusqu'au matin";
        if (input.interact && transitionT < 0) {
            fadeTransition([this] {
                Character& c = pc();
                c.hp = c.maxHp; mp = maxMp; stamina = maxStamina; c.status = Status();
                if (timeOfDay > 7.f) day++;
                timeOfDay = 7.f;
                saveGame(99);
                notify("Vous avez bien dormi. Vie, mana et endurance restaurées. (Partie sauvegardée)", vec3(0.7f, 0.9f, 1.f), "i_night");
            });
        }
    } else if (d.hasForge && distXZ(p.pos, d.forgePos) < 2.2f) {
        dungeonPrompt = "Travailler à la forge";
        if (input.interact) { craftStation = ST_FORGE; menu = Menu::Crafting; selRecipe = -1; play("forge", 0.6f); }
    }
    return true;
}

}  // namespace lk
