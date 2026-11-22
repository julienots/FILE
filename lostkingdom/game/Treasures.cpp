// LOST KINGDOM — 3.2.0 treasures of the world (World side): the sky archipelagos of every region, the totems of
// power, the ancient orbs and the sky chests. Everything is planned once, deterministically, for the whole map,
// so the game can count "12 / 34 totems" and a compass can point at the nearest one.
//
// A sky archipelago is three floating islands climbing in steps of ~9 m: a ground spring throws you onto the first,
// a wind spring on each deck throws you to the next, and the totem of power waits on the highest (~30 m up).
// An ancient orb floats on the middle island, a sky chest on the lowest.
#include "World.h"
#include "Props.h"
#include "../engine/core/Platform.h"
#include <algorithm>

namespace lk {

void World::planTreasures() {
    specials.clear();
    Rng gr(0x7A3A2Eu);
    auto dist2 = [](vec2 a, vec2 b) { return length(a - b); };
    auto farFromAll = [&](vec2 p, float m) {
        for (const Poi& q : pois) if (dist2(q.pos, p) < q.radius + m) return false;
        for (const Lake& l : lakes) if (dist2(l.c, p) < l.r + m) return false;
        for (const Road& rd : roads) for (size_t i = 0; i + 1 < rd.pts.size(); i++) if (distToSegment(p, rd.pts[i], rd.pts[i + 1]) < rd.width + std::min(m, 12.f)) return false;
        for (const Structure& s : structures) if ((s.kind == S_SKYISLAND || s.kind == S_GEYSER || s.kind == S_SKYGEYSER) && dist2(s.pos, p) < 45.f) return false;
        for (const Special& sp : specials) if (dist2(sp.pos.xz(), p) < 60.f) return false;
        return true;
    };
    auto groundOk = [&](vec2 p, int region) {
        if (p.x < 60 || p.y < 60 || p.x > WORLD_SIZE - 60 || p.y > WORLD_SIZE - 60) return false;
        if (regionId(p.x, p.y) != region) return false;
        return baseHeight(p.x, p.y, regionAt(p.x, p.y)) > 3.f;
    };
    auto bh = [&](vec2 p) { return baseHeight(p.x, p.y, regionAt(p.x, p.y)); };
    auto uidOf = [](int type, int i) { return hash3i(i, type, 4242, 0x51A7u) | 0x80000000u; };
    int nT = 0, nO = 0, nC = 0;
    auto addSpecial = [&](uint8_t type, vec3 pos, int region, bool sky = false) {
        Special s; s.type = type; s.pos = pos; s.region = region; s.reward = 0; s.sky = sky;
        s.uid = uidOf(type, type == 0 ? nT++ : type == 1 ? nO++ : nC++);
        specials.push_back(s);
    };
    // where each region lies: sample the map once
    std::vector<std::vector<vec2>> samples(R_COUNT);
    for (int j = 0; j < 96; j++)
        for (int i = 0; i < 96; i++) {
            vec2 p{(i + 0.5f) * WORLD_SIZE / 96.f, (j + 0.5f) * WORLD_SIZE / 96.f};
            int r = regionId(p.x, p.y);
            if (r >= 0 && r < R_COUNT) samples[r].push_back(p);
        }
    // ---- sky archipelagos (every region but the Lands of Aurora, which have their own islands)
    for (int r = 0; r < R_COUNT; r++) {
        if (r == R_AURORA || samples[r].size() < 12) continue;
        for (int tries = 0; tries < 260; tries++) {
            vec2 p = samples[r][gr.irange(0, (int)samples[r].size() - 1)] + vec2(gr.range(-30.f, 30.f), gr.range(-30.f, 30.f));
            if (!groundOk(p, r) || !farFromAll(p, 40.f)) continue;
            float a = gr.range(0, TAU);
            vec2 d1{std::sin(a), std::cos(a)};
            float a2 = a + gr.range(-1.f, 1.f);
            vec2 d2{std::sin(a2), std::cos(a2)};
            const float step = 2.f * SKY_ISLAND_R + 6.f;   // 24 m between centres: a gap of ~6 m between decks
            vec2 pB = p + d1 * step, pC = pB + d2 * step;
            vec2 gp = p - d1 * (SKY_ISLAND_R + 3.5f);       // the ground spring, below the first island
            if (!groundOk(gp, r) || !groundOk(pB, r) || !groundOk(pC, r) || !farFromAll(pB, 20.f) || !farFromAll(pC, 20.f) || !farFromAll(gp, 10.f)) continue;
            float top = 0;
            for (vec2 q : {p, pB, pC})
                for (int k = 0; k < 9; k++) { vec2 s = q + vec2(std::cos(k * 0.7f), std::sin(k * 0.7f)) * (k ? SKY_ISLAND_R : 0.f); top = std::max(top, bh(s)); }
            float gh = bh(gp);
            float dA = std::max(top + 10.f, gh + 15.f);
            if (dA - gh > 18.f) continue;                   // the ground spring must reach the first deck
            float dB = dA + 9.f, dC = dB + 9.f;
            vec2 pts[3] = {p, pB, pC}; float decks[3] = {dA, dB, dC};
            for (int k = 0; k < 3; k++) {
                Structure si; si.kind = S_SKYISLAND; si.pos = pts[k]; si.rot = gr.range(0, TAU); si.region = r; si.variant = (int)(decks[k] * 100.f);
                structures.push_back(si);
            }
            Structure ge; ge.kind = S_GEYSER; ge.pos = gp; ge.rot = std::atan2(d1.x, d1.y); ge.region = r; ge.variant = (int)((dA - gh) * 100.f);
            structures.push_back(ge);
            for (int k = 0; k < 2; k++) {   // wind springs on the decks of the first two islands, aimed at the next one
                vec2 dir = k == 0 ? d1 : d2;
                vec2 at = pts[k] + dir * (SKY_ISLAND_R * 0.55f);
                float land = length(pts[k + 1] - at) - SKY_ISLAND_R * 0.35f;
                (void)land;
                Structure sg; sg.kind = S_SKYGEYSER; sg.pos = at; sg.rot = std::atan2(dir.x, dir.y); sg.region = r; sg.variant = (int)((decks[k] + 0.4f) * 100.f);
                structures.push_back(sg);
            }
            addSpecial(0, vec3(pC.x - d2.x * 2.f, dC + 0.4f, pC.y - d2.y * 2.f), r, true);   // the totem on the summit
            addSpecial(1, vec3(pB.x + d1.y * 3.f, dB + 1.5f, pB.y - d1.x * 3.f), r, true);   // an orb on the middle island
            addSpecial(2, vec3(p.x - d1.y * 3.5f, dA + 0.4f, p.y + d1.x * 3.5f), r, true);   // a sky chest on the first
            break;
        }
    }
    // ---- totems on the high ground: two per region, on the highest dry spots, far apart
    for (int r = 0; r < R_COUNT; r++) {
        if (samples[r].size() < 8) continue;
        for (int n = 0; n < 2; n++) {
            vec2 best{-1, -1}; float bestH = -1e9f;
            for (int tries = 0; tries < 140; tries++) {
                vec2 p = samples[r][gr.irange(0, (int)samples[r].size() - 1)] + vec2(gr.range(-35.f, 35.f), gr.range(-35.f, 35.f));
                if (!groundOk(p, r) || !farFromAll(p, 35.f)) continue;
                bool farTotem = true;
                for (const Special& sp : specials) if (sp.type == 0 && sp.region == r && dist2(sp.pos.xz(), p) < 320.f) farTotem = false;
                if (!farTotem) continue;
                float h = bh(p);
                if (h > bestH) { bestH = h; best = p; }
            }
            if (best.x >= 0) addSpecial(0, vec3(best.x, bestH, best.y), r);
        }
    }
    // ---- ancient orbs: four per region, in the wild (some by water, some up high, some deep in the forests)
    for (int r = 0; r < R_COUNT; r++) {
        if (samples[r].size() < 8) continue;
        int placed = 0;
        for (int tries = 0; tries < 200 && placed < 4; tries++) {
            vec2 p = samples[r][gr.irange(0, (int)samples[r].size() - 1)] + vec2(gr.range(-40.f, 40.f), gr.range(-40.f, 40.f));
            if (!groundOk(p, r) || !farFromAll(p, 25.f)) continue;
            addSpecial(1, vec3(p.x, bh(p) + 1.1f, p.y), r);
            placed++;
        }
    }
    // rewards: the totems are ranked by distance from Brindlemere, so the first powers are the first found
    const Poi* home = nullptr; for (const Poi& q : pois) if (q.id == "brindlemere") home = &q;
    vec2 h0 = home ? home->pos : vec2(WORLD_SIZE * 0.5f);
    std::vector<int> tot;
    for (int i = 0; i < (int)specials.size(); i++) if (specials[i].type == 0) tot.push_back(i);
    // the totems on the ground first (reachable from the start), then the summits of the archipelagos
    std::sort(tot.begin(), tot.end(), [&](int a, int b) { if (specials[a].sky != specials[b].sky) return !specials[a].sky; return dist2(specials[a].pos.xz(), h0) < dist2(specials[b].pos.xz(), h0); });
    for (int k = 0; k < (int)tot.size(); k++) specials[tot[k]].reward = k;
    AGR_LOG("treasures: %d totems, %d orbs, %d sky chests", nT, nO, nC);
}

// once the terrain is final (roads, pads): the treasures on the ground sit on it
void World::settleTreasures() {
    for (Special& sp : specials) if (!sp.sky) sp.pos.y = height(sp.pos.x, sp.pos.z) + (sp.type == 1 ? 1.1f : 0.f);
}

// the treasures of one chunk, as interactive nodes (+ their glow)
void addChunkTreasures(const World& w, ChunkKey k, ChunkBuild& out) {
    for (const World::Special& sp : w.specials) {
        ChunkKey c = chunkOf(sp.pos.x, sp.pos.z);
        if (c.x != k.x || c.z != k.z) continue;
        NodeType t = sp.type == 0 ? NodeType::Totem : sp.type == 1 ? NodeType::Orb : NodeType::Chest;
        out.nodes.push_back({t, sp.pos, (float)(sp.uid % 628) / 100.f, sp.uid, sp.type == 2 ? 3 : sp.reward});
        if (sp.type == 0) { out.lights.push_back(vec4(sp.pos.x, sp.pos.y + 3.f, sp.pos.z, LI_MAGIC)); out.emitters.push_back(vec4(sp.pos.x, sp.pos.y + 2.5f, sp.pos.z, EM_SPARKLES)); }
        if (sp.type == 1) { out.lights.push_back(vec4(sp.pos.x, sp.pos.y + 0.6f, sp.pos.z, LI_CRYSTAL)); out.emitters.push_back(vec4(sp.pos.x, sp.pos.y + 0.3f, sp.pos.z, EM_SPARKLES)); }
    }
}

}  // namespace lk
