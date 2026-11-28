// LOST KINGDOM 2.3.6 — random world events: the open world now and then offers something to do on the way.
//   0 Embuscade : a band of the region's creatures has cornered a traveller's cart; beat them all for his chest.
//   1 Chute d'étoile : a falling star crashes nearby; the crater holds star ore and crystals.
//   2 Cache oubliée : an abandoned stash (a chest) spotted off the path.
// One event at a time, every 4 to 7 minutes outside dungeons, villages and fights; a marker shows where it is.
#include "Game.h"
#include <algorithm>

namespace lk {

static const char* EVENT_NAME[] = {"Embuscade", "Chute d'étoile", "Cache oubliée"};

bool Game::startWorldEvent(int kind) {
    if (player == NullEntity || dungeon.active) return false;
    Character& p = pc();
    Rng r((uint32_t)(gameTime * 97.0) + 17u * (uint32_t)day);
    // a flat, dry spot 30-50 m away, ahead of the hero when possible
    vec3 spot; bool ok = false;
    for (int t = 0; t < 24 && !ok; t++) {
        float a = p.yaw + r.range(-1.2f, 1.2f) + (t > 12 ? PI : 0.f), dd = r.range(30.f, 50.f);
        vec2 q = p.pos.xz() + vec2(std::sin(a), std::cos(a)) * dd;
        if (q.x < 40 || q.y < 40 || q.x > WORLD_SIZE - 40 || q.y > WORLD_SIZE - 40) continue;
        float gy = world.groundHeight(q.x, q.y), wl = world.waterLevel(q.x, q.y);
        if (wl > -1e8f && gy < wl + 0.4f) continue;
        if (world.normal(q.x, q.y).y < 0.88f) continue;
        { vec3 test(q.x, gy, q.y), before = test; world.collide(test, 3.f, gy); if (length(test.xz() - before.xz()) > 0.2f) continue; }
        int pi = world.nearestPoi(q, 70.f);
        if (pi >= 0 && (world.pois[pi].type == PoiType::Village || world.pois[pi].type == PoiType::Monastery)) continue;
        spot = vec3(q.x, gy, q.y); ok = true;
    }
    if (!ok) return false;
    worldEvent = WorldEvent();
    worldEvent.kind = kind; worldEvent.pos = spot; worldEvent.t = 0; worldEvent.active = true;
    uint32_t uid = 0xE0000000u ^ (uint32_t)(gameTime * 13.0);
    worldEvent.chestUid = uid;
    if (kind == 0) {
        std::vector<int> cands;
        int region = world.regionId(spot.x, spot.z);
        const auto& E = enemies();
        for (int i = 0; i < (int)E.size(); i++) if (E[i].region == region && E[i].behavior != Behavior::Boss && !E[i].hunter && !E[i].night) cands.push_back(i);
        if (cands.empty()) return false;
        int def = cands[r.irange(0, (int)cands.size() - 1)];
        int n = 3 + (level >= 8 ? 1 : 0);
        for (int i = 0; i < n; i++) {
            vec2 o = vec2(std::sin(i * TAU / n), std::cos(i * TAU / n)) * 4.5f;
            vec3 sp(spot.x + o.x, 0, spot.z + o.y); sp.y = groundAt(sp.x, sp.z);
            worldEvent.foes.push_back(spawnEnemy(def, sp, i == 0 && r.chance(0.35f), 0));
        }
        notify("Événement : une embuscade ! Des voyageurs sont attaqués non loin.", vec3(1.f, 0.6f, 0.4f), "i_boss");
    } else if (kind == 1) {
        worldEvent.fallT = 2.2f;   // the star is still falling: the crater appears on impact
        notify("Événement : une étoile filante tombe tout près !", vec3(0.7f, 0.85f, 1.f), "i_star");
        play("magic_burst", 0.7f, 0.6f);
    } else {
        NodeSpawn c; c.type = NodeType::Chest; c.pos = spot; c.rot = r.range(0, TAU); c.uid = uid; c.tier = level >= 10 ? 2 : 1;
        spawnNode(c);
        notify("Événement : une cache oubliée a été repérée.", vec3(1.f, 0.85f, 0.5f), "i_chest");
    }
    play("discover", 0.5f);
    return true;
}

void Game::updateWorldEvents(float dt) {
    if (player == NullEntity || cine.active) return;
    WorldEvent& ev = worldEvent;
    if (!ev.active) {
        if (dungeon.active || bossEnt != NullEntity || driving >= 0 || !gameOpts.events) return;
        eventTimer -= dt;
        if (eventTimer > 0) return;
        eventTimer = 240.f + (float)((uint32_t)(gameTime * 31.0) % 180u);   // 4 to 7 minutes
        int pi = world.nearestPoi(pc().pos.xz(), 80.f);
        if (pi >= 0 && world.pois[pi].type == PoiType::Village) return;
        int kind = (int)((uint32_t)(gameTime * 7.0) % 3u);
        if (!startWorldEvent(kind)) eventTimer = 30.f;
        return;
    }
    ev.t += dt;
    Character& p = pc();
    float d = distXZ(p.pos, ev.pos);
    if (ev.kind == 1 && ev.fallT > 0) {
        // the falling star: a streak of light over the last seconds, then the impact
        ev.fallT -= dt;
        vec3 sky = ev.pos + vec3(-30.f, 60.f, -20.f) * std::max(0.f, ev.fallT / 2.2f);
        particles.magicTrail(sky, vec3(0.7f, 0.85f, 1.f), 1.2f);
        if (ev.fallT <= 0) {
            particles.shockwave(ev.pos + vec3(0, 0.2f, 0), vec3(0.6f, 0.8f, 1.f), 7.f);
            world.dig(ev.pos.xz(), 5.f, 2.8f, true);   // 2.4.0 a real crater
            ev.pos.y = groundAt(ev.pos.x, ev.pos.z);
            for (int k = 0; k < 6; k++) particles.dust(ev.pos + vec3(particles.rng.range(-2.f, 2.f), 0, particles.rng.range(-2.f, 2.f)), 2.f, vec3(0.5f, 0.45f, 0.4f));
            play3D("explosion", ev.pos, 1.f, 0.8f, 120.f);
            addShake(d < 60.f ? 0.4f : 0.15f);
            static const NodeType kinds[] = {NodeType::RareOre, NodeType::RareOre, NodeType::Crystal};
            for (int k = 0; k < 3; k++) {
                NodeSpawn n; n.type = kinds[k]; n.rot = k * 2.1f; n.uid = ev.chestUid + 1 + k; n.tier = 2;
                vec2 o = vec2(std::sin(k * 2.1f), std::cos(k * 2.1f)) * 1.6f;
                n.pos = vec3(ev.pos.x + o.x, groundAt(ev.pos.x + o.x, ev.pos.z + o.y), ev.pos.z + o.y);
                spawnNode(n);
            }
        }
    }
    bool done = false;
    if (ev.kind == 0) {
        bool alive = false;
        for (Entity e : ev.foes) if (reg.valid(e)) { Character* c = reg.get<Character>(e); if (c && !c->dead) alive = true; }
        if (!alive) {
            NodeSpawn c; c.type = NodeType::Chest; c.pos = ev.pos; c.rot = 0.3f; c.uid = ev.chestUid; c.tier = level >= 10 ? 2 : 1;
            spawnNode(c);
            int g = 40 + level * 12; gold += g; gainXp(40 + level * 8);
            notify("Embuscade repoussée ! Le voyageur vous laisse son coffre (+" + std::to_string(g) + " or).", vec3(0.7f, 1.f, 0.6f), "i_chest");
            play("quest", 0.6f);
            eventsDone++;
            done = true;
        }
    } else if (d < 5.f && (ev.kind != 1 || ev.fallT <= 0)) {
        gainXp(25 + level * 5);
        notify(ev.kind == 1 ? "Le cratère de l'étoile : minerai d'étoile et cristaux à récolter." : "Vous avez trouvé la cache !", vec3(0.8f, 0.9f, 1.f), ev.kind == 1 ? "i_crystal" : "i_chest");
        eventsDone++;
        done = true;
    }
    if (!done && ev.t > 420.f && d > 80.f) {   // expired, far away: the creatures go home
        for (Entity e : ev.foes) if (reg.valid(e)) reg.destroyLater(e);
        done = true;
    }
    if (done) { ev.active = false; ev.foes.clear(); }
}

void Game::drawWorldEventHud() {
    if (!worldEvent.active || player == NullEntity || dungeon.active || cine.active || menu != Menu::None) return;
    UI& u = ui;
    const WorldEvent& ev = worldEvent;
    Character& p = pc();
    float d = distXZ(p.pos, ev.pos);
    vec3 wp = ev.pos + vec3(0, 3.f, 0);
    bool on; vec2 sp = worldToScreen(wp, &on);
    float m = 70.f;
    bool inside = on && sp.x > m && sp.x < u.W - m && sp.y > 130 && sp.y < u.H - m;
    if (!inside) {
        vec2 dir = normalize(minimapProject(ev.pos.xz() - p.pos.xz(), camYaw) + vec2(1e-4f));
        vec2 cc{u.W / 2, u.H / 2};
        float tx = dir.x != 0 ? (u.W / 2 - m) / std::fabs(dir.x) : 1e9f, ty = dir.y != 0 ? (u.H / 2 - m) / std::fabs(dir.y) : 1e9f;
        sp = cc + dir * std::min(tx, ty);
    }
    vec4 col = ev.kind == 0 ? vec4(1.f, 0.45f, 0.3f, 1.f) : (ev.kind == 1 ? vec4(0.6f, 0.8f, 1.f, 1.f) : vec4(1.f, 0.85f, 0.4f, 1.f));
    float pulse = 1.f + 0.12f * std::sin(realTime * 5.f);
    u.circle(sp, 20.f * pulse, vec4(0.05f, 0.04f, 0.03f, 0.85f), 20);
    u.sprite(ev.kind == 0 ? "i_boss" : (ev.kind == 1 ? "i_star" : "i_chest"), {sp.x - 15, sp.y - 15, 30, 30}, col);
    char b[48]; snprintf(b, sizeof b, "%s · %.0f m", EVENT_NAME[ev.kind], d);
    u.text(b, sp.x, sp.y + 22, 16, vec4(col.xyz(), 1.f), F_BOLD, A_CENTER, 0, true);
}

}  // namespace lk
