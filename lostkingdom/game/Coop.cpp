// LOST KINGDOM — 1.13.0 two players on the same Wi-Fi network.
//
// One phone hosts, the other joins (found automatically on the local network, or by typing the host's address).
// The host runs the world: the creatures, time of day and weather; the guest plays its own hero, with its own save,
// level, gear and quests. Each side sees the other's hero, and each sees the other's domain.
//   - every 50 ms both send their hero (position, animation, health); the host adds the creatures near the players;
//   - events go through a small reliable channel (numbered, resent until acknowledged): the guest's hits on a
//     creature, the creature's hits on the guest, the creatures killed (the guest then gets its experience, loot and
//     quest progress, bosses and Crown fragments included), the heroes' looks, the domains;
//   - on the guest, the creatures are puppets (no AI, no local spawning); on the host, the creatures also hunt the
//     guest's hero.
// UDP, port 47777. Discovery: the guest broadcasts on the local networks (and to 127.0.0.1, for the desktop test).
#include "Game.h"
#include <algorithm>
#include <sstream>

namespace lk {
using agr::ByteReader;
using agr::ByteWriter;
using agr::NetAddr;

enum : uint8_t { M_DISCOVER = 1, M_HOSTINFO, M_JOIN, M_WELCOME, M_STATE, M_LEAVE, M_FULL };
enum : uint8_t { E_HIT = 1, E_DAMAGE, E_KILL, E_LOOK, E_BASE };
static const uint32_t MAGIC = 0x4C4B3133;   // "LK13"
static const float BASE_RADIUS = 44.f;      // as in Base.cpp

static ByteWriter header(uint8_t type) { ByteWriter w; w.u32(MAGIC); w.u8(type); return w; }

static int16_t clipIndex(const Clip* c) { return c ? (int16_t)(c - &clip((ClipId)0)) : (int16_t)-1; }

// ------------------------------------------------------------------------------------------------ session
bool Game::coopHost() {
    coopLeave(false);
    if (!coop.sock.open(COOP_PORT)) { coop.status = "Impossible d'ouvrir le port 47777 (déjà utilisé ?)."; return false; }
    coop.mode = CoopState::Host;
    coop.status = "En attente d'un autre joueur sur le même Wi-Fi…";
    AGR_LOG("coop: hosting on port %u", coop.sock.localPort());
    return true;
}

bool Game::coopJoin(const NetAddr& host) {
    if (coop.mode != CoopState::Guest) {
        coopLeave(false);
        if (!coop.sock.open(0)) { coop.status = "Réseau indisponible."; return false; }
        coop.mode = CoopState::Guest;
    }
    if (!host.valid()) { coop.status = "Recherche des parties sur le Wi-Fi…"; return true; }
    coop.peer = host; coop.joining = true; coop.joinT = 0;
    coop.status = "Connexion à " + host.str() + "…";
    AGR_LOG("coop: joining %s", host.str().c_str());
    return true;
}

void Game::coopLeave(bool notifyPeer) {
    if (coop.mode == CoopState::Off) return;
    if (notifyPeer && coop.peer.valid()) { ByteWriter w = header(M_LEAVE); coop.sock.send(coop.peer, w.b.data(), (int)w.b.size()); }
    bool wasGuest = coop.mode == CoopState::Guest;
    if (coop.partner != NullEntity && reg.valid(coop.partner)) reg.destroyLater(coop.partner);
    for (auto& kv : coop.puppets) if (reg.valid(kv.second)) reg.destroyLater(kv.second);
    bool hadPeerBase = coop.peerBaseApplied;
    coop.sock.close();
    coop = CoopState();
    if (hadPeerBase) { world.resetRuntime(); applyBaseToWorld(); }
    if (wasGuest) { spawnedChunks.clear(); chunkSpawns.clear(); }   // creatures come back locally
    AGR_LOG("coop: session closed");
}

// ------------------------------------------------------------------------------------------------ per frame
void Game::updateCoop(float dt) {
    if (coop.mode == CoopState::Off) return;
    coopReceive();
    if (coop.mode == CoopState::Guest) {
        // discovery: broadcast every second until joined
        if (!coop.connected && !coop.joining) {
            coop.discoverT -= dt;
            if (coop.discoverT <= 0) {
                coop.discoverT = 1.f;
                ByteWriter w = header(M_DISCOVER);
                for (uint32_t b : agr::broadcastAddrs()) { NetAddr a; a.ip = b; a.port = COOP_PORT; coop.sock.send(a, w.b.data(), (int)w.b.size()); }
                NetAddr lo = NetAddr::make("127.0.0.1", COOP_PORT); coop.sock.send(lo, w.b.data(), (int)w.b.size());
            }
            for (auto it = coop.found.begin(); it != coop.found.end();) it = realTime - it->seen > 4.f ? coop.found.erase(it) : it + 1;
        }
        if (coop.joining && !coop.connected) {
            coop.joinT -= dt;
            if (coop.joinT <= 0) {
                coop.joinT = 0.5f;
                ByteWriter w = header(M_JOIN);
                w.str(hero().name); w.u16((uint16_t)level); w.str(coopLookString());
                coop.sock.send(coop.peer, w.b.data(), (int)w.b.size());
            }
        }
    }
    if (coop.connected) {
        if (realTime - coop.lastRecv > 6.f) {
            notify("Liaison perdue avec " + coop.peerName + ".", vec3(1.f, 0.6f, 0.4f), "i_waypoint");
            bool host = coop.mode == CoopState::Host;
            coopLeave(false);
            if (host) coopHost();   // the host keeps waiting for someone
            return;
        }
        coop.sendT -= dt;
        if (coop.sendT <= 0) { coop.sendT = 0.05f; coopSend(dt); }
        if (coop.peerLookDirty) {
            coop.peerLookDirty = false;
            Entity p = coopEnsurePartner();
            if (Character* c = reg.get<Character>(p)) coopBuildPartnerLook(*c, coop.peerLook);
        }
        if (!coop.peerBaseApplied && coop.peerBaseParts > 0 && coop.peerBaseGot >= coop.peerBaseParts) coopApplyPeerBase();
        // guest: drop creatures the host stopped sending
        if (coop.mode == CoopState::Guest)
            for (auto it = coop.puppets.begin(); it != coop.puppets.end();) {
                NetPuppet* np = reg.valid(it->second) ? reg.get<NetPuppet>(it->second) : nullptr;
                if (!np || realTime - np->lastUpdate > 3.f) { if (reg.valid(it->second)) reg.destroyLater(it->second); it = coop.puppets.erase(it); }
                else ++it;
            }
    }
}

std::string Game::coopLookString() const {
    std::ostringstream o;
    o << activeHero << ";" << dye;
    for (int i = 0; i < SL_COUNT; i++) o << ";" << equipment[i].id;
    return o.str();
}

void Game::coopWriteHero(ByteWriter& w) {
    Character& c = pc();
    int16_t cl = clipIndex(c.vis.anim.action());
    if (cl != coop.myLastClip) { coop.myLastClip = cl; if (cl >= 0) coop.myClipSeq++; }
    vec3 p = c.pos;
    if (driving >= 0 && driving < (int)vehicles.size()) p = vehicles[driving].pos;
    w.f32(p.x); w.f32(p.y); w.f32(p.z); w.f32(c.yaw); w.f32(c.vis.anim.speed);
    w.f32(c.hp); w.f32(c.maxHp); w.f32(c.def);
    w.u8((uint8_t)c.vis.anim.state); w.u8((uint8_t)activeHero);
    w.u8((c.dead ? 1 : 0) | (c.vis.anim.blocking ? 2 : 0) | (!c.grounded ? 4 : 0));
    w.i16(cl); w.i16((int16_t)c.vis.anim.stance); w.u8(coop.myClipSeq);
}

static CoopHeroState readHero(ByteReader& r) {
    CoopHeroState s;
    s.pos.x = r.f32(); s.pos.y = r.f32(); s.pos.z = r.f32(); s.yaw = r.f32(); s.speed = r.f32();
    s.hp = r.f32(); s.maxHp = r.f32(); s.def = r.f32();
    s.body = r.u8(); s.hero = r.u8(); s.flags = r.u8();
    s.clip = r.i16(); s.stance = r.i16(); s.clipSeq = r.u8();
    return s;
}

void Game::coopSend(float) {
    ByteWriter w = header(M_STATE);
    w.u32(coop.inSeq);           // ack: every event up to here was received
    coopWriteHero(w);
    if (coop.mode == CoopState::Host) {
        w.f32(timeOfDay); w.u8((uint8_t)weather); w.u16((uint16_t)day);
        // creatures near either player (alive, or dying), a slice per packet
        std::vector<Entity> list;
        vec3 a = pc().pos, b = (reg.valid(coop.partner) ? reg.get<Character>(coop.partner)->pos : a);
        reg.each<Character>([&](Entity e, Character& c) {
            if (c.team != Team::Enemy || reg.get<NetPuppet>(e)) return;
            if (dist2XZ(c.pos, a) < 75.f * 75.f || dist2XZ(c.pos, b) < 75.f * 75.f || c.bossDef >= 0) list.push_back(e);
        });
        const int per = 16;
        int n = std::min(per, (int)list.size());
        if (coop.enemyCursor >= (int)list.size()) coop.enemyCursor = 0;
        w.u8((uint8_t)n);
        for (int k = 0; k < n; k++) {
            Entity e = list[(coop.enemyCursor + k) % list.size()];
            Character& c = *reg.get<Character>(e);
            w.u32((uint32_t)e); w.i16((int16_t)c.enemyDef); w.i16((int16_t)c.bossDef); w.u8(c.elite ? 1 : 0);
            w.f32(c.pos.x); w.f32(c.pos.y); w.f32(c.pos.z); w.f32(c.yaw); w.f32(c.vis.anim.speed);
            w.f32(c.hp); w.f32(c.maxHp);
            int16_t cl = clipIndex(c.vis.anim.action());
            w.i16(cl); w.u8((uint8_t)(c.vis.anim.actionTime() < 0.15f ? (e * 7 + (int)(realTime * 2)) & 255 : 0));
            w.u8((uint8_t)c.vis.anim.state); w.u8(c.dead ? 1 : 0);
        }
        coop.enemyCursor += n;
    }
    // unacknowledged events (the oldest first), as many as fit
    int room = 1150 - (int)w.b.size();
    std::vector<const CoopEvent*> evs;
    for (const CoopEvent& e : coop.outbox) { int need = 7 + (int)e.data.size(); if (need > room) break; room -= need; evs.push_back(&e); }
    w.u8((uint8_t)evs.size());
    for (const CoopEvent* e : evs) { w.u32(e->seq); w.u8(e->type); w.u16((uint16_t)e->data.size()); w.b.insert(w.b.end(), e->data.begin(), e->data.end()); }
    coop.sock.send(coop.peer, w.b.data(), (int)w.b.size());
}

void Game::coopEvent(uint8_t type, const ByteWriter& w) {
    if (!coop.connected) return;
    CoopEvent e; e.seq = ++coop.outSeq; e.type = type; e.data = w.b;
    coop.outbox.push_back(e);
    if (coop.outbox.size() > 400) coop.outbox.pop_front();   // the peer is gone or very late
}

void Game::coopReceive() {
    uint8_t buf[2048];
    NetAddr from;
    for (int guard = 0; guard < 64; guard++) {
        int n = coop.sock.recv(buf, sizeof buf, from);
        if (n < 5) break;
        coopHandle(buf, n, from);
    }
}

void Game::coopHandle(const uint8_t* d, int n, const NetAddr& from) {
    ByteReader r(d, n);
    if (r.u32() != MAGIC) return;
    uint8_t type = r.u8();
    if (coop.mode == CoopState::Host) {
        if (type == M_DISCOVER) {
            ByteWriter w = header(M_HOSTINFO);
            w.str(hero().name); w.u16((uint16_t)level); w.u8(coop.connected ? 1 : 0);
            coop.sock.send(from, w.b.data(), (int)w.b.size());
            return;
        }
        if (type == M_JOIN) {
            if (coop.connected && from != coop.peer) { ByteWriter w = header(M_FULL); coop.sock.send(from, w.b.data(), (int)w.b.size()); return; }
            std::string name = r.str(); int lvl = r.u16(); std::string look = r.str();
            if (r.bad) return;
            bool fresh = !coop.connected;
            coop.peer = from; coop.connected = true; coop.lastRecv = realTime;
            coop.peerName = name.substr(0, 24) + " (invité)"; coop.peerLevel = lvl;
            if (fresh) {
                coop.peerLook = look; coop.peerLookDirty = true;
                notify(coop.peerName + " a rejoint votre monde !", vec3(0.6f, 1.f, 0.7f), "i_waypoint");
                play("quest", 0.7f);
                coop.status = "En jeu avec " + coop.peerName + ".";
                AGR_LOG("coop: %s joined from %s (level %d)", coop.peerName.c_str(), from.str().c_str(), lvl);
                coopSendBase();
            }
            ByteWriter w = header(M_WELCOME);
            w.str(hero().name); w.u16((uint16_t)level); w.str(coopLookString());
            w.f32(timeOfDay); w.u8((uint8_t)weather);
            coop.sock.send(from, w.b.data(), (int)w.b.size());
            return;
        }
    } else if (coop.mode == CoopState::Guest) {
        if (type == M_HOSTINFO) {
            std::string name = r.str(); int lvl = r.u16(); bool busy = r.u8() != 0;
            if (r.bad || busy) return;
            for (auto& f : coop.found) if (f.addr == from) { f.seen = realTime; f.name = name; f.level = lvl; return; }
            // the same game answering on two addresses (the network one and 127.0.0.1 on this computer): keep one
            const uint32_t LOOPBACK = 0x7F000001u;
            for (auto& f : coop.found)
                if (f.addr.port == from.port && f.name == name && (f.addr.ip == LOOPBACK || from.ip == LOOPBACK)) {
                    if (f.addr.ip == LOOPBACK) f.addr = from;   // prefer the network address
                    f.seen = realTime; return;
                }
            coop.found.push_back({from, name, lvl, realTime});
            return;
        }
        if (type == M_FULL) { coop.joining = false; coop.status = "Cette partie a déjà deux joueurs."; return; }
        if (type == M_WELCOME) {
            std::string name = r.str(); int lvl = r.u16(); std::string look = r.str(); float tod = r.f32(); int w8 = r.u8();
            if (r.bad) return;
            if (!coop.connected) {
                coop.connected = true; coop.joining = false; coop.peer = from;
                coop.peerName = name.substr(0, 24) + " (hôte)"; coop.peerLevel = lvl; coop.peerLook = look; coop.peerLookDirty = true;
                timeOfDay = tod; weather = weatherNext = w8; weatherBlend = 1;
                // the host's creatures replace ours
                reg.each<Character>([&](Entity e, Character& c) { if (c.team == Team::Enemy && !reg.get<NetPuppet>(e)) reg.destroyLater(e); });
                spawnedChunks.clear(); chunkSpawns.clear();
                notify("Vous êtes dans le monde de " + coop.peerName + " !", vec3(0.6f, 1.f, 0.7f), "i_waypoint");
                play("quest", 0.7f);
                coop.status = "En jeu avec " + coop.peerName + ".";
                AGR_LOG("coop: joined %s's world", coop.peerName.c_str());
                coopSendBase();
            }
            coop.lastRecv = realTime;
            return;
        }
    }
    if (!coop.connected || from != coop.peer) return;
    if (type == M_LEAVE) {
        notify(coop.peerName + " a quitté la partie.", vec3(1.f, 0.8f, 0.5f), "i_waypoint");
        bool host = coop.mode == CoopState::Host;
        coopLeave(false);
        if (host) coopHost();
        return;
    }
    if (type != M_STATE) return;
    coop.lastRecv = realTime;
    coop.snapshots++;
    uint32_t ack = r.u32();
    while (!coop.outbox.empty() && coop.outbox.front().seq <= ack) coop.outbox.pop_front();
    CoopHeroState hs = readHero(r);
    if (r.bad) return;
    coopApplyPartner(hs);
    if (coop.mode == CoopState::Guest) {
        float tod = r.f32(); int w8 = r.u8(); int dday = r.u16();
        if (std::fabs(wrapAngle((tod - timeOfDay) / 24.f * TAU)) > 0.05f) timeOfDay = tod;   // keep the clocks together
        if (w8 != weatherNext) { weatherNext = w8; weatherBlend = 0; }
        day = std::max(day, dday);
        int n = r.u8();
        for (int k = 0; k < n && !r.bad; k++) {
            uint32_t id = r.u32(); int def = r.i16(), boss = r.i16(); bool elite = r.u8() != 0;
            vec3 p; p.x = r.f32(); p.y = r.f32(); p.z = r.f32(); float yaw = r.f32(), spd = r.f32(), hp = r.f32(), mhp = r.f32();
            int16_t cl = r.i16(); uint8_t seq = r.u8(); uint8_t body = r.u8(); bool dead = r.u8() != 0;
            if (r.bad || def < 0 || def >= (int)enemies().size()) break;
            auto it = coop.puppets.find(id);
            Entity e = it != coop.puppets.end() && reg.valid(it->second) ? it->second : NullEntity;
            if (e == NullEntity) {
                if (dead) continue;
                e = spawnEnemy(def, p, elite, 0);
                reg.remove<AIBrain>(e);
                NetPuppet np; np.netId = id; np.target = p; np.targetYaw = yaw; np.lastUpdate = realTime;
                reg.add<NetPuppet>(e, np);
                Character& c = *reg.get<Character>(e);
                c.yaw = yaw;
                if (boss >= 0 && boss < (int)bosses().size()) { c.bossDef = boss; c.name = bosses()[boss].name; c.vis.rim = 0.5f; c.vis.emissiveBoost = 0.4f; }
                coop.puppets[id] = e;
            }
            Character& c = *reg.get<Character>(e);
            NetPuppet& np = *reg.get<NetPuppet>(e);
            np.target = p; np.targetYaw = yaw; np.speed = spd; np.lastUpdate = realTime; np.body = body;
            if (cl >= 0 && cl < C_COUNT && seq != 0 && seq != np.clipSeq) { np.clipSeq = seq; c.vis.anim.play(&clip((ClipId)cl), 1.f, false, 0.05f); c.attackClip = &clip((ClipId)cl); c.attackIdx = -2; }
            if (!c.dead) { c.maxHp = mhp; c.hp = std::max(1.f, hp); }
            if (c.bossDef >= 0 && !c.dead && distXZ(c.pos, pc().pos) < 45.f) bossEnt = e;
        }
    }
    int ne = r.u8();
    for (int k = 0; k < ne && !r.bad; k++) {
        uint32_t seq = r.u32(); uint8_t t = r.u8(); uint16_t len = r.u16();
        if (!r.need(len)) break;
        ByteReader er(d + r.i, len); r.i += len;
        if (seq != coop.inSeq + 1) continue;   // already handled (or a gap: it comes again, in order)
        coop.inSeq = seq;
        coopHandleEvent(t, er);
    }
}

void Game::coopHandleEvent(uint8_t type, ByteReader& r) {
    switch (type) {
    case E_HIT: {   // host: the guest hit one of our creatures
        Entity e = (Entity)r.u32(); float dmg = r.f32(); Element el = (Element)r.u8(); float dx = r.f32(), dz = r.f32(), poise = r.f32();
        if (r.bad || !reg.valid(e)) return;
        Character* t = reg.get<Character>(e);
        if (!t || t->dead || t->team != Team::Enemy) return;
        coop.recvHits++;
        t->hp -= dmg; t->vis.flash = 0.35f; t->lastHitBy = (float)realTime;
        t->poise -= poise;
        if (t->poise <= 0 && !(t->attackClip && t->bossDef >= 0)) { t->poise = t->maxPoise; t->hitStun = t->bossDef >= 0 ? 1.2f : 0.75f; t->attackClip = nullptr; }
        t->knock += vec3(dx, 0, dz) * (3.f / std::max(0.6f, t->mass));
        if (el != Element::Physical) applyStatus(*t, el, dmg / std::max(1.f, t->maxHp));
        hitFx(t->pos + vec3(0, t->height * 0.55f, 0), vec3(dx, 0, dz), el, 1.f, false);
        char b[32]; snprintf(b, sizeof b, "%d", (int)std::round(dmg));
        floatText(t->pos + vec3(0, t->height * 0.8f, 0), b, vec3(0.6f, 0.9f, 1.f), 20);
        if (AIBrain* ai = reg.get<AIBrain>(e)) if (ai->state != AIState::Intro) { ai->target = coop.partner; if (ai->state == AIState::Idle || ai->state == AIState::Patrol || ai->state == AIState::Return) ai->state = AIState::Chase; }
        if (t->hp <= 0) killCharacter(e, *t, coop.partner);
        break;
    }
    case E_DAMAGE: {   // guest: a creature hit our hero on the host's screen
        float dmg = r.f32(); Element el = (Element)r.u8(); float dx = r.f32(), dz = r.f32();
        if (r.bad || player == NullEntity) return;
        Character& c = pc();
        if (c.dead || c.invuln > 0) return;
        coop.recvDamage++;
        if (c.vis.anim.blocking) { dmg *= stamina > 0 ? 0.2f : 1.f; stamina -= dmg * 3.f; play("block", 0.9f); }
        dmg *= 1.f - std::min(0.6f, resist[(int)el]);
        c.hp -= dmg; c.vis.flash = 0.35f; c.knock += vec3(dx, 0, dz) * 2.5f;
        char b[32]; snprintf(b, sizeof b, "%d", (int)std::round(dmg));
        floatText(c.pos + vec3(0, 2.f, 0), b, vec3(1.f, 0.35f, 0.3f), 22);
        haptic(45, 200); addShake(0.3f); play("player_hurt", 0.7f);
        if (c.hp <= 0) killCharacter(player, c, NullEntity);
        break;
    }
    case E_KILL: {   // guest: a creature died on the host; it dies here too, and we get the reward when close
        uint32_t id = r.u32();
        if (r.bad) return;
        auto it = coop.puppets.find(id);
        if (it == coop.puppets.end() || !reg.valid(it->second)) return;
        Entity e = it->second;
        Character& c = *reg.get<Character>(e);
        if (c.dead) return;
        coop.recvKills++;
        bool near = distXZ(c.pos, pc().pos) < 90.f;
        reg.remove<NetPuppet>(e);   // from now on a plain dying creature (dissolves locally)
        coop.puppets.erase(it);
        c.hp = 0;
        if (near) killCharacter(e, c, player);
        else { c.dead = true; c.vis.anim.setState(BodyState::Dead, 0.2f); }
        break;
    }
    case E_LOOK: { coop.peerLook = r.str(); if (!r.bad) coop.peerLookDirty = true; break; }
    case E_BASE: {
        int part = r.u8(), total = r.u8(); std::string text = r.str();
        if (r.bad || total == 0) return;
        if ((int)coop.peerBaseChunks.size() != total) { coop.peerBaseChunks.assign(total, ""); coop.peerBaseGot = 0; coop.peerBaseApplied = false; }
        if (part < total && coop.peerBaseChunks[part].empty()) { coop.peerBaseChunks[part] = text.empty() ? " " : text; coop.peerBaseGot++; }
        coop.peerBaseParts = total;
        break;
    }
    }
}

// ------------------------------------------------------------------------------------------------ the partner
Entity Game::coopEnsurePartner() {
    if (coop.partner != NullEntity && reg.valid(coop.partner)) return coop.partner;
    Entity e = reg.create();
    Character c;
    c.team = coop.mode == CoopState::Host ? Team::Player : Team::Neutral;   // the host's creatures hunt and hit it
    c.name = coop.peerName; c.radius = 0.42f; c.height = 1.8f; c.maxHp = c.hp = 120; c.def = 3;
    c.pos = pc().pos + vec3(2, 0, 2);
    reg.add<Character>(e, c);
    NetPuppet np; np.hero = true; np.target = c.pos; np.lastUpdate = realTime;
    reg.add<NetPuppet>(e, np);
    coop.partner = e;
    coopBuildPartnerLook(*reg.get<Character>(e), coop.peerLook);
    return e;
}

void Game::coopBuildPartnerLook(Character& c, const std::string& look) {
    std::vector<std::string> f; std::string cur;
    for (char ch : look) { if (ch == ';') { f.push_back(cur); cur.clear(); } else cur += ch; }
    f.push_back(cur);
    int heroIdx = f.size() > 0 ? std::max(0, std::min((int)heroes().size() - 1, atoi(f[0].c_str()))) : 0;
    std::string eq[SL_COUNT];
    for (int i = 0; i < SL_COUNT && i + 2 < (int)f.size(); i++) eq[i] = item(f[i + 2]) ? f[i + 2] : "";
    buildHeroLook(c, heroIdx, eq, f.size() > 1 ? f[1] : "");
    c.vis.outlineColor = vec4(0.2f, 0.55f, 0.9f, 1);   // a blue outline: the other player
}

void Game::coopApplyPartner(const CoopHeroState& s) {
    Entity e = coopEnsurePartner();
    Character& c = *reg.get<Character>(e);
    NetPuppet& np = *reg.get<NetPuppet>(e);
    np.target = s.pos; np.targetYaw = s.yaw; np.speed = s.speed; np.lastUpdate = realTime; np.body = s.body;
    if (distXZ(c.pos, s.pos) > 12.f) c.pos = s.pos;   // joined far away, teleported, respawned
    c.maxHp = std::max(1.f, s.maxHp); c.hp = s.hp; c.def = s.def; c.name = coop.peerName;
    c.vis.anim.stance = s.stance;
    c.vis.anim.blocking = (s.flags & 2) != 0;
    if (s.clip >= 0 && s.clip < C_COUNT && s.clipSeq != np.clipSeq) { np.clipSeq = s.clipSeq; c.vis.anim.play(&clip((ClipId)s.clip), 1.f, false, 0.05f); }
    bool dead = (s.flags & 1) != 0;
    if (dead && !c.dead) { c.dead = true; c.vis.anim.setState(BodyState::Dead, 0.2f); }
    if (!dead && c.dead) { c.dead = false; c.deathT = 0; c.vis.dissolve = 0; c.vis.anim.setState(BodyState::Locomotion, 0.2f); c.pos = s.pos; }
}

void Game::updateNetPuppet(Entity e, Character& c, NetPuppet& np, float dt) {
    // smooth towards the last received position (a little ahead along the motion would add latency hiding; 50 ms
    // updates on a local network make plain smoothing enough)
    float d = distXZ(c.pos, np.target);
    if (d > 12.f) c.pos = np.target;   // teleport, respawn
    else c.pos = lerp(c.pos, np.target, std::min(1.f, dt * 14.f));
    c.yaw = dampAngle(c.yaw, np.targetYaw, 14.f, dt);
    c.vel = vec3(0);
    Animator& a = c.vis.anim;
    a.speed = np.speed;
    a.grounded = true;
    if (!c.dead && np.hero) {
        BodyState want = (BodyState)np.body;
        if (want == BodyState::Dead) want = BodyState::Locomotion;
        if (a.state != want) a.setState(want, 0.2f);
    }
    if (c.attackClip && !a.action()) c.attackClip = nullptr;
    (void)e;
}

bool Game::coopIsRemoteEnemy(Entity e) const {
    if (coop.mode != CoopState::Guest || !coop.connected) return false;
    Registry& r = const_cast<Registry&>(reg);
    Character* c = r.get<Character>(e);
    return r.get<NetPuppet>(e) != nullptr && c && c->team == Team::Enemy;
}

void Game::coopOnLocalHit(Entity dst, float dmg, Element elem, vec3 dir, float poise) {
    NetPuppet* np = reg.get<NetPuppet>(dst);
    if (!np) return;
    ByteWriter w; w.u32(np->netId); w.f32(dmg); w.u8((uint8_t)elem); w.f32(dir.x); w.f32(dir.z); w.f32(poise);
    coopEvent(E_HIT, w);
    coop.sentHits++;
}

void Game::coopOnPartnerDamaged(float dmg, Element elem, vec3 dir) {
    ByteWriter w; w.f32(dmg); w.u8((uint8_t)elem); w.f32(dir.x); w.f32(dir.z);
    coopEvent(E_DAMAGE, w);
    coop.sentDamage++;
}

void Game::coopOnEnemyKilled(Entity e) {
    if (coop.mode != CoopState::Host || !coop.connected) return;
    ByteWriter w; w.u32((uint32_t)e);
    coopEvent(E_KILL, w);
    coop.sentKills++;
}

Entity Game::coopNearestPlayer(vec3 p, float* dist) {
    Entity best = player;
    float bd = player != NullEntity ? distXZ(pc().pos, p) : 1e9f;
    if (player != NullEntity && pc().dead) bd = 1e9f;
    if (coop.mode == CoopState::Host && coop.connected && reg.valid(coop.partner)) {
        Character* q = reg.get<Character>(coop.partner);
        if (q && !q->dead) { float d = distXZ(q->pos, p); if (d < bd) { bd = d; best = coop.partner; } }
    }
    if (dist) *dist = bd;
    return best;
}

// ------------------------------------------------------------------------------------------------ domains
void Game::coopSendBase() {
    std::ostringstream o;
    saveBase(o);
    std::string s = o.str();
    const size_t part = 700;
    int total = std::max(1, (int)((s.size() + part - 1) / part));
    for (int i = 0; i < total; i++) {
        ByteWriter w; w.u8((uint8_t)i); w.u8((uint8_t)total); w.str(s.substr(i * part, part));
        coopEvent(E_BASE, w);
    }
}

void Game::coopApplyPeerBase() {
    coop.peerBaseApplied = true;
    std::string text;
    for (auto& c : coop.peerBaseChunks) text += c == " " ? "" : c;
    PlayerBase pb;
    std::istringstream in(text);
    std::string line;
    while (std::getline(in, line)) { std::istringstream l(line); std::string k; l >> k; loadBaseLine(k, l, &pb); }
    if (!pb.founded) return;
    if (base.founded && length(base.center - pb.center) < BASE_RADIUS * 2.f) return;   // the two domains would overlap
    Poi p; p.id = "coop_base"; p.name = pb.name + " (" + coop.peerName + ")"; p.type = PoiType::Village; p.pos = pb.center; p.radius = BASE_RADIUS;
    int pi = world.addRuntimePoi(p);
    Structure s; s.kind = S_BANNER; s.pos = pb.center; s.rot = 0; s.region = world.pois[pi].region; s.poi = pi;
    world.addRuntimeStructure(s);
    for (const BaseBuilt& b : pb.built) {
        Structure t; t.kind = baseBuildings()[b.def].kind; t.pos = b.pos; t.rot = b.rot; t.variant = b.variant; t.region = s.region; t.poi = pi;
        world.addRuntimeStructure(t);
    }
    notify("Le domaine de " + coop.peerName + " apparaît sur votre carte.", vec3(0.7f, 0.9f, 1.f), "i_village");
    AGR_LOG("coop: %s's domain added (%zu buildings)", coop.peerName.c_str(), pb.built.size());
}

// ------------------------------------------------------------------------------------------------ UI
void Game::drawCoopMenu(Rect c) {
    UI& u = ui;
    const vec4 GOLDT = rgba(0xf4d58a), PALE = rgba(0xf4e7c4), DIM = rgba(0xb0a48c);
    u.text("Jouer à deux", c.x, c.y - 4, 26, GOLDT, F_TITLE);
    u.text("Sur le même réseau Wi-Fi. L'hôte garde son monde (créatures, heure, météo) ; l'invité y entre avec son propre héros, "
           "sa sauvegarde, son niveau et ses quêtes. Chacun voit le héros et le domaine de l'autre. Restez proches : les créatures vivent autour de l'hôte.",
           c.x, c.y + 32, 16, DIM, F_BODY, A_LEFT, c.w * 0.95f);
    float y = c.y + 110;
    if (coop.mode == CoopState::Off) {
        if (u.fancyButton({c.x, y, 300, 52}, "Héberger une partie", true, "button", 20)) { coopHost(); play("ui_confirm", 0.7f); }
        if (u.fancyButton({c.x + 320, y, 300, 52}, "Rejoindre une partie", true, "button", 20)) { coopJoin(NetAddr()); play("ui_confirm", 0.7f); }
        return;
    }
    u.text(coop.status, c.x, y, 19, PALE, F_BOLD, A_LEFT, c.w * 0.9f); y += 36;
    if (coop.mode == CoopState::Host && !coop.connected) {
        std::string ips;
        for (auto& ip : agr::localIPv4()) ips += (ips.empty() ? "" : "  ·  ") + ip;
        u.text("Adresse de ce téléphone : " + (ips.empty() ? std::string("(pas de réseau)") : ips), c.x, y, 17, DIM, F_BODY); y += 30;
    }
    if (coop.mode == CoopState::Guest && !coop.connected) {
        u.text("Parties trouvées sur le Wi-Fi :", c.x, y, 17, DIM, F_BODY); y += 28;
        if (coop.found.empty()) { u.text("(aucune pour l'instant… l'autre joueur doit choisir « Héberger »)", c.x, y, 16, DIM, F_BODY); y += 30; }
        for (auto& f : coop.found) {
            char b[96]; snprintf(b, sizeof b, "%s · niveau %d", f.name.c_str(), f.level);
            if (u.fancyButton({c.x, y, 360, 46}, b, !coop.joining, "button_dark", 17)) coopJoin(f.addr);
            y += 54;
        }
        // manual address (when the broadcast is filtered by the router)
        u.text("Ou l'adresse de l'hôte : " + (coop.manualIp.empty() ? std::string("_") : coop.manualIp), c.x, y + 6, 17, PALE, F_BOLD); y += 40;
        static const char* keys[] = {"1", "2", "3", "4", "5", "6", "7", "8", "9", ".", "0", "<"};
        for (int i = 0; i < 12; i++) {
            Rect k{c.x + (i % 6) * 58.f, y + (i / 6) * 52.f, 52, 46};
            if (u.fancyButton(k, keys[i], true, "button_dark", 18)) {
                if (keys[i][0] == '<') { if (!coop.manualIp.empty()) coop.manualIp.pop_back(); }
                else if (coop.manualIp.size() < 15) coop.manualIp += keys[i];
            }
        }
        NetAddr a = NetAddr::make(coop.manualIp, COOP_PORT);
        if (u.fancyButton({c.x + 360, y, 180, 98}, "Rejoindre", a.valid() && !coop.joining, "button", 19)) coopJoin(a);
        y += 112;
    }
    if (coop.connected) {
        Character* p = reg.valid(coop.partner) ? reg.get<Character>(coop.partner) : nullptr;
        char b[160];
        snprintf(b, sizeof b, "%s · niveau %d · %s", coop.peerName.c_str(), coop.peerLevel, p ? (p->dead ? "à terre" : "en forme") : "…");
        u.text(b, c.x, y, 18, PALE, F_BODY); y += 30;
        if (p) { snprintf(b, sizeof b, "à %.0f m", distXZ(p->pos, pc().pos)); u.text(b, c.x, y, 16, DIM, F_BODY); y += 30; }
    }
    if (u.fancyButton({c.x, y + 10, 260, 50}, coop.mode == CoopState::Host ? "Arrêter d'héberger" : "Quitter la partie", true, "button_dark", 18)) { coopLeave(true); play("ui_close", 0.7f); }
}

void Game::drawCoopHud() {
    if (!coop.connected || !reg.valid(coop.partner)) return;
    UI& u = ui;
    Character& p = *reg.get<Character>(coop.partner);
    // name and health above the partner's head
    bool on = false;
    vec2 s = worldToScreen(p.pos + vec3(0, 2.3f, 0), &on);
    if (on && s.x > 0 && s.x < u.W && s.y > 0 && s.y < u.H) {
        u.text(coop.peerName, s.x, s.y - 26, 16, rgba(0x9fd0ff), F_BOLD, A_CENTER, 0, true);
        float f = saturate(p.hp / std::max(1.f, p.maxHp));
        u.rect({s.x - 40, s.y - 4, 80, 7}, vec4(0, 0, 0, 0.6f));
        u.rect({s.x - 39, s.y - 3, 78 * f, 5}, vec4(0.35f, 0.75f, 1.f, 0.95f));
    }
    // small panel under the hero's bars
    float x = 128, y = 72;
    char b[96]; snprintf(b, sizeof b, "%s · %.0f m", coop.peerName.c_str(), distXZ(p.pos, pc().pos));
    u.text(b, x, y, 15, rgba(0x9fd0ff), F_BOLD);
    float f = saturate(p.hp / std::max(1.f, p.maxHp));
    u.rect({x, y + 20, 150, 6}, vec4(0, 0, 0, 0.55f));
    u.rect({x + 1, y + 21, 148 * f, 4}, vec4(0.35f, 0.75f, 1.f, 0.95f));
}

}  // namespace lk
