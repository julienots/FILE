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
    default: return {hexColor(0x241a2a), hexColor(0x2e2236), hexColor(0x1a1220), hexColor(0x3a2a48), hexColor(0xb040ff), false, true, false, false, false}; // abyss
    }
}

bool Game::dungeonFloor(float x, float z, float& y) const {
    if (!dungeon.active || x < WORLD_SIZE) return false;
    int tx = (int)std::floor((x - dungeon.origin.x) / TILE), tz = (int)std::floor((z - dungeon.origin.z) / TILE);
    if (tx < 0 || tz < 0 || tx >= dungeon.W || tz >= dungeon.H) { y = dungeon.origin.y; return true; }
    TileType t = dungeon.tiles[tz * dungeon.W + tx];
    y = (t == TileType::Pit) ? dungeon.origin.y - 30.f : dungeon.origin.y;
    return true;
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
                // torches on walls facing a room
                for (int k = 0; k < 4; k++) {
                    int dx = k == 0 ? 1 : k == 1 ? -1 : 0, dz = k == 2 ? 1 : k == 3 ? -1 : 0;
                    if (T(x + dx, z + dz) == TileType::Floor && r.chance(0.09f)) {
                        vec3 tp = c + vec3(dx, 0, dz) * (TILE * 0.55f) + vec3(0, 2.6f, 0);
                        wl.box(tp - vec3(0, 0.25f, 0), {0.06f, 0.3f, 0.06f}, Mat(hexColor(0x3a2a1a)));
                        wl.ellipsoid(tp + vec3(0, 0.12f, 0), vec3(0.12f, 0.18f, 0.12f), Mat(hexColor(0xffa040), 0.5f, 0, 4.f), 6, 4);
                        d.lights.push_back(vec4(tp + vec3(dx, 0.3f, dz) * 0.5f, LI_FIRE));
                        d.emitters.push_back(vec4(tp + vec3(0, 0.2f, 0), EM_FIRE));
                    }
                }
            }
        }
    // pillars & accent glows in rooms, cobwebs, lava cracks
    for (size_t i = 0; i < d.rooms.size(); i++) {
        auto& rm = d.rooms[i];
        if (rm.w >= 4 && rm.h >= 4) for (int k = 0; k < 4; k++) {
            vec3 p = d.origin + vec3((rm.x + (k % 2 ? rm.w - 1 : 1)) * TILE, 0, (rm.z + (k / 2 ? rm.h - 1 : 1)) * TILE);
            wl.lathe({{0.6f, 0}, {0.5f, 0.4f}, {0.45f, 4.f}, {0.6f, 4.3f}, {0.6f, 4.6f}, {0.01f, 4.6f}}, Mat(th.trim, 0.8f), 10, p);
            Collider col; col.c = p.xz(); col.r = 0.6f; col.base = -1; col.top = 8; d.colliders.push_back(col);
        }
        vec3 rc = d.origin + vec3((rm.x + rm.w * 0.5f) * TILE, 0, (rm.z + rm.h * 0.5f) * TILE);
        if (rm.kind == 2 || i == 0) { pp.lathe({{2.6f, 0.02f}, {0.01f, 0.03f}}, Mat(th.accent, 0.3f, 0, 2.f), 24, rc); d.lights.push_back(vec4(rc + vec3(0, 3, 0), LI_MAGIC)); d.emitters.push_back(vec4(rc, EM_SPARKLES)); }
    }
    if (th.lava) for (int i = 0; i < 12; i++) { auto& rm = d.rooms[r.irange(0, (int)d.rooms.size() - 1)]; vec3 p = d.origin + vec3((rm.x + r.f() * rm.w) * TILE, 0.01f, (rm.z + r.f() * rm.h) * TILE); pp.box(p, {r.range(0.8f, 2.f), 0.02f, 0.06f}, Mat(hexColor(0xff6a1a), 0.5f, 0, 5.f), quat::axisAngle({0, 1, 0}, r.range(0, PI))); }
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
        dungeon.active = true;
        dungeon.returnPos = back;
        Character& c = pc();
        c.pos = dungeon.entrance; c.vel = vec3(0); c.yaw = 0;
        camFocus = c.pos;
        respawnPos = back;
        // populate rooms (except entrance and puzzle room)
        Rng r(D.seed + day);
        for (size_t i = 1; i < dungeon.rooms.size(); i++) {
            auto& rm = dungeon.rooms[i];
            if (rm.kind == 2 || rm.kind == 3) continue;
            int n = r.irange(2, 4);
            for (int k = 0; k < n; k++) {
                vec3 sp = dungeon.origin + vec3((rm.x + r.range(0.5f, rm.w - 0.5f)) * TILE, 0, (rm.z + r.range(0.5f, rm.h - 0.5f)) * TILE);
                int ei = enemyIndex(D.enemies[r.irange(0, (int)D.enemies.size() - 1)]);
                if (ei >= 0) { Entity e = spawnEnemy(ei, sp, r.chance(0.1f), 0); reg.get<AIBrain>(e)->home = sp; }
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
        dungeon.floor.destroy(); dungeon.walls.destroy(); dungeon.props.destroy();
        dungeon.tiles.clear(); dungeon.def = -1;
        play("door");
    });
}

void Game::updateDungeon(float dt) {
    dungeonPrompt.clear();
    nearDungeon = -1;
    if (player == NullEntity) return;
    Character& p = pc();
    // ---- overworld: entrances
    if (!dungeon.active) {
        for (int i = 0; i < (int)dungeons().size(); i++) {
            const Poi* poi = world.poiById(dungeons()[i].poi);
            if (!poi) continue;
            float d = length(poi->pos - p.pos.xz());
            if (d < 9.f) {
                bool locked = dungeons()[i].id == "abyss" && !questActive("m7") && !questsDone.count("m7");
                nearDungeon = i;
                dungeonPrompt = locked ? "Un sceau ancien bloque le passage" : "Entrer : " + dungeons()[i].name;
                if (input.interact && !locked && transitionT < 0) enterDungeon(i);
                if (input.interact && locked) notify("Le sceau de Morneval est encore intact. (Quête principale)", vec3(0.8f, 0.6f, 1.f), "i_lock");
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
