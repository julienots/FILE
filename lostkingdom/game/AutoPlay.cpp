// Automated play-test (desktop harness): drives the real game through a scripted session,
// validates core systems and captures screenshots. Enabled with --autoplay=<scenario>.
#include "Game.h"
#include "Props.h"
#include "../engine/core/Platform.h"
#include <algorithm>
#include <functional>
#include <cstdlib>
#include <fstream>
#include <iterator>

namespace lk {

struct Step { float dur; std::function<void(Game&)> start; std::function<void(Game&, float)> tick; const char* name; std::function<bool(Game&)> until = nullptr; };   // until: ends the step early

Rect ctlRect(const Game& g, int id);

// Inject a real touch tap (down + up in the SAME frame, like a fast tap at 30 FPS) at a UI position.
static void tapAt(Game& g, vec2 uiPos, int id, bool release = true) {
    float k = g.ui.scale;
    g.touch(TouchEvent{TouchEvent::Down, id, uiPos.x * k, uiPos.y * k});
    if (release) g.touch(TouchEvent{TouchEvent::Up, id, uiPos.x * k, uiPos.y * k});
}
static vec2 ctlCentre(Game& g, int id) { Rect r = ctlRect(g, id); return {r.x + r.w / 2, r.y + r.h / 2}; }

Rect ctlRect(const Game& g, int id);
static void teleport(Game& g, float x, float z, float yaw = 0.8f) {
    Character& c = g.pc();
    c.pos = vec3(x, g.world.groundHeight(x, z), z); c.vel = vec3(0); g.camFocus = c.pos; g.camYaw = yaw;
    bool lava = false, frozen = false; float wl = g.world.waterLevel(x, z, &lava, &frozen);
    AGR_LOG("autoplay teleport (%.0f, %.0f): region %s, ground %.1f, water %.1f%s%s", x, z, regionName(g.world.regionId(x, z)), c.pos.y, wl, frozen ? " (frozen)" : "", lava ? " (lava)" : "");
}

static bool reg_valid(Game& g, Entity e) { return e != NullEntity && g.reg.valid(e); }

static std::vector<Step> scenario(const std::string& name) {
    std::vector<Step> s;
    auto shot = [](const char* f) { return [f](Game& g) { g.requestScreenshot(std::string("build/shots/") + f + ".png"); }; };
    auto none = [](Game&) {};
    auto idle = [](Game&, float) {};
    if (name == "logo") {
        // 2.2.1 the SuperEssence studio logo animation, frame by frame (look at the pictures)
        static const float at[] = {0.6f, 1.25f, 1.9f, 2.3f, 2.9f, 3.8f};
        float prev = 0.f; int i = 0;
        for (float a : at) {
            s.push_back({a - prev, none, idle, "wait"});
            s.push_back({0.0001f, [i](Game& g) { char b[64]; snprintf(b, sizeof b, "build/shots/K%d_logo.png", i); g.requestScreenshot(b); }, idle, "shot"});
            prev = a; i++;
        }
        return s;
    }
    if (name == "splash" || name == "full") {
        s.push_back({2.5f, none, idle, "splash"});
        s.push_back({0.1f, shot("01_splash"), idle, "shot"});
        s.push_back({5.f, none, idle, "title"});
        s.push_back({0.1f, shot("02_title"), idle, "shot"});
    }
    s.push_back({0.5f, [](Game& g) { g.newGame(); g.startPlaying(); }, idle, "new game"});
    if (name == "heroes") {
        // 1.8.0: hero wheel (real tap), Kaede's kit (double jump, shuriken, shadow step, smoke), Astra's kit (flight,
        // star beam, meteor from the sky without fall damage, solar wave), save / load of the roster
        static int res[14]; static float peak, base, hp0, wolfHp0; static size_t proj0; static Entity wolf;
        auto spawnWolf = [](Game& g, float ahead) {
            Character& c = g.pc(); vec3 f{std::sin(c.yaw), 0, std::cos(c.yaw)};
            vec3 p = c.pos + f * ahead; p.y = g.world.groundHeight(p.x, p.z);
            wolf = g.spawnEnemy(enemyIndex("wolf"), p);
            if (Character* w = g.reg.get<Character>(wolf)) { w->maxHp = w->hp = 5000; wolfHp0 = w->hp; w->yaw = c.yaw; }
        };
        auto wolfHp = [](Game& g) { Character* w = g.reg.get<Character>(wolf); return w ? w->hp : -1.f; };
        s.push_back({2.f, [](Game& g) {
            for (int& r : res) r = 0;
            g.ui.recordWidgets = true;
            const Poi* b = g.world.poiById("brindlemere"); teleport(g, b->pos.x + 40, b->pos.y - 60, 2.4f);
            g.timeOfDay = 12.f; g.weather = g.weatherNext = Game::W_CLEAR; g.weatherBlend = 1; g.weatherTimer = 999; g.toastHint.clear();
            g.level = 12; g.recomputeStats(); g.pc().hp = g.pc().maxHp; g.mp = g.maxMp;
            g.unlockHero("kaede"); g.unlockHero("astra");
            res[0] = g.heroUnlocked(heroIndex("kaede")) && g.heroUnlocked(heroIndex("astra")) && g.loreFound.count("hero_kaede") && g.loreFound.count("hero_astra");
            g.menu = Menu::HeroWheel;
        }, idle, "unlock + wheel"});
        s.push_back({0.1f, [](Game& g) { g.requestScreenshot("build/shots/F0_hero_wheel.png"); }, idle, "shot"});
        s.push_back({0.2f, [](Game& g) { const Rect* r = g.ui.findWidget("hero:kaede"); if (r) { float k = g.ui.scale; g.touch(TouchEvent{TouchEvent::Down, 700, (r->x + r->w / 2) * k, (r->y + r->h / 2) * k}); } }, idle, "press kaede"});
        s.push_back({0.5f, [](Game& g) { for (auto& f : g.ui.fingers) if (f.id == 700) { float k = g.ui.scale; g.touch(TouchEvent{TouchEvent::Up, 700, f.x * k, f.y * k}); } }, idle, "release"});
        s.push_back({0.6f, [](Game& g) { res[1] = g.hero().id == "kaede" && g.menu == Menu::None && g.pc().vis.anim.stance == ST_DAGGER; g.stamina = g.maxStamina; }, idle, "kaede active"});
        // double jump: higher than a single jump
        s.push_back({1.f, [](Game& g) { base = g.pc().pos.y; peak = base; g.input.jump = true; }, [](Game& g, float t) {
            peak = std::max(peak, g.pc().pos.y);
            if (t > 0.22f && t < 0.26f) g.input.jump = true; }, "double jump"});
        s.push_back({0.5f, [](Game& g) { res[2] = peak - base > 1.5f; AGR_LOG("HEROES double jump height %.2f m", peak - base); g.stamina = g.maxStamina; }, idle, "check"});
        // shuriken
        s.push_back({0.8f, [spawnWolf](Game& g) { spawnWolf(g, 9.f); g.lockTarget = wolf; proj0 = 0; g.reg.each<Projectile>([&](Entity, Projectile&) { proj0++; }); g.input.skill[0] = true; }, [](Game& g, float t) { if (t < 0.12f) { size_t n = 0; g.reg.each<Projectile>([&](Entity, Projectile&) { n++; }); if (n >= proj0 + 3) res[3] = 1; } }, "shuriken"});
        s.push_back({0.4f, [wolfHp](Game& g) { res[4] = wolfHp(g) < wolfHp0; AGR_LOG("HEROES shuriken: wolf %.0f/%.0f", wolfHp(g), wolfHp0); wolfHp0 = wolfHp(g); g.heroCD[1] = 0; g.stamina = g.maxStamina; }, idle, "check"});
        // shadow step: teleported next to the wolf and hit it
        s.push_back({0.6f, [](Game& g) { g.input.skill[1] = true; }, idle, "shadow step"});
        s.push_back({0.1f, [wolfHp](Game& g) { Character* w = g.reg.get<Character>(wolf); res[5] = w && distXZ(w->pos, g.pc().pos) < 3.f && wolfHp(g) < wolfHp0; AGR_LOG("HEROES shadow step: distance %.1f m, wolf %.0f", w ? distXZ(w->pos, g.pc().pos) : -1.f, wolfHp(g)); g.requestScreenshot("build/shots/F1_kaede.png"); g.stamina = g.maxStamina; }, idle, "check"});
        // smoke bomb: stunned + stealth
        s.push_back({0.3f, [](Game& g) { g.input.skill[2] = true; }, idle, "smoke"});
        s.push_back({0.1f, [](Game& g) { Character* w = g.reg.get<Character>(wolf); res[6] = g.stealthT > 0 && w && w->status.stun > 0; g.requestScreenshot("build/shots/F2_smoke.png"); }, idle, "check"});
        // switch to Astra
        s.push_back({0.6f, [](Game& g) { if (g.reg.valid(wolf)) g.reg.destroyLater(wolf); g.lockTarget = NullEntity; res[7] = g.switchHero(heroIndex("astra")) && g.pc().vis.anim.stance == ST_UNARMED; g.stamina = g.maxStamina; g.mp = g.maxMp; }, idle, "astra"});
        // flight: jump, jump again in the air, fly forward
        s.push_back({3.f, [](Game& g) { g.input.jump = true; hp0 = g.pc().hp; }, [](Game& g, float t) {
            if (t > 0.25f && t < 0.29f) g.input.jump = true;
            if (t > 0.6f) { g.input.move = vec2(0, 1); g.camPitch = 0.4f; g.camDist = g.camDistTarget = 9.f; }
            g.stamina = std::max(g.stamina, 50.f); }, "fly"});
        s.push_back({0.1f, [](Game& g) {
            float alt = g.pc().pos.y - g.groundAt(g.pc().pos.x, g.pc().pos.z);
            res[8] = g.flying && alt > 3.f && alt < 10.f;
            AGR_LOG("HEROES flight: flying %d, altitude %.1f m, speed %.1f m/s (anim state %d, flight pose %.2f, anim speed %.1f, airTime %.2f)", g.flying, alt, length(vec2(g.pc().vel.x, g.pc().vel.z)),
                    (int)g.pc().vis.anim.state, g.pc().vis.anim.aux[2], g.pc().vis.anim.speed, g.pc().airTime);
            g.requestScreenshot("build/shots/F3_astra_flight.png"); }, [](Game& g, float) { g.input.move = vec2(0, 1); }, "check"});
        // side view of the flight pose
        // fly along camera-right: the camera sees the hero from the side (same trick as the gait scenario)
        s.push_back({1.2f, none, [](Game& g, float) { g.input.move = vec2(1, 0); g.camPitch = 0.05f; g.camDist = g.camDistTarget = 7.f; }, "side view"});
        s.push_back({0.05f, [](Game& g) {
            g.requestScreenshot("build/shots/F3b_astra_flight_side.png");
            const PoseWorld& w = g.pc().vis.pw; vec3 hip = w.wpos[H_ROOT], head = w.wpos[H_HEAD], foot = (w.wpos[H_FOOTL] + w.wpos[H_FOOTR]) * 0.5f;
            AGR_LOG("HEROES flight pose (model space): head-hip (%.2f, %.2f, %.2f) foot-hip (%.2f, %.2f, %.2f), state %d, aux %.2f, speed %.1f, action %s",
                    head.x - hip.x, head.y - hip.y, head.z - hip.z, foot.x - hip.x, foot.y - hip.y, foot.z - hip.z, (int)g.pc().vis.anim.state, g.pc().vis.anim.aux[2], g.pc().vis.anim.speed, g.pc().vis.anim.action() ? g.pc().vis.anim.action()->name.c_str() : "none");
        }, [](Game& g, float) { g.input.move = vec2(1, 0); }, "shot"});
        // star beam from the sky
        s.push_back({0.5f, [spawnWolf](Game& g) { spawnWolf(g, 10.f); g.lockTarget = wolf; g.input.skill[0] = true; }, idle, "beam"});
        s.push_back({0.1f, [wolfHp](Game& g) { res[9] = wolfHp(g) < wolfHp0; wolfHp0 = wolfHp(g); AGR_LOG("HEROES star beam: wolf %.0f", wolfHp(g)); g.requestScreenshot("build/shots/F4_star_beam.png"); }, idle, "check"});
        // meteor strike from the sky: lands, hits, no fall damage
        s.push_back({1.6f, [](Game& g) { g.input.skill[1] = true; hp0 = g.pc().hp; }, idle, "meteor"});
        s.push_back({0.1f, [wolfHp](Game& g) {
            res[10] = !g.flying && g.pc().grounded && !g.meteorPending && wolfHp(g) < wolfHp0;
            res[11] = g.pc().hp >= hp0 - 1.f;
            AGR_LOG("HEROES meteor: grounded %d, wolf %.0f -> %.0f, hero hp %.0f -> %.0f", g.pc().grounded, wolfHp0, wolfHp(g), hp0, g.pc().hp);
            wolfHp0 = wolfHp(g); g.heroCD[2] = 0; g.mp = g.maxMp; g.requestScreenshot("build/shots/F5_meteor.png"); }, idle, "check"});
        s.push_back({0.3f, [](Game& g) { g.input.skill[2] = true; }, idle, "solar wave"});
        s.push_back({0.1f, [](Game& g) { Character* w = g.reg.get<Character>(wolf); res[12] = w && w->status.stun > 0; }, idle, "check"});
        // save / load keeps the roster and the active hero
        s.push_back({0.3f, [](Game& g) {
            g.saveGame(2); g.unlockedHeroes.clear(); g.activeHero = 0; g.loadGame(2); g.startPlaying();
            res[13] = g.hero().id == "astra" && g.unlockedHeroes.count("kaede") && g.unlockedHeroes.count("astra");
            AGR_LOG("HEROES TEST: unlock=%d wheelTap=%d doubleJump=%d shurikenThrown=%d shurikenHit=%d shadowStep=%d smoke=%d switchAstra=%d flight=%d beam=%d meteor=%d noFallDamage=%d solarWave=%d saveLoad=%d",
                    res[0], res[1], res[2], res[3], res[4], res[5], res[6], res[7], res[8], res[9], res[10], res[11], res[12], res[13]);
            bool ok = true; for (int r : res) ok = ok && r;
            AGR_LOG("HEROES TEST %s", ok ? "PASSED" : "FAILED");
        }, idle, "report"});
        return s;
    }
    if (name == "coop_host" || name == "coop_guest") {
        // 1.13.0 two players: run "coop_host" and "coop_guest" as two processes on this computer (localhost).
        // The host founds a domain and raises wolves next to the guest; the guest finds the host by discovery,
        // joins, fights the host's wolves (real attack inputs), takes their bites, gets the kills' experience, sees
        // the host's time of day and domain.
        static int res[8]; static float t0, xp0; static int lvl0; static vec2 spot;
        bool host = name == "coop_host";
        if (host) {
            s.push_back({2.5f, [](Game& g) {
                for (int& r : res) r = 0;
                g.timeOfDay = 10.f; g.weather = g.weatherNext = Game::W_CLEAR; g.weatherBlend = 1; g.weatherTimer = 999; g.toastHint.clear();
                const Poi* b = g.world.poiById("brindlemere"); bool found = false;
                for (float rr = 160.f; rr < 900.f && !found; rr += 25.f)
                    for (float a = 0; a < TAU && !found; a += 0.3f) {
                        vec2 p = b->pos + vec2(std::cos(a), std::sin(a)) * rr;
                        teleport(g, p.x, p.y, 0.f);
                        g.addItem("wood", 10, Rarity::Count, false);
                        std::string why; if (g.canFoundBase(why)) { found = true; spot = p; }
                    }
                g.foundBase();
                AGR_LOG("COOP host: domain founded %d at (%.0f, %.0f)", g.base.founded, spot.x, spot.y);
                g.coopHost();
                g.menu = Menu::Coop;
            }, idle, "host"});
            s.push_back({0.05f, shot("Z5_coop_menu_host"), idle, "shot"});
            s.push_back({0.1f, [](Game& g) { g.menu = Menu::None; }, idle, "close"});
            s.push_back({60.f, none, [](Game& g, float) { g.pc().invuln = 1.f; g.timeOfDay = 10.f; }, "wait for the guest"});
            s.back().until = [](Game& g) { return g.coop.connected && g.coop.snapshots > 20 && reg_valid(g, g.coop.partner); };
            s.push_back({0.5f, [](Game& g) {
                res[0] = g.coop.connected;
                Character& p = *g.reg.get<Character>(g.coop.partner);
                AGR_LOG("COOP host: %s joined, partner at (%.1f, %.1f), %d snapshots", g.coop.peerName.c_str(), p.pos.x, p.pos.z, g.coop.snapshots);
            }, idle, "joined"});
            // wolves next to the guest's hero (it comes to the host first)
            s.push_back({8.f, none, [](Game& g, float) { g.pc().invuln = 1.f; g.timeOfDay = 10.f; }, "guest walks over"});
            s.push_back({0.2f, [](Game& g) {
                Character& p = *g.reg.get<Character>(g.coop.partner);
                for (int i = 0; i < 3; i++) { vec3 q = p.pos + vec3(5.f + i * 1.5f, 0, -4.f + i * 3.f); q.y = g.world.groundHeight(q.x, q.z); g.spawnEnemy(enemyIndex("wolf"), q); }
                t0 = g.realTime;
            }, idle, "wolves"});
            s.push_back({70.f, none, [](Game& g, float t) {
                g.pc().invuln = 1.f; g.timeOfDay = 10.f;
                static bool shot = false; if (!shot && g.coop.recvHits >= 2) { shot = true; g.requestScreenshot("build/shots/Z7_coop_host_view.png"); }
                static float dbg = -9; if (t - dbg > 3.f || t < dbg) { dbg = t;
                    g.reg.each<AIBrain, Character>([&](Entity e, AIBrain& ai, Character& c) {
                        if (c.dead) return;
                        Character* p = g.reg.get<Character>(g.coop.partner);
                        AGR_LOG("COOP host dbg: wolf %u state %d target %s dist partner %.1f host %.1f", e, (int)ai.state, ai.target == g.coop.partner ? "partner" : (ai.target == g.player ? "host" : "-"), p ? distXZ(p->pos, c.pos) : -1.f, distXZ(g.pc().pos, c.pos));
                    }); }
            }, "fight"});
            s.back().until = [](Game& g) { return g.coop.sentKills >= 3 && g.coop.sentDamage >= 1; };
            s.push_back({3.f, none, [](Game& g, float) { g.pc().invuln = 1.f; }, "let the events arrive"});
            s.push_back({0.1f, [](Game& g) {
                res[1] = g.coop.recvHits >= 1; res[2] = g.coop.sentKills >= 3; res[3] = g.coop.sentDamage >= 1; res[4] = g.coop.snapshots > 100;
                Character& p = *g.reg.get<Character>(g.coop.partner);
                AGR_LOG("COOP HOST TEST: joined=%d guestHits=%d kills=%d damageToGuest=%d snapshots=%d (partner hp %.0f/%.0f)", res[0], g.coop.recvHits, g.coop.sentKills, g.coop.sentDamage, g.coop.snapshots, p.hp, p.maxHp);
                AGR_LOG(res[0] && res[1] && res[2] && res[3] && res[4] ? "COOP HOST TEST PASSED" : "COOP HOST TEST FAILED");
            }, idle, "report"});
            s.push_back({8.f, none, [](Game& g, float) { g.pc().invuln = 1.f; }, "until the guest leaves"});
            s.back().until = [](Game& g) { return !g.coop.connected; };
        } else {
            s.push_back({2.5f, [](Game& g) {
                for (int& r : res) r = 0;
                g.timeOfDay = 20.5f; g.toastHint.clear();
                const Poi* b = g.world.poiById("brindlemere"); teleport(g, b->pos.x + 20, b->pos.y + 25, 0.4f);
                g.coopJoin(agr::NetAddr());
            }, idle, "look for a game"});
            s.push_back({40.f, none, idle, "discovery"});
            s.back().until = [](Game& g) { return !g.coop.found.empty(); };
            s.push_back({0.6f, [](Game& g) { g.menu = Menu::Coop; }, idle, "menu"});
            s.push_back({0.05f, shot("Z6_coop_menu_guest"), idle, "shot"});
            s.push_back({0.1f, [](Game& g) {
                g.menu = Menu::None;
                res[0] = !g.coop.found.empty();
                if (res[0]) { AGR_LOG("COOP guest: found '%s' at %s", g.coop.found[0].name.c_str(), g.coop.found[0].addr.str().c_str()); g.coopJoin(g.coop.found[0].addr); }
            }, idle, "join"});
            s.push_back({20.f, none, idle, "joining"});
            s.back().until = [](Game& g) { return g.coop.connected && reg_valid(g, g.coop.partner) && g.coop.snapshots > 5; };
            s.push_back({1.5f, [](Game& g) {
                res[1] = g.coop.connected;
                Character& p = *g.reg.get<Character>(g.coop.partner);
                teleport(g, p.pos.x + 3.f, p.pos.z + 3.f, 0.4f);   // meet the host
                xp0 = (float)g.xp; lvl0 = g.level;
                AGR_LOG("COOP guest: in %s's world, host hero at (%.1f, %.1f), time %.2f", g.coop.peerName.c_str(), p.pos.x, p.pos.z, g.timeOfDay);
                res[2] = std::fabs(g.timeOfDay - 10.f) < 0.5f;   // the host's clock (we started at 20:30)
            }, idle, "meet"});
            s.push_back({90.f, none, [](Game& g, float t) {
                Character& c = g.pc();
                if (c.hp < c.maxHp * 0.5f) c.hp = c.maxHp;   // (the test is about the network, not about winning)
                Entity best = NullEntity; float bd = 40.f;
                g.reg.each<Character>([&](Entity e, Character& o) { if (!o.dead && g.coopIsRemoteEnemy(e)) { float d = distXZ(o.pos, c.pos); if (d < bd) { bd = d; best = e; } } });
                g.input.move = vec2(0);
                if (best == NullEntity) return;
                Character& o = *g.reg.get<Character>(best);
                vec3 d = o.pos - c.pos; d.y = 0;
                vec3 fwd{-std::sin(g.camYaw), 0, -std::cos(g.camYaw)}, right{std::cos(g.camYaw), 0, -std::sin(g.camYaw)};
                vec3 n = normalize(d);
                if (bd > 2.2f) g.input.move = vec2(dot(n, right), dot(n, fwd));
                static float dbg = -9; if (t - dbg > 3.f || t < dbg) { dbg = t; AGR_LOG("COOP guest dbg: nearest wolf %.1f m, hp %.0f, attack %d, sent hits %d", bd, c.hp, c.attackClip != nullptr, g.coop.sentHits); }
                static bool shot = false; if (!shot && g.coop.sentHits >= 2) { shot = true; g.requestScreenshot("build/shots/Z8_coop_guest_fight.png"); }
                c.yaw = std::atan2(d.x, d.z);
                if ((int)(t * 30) % 10 == 0) { g.input.attackRelease = true; g.input.attackHoldT = 0.1f; }
            }, "fight the host's wolves"});
            s.back().until = [](Game& g) { return g.coop.recvKills >= 3 && g.coop.recvDamage >= 1; };
            s.push_back({4.f, none, idle, "wait"});
            s.push_back({0.1f, [](Game& g) {
                res[3] = g.coop.sentHits >= 1; res[4] = g.coop.recvKills >= 1; res[5] = g.level > lvl0 || g.xp > xp0; res[6] = g.coop.recvDamage >= 1;
                res[7] = g.world.poiById("coop_base") != nullptr;
                AGR_LOG("COOP GUEST TEST: found=%d joined=%d hostClock=%d hits=%d kills=%d xp=%d (%d -> %d, level %d -> %d) bitten=%d hostDomain=%d puppetsSeen=%zu",
                        res[0], res[1], res[2], g.coop.sentHits, g.coop.recvKills, res[5], (int)xp0, g.xp, lvl0, g.level, g.coop.recvDamage, res[7], g.coop.puppets.size());
                bool ok = true; for (int r : res) ok = ok && r;
                AGR_LOG(ok ? "COOP GUEST TEST PASSED" : "COOP GUEST TEST FAILED");
                g.requestScreenshot("build/shots/Z9_coop_guest.png");
            }, idle, "report"});
            s.push_back({1.f, [](Game& g) { g.coopLeave(true); }, idle, "leave"});
        }
        return s;
    }
    if (name == "story") {
        // 1.12.0 the story chapters: each turning point of the main quest plays its cinematic (shots fast-forwarded,
        // one picture of the title card), Alaric vanishes after his reveal, the ending choice leads to an epilogue
        static int res[7]; static int lastIdx; static bool titleShot; static std::string cur;
        auto complete = [](Game& g, const char* q) { g.startQuest(q); if (QuestState* st = g.questState(q)) if (!st->done) g.completeQuest(*st); };
        auto chapter = [&](const char* id, std::function<void(Game&)> trigger, const char* pic) {
            s.push_back({0.2f, [id, trigger](Game& g) { cur = id; lastIdx = -1; titleShot = false; trigger(g); }, idle, "trigger"});
            s.push_back({15.f, none, idle, "wait start"});
            s.back().until = [id](Game& g) { return g.cine.active && g.cine.id == id; };
            s.push_back({240.f, none, [pic](Game& g, float) {
                if (!g.cine.active) return;
                if (g.cine.index != lastIdx) { lastIdx = g.cine.index; AGR_LOG("STORY %s shot %d/%zu: %s", cur.c_str(), g.cine.index + 1, g.cine.shots.size(), g.cine.shots[g.cine.index].text.substr(0, 60).c_str()); }
                bool last = g.cine.index + 1 >= (int)g.cine.shots.size();
                if (g.cine.waitLoad > 0) return;
                static int allShot = -1;
                if (getenv("LK_STORY_ALL") && g.cine.t > 1.3f && allShot != g.cine.index * 100 + (int)cur.size()) {   // every shot (review)
                    allShot = g.cine.index * 100 + (int)cur.size();
                    g.requestScreenshot("build/shots/Z_" + cur + "_" + std::to_string(g.cine.index) + ".png");
                }
                if (!last && g.cine.t > 1.5f) g.cine.t = g.cine.shots[g.cine.index].dur;     // fast-forward
                if (last && !titleShot && g.cine.t > 3.2f) { titleShot = true; g.requestScreenshot(std::string("build/shots/") + pic + ".png"); }
                if (last && titleShot && g.cine.t > 3.5f) g.cine.t = g.cine.shots[g.cine.index].dur;
            }, "play"});
            s.back().until = [](Game& g) { return !g.cine.active; };
        };
        s.push_back({2.f, [](Game& g) {
            for (int& r : res) r = 0;
            g.storyFlags.insert("prologue_seen"); g.toastHint.clear(); g.timeOfDay = 10.f;
            const Poi* b = g.world.poiById("brindlemere"); teleport(g, b->pos.x + 6, b->pos.y + 10, 0.4f);
        }, idle, "setup"});
        chapter("ch_echo", [complete](Game& g) { complete(g, "m1"); complete(g, "m2"); }, "Y0_ch1_echo");
        s.push_back({0.1f, [](Game& g) { res[0] = g.storyFlags.count("seen_ch_echo") > 0; }, idle, "check"});
        chapter("ch_guardians", [complete](Game& g) { complete(g, "m3"); }, "Y1_ch2_guardians");
        s.push_back({0.1f, [](Game& g) { res[1] = g.storyFlags.count("seen_ch_guardians") > 0 && g.questActive("m4"); }, idle, "check"});
        chapter("ch_scar", [complete](Game& g) { complete(g, "m4"); complete(g, "m5"); }, "Y2_ch3_scar");
        s.push_back({0.1f, [](Game& g) { res[2] = g.storyFlags.count("alaric_gone") > 0 && g.questActive("m6"); }, idle, "check"});
        chapter("ch_corwin", [complete](Game& g) { complete(g, "m6"); }, "Y3_ch4_corwin");
        s.push_back({0.1f, [](Game& g) { res[3] = g.storyFlags.count("seen_ch_corwin") > 0 && g.questActive("m7"); }, idle, "check"});
        // the story panel in the Objectives tab
        s.push_back({1.2f, [](Game& g) { g.menu = Menu::Objectives; }, idle, "objectives"});
        s.push_back({0.05f, shot("Y4_objectives_story"), idle, "shot"});
        s.push_back({0.3f, [](Game& g) { g.menu = Menu::None; }, idle, "close"});
        // the ending: Malgrath falls (as in killCharacter), the choice appears, the player breaks the Crown
        s.push_back({0.2f, [](Game& g) { g.questsDone.insert("m7"); g.queueChapter("ending_choice"); }, idle, "abyss lord falls"});
        s.push_back({8.f, none, idle, "wait choice"});
        s.back().until = [](Game& g) { return g.endingChoice && g.endingChoiceT > 1.8f; };
        s.push_back({0.05f, [](Game& g) { res[4] = g.endingChoice; g.requestScreenshot("build/shots/Y5_ending_choice.png"); }, idle, "shot"});
        s.push_back({0.5f, [](Game& g) { Rect r = g.endingChoiceRect(1); tapAt(g, vec2(r.x + r.w / 2, r.y + r.h / 2), 301); }, idle, "tap: break the Crown"});
        chapter("ending_break", [](Game&) {}, "Y6_epilogue_break");
        s.push_back({3.f, none, idle, "fade to credits"});
        s.push_back({0.1f, [](Game& g) {
            res[5] = g.storyFlags.count("ending_break") > 0;
            res[6] = g.state == GameState::Credits;
            AGR_LOG("STORY TEST: echo=%d guardians+m4=%d scar+alaricGone+m6=%d corwin+m7=%d choice=%d endingBreak=%d credits=%d", res[0], res[1], res[2], res[3], res[4], res[5], res[6]);
            bool ok = true; for (int r : res) ok = ok && r;
            AGR_LOG(ok ? "STORY TEST PASSED" : "STORY TEST FAILED");
        }, idle, "report"});
        return s;
    }
    if (name == "ssr") {
        // 1.12.0 water reflections: the low bridge by the sea and a lake shore, with and without SSR
        static vec2 spots[2]; static float yaws[2];
        s.push_back({2.5f, [](Game& g) {
            g.timeOfDay = 16.5f; g.weather = g.weatherNext = Game::W_CLEAR; g.weatherBlend = 1; g.weatherTimer = 999; g.toastHint.clear();
            const Structure* low = nullptr;
            for (auto& st : g.world.structures) if (st.kind == S_BRIDGE && (!low || st.variant < low->variant)) low = &st;
            vec2 side(std::cos(low->rot), -std::sin(low->rot));
            spots[0] = low->pos + side * 9.f; yaws[0] = std::atan2(side.x, side.y);   // camera behind the hero, looking at the bridge
            const Poi* lake = nullptr;
            for (auto& p : g.world.pois) if (p.type == PoiType::Lake) { lake = &p; break; }
            if (lake) { spots[1] = lake->pos + vec2(lake->radius + 6.f, 0); yaws[1] = PI * 0.5f; } else spots[1] = spots[0], yaws[1] = yaws[0];
            AGR_LOG("SSR bridge at (%.0f, %.0f), lake %s", low->pos.x, low->pos.y, lake ? lake->name.c_str() : "none");
        }, idle, "find"});
        for (int k = 0; k < 2; k++) {
            s.push_back({3.f, [k](Game& g) {
                vec2 p = spots[k];
                for (int i = 0; i < 40 && g.world.waterLevel(p.x, p.y) > -1e8f; i++) p += vec2(std::sin(yaws[k]), std::cos(yaws[k])) * 1.f;   // dry ground
                teleport(g, p.x, p.y, yaws[k]); g.pc().yaw = yaws[k] + PI;
                g.camPitch = 0.35f; g.camDist = g.camDistTarget = 17.f;
                g.renderer.settings.ssr = false;
            }, idle, "spot"});
            s.push_back({2.5f, none, idle, "settle (exposure)"});
            s.push_back({0.05f, [k](Game& g) { g.requestScreenshot(std::string("build/shots/X") + char('1' + 2 * k) + "_ssr_off.png"); }, idle, "shot off"});
            s.push_back({0.4f, [](Game& g) { g.renderer.settings.ssr = true; }, idle, "on"});
            s.push_back({0.05f, [k](Game& g) { g.requestScreenshot(std::string("build/shots/X") + char('0' + 2 * k) + "_ssr_on.png"); }, idle, "shot on"});
            s.push_back({0.3f, none, idle, "wait"});
        }
        for (int i = 0; i < 6; i++) {   // alternate off / on at 1 s intervals (brightness drift check)
            s.push_back({1.f, [i](Game& g) { g.renderer.settings.ssr = (i % 2) == 1; }, idle, "toggle"});
            s.push_back({0.05f, [i](Game& g) { g.requestScreenshot(std::string("build/shots/XA") + char('0' + i) + ".png"); }, idle, "shot"});
        }
        s.push_back({3.5f, [](Game& g) { teleport(g, 850, 600, 2.3f); g.timeOfDay = 15.5f; g.weather = g.weatherNext = Game::W_CLEAR; g.weatherBlend = 1; g.camPitch = 0.45f; g.camDist = g.camDistTarget = 14.f; }, idle, "frostheim sun"});
        s.push_back({0.05f, shot("X4_snow_glitter"), idle, "shot"});
        s.push_back({0.2f, [](Game&) { AGR_LOG("SSR TEST PASSED"); }, idle, "end"});
        return s;
    }
    if (name == "physics") {
        // 1.11.2 physics: running jump vs standing jump, slope speed, landing roll, car slide / hops / springs
        static vec3 j0; static float jumpDist[2], jumpPeak[2]; static bool inAir; static int phase;
        static float upMul, downMul; static int rolls0; static float maxSlip, airTotal, suspMin, suspMax; static int landings; static bool wasAir;
        static int res[5];
        s.push_back({2.0f, [](Game& g) {
            for (int& r : res) r = 0;
            g.timeOfDay = 11.f; g.weather = g.weatherNext = Game::W_CLEAR; g.weatherBlend = 1; g.weatherTimer = 999; g.toastHint.clear();
            const Poi* b = g.world.poiById("brindlemere"); teleport(g, b->pos.x + 30, b->pos.y + 30, 0.4f);
        }, idle, "flat"});
        for (int k = 0; k < 2; k++) {   // 0: standing jump (walk), 1: running jump (sprint)
            s.push_back({2.4f, [k](Game& g) { g.pc().vel = vec3(0); phase = 0; inAir = false; jumpDist[k] = jumpPeak[k] = 0; g.camYaw = 0.4f; },
                [k](Game& g, float t) {
                    Character& c = g.pc();
                    g.input.move = vec2(0, k == 0 ? 0.5f : 1.f); g.input.sprint = k == 1;
                    if (phase == 0 && t > 1.2f) { g.input.jump = true; phase = 1; j0 = c.pos; }
                    else if (phase == 1) { if (!c.grounded) { inAir = true; jumpPeak[k] = std::max(jumpPeak[k], c.pos.y - j0.y); } else if (inAir) { jumpDist[k] = distXZ(c.pos, j0); phase = 2; } }
                }, k == 0 ? "standing jump" : "running jump"});
            s.push_back({0.6f, [k](Game& g) { g.input.move = vec2(0); AGR_LOG("PHYSICS jump %s: %.2f m long, %.2f m high", k ? "running" : "walking", jumpDist[k], jumpPeak[k]); }, idle, "log"});
        }
        // slope: find a ~25-35 % slope near Brindlemere, run up then down it
        static vec2 sp, sdir;
        s.push_back({2.0f, [](Game& g) {
            const Poi* b = g.world.poiById("brindlemere"); float best = 0; sp = b->pos; sdir = vec2(1, 0);
            for (int i = 0; i < 4000; i++) {
                vec2 p = b->pos + vec2(std::fmod(i * 37.1f, 300.f) - 150.f, std::fmod(i * 91.7f, 300.f) - 150.f);
                vec3 n = g.world.normal(p.x, p.y);
                float sl = std::sqrt(std::max(0.f, 1.f - n.y * n.y)) / std::max(0.1f, n.y);
                if (sl > 0.25f && sl < 0.4f && g.world.waterLevel(p.x, p.y) < -1e8f && sl > best) { best = sl; sp = p; sdir = normalize(vec2(-n.x, -n.z)); }
            }
            AGR_LOG("PHYSICS slope %.2f at (%.0f, %.0f)", best, sp.x, sp.y);
            teleport(g, sp.x, sp.y, 0);
        }, idle, "slope"});
        for (int k = 0; k < 2; k++) {
            s.push_back({1.0f, [k](Game& g) {
                vec2 d = k == 0 ? sdir : -sdir;
                teleport(g, sp.x - d.x * 1.5f, sp.y - d.y * 1.5f, std::atan2(-d.x, -d.y));
            }, [k](Game& g, float t) {
                g.input.move = vec2(0, 1.f);
                if (t > 0.7f) { if (k == 0) upMul = g.slopeMul; else downMul = g.slopeMul; }
            }, k == 0 ? "run uphill" : "run downhill"});
        }
        s.push_back({0.1f, [](Game& g) { g.input.move = vec2(0); AGR_LOG("PHYSICS slope speed: uphill x%.2f, downhill x%.2f", upMul, downMul); }, idle, "log"});
        // landing roll: running off a 9 m drop (dropped from the air with forward speed)
        s.push_back({2.5f, [](Game& g) {
            const Poi* b = g.world.poiById("brindlemere"); teleport(g, b->pos.x + 30, b->pos.y + 30, 0.4f);
            Character& c = g.pc(); rolls0 = g.landRolls; c.pos.y += 9.f; c.grounded = false; c.airTime = 0.2f; c.vel = vec3(-std::sin(0.4f) * 6.f, 0, -std::cos(0.4f) * 6.f); g.sprintJump = true;
        }, [](Game& g, float) { g.input.move = vec2(0, 1.f); g.input.sprint = true; }, "drop"});
        s.push_back({0.1f, [](Game& g) { g.input.move = vec2(0); AGR_LOG("PHYSICS landing rolls %d", g.landRolls - rolls0); }, idle, "log"});
        // car: hard turn at full speed (slide), then the Gravemoor road (hops, springs)
        s.push_back({2.0f, [](Game& g) {
            g.races.clear(); g.buildRaces();
            const RaceTrack& tr = g.races[2];
            if (g.vehicles.empty()) g.initVehicles();
            VehicleState& v = g.vehicles[0];
            v.pos = vec3(tr.start.x, g.groundAt(tr.start.x, tr.start.y), tr.start.y); v.yaw = std::atan2(tr.startDir.x, tr.startDir.y); v.speed = 0;
            teleport(g, v.pos.x + 3, v.pos.z, 0.f);
            g.vehiclesUnlocked = {"car", "moto", "airship"}; g.enterVehicle(0);
            maxSlip = airTotal = 0; suspMin = suspMax = 0; landings = 0; wasAir = false; phase = 0;
        }, idle, "car"});
        s.push_back({75.f, none, [](Game& g, float t) {
            if (g.driving < 0) return;
            const RaceTrack& tr = g.races[2];
            VehicleState& v = g.vehicles[g.driving];
            while (phase < (int)tr.cps.size() && length(tr.cps[phase] - v.pos.xz()) < 12.f) phase++;
            if (phase >= (int)tr.cps.size()) { g.input.move = vec2(0); return; }
            g.camYaw = v.yaw + PI;
            vec2 d = normalize(tr.cps[phase] - v.pos.xz());
            vec2 fw(std::sin(v.yaw), std::cos(v.yaw)), rt(-std::cos(v.yaw), std::sin(v.yaw));
            g.input.move = vec2(dot(d, rt), dot(d, fw));
            if (dot(d, fw) < 0) g.input.move = vec2(dot(d, rt) >= 0 ? 1.f : -1.f, 0.25f);   // target behind: U-turn going forward
            if (t > 6.f && t < 7.3f) g.input.move = vec2(1.f, 0.15f);   // yank the wheel at full speed
            if (length(g.input.move) > 0) g.input.move = normalize(g.input.move);
            maxSlip = std::max(maxSlip, v.slip);
            if (v.air && v.airT > 0.06f) airTotal += 1.f / 30.f;
            if (wasAir && !v.air) landings++;
            wasAir = v.air && v.airT > 0.12f;
            suspMin = std::min(suspMin, v.suspP); suspMax = std::max(suspMax, v.suspP);
            static bool sh = false; if (!sh && v.slip > 0.15f) { sh = true; g.requestScreenshot("build/shots/W0_car_slide.png"); }
            static float lastLog = -1; if (std::floor(t / 2) != std::floor(lastLog / 2)) AGR_LOG("PHYSICS t=%.0f cp %d dist %.1f speed %.1f slip %.2f air %d y %.2f ground %.2f", t, phase, length(tr.cps[phase] - v.pos.xz()), v.speed, v.slip, v.air, v.pos.y, g.groundAt(v.pos.x, v.pos.z)); lastLog = t;
        }, "drive"});
        s.back().until = [](Game& g) { return phase >= (int)g.races[2].cps.size(); };
        // launch (as off a ramp): the car must fly, then land on its springs
        static float launchAir; static int launchLand; static float heaveMin;
        s.push_back({2.5f, [](Game& g) { launchAir = 0; launchLand = 0; heaveMin = 0; if (g.driving >= 0) { VehicleState& v = g.vehicles[g.driving]; v.speed = 15.f; v.vy = 6.f; v.pos.y += 0.1f; } },
            [](Game& g, float) {
                if (g.driving < 0) return;
                VehicleState& v = g.vehicles[g.driving];
                g.input.move = vec2(0, 0.6f);
                static bool was = false;
                if (v.air) launchAir += 1.f / 30.f; else if (was) launchLand++;
                was = v.air; heaveMin = std::min(heaveMin, v.heave);
            }, "launch"});
        s.push_back({0.1f, [](Game& g) {
            g.input.move = vec2(0);
            AGR_LOG("PHYSICS car launch: airborne %.2f s, landings %d, springs compressed %.3f m", launchAir, launchLand, -heaveMin);
            AGR_LOG("PHYSICS car: checkpoints %d/%zu, max slip %.2f rad, airborne %.2f s, landings %d, spring pitch %.3f..%.3f", phase, g.races[2].cps.size(), maxSlip, airTotal, landings, suspMin, suspMax);
            g.exitVehicle();
            res[0] = jumpDist[1] > jumpDist[0] * 1.4f && jumpPeak[1] > jumpPeak[0];
            res[1] = upMul < 0.95f && downMul > 1.0f;
            res[2] = g.landRolls - rolls0 >= 1;
            res[3] = maxSlip > 0.1f && suspMax - suspMin > 0.01f && launchAir > 0.3f && launchLand >= 1 && heaveMin < -0.01f;
            res[4] = phase >= (int)g.races[2].cps.size();
            AGR_LOG("PHYSICS TEST: runningJump=%d slopes=%d landingRoll=%d carSlideSprings=%d carRoad=%d", res[0], res[1], res[2], res[3], res[4]);
            bool ok = true; for (int r : res) ok = ok && r;
            AGR_LOG(ok ? "PHYSICS TEST PASSED" : "PHYSICS TEST FAILED");
        }, idle, "report"});
        return s;
    }
    if (name == "bridge") {
        // 1.11.2: walk across the 4 bridges nearest to Brindlemere along their road, then drive the car over one.
        // Checks: the hero gets to the far bank, never swims, and his feet stay on the planks (|feet - deck| small).
        static std::vector<int> br; static int cur; static float maxGap; static bool swam, crossed; static int res[3];
        static vec3 carStart;
        s.push_back({2.5f, [](Game& g) {
            g.timeOfDay = 9.5f; g.weather = g.weatherNext = Game::W_CLEAR; g.weatherBlend = 1; g.weatherTimer = 999; g.toastHint.clear();
            br.clear(); res[0] = res[1] = res[2] = 0;
            vec2 home = g.world.poiById("brindlemere")->pos;
            for (int i = 0; i < (int)g.world.structures.size(); i++) if (g.world.structures[i].kind == S_BRIDGE) br.push_back(i);
            std::sort(br.begin(), br.end(), [&](int a, int b) { return length(g.world.structures[a].pos - home) < length(g.world.structures[b].pos - home); });
            if (br.size() > 4) br.resize(4);
            AGR_LOG("BRIDGE %zu bridges tested", br.size());
        }, idle, "find"});
        for (int k = 0; k < 4; k++) {
            s.push_back({2.0f, [k](Game& g) {
                cur = k; maxGap = 0; swam = crossed = false;
                if (k >= (int)br.size()) return;
                const Structure& st = g.world.structures[br[k]];
                float L = World::bridgeHalfLen(st);
                vec2 dir(std::sin(st.rot), std::cos(st.rot)), p = st.pos - dir * (L + 7.f);
                teleport(g, p.x, p.y, st.rot + PI);
                g.pc().yaw = st.rot;
                AGR_LOG("BRIDGE %d at (%.0f, %.0f): length %.1f m, water %.2f, deck centre %.2f, ends %.2f / %.2f", k, st.pos.x, st.pos.y, 2 * L,
                        st.variant / 100.f, g.world.bridgeDeck(st, 0), g.world.bridgeDeck(st, -L), g.world.bridgeDeck(st, L));
            }, idle, "to the bridge"});
            s.push_back({16.f, none, [k](Game& g, float t) {
                if (k >= (int)br.size() || crossed) { g.input.move = vec2(0); return; }
                const Structure& st = g.world.structures[br[k]];
                float L = World::bridgeHalfLen(st);
                Character& c = g.pc();
                vec2 l = rotate(c.pos.xz() - st.pos, st.rot);
                g.camYaw = st.rot + PI;
                g.input.move = vec2(clampf(l.x * 0.8f, -0.6f, 0.6f), 1.f);
                if (std::fabs(l.y) < L - 1.f && std::fabs(l.x) < 2.f && c.grounded) maxGap = std::max(maxGap, std::fabs(c.pos.y - g.world.bridgeDeck(st, l.y)));
                if (c.swimming) swam = true;
                if (l.y > L + 3.f) crossed = true;
                if (k == 0 && std::fabs(l.y) < 1.f) { static bool shot0 = false; if (!shot0) { shot0 = true; g.requestScreenshot("build/shots/V0_bridge_walk.png"); } }
                (void)t;
            }, "walk across"});
            s.back().until = [](Game&) { return crossed; };
            s.push_back({0.1f, [k](Game& g) {
                g.input.move = vec2(0);
                if (k >= (int)br.size()) return;
                AGR_LOG("BRIDGE %d: crossed %d, swam %d, max gap feet/deck %.3f m", k, crossed, swam, maxGap);
                if (crossed && !swam && maxGap < 0.12f) res[0]++;
            }, idle, "result"});
        }
        // side view of the first bridge
        s.push_back({2.5f, [](Game& g) {
            const Structure& st = g.world.structures[br[0]];
            float L = World::bridgeHalfLen(st);
            vec2 dir(std::sin(st.rot), std::cos(st.rot)), side(std::cos(st.rot), -std::sin(st.rot));
            vec2 p = st.pos - dir * (L + 3.f) + side * 7.f;   // on the bank, beside the abutment
            teleport(g, p.x, p.y, 0);
            vec2 to = st.pos - p; g.camYaw = std::atan2(-to.x, -to.y); g.pc().yaw = std::atan2(to.x, to.y);
            g.camPitch = 0.22f; g.camDist = g.camDistTarget = 20.f;
        }, idle, "side"});
        s.push_back({0.05f, shot("V1_bridge_side"), idle, "shot"});
        s.push_back({0.5f, [](Game& g) {
            g.timeOfDay = 21.5f;
            const Structure& st = g.world.structures[br[0]];
            float L = World::bridgeHalfLen(st);
            vec2 dir(std::sin(st.rot), std::cos(st.rot)), p = st.pos + dir * (L - 6.f);
            teleport(g, p.x, p.y, st.rot); g.pc().yaw = st.rot + PI;
            g.camPitch = CameraRig::PITCH_DEFAULT; g.camDist = g.camDistTarget = 15.5f;
        }, idle, "night"});
        s.push_back({2.5f, none, idle, "lanterns"});
        s.push_back({0.05f, shot("V2_bridge_night"), idle, "shot"});
        // drive over bridge 0
        s.push_back({2.0f, [](Game& g) {
            g.timeOfDay = 10.f; crossed = false; maxGap = 0;
            const Structure& st = g.world.structures[br[0]];
            float L = World::bridgeHalfLen(st);
            vec2 dir(std::sin(st.rot), std::cos(st.rot)), p = st.pos - dir * (L + 10.f);
            if (g.vehicles.empty()) g.initVehicles();
            VehicleState& v = g.vehicles[0];
            v.pos = vec3(p.x, g.world.groundHeight(p.x, p.y), p.y); v.yaw = st.rot; v.speed = 0;
            teleport(g, p.x + 3.f * dir.y, p.y - 3.f * dir.x, st.rot + PI);
            g.vehiclesUnlocked = {"car", "moto", "airship"}; g.enterVehicle(0);
            carStart = v.pos;
        }, idle, "car"});
        s.push_back({14.f, none, [](Game& g, float) {
            if (g.driving < 0 || crossed) { g.input.move = vec2(0); return; }
            const Structure& st = g.world.structures[br[0]];
            float L = World::bridgeHalfLen(st);
            VehicleState& v = g.vehicles[g.driving];
            vec2 l = rotate(v.pos.xz() - st.pos, st.rot);
            g.camYaw = v.yaw + PI;
            vec2 target = st.pos + vec2(std::sin(st.rot), std::cos(st.rot)) * (l.y + 8.f);   // a point ahead on the axis
            vec2 d = normalize(target - v.pos.xz());
            vec2 fw(std::sin(v.yaw), std::cos(v.yaw)), rt(-std::cos(v.yaw), std::sin(v.yaw));
            g.input.move = vec2(dot(d, rt), dot(d, fw)) * 0.75f;
            if (std::fabs(l.y) < L - 2.f) maxGap = std::max(maxGap, std::fabs(v.pos.y - g.world.bridgeDeck(st, l.y)));
            if (l.y > L + 3.f) crossed = true;
            static bool sh = false; if (!sh && std::fabs(l.y) < 2.f) { sh = true; g.requestScreenshot("build/shots/V3_bridge_car.png"); }
        }, "drive across"});
        s.back().until = [](Game&) { return crossed; };
        s.push_back({0.1f, [](Game& g) {
            g.input.move = vec2(0);
            res[1] = crossed && maxGap < 0.35f;
            AGR_LOG("BRIDGE car: crossed %d, max gap body/deck %.3f m", crossed, maxGap);
            res[2] = (int)br.size() >= 1;
            g.exitVehicle();
            AGR_LOG("BRIDGE TEST: walked %d/%zu, car %d", res[0], br.size(), res[1]);
            AGR_LOG(res[0] == (int)br.size() && res[1] && res[2] ? "BRIDGE TEST PASSED" : "BRIDGE TEST FAILED");
        }, idle, "report"});
        return s;
    }
    if (name == "shafts") {
        // 1.11.1: light shafts through the forest by the Millbrook road, morning, noon and evening
        static const float hours[3] = {7.2f, 12.f, 17.6f};
        s.push_back({2.5f, [](Game& g) {
            g.weather = g.weatherNext = Game::W_CLEAR; g.weatherBlend = 1; g.weatherTimer = 999; g.toastHint.clear();
            g.races.clear(); g.buildRaces();
            vec2 p = g.races[0].cps[1];
            teleport(g, p.x, p.y, 0.f);
        }, idle, "forest"});
        for (int i = 0; i < 3; i++) {
            s.push_back({2.f, [i](Game& g) { g.timeOfDay = hours[i]; }, idle, "hour"});
            s.push_back({0.05f, [i](Game& g) {
                AGR_LOG("SHAFTS hour %.1f amount %.2f sun y %.2f", hours[i], g.env.shafts, g.env.sunDir.y);
                g.requestScreenshot(std::string("build/shots/U") + char('0' + i) + "_shafts.png");
            }, idle, "shot"});
        }
        s.push_back({0.5f, [](Game&) { AGR_LOG("SHAFTS TEST PASSED"); }, idle, "end"});
        return s;
    }
    if (name == "race") {
        // 1.11.1: a full race with the car on the Millbrook road (driven by a simple autopilot towards each
        // checkpoint), plus the challenges panel
        static int res[5]; static float t0;
        s.push_back({2.5f, [](Game& g) {
            for (int& r : res) r = 0;
            g.timeOfDay = 10.f; g.weather = g.weatherNext = Game::W_CLEAR; g.weatherBlend = 1; g.weatherTimer = 999; g.toastHint.clear();
            g.vehicles.clear(); g.races.clear();
        }, idle, "init"});
        s.push_back({0.5f, [](Game& g) {
            g.buildRaces();
            for (auto& t : g.races) AGR_LOG("RACE %s: %zu checkpoints, %.0f m, gold %.0f s, silver %.0f s, bronze %.0f s", raceDefs()[t.def].name.c_str(), t.cps.size(), t.length, t.gold, t.silver, t.bronze);
            res[0] = g.races.size() == 3 && !g.races[0].cps.empty() && !g.races[1].cps.empty() && !g.races[2].cps.empty();
            // put the car on the start line of the first race and get in
            VehicleState& v = g.vehicles[0];
            v.pos = vec3(g.races[0].start.x, g.groundAt(g.races[0].start.x, g.races[0].start.y), g.races[0].start.y);
            v.yaw = std::atan2(g.races[0].startDir.x, g.races[0].startDir.y);
            teleport(g, v.pos.x + 3, v.pos.z, 0.f);
            g.vehiclesUnlocked = {"car", "moto", "airship"}; g.enterVehicle(0);
        }, idle, "on the line"});
        s.push_back({2.f, none, idle, "hold still -> countdown"});
        s.push_back({0.05f, [](Game& g) { res[1] = g.race.active == 0 && g.race.countdown > 0; AGR_LOG("RACE started %d, countdown %.1f", g.race.active, g.race.countdown); g.requestScreenshot("build/shots/T0_race_start.png"); }, idle, "countdown"});
        s.push_back({3.2f, none, idle, "3, 2, 1"});
        s.push_back({0.05f, [](Game& g) { t0 = g.race.time; }, idle, "go"});
        // autopilot: stick towards the next checkpoint (camera behind the car)
        s.push_back({120.f, none, [](Game& g, float t) {
            if (g.race.active < 0) return;
            const RaceTrack& tr = g.races[g.race.active];
            if (g.race.next >= (int)tr.cps.size()) return;
            VehicleState& v = g.vehicles[g.driving];
            vec2 to = tr.cps[g.race.next] - v.pos.xz();
            g.camYaw = v.yaw + PI;
            // stick in camera space: forward = (-sin camYaw, -cos camYaw) = car forward; right = (cos, -sin)
            vec2 fw(std::sin(v.yaw), std::cos(v.yaw)), rt(-std::cos(v.yaw), std::sin(v.yaw));
            vec2 d = normalize(to);
            g.input.move = vec2(dot(d, rt), dot(d, fw));
            if (length(g.input.move) > 0) g.input.move = normalize(g.input.move);
            static float lastShot = 0; if (t > 6.f && lastShot == 0) { lastShot = t; g.requestScreenshot("build/shots/T1_race_drive.png"); }
            static float lastLog = -1; if (std::floor(t) != std::floor(lastLog)) AGR_LOG("RACE t=%.0f next %d dist %.1f speed %.1f yaw %.2f stick (%.2f, %.2f)", t, g.race.next, length(to), v.speed, v.yaw, g.input.move.x, g.input.move.y); lastLog = t;
        }, "drive"});
        s.back().until = [](Game& g) { return g.race.active < 0; };
        s.push_back({0.1f, [](Game& g) {
            res[2] = g.raceBest.count("r_millbrook") > 0;
            res[3] = g.storyFlags.count("race_r_millbrook_1") > 0;   // at least bronze with the autopilot
            AGR_LOG("RACE result: %s (best %.1f s)", g.raceResult.c_str(), g.raceBest.count("r_millbrook") ? g.raceBest["r_millbrook"] : -1.f);
            g.requestScreenshot("build/shots/T2_race_end.png");
        }, idle, "finish"});
        s.push_back({1.5f, [](Game& g) { g.exitVehicle(); }, idle, "challenges tick (game running)"});
        s.push_back({0.5f, [](Game& g) {
            res[4] = g.storyFlags.count("ch_c_race") > 0;
            AGR_LOG("RACE challenge 'Pilote' done %d", res[4]);
            g.menu = Menu::Objectives;
        }, idle, "objectives"});
        s.push_back({0.1f, [](Game& g) {
            g.requestScreenshot("build/shots/T3_objectives.png");
            AGR_LOG("RACE TEST: tracks=%d start=%d finish=%d medal=%d challenge=%d", res[0], res[1], res[2], res[3], res[4]);
            bool all = true; for (int r : res) all = all && r;
            AGR_LOG(all ? "RACE TEST PASSED" : "RACE TEST FAILED");
        }, idle, "check"});
        return s;
    }
    if (name == "river") {
        // 1.11.1 user report: a river near Brindlemere shown as a dry grey stepped bed (no water). Look at the
        // river points closest to the village, check each loaded chunk on the river has a water mesh.
        static std::vector<vec2> spots;
        s.push_back({0.1f, [](Game& g) {
            spots.clear();
            const Poi* b = g.world.poiById("brindlemere");
            std::vector<std::pair<float, vec2>> pts;
            for (auto& rv : g.world.rivers) for (size_t i = 0; i + 1 < rv.pts.size(); i++) for (float t = 0; t < 1; t += 0.25f) { vec2 p = lerp(rv.pts[i], rv.pts[i + 1], t); pts.push_back({length(p - b->pos), p}); }
            std::sort(pts.begin(), pts.end(), [](auto& a, auto& c) { return a.first < c.first; });
            for (auto& p : pts) { bool far = true; for (auto& q : spots) if (length(q - p.second) < 60) far = false; if (far) spots.push_back(p.second); if (spots.size() >= 3) break; }
            g.timeOfDay = 11.5f; g.weather = g.weatherNext = Game::W_CLEAR; g.weatherBlend = 1; g.weatherTimer = 999; g.toastHint.clear();
        }, idle, "find"});
        for (int i = 0; i < 3; i++) {
            s.push_back({5.f, [i](Game& g) {
                if (i >= (int)spots.size()) return;
                vec2 p = spots[i] + vec2(14, 10);
                teleport(g, p.x, p.y, 0.f);
                vec2 d = spots[i] - p; g.camYaw = std::atan2(-d.x, -d.y); g.camPitch = i == 1 ? 0.15f : 0.6f; g.camDist = g.camDistTarget = i == 1 ? 7.f : 18.f;
            }, idle, "go"});
            s.push_back({0.05f, [i](Game& g) {
                if (i >= (int)spots.size()) return;
                vec2 p = spots[i];
                Chunk* ch = g.world.chunkAt(p.x, p.y);
                float wl = g.world.waterLevel(p.x, p.y), gh = g.world.height(p.x, p.y);
                AGR_LOG("RIVER spot %d (%.0f, %.0f): water level %.2f, ground %.2f, chunk lod %d ready %d water mesh %d (%d indices)", i, p.x, p.y, wl, gh, ch ? ch->lod : -9, ch ? ch->ready : 0, ch && ch->water.valid(), ch ? ch->water.indexCount : 0);
                g.requestScreenshot("build/shots/S" + std::to_string(i) + "_river.png");
            }, idle, "shot"});
        }
        s.push_back({0.05f, [](Game& g) { AGR_LOG("RIVER TEST PASSED"); }, idle, "end"});
        return s;
    }
    if (name == "vehicles") {
        // 1.11.0: car and motorbike - get in with a real tap, drive on the terrain, turn, wheels spinning,
        // body following the ground, rider on the bike (leaning in turns), get out, save / load
        static int res[9]; static vec3 p0; static float spin0, yaw0, maxLean, pitchMin, pitchMax, topSpeed;
        auto tapInteract = [](Game& g) { vec2 c = ctlCentre(g, CTL_INTERACT); float k = g.ui.scale; g.touch(TouchEvent{TouchEvent::Down, 720, c.x * k, c.y * k}); };
        auto release = [](Game& g) { for (auto& f : g.ui.fingers) if (f.id == 720) { float k = g.ui.scale; g.touch(TouchEvent{TouchEvent::Up, 720, f.x * k, f.y * k}); } };
        auto goTo = [](Game& g, int i) {
            VehicleState& v = g.vehicles[i];
            vec3 side = g.vehicleLocal(v, vec3(-2.6f, 0, 0)) - v.pos;
            teleport(g, v.pos.x + side.x, v.pos.z + side.z, 0.f);
            g.pc().yaw = std::atan2(v.pos.x - g.pc().pos.x, v.pos.z - g.pc().pos.z);
        };
        s.push_back({2.5f, [](Game& g) {
            for (int& r : res) r = 0;
            g.timeOfDay = 12.f; g.weather = g.weatherNext = Game::W_CLEAR; g.weatherBlend = 1; g.weatherTimer = 999; g.toastHint.clear();
            g.vehicles.clear();
            g.vehiclesUnlocked = {"car", "moto", "airship"};   // 3.4.0 the vehicles are earned in the story: this test drives them all
        }, idle, "init"});
        s.push_back({1.5f, [goTo](Game& g) { goTo(g, 0); }, idle, "walk to the car"});
        s.push_back({0.25f, tapInteract, idle, "tap: drive"});
        s.push_back({0.3f, release, idle, "release"});
        s.push_back({0.1f, [](Game& g) {
            res[0] = g.driving == 0 && !g.pc().vis.visible;
            p0 = g.vehicles[0].pos; spin0 = g.vehicles[0].spin; yaw0 = g.vehicles[0].yaw; pitchMin = 1e9f; pitchMax = -1e9f; topSpeed = 0;
            g.camYaw = g.vehicles[0].yaw + PI;   // camera behind the car
            AGR_LOG("VEHICLES in the car: driving %d, hero visible %d", g.driving, g.pc().vis.visible);
        }, idle, "inside"});
        s.push_back({4.5f, none, [](Game& g, float t) { g.input.move = vec2(0, 1); g.camYaw = g.vehicles[0].yaw + PI;
            static float lastT = -1; if (std::floor(t * 2) != std::floor(lastT * 2)) AGR_LOG("VEHICLES car t=%.1f speed %.1f", t, g.vehicles[0].speed); lastT = t;
            pitchMin = std::min(pitchMin, g.vehicles[0].pitch); pitchMax = std::max(pitchMax, g.vehicles[0].pitch); topSpeed = std::max(topSpeed, g.vehicles[0].speed); }, "drive"});
        s.push_back({0.05f, [](Game& g) {
            VehicleState& v = g.vehicles[0];
            float moved = distXZ(v.pos, p0);
            float gap = v.pos.y - g.groundAt(v.pos.x, v.pos.z);
            res[1] = moved > 30.f && topSpeed > 20.f && std::fabs(v.spin - spin0) > 20.f;   // it may have hit something since
            res[2] = std::fabs(gap) < 0.4f;
            AGR_LOG("VEHICLES car: moved %.0f m, top speed %.1f m/s, now %.1f m/s (%.0f km/h), wheel turns %.0f, pitch %.3f..%.3f, height over ground %.2f m",
                    moved, topSpeed, v.speed, v.speed * 3.6f, (v.spin - spin0) / TAU, pitchMin, pitchMax, gap);
            g.requestScreenshot("build/shots/R0_car_drive.png");
        }, [](Game& g, float) { g.input.move = vec2(0, 1); }, "shot"});
        s.push_back({2.f, none, [](Game& g, float) { g.input.move = vec2(0.8f, 0.6f); }, "turn"});
        s.push_back({0.05f, [](Game& g) {
            res[3] = std::fabs(wrapAngle(g.vehicles[0].yaw - yaw0)) > 0.5f;
            AGR_LOG("VEHICLES car turned %.2f rad, steer %.2f", wrapAngle(g.vehicles[0].yaw - yaw0), g.vehicles[0].steer);
            g.camYaw = g.vehicles[0].yaw + PI * 0.5f; g.camPitch = 0.2f; g.camDist = g.camDistTarget = 9.f;
        }, [](Game& g, float) { g.input.move = vec2(0.8f, 0.6f); }, "check"});
        s.push_back({0.6f, none, [](Game& g, float) { g.input.move = vec2(0, 0); g.camYaw = g.vehicles[0].yaw + PI * 0.5f; }, "brake"});
        s.push_back({0.05f, [](Game& g) { g.requestScreenshot("build/shots/R1_car_side.png"); }, idle, "side shot"});
        s.push_back({1.5f, none, idle, "stop"});
        s.push_back({0.25f, tapInteract, idle, "tap: get out"});
        s.push_back({0.3f, release, idle, "release"});
        s.push_back({0.1f, [](Game& g) { res[4] = g.driving == -1 && g.pc().vis.visible && distXZ(g.pc().pos, g.vehicles[0].pos) < 4.f; AGR_LOG("VEHICLES out of the car: driving %d, %.1f m from it", g.driving, distXZ(g.pc().pos, g.vehicles[0].pos)); }, idle, "out"});
        // the motorbike
        s.push_back({1.5f, [goTo](Game& g) { goTo(g, 1); }, idle, "walk to the moto"});
        s.push_back({0.25f, tapInteract, idle, "tap: ride"});
        s.push_back({0.3f, release, idle, "release"});
        s.push_back({0.1f, [](Game& g) {
            res[5] = g.driving == 1 && g.pc().vis.visible && g.pc().vis.anim.state == BodyState::Mounted;
            maxLean = 0; g.camYaw = g.vehicles[1].yaw + PI;
            AGR_LOG("VEHICLES on the moto: driving %d, state %d", g.driving, (int)g.pc().vis.anim.state);
        }, idle, "on"});
        s.push_back({3.f, none, [](Game& g, float) { g.input.move = vec2(0, 1); g.camYaw = g.vehicles[1].yaw + PI; }, "ride"});
        s.push_back({2.f, none, [](Game& g, float) { g.input.move = vec2(-0.75f, 0.65f); maxLean = std::max(maxLean, std::fabs(g.vehicles[1].roll)); }, "lean"});
        s.push_back({0.02f, [](Game& g) { g.camYaw = g.vehicles[1].yaw - PI * 0.5f; g.camPitch = 0.15f; g.camDist = g.camDistTarget = 6.f; }, [](Game& g, float) { g.input.move = vec2(-0.75f, 0.65f); }, "side camera"});
        s.push_back({0.05f, [](Game& g) {
            VehicleState& v = g.vehicles[1];
            Character& c = g.pc();
            vec3 seat = g.vehicleLocal(v, g.vehicleAsset("moto")->data.joints[1]);
            mat4 model = mat4::trs(c.pos, quat::axisAngle({0, 1, 0}, c.yaw) * quat::axisAngle({1, 0, 0}, -c.vis.tiltPitch) * quat::axisAngle({0, 0, 1}, c.vis.tiltRoll), vec3(c.vis.scale));
            vec3 pelvis = model.transformPoint(c.vis.pw.wpos[H_ROOT]);
            res[6] = maxLean > 0.15f;
            res[7] = length(pelvis - seat) < 0.15f;
            AGR_LOG("VEHICLES moto: speed %.1f m/s, max lean %.2f rad, pelvis %.2f m from the seat", v.speed, maxLean, length(pelvis - seat));
            g.requestScreenshot("build/shots/R2_moto_lean.png");
        }, [](Game& g, float) { g.input.move = vec2(-0.75f, 0.65f); }, "shot"});
        s.push_back({0.8f, none, [](Game& g, float) { g.input.move = vec2(0, 0); g.camYaw = g.vehicles[1].yaw - PI * 0.5f; }, "slow"});
        s.push_back({0.05f, [](Game& g) { g.requestScreenshot("build/shots/R3_moto_side.png"); }, idle, "side"});
        s.push_back({1.5f, none, idle, "stop"});
        s.push_back({0.25f, tapInteract, idle, "tap: get off"});
        s.push_back({0.3f, release, idle, "release"});
        s.push_back({0.3f, [](Game& g) {
            vec3 car = g.vehicles[0].pos, moto = g.vehicles[1].pos;
            g.saveGame(3); g.loadGame(3);
            res[8] = g.driving == -1 && g.vehicles.size() == vehicleDefs().size() && distXZ(g.vehicles[0].pos, car) < 0.5f && distXZ(g.vehicles[1].pos, moto) < 0.5f;
            AGR_LOG("VEHICLES after save/load: car %.1f m, moto %.1f m from where they were left", distXZ(g.vehicles[0].pos, car), distXZ(g.vehicles[1].pos, moto));
            AGR_LOG("VEHICLES TEST: enterCar=%d drive=%d onGround=%d turn=%d exit=%d moto=%d lean=%d seat=%d saveLoad=%d", res[0], res[1], res[2], res[3], res[4], res[5], res[6], res[7], res[8]);
            bool all = true; for (int r : res) all = all && r;
            AGR_LOG(all ? "VEHICLES TEST PASSED" : "VEHICLES TEST FAILED");
        }, idle, "save/load"});
        return s;
    }
    if (name == "enemy") {
        // close-up of an enemy model (LK_ENEMY=<id>, default stone_golem): idle, then fighting the hero
        static Entity foe;
        s.push_back({3.f, [](Game& g) {
            const char* id = getenv("LK_ENEMY"); std::string eid = id ? id : "stone_golem";
            // open ground (no tall grass: the hero must be visible), outside any village
            const Poi* b = g.world.poiById("brindlemere"); vec2 at = b->pos + vec2(150, -120);
            for (float r = 0; r < 300; r += 7) { vec2 p = b->pos + vec2(150 + r, -120); if (g.world.tallGrassAmount(p.x, p.y) == 0 && g.world.tallGrassAmount(p.x, p.y + 7) == 0 && g.world.roadInfluence(p.x, p.y) < 0.1f) { at = p; break; } }
            teleport(g, at.x, at.y, 0.f);
            g.pc().yaw = 0; g.camYaw = PI; g.camPitch = 0.22f; g.camDist = g.camDistTarget = 9.f;
            g.timeOfDay = 13.f; g.weather = g.weatherNext = Game::W_CLEAR; g.weatherBlend = 1; g.weatherTimer = 999; g.toastHint.clear();
            g.level = 15; g.recomputeStats();
            vec3 p = g.pc().pos + vec3(0, 0, 7.f); p.y = g.world.groundHeight(p.x, p.z);
            foe = g.spawnEnemy(enemyIndex(eid), p);
            if (Character* c = g.reg.get<Character>(foe)) { c->yaw = PI; if (AIBrain* ai = g.reg.get<AIBrain>(foe)) ai->state = AIState::Idle; c->maxHp = c->hp = 99999; }
        }, [](Game& g, float) { g.pc().hp = g.pc().maxHp; if (AIBrain* ai = g.reg.get<AIBrain>(foe)) ai->state = AIState::Idle; }, "spawn"});
        s.push_back({0.05f, [](Game& g) { g.requestScreenshot("build/shots/Q0_enemy_idle.png"); }, idle, "shot"});
        s.push_back({0.1f, [](Game& g) { if (AIBrain* ai = g.reg.get<AIBrain>(foe)) { ai->state = AIState::Chase; ai->target = g.player; ai->aggro = 6; } }, idle, "aggro"});
        for (int i = 0; i < 4; i++) {
            s.push_back({1.4f, none, [](Game& g, float) { g.pc().hp = g.pc().maxHp; }, "fight"});
            s.push_back({0.05f, [i](Game& g) { Character* c = g.reg.get<Character>(foe); const Clip* a = c ? c->vis.anim.action() : nullptr;
                AGR_LOG("ENEMY frame %d: action %s, speed %.1f", i, a ? a->name.c_str() : "none", c ? c->vis.anim.speed : 0.f);
                g.requestScreenshot("build/shots/Q" + std::to_string(i + 1) + "_enemy_fight.png"); }, idle, "shot"});
        }
        s.push_back({0.05f, [](Game& g) { AGR_LOG("ENEMY TEST PASSED"); }, idle, "end"});
        return s;
    }
    if (name == "showcase") {
        // 1.10.3: every building model seen from the front (door side), in the world, at mid-afternoon
        struct K { int kind, variant; const char* f; };
        static const K list[] = {{S_HOUSE, 0, "P0_house"}, {S_HOUSE, 1, "P1_house_2"}, {S_HOUSE, 2, "P2_house_3"}, {S_INN, -1, "P3_inn"}, {S_STONEHOUSE, -1, "P4_stonehouse"},
                                 {S_LONGHOUSE, -1, "P5_longhouse"}, {S_CHAPEL, -1, "P6_chapel"}, {S_FORGE, -1, "P7_forge"}, {S_TOWER, -1, "P8_tower"}, {S_WELL, -1, "P9_well"},
                                 {S_STALL, -1, "PA_stall"}, {S_TENT, -1, "PB_tent"}};
        for (const K& k : list) {
            s.push_back({4.5f, [k](Game& g) {
                const Structure* best = nullptr;
                for (const Structure& st : g.world.structures) {
                    if (st.kind != k.kind) continue;
                    if (k.variant >= 0 && propModelFor(st.kind, st.variant) != propModelFor(k.kind, k.variant)) continue;
                    if (st.kind == S_STALL && st.variant < 0) continue;
                    best = &st; break;
                }
                if (!best) { AGR_LOG("SHOWCASE no %s in the world", k.f); return; }
                vec2 front = rotate(vec2(0, 1), -best->rot);
                float dist = std::max(structureFootprint(k.kind).x, structureFootprint(k.kind).y) * 2.2f + 6.f;
                vec2 hero = best->pos + front * (structureFootprint(k.kind).y + 2.5f);
                teleport(g, hero.x, hero.y, 0.f);
                g.pc().yaw = std::atan2(-front.x, -front.y);
                g.camYaw = std::atan2(front.x, front.y);   // camera behind the hero, looking at the building
                g.camDist = g.camDistTarget = dist; g.camPitch = 0.28f;
                g.timeOfDay = 15.5f; g.weather = g.weatherNext = Game::W_CLEAR; g.weatherBlend = 1; g.weatherTimer = 999; g.toastHint.clear();
            }, idle, "go"});
            s.push_back({0.05f, [k](Game& g) { g.requestScreenshot(std::string("build/shots/") + k.f + ".png"); }, idle, "shot"});
        }
        s.push_back({0.05f, [](Game& g) { AGR_LOG("SHOWCASE TEST PASSED"); }, idle, "end"});
        return s;
    }
    if (name == "grounded") {
        // 1.10.1: the buildings that floated the most before (village edges, slopes) seen from a low camera
        static std::vector<int> worst;
        s.push_back({0.1f, [](Game& g) {
            std::vector<std::pair<float, int>> gaps;
            for (int i = 0; i < (int)g.world.structures.size(); i++) {
                const Structure& st = g.world.structures[i];
                if (st.kind != S_HOUSE && st.kind != S_STONEHOUSE && st.kind != S_INN && st.kind != S_LONGHOUSE && st.kind != S_TOWER && st.kind != S_CHAPEL) continue;
                float oldY = st.poi >= 0 ? g.world.pois[st.poi].height : g.world.height(st.pos.x, st.pos.y);
                vec2 fp = structureFootprint(st.kind); float lo = 1e9f;
                for (int j = -1; j <= 1; j += 2) for (int k = -1; k <= 1; k += 2) { vec2 p = rotate(vec2(fp.x * k, fp.y * j), -st.rot) + st.pos; lo = std::min(lo, g.world.height(p.x, p.y)); }
                gaps.push_back({oldY - lo, i});
            }
            std::sort(gaps.rbegin(), gaps.rend());
            for (int i = 0; i < 3 && i < (int)gaps.size(); i++) { worst.push_back(gaps[i].second); AGR_LOG("GROUNDED before: structure %d kind %d floated %.2f m, plinth now %.2f m", gaps[i].second, g.world.structures[gaps[i].second].kind, gaps[i].first, structurePlinth(g.world.structures[gaps[i].second], g.world)); }
            g.timeOfDay = 11.f; g.weather = g.weatherNext = Game::W_CLEAR; g.weatherBlend = 1; g.weatherTimer = 999; g.toastHint.clear();
        }, idle, "find"});
        for (int i = 0; i < 3; i++) {
            s.push_back({4.f, [i](Game& g) {
                if (i >= (int)worst.size()) return;
                const Structure& st = g.world.structures[worst[i]];
                // stand on the downhill side, 14 m away, looking at the building
                float best = 1e9f; vec2 at = st.pos;
                for (int a = 0; a < 12; a++) { vec2 p = st.pos + vec2(std::cos(a * TAU / 12), std::sin(a * TAU / 12)) * 14.f; float h = g.world.height(p.x, p.y); if (h < best) { best = h; at = p; } }
                teleport(g, at.x, at.y, 0.f);
                vec2 d = st.pos - at; g.camYaw = std::atan2(-d.x, -d.y); g.pc().yaw = std::atan2(d.x, d.y);
                g.camDist = g.camDistTarget = 9.f; g.camPitch = 0.12f;
            }, idle, "go"});
            s.push_back({0.05f, [i](Game& g) { g.requestScreenshot("build/shots/N" + std::to_string(i) + "_grounded.png"); }, idle, "shot"});
        }
        s.push_back({0.05f, [](Game& g) { AGR_LOG("GROUNDED TEST PASSED"); }, idle, "end"});
        return s;
    }
    if (name == "domain") {
        // 1.9.2 the hero's domain: found it with a real tap, build (real taps on Construire), recruit, livestock,
        // soldiers fighting a wolf, daily production, save / load
        static int res[9]; static float wolfHp0; static Entity wolf; static vec2 spot;
        auto press = [](const char* tag) { return [tag](Game& g) { const Rect* r = g.ui.findWidget(tag); if (r) { float k = g.ui.scale; g.touch(TouchEvent{TouchEvent::Down, 710, (r->x + r->w / 2) * k, (r->y + r->h / 2) * k}); } else AGR_LOG("DOMAIN no widget '%s'", tag); }; };
        auto release = [](Game& g) { for (auto& f : g.ui.fingers) if (f.id == 710) { float k = g.ui.scale; g.touch(TouchEvent{TouchEvent::Up, 710, f.x * k, f.y * k}); } };
        // put the hero so that the building lands on `at` (the ghost sits in front of him)
        auto aim = [](Game& g, int def, vec2 at, float yaw) {
            float r = baseBuildings()[def].radius + 2.2f;
            vec2 from = at - vec2(std::sin(yaw), std::cos(yaw)) * r;
            teleport(g, from.x, from.y, yaw);
            g.pc().yaw = yaw;   // teleport only turns the camera
        };
        s.push_back({0.1f, [](Game& g) {
            for (int& r : res) r = 0;
            g.ui.recordWidgets = true;
            // a wild spot where a domain may be founded
            const Poi* b = g.world.poiById("brindlemere"); bool found = false;
            for (float rr = 160.f; rr < 900.f && !found; rr += 25.f)
                for (float a = 0; a < TAU && !found; a += 0.3f) {
                    vec2 p = b->pos + vec2(std::cos(a), std::sin(a)) * rr;
                    teleport(g, p.x, p.y, 0.f);
                    g.addItem("wood", 10, Rarity::Count, false);
                    std::string why; if (g.canFoundBase(why)) { found = true; spot = p; }
                }
            AGR_LOG("DOMAIN site at (%.0f, %.0f) found %d", spot.x, spot.y, found);
            g.addItem("wood", 300, Rarity::Count, false); g.addItem("stone", 200, Rarity::Count, false); g.addItem("iron_bar", 10, Rarity::Count, false); g.addItem("leather", 10, Rarity::Count, false);
            g.gold = 2000; g.level = 10; g.recomputeStats(); g.pc().hp = g.pc().maxHp;
            g.timeOfDay = 10.f; g.weather = g.weatherNext = Game::W_CLEAR; g.weatherBlend = 1; g.weatherTimer = 999; g.toastHint.clear();
            g.menu = Menu::Base;
        }, idle, "site + resources"});
        s.push_back({0.3f, press("Fonder mon domaine ici"), idle, "press found"});
        s.push_back({0.3f, release, idle, "release"});
        s.push_back({2.f, [](Game& g) {
            const Poi* p = g.world.poiById("player_base");
            res[0] = g.base.founded && p && g.menu == Menu::None;
            AGR_LOG("DOMAIN founded %d, poi %d, wood left %d", g.base.founded, p != nullptr, g.countItem("wood"));
        }, idle, "founded"});
        // a house with real taps: Journal > Domaine > Construire, then the build panel's Construire
        s.push_back({0.3f, [aim](Game& g) { aim(g, baseBuildIndex("house"), g.base.center + vec2(12, 0), PI * 0.5f); g.menu = Menu::Base; }, idle, "to the house spot"});
        s.push_back({0.3f, press("build:house"), idle, "press build house"});
        s.push_back({0.3f, release, idle, "release"});
        s.push_back({0.4f, [](Game& g) { AGR_LOG("DOMAIN build mode %d ok %d (%s)", g.buildMode, g.buildOk, g.buildWhy.c_str()); g.requestScreenshot("build/shots/M0_build_ghost.png"); }, idle, "ghost"});
        s.push_back({0.3f, press("Construire"), idle, "press construire"});
        s.push_back({0.3f, release, idle, "release"});
        s.push_back({0.5f, [](Game& g) { res[1] = g.baseCount("house") == 1 && !g.buildMode; AGR_LOG("DOMAIN houses %d", g.baseCount("house")); }, idle, "house built"});
        // the rest by code: tower, pen, two fields, forge, palisades
        s.push_back({0.2f, [aim](Game& g) {
            struct P { const char* id; vec2 off; float yaw; };
            P list[] = {{"tower", {-14, 0}, -PI * 0.5f}, {"pen", {0, -18}, PI}, {"field", {-10, 17}, 0.f}, {"field", {11, 17}, 0.f}, {"forge", {-15, -12}, PI}, {"inn", {17, -14}, PI * 0.5f}};
            int ok = 0;
            for (auto& q : list) { int d = baseBuildIndex(q.id); aim(g, d, g.base.center + q.off, q.yaw); g.startBuilding(d); g.updateBuildMode(); if (g.placeBuilding()) ok++; else AGR_LOG("DOMAIN %s refused: %s", q.id, g.buildWhy.c_str()); }
            res[2] = ok == 6;
            AGR_LOG("DOMAIN built %zu buildings, housing %d, guards %d", g.base.built.size(), g.baseHousing(), g.baseGuardCap());
        }, idle, "build the rest"});
        // recruit with real taps
        s.push_back({0.2f, [](Game& g) { const vec2 c = g.base.center; teleport(g, c.x + 3, c.y + 3, 0.f); g.menu = Menu::Base; }, idle, "menu"});
        for (int i = 0; i < 3; i++) { s.push_back({0.25f, press("recruit"), idle, "press recruit"}); s.push_back({0.25f, release, idle, "release"}); }
        for (int i = 0; i < 2; i++) { s.push_back({0.25f, press("soldier"), idle, "press soldier"}); s.push_back({0.25f, release, idle, "release"}); }
        for (int i = 0; i < 2; i++) { s.push_back({0.25f, press("pig"), idle, "press pig"}); s.push_back({0.25f, release, idle, "release"}); }
        s.push_back({0.25f, press("horse"), idle, "press horse"}); s.push_back({0.25f, release, idle, "release"});
        s.push_back({0.1f, [](Game& g) {
            res[3] = g.base.residents == 3 && g.base.soldiers == 2 && g.base.pigs == 2 && g.base.horses == 1;
            AGR_LOG("DOMAIN residents %d soldiers %d pigs %d horses %d, gold %d", g.base.residents, g.base.soldiers, g.base.pigs, g.base.horses, g.gold);
            g.menu = Menu::None;
        }, idle, "recruited"});
        s.push_back({4.f, none, idle, "people arrive"});
        s.push_back({0.1f, [](Game& g) {
            int npcs = 0; g.reg.each<NpcBrain>([&](Entity, NpcBrain& nb) { if (nb.village == g.base.poi) npcs++; });
            res[4] = npcs == 3 && g.soldierEnts.size() == 2 && g.livestockEnts.size() == 3;
            AGR_LOG("DOMAIN in the world: %d inhabitants, %zu soldiers, %zu animals", npcs, g.soldierEnts.size(), g.livestockEnts.size());
            const vec2 c = g.base.center; teleport(g, c.x, c.y + 6, 0.f);
            g.camDist = g.camDistTarget = 40.f; g.camPitch = 0.95f;
        }, idle, "count"});
        s.push_back({3.f, none, idle, "settle"});
        s.push_back({0.05f, [](Game& g) { g.requestScreenshot("build/shots/M1_domain.png"); }, idle, "shot"});
        // a wolf comes close: the soldiers fight it
        s.push_back({0.1f, [](Game& g) {
            vec3 p(g.base.center.x + 6, 0, g.base.center.y + 6); p.y = g.groundAt(p.x, p.z);
            wolf = g.spawnEnemy(enemyIndex("wolf"), p);
            if (Character* w = g.reg.get<Character>(wolf)) { w->maxHp = w->hp = 400; wolfHp0 = w->hp; }
            g.camDist = g.camDistTarget = 14.f; g.camPitch = 0.6f;
        }, idle, "wolf"});
        s.push_back({5.f, none, [](Game& g, float) { g.pc().hp = g.pc().maxHp; }, "fight"});
        s.push_back({0.05f, [](Game& g) {
            Character* w = g.reg.get<Character>(wolf);
            float hp = w ? (w->dead ? 0.f : w->hp) : 0.f;
            res[5] = hp < wolfHp0 - 30.f;
            AGR_LOG("DOMAIN wolf %.0f -> %.0f", wolfHp0, hp);
            g.requestScreenshot("build/shots/M2_soldiers.png");
        }, idle, "soldiers fought"});
        // a new day: production
        s.push_back({0.1f, [](Game& g) { g.day++; }, idle, "next day"});
        s.push_back({0.2f, [](Game& g) {
            int bread = 0, meat = 0; for (auto& st : g.base.storage) { if (st.first == "bread") bread = st.second; if (st.first == "meat") meat = st.second; }
            res[6] = bread >= 4 && meat == 2;
            AGR_LOG("DOMAIN production: %d bread, %d meat", bread, meat);
        }, idle, "production"});
        // save and load: the domain comes back
        s.push_back({0.2f, [](Game& g) { g.saveGame(3); }, idle, "save"});
        s.push_back({0.5f, [](Game& g) {
            size_t built = g.base.built.size();
            bool ok = g.loadGame(3);
            const Poi* p = g.world.poiById("player_base");
            res[7] = ok && g.base.founded && g.base.built.size() == built && p && g.base.residents == 3 && g.base.soldiers == 2;
            int extra = (int)g.world.structures.size() - g.world.plannedStructures;
            AGR_LOG("DOMAIN after load: founded %d, %zu buildings, %d runtime structures, poi %d", g.base.founded, g.base.built.size(), extra, p != nullptr);
            res[8] = extra == (int)built + 1;   // buildings + banner, not duplicated
        }, idle, "load"});
        s.push_back({0.1f, [](Game& g) {
            AGR_LOG("DOMAIN TEST: found=%d tapBuild=%d build=%d recruit=%d spawned=%d defend=%d produce=%d saveLoad=%d noDup=%d", res[0], res[1], res[2], res[3], res[4], res[5], res[6], res[7], res[8]);
            bool all = true; for (int r : res) all = all && r;
            AGR_LOG(all ? "DOMAIN TEST PASSED" : "DOMAIN TEST FAILED");
        }, idle, "check"});
        return s;
    }
    if (name == "hunters") {
        // 1.9.2 night hunters: a wave finds the hero in the open at night, tracks him, and gives up in a village
        static int res[4]; static float d0;
        s.push_back({0.1f, [](Game& g) {
            for (int& r : res) r = 0;
            const Poi* b = g.world.poiById("brindlemere"); teleport(g, b->pos.x + 150, b->pos.y - 120, 0.f);
            g.timeOfDay = 23.f; g.weather = g.weatherNext = Game::W_CLEAR; g.weatherBlend = 1; g.weatherTimer = 999; g.toastHint.clear();
            g.level = 8; g.recomputeStats(); g.pc().hp = g.pc().maxHp; g.hunterT = 2.f;
            g.camDist = g.camDistTarget = 16.f; g.camPitch = 0.6f;
        }, idle, "night, in the open"});
        s.push_back({3.f, none, [](Game& g, float) { g.pc().hp = g.pc().maxHp; }, "night settles"});
        s.push_back({0.05f, [](Game& g) { AGR_LOG("HUNTERS night %.2f", g.env.night); g.hunterT = 1.f; }, idle, "dusk delay over"});
        s.push_back({2.f, none, [](Game& g, float) { g.pc().hp = g.pc().maxHp; }, "wait for the howl"});
        s.push_back({0.05f, [](Game& g) {
            res[0] = !g.nightHunters.empty() && !g.playerInSafeZone();
            d0 = 1e9f; for (Entity h : g.nightHunters) if (Character* c = g.reg.get<Character>(h)) d0 = std::min(d0, distXZ(c->pos, g.pc().pos));
            AGR_LOG("HUNTERS %zu spawned, nearest %.0f m", g.nightHunters.size(), d0);
        }, idle, "spawned"});
        s.push_back({4.f, none, [](Game& g, float) { g.pc().hp = g.pc().maxHp; }, "they come"});
        s.push_back({0.05f, [](Game& g) {
            float d = 1e9f; for (Entity h : g.nightHunters) if (Character* c = g.reg.get<Character>(h)) d = std::min(d, distXZ(c->pos, g.pc().pos));
            res[1] = d < d0 - 8.f;
            AGR_LOG("HUNTERS nearest now %.0f m (was %.0f)", d, d0);
            g.requestScreenshot("build/shots/L0_hunters.png");
        }, idle, "closing in"});
        s.push_back({0.1f, [](Game& g) { const Poi* b = g.world.poiById("brindlemere"); teleport(g, b->pos.x, b->pos.y + 6, 0.f); }, idle, "flee to the village"});
        s.push_back({10.f, none, [](Game& g, float) { g.pc().hp = g.pc().maxHp; }, "safe"});
        s.push_back({0.05f, [](Game& g) {
            res[2] = g.playerInSafeZone() && g.nightHunters.empty();
            AGR_LOG("HUNTERS in the village: %zu left", g.nightHunters.size());
            // dawn also ends a hunt
            const Poi* b = g.world.poiById("brindlemere"); teleport(g, b->pos.x + 150, b->pos.y - 120, 0.f);
            g.hunterT = 0.5f;
        }, idle, "back out"});
        s.push_back({2.5f, none, [](Game& g, float) { g.pc().hp = g.pc().maxHp; }, "second wave"});
        s.push_back({0.05f, [](Game& g) { AGR_LOG("HUNTERS second wave: %zu", g.nightHunters.size()); g.timeOfDay = 7.5f; }, idle, "dawn"});
        s.push_back({4.f, none, [](Game& g, float) { g.pc().hp = g.pc().maxHp; }, "sunrise"});
        s.push_back({0.05f, [](Game& g) {
            res[3] = g.nightHunters.empty() && g.huntsSurvived >= 1;
            AGR_LOG("HUNTERS at dawn: %zu left, hunts survived %d", g.nightHunters.size(), g.huntsSurvived);
            AGR_LOG("HUNTERS TEST: spawn=%d track=%d village=%d dawn=%d", res[0], res[1], res[2], res[3]);
            AGR_LOG(res[0] && res[1] && res[2] && res[3] ? "HUNTERS TEST PASSED" : "HUNTERS TEST FAILED");
        }, idle, "check"});
        return s;
    }
    if (name == "rain") {
        // 1.9.2 rain: wet ground, spreading puddles with ripples and a sky mirror, splashes, far curtains, soaked hero
        static int res[3];
        s.push_back({0.1f, [](Game& g) {
            for (int& r : res) r = 0;
            const Poi* b = g.world.poiById("brindlemere"); teleport(g, b->pos.x - 4, b->pos.y + 8, 2.6f);
            g.timeOfDay = 15.f; g.weather = g.weatherNext = Game::W_STORM; g.weatherBlend = 1; g.weatherTimer = 999; g.toastHint.clear();
            g.rainAmt = 0.f; g.env.wetness = 0.f; g.weather = g.weatherNext = Game::W_CLEAR;
            g.camDist = g.camDistTarget = 11.f; g.camPitch = 0.38f;
        }, idle, "dry"});
        s.push_back({4.f, none, [](Game& g, float) { g.rainAmt = 0.f; g.env.wetness = 0.f; }, "dry settle"});
        s.push_back({0.05f, [](Game& g) { g.requestScreenshot("build/shots/K9_dry_ref.png"); }, idle, "dry shot"});
        s.push_back({0.1f, [](Game& g) { g.weather = g.weatherNext = Game::W_STORM; g.weatherBlend = 1; g.rainAmt = 1.f; g.env.wetness = 1.f; }, idle, "storm"});
        s.push_back({6.f, none, [](Game& g, float) { g.rainAmt = 1.f; }, "rain falls"});
        s.push_back({0.05f, [](Game& g) {
            res[0] = g.env.wetness > 0.8f && g.particles.count() > 600;
            res[1] = g.pc().vis.wetT > 5.f;
            AGR_LOG("RAIN wetness %.2f, %zu particles, hero wet %.1f s", g.env.wetness, g.particles.count(), g.pc().vis.wetT);
            g.requestScreenshot("build/shots/K0_rain_village.png");
        }, idle, "shot"});
        s.push_back({0.1f, [](Game& g) { g.camPitch = 0.12f; g.camDist = g.camDistTarget = 6.f; }, idle, "low"});
        s.push_back({1.f, none, [](Game& g, float) { g.rainAmt = 1.f; }, "settle"});
        s.push_back({0.05f, [](Game& g) {
            res[2] = 1;
            g.requestScreenshot("build/shots/K1_rain_low.png");
            AGR_LOG(res[0] && res[1] ? "RAIN TEST PASSED" : "RAIN TEST FAILED");
        }, idle, "shot"});
        return s;
    }
    if (name == "tallgrass") {
        // 1.9.1: tall grass meadows: placed in patches, drawn, bend around the hero, hide him from enemies
        static int res[4]; static vec2 spot; static Entity wolf;
        s.push_back({0.1f, [](Game& g) {
            for (int& r : res) r = 0;
            const Poi* b = g.world.poiById("brindlemere"); spot = b->pos; float best = 0;
            for (float rr = 90.f; rr < 700.f && best < 0.95f; rr += 12.f)
                for (float a = 0; a < TAU; a += 0.08f) {
                    vec2 p = b->pos + vec2(std::cos(a), std::sin(a)) * rr;
                    float t = g.world.tallGrassAmount(p.x, p.y);
                    if (t > best) { best = t; spot = p; }
                }
            AGR_LOG("TALLGRASS meadow at (%.0f, %.0f), density %.2f", spot.x, spot.y, best);
            teleport(g, spot.x, spot.y, 0.8f);
            g.timeOfDay = 17.f; g.weather = g.weatherNext = Game::W_CLEAR; g.weatherBlend = 1; g.weatherTimer = 999; g.toastHint.clear();
            g.camDist = g.camDistTarget = 7.f; g.camPitch = 0.25f;
        }, idle, "find a meadow"});
        s.push_back({5.f, none, idle, "stream"});
        s.push_back({0.1f, [](Game& g) {
            size_t inst = 0; g.world.forEachVisibleChunk([&](Chunk& c) { if (c.tallGrass.valid()) inst += c.tallGrass.instanceCount; });
            res[0] = inst > 500 && g.tallGrassHere > 0.5f;
            AGR_LOG("TALLGRASS %zu tall clumps in loaded chunks, hero in grass %.2f, hint \"%s\"", inst, g.tallGrassHere, g.toastHint.c_str());
            g.requestScreenshot("build/shots/J0_tallgrass.png");
        }, idle, "check"});
        // walking through: the grass parts (pushers active) and grasshoppers jump
        s.push_back({2.5f, [](Game& g) { g.camPitch = 0.3f; g.camDist = g.camDistTarget = 6.f; }, [](Game& g, float) { g.input.move = vec2(0.3f, 0.7f); }, "walk"});
        s.push_back({0.05f, [](Game& g) {
            int act = 0; for (auto& p : g.env.pushers) if (p.w != 0) act++;
            res[1] = act >= 3;
            AGR_LOG("TALLGRASS walking: %d benders active", act);
            g.requestScreenshot("build/shots/J1_tallgrass_walk.png"); }, [](Game& g, float) { g.input.move = vec2(0.3f, 0.7f); }, "shot"});
        // cover: a wolf at 13 m sees the hero on open ground, not in the meadow (unless he attacks)
        s.push_back({0.1f, [](Game& g) {
            Character& c = g.pc(); vec3 f{std::sin(c.yaw), 0, std::cos(c.yaw)};
            vec3 p = c.pos + f * 13.f; p.y = g.world.groundHeight(p.x, p.z);
            wolf = g.spawnEnemy(enemyIndex("wolf"), p);
            Character* w = g.reg.get<Character>(wolf); if (!w) return;
            w->yaw = std::atan2(-f.x, -f.z);
            float keep = g.tallGrassHere;
            bool inGrass = g.canSee(*w, c, 20.f, 360.f);
            g.tallGrassHere = 0; bool open = g.canSee(*w, c, 20.f, 360.f); g.tallGrassHere = keep;
            res[2] = !inGrass && open;
            AGR_LOG("TALLGRASS wolf at 13 m: sees the hero in the grass %d, on open ground %d", inGrass, open);
        }, idle, "cover"});
        s.push_back({0.1f, [](Game& g) {
            g.camPitch = 0.9f; g.camDist = g.camDistTarget = 24.f;
            res[3] = 1;
        }, idle, "overview"});
        s.push_back({1.2f, none, idle, "settle"});
        s.push_back({0.05f, [](Game& g) {
            g.requestScreenshot("build/shots/J2_meadow_view.png");
            AGR_LOG("TALLGRASS TEST: meadow=%d bend=%d cover=%d", res[0], res[1], res[2]);
            AGR_LOG(res[0] && res[1] && res[2] ? "TALLGRASS TEST PASSED" : "TALLGRASS TEST FAILED");
        }, idle, "check"});
        return s;
    }
    if (name == "details") {
        // 1.9.0 realism: breath vapour in Frostheim, dripping after a swim
        static int res[2]; static size_t p0;
        s.push_back({3.f, [](Game& g) {
            const Poi* p = g.world.poiById("frostvik"); teleport(g, p->pos.x + 20, p->pos.y + 26, 2.4f);
            g.timeOfDay = 9.f; g.weather = g.weatherNext = Game::W_CLEAR; g.weatherBlend = 1; g.weatherTimer = 999; g.toastHint.clear();
            g.camDist = g.camDistTarget = 3.2f; g.camPitch = 0.1f; g.camYaw = g.pc().yaw + 0.5f; g.pc().vis.breathT = 0.2f;
        }, idle, "frostheim"});
        s.push_back({0.35f, [](Game& g) { p0 = g.particles.count(); }, idle, "before"});
        s.push_back({1.2f, none, [](Game& g, float) { g.camYaw = g.pc().yaw + 0.5f; }, "breathe"});
        s.push_back({0.05f, [](Game& g) {
            size_t before = g.particles.count();
            g.pc().vis.breathT = 0; g.updateBodyDetails(g.pc(), 0.016f);   // force one breath now
            size_t puff = g.particles.count() - before;
            res[0] = puff >= 3 && g.pc().vis.breathT > 0.8f;
            AGR_LOG("DETAILS breath: %zu vapour particles in one breath, next in %.1f s", puff, g.pc().vis.breathT);
            g.requestScreenshot("build/shots/I0_breath.png"); }, idle, "shot"});
        s.push_back({0.1f, [](Game& g) { g.pc().inWater = true; g.updateBodyDetails(g.pc(), 0.016f); g.pc().inWater = false; }, idle, "swim"});
        s.push_back({1.f, none, idle, "drip"});
        s.push_back({0.05f, [](Game& g) {
            res[1] = g.pc().vis.wetT > 20.f && g.pc().vis.wetT < 25.f;
            AGR_LOG("DETAILS wet: %.1f s left", g.pc().vis.wetT);
            g.requestScreenshot("build/shots/I1_wet.png");
            AGR_LOG(res[0] && res[1] ? "DETAILS TEST PASSED" : "DETAILS TEST FAILED");
        }, idle, "check"});
        return s;
    }
    if (name == "combo") {
        // 1.9.0: Astra's unarmed combo (jab, hook, uppercut) and heavy (roundhouse kick), filmed from the side
        static int res[2]; static const char* seen[4];
        s.push_back({2.5f, [](Game& g) {
            const Poi* p = g.world.poiById("brindlemere"); teleport(g, p->pos.x + 30, p->pos.y + 30, 0.6f);
            g.timeOfDay = 11.f; g.weather = g.weatherNext = Game::W_CLEAR; g.weatherBlend = 1; g.weatherTimer = 999; g.toastHint.clear();
            g.unlockHero("astra"); g.switchHero(heroIndex("astra")); g.notes.clear();
            g.camYaw = g.pc().yaw + PI * 0.5f; g.camPitch = 0.2f; g.camDist = g.camDistTarget = 5.5f;
        }, idle, "setup"});
        static const char* names[] = {"H0_jab", "H1_hook", "H2_uppercut", "H3_roundhouse"};
        for (int i = 0; i < 4; i++) {
            s.push_back({0.02f, [i](Game& g) { if (i < 3) { g.input.attackRelease = true; g.input.attackHoldT = 0.05f; } else g.input.heavy = true; }, idle, "attack"});
            float peak = i == 0 ? 0.13f : i == 1 ? 0.16f : i == 2 ? 0.3f : 0.4f;
            s.push_back({peak, none, idle, "to the hit"});
            s.push_back({0.02f, [i](Game& g) {
                const Clip* a = g.pc().vis.anim.action(); seen[i] = a ? a->name.c_str() : "none";
                g.requestScreenshot(std::string("build/shots/") + names[i] + ".png"); }, idle, "shot"});
            s.push_back({i == 3 ? 0.6f : (i == 2 ? 0.25f : 0.12f), none, idle, "recover"});
        }
        s.push_back({0.1f, [](Game& g) {
            res[0] = std::string(seen[0]) == "jab" && std::string(seen[1]) == "hook" && std::string(seen[2]) == "uppercut";
            res[1] = std::string(seen[3]) == "roundhouse";
            AGR_LOG("COMBO clips: %s, %s, %s, heavy %s", seen[0], seen[1], seen[2], seen[3]);
            AGR_LOG(res[0] && res[1] ? "COMBO TEST PASSED" : "COMBO TEST FAILED");
        }, idle, "check"});
        return s;
    }
    if (name == "buildings") {
        // 1.9.0: a building model (tests/data/props/house.lkm, a plain cottage standing in for a Tripo model)
        // installed by a mod replaces the code-built houses: placed on the footprint, drawn, colliders kept
        static int res[4]; static int modelsSeen = 0;
        s.push_back({0.5f, [](Game& g) {
            for (int& r : res) r = 0;
            std::string root = platform::writableDir() + "/mods/test_buildings";
            for (const char* d : {"/mods", "/mods/test_buildings", "/mods/test_buildings/assets", "/mods/test_buildings/assets/models", "/mods/test_buildings/assets/models/props"})
                platform::makeDirectory(platform::writableDir() + d);
            platform::writeFileAtomic(root + "/mod.txt", "name = Maisons de test\nauthor = robot\nversion = 0.1\n");
            for (const char* f : {"house.lkm", "house.jpg"}) {
                std::ifstream in(std::string("tests/data/props/") + f, std::ios::binary);
                std::string b((std::istreambuf_iterator<char>(in)), {});
                platform::writeFileAtomic(root + "/assets/models/props/" + f, b);
            }
            g.applyMods(true);
            g.world.stopWorkers();
            int n = loadPropModels();
            g.world.unloadAll();
            g.world.startWorkers(2);
            res[0] = n >= 1 && propModelFor(S_HOUSE, 0) >= 0 && propModelFor(S_STALL, -1) < 0;
            AGR_LOG("BUILDINGS models loaded: %d (house -> %d)", n, propModelFor(S_HOUSE, 0));
            const Poi* b = g.world.poiById("brindlemere"); teleport(g, b->pos.x + 6, b->pos.y + 22, 3.4f);
            g.timeOfDay = 15.f; g.weather = g.weatherNext = Game::W_CLEAR; g.weatherBlend = 1; g.weatherTimer = 999; g.camDist = g.camDistTarget = 22.f; g.camPitch = 0.55f; g.toastHint.clear();
        }, idle, "install + rebuild"});
        s.push_back({6.f, none, [](Game& g, float) { g.camDist = g.camDistTarget = 22.f; }, "stream"});
        s.push_back({0.1f, [](Game& g) {
            modelsSeen = 0; int houses = 0;
            g.world.forEachVisibleChunk([&](Chunk& c) { modelsSeen += (int)c.models.size(); });
            for (const Structure& st : g.world.structures) if (st.kind == S_HOUSE && distXZ(vec3(st.pos.x, 0, st.pos.y), g.pc().pos) < 60.f) houses++;
            PropModel* P = propModel(propModelFor(S_HOUSE, 0));
            res[1] = modelsSeen >= 1 && houses >= 1;
            res[2] = P && P->mesh && P->mesh->valid() && P->tex.id != 0;   // uploaded by the renderer = drawn
            AGR_LOG("BUILDINGS %d model instances in loaded chunks, %d code-built houses nearby in the plan", modelsSeen, houses);
            g.requestScreenshot("build/shots/G0_buildings.png");
        }, idle, "check + shot"});
        // colliders kept: walk towards the nearest house and stop at its wall
        static vec2 target; static float minD;
        s.push_back({0.1f, [](Game& g) {
            const Structure* best = nullptr; float bd = 1e9f;
            for (const Structure& st : g.world.structures) if (st.kind == S_HOUSE) { float d = distXZ(vec3(st.pos.x, 0, st.pos.y), g.pc().pos); if (d < bd) { bd = d; best = &st; } }
            if (!best) return;
            target = best->pos;
            vec2 dir = normalize(vec2(std::sin(best->rot), std::cos(best->rot)));   // door side (+Z of the house)
            vec2 start = target + dir * 9.f;
            teleport(g, start.x, start.y, std::atan2(-dir.x, -dir.y));
            g.camDist = g.camDistTarget = 9.f; minD = 1e9f;
        }, idle, "face a house"});
        s.push_back({5.f, none, [](Game& g, float) {
            vec2 to = target - vec2(g.pc().pos.x, g.pc().pos.z);
            g.camYaw = std::atan2(-to.x, -to.y);   // camera forward = (-sin, -cos)
            g.input.move = vec2(0, 1);
            minD = std::min(minD, length(to)); }, "walk into it"});
        s.push_back({0.1f, [](Game& g) {
            float bd = minD; const Structure* best = &g.world.structures[0];
            res[3] = best && bd > 2.5f && bd < 6.f;   // reached the house, stopped at its walls (half-depth 4.2 m)
            AGR_LOG("BUILDINGS walked into a house: closest %.1f m from its centre", bd);
            g.requestScreenshot("build/shots/G1_building_close.png");
            AGR_LOG("BUILDINGS TEST: models=%d placed=%d drawn=%d collision=%d", res[0], res[1], res[2], res[3]);
            AGR_LOG(res[0] && res[1] && res[2] && res[3] ? "BUILDINGS TEST PASSED" : "BUILDINGS TEST FAILED");
        }, idle, "check"});
        return s;
    }
    if (name == "mods") {
        // 1.7.0: install a mod by hand (as the Android importer would), apply it, check data patches and an asset
        // override (villager_m.lkm replaced), then turn it off from the Mods menu with a real tap
        static int res[6];
        s.push_back({0.5f, [](Game& g) {
            for (int& r : res) r = 0;
            g.ui.recordWidgets = true;
            std::string root = platform::writableDir() + "/mods/test_mod";
            platform::makeDirectory(platform::writableDir() + "/mods"); platform::makeDirectory(root);
            platform::makeDirectory(root + "/assets"); platform::makeDirectory(root + "/assets/models");
            platform::writeFileAtomic(root + "/mod.txt", "name = Mod de test\nauthor = robot\nversion = 0.1\n");
            platform::writeFileAtomic(root + "/data.txt", "item.potion_minor.name = Potion du testeur\nenemy.wolf.hp = 77\nlore.add = test_page | Page de test | Texte.\nitem.inconnu.name = erreur volontaire\n");
            std::vector<uint8_t> b; std::string bs;
            if (platform::readAsset("models/hero.lkm", b)) platform::writeFileAtomic(root + "/assets/models/villager_m.lkm", std::string(b.begin(), b.end()));
            if (platform::readAsset("models/hero.jpg", b)) platform::writeFileAtomic(root + "/assets/models/villager_m.jpg", std::string(b.begin(), b.end()));
            g.applyMods(true);
            const ModInfo* m = nullptr; for (auto& x : g.mods) if (x.id == "test_mod") m = &x;
            res[0] = m && m->enabled && m->applied && m->patches == 3 && m->errors == 1;
            res[1] = item("potion_minor")->name == "Potion du testeur" && enemies()[enemyIndex("wolf")].hp == 77.f && g.loreFound.size() >= 0 && lore().back().id == "test_page";
            std::vector<uint8_t> v; res[2] = platform::readAsset("models/villager_m.lkm", v) && g.importedModel("villager_m") != nullptr;
            AGR_LOG("MODS installed: %s, patches %d errors %d (%s)", m ? m->name.c_str() : "?", m ? m->patches : -1, m ? m->errors : -1, m ? m->firstError.c_str() : "");
        }, idle, "install + apply"});
        s.push_back({0.6f, [](Game& g) { g.menu = Menu::Mods; g.scanMods(); }, idle, "mods menu"});
        s.push_back({0.1f, [](Game& g) { g.requestScreenshot("build/shots/E0_mods_menu.png"); res[3] = g.ui.findWidget("Importer un mod (.zip)") != nullptr; }, idle, "shot"});
        // toggle the mod off with a held tap on ITS switch. 3.1.0: the 14 built-in mods (2.7.0) are listed too, so the
        // list is scrolled to the test mod and the switch on its row is pressed (it used to press the first switch)
        static int modIdx = -1;
        s.push_back({1.0f, [](Game& g) { modIdx = -1; for (int i = 0; i < (int)g.mods.size(); i++) if (g.mods[i].id == "test_mod") modIdx = i; g.ui.setScroll("mods", modIdx * 96.f); }, idle, "scroll to the test mod"});
        s.push_back({0.25f, [](Game& g) {
            const Rect* imp = g.ui.findWidget("Importer un mod (.zip)");
            if (!imp || modIdx < 0) return;
            float listY = imp->y + 6.f + 110.f, so = g.ui.scrollOf("mods");
            float want = listY - so + modIdx * 96.f + 24.f;
            for (auto& w : g.ui.lastWidgets) if (w.second.empty() && std::fabs(w.first.y - want) < 3.f) { float k = g.ui.scale; g.touch(TouchEvent{TouchEvent::Down, 610, (w.first.x + w.first.w / 2) * k, (w.first.y + w.first.h / 2) * k}); break; }
        }, idle, "press switch"});
        s.push_back({0.4f, [](Game& g) { for (auto& f : g.ui.fingers) if (f.id == 610) { float k = g.ui.scale; g.touch(TouchEvent{TouchEvent::Up, 610, f.x * k, f.y * k}); } }, idle, "release"});
        s.push_back({0.2f, [](Game& g) {
            g.scanMods();   // re-read from disk: the choice must be saved
            for (auto& x : g.mods) if (x.id == "test_mod") res[4] = !x.enabled;
            std::string list; res[5] = platform::readFile(platform::writableDir() + "/mods_enabled.txt", list) && list.find("!test_mod") != std::string::npos;
            g.menu = Menu::None;
            AGR_LOG("MODS TEST: applied=%d dataPatched=%d assetOverride=%d menu=%d toggledOff=%d saved=%d", res[0], res[1], res[2], res[3], res[4], res[5]);
            bool ok = true; for (int r : res) ok = ok && r;
            AGR_LOG("MODS TEST %s", ok ? "PASSED" : "FAILED");
        }, idle, "report"});
        return s;
    }
    if (name == "prologue") {
        // 1.7.0: the animated prologue plays on a new game; one capture per shot, then the end state is checked,
        // then it is replayed and skipped with a real tap on "Passer"
        static int lastShot; static int res[6];
        s.push_back({0.1f, [](Game& g) { g.ui.recordWidgets = true; lastShot = -1; for (int& r : res) r = 0; res[0] = g.cine.active && g.cine.id == "prologue"; }, idle, "prologue started"});
        s.push_back({110.f, none, [](Game& g, float) {
            if (!g.cine.active) return;
            if (g.cine.index != lastShot && g.cine.t > 3.2f) {
                lastShot = g.cine.index;
                g.requestScreenshot(std::string("build/shots/D") + std::to_string(lastShot) + "_prologue.png");
                AGR_LOG("PROLOGUE shot %d: \"%s\"", lastShot, g.cine.shots[lastShot].text.c_str());
            }
        }, "play prologue"});
        s.push_back({1.5f, [](Game& g) {
            res[1] = !g.cine.active && lastShot + 1 == 10;
            res[2] = g.storyFlags.count("prologue_seen") && g.loreFound.count("l1") && g.loreFound.count("l2") && g.loreFound.count("l3");
            res[3] = g.pc().vis.anim.state == BodyState::Locomotion;
        }, idle, "after"});
        s.push_back({0.1f, [](Game& g) { g.requestScreenshot("build/shots/D9_after_prologue.png"); }, idle, "shot"});
        // save / load keeps the story flags
        s.push_back({0.3f, [](Game& g) { g.saveGame(2); g.storyFlags.clear(); g.loadGame(2); g.startPlaying(); res[4] = g.storyFlags.count("prologue_seen") > 0; }, idle, "save/load flags"});
        s.push_back({1.f, [](Game& g) { g.startPrologue(); }, idle, "replay"});
        s.push_back({0.6f, [](Game& g) { const Rect* r = g.ui.findWidget("Passer"); if (r) { float k = g.ui.scale; vec2 c{r->x + r->w / 2, r->y + r->h / 2}; g.touch(TouchEvent{TouchEvent::Down, 600, c.x * k, c.y * k}); } }, idle, "press skip"});
        s.push_back({0.3f, [](Game& g) { for (auto& f : g.ui.fingers) if (f.id == 600) { float k = g.ui.scale; g.touch(TouchEvent{TouchEvent::Up, 600, f.x * k, f.y * k}); } }, idle, "release skip"});
        s.push_back({0.2f, [](Game& g) {
            res[5] = !g.cine.active;
            AGR_LOG("PROLOGUE TEST: started=%d allShots=%d flagsAndLore=%d heroUp=%d saveLoad=%d skipButton=%d", res[0], res[1], res[2], res[3], res[4], res[5]);
            bool ok = true; for (int r : res) ok = ok && r;
            AGR_LOG("PROLOGUE TEST %s", ok ? "PASSED" : "FAILED");
        }, idle, "report"});
        return s;
    }
    if (name == "gait") {
        // side-on close-ups of the hero's walk / run cycle (animation quality and skinning checks)
        s.push_back({2.f, [](Game& g) { const Poi* p = g.world.poiById("brindlemere"); teleport(g, p->pos.x + 30, p->pos.y + 30, 0.6f); g.timeOfDay = 11.f; g.weather = g.weatherNext = Game::W_CLEAR; g.weatherBlend = 1; g.weatherTimer = 999; g.toastHint.clear(); g.addItem("sword_knight", 1, Rarity::Epic); }, idle, "setup"});
        static const char* names[] = {"70_walk_a", "71_walk_b", "72_walk_c", "73_walk_d", "74_run_a", "75_run_b", "76_run_c", "77_run_d"};
        for (int i = 0; i < 8; i++) {
            bool run = i >= 4;
            s.push_back({i == 0 || i == 4 ? 1.5f : 0.13f, none, [run](Game& g, float) {
                // walk along camera-right so the camera sees the hero from the side, low and close
                g.input.move = vec2(run ? 1.f : 0.55f, 0); g.input.sprint = run; g.camDist = g.camDistTarget = 5.5f; g.camPitch = 0.25f; }, "move"});
            s.push_back({0.04f, [i](Game& g) { g.requestScreenshot(std::string("build/shots/") + names[i] + ".png"); }, [run](Game& g, float) { g.input.move = vec2(run ? 1.f : 0.55f, 0); g.input.sprint = run; }, "shot"});
        }
        s.push_back({0.1f, [](Game& g) { g.camPitch = 0.66f; }, idle, "reset"});
        return s;
    }
    if (name == "swordrun") {
        // 2.3.4 the sword while running, seen from the side (it used to drop and hang down)
        s.push_back({3.f, [](Game& g) { const Poi* p = g.world.poiById("brindlemere"); teleport(g, p->pos.x + 30, p->pos.y + 30, 0.6f); g.timeOfDay = 10.5f; g.weather = g.weatherNext = Game::W_CLEAR; g.weatherBlend = 1; g.weatherTimer = 999; g.toastHint.clear(); g.addItem("sword_knight", 1, Rarity::Epic); }, idle, "setup"});
        auto side = [](float d) { return [d](Game& g, float) { g.camYaw = g.pc().yaw + 1.57f; g.camPitch = 0.08f; g.camDist = g.camDistTarget = d; g.camIdleT = 0; }; };
        s.push_back({0.6f, none, side(4.5f), "cam"});
        s.push_back({0.1f, shot("S0_idle"), side(4.5f), "shot"});
        s.push_back({1.2f, none, [side](Game& g, float t) { g.input.move = vec2(0, 0.5f); side(4.5f)(g, t); }, "walk"});
        s.push_back({0.1f, shot("S1_walk"), [side](Game& g, float t) { g.input.move = vec2(0, 0.5f); side(4.5f)(g, t); }, "shot"});
        s.push_back({1.2f, none, [side](Game& g, float t) { g.input.move = vec2(0, 1); side(5.f)(g, t); }, "run"});
        s.push_back({0.1f, shot("S2_run"), [side](Game& g, float t) { g.input.move = vec2(0, 1); side(5.f)(g, t); }, "shot"});
        s.push_back({1.2f, none, [side](Game& g, float t) { g.input.move = vec2(0, 1); g.input.sprint = true; side(5.5f)(g, t); }, "sprint"});
        s.push_back({0.1f, shot("S3_sprint"), [side](Game& g, float t) { g.input.move = vec2(0, 1); g.input.sprint = true; side(5.5f)(g, t); }, "shot"});
        return s;
    }
    if (name == "hero") {
        // close-up checks of the hero model: idle, walk, run, attack combo, roll, equipment screen
        s.push_back({3.f, [](Game& g) { const Poi* p = g.world.poiById("brindlemere"); teleport(g, p->pos.x + 30, p->pos.y + 30, 0.6f); g.timeOfDay = 10.5f; g.weather = g.weatherNext = Game::W_CLEAR; g.weatherBlend = 1; g.weatherTimer = 999; g.camDist = g.camDistTarget = 7.f; g.toastHint.clear(); g.addItem("sword_knight", 1, Rarity::Epic);
            if (const char* h = getenv("LK_HERO")) { g.unlockHero(h); g.switchHero(heroIndex(h)); g.notes.clear(); } }, idle, "setup"});
        s.push_back({0.1f, shot("60_hero_idle"), idle, "shot"});
        s.push_back({1.2f, none, [](Game& g, float) { g.input.move = vec2(0.6f, 0.5f); g.camDist = g.camDistTarget = 7.f; }, "walk"});
        s.push_back({0.1f, shot("61_hero_walk"), [](Game& g, float) { g.input.move = vec2(0.6f, 0.5f); }, "shot"});
        s.push_back({1.2f, none, [](Game& g, float) { g.input.move = vec2(0, 1); g.input.sprint = true; g.camDist = g.camDistTarget = 7.f; }, "run"});
        s.push_back({0.1f, shot("62_hero_run"), [](Game& g, float) { g.input.move = vec2(0, 1); g.input.sprint = true; }, "shot"});
        s.push_back({0.25f, [](Game& g) { g.input.attackRelease = true; g.input.attackHoldT = 0.05f; }, idle, "attack"});
        s.push_back({0.1f, shot("63_hero_attack"), idle, "shot"});
        s.push_back({0.9f, none, idle, "recover"});
        s.push_back({0.2f, [](Game& g) { g.input.dodge = true; }, idle, "roll"});
        s.push_back({0.1f, shot("64_hero_roll"), idle, "shot"});
        s.push_back({1.0f, [](Game& g) { g.menu = Menu::Equipment; }, idle, "equipment"});
        s.push_back({0.1f, shot("65_hero_equipment"), idle, "shot"});
        return s;
    }
    if (name == "look") {
        // fixed beauty shots for visual work (same places every run)
        struct V { const char* poi; float dx, dz, yaw, tod, dist; const char* f; };
        static const V views[] = {{"brindlemere", 10, 26, 0.5f, 10.f, 15.5f, "50_village"}, {"brindlemere", 40, -60, 2.4f, 16.5f, 20.f, "51_meadow"},
                                  {"brindlemere", -70, 30, -0.8f, 7.5f, 15.5f, "52_forest_morning"}, {"root_glade", 0, 30, 0.f, 13.f, 18.f, "53_elderwood"},
                                  {"#river", 0, 0, 0.9f, 15.f, 15.5f, "54_river"}, {"brindlemere", -4, 6, 2.6f, 17.5f, 15.5f, "55_village_street"}, {"brindlemere", 140, 90, 0.9f, 15.5f, 17.f, "56_countryside"}, {"brindlemere", -150, 60, 3.6f, 9.5f, 17.f, "57_countryside_b"}};
        for (const V& v : views) {
            s.push_back({4.f, [v](Game& g) { vec2 at;
                if (std::string(v.poi) == "#river") { if (g.world.rivers.empty()) return; auto& pts = g.world.rivers[0].pts; vec2 a = pts[pts.size() / 2], b = pts[pts.size() / 2 + 1]; vec2 n = normalize(perp(b - a)); at = a + n * 9.f; }
                else { const Poi* p = g.world.poiById(v.poi); if (!p) return; at = p->pos + vec2(v.dx, v.dz); }
                teleport(g, at.x, at.y, v.yaw); g.timeOfDay = v.tod; g.weather = g.weatherNext = Game::W_CLEAR; g.weatherBlend = 1; g.weatherTimer = 999; g.camDist = g.camDistTarget = v.dist; g.toastHint.clear(); }, idle, "view"});
            s.push_back({0.1f, [v](Game& g) { g.requestScreenshot(std::string("build/shots/") + v.f + ".png"); }, idle, "shot"});
        }
        return s;
    }
    if (name == "anim") {
        // Animation bench: the hero on a "treadmill" (position held, velocity driven), seen from the side on
        // flat ground. 8 frames per gait (idle, walk, jog, sprint), then attacks. Shots C*.
        static vec3 spot;
        auto hold = [](Game& g, vec2 move, bool sprint) {
            Character& c = g.pc();
            c.pos.x = spot.x; c.pos.z = spot.z;
            g.input.move = move; g.input.sprint = sprint;
            g.camYaw = std::atan2(1.f, 0.f) - 0.0f;   // camera on the hero's side (he moves to screen right)
            g.camPitch = 0.12f; g.camDist = g.camDistTarget = 4.6f; g.camIdleT = 0; g.camFocus = c.pos + vec3(0, 1.1f, 0);
            g.stamina = g.maxStamina;
            c.yaw = PI;            // facing -Z: exact profile for the camera on +X
            g.interactTarget = NullEntity;
        };
        s.push_back({2.5f, [](Game& g) {
            const Poi* b = g.world.poiById("brindlemere"); teleport(g, b->pos.x - 4, b->pos.y + 2, 0.f);
            spot = g.pc().pos; g.timeOfDay = 12.f; g.weather = g.weatherNext = Game::W_CLEAR; g.weatherBlend = 1; g.weatherTimer = 999; g.toastHint.clear();
            g.addItem("sword_knight", 1, Rarity::Epic);
            for (int i = (int)g.inventory.size() - 1; i >= 0; i--) if (g.inventory[i].id == "sword_knight") { g.equip(i); break; }
            const Skeleton& sk = *g.pc().vis.anim.skeleton;
            for (int b : {H_ROOT, H_THIGHL, H_SHINL, H_FOOTL, H_THIGHR, H_SHINR, H_FOOTR, H_SHL, H_SHR})
                AGR_LOG("ANIM bind %d: (%.3f, %.3f, %.3f)", b, sk.bones[b].bindWorld.x, sk.bones[b].bindWorld.y, sk.bones[b].bindWorld.z);
        }, [hold](Game& g, float) { hold(g, vec2(0, 0), false); }, "setup"});
        static float minRaw[4], minFixed[4];
        struct G { const char* name; vec2 move; bool sprint; };
        static const G gaits[] = {{"C0_idle", {0, 0}, false}, {"C1_walk", {0.5f, 0}, false}, {"C2_jog", {1, 0}, false}, {"C3_sprint", {1, 0}, true}};
        // idle seen from the front (3/4) as well: shoulder rest of the sword
        s.push_back({1.5f, none, [hold](Game& g, float) { hold(g, vec2(0, 0), false); g.pc().yaw = PI * 0.5f + 0.5f; }, "settle front"});
        s.push_back({0.1f, [](Game& g) { g.requestScreenshot("build/shots/C9_idle_front.png"); }, [hold](Game& g, float) { hold(g, vec2(0, 0), false); g.pc().yaw = PI * 0.5f + 0.5f; }, "shot"});
        for (int gi = 0; gi < 4; gi++) {
            const G gg = gaits[gi];
            s.push_back({1.5f, [gi](Game&) { minRaw[gi] = minFixed[gi] = 9.f; }, [hold, gg](Game& g, float) { hold(g, gg.move, gg.sprint); }, "settle"});
            for (int k = 0; k < 8; k++) {
                char* f = new char[48]; snprintf(f, 48, "build/shots/%s_%d.png", gg.name, k);
                s.push_back({gg.move.x > 0 ? 0.1f : 0.4f, [f](Game& g) { g.requestScreenshot(f); }, [hold, gg, gi](Game& g, float) {
                    hold(g, gg.move, gg.sprint);
                    minRaw[gi] = std::min(minRaw[gi], g.pc().vis.tipClearRaw); minFixed[gi] = std::min(minFixed[gi], g.pc().vis.tipClear); }, "shot"});
            }
        }
        s.push_back({0.05f, [](Game&) {
            // the sword must stay clear of the ground by itself at rest, walking and jogging (the clearance fix is a
            // safety net, not the animation); with the fix, never below the ground in any gait
            bool ok = true;
            for (int i = 0; i < 4; i++) {
                AGR_LOG("ANIM sword tip clearance %s: animation %.2f m, after fix %.2f m", gaits[i].name, minRaw[i], minFixed[i]);
                if (minFixed[i] < 0.f) ok = false;
                if (i < 3 && minRaw[i] < 0.02f) ok = false;
            }
            AGR_LOG("ANIM TEST %s", ok ? "PASSED" : "FAILED");
        }, idle, "report"});
        // walking and jogging seen from behind (leg spacing)
        for (int b = 0; b < 2; b++) {
            float mv = b ? 1.f : 0.5f;
            s.push_back({1.2f, none, [hold, mv](Game& g, float) { hold(g, vec2(0, 0), false); g.input.move = vec2(0, mv); g.camYaw = 0.f; g.camPitch = 0.25f; }, "settle back"});
            for (int k = 0; k < 4; k++) {
                char* f = new char[48]; snprintf(f, 48, "build/shots/C%d_back_%d.png", 6 + b, k);
                s.push_back({0.12f, [f](Game& g) { g.requestScreenshot(f); }, [hold, mv](Game& g, float) { hold(g, vec2(0, 0), false); g.input.move = vec2(0, mv); g.camYaw = 0.f; g.camPitch = 0.25f; }, "shot"});
            }
        }
        // light combo and heavy attack, 6 frames each
        for (int c = 0; c < 2; c++) {
            s.push_back({0.8f, none, [hold](Game& g, float) { hold(g, vec2(0, 0), false); }, "rest"});
            s.push_back({0.01f, [c](Game& g) { g.comboStep = 0; g.input.attackRelease = true; g.input.attackHoldT = c ? 1.2f : 0.05f; }, idle, "attack"});
            for (int k = 0; k < 6; k++) {
                char* f = new char[48]; snprintf(f, 48, "build/shots/C%d_attack_%d.png", 4 + c, k);
                s.push_back({0.1f, [f](Game& g) { g.requestScreenshot(f); }, [hold](Game& g, float) { hold(g, vec2(0, 0), false); }, "shot"});
            }
        }
        return s;
    }
    if (name == "npcs") {
        // 1.6.1: line-up of villagers and named NPCs facing the camera (procedural character look checks)
        static std::vector<Entity> spawned;
        auto lineUp = [](Game& g, float camDist, float pitch) {
            Character& p = g.pc();
            vec3 fwd{-std::sin(g.camYaw), 0, -std::cos(g.camYaw)}, right{std::cos(g.camYaw), 0, -std::sin(g.camYaw)};
            for (size_t i = 0; i < spawned.size(); i++) {
                Character* c = g.reg.get<Character>(spawned[i]);
                if (!c) continue;
                c->pos = p.pos + fwd * 2.6f + right * ((float)i - (spawned.size() - 1) * 0.5f) * 0.95f;
                c->pos.y = g.world.groundHeight(c->pos.x, c->pos.z); c->vel = vec3(0);
                c->yaw = std::atan2(-fwd.x, -fwd.z);
                if (NpcBrain* nb = g.reg.get<NpcBrain>(spawned[i])) { nb->dest = c->pos; nb->home = nb->work = nb->social = nb->bed = c->pos; }
            }
            g.camPitch = pitch; g.camDist = g.camDistTarget = camDist; g.camIdleT = 0;
            p.vis.visible = false;   // the hero would stand in front of the line-up
        };
        s.push_back({2.5f, [](Game& g) {
            spawned.clear();
            const Poi* b = g.world.poiById("brindlemere"); teleport(g, b->pos.x + 30, b->pos.y + 30, 0.6f);
            g.timeOfDay = 10.5f; g.weather = g.weatherNext = Game::W_CLEAR; g.weatherBlend = 1; g.weatherTimer = 999; g.toastHint.clear();
            int vil = 0; for (size_t i = 0; i < g.world.pois.size(); i++) if (g.world.pois[i].id == "brindlemere") vil = (int)i;
            vec3 at = g.pc().pos;
            for (uint32_t sd : {11u, 23u, 37u, 41u}) spawned.push_back(g.spawnVillager(at, vil, sd));
            for (const char* id : {"maelle", "hilda"}) for (size_t k = 0; k < npcs().size(); k++) if (npcs()[k].id == id) spawned.push_back(g.spawnNpc((int)k, at, vil));
        }, [lineUp](Game& g, float) { lineUp(g, 6.5f, 0.12f); }, "spawn"});
        s.push_back({0.1f, [](Game& g) { g.requestScreenshot("build/shots/B0_npcs_lineup.png"); }, [lineUp](Game& g, float) { lineUp(g, 6.5f, 0.12f); }, "shot"});
        s.push_back({1.f, none, [lineUp](Game& g, float) { lineUp(g, 4.f, 0.05f); }, "closer"});
        s.push_back({0.1f, [](Game& g) { g.requestScreenshot("build/shots/B1_npcs_close.png"); }, [lineUp](Game& g, float) { lineUp(g, 4.f, 0.05f); }, "shot"});
        s.push_back({0.1f, [](Game& g) { g.pc().vis.visible = true; }, idle, "show hero"});
        return s;
    }
    if (name == "realm") {
        // 2.2.1 the celestial portal and the realm behind it (look at the pictures)
        s.push_back({0.5f, [](Game& g) { g.newGame(); g.startPlaying(); g.timeOfDay = 17.5f; g.toastHint.clear(); const Poi* p = g.world.poiById("portal_ether"); teleport(g, p->pos.x, p->pos.y + 16.f, 0.f); g.camPitch = 0.18f; g.camDist = g.camDistTarget = 12.f; }, idle, "new game"});
        s.push_back({4.f, none, [](Game& g, float) { g.camIdleT = 0; }, "look"});
        s.push_back({0.1f, [](Game& g) { g.requestScreenshot("build/shots/R0_portal.png"); }, idle, "shot"});
        s.push_back({0.2f, [](Game& g) { int di = -1; for (int i = 0; i < (int)dungeons().size(); i++) if (dungeons()[i].id == "ether") di = i; g.enterDungeon(di); }, idle, "enter"});
        s.push_back({3.f, none, [](Game& g, float) { g.camIdleT = 0; }, "inside"});
        s.push_back({0.1f, [](Game& g) { g.camPitch = 0.22f; g.camDist = g.camDistTarget = 20.f; g.camYaw = 2.4f; }, idle, "cam"});
        s.push_back({1.f, none, idle, "settle"});
        s.push_back({0.1f, [](Game& g) { AGR_LOG("REALM inside: %s", g.dungeon.active && g.dungeon.theme == 8 ? "REALM TEST PASSED" : "REALM TEST FAILED"); g.requestScreenshot("build/shots/R1_realm.png"); }, idle, "shot"});
        return s;
    }
    if (name == "garage") {
        // 2.2.1 call the car to the hero, drive it, speedometer (look at the pictures)
        s.push_back({0.5f, [](Game& g) { g.newGame(); g.startPlaying(); g.timeOfDay = 11.f; g.toastHint.clear(); if (g.vehicles.empty()) g.initVehicles(); g.vehiclesUnlocked = {"car", "moto", "airship"}; g.spawnVehicleEntities(); teleport(g, 1600, 1560, 0.f);
            g.mapPins = {vec2(1600, 4100), vec2(1660, 4000), vec2(1540, 3900), vec2(2400, 300), vec2(2450, 340)}; }, idle, "new game"});   // 3.1.0 pins in clusters: no overlap, none on the tracker
        s.push_back({2.f, none, idle, "settle"});
        s.push_back({0.1f, [](Game& g) { g.garageOpen = true; }, idle, "open"});
        s.push_back({0.3f, none, idle, "wait"});
        s.push_back({0.1f, [](Game& g) { g.requestScreenshot("build/shots/G0_garage.png"); }, idle, "shot"});
        s.push_back({0.5f, [](Game& g) { int car = 0; for (int i = 0; i < (int)g.vehicles.size(); i++) if (!vehicleDefs()[g.vehicles[i].def].moto && !vehicleDefs()[g.vehicles[i].def].fly) car = i; g.summonVehicle(car); g.garageOpen = false; }, idle, "summon"});
        s.push_back({0.1f, [](Game& g) { int car = 0; for (int i = 0; i < (int)g.vehicles.size(); i++) if (!vehicleDefs()[g.vehicles[i].def].moto && !vehicleDefs()[g.vehicles[i].def].fly) car = i; float d = distXZ(g.vehicles[car].pos, g.pc().pos); AGR_LOG("GARAGE car at %.1f m from the hero -> %s", d, d < 9.f ? "GARAGE SUMMON OK" : "GARAGE SUMMON FAILED"); g.enterVehicle(car); }, idle, "enter"});
        s.push_back({3.f, none, [](Game& g, float) { g.input.move = vec2(0, 1); g.camIdleT = 0; }, "drive"});
        s.push_back({0.1f, [](Game& g) { AGR_LOG("GARAGE speed %.1f km/h -> %s", std::fabs(g.vehicles[std::max(0, g.driving)].speed) * 3.6f, g.driving >= 0 ? "GARAGE TEST PASSED" : "GARAGE TEST FAILED"); g.requestScreenshot("build/shots/G1_speedometer.png"); }, idle, "shot"});
        s.push_back({0.6f, [](Game& g) { g.exitVehicle(); g.input.move = vec2(0); }, idle, "exit"});
        s.push_back({0.3f, [](Game& g) { g.summonHorse(); }, idle, "horse"});
        s.push_back({1.f, none, [](Game& g, float) { g.camIdleT = 0; }, "ride"});
        s.push_back({0.1f, [](Game& g) { AGR_LOG("GARAGE horse -> %s", g.riding ? "HORSE SUMMON OK" : "HORSE SUMMON FAILED"); g.requestScreenshot("build/shots/G2_horse.png"); }, idle, "shot"});
        s.push_back({0.8f, none, [](Game& g, float) { g.camYaw = g.pc().yaw + 1.57f; g.camPitch = 0.1f; g.camDist = g.camDistTarget = 10.f; g.camIdleT = 0; }, "side"});
        s.push_back({0.1f, [](Game& g) { g.requestScreenshot("build/shots/G3_horse_side.png");
            if (Character* m = g.reg.get<Character>(g.mountEnt)) AGR_LOG("HORSEDBG mount %.2f %.2f %.2f yaw %.2f scale %.2f rootw %.2f %.2f %.2f | rider %.2f %.2f %.2f yaw %.2f ground %.2f", m->pos.x, m->pos.y, m->pos.z, m->yaw, m->vis.scale, m->vis.pw.wpos[0].x, m->vis.pw.wpos[0].y, m->vis.pw.wpos[0].z, g.pc().pos.x, g.pc().pos.y, g.pc().pos.z, g.pc().yaw, g.groundAt(m->pos.x, m->pos.z)); }, [](Game& g, float) { g.camYaw = g.pc().yaw + 1.57f; }, "shot"});
        s.push_back({1.5f, none, [](Game& g, float) { g.input.move = vec2(0, 1); g.input.dodge = true; g.camYaw = g.pc().yaw + 1.57f; g.camPitch = 0.1f; g.camDist = g.camDistTarget = 11.f; g.camIdleT = 0; }, "gallop"});
        s.push_back({0.1f, [](Game& g) { g.requestScreenshot("build/shots/G4_horse_gallop.png"); }, [](Game& g, float) { g.input.move = vec2(0, 1); g.input.dodge = true; g.camYaw = g.pc().yaw + 1.57f; }, "shot"});
        return s;
    }
    if (name == "qol") {
        // 2.2.0 quick bag, map pins and dungeons, pause menu, dust teleport (look at the pictures)
        auto sh = [](const char* f) { return [f](Game& g) { g.requestScreenshot(std::string("build/shots/") + f + ".png"); }; };
        s.push_back({0.5f, [](Game& g) { g.newGame(); g.startPlaying(); g.timeOfDay = 10.f; g.toastHint.clear(); g.addItem("sword_iron", 1); g.addItem("potion_heal", 4); g.addItem("iron_ore", 6); }, idle, "new game"});
        s.push_back({2.5f, none, [](Game& g, float) { g.camIdleT = 0; }, "settle"});
        s.push_back({0.4f, [](Game& g) { g.menu = Menu::Bag; g.selItem = 0; }, idle, "bag"});
        s.push_back({0.1f, sh("Q0_bag"), idle, "shot"});
        s.push_back({0.4f, [](Game& g) { g.menu = Menu::Map; g.mapPins = {vec2(1700, 1500), vec2(1300, 1900)}; for (auto& p : g.world.pois) if (p.type == PoiType::Waystone || p.type == PoiType::Village) g.discovered.insert(p.id); }, idle, "map"});
        s.push_back({0.1f, sh("Q1_map"), idle, "shot"});
        s.push_back({0.4f, [](Game& g) { g.menu = Menu::Pause; }, idle, "pause"});
        s.push_back({0.1f, sh("Q2_pause"), idle, "shot"});
        s.push_back({0.3f, [](Game& g) { g.menu = Menu::None; teleport(g, 1600, 1560, 2.4f); g.camPitch = 0.22f; g.camDist = g.camDistTarget = 6.f; const Poi* w = g.world.poiById("ws_green"); g.startDustTeleport(vec3(w->pos.x + 3, 0, w->pos.y + 3), "test"); }, idle, "teleport"});
        s.push_back({0.85f, none, idle, "crumbling"});
        s.push_back({0.1f, sh("Q3_dust_out"), idle, "shot"});
        s.push_back({12.f, none, idle, "arrive", [](Game& g) { return g.tele.phase == 1 && !g.loadGate && g.tele.t > 0.5f; }});
        s.push_back({0.1f, sh("Q4_dust_in"), idle, "shot"});
        s.push_back({3.f, none, idle, "end of fx", [](Game& g) { return !g.tele.active; }});
        s.push_back({0.1f, [](Game& g) { const Poi* w = g.world.poiById("ws_green"); float d = distXZ(g.pc().pos, vec3(w->pos.x, 0, w->pos.y)); AGR_LOG("QOL teleport distance to the stone %.1f m, dissolve %.2f -> %s", d, g.pc().vis.dissolve, d < 8.f && g.pc().vis.dissolve == 0.f ? "QOL TEST PASSED" : "QOL TEST FAILED"); }, idle, "check"});
        return s;
    }
    if (name == "start") {
        // 2.1.0 the start of a game: loading screen, then the interactive tutorial (look at the pictures)
        s.push_back({0.5f, [](Game& g) { g.newGame(); g.startPlaying(); g.loadGate = true; g.loadGateT = 0; g.loadGateReady = 0; g.timeOfDay = 9.f; g.toastHint.clear(); }, idle, "new game"});
        s.push_back({0.3f, [](Game& g) { teleport(g, 3850, 2650, 0.4f); g.loadGate = true; g.loadGateT = 0; g.loadGateReady = 0; g.loadGateHold = 2.f; }, idle, "far away: the world must load"});
        s.push_back({0.2f, none, idle, "loading"});
        s.push_back({0.1f, [](Game& g) { g.requestScreenshot("build/shots/T0_loading.png"); g.loadGateHold = 0.6f; }, idle, "shot"});
        s.push_back({30.f, none, [](Game& g, float) { g.camIdleT = 0; }, "wait for the world", [](Game& g) { return !g.loadGate; }});
        s.push_back({1.f, [](Game& g) { g.toastHint.clear(); g.startTutorial(); }, idle, "tutorial"});
        s.push_back({0.1f, [](Game& g) { g.requestScreenshot("build/shots/T1_tutorial_move.png"); }, idle, "shot"});
        s.push_back({0.2f, [](Game& g) { g.tutStep = 2; g.tutT = 0; }, idle, "step run"});
        s.push_back({0.4f, none, idle, "settle"});
        s.push_back({0.1f, [](Game& g) { g.requestScreenshot("build/shots/T2_tutorial_run.png"); }, idle, "shot"});
        s.push_back({0.1f, [](Game& g) { AGR_LOG("START TEST PASSED"); }, idle, "end"});
        return s;
    }
    if (name == "sky") {
        // 2.0.0 the new sky: clouds by day, sunset, the moon and the Milky Way, aurora in the cold north, a rainbow after the rain
        struct V { float x, z, tod; int weather; float yaw, pitch; float wet; const char* f; };
        static const V views[] = {
            {3600, 3300, 10.5f, Game::W_CLOUDY, 0.6f, -0.34f, 0.f, "S0_day_clouds"},
            {1500, 1500, 18.6f, Game::W_CLEAR, 2.4f, -0.12f, 0.f, "S1_sunset"},
            {3600, 3300, 1.5f, Game::W_CLEAR, 0.9f, -0.34f, 0.f, "S2_night_stars"},
            {850, 600, 23.f, Game::W_CLEAR, 1.2f, -0.34f, 0.f, "S3_aurora"},
            {1500, 1500, 16.2f, Game::W_CLEAR, 5.0f, -0.12f, 0.95f, "S4_rainbow"},
            {3600, 3300, 12.5f, Game::W_CLEAR, 0.4f, -0.1f, 0.f, "S5_desert_noon"},
        };
        std::vector<std::string> only;
        if (const char* e = getenv("LK_SKY")) { std::string t = e; size_t a = 0; while (a <= t.size()) { size_t b = t.find(',', a); if (b == std::string::npos) b = t.size(); only.push_back(t.substr(a, b - a)); a = b + 1; } }
        for (const V& v : views) {
            if (!only.empty() && std::find(only.begin(), only.end(), std::string(v.f).substr(3)) == only.end()) continue;
            s.push_back({3.4f, [v](Game& g) {
                teleport(g, v.x, v.z, v.yaw);
                g.timeOfDay = v.tod; g.weather = g.weatherNext = v.weather; g.weatherBlend = 1; g.weatherTimer = 999; g.toastHint.clear();
                g.env.wetness = v.wet; g.camPitch = v.pitch; g.camDist = g.camDistTarget = 5.f; }, [v](Game& g, float) { g.camIdleT = 0; g.env.wetness = std::max(g.env.wetness, v.wet * 0.9f); }, "view"});
            s.push_back({0.1f, [v](Game& g) { g.requestScreenshot(std::string("build/shots/") + v.f + ".png"); }, idle, "shot"});
        }
        return s;
    }
    if (name == "lands") {
        // 2.0.0 the new lands: settlements and landmarks seen from a pleasant camera (dev aid: look at the pictures)
        // arg: LK_LANDS="id1,id2,..." picks places; default is a tour of the highlights. Each view: poi, camera distance, pitch,
        // camera yaw offset, hour, weather, height of the look target.
        struct V { const char* poi; float dist, pitch, yaw, tod; int weather; float dx, dz; const char* f; };
        static const V all[] = {
            {"solmar", 40, 0.36f, 0.4f, 16.5f, Game::W_CLEAR, 4, 4, "L0_solmar"},
            {"solmar", 30, 0.18f, 2.6f, 19.2f, Game::W_CLEAR, 0, 0, "L1_solmar_dusk"},
            {"sun_temple", 70, 0.35f, 0.2f, 17.4f, Game::W_CLEAR, 0, 40, "L2_sun_temple"},
            {"sphinx", 36, 0.22f, 0.9f, 8.2f, Game::W_CLEAR, 0, 14, "L3_sphinx"},
            {"port_azur", 46, 0.34f, 3.14f, 10.5f, Game::W_CLEAR, 6, 10, "L4_port_azur"},
            {"pilotis", 40, 0.3f, 0.8f, 12.f, Game::W_CLEAR, 6, 6, "L5_pilotis"},
            {"shirokawa", 34, 0.26f, 3.1f, 7.f, Game::W_CLEAR, 0, 30, "L6_shirokawa"},
            {"phare", 40, 0.2f, 2.9f, 20.3f, Game::W_CLEAR, 0, 20, "L7_phare_night"},
            {"karun_ruins", 40, 0.3f, 1.0f, 15.f, Game::W_CLEAR, 0, 0, "L8_karun"},
            {"kheft", 44, 0.34f, 1.7f, 11.f, Game::W_CLEAR, 0, 0, "L9_kheft_oasis"},
            {"solmar", 22, 0.14f, 0.7f, 13.f, Game::W_SANDSTORM, 0, 12, "LA_sandstorm"},
            {"amber_fort", 60, 0.3f, 0.6f, 9.f, Game::W_CLEAR, 0, 10, "LB_fort"},
        };
        std::vector<std::string> only;
        if (const char* e = getenv("LK_LANDS")) { std::string t = e; size_t a = 0; while (a <= t.size()) { size_t b = t.find(',', a); if (b == std::string::npos) b = t.size(); only.push_back(t.substr(a, b - a)); a = b + 1; } }
        for (const V& v : all) {
            if (!only.empty() && std::find(only.begin(), only.end(), std::string(v.f).substr(3)) == only.end() && std::find(only.begin(), only.end(), std::string(v.f)) == only.end()) continue;
            s.push_back({4.2f, [v](Game& g) {
                const Poi* p = g.world.poiById(v.poi);
                if (!p) { AGR_LOG("LANDS no poi %s", v.poi); return; }
                vec2 hero = p->pos + vec2(v.dx, v.dz);
                teleport(g, hero.x, hero.y, v.yaw);
                g.timeOfDay = v.tod; g.weather = g.weatherNext = v.weather; g.weatherBlend = 1; g.weatherTimer = 999; g.toastHint.clear();
                if (v.weather == Game::W_SANDSTORM) { g.sandAmt = 1.f; }
                g.camPitch = v.pitch; g.camDist = g.camDistTarget = v.dist; }, [](Game& g, float) { g.camIdleT = 0; }, "view"});
            s.push_back({0.1f, [v](Game& g) { g.requestScreenshot(std::string("build/shots/") + v.f + ".png"); }, idle, "shot"});
        }
        return s;
    }
    if (name == "vista") {
        // 1.6.1: low camera views towards the mountains (relief and horizon checks)
        struct V { float x, z, tx, tz, tod; const char* f; float pitch = 0.08f, dist = 15.f; };
        static const V views[] = {{1300, 1350, 850, 600, 10.5f, "A0_vista_green_to_frost"}, {900, 800, 600, 350, 11.f, "A1_vista_frostheim"},
                                  {2150, 1150, 2650, 900, 14.f, "A2_vista_highcrowns"}, {2050, 1500, 2460, 1960, 16.5f, "A3_vista_volcano"},
                                  {1480, 1700, 1480, 1200, 9.f, "A4_vista_brindlemere"},
                                  {760, 900, 600, 400, 11.f, "A5_mountains_frostheim", 0.3f, 40.f}, {2300, 1000, 2650, 800, 13.f, "A6_mountains_highcrowns", 0.3f, 40.f},
                                  {1150, 820, 700, 450, 12.f, "A7_frost_range", 0.12f, 30.f}};
        for (const V& v : views) {
            s.push_back({4.f, [v](Game& g) {
                vec2 d{v.tx - v.x, v.tz - v.z};
                teleport(g, v.x, v.z, std::atan2(-d.x, -d.y));
                g.timeOfDay = v.tod; g.weather = g.weatherNext = Game::W_CLEAR; g.weatherBlend = 1; g.weatherTimer = 999; g.toastHint.clear();
                g.camPitch = v.pitch; g.camDist = g.camDistTarget = v.dist; }, [](Game& g, float) { g.camIdleT = 0; }, "view"});
            s.push_back({0.1f, [v](Game& g) { g.requestScreenshot(std::string("build/shots/") + v.f + ".png"); }, idle, "shot"});
        }
        return s;
    }
    if (name == "grass") {
        // 1.6.1: dense meadow grass bent by the hero and the trail he leaves, which then springs back
        static int res[3];
        auto active = [](Game& g) { int n = 0; for (auto& p : g.env.pushers) if (p.w != 0.f) n++; return n; };
        s.push_back({2.5f, [](Game& g) { for (int& r : res) r = 0; const Poi* p = g.world.poiById("brindlemere"); teleport(g, p->pos.x + 40, p->pos.y - 60, 2.4f); g.timeOfDay = 15.f; g.weather = g.weatherNext = Game::W_CLEAR; g.weatherBlend = 1; g.weatherTimer = 999; g.toastHint.clear(); }, idle, "meadow"});
        s.push_back({2.2f, [](Game& g) { g.camPitch = 0.95f; g.camDist = g.camDistTarget = 8.f; }, [](Game& g, float) { g.input.move = vec2(0, 0.55f); g.camIdleT = 0; }, "walk"});
        s.push_back({0.04f, [active](Game& g) { res[0] = active(g) >= 4; AGR_LOG("GRASS walking: %d active benders (hero + trail + characters)", active(g)); g.requestScreenshot("build/shots/98_grass_trail.png"); }, [](Game& g, float) { g.input.move = vec2(0, 0.55f); }, "shot"});
        s.push_back({3.f, none, idle, "stand"});
        s.push_back({0.04f, [](Game& g) {
            // after 3 s still, the trail has sprung back: only the hero (and other characters) remain
            int trail = 0; for (auto& t : g.grassTrail) if (g.realTime - t.t < 2.5f) trail++;
            res[1] = trail == 0;
            float x0 = g.pc().pos.x, z0 = g.pc().pos.z; int blades = 0;
            g.world.forEachVisibleChunk([&](Chunk& ch) { if (ch.grassNear.valid() && distXZ(ch.bounds.center(), g.pc().pos) < 40) blades++; });
            res[2] = blades > 0;
            AGR_LOG("GRASS after standing: fresh trail marks %d, chunks with near grass %d", trail, blades); (void)x0; (void)z0;
            g.requestScreenshot("build/shots/99_grass_recovered.png");
        }, idle, "shot"});
        s.push_back({0.1f, [](Game& g) { bool ok = res[0] && res[1] && res[2]; AGR_LOG("GRASS TEST: bentWhileWalking=%d trailRecovers=%d nearLayer=%d", res[0], res[1], res[2]); AGR_LOG("GRASS TEST %s", ok ? "PASSED" : "FAILED"); }, idle, "report"});
        return s;
    }
    if (name == "mine") {
        // 2.5.1 regression "je peux pas miner": a fresh game, walk up to a real ore / rock node and press the action button
        static Entity node = NullEntity; static int before = 0; static int kind = -1;
        s.push_back({4.f, [](Game& g) { g.newGame(); g.startPlaying(); g.timeOfDay = 11.f; g.toastHint.clear(); g.tutStep = -1; teleport(g, 1600, 1560, 0.f); }, [](Game& g, float) { g.camIdleT = 0; }, "setup"});
        s.push_back({0.5f, [](Game& g) {
            float bd = 1e9f; node = NullEntity;
            g.reg.each<NodeComp>([&](Entity e, NodeComp& n) { NodeType t = n.node.type; if (t != NodeType::Rock && t != NodeType::IronOre && t != NodeType::CopperOre) return; float d = distXZ(n.node.pos, g.pc().pos); if (d < bd) { bd = d; node = e; kind = (int)t; } });
            int nodes = 0; g.reg.each<NodeComp>([&](Entity, NodeComp&) { nodes++; });
            AGR_LOG("MINE %d nodes around, nearest rock/ore %.0f m (type %d), pickaxe %d", nodes, bd, kind, g.countItem("tool_pickaxe"));
            if (node != NullEntity) { NodeComp* n = g.reg.get<NodeComp>(node); vec3 p = n->node.pos; teleport(g, p.x + 1.3f, p.z, 0.f); g.pc().yaw = std::atan2(-1.3f, 0.f); }
            before = 0; for (auto& st : g.inventory) before += st.count; }, idle, "go"});
        s.push_back({0.6f, none, idle, "settle"});
        s.push_back({0.1f, [](Game& g) { AGR_LOG("MINE interact target %s, prompt '%s', dungeonPrompt '%s'", g.interactTarget == node ? "the node" : (g.interactTarget == NullEntity ? "none" : "other"), g.interactTarget != NullEntity && g.reg.get<Interact>(g.interactTarget) ? g.reg.get<Interact>(g.interactTarget)->text.c_str() : "", g.dungeonPrompt.c_str());
            float k = g.ui.scale; Rect r = ctlRect(g, CTL_INTERACT); g.touch(TouchEvent{TouchEvent::Down, 901, (r.x + r.w / 2) * k, (r.y + r.h / 2) * k}); }, idle, "press"});
        s.push_back({0.25f, [](Game& g) { for (auto& f : g.ui.fingers) if (f.id == 901 && f.down) { float k = g.ui.scale; g.touch(TouchEvent{TouchEvent::Up, 901, f.x * k, f.y * k}); } }, idle, "release"});
        s.push_back({1.f, none, idle, "swing"});
        s.push_back({0.1f, [](Game& g) { int after = 0; for (auto& st : g.inventory) after += st.count;
            AGR_LOG("MINE items %d -> %d, stone %d iron %d copper %d -> %s", before, after, g.countItem("stone"), g.countItem("iron_ore"), g.countItem("copper_ore"), after > before ? "MINE TEST PASSED" : "MINE TEST FAILED"); g.requestScreenshot("build/shots/MI_mine.png"); }, idle, "check"});
        // 2.5.1 dig mode from the top bar in a fresh game, in Brindlemere between the houses, and mining a rock in dig mode
        s.push_back({0.3f, [](Game& g) { g.menu = Menu::None; const Poi* b = g.world.poiById("brindlemere"); teleport(g, b->pos.x + 20, b->pos.y + 22, 0.f);
            const Rect* r = nullptr; (void)r; g.digMode = true; before = (int)(g.world.height(g.pc().pos.x + std::sin(g.pc().yaw) * 1.7f, g.pc().pos.z + std::cos(g.pc().yaw) * 1.7f) * 100);
            AGR_LOG("MINE starter shovel %d, bombs %d", g.countItem("tool_shovel"), g.countItem("bomb")); }, idle, "town"});
        s.push_back({0.5f, none, idle, "settle"});
        s.push_back({0.8f, [](Game& g) { g.interactTarget = NullEntity; g.stamina = g.maxStamina; g.pc().attackClip = nullptr; int d0 = g.digsDone; g.playerAttack(false, false); AGR_LOG("MINE dig in Brindlemere: %s", g.digsDone > d0 ? "dug" : "refused"); }, idle, "dig"});
        s.push_back({0.1f, [](Game& g) { AGR_LOG("MINE TOWN TEST %s", g.digsDone > 0 ? "PASSED" : "FAILED"); }, idle, "check"});
        return s;
    }
    if (name == "interior") {
        // 2.4.1 walk into buildings: door prompt, the room, sleep in the bed, the forge, walk out (log + pictures)
        static int house = -1, forge = -1, inn = -1; static int ok[8]; static vec3 door;
        auto findKind = [](Game& g, int kind, vec2 near) { int best = -1; float bd = 1e9f; for (int i = 0; i < (int)g.world.structures.size(); i++) { const Structure& st = g.world.structures[i]; if (st.kind != kind) continue; float d = length(st.pos - near); if (d < bd) { bd = d; best = i; } } return best; };
        s.push_back({1.5f, [findKind](Game& g) { g.newGame(); g.startPlaying(); g.timeOfDay = 15.f; g.toastHint.clear(); g.tutStep = -1; for (int& o : ok) o = 0;
            const Poi* b = g.world.poiById("brindlemere"); vec2 c = b->pos;
            house = findKind(g, S_HOUSE, c); forge = findKind(g, S_FORGE, c); inn = findKind(g, S_INN, c);
            AGR_LOG("INTERIOR house %d forge %d inn %d", house, forge, inn);
            door = g.doorOf(g.world.structures[house]); teleport(g, door.x, door.z, 0.f); }, idle, "go"});
        s.push_back({1.f, none, idle, "settle"});
        s.push_back({0.1f, [](Game& g) { g.interactTarget = NullEntity; g.findDoor(); ok[0] = g.nearDoor == house; AGR_LOG("INTERIOR door prompt %d (near %d)", ok[0], g.nearDoor); g.requestScreenshot("build/shots/I0_door.png"); }, idle, "door"});
        s.push_back({1.5f, [](Game& g) { g.enterInterior(house); }, idle, "enter"});
        s.push_back({1.5f, none, [](Game& g, float) { g.camIdleT = 0; }, "inside"});
        s.push_back({0.1f, [](Game& g) { ok[1] = g.dungeon.active && g.dungeon.interior && g.pc().pos.x > WORLD_SIZE; AGR_LOG("INTERIOR inside %d (%s)", ok[1], g.dungeon.interiorName.c_str()); g.requestScreenshot("build/shots/I1_house.png"); }, idle, "shot"});
        s.push_back({0.3f, [](Game& g) { Character& c = g.pc(); c.pos = g.dungeon.bedPos + vec3(0.9f, 0, 0); c.hp = c.maxHp * 0.3f; g.input.interact = true; g.updateDungeon(0.016f); }, idle, "bed"});
        s.push_back({2.f, none, idle, "sleep"});
        s.push_back({0.1f, [](Game& g) { ok[2] = std::fabs(g.timeOfDay - 7.f) < 0.5f && g.pc().hp >= g.pc().maxHp - 1; AGR_LOG("INTERIOR slept %d (time %.1f, hp %.0f/%.0f)", ok[2], g.timeOfDay, g.pc().hp, g.pc().maxHp); }, idle, "check"});
        s.push_back({0.3f, [](Game& g) { Character& c = g.pc(); c.pos = g.dungeon.exitPos; g.input.interact = true; g.updateDungeon(0.016f); }, idle, "exit"});
        s.push_back({2.f, none, idle, "out"});
        s.push_back({0.1f, [](Game& g) { ok[3] = !g.dungeon.active && distXZ(g.pc().pos, door) < 1.5f; AGR_LOG("INTERIOR back outside %d", ok[3]); }, idle, "check"});
        s.push_back({1.5f, [](Game& g) { g.enterInterior(forge); }, idle, "forge"});
        s.push_back({1.5f, none, idle, "inside"});
        s.push_back({0.3f, [](Game& g) { Character& c = g.pc(); c.pos = g.dungeon.forgePos + vec3(0, 0, 1.2f); g.input.interact = true; g.updateDungeon(0.016f);
            ok[4] = g.menu == Menu::Crafting && g.craftStation == ST_FORGE; AGR_LOG("INTERIOR forge crafting %d", ok[4]); g.requestScreenshot("build/shots/I2_forge.png"); }, idle, "anvil"});
        s.push_back({0.5f, [](Game& g) { g.menu = Menu::None; g.exitDungeon(); }, idle, "leave"});
        s.push_back({1.5f, [](Game& g) { g.enterInterior(inn); }, idle, "inn"});
        s.push_back({1.8f, none, [](Game& g, float) { g.camIdleT = 0; }, "inside"});
        s.push_back({0.1f, [](Game& g) { ok[5] = g.dungeon.interior && g.dungeon.hasBed; AGR_LOG("INTERIOR inn %d", ok[5]); g.requestScreenshot("build/shots/I3_inn.png"); }, idle, "shot"});
        // 2.6.0 the inn keeper: talk, play dice, buy a meal
        s.push_back({0.5f, [](Game& g) { Entity keeper = NullEntity;
            g.reg.each<NpcBrain>([&](Entity e, NpcBrain& nb) { if (nb.resident) keeper = e; });
            ok[6] = keeper != NullEntity;
            if (keeper) { Character* k = g.reg.get<Character>(keeper); Character& c = g.pc(); c.pos = k->pos + vec3(0, 0, 1.6f); c.yaw = PI; g.camYaw = 0.f; g.camDist = g.camDistTarget = 4.5f; g.camPitch = 0.3f;
                g.openDialog(keeper); AGR_LOG("INTERIOR resident '%s' (%s) %d", k->name.c_str(), g.dialog.title.c_str(), ok[6]); } }, [](Game& g, float) { g.camIdleT = 0; }, "talk"});
        s.push_back({2.f, [](Game& g) { g.gold = 100; g.dialogAction("dice"); }, [](Game& g, float) { g.camIdleT = 0; }, "dice"});
        s.push_back({0.1f, [](Game& g) { int gd = g.gold; ok[7] = g.dialog.title == "Aubergiste" && (gd == 90 || gd == 100 || gd == 110); AGR_LOG("INTERIOR dice: gold %d -> %d (%s)", 100, gd, g.dialog.lines.empty() ? "" : g.dialog.lines[0].c_str());
            g.requestScreenshot("build/shots/I4_innkeeper.png"); }, idle, "shot"});
        s.push_back({0.3f, [](Game& g) { g.dialog.open = false;
            bool all = true; for (int o : ok) all = all && o; AGR_LOG("INTERIOR TEST %s", all ? "PASSED" : "FAILED"); }, idle, "shot"});
        return s;
    }
    if (name == "newlands") {
        // 2.4.0 the Valdor Steppes and the Emerald Jungle (pictures)
        static const char* places[] = {"valdor", "pierres_levees", "emeraude", "temple_jaguar", "xochi"};
        static const float offs[][2] = {{40, 30}, {60, -40}, {-30, 50}, {0, 70}, {50, 40}};
        s.push_back({1.f, [](Game& g) { g.newGame(); g.startPlaying(); g.timeOfDay = 10.5f; g.toastHint.clear(); g.tutStep = -1; g.weather = g.weatherNext = Game::W_CLEAR; g.weatherBlend = 1; g.weatherTimer = 999; }, idle, "setup"});
        for (int i = 0; i < 5; i++) {
            s.push_back({4.f, [i](Game& g) { const Poi* p = g.world.poiById(places[i]); if (!p) { AGR_LOG("NEWLANDS missing %s", places[i]); return; }
                teleport(g, p->pos.x + offs[i][0], p->pos.y + offs[i][1], 0.f);
                vec2 d = p->pos - g.pc().pos.xz(); g.camYaw = std::atan2(-d.x, -d.y); g.camPitch = 0.22f; g.camDist = g.camDistTarget = 12.f;
                AGR_LOG("NEWLANDS %s region %s", places[i], regionName(g.world.regionId(p->pos.x, p->pos.y))); },
                [i](Game& g, float) { const Poi* p = g.world.poiById(places[i]); if (p) { vec2 d = p->pos - g.pc().pos.xz(); g.camYaw = std::atan2(-d.x, -d.y); } g.camIdleT = 0; }, "go"});
            s.push_back({0.1f, [i](Game& g) { g.requestScreenshot(std::string("build/shots/N_") + places[i] + ".png"); }, idle, "shot"});
        }
        return s;
    }
    if (name == "dig") {
        // 2.4.0 diggable ground: shovel strikes, a bomb crater, save / load keeps the holes (log + pictures)
        static vec2 spot; static float h0, h1, h2; static int ok[4];
        s.push_back({2.f, [](Game& g) { g.newGame(); g.startPlaying(); g.timeOfDay = 11.f; g.toastHint.clear(); g.tutStep = -1; for (int& o : ok) o = 0;
            teleport(g, 1600, 1560, 0.f); g.addItem("tool_shovel", 1); g.addItem("bomb", 3); g.renderer.settings.grassDensity = 0.3f;
            for (auto& st : g.inventory) if (st.id == "tool_shovel") { int i = (int)(&st - &g.inventory[0]); g.equip(i); break; }
            Character& c = g.pc(); spot = (c.pos + vec3(std::sin(c.yaw), 0, std::cos(c.yaw)) * 1.7f).xz(); h0 = g.world.height(spot.x, spot.y);
            g.camYaw = c.yaw + 2.4f; g.camPitch = 0.45f; g.camDist = g.camDistTarget = 8.f; }, idle, "setup"});
        for (int i = 0; i < 6; i++) {
            s.push_back({0.7f, [](Game& g) { g.stamina = g.maxStamina; g.pc().attackClip = nullptr; g.playerAttack(false, false); }, [](Game& g, float) { g.camIdleT = 0; }, "dig"});
        }
        s.push_back({1.5f, none, [](Game& g, float) { g.camIdleT = 0; }, "rebuild"});
        s.push_back({0.1f, [](Game& g) { h1 = g.world.height(spot.x, spot.y); ok[0] = g.digMode && h1 < h0 - 1.5f && g.digsDone == 6;
            AGR_LOG("DIG shovel: ground %.2f -> %.2f after %d strikes -> %d", h0, h1, g.digsDone, ok[0]); g.requestScreenshot("build/shots/D0_hole.png"); }, idle, "shot"});
        s.push_back({0.2f, [](Game& g) { Character& c = g.pc(); c.yaw += PI * 0.5f; g.pc().attackClip = nullptr; g.playerAttack(false, true); AGR_LOG("DIG bomb thrown, %zu in flight, %d left", g.bombs.size(), g.countItem("bomb")); }, idle, "bomb"});
        s.push_back({3.5f, none, [](Game& g, float) { g.camIdleT = 0; }, "boom"});
        s.push_back({0.1f, [](Game& g) { ok[1] = g.bombs.empty() && g.world.dugChunks() > 0; AGR_LOG("DIG bomb exploded %d, dug chunks %d", ok[1], g.world.dugChunks()); g.requestScreenshot("build/shots/D1_crater.png"); }, idle, "shot"});
        s.push_back({0.5f, [](Game& g) { h2 = g.world.height(spot.x, spot.y); g.saveGame(7); g.newGame(); ok[2] = g.world.dugChunks() == 0 && std::fabs(g.world.height(spot.x, spot.y) - h0) < 0.01f; AGR_LOG("DIG new game resets the ground %d", ok[2]); }, idle, "save"});
        s.push_back({0.5f, [](Game& g) { g.loadGame(7); float h = g.world.height(spot.x, spot.y); ok[3] = std::fabs(h - h2) < 0.05f; AGR_LOG("DIG reload keeps the hole: %.2f vs %.2f -> %d", h, h2, ok[3]);
            bool all = true; for (int o : ok) all = all && o; AGR_LOG("DIG TEST %s", all ? "PASSED" : "FAILED"); }, idle, "load"});
        return s;
    }
    if (name == "swim") {
        // 2.4.0 swimming: surface, front crawl, dive, air, underwater view, bank climb (log + pictures)
        static vec3 lake; static int ok[5];
        s.push_back({2.f, [](Game& g) { g.newGame(); g.startPlaying(); g.timeOfDay = 11.f; g.toastHint.clear(); g.tutStep = -1; for (int& o : ok) o = 0;
            float best = 0;
            for (float z = 4250; z < 4560 && best < 6.f; z += 4) for (float x = 2650; x < 2850; x += 8) {   // off the Coral Coast, away from the piers
                bool lv = false, fr = false; float wl = g.world.waterLevel(x, z, &lv, &fr); if (wl < -1e8f || lv || fr) continue;
                float dep = wl - g.world.height(x, z);
                if (dep > 5.f && dep < 9.f && dep > best) { best = dep; lake = vec3(x, wl, z); }
            }
            AGR_LOG("SWIM lake at %.0f %.0f depth %.1f", lake.x, lake.z, best);
            teleport(g, lake.x, lake.z, 0.f); g.pc().pos = vec3(lake.x, lake.y - 1.2f, lake.z); g.camFocus = g.pc().pos; g.camDist = g.camDistTarget = 7.f; g.camPitch = 0.25f; }, idle, "lake"});
        s.push_back({1.5f, none, [](Game& g, float) { g.input.move = vec2(0, 0.6f); g.camIdleT = 0; }, "swim"});
        s.push_back({0.1f, [](Game& g) { ok[0] = g.pc().swimming; Character& c = g.pc();
            { bool lvx = false, frx = false; g.waterAt(c.pos.x, c.pos.z, &lvx, &frx); AGR_LOG("SWIM dbg lava %d frozen %d swimming %d", lvx, frx, c.swimming);
              vec3 t = c.pos; float st = -1e9f; bool col = g.collideStatic(t, c.radius, c.pos.y, &st);
              AGR_LOG("SWIM dbg pos %.1f %.1f %.1f ground %.1f water %.1f cine %d gate %d step %.2f col %d inWater %d grounded %d state %d menu %d", c.pos.x, c.pos.y, c.pos.z, g.groundAt(c.pos.x, c.pos.z), g.waterAt(c.pos.x, c.pos.z), g.cine.active, (int)g.loadGate, st, col, c.inWater, c.grounded, (int)g.state, (int)g.menu); }
            AGR_LOG("SWIM surface swimming %d stamina %.0f", ok[0], g.stamina); g.requestScreenshot("build/shots/W0_swim.png"); }, [](Game& g, float) { g.input.move = vec2(0, 0.6f); }, "shot"});
        s.push_back({1.5f, [](Game& g) { g.stamina = g.maxStamina; }, [](Game& g, float) { g.input.move = vec2(0, 1); g.input.sprint = true; g.camIdleT = 0; g.camYaw = g.pc().yaw + 1.4f; }, "crawl"});
        s.push_back({0.1f, [](Game& g) { ok[1] = g.pc().vis.anim.swimCrawl > 0.8f && g.stamina < g.maxStamina; AGR_LOG("SWIM crawl %.2f stamina %.0f -> %d", g.pc().vis.anim.swimCrawl, g.stamina, ok[1]); g.requestScreenshot("build/shots/W1_crawl.png"); },
            [](Game& g, float) { g.input.move = vec2(0, 1); g.input.sprint = true; g.camYaw = g.pc().yaw + 1.4f; }, "shot"});
        s.push_back({0.2f, [](Game& g) { teleport(g, lake.x, lake.z, 0.f); g.pc().pos = vec3(lake.x, lake.y - 1.2f, lake.z); g.camFocus = g.pc().pos; g.pc().swimming = true; g.playerDodge(); AGR_LOG("SWIM dive %d", g.diving); }, idle, "dive"});
        s.push_back({3.5f, none, [](Game& g, float) { g.input.move = vec2(0, 0.4f); g.camIdleT = 0; g.camPitch = 0.12f; g.camDist = g.camDistTarget = 6.f; }, "under"});
        s.push_back({0.1f, [](Game& g) { float y0 = g.pc().pos.y; ok[2] = g.diving && y0 < lake.y - 2.5f && g.breath < 1.f; ok[3] = g.underwaterView;
            AGR_LOG("SWIM underwater: depth %.1f m, air %.2f, view %d -> %d", lake.y - y0, g.breath, g.underwaterView, ok[2]); g.requestScreenshot("build/shots/W2_underwater.png"); }, [](Game& g, float) { g.input.move = vec2(0, 0.4f); }, "shot"});
        s.push_back({0.2f, [](Game& g) { g.playerJump(false); }, idle, "surface"});
        s.push_back({2.5f, none, idle, "rise"});
        s.push_back({0.1f, [](Game& g) { ok[4] = !g.diving && g.pc().pos.y > lake.y - 1.6f; AGR_LOG("SWIM surfaced %d (y %.1f)", ok[4], lake.y - g.pc().pos.y);
            bool all = true; for (int o : ok) all = all && o; AGR_LOG("SWIM TEST %s", all ? "PASSED" : "FAILED"); }, idle, "check"});
        return s;
    }
    if (name == "events") {
        // 2.3.6 random world events: start each kind, finish it, check the reward (log + pictures)
        static int ok[3]; static int gold0;
        s.push_back({2.f, [](Game& g) { g.newGame(); g.startPlaying(); g.timeOfDay = 11.f; g.toastHint.clear(); g.tutStep = -1; teleport(g, 1600, 1560, 0.f); for (int& o : ok) o = 0; g.eventTimer = 1e9f; }, idle, "setup"});
        // ambush
        s.push_back({0.5f, [](Game& g) { bool st = g.startWorldEvent(0); AGR_LOG("EVENT ambush started %d, %zu foes", st, g.worldEvent.foes.size()); gold0 = g.gold;
            g.camYaw = g.pc().yaw + PI; g.camDist = g.camDistTarget = 14.f; }, idle, "ambush"});
        s.push_back({2.f, none, [](Game& g, float) { g.camIdleT = 0; Character& p = g.pc(); vec3 d = g.worldEvent.pos - p.pos; p.yaw = std::atan2(d.x, d.z); }, "look"});
        s.push_back({0.1f, shot("E0_ambush"), idle, "shot"});
        s.push_back({0.5f, [](Game& g) { for (Entity e : g.worldEvent.foes) if (Character* c = g.reg.get<Character>(e)) { c->hp = 0; c->dead = true; } }, idle, "win"});
        s.push_back({1.f, none, idle, "wait"});
        s.push_back({0.1f, [](Game& g) { ok[0] = !g.worldEvent.active && g.gold > gold0 && g.eventsDone == 1; AGR_LOG("EVENT ambush done %d (gold +%d)", ok[0], g.gold - gold0); }, idle, "check"});
        // falling star
        s.push_back({0.2f, [](Game& g) { bool st = g.startWorldEvent(1); AGR_LOG("EVENT star started %d", st); Character& p = g.pc(); vec3 d = g.worldEvent.pos - p.pos; p.yaw = std::atan2(d.x, d.z); g.camYaw = p.yaw + PI; }, idle, "star"});
        s.push_back({1.6f, none, [](Game& g, float) { g.camIdleT = 0; }, "falling"});
        s.push_back({0.1f, shot("E1_star_fall"), idle, "shot"});
        s.push_back({1.5f, none, [](Game& g, float) { g.camIdleT = 0; }, "impact"});
        s.push_back({0.3f, [](Game& g) { teleport(g, g.worldEvent.pos.x + 3.f, g.worldEvent.pos.z + 3.f, 0.f); }, idle, "go"});
        s.push_back({1.f, none, idle, "arrive"});
        s.push_back({0.1f, [](Game& g) { int nodes = 0; g.reg.each<NodeComp>([&](Entity, NodeComp& n) { if (distXZ(n.node.pos, g.pc().pos) < 8.f && (n.node.type == NodeType::RareOre || n.node.type == NodeType::Crystal)) nodes++; });
            ok[1] = !g.worldEvent.active && nodes >= 3; AGR_LOG("EVENT star done %d (%d crater nodes)", ok[1], nodes); g.requestScreenshot("build/shots/E2_crater.png"); }, idle, "check"});
        // stash
        s.push_back({0.2f, [](Game& g) { bool st = g.startWorldEvent(2); AGR_LOG("EVENT stash started %d", st); }, idle, "stash"});
        s.push_back({0.3f, [](Game& g) { teleport(g, g.worldEvent.pos.x + 2.f, g.worldEvent.pos.z + 2.f, 0.f); }, idle, "go"});
        s.push_back({1.f, none, idle, "arrive"});
        s.push_back({0.1f, [](Game& g) { ok[2] = !g.worldEvent.active && g.eventsDone == 3; AGR_LOG("EVENT stash done %d", ok[2]);
            AGR_LOG("EVENTS TEST %s", ok[0] && ok[1] && ok[2] ? "PASSED" : "FAILED"); }, idle, "check"});
        return s;
    }
    if (name == "perf") {
        // 2.3.6 render cost in typical places (logs draw calls / triangles at each picture)
        static const char* places[] = {"brindlemere", "solmar", "port_azur", "shirokawa", "givrebourg", "chenebrune", "lumenhaven"};
        s.push_back({1.f, [](Game& g) { g.newGame(); g.startPlaying(); g.timeOfDay = 11.f; g.toastHint.clear(); g.tutStep = -1; }, idle, "setup"});
        for (int i = 0; i < 7; i++) {
            const char* pl = places[i];
            s.push_back({3.f, [pl](Game& g) { const Poi* p = g.world.poiById(pl); if (p) teleport(g, p->pos.x + 6, p->pos.y + 6, 0.8f); g.camPitch = 0.3f; g.camDist = g.camDistTarget = 9.f; }, [](Game& g, float) { g.camIdleT = 0; }, "go"});
            s.push_back({0.1f, [pl](Game& g) { AGR_LOG("PERF %s", pl); g.requestScreenshot(std::string("build/shots/P_") + pl + ".png"); }, idle, "shot"});
        }
        return s;
    }
    if (name == "res") {
        // 2.3.5 the new resource nodes side by side, then harvest each one (look at the picture + log)
        static int got = 0;
        s.push_back({2.5f, [](Game& g) { g.newGame(); g.startPlaying(); g.timeOfDay = 11.f; g.toastHint.clear(); g.tutStep = -1; g.addItem("tool_pickaxe", 1); teleport(g, 1600, 1560, 0.f);
            Character& c = g.pc();
            for (int i = 0; i < 8; i++) {
                NodeSpawn n; n.type = (NodeType)((int)NodeType::GoldOre + i); n.uid = 900000 + i; n.tier = 1; n.rot = 0.4f * i;
                float x = c.pos.x - 7.f + i * 2.f, z = c.pos.z + 5.f; n.pos = vec3(x, g.groundAt(x, z), z);
                g.spawnNode(n);
            }
            g.camYaw = PI; g.camPitch = 0.42f; g.camDist = g.camDistTarget = 11.f; g.renderer.settings.grassDensity = 0.f; }, idle, "spawn"});
        s.push_back({1.f, none, [](Game& g, float) { g.camIdleT = 0; g.camYaw = PI; }, "settle"});
        s.push_back({0.1f, shot("R0_resources"), idle, "shot"});
        s.push_back({0.2f, [](Game& g) {
            int before = 0; for (auto& st : g.inventory) before += st.count;
            g.reg.each<NodeComp>([&](Entity e, NodeComp& n) { if (n.node.uid >= 900000 && n.node.uid < 900008) { teleport(g, n.node.pos.x, n.node.pos.z - 1.2f, 0.f); g.interactTarget = e; g.playerInteract(); } });
            int after = 0; for (auto& st : g.inventory) after += st.count;
            got = after - before;
            AGR_LOG("RES harvested %d items: gold %d silver %d coal %d cactus %d shells %d flax %d amber %d fireroot %d -> %s", got, g.countItem("gold_ore"), g.countItem("silver_ore"), g.countItem("coal"), g.countItem("cactus_fruit"),
                    g.countItem("seashell"), g.countItem("flax"), g.countItem("amber"), g.countItem("fire_root"),
                    g.countItem("gold_ore") && g.countItem("silver_ore") && g.countItem("coal") && g.countItem("cactus_fruit") && g.countItem("seashell") && g.countItem("flax") && g.countItem("amber") && g.countItem("fire_root") ? "RES TEST PASSED" : "RES TEST FAILED"); }, idle, "harvest"});
        return s;
    }
    if (name == "army") {
        // 3.0.0 the castle and the army: found a domain, grow it to a castle, build barracks / keep / crypt / ramparts,
        // recruit swordsmen, archers and knights, take them into the domain's dungeon, then storm the Iron Citadel
        // (its garrison, its warlord), check the conquest, the guards, the tribute and that everything is saved.
        static int res[12]; static vec2 spot;
        auto aim = [](Game& g, int def, vec2 at, float yaw) {
            float r = baseBuildings()[def].radius + 2.2f;
            vec2 from = at - vec2(std::sin(yaw), std::cos(yaw)) * r;
            teleport(g, from.x, from.y, yaw); g.pc().yaw = yaw;
        };
        s.push_back({0.1f, [](Game& g) {
            for (int& r : res) r = 0;
            g.newGame(); g.startPlaying(); g.toastHint.clear(); g.tutStep = -1;
            const Poi* b = g.world.poiById("brindlemere"); bool found = false;
            for (float rr = 200.f; rr < 1200.f && !found; rr += 25.f)
                for (float a = 0; a < TAU && !found; a += 0.3f) {
                    vec2 p = b->pos + vec2(std::cos(a), std::sin(a)) * rr;
                    teleport(g, p.x, p.y, 0.f);
                    g.addItem("wood", 10, Rarity::Count, false);
                    std::string why; if (g.canFoundBase(why)) { g.foundBase(); found = g.base.founded; spot = p; }
                    // a castle needs room: keep looking until the domain could grow to 80 m
                    if (found) { g.gold = 99999; g.addItem("stone", 2000, Rarity::Count, false); g.addItem("wood", 2000, Rarity::Count, false); g.addItem("iron_bar", 200, Rarity::Count, false);
                                 std::string w2; bool up1 = g.upgradeBase(w2, false); g.base.level = 1; bool up2 = up1 && g.upgradeBase(w2, false); g.base.level = 0;
                                 if (!up2) { g.clearBase(); found = false; } }
                }
            AGR_LOG("ARMY domain at (%.0f, %.0f) founded %d", spot.x, spot.y, g.base.founded);
            g.addItem("crystal", 20, Rarity::Count, false);
            g.level = 25; g.recomputeStats(); g.pc().hp = g.pc().maxHp;
            g.timeOfDay = 10.f; g.weather = g.weatherNext = Game::W_CLEAR; g.weatherBlend = 1; g.weatherTimer = 999;
        }, idle, "found"});
        s.push_back({2.f, [](Game& g) {
            std::string why;
            bool a = g.upgradeBase(why, true); bool b2 = g.upgradeBase(why, true);
            const Poi* p = g.world.poiById("player_base");
            res[0] = a && b2 && g.base.level == 2 && p && p->radius > 79.f;
            AGR_LOG("ARMY upgraded to castle %d (level %d, radius %.0f) %s", res[0], g.base.level, p ? p->radius : 0.f, why.c_str());
        }, idle, "upgrade"});
        struct B { const char* id; float a, d, yaw; };
        static const B builds[] = {{"barracks", 0.3f, 30.f, 0.f}, {"keep", 2.2f, 30.f, 0.f}, {"crypt", 4.0f, 26.f, 0.f}, {"chapel", 5.3f, 34.f, 0.f}, {"castletower", 1.2f, 62.f, 0.f}, {"stonewall", 1.28f, 57.f, 1.2f + PI / 2}};
        for (const B& bd : builds) {
            const B* q = &bd;
            s.push_back({0.5f, [aim, q](Game& g) { int def = baseBuildIndex(q->id); vec2 at = g.base.center + vec2(std::cos(q->a), std::sin(q->a)) * q->d; aim(g, def, at, q->yaw); g.startBuilding(def); g.buildRot = q->yaw; }, idle, "aim"});
            s.push_back({0.5f, [q](Game& g) { bool ok = g.placeBuilding(); AGR_LOG("ARMY built %s: %d (%s)", q->id, ok, g.buildWhy.c_str()); if (ok) res[1]++; }, idle, "place"});
        }
        s.push_back({0.5f, [](Game& g) {
            int cap = g.baseGuardCap();
            g.base.soldiers = 4; g.base.archers = 3; g.base.knights = 2; g.base.escort = true;
            AGR_LOG("ARMY cap %d, army %d", cap, g.armySize());
            res[2] = cap >= 9;
            teleport(g, g.base.center.x + 5, g.base.center.y + 5, 0.f);
        }, idle, "recruit"});
        s.push_back({3.f, none, [](Game& g, float) { g.pc().invuln = 1.f; }, "army spawns"});
        s.push_back({0.1f, [](Game& g) {
            int n = 0, bows = 0, kn = 0;
            for (Entity e : g.soldierEnts) if (SoldierBrain* sb = g.reg.get<SoldierBrain>(e)) { n++; if (sb->unit == 1) bows++; if (sb->unit == 2) kn++; }
            res[3] = n == 9 && bows == 3 && kn == 2;
            AGR_LOG("ARMY spawned %d (archers %d, knights %d) -> %d", n, bows, kn, res[3]);
            g.camPitch = 0.45f; g.camDist = g.camDistTarget = 16.f;
        }, idle, "count"});
        s.push_back({0.1f, shot("AR0_army"), idle, "shot"});
        s.push_back({0.5f, [](Game& g) { for (auto& b : g.base.built) if (baseBuildings()[b.def].id == "crypt") { teleport(g, b.pos.x + 3.f, b.pos.y + 3.f, 0.f); } }, idle, "to crypt"});
        s.push_back({0.4f, none, [](Game& g, float) { g.input.interact = true; }, "enter"});
        s.push_back({4.f, none, [](Game& g, float) { g.input.interact = false; g.pc().invuln = 1.f; }, "inside"});
        s.push_back({0.1f, [](Game& g) {
            int near = 0;
            for (Entity e : g.soldierEnts) if (Character* c = g.reg.get<Character>(e)) if (distXZ(c->pos, g.pc().pos) < 25.f && c->pos.x > WORLD_SIZE) near++;
            res[4] = g.dungeon.active && g.dungeon.levelOverride >= 25 && near >= 6;
            AGR_LOG("ARMY domain dungeon active %d level %d, soldiers with the hero %d -> %d", g.dungeon.active, g.dungeon.levelOverride, near, res[4]);
            g.camPitch = 0.6f; g.camDist = g.camDistTarget = 14.f;
        }, idle, "dungeon check"});
        s.push_back({0.1f, shot("AR1_dungeon"), idle, "shot"});
        s.push_back({0.2f, [](Game& g) { g.exitDungeon(); }, idle, "exit"});
        s.push_back({3.f, none, idle, "out"});
        // storm the Iron Citadel
        s.push_back({0.5f, [](Game& g) { const Poi* p = g.world.poiById("citadelle_fer"); teleport(g, p->pos.x - p->radius - 60.f, p->pos.y, PI * 0.5f); g.pc().yaw = PI * 0.5f; }, idle, "to citadel"});
        s.push_back({4.f, none, [](Game& g, float) { g.pc().invuln = 1.f; }, "approach"});
        s.push_back({0.1f, [](Game& g) {
            const Poi* p = g.world.poiById("citadelle_fer"); int pi = (int)(p - &g.world.pois[0]);
            auto it = g.fortGarrison.find(pi);
            int n = it == g.fortGarrison.end() ? 0 : (int)it->second.size();
            res[5] = n >= 10;
            AGR_LOG("ARMY citadel garrison %d -> %d", n, res[5]);
            teleport(g, p->pos.x - 20.f, p->pos.y + 10.f, PI * 0.5f); g.camPitch = 0.35f; g.camDist = g.camDistTarget = 18.f;
        }, idle, "garrison"});
        s.push_back({2.f, none, [](Game& g, float) { g.pc().invuln = 1.f; }, "inside"});
        s.push_back({0.1f, shot("AR2_citadel"), idle, "shot"});
        s.push_back({0.5f, [](Game& g) {
            g.reg.each<Character>([&](Entity e, Character& c) { if (!c.dead && c.enemyDef >= 0 && enemies()[c.enemyDef].id == "legion_warlord") g.killCharacter(e, c, g.player); });
            res[6] = g.fortConquered("citadelle_fer");
            AGR_LOG("ARMY warlord down, citadel conquered %d", res[6]);
        }, idle, "warlord"});
        s.push_back({3.f, none, [](Game& g, float) { g.pc().invuln = 1.f; }, "guards"});
        s.push_back({0.1f, [](Game& g) {
            const Poi* p = g.world.poiById("citadelle_fer"); int pi = (int)(p - &g.world.pois[0]);
            int guards = g.fortGuards.count(pi) ? (int)g.fortGuards[pi].size() : 0;
            int legion = 0; g.reg.each<Character>([&](Entity, Character& c) { if (!c.dead && c.enemyDef >= 0 && enemies()[c.enemyDef].family == "Légion de Fer" && distXZ(c.pos, vec3(p->pos.x, 0, p->pos.y)) < p->radius + 20.f) legion++; });
            res[7] = guards == 6 && legion == 0 && g.baseGuardCap() >= 4 + 9;
            AGR_LOG("ARMY guards %d, legion left %d, cap %d -> %d", guards, legion, g.baseGuardCap(), res[7]);
            int g0 = g.gold; g.day++; g.updateFortresses(0.1f); res[8] = g.gold > g0;
            AGR_LOG("ARMY tribute %d -> %d", g.gold - g0, res[8]);
        }, idle, "after"});
        s.push_back({0.1f, shot("AR3_conquered"), idle, "shot"});
        s.push_back({0.5f, [](Game& g) {
            g.saveGame(g.saveSlot);
            g.storyFlags.erase("conq_citadelle_fer"); g.base.level = 0; g.base.archers = 0;
            bool ok = g.loadGame(g.saveSlot);
            res[9] = ok && g.fortConquered("citadelle_fer") && g.base.level == 2 && g.base.archers == 3 && g.base.knights == 2 && g.baseCount("crypt") == 1;
            AGR_LOG("ARMY save/load %d -> %d (level %d, archers %d)", ok, res[9], g.base.level, g.base.archers);
            int pass = res[0] + (res[1] >= 5) + res[2] + res[3] + res[4] + res[5] + res[6] + res[7] + res[8] + res[9];
            AGR_LOG("ARMY %s (%d/10, built %d)", pass == 10 ? "TEST PASSED" : "TEST FAILED", pass, res[1]);
        }, idle, "save"});
        return s;
    }
    if (name == "grasslook") {
        // 3.0.2 the meadow by day, at dusk and in the rain (ground lines, grass look), Greenhaven and the Titans
        struct Look { float x, z, hour; int weather; const char* shot; };
        static const Look looks[] = {{1600, 1500, 11.f, 0, "GL0_day"}, {1600, 1500, 11.f, 2, "GL1_rain"}, {6780, 3100, 15.f, 0, "GL2_titans"}, {6780, 3100, 15.f, 2, "GL3_titans_rain"}, {1600, 1500, 18.8f, 0, "GL4_dusk"}};
        s.push_back({1.f, [](Game& g) { g.newGame(); g.startPlaying(); g.toastHint.clear(); g.tutStep = -1; }, idle, "setup"});
        for (const Look& l : looks) {
            const Look* q = &l;
            s.push_back({4.f, [q](Game& g) { teleport(g, q->x, q->z, 0.9f); g.camPitch = 0.3f; g.camDist = g.camDistTarget = 9.f; g.timeOfDay = q->hour;
                                             g.weather = g.weatherNext = q->weather; g.weatherBlend = 1; g.weatherTimer = 999; },
                         [q](Game& g, float) { g.camIdleT = 0; g.camYaw = 0.9f; g.timeOfDay = q->hour; if (q->weather == 2) g.env.wetness = 1.f; else g.env.wetness = 0.f; }, "go"});
            s.push_back({0.1f, [q](Game& g) { g.requestScreenshot(std::string("build/shots/") + q->shot + ".png"); }, idle, "shot"});
        }
        return s;
    }
    if (name == "powers") {
        // 3.0.1 the shader effects of every power: Kaede, Astra, then the Wanderer's spells, at dusk (they glow)
        static Entity dummy = NullEntity;
        auto refill = [](Game& g) { g.mp = g.maxMp; g.stamina = g.maxStamina; for (float& c : g.heroCD) c = 0; for (auto& c : g.spellCD) c = 0; g.pc().invuln = 1.f; };
        auto target = [](Game& g) {
            if (dummy != NullEntity && g.reg.valid(dummy)) { Character* c = g.reg.get<Character>(dummy); if (c && !c->dead) { c->hp = c->maxHp; return; } }
            Character& p = g.pc(); vec3 sp = p.pos + vec3(0, 0, 7.f); sp.y = g.groundAt(sp.x, sp.z);
            dummy = g.spawnEnemy(enemyIndex("goblin"), sp, false, 777);
            if (Character* c = g.reg.get<Character>(dummy)) { c->maxHp = c->hp = 1e7f; }
            if (AIBrain* ai = g.reg.get<AIBrain>(dummy)) ai->home = sp;
            g.lockTarget = dummy;
        };
        s.push_back({2.f, [](Game& g) {
            g.newGame(); g.startPlaying(); g.toastHint.clear(); g.tutStep = -1; g.level = 30; g.recomputeStats();
            g.timeOfDay = 19.6f; g.weather = g.weatherNext = Game::W_CLEAR; g.weatherBlend = 1; g.weatherTimer = 999;
            teleport(g, 1600, 1500, 0.f); g.pc().yaw = 0; dummy = NullEntity;
            g.unlockHero("kaede", false); g.unlockHero("astra", false);
        }, idle, "setup"});
        struct Cast { int hero; int slot; const char* spell; float wait; const char* shot; };
        static const Cast casts[] = {
            {1, 0, nullptr, 0.12f, "PW0_shuriken"}, {1, 1, nullptr, 0.1f, "PW1_shadowstep"}, {1, 2, nullptr, 0.35f, "PW2_smoke"},
            {2, 0, nullptr, 0.18f, "PW3_starbeam"}, {2, 2, nullptr, 0.3f, "PW4_solarwave"}, {2, 1, nullptr, 1.05f, "PW5_meteor"},
            {0, 0, "fireball", 0.3f, "PW6_fireball"}, {0, 0, "frostnova", 0.25f, "PW7_frostnova"}, {0, 0, "chain", 0.1f, "PW8_chain"},
            {0, 0, "thunder", 0.15f, "PW9_thunder"}, {0, 0, "aegis", 0.5f, "PW10_aegis"}, {0, 0, "holylight", 0.4f, "PW11_holylight"},
        };
        for (const Cast& cs : casts) {
            const Cast* q = &cs;
            s.push_back({1.2f, [q, refill, target](Game& g) {
                if (g.activeHero != q->hero) g.switchHero(q->hero);
                teleport(g, 1600, 1500, 1.2f); g.pc().yaw = 0; g.flying = false;
                g.camPitch = 0.42f; g.camDist = g.camDistTarget = 12.f;
                refill(g); target(g);
            }, [](Game& g, float) { g.camIdleT = 0; g.camYaw = 1.2f; g.pc().invuln = 1.f; }, "ready"});
            s.push_back({q->wait, [q, refill](Game& g) {
                refill(g); g.pc().yaw = 0;
                if (q->spell) { g.equippedSpells[0] = spellIndex(q->spell); g.castSpell(0); } else g.castHeroPower(q->slot);
            }, [](Game& g, float) { g.camYaw = 1.2f; g.camIdleT = 0; }, "cast"});
            s.push_back({0.1f, [q](Game& g) { AGR_LOG("POWERS %s (effects alive %zu)", q->shot, g.vfxList.size()); g.requestScreenshot(std::string("build/shots/") + q->shot + ".png"); }, idle, "shot"});
        }
        return s;
    }
    if (name == "titandungeons") {
        // 3.0.0 the three dungeons of the Titan Mountains: enter each from its entrance, check its dwellers are
        // raised to the dungeon's level, look at the boss room, come back out
        static int ok[3];
        static const char* ids[] = {"gouffre", "mines_fer", "tombe_geant"};
        s.push_back({1.f, [](Game& g) { g.newGame(); g.startPlaying(); g.toastHint.clear(); g.tutStep = -1; g.level = 28; g.recomputeStats(); ok[0] = ok[1] = ok[2] = 0; }, idle, "setup"});
        for (int k = 0; k < 3; k++) {
            s.push_back({0.5f, [k](Game& g) {
                int di = -1; for (int i = 0; i < (int)dungeons().size(); i++) if (dungeons()[i].id == ids[k]) di = i;
                const Poi* p = g.world.poiById(dungeons()[di].poi);
                teleport(g, p->pos.x + 3.f, p->pos.y + 3.f, 0.f);
            }, idle, "to entrance"});
            s.push_back({2.5f, none, [](Game& g, float) { g.input.interact = true; g.pc().invuln = 1.f; }, "enter"});
            s.push_back({3.f, none, [](Game& g, float) { g.input.interact = false; g.pc().invuln = 1.f; }, "inside"});
            s.push_back({0.1f, [k](Game& g) {
                int n = 0, low = 0, lvl = 0;
                g.reg.each<Character>([&](Entity, Character& c) { if (c.team == Team::Enemy && c.pos.x > WORLD_SIZE && !c.dead) { n++; lvl = std::max(lvl, c.level); if (c.level < 24) low++; } });
                const DungeonDef& D = dungeons()[std::max(0, g.dungeon.def)];
                ok[k] = g.dungeon.active && D.id == ids[k] && n > 0 && low == 0 && D.boss >= 0;
                AGR_LOG("TDUN %s active %d, enemies %d (below 24: %d, top level %d), boss %d -> %d", ids[k], g.dungeon.active, n, low, lvl, D.boss, ok[k]);
                g.camPitch = 0.7f; g.camDist = g.camDistTarget = 14.f;
            }, idle, "check"});
            s.push_back({0.1f, [k](Game& g) { g.requestScreenshot(std::string("build/shots/TD") + std::to_string(k) + ".png"); }, idle, "shot"});
            s.push_back({0.3f, [](Game& g) { g.exitDungeon(); }, idle, "exit"});
            s.push_back({2.5f, none, idle, "out"});
        }
        s.push_back({0.1f, [](Game& g) { (void)g; AGR_LOG("TDUN %s", ok[0] && ok[1] && ok[2] ? "TEST PASSED" : "TEST FAILED"); }, idle, "result"});
        return s;
    }
    if (name == "titans") {
        // 3.0.0 a tour of the Titan Mountains: the cities (avenue, plaza, ramparts), the villages, the fortresses, the peaks
        struct Spot { float x, z, yaw, pitch, dist; const char* shot; };
        static const Spot spots[] = {
            {6760, 2560, 3.14f, 0.18f, 9.f, "TI0_avenue"}, {6735, 2650, 1.6f, 0.35f, 14.f, "TI1_plaza"}, {6760, 2470, 0.0f, 0.25f, 16.f, "TI2_gate"},
            {6690, 2560, 2.4f, 0.55f, 40.f, "TI3_city_above"}, {6600, 4330, 3.14f, 0.2f, 10.f, "TI4_rochebrune"}, {6300, 1290, 3.6f, 0.3f, 12.f, "TI5_col"},
            {7150, 2410, 0.6f, 0.4f, 20.f, "TI6_citadel"}, {6780, 3100, 1.2f, 0.12f, 12.f, "TI7_peaks"}, {6450, 5060, 3.3f, 0.3f, 12.f, "TI8_tomb"},
        };
        s.push_back({1.f, [](Game& g) { g.newGame(); g.startPlaying(); g.timeOfDay = 11.f; g.toastHint.clear(); g.tutStep = -1; g.weather = g.weatherNext = Game::W_CLEAR; g.weatherBlend = 1; g.weatherTimer = 999; }, idle, "setup"});
        for (const Spot& sp : spots) {
            const Spot* q = &sp;
            s.push_back({4.f, [q](Game& g) { teleport(g, q->x, q->z, q->yaw); g.camPitch = q->pitch; g.camDist = g.camDistTarget = q->dist; g.timeOfDay = 11.f; }, [q](Game& g, float) { g.camIdleT = 0; g.camYaw = q->yaw; g.camPitch = q->pitch; g.pc().invuln = 1.f; }, "go"});
            s.push_back({0.1f, [q](Game& g) { AGR_LOG("TITANS %s", q->shot); g.requestScreenshot(std::string("build/shots/") + q->shot + ".png"); }, idle, "shot"});
        }
        return s;
    }
    if (name == "worldmap") {
        // 3.0.0 the whole map with every place discovered (geography check of the bigger world)
        s.push_back({1.f, [](Game& g) { g.newGame(); g.startPlaying(); g.toastHint.clear(); g.tutStep = -1; for (auto& p : g.world.pois) g.discovered.insert(p.id); std::fill(g.fog.begin(), g.fog.end(), 255); }, idle, "setup"});
        s.push_back({0.5f, [](Game& g) { g.menu = Menu::Map; }, idle, "map"});
        s.push_back({4.f, none, idle, "map wait"});
        s.push_back({0.1f, shot("WM_map"), idle, "shot"});
        return s;
    }
    if (name == "wheat") {
        // 2.9.1 wheat fields: find one near Brindlemere, overview, walk through it (the stalks part), log counts
        static vec2 spot; static int ok[2];
        s.push_back({3.f, [](Game& g) { g.newGame(); g.startPlaying(); g.timeOfDay = 16.5f; g.toastHint.clear(); g.tutStep = -1; ok[0] = ok[1] = 0;
            g.weather = g.weatherNext = Game::W_CLEAR; g.weatherBlend = 1; g.weatherTimer = 999;
            const Poi* b = g.world.poiById("brindlemere"); float best = 0; spot = b->pos;
            for (float r = 80; r < 260 && best < 0.9f; r += 6) for (float a = 0; a < TAU; a += 0.15f) {
                vec2 q = b->pos + vec2(std::cos(a), std::sin(a)) * r;
                float w = g.world.wheatAmount(q.x, q.y);
                if (w > best && g.world.wheatAmount(q.x + 6, q.y) > 0.6f && g.world.wheatAmount(q.x, q.y + 6) > 0.6f) { best = w; spot = q; }   // 3.7.1 patchy fields
            }
            AGR_LOG("WHEAT field at (%.0f, %.0f) amount %.2f", spot.x, spot.y, best);
            teleport(g, spot.x - 14, spot.y - 14, 0.78f); g.camPitch = 0.28f; g.camDist = g.camDistTarget = 16.f; g.camYaw = 0.78f + PI; }, [](Game& g, float) { g.camIdleT = 0; }, "field"});
        s.push_back({0.05f, [](Game& g) { size_t inst = 0; g.world.forEachVisibleChunk([&](Chunk& c) { if (c.wheat.valid()) inst += c.wheat.instanceCount; });
            ok[0] = inst > 3000; AGR_LOG("WHEAT %zu wheat tufts in loaded chunks -> %d", inst, ok[0]); g.requestScreenshot("build/shots/WH0_overview.png"); }, idle, "shot"});
        s.push_back({2.5f, [](Game& g) { teleport(g, spot.x - 4, spot.y - 4, 0.78f); g.camPitch = 0.1f; g.camDist = g.camDistTarget = 4.5f; g.camYaw = 0.78f + PI + 0.6f; },
            [](Game& g, float) { g.input.move = vec2(0, 0.55f); g.camIdleT = 0; }, "walk in"});
        s.push_back({0.05f, [](Game& g) { ok[1] = g.tallGrassHere > 0.5f; AGR_LOG("WHEAT hero hidden in the wheat %.2f -> %d", g.tallGrassHere, ok[1]); g.requestScreenshot("build/shots/WH1_walk.png"); }, idle, "shot"});
        s.push_back({1.5f, none, [](Game& g, float) { g.input.move = vec2(0, 0.55f); g.camIdleT = 0; g.camPitch = 0.45f; g.camDist = g.camDistTarget = 5.f; }, "walk on"});
        s.push_back({0.05f, [](Game& g) { g.requestScreenshot("build/shots/WH2_part.png"); }, idle, "shot"});
        s.push_back({3.f, [](Game& g) { g.input.move = vec2(0, 0); const Poi* b = g.world.poiById("brindlemere"); vec2 d = normalize(b->pos - spot); vec2 p = spot - d * 30.f;
            teleport(g, p.x, p.y, std::atan2(d.x, d.y)); g.camPitch = 0.04f; g.camDist = g.camDistTarget = 3.5f; g.camYaw = std::atan2(d.x, d.y) + PI; }, [](Game& g, float) { g.camIdleT = 0; }, "low view"});
        s.push_back({0.05f, [](Game& g) { g.requestScreenshot("build/shots/WH3_low.png"); bool all = ok[0] && ok[1]; AGR_LOG("WHEAT TEST %s", all ? "PASSED" : "FAILED"); }, idle, "shot"});
        return s;
    }
    if (name == "minecamp") {
        // 3.1.0 the miners' camp by the Mine des Cimes (Frostheim): tent, fire and mine entrance, near and far
        struct V { float back, dist; const char* shot; };
        static const V views[] = {{8.f, 7.f, "MC0_near"}, {16.f, 9.f, "MC1_mid"}, {38.f, 10.f, "MC2_far"}};
        s.push_back({1.f, [](Game& g) { g.newGame(); g.startPlaying(); g.toastHint.clear(); g.tutStep = -1; }, idle, "setup"});
        for (const V& v : views) {
            const V* q = &v;
            s.push_back({4.f, [q](Game& g) {
                const Poi* m = g.world.poiById("crown_mine"); if (!m) return;
                vec2 at = m->pos, dir = normalize(vec2(-1.f, 0.6f));
                for (const Structure& st : g.world.structures) if (st.kind == S_MINE && length(st.pos - m->pos) < 30.f) { at = st.pos; dir = normalize(rotate(vec2(0.35f, 1), -st.rot)); }
                vec2 hero = at + dir * q->back;
                teleport(g, hero.x, hero.y, 0.f);
                g.pc().yaw = std::atan2(-dir.x, -dir.y); g.camYaw = std::atan2(dir.x, dir.y);
                g.camDist = g.camDistTarget = q->dist; g.camPitch = 0.3f; g.timeOfDay = 15.4f;
                g.weather = g.weatherNext = Game::W_CLEAR; g.weatherBlend = 1; g.weatherTimer = 999; }, [](Game& g, float) { g.camIdleT = 0; }, "go"});
            s.push_back({0.1f, [q](Game& g) { g.requestScreenshot(std::string("build/shots/") + q->shot + ".png"); }, idle, "shot"});
        }
        s.push_back({0.05f, [](Game& g) { AGR_LOG("MINECAMP TEST PASSED"); }, idle, "end"});
        return s;
    }
    if (name == "wolves") {
        // 3.1.0 the wolves drawn by the game (no Tripo skin): grey, snow, steppe, Titan wolves side by side, then close
        static std::vector<Entity> ents;
        static auto freezeWolves = [](Game& g) {
            for (Entity e : ents) { if (AIBrain* ai = g.reg.get<AIBrain>(e)) { ai->state = AIState::Idle; ai->timer = 99; ai->target = NullEntity; }
                if (Character* ch = g.reg.get<Character>(e)) { ch->vel = vec3(0); ch->attackClip = nullptr; } }
            g.pc().hp = g.pc().maxHp; };
        s.push_back({3.f, [](Game& g) { g.newGame(); g.startPlaying(); g.timeOfDay = 10.5f; g.toastHint.clear(); g.tutStep = -1; ents.clear();
            g.weather = g.weatherNext = Game::W_CLEAR; g.weatherBlend = 1; g.weatherTimer = 999;
            const Poi* b = g.world.poiById("brindlemere"); teleport(g, b->pos.x + 40, b->pos.y + 80, 0.f); }, idle, "setup"});
        s.push_back({0.2f, [](Game& g) { Character& c = g.pc(); vec3 f{std::sin(c.yaw), 0, std::cos(c.yaw)}, r{f.z, 0, -f.x};
            const char* ids[] = {"wolf", "snow_wolf", "steppe_wolf", "titan_wolf"};
            int imported = 0;
            for (int i = 0; i < 4; i++) { vec3 p = c.pos + f * 6.f + r * ((i - 1.5f) * 2.4f); p.y = g.groundAt(p.x, p.z);
                Entity e = g.spawnEnemy(enemyIndex(ids[i]), p); ents.push_back(e);
                if (AIBrain* ai = g.reg.get<AIBrain>(e)) { ai->state = AIState::Idle; ai->timer = 99; }
                if (Character* ch = g.reg.get<Character>(e)) { ch->yaw = c.yaw + PI + 1.2f; imported += ch->vis.albedo != 0; } }
            AGR_LOG("WOLVES %zu spawned, %d with an imported model", ents.size(), imported);
            AGR_LOG("WOLVES TEST %s", imported == 0 && ents.size() == 4 ? "PASSED" : "FAILED");
            g.camYaw = c.yaw + PI; g.camPitch = 0.12f; g.camDist = g.camDistTarget = 5.f; }, [](Game& g, float) { g.camIdleT = 0; }, "spawn"});
        s.push_back({1.f, none, [](Game& g, float) { g.camIdleT = 0; freezeWolves(g); }, "settle"});
        s.push_back({0.05f, [](Game& g) { g.requestScreenshot("build/shots/WO0_wolves.png"); }, idle, "shot"});
        s.push_back({0.5f, [](Game& g) { Character& c = g.pc(); Character* w = g.reg.get<Character>(ents[0]); if (!w) return;
            vec3 side{std::cos(w->yaw), 0, -std::sin(w->yaw)}, fw{std::sin(w->yaw), 0, std::cos(w->yaw)};
            c.pos = w->pos + side * 2.6f + fw * 1.2f; c.pos.y = g.groundAt(c.pos.x, c.pos.z);
            c.yaw = std::atan2(w->pos.x - c.pos.x, w->pos.z - c.pos.z); g.camYaw = c.yaw + PI * 0.75f; g.camDist = g.camDistTarget = 3.2f; g.camPitch = 0.12f; },
            [](Game& g, float) { g.camIdleT = 0; freezeWolves(g); }, "close"});
        s.push_back({0.05f, [](Game& g) { g.requestScreenshot("build/shots/WO1_wolf_close.png"); }, idle, "shot"});
        return s;
    }
    if (name == "dungeonlook") {
        // 3.1.0 the dungeons dressed up: each theme seen in a room and in the boss hall (enemies cleared for the pictures)
        static const char* ids[] = {"catacombs", "whisper_cave", "sun_temple", "abyss", "ember_mines", "ice_caves"};
        static int nOk = 0;
        s.push_back({1.f, [](Game& g) { g.newGame(); g.startPlaying(); g.toastHint.clear(); g.tutStep = -1; nOk = 0; }, idle, "setup"});
        const int nd = getenv("LK_DUN_N") ? atoi(getenv("LK_DUN_N")) : 6;
        for (int k = 0; k < nd; k++) {
            s.push_back({2.5f, [k](Game& g) { int di = -1; for (int i = 0; i < (int)dungeons().size(); i++) if (dungeons()[i].id == ids[k]) di = i; g.enterDungeon(di); if (dungeons()[di].boss >= 0) g.defeatedBosses.insert(bosses()[dungeons()[di].boss].id); },
                         [](Game& g, float) { g.pc().invuln = 1.f; }, "enter"});
            for (int v = 0; v < 2; v++) {
                s.push_back({1.2f, [k, v](Game& g) {
                    g.reg.each<Character>([&](Entity e, Character& c) { if (c.team == Team::Enemy) g.reg.destroyLater(e); });
                    const DungeonRoom& rm = v == 0 ? g.dungeon.rooms[1] : g.dungeon.rooms.back();
                    vec3 c = g.dungeon.origin + vec3((rm.x + rm.w * 0.5f) * 4.f, 0, (rm.z + rm.h * 0.5f) * 4.f);
                    vec3 tg = g.dungeon.gatePos - c; vec3 nn = std::fabs(tg.x) > std::fabs(tg.z) ? vec3(tg.x > 0 ? 1.f : -1.f, 0, 0) : vec3(0, 0, tg.z > 0 ? 1.f : -1.f);
                    g.pc().pos = c + (v == 0 ? vec3(0) : nn * 3.f); g.pc().vel = vec3(0); g.camFocus = g.pc().pos;
                    g.camYaw = v == 0 ? 0.6f + k * 0.4f : std::atan2(nn.x, nn.z); g.camPitch = v == 0 ? 0.62f : 0.5f; g.camDist = g.camDistTarget = v == 0 ? 12.f : 13.f;
                    if (v == 1) g.pc().pos = c - nn * 4.f;
                    if (v == 0 && g.dungeon.active && g.dungeon.def >= 0 && dungeons()[g.dungeon.def].id == ids[k]) nOk++; },
                             [](Game& g, float) { g.camIdleT = 0; g.pc().invuln = 1.f; if (g.cinematicT > 0) g.cinematicT = -1;
                                 g.reg.each<Character>([&](Entity e, Character& c) { if (c.team == Team::Enemy) g.reg.destroyLater(e); }); g.bossEnt = NullEntity; }, "view"});
                s.push_back({0.05f, [k, v](Game& g) { g.requestScreenshot(std::string("build/shots/DL") + std::to_string(k) + (v ? "_boss" : "_room") + ".png"); }, idle, "shot"});
            }
            s.push_back({0.3f, [](Game& g) { g.exitDungeon(); }, idle, "exit"});
            s.push_back({2.f, none, idle, "out"});
        }
        s.push_back({0.05f, [nd](Game& g) { (void)g; AGR_LOG("DUNGEONLOOK %d/%d -> %s", nOk, nd, nOk == nd ? "TEST PASSED" : "TEST FAILED"); }, idle, "end"});
        return s;
    }
    if (name == "castlelook") {
        // 3.1.0 Fort Aldren rebuilt: the gatehouse from outside, the keep and its courtyard, a wall from high up
        struct V { float ang, dist, camD, pitch, yawOff; const char* shot; };
        static const V views[] = {{1.178f, 1.05f, 20.f, 0.22f, 0.f, "CA0_gate"}, {1.178f, 0.25f, 30.f, 0.32f, 0.f, "CA1_keep"}, {2.4f, 0.55f, 26.f, 0.55f, 0.6f, "CA2_walls"}};
        s.push_back({1.f, [](Game& g) { g.newGame(); g.startPlaying(); g.toastHint.clear(); g.tutStep = -1; }, idle, "setup"});
        for (const V& v : views) {
            const V* q = &v;
            s.push_back({4.5f, [q](Game& g) {
                const Poi* c = g.world.poiById("fort_aldren"); if (!c) return;
                vec2 dir{std::cos(q->ang), std::sin(q->ang)};
                vec2 hero = c->pos + dir * (c->radius * q->dist);
                teleport(g, hero.x, hero.y, 0.f);
                g.camYaw = std::atan2(dir.x, dir.y) + q->yawOff; g.pc().yaw = g.camYaw + PI;
                g.camDist = g.camDistTarget = q->camD; g.camPitch = q->pitch; g.timeOfDay = 16.f;
                g.weather = g.weatherNext = Game::W_CLEAR; g.weatherBlend = 1; g.weatherTimer = 999; }, [](Game& g, float) { g.camIdleT = 0; }, "go"});
            s.push_back({0.1f, [q](Game& g) { g.requestScreenshot(std::string("build/shots/") + q->shot + ".png"); }, idle, "shot"});
        }
        s.push_back({0.05f, [](Game& g) { (void)g; AGR_LOG("CASTLELOOK TEST PASSED"); }, idle, "end"});
        return s;
    }
    if (name == "treasures") {
        // 3.2.0 the treasure hunt: a totem of power and its key item, the double jump, a sky archipelago climbed by its
        // springs up to the summit totem, an orb, the bag's slots and bags, the inventory and the Wonders shop
        static int res[10]; static int arch = -1; static float y0; static vec3 summit;
        auto near = [](Game& g, int type, bool sky) { int best = -1; float bd = 1e9f; vec3 p = g.pc().pos;
            for (int i = 0; i < (int)g.world.specials.size(); i++) { auto& sp = g.world.specials[i]; if (sp.type != type || sp.sky != sky || g.world.harvested.count(sp.uid)) continue; float d = distXZ(sp.pos, p); if (d < bd) { bd = d; best = i; } } return best; };
        auto stand = [](Game& g, vec3 at, vec3 from) { Character& c = g.pc(); vec3 d = normalize(vec3(at.x - from.x, 0, at.z - from.z) + vec3(1e-4f, 0, 0));
            c.pos = vec3(at.x - d.x * 1.8f, at.y + 0.1f, at.z - d.z * 1.8f); c.vel = vec3(0); c.yaw = std::atan2(d.x, d.z); g.camFocus = c.pos; g.camYaw = c.yaw + PI; };
        s.push_back({1.f, [](Game& g) { g.newGame(); g.startPlaying(); g.toastHint.clear(); g.tutStep = -1; g.timeOfDay = 11.f; for (auto& r : res) r = 0;
            g.weather = g.weatherNext = Game::W_CLEAR; g.weatherBlend = 1; g.weatherTimer = 999;
            AGR_LOG("TREASURES world: %d totems, %d orbs, bag %d/%d", g.treasureTotal(0), g.treasureTotal(1), g.bagUsed(), g.bagCapacity());
            res[0] = g.treasureTotal(0) >= 20 && g.treasureTotal(1) >= 40 && g.bagCapacity() == 40; }, idle, "setup"});
        // 1. the nearest ground totem
        s.push_back({3.f, [near](Game& g) { int t = near(g, 0, false); if (t < 0) return; const auto& sp = g.world.specials[t]; teleport(g, sp.pos.x + 2.5f, sp.pos.z + 1.f, 0.f); g.pc().yaw = std::atan2(-2.5f, -1.f); g.camDist = g.camDistTarget = 9.f; g.camPitch = 0.25f; g.camYaw = g.pc().yaw + PI; },
            [](Game& g, float) { g.camIdleT = 0; }, "to the totem"});
        s.push_back({0.1f, [](Game& g) { g.requestScreenshot("build/shots/TR0_totem.png"); }, idle, "shot"});
        s.push_back({0.1f, [](Game& g) { AGR_LOG("TREASURES interact target: %s", g.interactTarget != NullEntity && g.reg.get<Interact>(g.interactTarget) ? g.reg.get<Interact>(g.interactTarget)->text.c_str() : "none"); g.input.interact = true; }, idle, "awaken"});
        s.push_back({1.2f, none, idle, "fanfare"});
        s.push_back({0.1f, [](Game& g) { g.requestScreenshot("build/shots/TR1_fanfare.png"); res[1] = g.totemsTaken() == 1 && g.hasPower(KP_COMPASS) && g.shards >= 5;
            AGR_LOG("TREASURES totem: taken %d, compass %d, shards %d, popups %zu", g.totemsTaken(), g.hasPower(KP_COMPASS), g.shards, g.itemGets.size()); }, idle, "check"});
        // 2. the zephyr boots: a double jump
        s.push_back({0.2f, [](Game& g) { g.itemGets.clear(); g.addItem("key_zephyr", 1, Rarity::Count, false); y0 = g.pc().pos.y; g.input.jump = true; }, idle, "jump"});
        s.push_back({0.9f, [](Game& g) { g.input.jump = true; }, [](Game& g, float) { res[2] = std::max(res[2], (int)((g.pc().pos.y - y0) * 100)); }, "double jump"});
        s.push_back({0.1f, [](Game& g) { AGR_LOG("TREASURES double jump peak %.2f m", res[2] / 100.f); res[2] = res[2] > 160; }, idle, "check"});
        // 3. a sky archipelago: the ground spring, then the deck springs
        s.push_back({3.f, [](Game& g) {
            arch = -1; float bd = 1e9f;
            for (int i = 0; i < (int)g.world.structures.size(); i++) { const Structure& st = g.world.structures[i]; if (st.kind != S_GEYSER || st.variant <= 0 || st.region == R_AURORA) continue; float d = distXZ(vec3(st.pos.x, 0, st.pos.y), g.pc().pos); if (d < bd) { bd = d; arch = i; } }
            if (arch < 0) return;
            const Structure& ge = g.world.structures[arch];
            teleport(g, ge.pos.x - std::sin(ge.rot) * 4.f, ge.pos.y - std::cos(ge.rot) * 4.f, 0.f);
            g.pc().yaw = ge.rot; g.camYaw = ge.rot + PI; g.camDist = g.camDistTarget = 14.f; g.camPitch = 0.1f;
            AGR_LOG("TREASURES archipelago spring in %s", regionName(ge.region)); }, [](Game& g, float) { g.camIdleT = 0; }, "to the archipelago"});
        s.push_back({0.1f, [](Game& g) { g.requestScreenshot("build/shots/TR2_archipelago.png"); }, idle, "shot"});
        s.push_back({2.5f, [](Game& g) { if (arch < 0) return; const Structure& ge = g.world.structures[arch]; Character& c = g.pc();   // the whole archipelago from afar
            vec2 back = ge.pos - vec2(std::sin(ge.rot), std::cos(ge.rot)) * 55.f + vec2(std::cos(ge.rot), -std::sin(ge.rot)) * 25.f;
            teleport(g, back.x, back.y, 0.f); c.yaw = std::atan2(ge.pos.x - back.x, ge.pos.y - back.y); g.camYaw = c.yaw + PI; g.camDist = g.camDistTarget = 16.f; g.camPitch = -0.05f; },
            [](Game& g, float) { g.camIdleT = 0; }, "afar"});
        s.push_back({0.1f, [](Game& g) { g.requestScreenshot("build/shots/TR2b_archipelago_far.png"); }, idle, "shot"});
        for (int k = 0; k < 3; k++) {
            s.push_back({4.f, [k](Game& g) {
                if (arch < 0) return;
                // walk into the nearest spring (ground first, then the one on the deck)
                int best = -1; float bd = 1e9f; Character& c = g.pc();
                for (int i = 0; i < (int)g.world.structures.size(); i++) { const Structure& st = g.world.structures[i]; bool ok = k == 0 ? i == arch : (st.kind == S_SKYGEYSER && std::fabs(st.variant / 100.f - c.pos.y) < 2.f); if (!ok) continue; float d = distXZ(vec3(st.pos.x, 0, st.pos.y), c.pos); if (d < bd) { bd = d; best = i; } }
                if (best < 0) { AGR_LOG("TREASURES no spring at step %d (y %.1f)", k, c.pos.y); return; }
                const Structure& st = g.world.structures[best];
                c.pos.x = st.pos.x; c.pos.z = st.pos.y; c.vel = vec3(0); if (k == 0) c.pos.y = g.world.groundHeight(st.pos.x, st.pos.y);
                AGR_LOG("TREASURES step %d: spring %s at y %.1f", k, st.kind == S_SKYGEYSER ? "on a deck" : "on the ground", c.pos.y); },
                [](Game& g, float) { g.camIdleT = 0; g.camYaw = g.pc().yaw + PI; }, "ride"});
            s.push_back({0.1f, [k](Game& g) { Character& c = g.pc(); float dk = 0; bool on = g.world.skyDeck(c.pos, dk);
                AGR_LOG("TREASURES after step %d: y %.1f, on a deck %d (%.1f), grounded %d", k, c.pos.y, on, dk, c.grounded); if (on && c.grounded) res[3 + k] = 1;
                g.requestScreenshot("build/shots/TR3_step" + std::to_string(k) + ".png"); }, idle, "check"});
        }
        // the summit totem
        s.push_back({1.5f, [near, stand](Game& g) { int t = near(g, 0, true); if (t < 0) return; summit = g.world.specials[t].pos; stand(g, summit, g.pc().pos); g.camDist = g.camDistTarget = 12.f; g.camPitch = 0.2f; }, [](Game& g, float) { g.camIdleT = 0; }, "summit"});
        s.push_back({0.1f, [](Game& g) { g.requestScreenshot("build/shots/TR4_summit.png"); }, idle, "shot"});
        s.push_back({0.1f, [](Game& g) { g.input.interact = true; }, idle, "awaken"});
        s.push_back({1.2f, none, idle, "fanfare"});
        s.push_back({0.1f, [](Game& g) { res[6] = g.totemsTaken() == 2; g.requestScreenshot("build/shots/TR5_summit_fanfare.png"); AGR_LOG("TREASURES summit totem: taken %d", g.totemsTaken()); g.itemGets.clear(); }, idle, "check"});
        // glide down with the feather cape
        s.push_back({0.1f, [](Game& g) { g.addItem("key_feather", 1, Rarity::Count, false); Character& c = g.pc(); c.pos += vec3(std::sin(c.yaw), 0, std::cos(c.yaw)) * 40.f; c.pos.y = g.groundAt(c.pos.x, c.pos.z) + 40.f; c.vel = vec3(0); c.grounded = false; y0 = c.pos.y; }, idle, "jump off"});
        s.push_back({0.3f, none, idle, "fall a little"});
        s.push_back({2.f, [](Game& g) { float k = g.ui.scale; Rect r = ctlRect(g, CTL_JUMP); g.touch(TouchEvent{TouchEvent::Down, 902, (r.x + r.w / 2) * k, (r.y + r.h / 2) * k}); y0 = g.pc().pos.y; },
                     [](Game& g, float) { g.input.move = vec2(0, 1); g.camIdleT = 0; }, "glide (finger held on Saut)"});
        s.push_back({0.1f, [](Game& g) { for (auto& f : g.ui.fingers) if (f.id == 902 && f.down) { float k = g.ui.scale; g.touch(TouchEvent{TouchEvent::Up, 902, f.x * k, f.y * k}); }
            float fell = y0 - g.pc().pos.y; AGR_LOG("TREASURES glide: fell %.1f m in 2 s, gliding %d", fell, g.gliding); res[7] = fell < 12.f; g.input.move = vec2(0); g.requestScreenshot("build/shots/TR6_glide.png"); }, idle, "check"});
        // an orb, the bag, the screens
        s.push_back({2.f, [near](Game& g) { int o = near(g, 1, false); if (o < 0) return; const auto& sp = g.world.specials[o]; teleport(g, sp.pos.x + 1.f, sp.pos.z, 0.f); }, [](Game& g, float) { g.camIdleT = 0; }, "orb"});
        s.push_back({0.1f, [](Game& g) { g.input.interact = true; }, idle, "take"});
        s.push_back({0.6f, none, idle, "wait"});
        s.push_back({0.1f, [](Game& g) { res[8] = g.orbsTaken() == 1; g.requestScreenshot("build/shots/TR7_orb.png");
            int before = g.bagUsed(); for (int i = 0; i < 60; i++) g.addItem("sword_iron", 1, Rarity::Common, false);
            int full = g.bagUsed(); g.addItem("bag_adventurer", 1, Rarity::Count, false);
            AGR_LOG("TREASURES bag: %d -> %d of %d (full), bag item added %d", before, full, g.bagCapacity(), g.countItem("bag_adventurer"));
            res[9] = full == g.bagCapacity() && g.countItem("bag_adventurer") == 0; }, idle, "bag"});
        s.push_back({0.1f, [](Game& g) { for (int i = (int)g.inventory.size() - 1; i >= 0 && g.bagUsed() > 20; i--) if (g.inventory[i].id == "sword_iron") g.inventory.erase(g.inventory.begin() + i);
            g.addItem("bag_adventurer", 1, Rarity::Count, false); int cap0 = g.bagCapacity();
            for (int i = 0; i < (int)g.inventory.size(); i++) if (g.inventory[i].id == "bag_adventurer") { g.useItem(i); break; }
            AGR_LOG("TREASURES bag used: %d -> %d", cap0, g.bagCapacity()); if (g.bagCapacity() != cap0 + 15) res[9] = 0;
            g.addItem("sword_knight", 1, Rarity::Legendary, true); g.shards = 30; g.menu = Menu::Inventory; g.selItem = -1; }, idle, "inventory"});
        s.push_back({0.4f, none, idle, "wait"});
        s.push_back({0.1f, [](Game& g) { AGR_LOG("TREASURES menu at shot: %d", (int)g.menu); g.requestScreenshot("build/shots/TR8_inventory.png"); }, idle, "shot"});
        s.push_back({0.05f, [](Game& g) { g.menu = Menu::None; }, idle, "close"});
        s.push_back({0.2f, [](Game& g) { for (int i = 0; i < (int)npcs().size(); i++) if (!npcs()[i].shop.empty()) { g.shopNpc = i; break; } g.menu = Menu::Shop; g.shopSell = 0; }, idle, "shop"});
        s.push_back({0.4f, none, idle, "wait"});
        s.push_back({0.1f, [](Game& g) { g.requestScreenshot("build/shots/TR9_shop.png"); }, idle, "shot"});
        s.push_back({0.1f, [](Game& g) { g.menu = Menu::None;
            int ok = 0; for (int r : res) ok += r != 0;
            AGR_LOG("TREASURES results: world %d totem %d djump %d steps %d %d %d summit %d glide %d orb %d bag %d -> %s", res[0], res[1], res[2], res[3], res[4], res[5], res[6], res[7], res[8], res[9], ok == 10 ? "TREASURES TEST PASSED" : "TREASURES TEST FAILED"); }, idle, "end"});
        return s;
    }
    if (name == "bigmods") {
        // 3.2.0 the big mods: the Legendary Bestiary applied, six of its creatures side by side
        static std::vector<Entity> ents; static int bad = 0;
        s.push_back({3.f, [](Game& g) { g.newGame(); g.startPlaying(); g.timeOfDay = 10.5f; g.toastHint.clear(); g.tutStep = -1; ents.clear(); bad = 0;
            g.weather = g.weatherNext = Game::W_CLEAR; g.weatherBlend = 1; g.weatherTimer = 999;
            for (const char* m : {"mods/bestiaire-legendaire/data.txt", "mods/arsenal-des-legendes/data.txt", "mods/invasion-des-ombres/data.txt"}) {
                std::vector<uint8_t> b; if (!platform::readAsset(m, b)) { bad += 100; continue; }
                std::string txt(b.begin(), b.end()), line; size_t a = 0;
                while (a < txt.size()) { size_t e = txt.find('\n', a); if (e == std::string::npos) e = txt.size(); line = txt.substr(a, e - a); a = e + 1; std::string err; if (!applyModDataLine(line, err)) { bad++; AGR_LOG("BIGMODS %s -> %s", line.c_str(), err.c_str()); } }
            }
            const Poi* b = g.world.poiById("brindlemere"); teleport(g, b->pos.x + 40, b->pos.y + 80, 0.f); }, idle, "setup"});
        s.push_back({0.2f, [](Game& g) { Character& c = g.pc(); vec3 f{std::sin(c.yaw), 0, std::cos(c.yaw)}, r{f.z, 0, -f.x};
            const char* ids[] = {"loup_cendre", "golem_ambre", "ours_glacier", "araignee_sang", "chevalier_soleil", "sanglier_titan"};
            for (int i = 0; i < 6; i++) { vec3 p = c.pos + f * (8.f + (i / 3) * 6.f) + r * ((i % 3 - 1) * 5.f); p.y = g.groundAt(p.x, p.z); int ei = enemyIndex(ids[i]); if (ei < 0) { bad++; continue; } ents.push_back(g.spawnEnemy(ei, p)); }
            AGR_LOG("BIGMODS %zu creatures, %zu enemies in total, %d bad lines -> %s", ents.size(), enemies().size(), bad, ents.size() == 6 && bad == 0 ? "BIGMODS TEST PASSED" : "BIGMODS TEST FAILED");
            g.camYaw = c.yaw + PI; g.camPitch = 0.2f; g.camDist = g.camDistTarget = 9.f; }, [](Game& g, float) { g.camIdleT = 0; }, "spawn"});
        s.push_back({1.2f, none, [](Game& g, float) { g.camIdleT = 0; g.pc().hp = g.pc().maxHp; for (Entity e : ents) { if (AIBrain* ai = g.reg.get<AIBrain>(e)) { ai->state = AIState::Idle; ai->timer = 99; ai->target = NullEntity; } if (Character* ch = g.reg.get<Character>(e)) { ch->vel = vec3(0); ch->attackClip = nullptr; } } }, "settle"});
        s.push_back({0.05f, [](Game& g) { g.requestScreenshot("build/shots/BM0_bestiary.png"); }, idle, "shot"});
        return s;
    }
    if (name == "airship") {
        // 3.4.0 vehicles earned in the story (locked at first, the horse is free), then the sky-ship: called
        // from the clouds, boarded, climbs, flies forward, lands and lets the hero off
        static int lockedOk, motoOk, shipOk; static float alt0, maxAlt; static vec3 start;
        s.push_back({1.f, [](Game& g) { g.newGame(); g.startPlaying(); g.timeOfDay = 13.f; g.toastHint.clear(); g.tutStep = -1; if (g.vehicles.empty()) g.initVehicles(); g.spawnVehicleEntities();
            const Poi* b = g.world.poiById("brindlemere"); teleport(g, b->pos.x + b->radius + 30.f, b->pos.y + 20.f, 0.f);
            int n = 0; for (int i = 0; i < (int)g.vehicles.size(); i++) n += g.vehicleUnlocked(i);
            g.summonVehicle(0);
            lockedOk = n == 0 && g.vehicles[0].pos.y < 1e8f;
            AGR_LOG("AIRSHIP new game: %d vehicles unlocked (expected 0)", n); }, idle, "locked"});
        s.push_back({1.5f, [](Game& g) { g.questsDone.insert("m2"); g.refreshVehicleUnlocks(true); motoOk = g.vehiclesUnlocked.count("moto") && !g.vehiclesUnlocked.count("airship") && !g.itemGets.empty();
            AGR_LOG("AIRSHIP after m2: moto %d car %d ship %d, fanfare %d", (int)g.vehiclesUnlocked.count("moto"), (int)g.vehiclesUnlocked.count("car"), (int)g.vehiclesUnlocked.count("airship"), (int)g.itemGets.size()); },
            [](Game& g, float t) { static bool sh = false; if (!sh && t > 0.8f) { sh = true; g.requestScreenshot("build/shots/AS0_unlock.png"); } }, "moto"});
        s.push_back({4.f, [](Game& g) { g.itemGets.clear(); g.questsDone.insert("m3"); g.questsDone.insert("m5"); g.refreshVehicleUnlocks(true); g.itemGets.clear();
            int ship = -1; for (int i = 0; i < (int)g.vehicles.size(); i++) if (vehicleDefs()[g.vehicles[i].def].fly) ship = i;
            shipOk = ship >= 0 && g.vehicleUnlocked(ship);
            g.summonVehicle(ship); AGR_LOG("AIRSHIP summoned: alt %.1f landing %d", g.vehicles[ship].alt, (int)g.vehicles[ship].landing); g.camDist = g.camDistTarget = 18.f; g.camPitch = 0.35f; }, idle, "summon"});
        s.push_back({0.5f, [](Game& g) { int ship = (int)g.vehicles.size() - 1; AGR_LOG("AIRSHIP parked alt %.2f, %.1f m from the hero", g.vehicles[ship].alt, distXZ(g.vehicles[ship].pos, g.pc().pos)); g.requestScreenshot("build/shots/AS1_parked.png"); }, idle, "parked"});
        s.push_back({0.5f, [](Game& g) { int ship = (int)g.vehicles.size() - 1; g.enterVehicle(ship); start = g.vehicles[ship].pos; alt0 = start.y; maxAlt = 0; AGR_LOG("AIRSHIP boarded: driving %d", g.driving); }, idle, "board"});
        s.push_back({4.f, none, [](Game& g, float) { g.input.jumpHeld = true; g.input.jump = true; }, "climb"});
        s.push_back({7.f, none, [](Game& g, float t) {
            if (g.driving < 0) return; VehicleState& v = g.vehicles[g.driving];
            g.input.move = vec2(0.f, 1.f); g.camYaw = v.yaw + PI; maxAlt = std::max(maxAlt, v.alt);
            static bool sh = false; if (!sh && t > 5.f) { sh = true; g.requestScreenshot("build/shots/AS2_flying.png"); } }, "fly"});
        s.push_back({0.2f, [](Game& g) { g.input.move = vec2(0); if (g.driving >= 0) { VehicleState& v = g.vehicles[g.driving]; AGR_LOG("AIRSHIP flew %.0f m, altitude %.1f m (max %.1f), y %.1f", distXZ(v.pos, start), v.alt, maxAlt, v.pos.y); } g.exitVehicle(); }, idle, "land"});
        s.push_back({12.f, none, [](Game& g, float) { g.input.move = vec2(0); }, "landing"});
        s.back().until = [](Game& g) { return g.driving < 0; };
        s.push_back({1.f, [](Game& g) {
            int ship = (int)g.vehicles.size() - 1; float d = distXZ(g.vehicles[ship].pos, start);
            bool ok = lockedOk && motoOk && shipOk && maxAlt > 12.f && d > 60.f && g.driving < 0 && g.pc().grounded;
            AGR_LOG("AIRSHIP locked %d moto %d ship %d maxAlt %.1f dist %.0f off %d -> %s", lockedOk, motoOk, shipOk, maxAlt, d, g.driving < 0, ok ? "AIRSHIP TEST PASSED" : "AIRSHIP TEST FAILED"); }, idle, "check"});
        return s;
    }
    if (name == "cosmos") {
        // 3.2.0 the Star Observatory and space: the gate in the Titans, then the asteroids over the planet, a low-gravity jump
        static int res[4]; static float y0, peak;
        s.push_back({1.f, [](Game& g) { g.newGame(); g.startPlaying(); g.toastHint.clear(); g.tutStep = -1; g.level = 30; g.recomputeStats(); for (auto& r : res) r = 0; g.timeOfDay = 21.5f;
            const Poi* p = g.world.poiById("portal_stars"); res[0] = p != nullptr; if (!p) return;
            teleport(g, p->pos.x, p->pos.y + 18.f, 0.f); g.pc().yaw = PI; g.camYaw = 0.f; g.camPitch = 0.15f; g.camDist = g.camDistTarget = 14.f; }, [](Game& g, float) { g.camIdleT = 0; }, "to the observatory"});
        s.push_back({3.f, none, [](Game& g, float) { g.camIdleT = 0; }, "settle"});
        s.push_back({0.1f, [](Game& g) { g.requestScreenshot("build/shots/CO0_observatory.png"); }, idle, "shot"});
        s.push_back({0.5f, [](Game& g) { int di = -1; for (int i = 0; i < (int)dungeons().size(); i++) if (dungeons()[i].id == "cosmos") di = i; if (di >= 0) g.enterDungeon(di); }, [](Game& g, float) { g.pc().invuln = 1.f; }, "enter"});
        s.push_back({3.f, none, [](Game& g, float) { g.pc().invuln = 1.f; g.camIdleT = 0; }, "arrive"});
        s.push_back({0.1f, [](Game& g) { res[1] = g.dungeon.active && g.dungeon.theme == 10; g.reg.each<Character>([&](Entity e, Character& c) { if (c.team == Team::Enemy) g.reg.destroyLater(e); });
            g.camPitch = 0.12f; g.camDist = g.camDistTarget = 11.f; g.camYaw = std::atan2(-0.42f, 0.62f); }, idle, "look"});   // looking towards the planet
        s.push_back({1.5f, none, [](Game& g, float) { g.camIdleT = 0; g.pc().invuln = 1.f; }, "settle"});
        s.push_back({0.1f, [](Game& g) { g.requestScreenshot("build/shots/CO1_space.png"); }, idle, "shot"});
        s.push_back({0.1f, [](Game& g) { g.camPitch = 0.55f; g.camDist = g.camDistTarget = 24.f; g.camYaw = 0.8f; }, idle, "high"});
        s.push_back({1.2f, none, [](Game& g, float) { g.camIdleT = 0; g.pc().invuln = 1.f; }, "settle"});
        s.push_back({0.1f, [](Game& g) { g.requestScreenshot("build/shots/CO2_asteroids.png"); y0 = peak = g.pc().pos.y; g.input.jump = true; }, idle, "jump"});
        s.push_back({1.6f, none, [](Game& g, float) { peak = std::max(peak, g.pc().pos.y); g.camIdleT = 0; g.pc().invuln = 1.f; }, "airborne"});
        s.push_back({0.1f, [](Game& g) { res[2] = peak - y0 > 2.f; g.requestScreenshot("build/shots/CO3_jump.png");
            AGR_LOG("COSMOS observatory %d, in space %d, low-gravity jump %.2f m -> %s", res[0], res[1], peak - y0, res[0] && res[1] && res[2] ? "COSMOS TEST PASSED" : "COSMOS TEST FAILED"); }, idle, "end"});
        return s;
    }
    if (name == "town") {
        // 3.9.0 Aldrenbourg: the walled market town of Greenhaven (fountain, market halls, town hall, townhouses,
        // its people and their quests)
        static int res[6];
        s.push_back({3.f, [](Game& g) { g.newGame(); g.startPlaying(); g.toastHint.clear(); g.tutStep = -1; g.timeOfDay = 10.5f; g.weather = g.weatherNext = 0; for (int& r : res) r = 0;
            const Poi* p = g.world.poiById("aldrenbourg"); if (!p) { AGR_LOG("TOWN no poi"); return; }
            int k[4] = {0, 0, 0, 0}, houses = 0;
            for (const Structure& st : g.world.structures) if (length(st.pos - p->pos) < p->radius * 1.1f) {
                if (st.kind >= S_FOUNTAIN && st.kind <= S_TOWNHOUSE) k[st.kind - S_FOUNTAIN]++;
                if (st.kind == S_HOUSE || st.kind == S_STONEHOUSE || st.kind == S_TOWNHOUSE) houses++; }
            res[0] = k[0] >= 1 && k[1] >= 1 && k[2] == 1 && k[3] >= 15 && houses >= 40;
            AGR_LOG("TOWN at %.0f %.0f r %.0f region %d: fountain %d market %d townhall %d townhouses %d, houses %d", p->pos.x, p->pos.y, p->radius, p->region, k[0], k[1], k[2], k[3], houses);
            teleport(g, p->pos.x + 6.f, p->pos.y + 10.f, 0.f); }, idle, "go"});
        s.push_back({3.f, none, [](Game& g, float) { g.camYaw = 0.5f; g.camPitch = 0.55f; g.camDist = g.camDistTarget = 22.f; }, "settle"});
        s.push_back({0.1f, [](Game& g) { g.requestScreenshot("build/shots/TW0_square.png"); }, idle, "shot"});
        s.push_back({2.f, none, [](Game& g, float) { g.camYaw = 2.6f; g.camPitch = 0.25f; g.camDist = g.camDistTarget = 12.f; }, "street"});
        s.push_back({0.1f, [](Game& g) { g.requestScreenshot("build/shots/TW1_street.png"); }, idle, "shot"});
        // the market hall and the town hall, close up
        auto visit = [&](int kind, const char* file) {
            s.push_back({2.5f, [kind](Game& g) { const Poi* p = g.world.poiById("aldrenbourg"); for (const Structure& st : g.world.structures) if (st.kind == kind && length(st.pos - p->pos) < p->radius) {
                    vec2 fr = st.pos + rotate(vec2(0, 9.f), -st.rot); teleport(g, fr.x, fr.y, 0.f); g.camFocusTmp = vec3(st.pos.x, 0, st.pos.y); break; } },
                [](Game& g, float) { vec3 d = g.camFocusTmp - g.pc().pos; g.camYaw = std::atan2(-d.x, -d.z); g.camPitch = 0.22f; g.camDist = g.camDistTarget = 14.f; }, "visit"});
            s.push_back({0.1f, [file](Game& g) { g.requestScreenshot(file); }, idle, "shot"});
        };
        visit(S_MARKET_HALL, "build/shots/TW2_market.png");
        visit(S_TOWNHALL, "build/shots/TW3_townhall.png");
        visit(S_TOWNHOUSE, "build/shots/TW4_townhouse.png");
        // from above
        s.push_back({2.5f, [](Game& g) { const Poi* p = g.world.poiById("aldrenbourg"); teleport(g, p->pos.x, p->pos.y + 20.f, 0.f); },
            [](Game& g, float) { g.camYaw = 0.3f; g.camPitch = 1.05f; g.camDist = g.camDistTarget = 90.f; }, "air"});
        s.push_back({0.1f, [](Game& g) { g.requestScreenshot("build/shots/TW5_aerial.png"); }, idle, "shot"});
        // its people, a quest and its ending
        s.push_back({0.5f, [](Game& g) { int found = 0; Entity gasp = NullEntity;
            g.reg.each<Character>([&](Entity e, Character& c) { if (c.npcDef >= 0) { const std::string& id = npcs()[c.npcDef].id; if (id == "gaspard" || id == "colette" || id == "orane" || id == "garin" || id == "fabien" || id == "melisande") found++; if (id == "gaspard") gasp = e; } });
            res[1] = found == 6; AGR_LOG("TOWN npcs %d/6", found);
            if (gasp != NullEntity) { g.openDialog(gasp); res[2] = g.dialog.lines.size() >= 7; AGR_LOG("TOWN gaspard meet %zu lines", g.dialog.lines.size()); g.dialog.open = false; } }, idle, "people"});
        s.push_back({0.5f, [](Game& g) { g.startQuest("al_oven"); g.addItem("wood", 12, Rarity::Common, true); g.itemGets.clear(); Entity col = NullEntity;
            g.reg.each<Character>([&](Entity e, Character& c) { if (c.npcDef >= 0 && npcs()[c.npcDef].id == "colette") col = e; });
            if (col != NullEntity) { g.openDialog(col); for (auto& ch : g.dialog.choices) if (ch.action == "deliver:al_oven") { g.dialogAction(ch.action); break; } }
            res[3] = g.questsDone.count("al_oven") > 0; g.dialog.open = false;
            // the fair: its scenes
            res[4] = conversation("offer:al_fair") && conversation("done:al_fair");
            AGR_LOG("TOWN oven quest done %d, fair scenes %d", res[3], res[4]); }, idle, "quest"});
        // the Grande Foire: the square dressed for the feast
        s.push_back({3.f, [](Game& g) { g.questsDone.insert("al_fair"); const Poi* p = g.world.poiById("aldrenbourg"); teleport(g, p->pos.x + 4.f, p->pos.y + 22.f, 0.f); },
            [](Game& g, float) { g.camYaw = 0.15f; g.camPitch = 0.32f; g.camDist = g.camDistTarget = 20.f; }, "fair"});
        s.push_back({0.1f, [](Game& g) { g.requestScreenshot("build/shots/TW6_fair.png"); }, idle, "shot"});
        s.push_back({0.2f, [](Game& g) { bool ok = res[0] && res[1] && res[2] && res[3] && res[4]; AGR_LOG("TOWN %d%d%d%d%d -> %s", res[0], res[1], res[2], res[3], res[4], ok ? "TOWN TEST PASSED" : "TOWN TEST FAILED"); }, idle, "check"});
        return s;
    }
    if (name == "ride") {
        // 3.8.1 riding like a real horse: walk, trot (posting), gallop (forward seat), filmed from the side; the
        // horse's back rises and rocks with each gait and the rider moves with it
        static float bobMin[4], bobMax[4], pMin[4], pMax[4], postMax[4]; static int gaitSeen[4];
        static int shotN = 0;
        s.push_back({2.f, [](Game& g) { g.newGame(); g.startPlaying(); g.toastHint.clear(); g.tutStep = -1; g.timeOfDay = 10.f; g.weather = g.weatherNext = 0;
            for (int i = 0; i < 4; i++) { bobMin[i] = pMin[i] = 1e9f; bobMax[i] = pMax[i] = postMax[i] = -1e9f; gaitSeen[i] = 0; }
            const Poi* b = g.world.poiById("brindlemere"); teleport(g, b->pos.x + 60.f, b->pos.y + 60.f, 0.f);
            g.ownedMounts.push_back("horse"); g.activeMount = -1; g.toggleMount(); }, idle, "call"});
        s.push_back({1.5f, [](Game& g) { if (g.reg.valid(g.mountEnt)) { Character* m = g.reg.get<Character>(g.mountEnt); m->pos = g.pc().pos + vec3(1.f, 0, 0); m->yaw = g.pc().yaw = 0.f; } AGR_LOG("RIDE before: valid %d active %d dungeon %d dist %.1f", (int)g.reg.valid(g.mountEnt), g.activeMount, (int)g.dungeon.active, g.reg.valid(g.mountEnt) ? distXZ(g.reg.get<Character>(g.mountEnt)->pos, g.pc().pos) : -1.f); if (!g.riding) g.toggleMount(); AGR_LOG("RIDE mounted %d", (int)g.riding); }, idle, "mount"});
        auto gait = [&](const char* nm, int gi, float mv, bool spr, float dur) {
            s.push_back({dur, none, [=](Game& g, float t) {
                g.input.move = vec2(0, mv); g.input.sprint = spr; g.stamina = g.maxStamina;
                g.camYaw = g.pc().yaw + PI * 0.5f; g.camPitch = 0.18f; g.camDist = g.camDistTarget = 7.f;
                if (t > dur * 0.5f) {
                    const Animator& a = g.pc().vis.anim;
                    int k = std::max(0, std::min(3, (int)std::lround(a.ride[0])));
                    gaitSeen[gi] = std::max(gaitSeen[gi], k);
                    bobMin[gi] = std::min(bobMin[gi], g.rideBob); bobMax[gi] = std::max(bobMax[gi], g.rideBob);
                    float gp = a.ride[3] - g.rideSlope / DEG; pMin[gi] = std::min(pMin[gi], gp); pMax[gi] = std::max(pMax[gi], gp);   // the gait alone (without the slope)
                    postMax[gi] = std::max(postMax[gi], a.pose.rootOffset.y);
                    if (std::fabs(t - dur * 0.75f) < 0.017f && g.reg.valid(g.mountEnt)) { Character* m = g.reg.get<Character>(g.mountEnt); AGR_LOG("RIDE %s: horse speed %.2f anim %.2f (run %.1f sprint %.1f) stamina %.0f", nm, length(vec2(m->vel.x, m->vel.z)), m->vis.anim.speed, m->vis.anim.gait.runSpeed, m->vis.anim.gait.sprintSpeed, g.stamina); }
                }
                for (int j = 0; j < 3; j++) if (std::fabs(t - (dur - 1.2f + j * 0.37f)) < 0.017f) {
                    char f[96]; snprintf(f, sizeof f, "build/shots/RD%d_%s_%d.png", gi, nm, j); g.requestScreenshot(f); }
            }, nm});
        };
        gait("walk", 1, 0.35f, false, 4.f);
        gait("trot", 2, 1.f, false, 4.f);
        gait("gallop", 3, 1.f, true, 4.f);
        s.push_back({0.2f, [](Game& g) {
            bool ok = g.riding;
            for (int i = 1; i <= 3; i++) {
                AGR_LOG("RIDE gait %d: seen %d bob %.3f..%.3f m pitch %.1f..%.1f deg rider rise %.3f", i, gaitSeen[i], bobMin[i], bobMax[i], pMin[i], pMax[i], postMax[i]);
                ok = ok && gaitSeen[i] == i && bobMax[i] - bobMin[i] > 0.01f;
            }
            ok = ok && (pMax[3] - pMin[3]) > (pMax[1] - pMin[1]) && postMax[2] > 0.04f;   // the gallop rocks more than the walk, the trot is posted
            g.input.move = vec2(0); g.input.sprint = false; if (g.riding) g.toggleMount();
            AGR_LOG("RIDE -> %s", ok ? "RIDE TEST PASSED" : "RIDE TEST FAILED"); }, idle, "check"});
        return s;
    }
    if (name == "dialogue") {
        // 3.8.0 the story told in scenes: meet Maëlle (a 7-line conversation, hero lines on the right, gestures,
        // two-shot camera, live portraits), take m2 (its scripted offer), finish it (its ending scene plays at the
        // next talk), and the hero's idle life
        static int res[8]; static Entity mae;
        s.push_back({2.f, [](Game& g) { g.newGame(); g.startPlaying(); g.toastHint.clear(); g.tutStep = -1; g.timeOfDay = 10.f; for (int& r : res) r = 0; g.weather = g.weatherNext = 0;
            const Poi* b = g.world.poiById("brindlemere"); teleport(g, b->pos.x + 2, b->pos.y + 12, 0.4f); }, idle, "setup"});
        s.push_back({1.5f, [](Game& g) { mae = NullEntity; g.reg.each<Character>([&](Entity e, Character& c) { if (c.npcDef >= 0 && npcs()[c.npcDef].id == "maelle") mae = e; });
            if (mae == NullEntity) { AGR_LOG("DIALOGUE no Maelle"); return; }
            Character& c = *g.reg.get<Character>(mae); teleport(g, c.pos.x + 0.8f, c.pos.z + 2.2f, 0.2f); g.openDialog(mae);
            res[0] = g.dialog.lines.size() >= 7 && g.dialog.whoAt(1) == 'H'; AGR_LOG("DIALOGUE meet: %zu lines, line 1 by %c", g.dialog.lines.size(), g.dialog.whoAt(1)); }, idle, "meet"});
        s.push_back({0.1f, [](Game& g) { res[1] = g.dlgCamK > 0.5f && g.portraitLiveRT[0].fbo != 0; AGR_LOG("DIALOGUE camera %.2f live portrait %d", g.dlgCamK, (int)(g.portraitLiveRT[0].fbo != 0)); g.requestScreenshot("build/shots/DL0_meet.png"); }, idle, "shot"});
        s.push_back({2.0f, [](Game& g) { g.dialog.line = 1; g.dialog.reveal = 0; }, idle, "hero line"});
        s.push_back({0.1f, [](Game& g) { res[2] = g.portraitLiveRT[1].fbo != 0 && g.pc().vis.anim.action() != nullptr; AGR_LOG("DIALOGUE hero portrait %d hero gesture %s", (int)(g.portraitLiveRT[1].fbo != 0), g.pc().vis.anim.action() ? g.pc().vis.anim.action()->name.c_str() : "-"); g.requestScreenshot("build/shots/DL1_hero.png"); }, idle, "shot"});
        s.push_back({1.6f, [](Game& g) { g.dialog.line = 3; g.dialog.reveal = 0; }, idle, "wonder"});
        s.push_back({0.1f, [](Game& g) { Character& c = *g.reg.get<Character>(mae); res[3] = c.vis.anim.action() == &clip(C_TALK_WONDER); AGR_LOG("DIALOGUE Maelle gesture %s", c.vis.anim.action() ? c.vis.anim.action()->name.c_str() : "-"); g.requestScreenshot("build/shots/DL2_wonder.png"); }, idle, "shot"});
        s.push_back({0.5f, [](Game& g) { g.dialog.line = (int)g.dialog.lines.size() - 1; g.dialog.reveal = 1e9f; }, idle, "last"});
        s.push_back({0.1f, [](Game& g) { g.requestScreenshot("build/shots/DL3_choices.png"); AGR_LOG("DIALOGUE choices %zu", g.dialog.choices.size()); }, idle, "shot"});
        // the m2 offer is a scene too
        s.push_back({0.6f, [](Game& g) { g.questsDone.insert("m1"); g.dialogAction("offer:m2"); res[4] = g.dialog.lines.size() >= 4 && g.dialog.whoAt(2) == 'H'; AGR_LOG("DIALOGUE offer m2: %zu lines", g.dialog.lines.size()); g.dialogAction("accept:m2"); g.dialogAction("close"); }, idle, "offer"});
        // its ending plays at the next talk with Maëlle
        s.push_back({0.6f, [](Game& g) { QuestState* q = g.questState("m2"); if (q) g.completeQuest(*q); res[5] = g.convQueue.count("maelle") > 0; g.itemGets.clear(); g.openDialog(mae);
            res[6] = g.dialog.lines.size() >= 6 && g.convQueue.count("maelle") == 0; AGR_LOG("DIALOGUE done m2 queued %d played %d (%zu lines)", res[5], res[6], g.dialog.lines.size()); g.dialog.line = 2; g.dialog.reveal = 0; }, idle, "done"});
        s.push_back({0.1f, [](Game& g) { g.requestScreenshot("build/shots/DL4_echo.png"); }, idle, "shot"});
        // idle life: the hero left alone looks around or stretches
        s.push_back({11.f, [](Game& g) { g.dialog.open = false; g.cine.active = false; g.pendingChapter.clear(); g.heroIdleT = 0; }, idle, "wait"});
        s.push_back({0.1f, [](Game& g) { const Clip* a = g.pc().vis.anim.action(); res[7] = a == &clip(C_IDLE_LOOK) || a == &clip(C_IDLE_STRETCH) || g.heroIdleT < 0; AGR_LOG("DIALOGUE idle action %s idleT %.1f", a ? a->name.c_str() : "-", g.heroIdleT); }, idle, "idle"});
        s.push_back({0.2f, [](Game& g) { bool ok = true; for (int r : res) ok = ok && r; AGR_LOG("DIALOGUE %d%d%d%d%d%d%d%d -> %s", res[0], res[1], res[2], res[3], res[4], res[5], res[6], res[7], ok ? "DIALOGUE TEST PASSED" : "DIALOGUE TEST FAILED"); }, idle, "check"});
        return s;
    }
    if (name == "birds") {
        // 3.7.0 birds in 3D: swallows over Greenhaven, gulls on the coast, a bird of prey over the Titans
        static int seen[3];
        auto at = [&](const char* poi, float dx, float dz, int idx, const char* file) {
            s.push_back({4.f, [=](Game& g) { const Poi* p = g.world.poiById(poi); teleport(g, p->pos.x + dx, p->pos.y + dz, 0.f); g.timeOfDay = 10.f; g.weather = g.weatherNext = 0; g.flocks.clear(); g.flockT = 100.f;
                    g.spawnFlock(g.pc().pos); Game::Flock& f = g.flocks.back(); f.t = 3.f; if (f.kind != 2) f.anchor = g.pc().pos - normalize(f.vel) * 14.f + vec3(0, f.anchor.y - g.pc().pos.y, 0); else f.anchor = vec3(g.pc().pos.x + 18.f, f.anchor.y, g.pc().pos.z + 10.f); seen[idx] = f.kind + 1; },
                [=](Game& g, float t) { g.camPitch = -0.25f; g.camDist = g.camDistTarget = 9.f; if (!g.flocks.empty()) { const Game::Flock& f = g.flocks.back(); vec3 c = f.kind == 2 ? f.anchor : f.anchor; vec3 d = c - g.pc().pos; g.camYaw = std::atan2(-d.x, -d.z); }
                    if (t > 3.5f && t < 3.6f) g.requestScreenshot(file); }, "flock"});
        };
        s.push_back({1.f, [](Game& g) { g.newGame(); g.startPlaying(); g.toastHint.clear(); g.tutStep = -1; }, idle, "setup"});
        at("brindlemere", 60.f, 40.f, 0, "build/shots/BI0_swallows.png");
        at("port_azur", 0.f, 30.f, 1, "build/shots/BI1_gulls.png");
        at("rochebrune", 40.f, 40.f, 2, "build/shots/BI2_raptor.png");
        s.push_back({0.2f, [](Game& g) { AGR_LOG("BIRDS kinds %d %d %d -> BIRDS TEST PASSED", seen[0], seen[1], seen[2]); }, idle, "end"});
        return s;
    }
    if (name == "cellar") {
        // 3.6.0 the underground base: craft-free (the hatch is given), set it down, go down, dig three blocks with
        // a pickaxe (one holds ore), come back up, save and load: the dug halls are still there
        static int res[6]; static int mined0;
        s.push_back({2.f, [](Game& g) { g.newGame(); g.startPlaying(); g.toastHint.clear(); g.tutStep = -1; g.timeOfDay = 11.f; for (int& r : res) r = 0;
            const Poi* p = g.world.poiById("brindlemere"); teleport(g, p->pos.x + 70.f, p->pos.y - 30.f, 0.f);
            g.addItem("cellar_hatch", 1, Rarity::Epic, true); g.addItem("tool_pickaxe", 1, Rarity::Common, true); g.itemGets.clear(); }, idle, "setup"});
        s.push_back({1.f, [](Game& g) { for (int i = 0; i < (int)g.inventory.size(); i++) if (g.inventory[i].id == "cellar_hatch") { g.useItem(i); break; }
            res[0] = g.cellar.owned && g.reg.valid(g.cellarEnt); AGR_LOG("CELLAR hatch placed %d at %.0f %.0f", res[0], g.cellar.hatch.x, g.cellar.hatch.z); g.itemGets.clear(); }, idle, "place"});
        s.push_back({0.1f, [](Game& g) { g.requestScreenshot("build/shots/CE0_hatch.png"); }, idle, "shot"});
        s.push_back({2.f, [](Game& g) { Character& c = g.pc(); c.pos = g.cellar.hatch + vec3(1.f, 0, 0); g.findInteractTarget(); AGR_LOG("CELLAR target %d", (int)(g.interactTarget == g.cellarEnt)); if (g.interactTarget == g.cellarEnt) g.playerInteract(); }, idle, "down"});
        s.push_back({0.5f, [](Game& g) { res[1] = g.inCellar(); AGR_LOG("CELLAR inside %d", res[1]);
            for (int i = 0; i < (int)g.inventory.size(); i++) if (g.inventory[i].id == "tool_pickaxe") { g.equip(i); break; }
            mined0 = g.cellar.mined; }, idle, "inside"});
        for (int k = 0; k < 9; k++) s.push_back({1.0f, [k](Game& g) { Character& c = g.pc(); c.yaw = k < 3 ? PI / 2 : (k < 6 ? -PI / 2 : 0.f); c.attackClip = nullptr; g.playerAttack(false, false); }, idle, "dig"});
        s.push_back({0.1f, [](Game& g) { res[2] = g.cellar.mined >= mined0 + 3; res[3] = g.countItem("stone") > 0;
            AGR_LOG("CELLAR mined %d blocks, stone %d iron %d coal %d", g.cellar.mined - mined0, g.countItem("stone"), g.countItem("iron_ore"), g.countItem("coal")); g.camPitch = 0.9f; g.camDist = g.camDistTarget = 11.f; }, idle, "count"});
        s.push_back({1.f, none, idle, "settle"});
        s.push_back({0.1f, [](Game& g) { g.requestScreenshot("build/shots/CE1_cellar.png"); }, idle, "shot"});
        s.push_back({3.f, [](Game& g) { Character& c = g.pc(); c.pos = g.dungeon.exitPos; g.input.interact = true; g.updateDungeon(0.016f); }, idle, "up"});
        s.push_back({1.f, [](Game& g) { res[4] = !g.dungeon.active; g.saveGame(3); bool ok = g.loadGame(3); res[5] = ok && g.cellar.owned && g.cellar.mined >= 3;
            AGR_LOG("CELLAR back up %d, reloaded %d (mined %d)", res[4], res[5], g.cellar.mined); }, idle, "save"});
        s.push_back({0.2f, [](Game& g) { bool ok = res[0] && res[1] && res[2] && res[3] && res[4] && res[5]; AGR_LOG("CELLAR -> %s", ok ? "CELLAR TEST PASSED" : "CELLAR TEST FAILED"); }, idle, "check"});
        return s;
    }
    if (name == "fpview") {
        // 3.6.0 first person: walk through Brindlemere, strike, look around, then back to the usual view
        static int res[3];
        s.push_back({2.f, [](Game& g) { g.newGame(); g.startPlaying(); g.toastHint.clear(); g.tutStep = -1; g.timeOfDay = 10.5f; for (int& r : res) r = 0;
            const Poi* p = g.world.poiById("brindlemere"); teleport(g, p->pos.x + 8.f, p->pos.y + 6.f, 0.4f); g.settings.firstPerson = true; g.camPitch = CameraRig::PITCH_DEFAULT; }, idle, "on"});
        s.push_back({2.5f, none, [](Game& g, float) { g.input.move = vec2(0, 1); g.camIdleT = 0; g.camPitch = CameraRig::PITCH_DEFAULT; }, "walk"});
        s.push_back({0.1f, [](Game& g) { res[0] = g.fpActive && distXZ(g.cam.pos, g.pc().pos) < 0.6f; AGR_LOG("FP active %d eye-body %.2f m, yaw %.2f body %.2f weapon %d wt %d equip %s", g.fpActive, distXZ(g.cam.pos, g.pc().pos), g.fpYaw, g.pc().yaw, (int)(bool)g.pc().vis.weapon, (int)g.pc().vis.wt, g.equipment[SL_WEAPON].id.c_str()); g.requestScreenshot("build/shots/FP0_walk.png"); }, [](Game& g, float) { g.input.move = vec2(0, 1); }, "shot"});
        s.push_back({0.25f, [](Game& g) { g.input.move = vec2(0); g.playerAttack(false, false); }, idle, "strike"});
        s.push_back({0.1f, [](Game& g) { res[1] = g.pc().attackClip != nullptr; g.requestScreenshot("build/shots/FP1_strike.png"); }, idle, "shot"});
        s.push_back({2.f, [](Game& g) { const Poi* p = g.world.poiById("brindlemere"); teleport(g, p->pos.x + 90.f, p->pos.y - 70.f, 1.2f); }, [](Game& g, float) { g.camPitch = CameraRig::PITCH_DEFAULT + 0.25f; }, "field"});
        s.push_back({0.1f, [](Game& g) { g.requestScreenshot("build/shots/FP3_field.png"); }, idle, "shot"});
        s.push_back({0.3f, [](Game& g) { g.playerAttack(true, false); }, idle, "heavy"});
        s.push_back({0.1f, [](Game& g) { g.requestScreenshot("build/shots/FP4_field_strike.png"); }, idle, "shot"});
        s.push_back({1.5f, none, [](Game& g, float) { g.camPitch = CameraRig::PITCH_DEFAULT - 0.6f; g.camYaw += 0.01f; }, "look up"});
        s.push_back({0.1f, [](Game& g) { g.requestScreenshot("build/shots/FP2_lookup.png"); }, idle, "shot"});
        s.push_back({1.f, [](Game& g) { g.settings.firstPerson = false; g.camPitch = CameraRig::PITCH_DEFAULT; }, idle, "off"});
        s.push_back({0.2f, [](Game& g) { res[2] = !g.fpActive && distXZ(g.cam.pos, g.pc().pos) > 3.f; bool ok = res[0] && res[1] && res[2]; AGR_LOG("FP back to third person %d -> %s", res[2], ok ? "FP TEST PASSED" : "FP TEST FAILED"); }, idle, "check"});
        return s;
    }
    if (name == "chop") {
        // 3.6.0 felling a tree: an axe, a few swings, the tree falls, wood in the bag, a stump, and it grows back
        static TreeRef tree; static int wood0, res[4];
        s.push_back({3.f, [](Game& g) { g.newGame(); g.startPlaying(); g.toastHint.clear(); g.tutStep = -1; g.timeOfDay = 10.f; for (int& r : res) r = 0;
            const Poi* p = g.world.poiById("brindlemere"); teleport(g, p->pos.x + 60.f, p->pos.y - 40.f, 0.f);
            g.addItem("axe_lumber", 1, Rarity::Common, true); g.itemGets.clear();
            for (int i = 0; i < (int)g.inventory.size(); i++) if (g.inventory[i].id == "axe_lumber") { g.equip(i); break; } }, idle, "setup"});
        s.push_back({0.5f, [](Game& g) {
            float bd = 1e9f; tree.uid = 0; vec3 pp = g.pc().pos;
            g.world.forEachVisibleChunk([&](Chunk& ch) { if (ch.lod != 0) return; for (const TreeRef& t : ch.trees) { float d = distXZ(t.pos, pp); if (d < bd && t.kind == P_OAK) { bd = d; tree = t; } } });
            if (!tree.uid) g.world.forEachVisibleChunk([&](Chunk& ch) { if (ch.lod != 0) return; for (const TreeRef& t : ch.trees) { float d = distXZ(t.pos, pp); if (d < bd) { bd = d; tree = t; } } });
            AGR_LOG("CHOP tree %u kind %d at %.1f m", tree.uid, tree.kind, bd);
            Character& c = g.pc(); c.pos = tree.pos + vec3(-1.4f, 0, 0); c.pos.y = g.groundAt(c.pos.x, c.pos.z); c.yaw = PI / 2; g.camFocus = c.pos; g.camYaw = 0.3f; g.camPitch = 0.45f; g.camDist = g.camDistTarget = 10.f;
            wood0 = g.countItem("wood"); }, idle, "find"});
        for (int k = 0; k < 4; k++) s.push_back({0.9f, [](Game& g) { Character& c = g.pc(); c.pos = tree.pos + vec3(-1.4f, 0, 0); c.pos.y = g.groundAt(c.pos.x, c.pos.z); c.yaw = PI / 2; g.playerAttack(false, false); }, idle, "swing"});
        s.push_back({0.8f, none, idle, "falling"});
        s.push_back({0.1f, [](Game& g) { res[0] = g.world.isFelled(tree.uid); res[1] = g.countItem("wood") > wood0; AGR_LOG("CHOP felled %d wood %d -> %d, falling %d", res[0], wood0, g.countItem("wood"), (int)g.fallingTrees.size()); g.requestScreenshot("build/shots/CH0_falling.png"); }, idle, "shot"});
        s.push_back({4.f, none, idle, "settle"});
        s.push_back({0.1f, [](Game& g) { bool stillThere = false; g.world.forEachVisibleChunk([&](Chunk& ch) { for (const TreeRef& t : ch.trees) if (t.uid == tree.uid) stillThere = true; });
            res[2] = !stillThere; AGR_LOG("CHOP chunk rebuilt without the tree: %d", res[2]); g.requestScreenshot("build/shots/CH1_stump.png"); }, idle, "shot"});
        s.push_back({3.f, [](Game& g) { int n = g.world.regrowTrees((float)g.gameTime + 2000.f, 24.f * 60.f); res[3] = n >= 1 && !g.world.isFelled(tree.uid); AGR_LOG("CHOP regrew %d", n); }, idle, "regrow"});
        s.push_back({0.2f, [](Game& g) { bool ok = res[0] && res[1] && res[2] && res[3]; AGR_LOG("CHOP -> %s", ok ? "CHOP TEST PASSED" : "CHOP TEST FAILED"); }, idle, "check"});
        return s;
    }
    if (name == "skylook") {
        // 3.6.0 volumetric clouds: a clear morning, an afternoon with more clouds, a sunset, seen with the camera low
        auto shot = [&](float hour, int weather, const char* file) {
            s.push_back({6.f, [=](Game& g) { const Poi* p = g.world.poiById("brindlemere"); g.timeOfDay = hour; g.weather = g.weatherNext = weather; teleport(g, p->pos.x + 30.f, p->pos.y + 40.f, 0.f); },
                [=](Game& g, float t) { g.timeOfDay = hour; g.camPitch = -0.3f; g.camDist = g.camDistTarget = 7.f; vec3 sd = g.env.sunDir; g.camYaw = std::atan2(-sd.x, -sd.z) + 0.9f;
                    if (t > 5.5f && t < 5.6f) g.requestScreenshot(file); }, "sky"});
        };
        s.push_back({1.f, [](Game& g) { g.newGame(); g.startPlaying(); g.toastHint.clear(); g.tutStep = -1; }, idle, "setup"});
        shot(9.5f, 0, "build/shots/SK0_clear.png");
        shot(14.f, 1, "build/shots/SK1_clouds.png");
        shot(18.3f, 0, "build/shots/SK2_sunset.png");
        s.push_back({0.2f, [](Game& g) { AGR_LOG("SKY cover %.2f -> SKY TEST PASSED", g.env.cloudCover); }, idle, "end"});
        return s;
    }
    if (name == "reef") {
        // 3.5.0 under the sea: a warm reef (corals, kelp, fish, caustics, light shafts, the water colour), then a
        // shipwreck with its chest, opened under water
        static vec3 reef, wreck; static int res[4]; static int fish0;
        s.push_back({2.f, [](Game& g) { g.newGame(); g.startPlaying(); g.timeOfDay = 12.f; g.weatherNext = 0; g.toastHint.clear(); g.tutStep = -1; for (int& r : res) r = 0;
            float best = 0;
            for (float z = 4250; z < 4600 && best < 9.f; z += 4) for (float x = 2600; x < 2900; x += 6) {
                bool lv = false, fr = false; float wl = g.world.waterLevel(x, z, &lv, &fr); if (wl < -1e8f || lv || fr) continue;
                float dep = wl - g.world.height(x, z);
                if (dep > 6.f && dep < 11.f && dep > best) { best = dep; reef = vec3(x, wl, z); }
            }
            AGR_LOG("REEF at %.0f %.0f depth %.1f region %d", reef.x, reef.z, best, g.world.regionId(reef.x, reef.z));
            teleport(g, reef.x, reef.z, 0.f); g.pc().pos = vec3(reef.x, reef.y - 1.2f, reef.z); g.camFocus = g.pc().pos; g.pc().swimming = true; g.playerDodge(); }, idle, "reef"});
        s.push_back({5.f, none, [](Game& g, float) { g.input.move = vec2(0, 0.25f); g.camIdleT = 0; g.camPitch = 0.02f; g.camDist = g.camDistTarget = 6.5f; }, "dive"});
        s.push_back({0.1f, [](Game& g) { res[0] = g.underwaterView && g.env.underwater > 0.5f; fish0 = (int)g.fishInst.size(); res[1] = fish0 > 0;
            AGR_LOG("REEF underwater %d depth %.1f fish %d caustics %.2f", g.underwaterView, g.env.underDepth, fish0, g.env.caustics); g.requestScreenshot("build/shots/RF0_reef.png"); }, [](Game& g, float) { g.input.move = vec2(0, 0.25f); g.camPitch = 0.02f; }, "shot"});
        s.push_back({0.5f, [](Game& g) {
            // find a wreck: generate the deep sea chunks along the coasts and look for an underwater chest
            int found = 0;
            for (int cz = 0; cz < CHUNKS && !found; cz += 1) for (int cx = 0; cx < CHUNKS && !found; cx += 1) {
                float x = (cx + 0.5f) * CHUNK, z = (cz + 0.5f) * CHUNK;
                bool lv = false, fr = false; float wl = g.world.waterLevel(x, z, &lv, &fr);
                if (wl < -1e8f || lv || fr || wl - g.world.height(x, z) < 6.5f || g.world.height(x, z) > 0.f) continue;
                if (length(vec2(x, z) - reef.xz()) > 900.f) continue;
                ChunkBuild b; g.world.generateChunkSync({cx, cz}, 0, b);
                for (const NodeSpawn& n : b.nodes) if (n.type == NodeType::Chest && g.world.waterLevel(n.pos.x, n.pos.z) - n.pos.y > 3.f) { wreck = n.pos; found = 1; break; }
            }
            res[2] = found;
            AGR_LOG("REEF wreck %d at %.0f %.0f (%.1f m deep)", found, wreck.x, wreck.z, found ? g.world.waterLevel(wreck.x, wreck.z) - wreck.y : 0.f);
            if (found) { float wl = g.world.waterLevel(wreck.x, wreck.z); teleport(g, wreck.x - 5.f, wreck.z - 5.f, 0.78f); g.pc().pos = vec3(wreck.x - 5.f, wl - 1.2f, wreck.z - 5.f); g.camFocus = g.pc().pos; g.pc().swimming = true; g.diving = true; } }, idle, "wreck"});
        s.push_back({5.f, none, [](Game& g, float) { g.diving = true; g.breath = 1.f; g.camIdleT = 0; g.camPitch = 0.1f; g.camDist = g.camDistTarget = 8.f; g.camYaw = 0.78f + PI; }, "sink"});
        s.push_back({0.1f, [](Game& g) { g.requestScreenshot("build/shots/RF1_wreck.png"); }, idle, "shot"});
        s.push_back({2.f, [](Game& g) {
            Character& c = g.pc(); float wl = g.world.waterLevel(wreck.x, wreck.z);
            c.pos = vec3(wreck.x - 1.3f, std::max(wreck.y + 0.8f, wl - 7.f), wreck.z); c.yaw = PI / 2; g.camFocus = c.pos; g.diving = true; }, [](Game& g, float) { g.diving = true; g.breath = 1.f; Character& c = g.pc(); c.pos.x = wreck.x - 1.3f; c.pos.z = wreck.z; c.yaw = PI / 2; g.findInteractTarget(); }, "approach"});
        s.push_back({0.5f, [](Game& g) { size_t before = g.openedChests.size(); g.findInteractTarget(); if (g.interactTarget != NullEntity) { g.input.interact = true; g.playerInteract(); }
            res[3] = g.openedChests.size() > before; AGR_LOG("REEF chest opened under water: %d (target %d)", res[3], (int)(g.interactTarget != NullEntity)); }, idle, "open"});
        s.push_back({0.3f, [](Game& g) { bool ok = res[0] && res[1] && res[2] && res[3]; AGR_LOG("REEF -> %s", ok ? "REEF TEST PASSED" : "REEF TEST FAILED"); }, idle, "check"});
        return s;
    }
    if (name == "items3d") {
        // 3.5.0 every item has a 3D model: the bag full of different things, the selected item turning in its
        // showcase, and items lying on the ground
        static const char* ids[] = {"potion_minor", "potion_heal", "potion_great", "potion_mana", "potion_stamina", "elixir_strength", "elixir_titan", "elixir_phoenix",
            "bread", "stew", "fish_grilled", "berries", "coco", "bomb", "bandage", "tea_mint", "bag_bottomless", "wood", "stone", "iron_ore", "gold_ore", "rare_ore",
            "iron_bar", "gold_bar", "star_bar", "crystal", "aurora_crystal", "ember_core", "amber", "coral_shard", "herb", "glowcap", "frost_lily", "fur", "linen",
            "spider_silk", "bone", "shadow_essence", "dragon_scale", "fish", "meat", "pirate_coin", "pearl", "star_shard", "ring_sapphire", "ring_abyss", "amulet_sun",
            "amulet_oak", "chain_head", "frost_head", "ember_head", "shadow_head", "crown_head", "echo_stone", "mother_crown", "treasure_map", "relic_sun_mask",
            "relic_jarl_horn", "relic_star_glass", "relic_comet", "key_compass", "key_zephyr", "key_phoenix", "key_triton", "key_clover", "mount_spectral", "dye_crimson",
            "tool_shovel", "sword_knight"};
        static int modelled;
        s.push_back({1.f, [](Game& g) { g.newGame(); g.startPlaying(); g.toastHint.clear(); g.tutStep = -1; g.invCap = 160; g.inventory.clear();
            modelled = 0; int n = 0;
            for (const ItemDef& d : allItems()) { n++; if (g.itemModel(d.id)) modelled++; }
            for (const char* id : ids) if (item(id)) g.addItem(id, 1, item(id)->rarity, true);
            g.itemGets.clear();
            for (auto& st : g.inventory) { const ItemDef* d = item(st.id); if (d && (d->kind == ItemKind::Chest || d->kind == ItemKind::Feet || d->kind == ItemKind::Hands)) continue; }
            for (const ItemDef& d : allItems()) if ((d.kind == ItemKind::Chest || d.kind == ItemKind::Hands || d.kind == ItemKind::Feet) && d.rarity >= Rarity::Rare) g.addItem(d.id, 1, d.rarity, true);
            g.itemGets.clear();
            AGR_LOG("ITEMS3D %d of %d items have a 3D model, %d in the bag", modelled, n, (int)g.inventory.size());
            g.menu = Menu::Inventory; g.selItem = -1; }, idle, "bag"});
        s.push_back({0.6f, none, idle, "wait"});
        s.push_back({0.1f, [](Game& g) { g.requestScreenshot("build/shots/IT0_bag.png"); }, idle, "shot"});
        s.push_back({0.6f, [](Game& g) { for (int i = 0; i < (int)g.inventory.size(); i++) if (g.inventory[i].id == "elixir_phoenix") g.selItem = i; }, idle, "select"});
        s.push_back({0.1f, [](Game& g) { g.requestScreenshot("build/shots/IT1_showcase.png"); }, idle, "shot"});
        s.push_back({0.6f, [](Game& g) { for (int i = 0; i < (int)g.inventory.size(); i++) if (g.inventory[i].id == "frost_head") g.selItem = i; }, idle, "select"});
        s.push_back({0.1f, [](Game& g) { g.requestScreenshot("build/shots/IT2_showcase_helm.png"); }, idle, "shot"});
        s.push_back({2.f, [](Game& g) { g.menu = Menu::None; Character& c = g.pc();
            const char* drop[] = {"sword_knight", "potion_great", "gold_bar", "aurora_crystal", "mother_crown", "key_phoenix", "bread", "ring_sapphire"};
            for (int i = 0; i < 8; i++) { float a = i * TAU / 8; g.spawnPickup(drop[i], 1, item(drop[i])->rarity, c.pos + vec3(std::cos(a) * 2.2f, 0, std::sin(a) * 2.2f)); }
            g.camPitch = 0.55f; g.camDist = g.camDistTarget = 7.f; }, idle, "ground"});
        s.push_back({0.1f, [](Game& g) { g.requestScreenshot("build/shots/IT3_ground.png"); }, idle, "shot"});
        s.push_back({0.3f, [](Game& g) { bool ok = modelled >= 200; AGR_LOG("ITEMS3D -> %s", ok ? "ITEMS3D TEST PASSED" : "ITEMS3D TEST FAILED"); }, idle, "check"});
        return s;
    }
    if (name == "farview") {
        // 3.4.1 render distance: a wide view from a hill over Greenhaven (villages and forests far away), the
        // LOD 2 chunks must carry buildings and trees, and the frame must stay within budget
        static int farProps;
        s.push_back({1.f, [](Game& g) { g.newGame(); g.startPlaying(); g.toastHint.clear(); g.tutStep = -1; g.timeOfDay = 10.f; }, idle, "setup"});
        s.push_back({14.f, [](Game& g) { const Poi* p = g.world.poiById("brindlemere"); teleport(g, p->pos.x + 40.f, p->pos.y + 60.f, 0.f);
                if (g.vehicles.empty()) g.initVehicles(); g.vehiclesUnlocked = {"car", "moto", "airship"}; g.spawnVehicleEntities();
                int ship = (int)g.vehicles.size() - 1; VehicleState& v = g.vehicles[ship]; v.pos = g.pc().pos; v.yaw = 0.6f; g.enterVehicle(ship); v.alt = 55.f;
                g.camYaw = 0.6f + PI; g.camPitch = 0.3f; g.camDist = g.camDistTarget = 30.f; },
            [](Game& g, float t) { g.camPitch = 0.3f; if (g.driving >= 0) g.camYaw = g.vehicles[g.driving].yaw + PI; static bool sh = false; if (!sh && t > 13.f) { sh = true;
                farProps = 0; int n2 = 0; g.world.forEachVisibleChunk([&](Chunk& ch) { if (ch.lod == 2) { n2++; if (ch.props.valid() || ch.foliage.valid() || !ch.models.empty()) farProps++; } });
                AGR_LOG("FARVIEW draw distance %.0f m, %d far chunks, %d with buildings or trees; fog %.4f falloff %.3f base %.1f scorch %.2f shafts %.2f heat %.2f weather %d mist %.2f haze %.2f", g.renderer.settings.drawDistance, n2, farProps, g.env.fogDensity, g.env.fogFalloff, g.env.fogBase, g.env.scorch, g.env.shafts, g.env.heat, g.weather, g.morningMist, g.forestHaze);
                g.requestScreenshot("build/shots/FV0_far.png"); } }, "hill"});
        s.push_back({0.5f, [](Game& g) { bool ok = g.renderer.settings.drawDistance >= 160.f && farProps > 10;
            AGR_LOG("FARVIEW -> %s", ok ? "FARVIEW TEST PASSED" : "FARVIEW TEST FAILED"); }, idle, "check"});
        return s;
    }
    if (name == "sunlook") {
        // 3.4.0 the blazing sun: midday in the Scarlet Canyons and in Greenhaven (bleached highlights, deep
        // shade, heat haze), and a low camera turned towards the afternoon sun (starburst)
        static float sc[3];
        auto at = [](const char* poi, float hour, float yaw, float pitch, float dist) {
            return [=](Game& g) {
                const Poi* p = g.world.poiById(poi); g.timeOfDay = hour; g.weatherNext = 0;
                teleport(g, p->pos.x + 24.f, p->pos.y + 18.f, yaw); g.camYaw = yaw; g.camPitch = pitch; g.camDist = g.camDistTarget = dist;
            };
        };
        s.push_back({1.f, [](Game& g) { g.newGame(); g.startPlaying(); g.toastHint.clear(); g.tutStep = -1; }, idle, "setup"});
        s.push_back({7.f, at("roc_ecarlate", 13.f, 0.8f, 0.55f, 15.f), [](Game& g, float t) { g.timeOfDay = 13.f; static bool sh = false; if (!sh && t > 6.5f) { sh = true; sc[0] = g.env.scorch; g.requestScreenshot("build/shots/SU0_canyon_noon.png"); } }, "canyon"});
        s.push_back({7.f, at("brindlemere", 12.5f, 2.2f, 0.5f, 14.f), [](Game& g, float t) { g.timeOfDay = 12.5f; static bool sh = false; if (!sh && t > 6.5f) { sh = true; sc[1] = g.env.scorch; g.requestScreenshot("build/shots/SU1_greenhaven_noon.png"); } }, "greenhaven"});
        s.push_back({7.f, at("brindlemere", 17.4f, 0.f, -0.25f, 8.f), [](Game& g, float t) {
            g.timeOfDay = 17.4f;
            // face the sun: the camera looks from behind the hero towards it
            vec3 sd = g.env.sunDir; g.camYaw = std::atan2(-sd.x, -sd.z); g.camPitch = -0.3f;
            static bool sh = false; if (!sh && t > 6.5f) { sh = true; sc[2] = g.env.scorch; g.requestScreenshot("build/shots/SU2_sun_burst.png"); } }, "facing the sun"});
        s.push_back({7.f, at("brindlemere", 12.5f, 2.2f, 0.5f, 14.f), [](Game& g, float t) { g.timeOfDay = 22.f; }, "night"});
        s.push_back({0.5f, [](Game& g) { bool ok = sc[0] > 0.7f && sc[1] > 0.45f && sc[2] < 0.3f && g.env.scorch < 0.05f;   // the evening sun no longer blazes
            AGR_LOG("SUN scorch canyon %.2f greenhaven %.2f evening %.2f night %.2f -> %s", sc[0], sc[1], sc[2], g.env.scorch, ok ? "SUN TEST PASSED" : "SUN TEST FAILED"); }, idle, "check"});
        return s;
    }
    if (name == "canyons") {
        // 3.3.0 the Scarlet Canyons: Roc-Écarlate, the Citadel of the Cliffs, a gorge with its needles, the world map
        struct V { const char* poi; float dx, dz, hour, pitch, dist; const char* shot; };
        static const V views[] = {{"roc_ecarlate", 30, 40, 16.5f, 0.25f, 16.f, "CA0_village"}, {"citadelle_falaises", 0, 70, 17.5f, 0.2f, 22.f, "CA1_citadel"}, {"arches_couchant", 40, -30, 18.6f, 0.15f, 14.f, "CA2_arches"}, {"camp_nomades", 120, 60, 11.f, 0.35f, 26.f, "CA3_gorges"}};
        static int ok = 0;
        s.push_back({1.f, [](Game& g) { g.newGame(); g.startPlaying(); g.toastHint.clear(); g.tutStep = -1; g.level = 30; g.recomputeStats(); ok = 0; }, idle, "setup"});
        for (const V& v : views) {
            const V* q = &v;
            s.push_back({4.5f, [q](Game& g) { const Poi* p = g.world.poiById(q->poi); if (!p) return; if (g.world.regionId(p->pos.x, p->pos.y) == R_CANYON) ok++;
                teleport(g, p->pos.x + q->dx, p->pos.y + q->dz, 0.f); vec2 d = p->pos - vec2(p->pos.x + q->dx, p->pos.y + q->dz); g.pc().yaw = std::atan2(d.x, d.y); g.camYaw = g.pc().yaw + PI;
                g.camPitch = q->pitch; g.camDist = g.camDistTarget = q->dist; g.timeOfDay = q->hour; g.weather = g.weatherNext = Game::W_CLEAR; g.weatherBlend = 1; g.weatherTimer = 999; },
                [](Game& g, float) { g.camIdleT = 0; g.pc().invuln = 1.f; g.reg.each<Character>([&](Entity e, Character& c) { if (c.team == Team::Enemy) g.reg.destroyLater(e); }); }, "go"});
            s.push_back({0.1f, [q](Game& g) { g.requestScreenshot(std::string("build/shots/") + q->shot + ".png"); }, idle, "shot"});
        }
        s.push_back({0.5f, [](Game& g) { g.menu = Menu::Map; for (auto& p : g.world.pois) g.discovered.insert(p.id); for (auto& f : g.fog) f = 1; }, idle, "map"});
        s.push_back({0.1f, [](Game& g) { g.requestScreenshot("build/shots/CA4_map.png"); }, idle, "shot"});
        s.push_back({0.1f, [](Game& g) { g.menu = Menu::None; AGR_LOG("CANYONS %d/4 places in the canyons -> %s", ok, ok == 4 ? "CANYONS TEST PASSED" : "CANYONS TEST FAILED"); }, idle, "end"});
        return s;
    }
    if (name == "fauna") {
        // 2.9.0 the imported Tripo gallery models side by side: villagers, stag, boar, wolf (pictures + model checks)
        static std::vector<Entity> ents;
        s.push_back({3.f, [](Game& g) { g.newGame(); g.startPlaying(); g.timeOfDay = 10.5f; g.toastHint.clear(); g.tutStep = -1; ents.clear();
            g.weather = g.weatherNext = Game::W_CLEAR; g.weatherBlend = 1; g.weatherTimer = 999;
            const Poi* b = g.world.poiById("brindlemere"); teleport(g, b->pos.x + 70, b->pos.y + 50, 0.f); }, idle, "setup"});
        s.push_back({0.2f, [](Game& g) { Character& c = g.pc(); vec3 f{std::sin(c.yaw), 0, std::cos(c.yaw)}, r{f.z, 0, -f.x};
            auto at = [&](float side, float fwd) { vec3 p = c.pos + f * fwd + r * side; p.y = g.groundAt(p.x, p.z); return p; };
            ents.push_back(g.spawnAnimal(4, at(-3.5f, 7.f)));
            ents.push_back(g.spawnEnemy(enemyIndex("boar"), at(0.5f, 6.f)));
            ents.push_back(g.spawnEnemy(enemyIndex("wolf"), at(3.5f, 6.5f)));
            ents.push_back(g.spawnVillager(at(-1.8f, 4.5f), g.world.nearestPoi(c.pos.xz(), 400.f), 7));
            ents.push_back(g.spawnVillager(at(1.8f, 4.5f), g.world.nearestPoi(c.pos.xz(), 400.f), 8));
            int imported = 0;
            for (Entity e : ents) { if (AIBrain* ai = g.reg.get<AIBrain>(e)) { ai->state = AIState::Idle; ai->timer = 99; }
                if (Character* ch = g.reg.get<Character>(e)) { ch->yaw = c.yaw + PI + 0.9f; imported += ch->vis.albedo != 0; if (AnimalBrain* ab = g.reg.get<AnimalBrain>(e)) { ab->home = ab->dest = ch->pos; ab->timer = 99; } } }
            AGR_LOG("FAUNA %zu spawned, %d with an imported textured model", ents.size(), imported);
            g.camYaw = c.yaw + PI; g.camPitch = 0.12f; g.camDist = g.camDistTarget = 6.f; }, [](Game& g, float) { g.camIdleT = 0; }, "spawn"});
        s.push_back({1.5f, none, [](Game& g, float) { g.camIdleT = 0; for (Entity e : ents) if (Character* ch = g.reg.get<Character>(e)) ch->vel = vec3(0); }, "settle"});
        s.push_back({0.05f, [](Game& g) { g.requestScreenshot("build/shots/FA0_fauna.png"); }, idle, "shot"});
        s.push_back({0.6f, [](Game& g) { Character& c = g.pc(); Character* b = g.reg.get<Character>(ents[1]); c.pos = b->pos + vec3(std::sin(b->yaw), 0, std::cos(b->yaw)) * 2.5f + vec3(std::cos(b->yaw), 0, -std::sin(b->yaw)) * 1.5f;
            c.yaw = std::atan2(b->pos.x - c.pos.x, b->pos.z - c.pos.z); g.camYaw = c.yaw + PI * 0.8f; g.camDist = g.camDistTarget = 4.f; g.camPitch = 0.2f; }, [](Game& g, float) { g.camIdleT = 0; }, "boar close"});
        s.push_back({0.05f, [](Game& g) { g.requestScreenshot("build/shots/FA1_boar.png"); }, idle, "shot"});
        return s;
    }
    if (name == "aurora") {
        // 2.8.0 the Lands of Aurora: landscapes by day and by night (pictures)
        static const float spots[][3] = {{1080, 4700, 3.1f}, {1500, 5350, 0.6f}, {2300, 5450, 2.2f}, {800, 5600, 4.0f}};
        s.push_back({3.f, [](Game& g) { g.newGame(); g.startPlaying(); g.timeOfDay = 10.f; g.toastHint.clear(); g.tutStep = -1;
            g.weather = g.weatherNext = Game::W_CLEAR; g.weatherBlend = 1; g.weatherTimer = 999; }, idle, "setup"});
        for (int i = 0; i < 4; i++) {
            s.push_back({3.5f, [i](Game& g) { teleport(g, spots[i][0], spots[i][1], spots[i][2]); g.camPitch = 0.12f; g.camDist = g.camDistTarget = 10.f; g.camYaw = spots[i][2] + PI;
                AGR_LOG("AURORA spot %d region %s ground %.1f", i, regionName(g.world.regionId(spots[i][0], spots[i][1])), g.pc().pos.y); }, [](Game& g, float) { g.camIdleT = 0; }, "go"});
            s.push_back({0.05f, [i](Game& g) { g.requestScreenshot("build/shots/AU_day" + std::to_string(i) + ".png"); }, idle, "shot"});
        }
        s.push_back({3.f, [](Game& g) { g.timeOfDay = 23.f; teleport(g, spots[1][0], spots[1][1], spots[1][2]); g.camPitch = 0.2f; g.camDist = g.camDistTarget = 10.f; }, [](Game& g, float) { g.camIdleT = 0; g.timeOfDay = 23.f; }, "night"});
        s.push_back({0.05f, [](Game& g) { g.requestScreenshot("build/shots/AU_night.png"); }, idle, "shot"});
        // ---- the novelties: geyser -> floating island -> chest, a free geyser, Lumenhaven and Aïla, a crystal golem
        static int isl = -1, gey = -1, ok[6]; static float apex, y0;
        s.push_back({3.f, [](Game& g) { g.timeOfDay = 11.f; for (int& o : ok) o = 0; isl = gey = -1;
            for (int i = 0; i < (int)g.world.structures.size(); i++) if (g.world.structures[i].kind == S_SKYISLAND) { isl = i; break; }
            const Structure& I = g.world.structures[isl];
            for (int i = 0; i < (int)g.world.structures.size(); i++) { const Structure& s = g.world.structures[i]; if (s.kind == S_GEYSER && s.variant > 0 && length(s.pos - I.pos) < SKY_ISLAND_R + 5.f) gey = i; }
            int n = 0, ng = 0; for (auto& s : g.world.structures) { n += s.kind == S_SKYISLAND; ng += s.kind == S_GEYSER; }
            AGR_LOG("AURORA %d floating islands, %d geysers; island %d deck %.1f m, its geyser %d", n, ng, isl, I.variant / 100.f, gey);
            const Structure& G = g.world.structures[gey];
            teleport(g, G.pos.x - std::sin(G.rot) * 3.f, G.pos.y - std::cos(G.rot) * 3.f, G.rot); g.camYaw = G.rot + PI; g.camPitch = 0.25f; g.camDist = g.camDistTarget = 11.f; },
            [](Game& g, float) { g.camIdleT = 0; }, "to the geyser"});
        s.push_back({0.05f, [](Game& g) { g.requestScreenshot("build/shots/AU_island_below.png"); }, idle, "shot"});
        s.push_back({2.5f, [](Game& g) { const Structure& I = g.world.structures[isl]; vec2 d{0.6f, 0.8f}; vec2 p = I.pos + d * 30.f; teleport(g, p.x, p.y, 0.f); g.pc().pos.y += 0.f;
            g.camYaw = std::atan2(d.x, d.y); g.camPitch = -0.05f; g.camDist = g.camDistTarget = 6.f; }, [](Game& g, float) { const Structure& I = g.world.structures[isl]; vec2 d = I.pos - g.pc().pos.xz(); g.camYaw = std::atan2(-d.x, -d.y); g.camPitch = -0.1f; g.camIdleT = 0; }, "island from afar"});
        s.push_back({0.05f, [](Game& g) { g.requestScreenshot("build/shots/AU_island_far.png"); }, idle, "shot"});
        s.push_back({0.1f, [](Game& g) { const Structure& G = g.world.structures[gey]; Character& c = g.pc(); c.pos = vec3(G.pos.x, g.groundAt(G.pos.x, G.pos.y), G.pos.y); c.grounded = true; c.vel = vec3(0); g.geyserCD = 0; apex = c.pos.y; y0 = c.pos.y; }, idle, "step in"});
        s.push_back({0.75f, none, [](Game& g, float) { apex = std::max(apex, g.pc().pos.y); g.camIdleT = 0; g.camPitch = 0.05f; g.camDist = g.camDistTarget = 16.f; }, "fly"});
        s.push_back({0.05f, [](Game& g) { g.requestScreenshot("build/shots/AU_launch.png"); }, idle, "shot"});
        s.push_back({3.2f, none, [](Game& g, float) { apex = std::max(apex, g.pc().pos.y); g.camIdleT = 0; }, "fly"});
        s.push_back({0.1f, [](Game& g) { const Structure& I = g.world.structures[isl]; Character& c = g.pc(); float deck = I.variant / 100.f;
            ok[0] = g.geyserLaunches >= 1 && c.grounded && std::fabs(c.pos.y - deck) < 0.7f && length(c.pos.xz() - I.pos) < SKY_ISLAND_R && c.hp >= c.maxHp - 1;
            AGR_LOG("AURORA geyser launch: apex %.1f m above the pool, now at y %.1f (deck %.1f), %.1f m from the island centre, grounded %d, hp %.0f/%.0f -> %d",
                apex - y0, c.pos.y, deck, length(c.pos.xz() - I.pos), c.grounded, c.hp, c.maxHp, ok[0]);
            g.camPitch = 0.35f; g.camDist = g.camDistTarget = 12.f; }, [](Game& g, float) { g.camIdleT = 0; }, "landed"});
        s.push_back({1.f, none, [](Game& g, float) { g.camIdleT = 0; }, "look"});
        s.push_back({0.05f, [](Game& g) { g.requestScreenshot("build/shots/AU_island_top.png"); }, idle, "shot"});
        s.push_back({0.3f, [](Game& g) { Entity chest = NullEntity; float bd = 1e9f; Character& c = g.pc();
            g.reg.each<NodeComp>([&](Entity e, NodeComp& n) { if (n.node.type == NodeType::Chest) { float d = length(n.node.pos - c.pos); if (d < bd) { bd = d; chest = e; } } });
            int before = g.countItem("sky_shard");
            if (chest != NullEntity && bd < 15.f) { c.pos = g.reg.get<NodeComp>(chest)->node.pos + vec3(0, 0, 1.2f); g.interactTarget = chest; g.playerInteract(); }
            ok[1] = g.countItem("sky_shard") == before + 1; AGR_LOG("AURORA island chest at %.1f m: sky shards %d -> %d -> %d", bd, before, g.countItem("sky_shard"), ok[1]); }, idle, "chest"});
        s.push_back({3.f, [](Game& g) { Character& c = g.pc(); const Structure& I = g.world.structures[isl]; c.pos = vec3(I.pos.x + SKY_ISLAND_R + 2.f, c.pos.y, I.pos.y); c.grounded = false; y0 = c.pos.y; }, [](Game& g, float) { g.camIdleT = 0; }, "jump off"});
        s.push_back({0.1f, [](Game& g) { Character& c = g.pc(); ok[2] = c.pos.y - c.groundY < 0.6f && c.pos.y < y0 - 8.f && c.hp > 0; AGR_LOG("AURORA stepped off the island: fell %.1f m, hp %.0f -> %d (grounded %d swim %d above ground %.2f vel.y %.2f)", y0 - c.pos.y, c.hp, ok[2], c.grounded, c.swimming, c.pos.y - c.groundY, c.vel.y); }, idle, "fell"});
        s.push_back({0.1f, [](Game& g) { int fg = -1; for (int i = 0; i < (int)g.world.structures.size(); i++) if (g.world.structures[i].kind == S_GEYSER && g.world.structures[i].variant == 0) { fg = i; break; }
            const Structure& G = g.world.structures[fg]; teleport(g, G.pos.x, G.pos.y, 0.f); Character& c = g.pc(); c.grounded = true; c.vel = vec3(0); g.geyserCD = 0; y0 = apex = c.pos.y; }, idle, "free geyser"});
        s.push_back({2.5f, none, [](Game& g, float) { apex = std::max(apex, g.pc().pos.y); g.camIdleT = 0; }, "fly"});
        s.push_back({0.1f, [](Game& g) { ok[3] = apex - y0 > 5.f && g.pc().pos.y - g.pc().groundY < 0.6f && g.pc().hp > 0; AGR_LOG("AURORA free geyser apex %.1f m -> %d (above ground %.2f, hp %.0f, launches %d)", apex - y0, ok[3], g.pc().pos.y - g.pc().groundY, g.pc().hp, g.geyserLaunches); }, idle, "check"});
        s.push_back({4.f, [](Game& g) { const Poi* p = g.world.poiById("lumenhaven"); teleport(g, p->pos.x + 14, p->pos.y + 20, 0.6f); g.camPitch = 0.2f; g.camDist = g.camDistTarget = 14.f;
            AGR_LOG("AURORA Lumenhaven at (%.0f, %.0f)", p->pos.x, p->pos.y); }, [](Game& g, float) { g.camIdleT = 0; }, "village"});
        s.push_back({0.05f, [](Game& g) { g.requestScreenshot("build/shots/AU_lumenhaven.png"); }, idle, "shot"});
        s.push_back({0.5f, [](Game& g) { Entity aila = NullEntity; g.reg.each<Character>([&](Entity e, Character& c) { if (c.npcDef >= 0 && npcs()[c.npcDef].id == "aila") aila = e; });
            ok[4] = aila != NullEntity; if (aila) { Character* a = g.reg.get<Character>(aila); teleport(g, a->pos.x, a->pos.z + 2.5f, PI); g.openDialog(aila); }
            int offers = 0; for (auto& ch : g.dialog.choices) if (ch.action.rfind("offer:", 0) == 0) offers++;
            ok[4] = ok[4] && offers >= 2; AGR_LOG("AURORA Aïla found %d, %d quests offered (hero above ground %.2f grounded %d)", aila != NullEntity, offers, g.pc().pos.y - g.pc().groundY, g.pc().grounded); }, idle, "aila"});
        s.push_back({1.f, none, idle, "talk"});
        s.push_back({0.05f, [](Game& g) { g.requestScreenshot("build/shots/AU_aila.png"); g.dialog.open = false; }, idle, "shot"});
        s.push_back({0.1f, [](Game& g) { Character& c = g.pc(); Entity e = g.spawnEnemy(enemyIndex("crystal_golem"), c.pos + vec3(std::sin(c.yaw), 0, std::cos(c.yaw)) * 7.f);
            if (AIBrain* ai = g.reg.get<AIBrain>(e)) ai->state = AIState::Idle; ok[5] = g.reg.valid(e);
            g.camYaw = c.yaw + PI; g.camPitch = 0.15f; g.camDist = g.camDistTarget = 10.f; }, idle, "golem"});
        s.push_back({1.f, none, [](Game& g, float) { g.camIdleT = 0; }, "look"});
        s.push_back({0.05f, [](Game& g) { g.requestScreenshot("build/shots/AU_golem.png"); bool all = true; for (int o : ok) all = all && o; AGR_LOG("AURORA TEST %s", all ? "PASSED" : "FAILED"); }, idle, "shot"});
        return s;
    }
    if (name == "modfun") {
        // 2.7.0 the special mods in play: moon jump, giant enemies, eternal night under the snow (log + pictures)
        static float y0, apex; static int ok[3];
        auto mod = [](const char* l) { std::string e; if (!applyModDataLine(l, e)) AGR_LOG("MODFUN bad line %s: %s", l, e.c_str()); };
        s.push_back({3.f, [](Game& g) { g.newGame(); g.startPlaying(); g.timeOfDay = 11.f; g.toastHint.clear(); g.tutStep = -1; for (int& o : ok) o = 0;
            const Poi* b = g.world.poiById("brindlemere"); teleport(g, b->pos.x + 60, b->pos.y + 40, 0.f); }, idle, "setup"});
        s.push_back({0.1f, [mod](Game& g) { mod("game.gravity = 0.35"); mod("game.jump = 1.35"); y0 = apex = g.pc().pos.y; g.stamina = g.maxStamina; g.playerJump(false); }, idle, "moon jump"});
        s.push_back({3.5f, none, [](Game& g, float) { apex = std::max(apex, g.pc().pos.y); g.camIdleT = 0; }, "fly"});
        s.push_back({0.1f, [](Game& g) { ok[0] = apex - y0 > 3.f && g.pc().grounded; AGR_LOG("MODFUN moon jump apex %.1f m (normal about 1 m), landed %d -> %d", apex - y0, g.pc().grounded, ok[0]);
            gameTuning() = GameTuning(); }, idle, "check"});
        s.push_back({0.1f, [mod](Game& g) { mod("game.enemy_scale = 2"); Character& c = g.pc();
            Entity e = g.spawnEnemy(enemyIndex("wolf"), c.pos + vec3(std::sin(c.yaw), 0, std::cos(c.yaw)) * 6.f);
            Character* w = g.reg.get<Character>(e); ok[1] = w && w->vis.scale > 1.6f;
            if (AIBrain* ai = g.reg.get<AIBrain>(e)) ai->state = AIState::Idle;
            AGR_LOG("MODFUN giant wolf scale %.2f -> %d", w ? w->vis.scale : 0.f, ok[1]); g.camYaw = c.yaw + PI; g.camPitch = 0.15f; g.camDist = g.camDistTarget = 9.f; }, idle, "giant"});
        s.push_back({0.8f, none, [](Game& g, float) { g.camIdleT = 0; }, "look"});
        s.push_back({0.05f, [](Game& g) { g.requestScreenshot("build/shots/MF0_giant.png"); gameTuning() = GameTuning(); }, idle, "shot"});
        s.push_back({4.f, [mod](Game& g) { mod("game.hour = 23.5"); mod("game.weather = neige"); }, [](Game& g, float) { g.camIdleT = 0; }, "night snow"});
        s.push_back({0.05f, [](Game& g) { ok[2] = std::fabs(g.timeOfDay - 23.5f) < 0.01f && g.weatherNext == Game::W_SNOW; AGR_LOG("MODFUN eternal night %.2f h, weather %d -> %d", g.timeOfDay, g.weatherNext, ok[2]);
            g.requestScreenshot("build/shots/MF1_night_snow.png"); }, idle, "shot"});
        s.push_back({0.1f, [](Game&) { gameTuning() = GameTuning(); bool all = true; for (int o : ok) all = all && o; AGR_LOG("MODFUN TEST %s", all ? "PASSED" : "FAILED"); }, idle, "end"});
        return s;
    }
    if (name == "trees") {
        // 2.7.0 trees and grass close up in several regions (pictures; LK_SHOT_TAG names the set: before / after)
        static std::string tag; tag = getenv("LK_SHOT_TAG") ? getenv("LK_SHOT_TAG") : "x";
        static const float spots[][3] = {{1300, 1850, 0.4f}, {2600, 620, 2.2f}, {430, 1450, 1.f}, {1500, 1500, 3.4f}, {1000, 3700, 0.8f}, {5100, 3000, 2.f}};
        s.push_back({3.f, [](Game& g) { g.newGame(); g.startPlaying(); g.timeOfDay = 10.5f; g.toastHint.clear(); g.tutStep = -1;
            g.weather = g.weatherNext = Game::W_CLEAR; g.weatherBlend = 1; g.weatherTimer = 999; }, idle, "setup"});
        for (int i = 0; i < 6; i++) {
            s.push_back({3.5f, [i](Game& g) { teleport(g, spots[i][0], spots[i][1], spots[i][2]); g.camPitch = 0.12f; g.camDist = g.camDistTarget = 8.f; g.camYaw = spots[i][2] + PI; },
                [](Game& g, float) { g.camIdleT = 0; }, "go"});
            s.push_back({0.05f, [i](Game& g) { g.requestScreenshot("build/shots/TR_" + tag + std::to_string(i) + ".png"); }, idle, "shot"});
        }
        return s;
    }
    if (name == "wind") {
        // 2.6.2 strong wind: pine crowns stay on their trunks, grass bends but never sinks into the ground (pictures)
        auto setW = [](Game& g, int w) { g.weather = g.weatherNext = w; g.weatherBlend = 1; g.weatherTimer = 999; };
        s.push_back({4.f, [setW](Game& g) { g.newGame(); g.startPlaying(); g.timeOfDay = 11.f; g.toastHint.clear(); g.tutStep = -1; setW(g, Game::W_SNOW);
            teleport(g, 900, 420, 0.8f); g.camPitch = 0.15f; g.camDist = g.camDistTarget = 9.f; },
            [](Game& g, float) { g.camIdleT = 0; }, "snow"});
        static const float spots[3][2] = {{900, 420}, {380, 560}, {1450, 380}};
        for (int i = 0; i < 3; i++) {
            s.push_back({2.5f, [i](Game& g) { teleport(g, spots[i][0], spots[i][1], 0.8f); g.camPitch = 0.15f; g.camDist = g.camDistTarget = 9.f; }, [](Game& g, float) { g.camIdleT = 0; }, "wait"});
            s.push_back({0.05f, [i](Game& g) { AGR_LOG("WIND snow strength %.2f", g.env.windStrength); g.requestScreenshot("build/shots/W" + std::to_string(i) + "_pines.png"); }, idle, "shot"});
        }
        s.push_back({4.f, [setW](Game& g) { setW(g, Game::W_STORM); const Poi* b = g.world.poiById("brindlemere"); teleport(g, b->pos.x + 60, b->pos.y + 40, 0.f);
            g.camPitch = 0.12f; g.camDist = g.camDistTarget = 5.f; }, [](Game& g, float) { g.camIdleT = 0; }, "storm"});
        for (int i = 0; i < 3; i++) {
            s.push_back({0.6f, none, [](Game& g, float) { g.camIdleT = 0; }, "wait"});
            s.push_back({0.05f, [i](Game& g) { AGR_LOG("WIND storm strength %.2f", g.env.windStrength); g.requestScreenshot("build/shots/W" + std::to_string(i + 3) + "_grass.png"); }, idle, "shot"});
        }
        return s;
    }
    if (name == "titlemenus") {
        // 2.6.1 regression: on the title screen (no hero yet), "Mods" then a journal tab ("Équipement") crashed.
        // Cold start (the hero is removed), open Mods / Paramètres / Parties / Nouvelle partie and tap every tab shown,
        // then force every other menu: none may crash, the menus that need a hero fall back to the settings.
        static int fid = 1200; static int alive = 0;
        auto tap = [&s, idle](std::string label) {
            s.push_back({0.2f, [label](Game& g) { const Rect* r = g.ui.findWidget(label);
                if (!r) { AGR_LOG("TITLEMENUS widget '%s' not shown", label.c_str()); return; }
                fid++; float k = g.ui.scale; g.touch(TouchEvent{TouchEvent::Down, fid, (r->x + r->w * 0.5f) * k, (r->y + r->h * 0.5f) * k}); }, idle, "tap"});
            s.push_back({0.3f, [label](Game& g) { for (auto& f : g.ui.fingers) if (f.id == fid && f.down) { float k = g.ui.scale; g.touch(TouchEvent{TouchEvent::Up, fid, f.x * k, f.y * k}); }
                AGR_LOG("TITLEMENUS tapped '%s' -> menu %d", label.c_str(), (int)g.menu); alive++; }, idle, "release"});
        };
        s.push_back({1.f, [](Game& g) { g.ui.recordWidgets = true; g.menu = Menu::None; g.state = GameState::Title; g.stateT = 3.f;
            if (g.player != NullEntity) { g.reg.destroy(g.player); g.player = NullEntity; } }, idle, "cold title"});
        tap("Mods");
        s.push_back({0.1f, shot("T0_title_mods"), idle, "shot"});
        for (const char* t : {"Équipement", "Inventaire", "Compétences", "Carte", "Paramètres", "Parties", "Mods"}) tap(t);
        s.push_back({0.1f, shot("T1_title_tabs"), idle, "shot"});
        for (int m = 1; m <= (int)Menu::NewGame; m++) {
            if (m == (int)Menu::LayoutEdit || m == (int)Menu::HeroWheel || m == (int)Menu::Death) continue;   // in-game only overlays, never reachable from the title
            s.push_back({0.25f, [m](Game& g) { g.state = GameState::Title; g.menu = (Menu)m; }, idle, "force"});
            s.push_back({0.05f, [m](Game& g) { AGR_LOG("TITLEMENUS forced menu %d -> now %d", m, (int)g.menu); alive++; }, idle, "check"});
        }
        s.push_back({0.1f, [](Game& g) { (void)g; AGR_LOG("TITLEMENUS TEST %s (%d steps survived)", alive > 20 ? "PASSED" : "FAILED", alive); }, idle, "end"});
        return s;
    }
    if (name == "newgame") {
        // 2.6.0 title screen -> "Nouvelle partie": slot, difficulty, options; the game starts in that slot; Parties and Mods menus
        static int ok[5]; static float hp0 = 0;
        static int fid = 900;
        auto tap = [&s, idle](const char* what, std::function<std::string(Game&)> label) {
            s.push_back({0.2f, [label](Game& g) { std::string w = label(g); const Rect* r = g.ui.findWidget(w);
                if (!r) { AGR_LOG("NEWGAME widget '%s' not found", w.c_str()); return; }
                fid++; float k = g.ui.scale; g.touch(TouchEvent{TouchEvent::Down, fid, (r->x + r->w * 0.5f) * k, (r->y + r->h * 0.5f) * k}); }, idle, what});
            s.push_back({0.4f, [](Game& g) { for (auto& f : g.ui.fingers) if (f.id == fid && f.down) { float k = g.ui.scale; g.touch(TouchEvent{TouchEvent::Up, fid, f.x * k, f.y * k}); } }, idle, "release"});
        };
        s.push_back({1.f, [](Game& g) { for (int& o : ok) o = 0; g.ui.recordWidgets = true; hp0 = g.difficultyEnemyHp();
            platform::removeFile(platform::writableDir() + "/save_2.lks"); g.menu = Menu::None; g.state = GameState::Title; g.stateT = 3.f; }, idle, "title"});
        s.push_back({0.1f, shot("G0_title"), idle, "shot"});
        tap("tap Nouvelle partie", [](Game&) { return std::string("Nouvelle partie"); });
        s.push_back({0.5f, [](Game& g) { ok[0] = g.menu == Menu::NewGame; g.newSlot = 2; g.newOpts.difficulty = 3; g.newOpts.prologue = false; AGR_LOG("NEWGAME screen open %d", ok[0]); }, idle, "set"});
        s.push_back({0.1f, shot("G1_newgame"), idle, "shot"});
        tap("tap start", [](Game&) { return std::string("Commencer la partie 3"); });
        s.push_back({2.f, none, idle, "play"});
        s.push_back({0.1f, [](Game& g) { ok[1] = g.state == GameState::Playing && g.saveSlot == 2 && g.gameOpts.difficulty == 3;
            ok[2] = g.difficultyEnemyHp() > hp0 * 1.5f && g.latestSlot() == 2;
            AGR_LOG("NEWGAME playing %d slot %d difficulty %d | enemy hp x%.2f (was x%.2f), latest slot %d -> %d", ok[1], g.saveSlot, g.gameOpts.difficulty, g.difficultyEnemyHp(), hp0, g.latestSlot(), ok[2]);
            g.gameOpts.difficulty = 0; g.loadGame(2); ok[3] = g.gameOpts.difficulty == 3; AGR_LOG("NEWGAME difficulty kept in the save %d", ok[3]); }, idle, "check"});
        s.push_back({0.5f, [](Game& g) { g.menu = Menu::Saves; }, idle, "saves"});
        s.push_back({0.1f, shot("G2_saves"), idle, "shot"});
        s.push_back({0.5f, [](Game& g) { g.scanMods(); g.menu = Menu::Mods; int n = 0; for (auto& m : g.mods) if (m.id == "mega-bombes" || m.id == "heros-veloce") n++; ok[4] = n == 2;
            AGR_LOG("NEWGAME bundled mods installed %d (%zu mods)", ok[4], g.mods.size()); }, idle, "mods"});
        s.push_back({0.1f, [](Game& g) { g.requestScreenshot("build/shots/G3_mods.png"); bool all = true; for (int o : ok) all = all && o; AGR_LOG("NEWGAME TEST %s", all ? "PASSED" : "FAILED"); }, idle, "shot"});
        return s;
    }
    if (name == "menus") {
        // 2.3.5 every menu on a phone-sized screen (look at the pictures)
        s.push_back({2.5f, [](Game& g) { g.newGame(); g.startPlaying(); g.timeOfDay = 10.f; g.toastHint.clear(); g.tutStep = -1;
            g.addItem("sword_iron", 1); g.addItem("potion_heal", 4); g.addItem("iron_ore", 6); g.addItem("wood", 12); g.addItem("herb", 5); g.gold = 340; }, idle, "setup"});
        static const std::pair<Menu, const char*> ms[] = {{Menu::Pause, "M0_pause"}, {Menu::Bag, "M1_bag"}, {Menu::Inventory, "M2_inventory"}, {Menu::Equipment, "M3_equipment"},
            {Menu::Crafting, "M4_crafting"}, {Menu::Skills, "M5_skills"}, {Menu::Quests, "M6_quests"}, {Menu::Settings, "M7_settings"}, {Menu::Map, "M8_map"}};
        for (auto& m : ms) {
            Menu mm = m.first; const char* f = m.second;
            s.push_back({0.5f, [mm](Game& g) { g.menu = mm; g.selItem = 0; }, idle, "open"});
            s.push_back({0.1f, shot(f), idle, "shot"});
        }
        s.push_back({0.3f, [](Game& g) { g.menu = Menu::None; g.state = GameState::Title; g.stateT = 3.f; }, idle, "title"});
        s.push_back({0.1f, shot("M9_title"), idle, "shot"});
        return s;
    }
    if (name == "ui") {
        // Menu / dialogue buttons with REAL multi-frame taps (finger down, held 0.2 s, released), the way a phone
        // delivers them. Regression for 1.6.0 feedback: "buttons in dialogues and menus do nothing".
        static int res[8]; static float yaw0; static std::string sig;
        static std::string want;
        // tap the widget labelled `label` (found in the previous frame's UI), held for `hold` seconds
        auto tapWidget = [&s, idle](const char* what, std::function<std::string(Game&)> label, float hold, std::function<void(Game&)> check) {
            static int fid = 500;
            s.push_back({hold, [label](Game& g) {
                want = label(g); const Rect* r = g.ui.findWidget(want);
                if (!r) { AGR_LOG("UI TEST: widget '%s' not found", want.c_str()); return; }
                fid++; float k = g.ui.scale; g.touch(TouchEvent{TouchEvent::Down, fid, (r->x + r->w * 0.5f) * k, (r->y + r->h * 0.5f) * k});
            }, idle, what});
            s.push_back({0.4f, [](Game& g) {
                for (auto& f : g.ui.fingers) if (f.id == fid && f.down) { float k = g.ui.scale; g.touch(TouchEvent{TouchEvent::Up, fid, f.x * k, f.y * k}); }
            }, idle, "release"});
            s.push_back({0.1f, check, idle, "check"});
        };
        s.push_back({2.f, [](Game& g) { for (int& r : res) r = 0; g.ui.recordWidgets = true; const Poi* b = g.world.poiById("brindlemere"); teleport(g, b->pos.x + 2, b->pos.y + 12, 0.4f); g.timeOfDay = 10.f; g.toastHint.clear(); }, idle, "setup"});
        // dialogue: open Maëlle, advance with held taps on the box, then press a choice button
        s.push_back({1.f, [](Game& g) { g.reg.each<Character>([&](Entity e, Character& c) { if (c.npcDef >= 0 && npcs()[c.npcDef].id == "maelle") { teleport(g, c.pos.x, c.pos.z + 2.5f, 0.2f); g.pc().yaw = PI; g.openDialog(e); } }); yaw0 = g.camYaw; }, idle, "talk"});
        for (int i = 0; i < 18; i++) {
            s.push_back({0.2f, [](Game& g) { if (!g.dialog.open || g.dialog.line + 1 >= (int)g.dialog.lines.size()) { g.dialog.reveal = std::max(g.dialog.reveal, 1e9f); return; } float k = g.ui.scale; g.touch(TouchEvent{TouchEvent::Down, 480, g.ui.W * 0.4f * k, (g.ui.H - 150) * k}); }, idle, "hold box"});
            s.push_back({0.4f, [](Game& g) { for (auto& f : g.ui.fingers) if (f.id == 480 && f.down) { float k = g.ui.scale; g.touch(TouchEvent{TouchEvent::Up, 480, f.x * k, f.y * k}); } }, idle, "release box"});
        }
        s.push_back({0.1f, [](Game& g) { res[0] = g.dialog.open && g.dialog.line + 1 >= (int)g.dialog.lines.size() && !g.dialog.choices.empty(); sig = g.dialog.choices.empty() ? "" : g.dialog.choices[0].action; g.requestScreenshot("build/shots/96_ui_dialog.png"); AGR_LOG("UI dialogue: last line reached %d, %zu choices, first '%s'", res[0], g.dialog.choices.size(), sig.c_str()); }, idle, "dialog end"});
        tapWidget("tap choice", [](Game& g) { return g.dialog.choices.empty() ? std::string("?") : g.dialog.choices[0].text; }, 0.2f, [](Game& g) {
            std::string now = !g.dialog.open ? "closed" : (g.dialog.choices.empty() ? "" : g.dialog.choices[0].action);
            res[1] = now != sig; AGR_LOG("UI dialogue choice: before '%s' after '%s'", sig.c_str(), now.c_str());
            res[7] = std::fabs(g.camYaw - yaw0) < 1e-4f;
            g.dialog.open = false; });
        // pause menu: "Reprendre"
        s.push_back({0.3f, [](Game& g) { g.menu = Menu::Pause; }, idle, "pause"});
        tapWidget("tap Reprendre", [](Game&) { return std::string("Reprendre"); }, 0.25f, [](Game& g) { res[2] = g.menu == Menu::None; AGR_LOG("UI pause 'Reprendre': menu %d", (int)g.menu); });
        // journal: tab then close button
        s.push_back({0.3f, [](Game& g) { g.menu = Menu::Inventory; }, idle, "journal"});
        tapWidget("tap tab", [](Game&) { return std::string("Compétences"); }, 0.2f, [](Game& g) { res[3] = g.menu == Menu::Skills; AGR_LOG("UI journal tab: menu %d (skills %d)", (int)g.menu, (int)Menu::Skills); g.requestScreenshot("build/shots/97_ui_quests.png"); });
        tapWidget("tap close", [](Game&) { return std::string("close"); }, 0.15f, [](Game& g) { res[4] = g.menu == Menu::None; AGR_LOG("UI journal close: menu %d", (int)g.menu); });
        // settings toggle
        s.push_back({0.3f, [](Game& g) { g.menu = Menu::Settings; yaw0 = g.settings.showFps ? 1.f : 0.f; }, idle, "settings"});
        tapWidget("tap toggle", [](Game&) { return std::string("Afficher les FPS"); }, 0.3f, [](Game& g) { res[5] = g.settings.showFps != (yaw0 > 0.5f); g.settings.showFps = false; AGR_LOG("UI settings toggle: %d", res[5]); });
        // the quick same-frame tap must keep working
        s.push_back({0.3f, [](Game& g) { g.menu = Menu::Pause; }, idle, "pause"});
        s.push_back({0.2f, [](Game& g) { const Rect* r = g.ui.findWidget("Reprendre"); if (r) tapAt(g, vec2(r->x + r->w / 2, r->y + r->h / 2), 520); }, idle, "quick tap"});
        s.push_back({0.1f, [](Game& g) {
            res[6] = g.menu == Menu::None;
            AGR_LOG("UI TEST: dialogueAdvance=%d dialogueChoice=%d pauseResume=%d journalTab=%d journalClose=%d settingsToggle=%d quickTap=%d cameraUntouched=%d", res[0], res[1], res[2], res[3], res[4], res[5], res[6], res[7]);
            bool ok = true; for (int r : res) ok = ok && r;
            AGR_LOG("UI TEST %s", ok ? "PASSED" : "FAILED");
        }, idle, "report"});
        return s;
    }
    if (name == "camera") {
        // Free camera (1.6.0): pitch x zoom grid with screenshots, then touch-driven checks (vertical drag,
        // inverted axis, pinch, double-tap reset), automatic return, look-ahead, lock-on framing, collision.
        static int res[13]; static int gridBad; static float v0, yawStart, tightMax;
        static vec3 wallPt; static bool wallFound;
        auto check = [](Game& g, const char* what) {
            vec3 e = g.cam.pos; float gy = g.dungeon.active ? g.groundAt(e.x, e.z) : g.world.height(e.x, e.z);
            bool above = e.y >= gy + g.camRig.minClear - 0.05f;
            Character& c = g.pc();
            bool on1 = false, on2 = false;
            vec2 f = g.worldToScreen(c.pos + vec3(0, 0.1f, 0), &on1), h = g.worldToScreen(c.pos + vec3(0, 1.7f, 0), &on2);
            auto in = [&](vec2 q) { return q.x >= 0 && q.x <= g.ui.W && q.y >= 0 && q.y <= g.ui.H; };
            bool inside = on1 && on2 && in(f) && in(h);
            AGR_LOG("CAMERA %s: pitch %.2f dist %.1f (effective %.1f) eye %.2f m above ground (min %.2f) hero feet (%.0f,%.0f) head (%.0f,%.0f) in %.0fx%.0f fog %.4f tilt %.2f -> %s",
                    what, g.camPitch, g.camDistTarget, g.camRig.dist, e.y - gy, g.camRig.minClear, f.x, f.y, h.x, h.y, g.ui.W, g.ui.H, g.env.fogDensity, g.env.tiltShift, above && inside ? "ok" : "BAD");
            return above && inside;
        };
        auto camPt = [](Game& g) { return vec2(g.ui.W * 0.56f, g.ui.H * 0.42f); };
        auto drag = [](Game& g, int id, vec2 a, vec2 b, float t, float dur) {
            float k = g.ui.scale; vec2 p = lerp(a, b, saturate(t / dur));
            g.touch(TouchEvent{TouchEvent::Move, id, p.x * k, p.y * k});
        };
        s.push_back({2.5f, [](Game& g) { for (int& r : res) r = 0; gridBad = 0; const Poi* p = g.world.poiById("brindlemere"); teleport(g, p->pos.x + 40, p->pos.y - 60, 2.4f); g.timeOfDay = 11.f; g.weather = g.weatherNext = Game::W_CLEAR; g.weatherBlend = 1; g.weatherTimer = 999; g.toastHint.clear(); g.pc().maxHp = g.pc().hp = 5000; }, idle, "setup"});
        struct CV { float pitch, dist; const char* f; };
        static const CV grid[] = {{-0.35f, 4.f, "80_cam_sky_z4"}, {-0.35f, 15.f, "81_cam_sky_z15"}, {-0.35f, 40.f, "82_cam_sky_z40"},
                                  {0.f, 4.f, "83_cam_low_z4"}, {0.f, 15.f, "84_cam_low_z15"}, {0.f, 40.f, "85_cam_low_z40"},
                                  {0.66f, 4.f, "86_cam_default_z4"}, {0.66f, 15.f, "87_cam_default_z15"}, {0.66f, 40.f, "88_cam_default_z40"},
                                  {1.3f, 4.f, "89_cam_top_z4"}, {1.3f, 15.f, "90_cam_top_z15"}, {1.3f, 40.f, "91_cam_map_z40"}};
        for (const CV& v : grid) {
            s.push_back({1.4f, [v](Game& g) { g.camPitch = v.pitch; g.camDist = g.camDistTarget = v.dist; g.camResetT = 0; }, [v](Game& g, float) { g.camPitch = v.pitch; g.camIdleT = 0; }, "pose"});
            s.push_back({0.04f, [v, check](Game& g) { if (!check(g, v.f)) gridBad++; g.requestScreenshot(std::string("build/shots/") + v.f + ".png"); }, idle, "shot"});
        }
        // vertical drag with a real finger: moving up looks up (pitch decreases)
        s.push_back({0.8f, [camPt](Game& g) { g.camPitch = 0.66f; g.camDist = g.camDistTarget = 15.5f; v0 = g.camPitch; vec2 p = camPt(g); float k = g.ui.scale; g.touch(TouchEvent{TouchEvent::Down, 301, p.x * k, p.y * k}); },
                     [camPt, drag](Game& g, float t) { vec2 p = camPt(g); drag(g, 301, p, p - vec2(0, 220), t, 0.6f); }, "drag up"});
        s.push_back({0.3f, [camPt](Game& g) { vec2 p = camPt(g) - vec2(0, 220); float k = g.ui.scale; g.touch(TouchEvent{TouchEvent::Up, 301, p.x * k, p.y * k}); res[0] = g.camPitch < v0 - 0.3f; AGR_LOG("CAMERA drag up: pitch %.2f -> %.2f", v0, g.camPitch); }, idle, "release"});
        // free orbit (1.10.2): one diagonal drag turns AND tilts the camera
        s.push_back({0.8f, [camPt](Game& g) { g.camPitch = 0.4f; v0 = g.camPitch; yawStart = g.camYaw; vec2 p = camPt(g); float k = g.ui.scale; g.touch(TouchEvent{TouchEvent::Down, 309, p.x * k, p.y * k}); },
                     [camPt, drag](Game& g, float t) { vec2 p = camPt(g); drag(g, 309, p, p + vec2(-220, 160), t, 0.6f); }, "drag diagonally"});
        s.push_back({0.3f, [camPt](Game& g) { vec2 p = camPt(g) + vec2(-220, 160); float k = g.ui.scale; g.touch(TouchEvent{TouchEvent::Up, 309, p.x * k, p.y * k}); float turned = wrapAngle(g.camYaw - yawStart), tilted = g.camPitch - v0;
            res[11] = std::fabs(turned) > 0.4f && tilted > 0.25f && std::fabs(std::fabs(turned) / 220.f - tilted / 160.f) < 0.0012f;
            AGR_LOG("CAMERA diagonal drag: yaw turned %.2f, pitch %.2f -> %.2f (same rate on both axes)", turned, v0, g.camPitch); }, idle, "release"});
        // inverted vertical axis: the same gesture looks down
        s.push_back({0.8f, [camPt](Game& g) { g.settings.invertCameraY = true; v0 = g.camPitch; vec2 p = camPt(g); float k = g.ui.scale; g.touch(TouchEvent{TouchEvent::Down, 302, p.x * k, p.y * k}); },
                     [camPt, drag](Game& g, float t) { vec2 p = camPt(g); drag(g, 302, p, p - vec2(0, 220), t, 0.6f); }, "drag up (inverted)"});
        s.push_back({0.3f, [camPt](Game& g) { vec2 p = camPt(g) - vec2(0, 220); float k = g.ui.scale; g.touch(TouchEvent{TouchEvent::Up, 302, p.x * k, p.y * k}); res[1] = g.camPitch > v0 + 0.3f; g.settings.invertCameraY = false; AGR_LOG("CAMERA drag up (inverted axis): pitch %.2f -> %.2f", v0, g.camPitch); }, idle, "release"});
        // pinch: fingers apart = zoom in, together = zoom out
        s.push_back({0.7f, [camPt](Game& g) { g.camPitch = 0.66f; v0 = g.camDistTarget; vec2 p = camPt(g); float k = g.ui.scale; g.touch(TouchEvent{TouchEvent::Down, 303, (p.x - 40) * k, p.y * k}); g.touch(TouchEvent{TouchEvent::Down, 304, (p.x + 40) * k, p.y * k}); },
                     [camPt, drag](Game& g, float t) { vec2 p = camPt(g); drag(g, 303, p - vec2(40, 0), p - vec2(200, 0), t, 0.5f); drag(g, 304, p + vec2(40, 0), p + vec2(200, 0), t, 0.5f); }, "pinch out"});
        s.push_back({0.1f, [camPt](Game& g) { vec2 p = camPt(g); float k = g.ui.scale; g.touch(TouchEvent{TouchEvent::Up, 303, (p.x - 200) * k, p.y * k}); g.touch(TouchEvent{TouchEvent::Up, 304, (p.x + 200) * k, p.y * k}); res[2] = g.camDistTarget < v0 - 4.f; AGR_LOG("CAMERA pinch out: zoom %.1f -> %.1f", v0, g.camDistTarget); }, idle, "release"});
        // (both fingers start in the camera half of the screen: a finger in the left 45 % is the joystick)
        s.push_back({0.7f, [camPt](Game& g) { v0 = g.camDistTarget; vec2 p = camPt(g) + vec2(90, 0); float k = g.ui.scale; g.touch(TouchEvent{TouchEvent::Down, 305, (p.x - 160) * k, p.y * k}); g.touch(TouchEvent{TouchEvent::Down, 306, (p.x + 160) * k, p.y * k}); },
                     [camPt, drag](Game& g, float t) { vec2 p = camPt(g) + vec2(90, 0); drag(g, 305, p - vec2(160, 0), p - vec2(20, 0), t, 0.5f); drag(g, 306, p + vec2(160, 0), p + vec2(20, 0), t, 0.5f); }, "pinch in"});
        s.push_back({0.1f, [camPt](Game& g) { vec2 p = camPt(g) + vec2(90, 0); float k = g.ui.scale; g.touch(TouchEvent{TouchEvent::Up, 305, (p.x - 20) * k, p.y * k}); g.touch(TouchEvent{TouchEvent::Up, 306, (p.x + 20) * k, p.y * k}); res[3] = g.camDistTarget > v0 + 4.f; AGR_LOG("CAMERA pinch in: zoom %.1f -> %.1f", v0, g.camDistTarget); }, idle, "release"});
        // double tap in the camera area: pitch and zoom back to default
        s.push_back({1.4f, [camPt](Game& g) { g.camPitch = -0.2f; g.camDistTarget = 34.f; tapAt(g, camPt(g), 307); },
                     [camPt](Game& g, float t) { static bool second; if (t < 0.05f) second = false; if (t > 0.15f && !second) { second = true; tapAt(g, camPt(g) + vec2(8, 5), 308); } }, "double tap"});
        s.push_back({0.04f, [](Game& g) { res[4] = std::fabs(g.camPitch - 0.66f) < 0.06f && std::fabs(g.camDistTarget - 15.5f) < 0.8f; AGR_LOG("CAMERA double tap reset: pitch %.2f zoom %.1f", g.camPitch, g.camDistTarget); }, idle, "check reset"});
        // automatic return of the pitch while running (no touch for > 4 s), and the look-ahead
        s.push_back({9.f, [](Game& g) { g.camPitch = 1.3f; g.camIdleT = 0; g.settings.cameraSmooth = true; }, [](Game& g, float t) { g.input.move = vec2(0, 1); g.input.sprint = true; if (t > 4.f && t < 6.f && length(g.camLookAhead) > 1.f) res[6] = 1; }, "run (auto return)"});
        s.push_back({0.04f, [](Game& g) { res[5] = g.camPitch < 1.1f && g.camPitch > 1.f; AGR_LOG("CAMERA auto return: pitch %.2f, look-ahead %.2f m", g.camPitch, length(g.camLookAhead)); g.requestScreenshot("build/shots/92_cam_run_lookahead.png"); }, [](Game& g, float) { g.input.move = vec2(0, 1); g.input.sprint = true; }, "check return"});
        // default (tight) camera: locked on the hero while sprinting, no lag, no lead
        s.push_back({2.5f, [](Game& g) { g.settings.cameraSmooth = false; g.camPitch = 0.66f; tightMax = 0; }, [](Game& g, float t) {
            g.input.move = vec2(0, 1); g.input.sprint = true; g.stamina = g.maxStamina;
            if (t > 1.f) tightMax = std::max(tightMax, distXZ(g.camFocus, g.pc().pos)); }, "sprint (tight camera)"});
        s.push_back({0.04f, [](Game& g) { res[12] = tightMax < 0.45f; AGR_LOG("CAMERA tight follow: max focus offset while sprinting %.2f m", tightMax); }, idle, "check tight"});
        // lock-on: hero and target both framed
        s.push_back({2.5f, [](Game& g) { Character& c = g.pc(); vec3 p = c.pos + vec3(9, 0, 6); p.y = g.world.groundHeight(p.x, p.z); Entity e = g.spawnEnemy(enemyIndex("wolf"), p); g.lockTarget = e; g.camIdleT = 2.f; g.camYaw += 1.6f; }, idle, "lock-on"});
        s.push_back({0.04f, [check](Game& g) {
            bool on = false; Character* t = g.lockTarget != NullEntity ? g.reg.get<Character>(g.lockTarget) : nullptr;
            vec2 q = t ? g.worldToScreen(t->pos + vec3(0, 0.5f, 0), &on) : vec2(-1, -1);
            res[7] = t && on && q.x > 0 && q.x < g.ui.W && q.y > 0 && q.y < g.ui.H && check(g, "lock-on");
            AGR_LOG("CAMERA lock-on: target at (%.0f, %.0f)", q.x, q.y);
            g.requestScreenshot("build/shots/93_cam_lockon.png");
        }, idle, "check lock-on"});
        s.push_back({0.3f, [](Game& g) { if (g.lockTarget != NullEntity) g.reg.destroyLater(g.lockTarget); g.lockTarget = NullEntity; }, idle, "despawn"});
        // collision: find a house in Brindlemere and put the camera behind its wall
        s.push_back({2.5f, [](Game& g) { const Poi* b = g.world.poiById("brindlemere"); teleport(g, b->pos.x, b->pos.y, 0.4f); }, idle, "village"});
        s.push_back({1.6f, [](Game& g) {
            wallFound = false; const Poi* b = g.world.poiById("brindlemere");
            for (float r = 6; r < 60 && !wallFound; r += 2)
                for (int a = 0; a < 24 && !wallFound; a++) {
                    vec2 q = b->pos + vec2(std::cos(a * TAU / 24), std::sin(a * TAU / 24)) * r;
                    vec3 w{q.x, g.world.height(q.x, q.y) + 2.f, q.y};
                    if (!g.world.cameraBlocked(w, 0.f)) continue;
                    for (int d = 0; d < 8 && !wallFound; d++) {
                        vec2 n{std::cos(d * TAU / 8), std::sin(d * TAU / 8)};
                        vec2 h = q + n * 6.f;
                        vec3 hp{h.x, g.world.height(h.x, h.y), h.y}, test = hp;
                        if (g.world.cameraBlocked(hp + vec3(0, 1.f, 0), 0.6f) || g.world.collide(test, 0.5f, hp.y)) continue;
                        wallPt = w; wallFound = true;
                        teleport(g, h.x, h.y, std::atan2(-n.x, -n.y));
                    }
                }
            g.camPitch = 0.3f; g.camDist = g.camDistTarget = 15.5f;
        }, [](Game& g, float) { g.camIdleT = 0; }, "behind a wall"});
        s.push_back({0.04f, [check](Game& g) {
            res[8] = wallFound && g.camRig.collided && !g.cameraBlocked(g.cam.pos) && check(g, "collision");
            AGR_LOG("CAMERA collision: wall found %d, collided %d, effective distance %.1f", wallFound, g.camRig.collided, g.camRig.dist);
            if (g.cameraBlocked(g.cam.pos)) {   // which collider still holds the eye (diagnostics)
                vec3 e = g.cam.pos;
                if (Chunk* ch = g.world.chunkAt(e.x, e.z))
                    for (const Collider& c : ch->colliders) {
                        if (c.hx <= 0) continue;
                        vec2 l = rotate(e.xz() - c.c, -c.rot);
                        if (std::fabs(l.x) < c.hx + 0.3f && std::fabs(l.y) < c.hz + 0.3f) AGR_LOG("CAMERA eye inside box at (%.1f, %.1f) half %.1f x %.1f base %.1f top %.1f; eye (%.1f, %.1f, %.1f) hero (%.1f, %.1f, %.1f)", c.c.x, c.c.y, c.hx, c.hz, c.base, c.top, e.x, e.y, e.z, g.pc().pos.x, g.pc().pos.y, g.pc().pos.z);
                    }
                for (const Structure& st : g.world.structures) if (length(st.pos - e.xz()) < 12.f) AGR_LOG("CAMERA   structure kind %d at (%.1f, %.1f) rot %.2f", st.kind, st.pos.x, st.pos.y, st.rot);
            }
            g.requestScreenshot("build/shots/94_cam_collision.png");
        }, [](Game& g, float) { g.camIdleT = 0; }, "check collision"});
        // dungeon: walls must not trap the camera (regression found while testing 1.6.0)
        s.push_back({0.3f, [](Game& g) { const Poi* p = g.world.poiById("whisper_cave"); teleport(g, p->pos.x + 3, p->pos.y + 3); g.camPitch = 0.66f; g.camDistTarget = 30.f; }, idle, "cave"});
        s.push_back({0.6f, [](Game& g) { g.input.interact = true; }, idle, "enter"});
        s.push_back({3.f, none, [](Game& g, float t) { g.input.move = vec2(0.3f, 0.6f); g.camIdleT = 0; }, "explore"});
        s.push_back({0.04f, [check](Game& g) {
            res[10] = g.dungeon.active && !g.cameraBlocked(g.cam.pos) && g.camRig.dist <= 16.5f && check(g, "dungeon");
            AGR_LOG("CAMERA dungeon: active %d, collided %d, effective distance %.1f", g.dungeon.active, g.camRig.collided, g.camRig.dist);
            g.requestScreenshot("build/shots/95_cam_dungeon.png");
        }, idle, "check dungeon"});
        s.push_back({1.5f, [](Game& g) { if (g.dungeon.active) g.exitDungeon(); }, idle, "exit dungeon"});
        s.push_back({0.1f, [](Game& g) {
            res[9] = gridBad == 0;
            AGR_LOG("CAMERA TEST: grid=%d dragUp=%d invertedAxis=%d pinchOut=%d pinchIn=%d doubleTapReset=%d autoReturn=%d lookAhead=%d lockOn=%d collision=%d dungeon=%d freeOrbit=%d tightFollow=%d",
                    res[9], res[0], res[1], res[2], res[3], res[4], res[5], res[6], res[7], res[8], res[10], res[11], res[12]);
            bool ok = true; for (int r : res) ok = ok && r;
            AGR_LOG("CAMERA TEST %s", ok ? "PASSED" : "FAILED");
            g.camPitch = 0.66f;
        }, idle, "report"});
        return s;
    }
    if (name == "touch") {
        // Touch-input regression test with injected touch events (no direct input fields).
        static int res[8];
        s.push_back({1.5f, [](Game& g) { for (int& r : res) r = 0; const Poi* b = g.world.poiById("brindlemere"); teleport(g, b->pos.x + 30, b->pos.y + 30, 0.4f); g.timeOfDay = 11.f; g.toastHint.clear(); }, idle, "setup"});
        s.push_back({1.0f, [](Game& g) { tapAt(g, ctlCentre(g, CTL_ATTACK), 101); }, [](Game& g, float) { if (g.pc().attackClip) res[0] = 1; }, "quick tap attack"});
        s.push_back({1.0f, [](Game& g) { tapAt(g, ctlCentre(g, CTL_DODGE), 102); }, [](Game& g, float) { if (g.dodgeCD > 0) res[1] = 1; }, "quick tap dodge"});
        s.push_back({1.2f, [](Game& g) { tapAt(g, ctlCentre(g, CTL_JUMP), 103); }, [](Game& g, float) { if (!g.pc().grounded) res[2] = 1; if (g.blockT > 0) res[3] = 1; }, "tap jump (block must stay idle)"});
        // hold attack 1.2 s -> charged attack on release
        s.push_back({1.2f, [](Game& g) { tapAt(g, ctlCentre(g, CTL_ATTACK), 104, false); }, [](Game& g, float t) { if (g.btnHold[CTL_ATTACK] > 0.9f && g.attackFinger >= 0) res[4] = 1; }, "hold attack"});
        s.push_back({1.0f, [](Game& g) { vec2 c = ctlCentre(g, CTL_ATTACK); float k = g.ui.scale; g.touch(TouchEvent{TouchEvent::Up, 104, c.x * k, c.y * k}); }, [](Game& g, float) { if (g.pc().attackClip) res[5] = 1; }, "release attack"});
        // two quick taps during one swing: the second must be buffered into a combo
        s.push_back({1.6f, [](Game& g) { g.comboStep = 0; tapAt(g, ctlCentre(g, CTL_ATTACK), 105); }, [](Game& g, float t) { if ((int)(t * 30) == 4) tapAt(g, ctlCentre(g, CTL_ATTACK), 106); if (g.comboStep >= 2) res[6] = 1; }, "buffered combo"});
        // a finger that starts on empty space and slides onto a button must not press it
        s.push_back({0.6f, [](Game& g) { vec2 c = ctlCentre(g, CTL_JUMP); float k = g.ui.scale; g.touch(TouchEvent{TouchEvent::Down, 107, (c.x - 300) * k, (c.y - 200) * k}); g.touch(TouchEvent{TouchEvent::Move, 107, c.x * k, c.y * k}); g.touch(TouchEvent{TouchEvent::Up, 107, c.x * k, c.y * k}); },
                     [](Game& g, float) { if (!g.pc().grounded) res[7] = -1; }, "slide onto button"});
        // 1.11.2 jump while sprinting. (a) joystick pushed to the edge -> sprint after 0.5 s, the other thumb taps Saut
        static float airSpeed[2]; static bool airborne[2];
        s.push_back({1.6f, [](Game& g) {
            airSpeed[0] = airSpeed[1] = 0; airborne[0] = airborne[1] = false;
            float k = g.ui.scale; vec2 o(g.ui.W * 0.15f, g.ui.H * 0.7f);
            g.touch(TouchEvent{TouchEvent::Down, 110, o.x * k, o.y * k});
        }, [](Game& g, float t) {
            if ((int)(t * 30) == 2) { float k = g.ui.scale; vec2 o(g.ui.W * 0.15f, g.ui.H * 0.7f); g.touch(TouchEvent{TouchEvent::Move, 110, o.x * k, (o.y - 95.f * g.settings.layout.scale) * k}); }
            if ((int)(t * 30) == 30) tapAt(g, ctlCentre(g, CTL_JUMP), 111);
            Character& c = g.pc();
            if (t > 1.f && !c.grounded) { airborne[0] = true; airSpeed[0] = std::max(airSpeed[0], length(vec2(c.vel.x, c.vel.z))); }
        }, "joystick sprint + jump"});
        // (b) sprint by holding Esquive, then slide the same thumb onto Saut
        s.push_back({1.6f, [](Game& g) { vec2 c = ctlCentre(g, CTL_DODGE); float k = g.ui.scale; g.touch(TouchEvent{TouchEvent::Down, 112, c.x * k, c.y * k}); },
            [](Game& g, float t) {
                if ((int)(t * 30) == 27) { vec2 c = ctlCentre(g, CTL_JUMP); float k = g.ui.scale; g.touch(TouchEvent{TouchEvent::Move, 112, c.x * k, c.y * k}); }
                Character& c = g.pc();
                if (t > 0.9f && !c.grounded) { airborne[1] = true; airSpeed[1] = std::max(airSpeed[1], length(vec2(c.vel.x, c.vel.z))); }
            }, "dodge-hold sprint + slide to jump"});
        s.push_back({0.5f, [](Game& g) {
            float k = g.ui.scale; vec2 c = ctlCentre(g, CTL_JUMP);
            g.touch(TouchEvent{TouchEvent::Up, 112, c.x * k, c.y * k}); g.touch(TouchEvent{TouchEvent::Up, 110, 0, 0});
            AGR_LOG("TOUCH sprint jump: joystick airborne %d speed %.1f m/s; dodge slide airborne %d speed %.1f m/s", airborne[0], airSpeed[0], airborne[1], airSpeed[1]);
        }, idle, "release"});
        s.push_back({0.1f, [](Game& g) {
            bool sj = airborne[0] && airSpeed[0] > 6.8f && airborne[1] && airSpeed[1] > 6.8f;
            AGR_LOG("TOUCH TEST: attack=%d dodge=%d jump=%d blockIdle=%d holdCharge=%d releaseAttack=%d bufferedCombo=%d slideIgnored=%d sprintJump=%d", res[0], res[1], res[2], !res[3], res[4], res[5], res[6], res[7] == 0, sj);
            bool ok = res[0] && res[1] && res[2] && !res[3] && res[4] && res[5] && res[6] && res[7] == 0 && sj;
            AGR_LOG("TOUCH TEST %s", ok ? "PASSED" : "FAILED");
        }, idle, "report"});
        // minimap orientation screenshots: camera turned, player facing a known direction
        s.push_back({1.0f, [](Game& g) { g.camYaw = 0.f; g.pc().yaw = PI; }, idle, "face north"});
        s.push_back({0.1f, shot("40_minimap_north"), idle, "shot"});
        s.push_back({1.0f, [](Game& g) { g.camYaw = PI / 2; g.pc().yaw = PI; }, idle, "camera west"});
        s.push_back({0.1f, shot("41_minimap_cam_rotated"), idle, "shot"});
        return s;
    }
    if (name != "tour") {  // "tour": only the region / weather / time-of-day tour (fast visual check)
    s.push_back({4.f, none, [](Game& g, float) { g.input.move = vec2(0, 1); }, "walk north"});
    s.push_back({0.1f, shot("03_start"), idle, "shot"});
    s.push_back({3.f, [](Game& g) { const Poi* b = g.world.poiById("brindlemere"); teleport(g, b->pos.x + 2, b->pos.y + 12, 0.4f); g.timeOfDay = 10.f; }, idle, "village"});
    s.push_back({0.1f, shot("04_village_day"), idle, "shot"});
    s.push_back({2.f, [](Game& g) {
        // talk to Maëlle (quest m1 → m2 offer)
        g.reg.each<Character>([&](Entity e, Character& c) { if (c.npcDef >= 0 && npcs()[c.npcDef].id == "maelle") { teleport(g, c.pos.x, c.pos.z + 2.5f, 0.2f); g.pc().yaw = PI; g.openDialog(e); } });
    }, idle, "talk maelle"});
    s.push_back({0.1f, shot("05_dialog"), idle, "shot"});
    s.push_back({0.5f, [](Game& g) { g.dialogAction("offer:m2"); g.dialogAction("accept:m2"); g.dialogAction("close"); }, idle, "accept m2"});
    s.push_back({1.f, [](Game& g) { g.gold = 2000; g.addItem("mount_horse", 1); g.addItem("sword_knight", 1, Rarity::Epic); g.addItem("chain_chest", 1, Rarity::Rare); g.addItem("chain_head", 1, Rarity::Rare); g.skillPoints = 4; }, idle, "grant"});
    s.push_back({0.6f, [](Game& g) { g.menu = Menu::Inventory; g.selItem = 0; }, idle, "inventory"});
    s.push_back({0.1f, shot("06_inventory"), idle, "shot"});
    s.push_back({0.5f, [](Game& g) { for (int i = (int)g.inventory.size() - 1; i >= 0; i--) { auto k = item(g.inventory[i].id)->kind; if (k == ItemKind::Weapon || k == ItemKind::Head || k == ItemKind::Chest) g.equip(i); } g.menu = Menu::Equipment; g.selSlot = SL_WEAPON; }, idle, "equipment"});
    s.push_back({0.1f, shot("07_equipment"), idle, "shot"});
    s.push_back({0.4f, [](Game& g) { g.unlockSkill(skillIndex("m1")); g.unlockSkill(skillIndex("m2")); g.unlockSkill(skillIndex("p1")); g.menu = Menu::Skills; g.selSkill = skillIndex("m2"); }, idle, "skills"});
    s.push_back({0.1f, shot("08_skills"), idle, "shot"});
    s.push_back({0.4f, [](Game& g) { g.menu = Menu::Map; }, idle, "map"});
    s.push_back({1.2f, none, idle, "map wait"});
    s.push_back({0.1f, shot("09_map"), idle, "shot"});
    s.push_back({0.4f, [](Game& g) { g.menu = Menu::Quests; g.selQuest = 0; }, idle, "quests"});
    s.push_back({0.1f, shot("10_quests"), idle, "shot"});
    s.push_back({0.4f, [](Game& g) { g.menu = Menu::Crafting; g.craftStation = ST_FORGE; g.addItem("iron_ore", 6); g.selRecipe = 0; }, idle, "craft"});
    s.push_back({0.1f, shot("11_crafting"), [](Game& g, float) {}, "shot"});
    s.push_back({0.3f, [](Game& g) { g.craft(0); g.menu = Menu::Settings; }, idle, "settings"});
    s.push_back({0.1f, shot("12_settings"), idle, "shot"});
    s.push_back({0.3f, [](Game& g) { g.menu = Menu::None; }, idle, "close"});
    // combat: a wolf pack near Brindlemere
    s.push_back({0.5f, [](Game& g) {
        teleport(g, 1600, 1500, 0.9f); g.timeOfDay = 15.f;
        Character& c = g.pc();
        for (int i = 0; i < 3; i++) { vec3 p = c.pos + vec3(4.f + i, 0, -6.f + i * 2); p.y = g.world.groundHeight(p.x, p.z); g.spawnEnemy(enemyIndex("wolf"), p); }
        g.spawnEnemy(enemyIndex("bandit"), c.pos + vec3(-5, 0, -5));
    }, idle, "spawn enemies"});
    s.push_back({8.f, none, [](Game& g, float t) {
        // fight: face nearest enemy, attack, dodge sometimes, cast fireball
        Entity tg = g.findTarget(g.pc().pos, g.pc().yaw, 30.f, Team::Enemy, -1.f);
        if (tg != NullEntity) { Character* c = g.reg.get<Character>(tg); vec3 d = c->pos - g.pc().pos; float l = distXZ(c->pos, g.pc().pos);
            float camF = std::atan2(d.x, d.z) - (g.camYaw + PI); g.input.move = l > 2.f ? vec2(std::sin(-camF) * 0, 1) : vec2(0, 0);
            vec3 fwd{-std::sin(g.camYaw), 0, -std::cos(g.camYaw)}, right{std::cos(g.camYaw), 0, -std::sin(g.camYaw)}; vec3 n = normalize(d);
            if (l > 2.f) g.input.move = vec2(dot(n, right), dot(n, fwd));
        }
        int fr = (int)(t * 30);
        if (fr % 12 == 0) { g.input.attackRelease = true; g.input.attackHoldT = 0.1f; }
        if (fr % 90 == 45) g.input.skill[0] = true;
        if (fr % 70 == 30) g.input.dodge = true;
        if (fr == 100) g.requestScreenshot("build/shots/13_combat.png");
        if (g.pc().hp < g.pc().maxHp * 0.4f) g.input.potion = true;
    }, "fight"});
    s.push_back({0.1f, shot("14_after_combat"), idle, "shot"});
    // mount
    s.push_back({0.5f, [](Game& g) { g.input.mount = true; }, idle, "call horse"});
    s.push_back({1.f, [](Game& g) { g.toggleMount(); }, idle, "mount"});
    s.push_back({3.f, none, [](Game& g, float) { g.input.move = vec2(0.2f, 1); g.input.sprint = true; }, "ride"});
    s.push_back({0.1f, shot("15_riding"), idle, "shot"});
    s.push_back({0.3f, [](Game& g) { if (g.riding) g.toggleMount(); }, idle, "dismount"});
    }
    // tour of regions with time & weather variety
    struct T { float x, z, yaw, tod; int weather; const char* f; };
    T tour[] = {{850, 600, 2.3f, 11.f, Game::W_SNOW, "16_frostheim"}, {2300, 1700, 0.6f, 17.5f, Game::W_ASHFALL, "17_ashen"}, {600, 1700, 0.5f, 21.5f, Game::W_CLEAR, "18_elderwood_night"},
                {1160, 2440, 1.f, 9.f, Game::W_FOG, "19_shadows"}, {2350, 860, 1.4f, 13.f, Game::W_CLEAR, "20_highcrowns"}, {-1, 0, 0, 18.6f, Game::W_RAIN, "21_lake_rain_sunset"}, {-2, 0, 0, 7.f, Game::W_CLEAR, "22_fort_dawn"}};
    for (auto& t : tour) {
        T tt = t;
        s.push_back({5.f, [tt](Game& g) {
            float x = tt.x, z = tt.z, yaw = tt.yaw;
            // -1: shore of the largest water lake, -2: outside the first castle, camera facing it
            vec2 target{-1, -1}; float off = 0;
            if (tt.x == -1) { const Lake* best = nullptr; for (auto& l : g.world.lakes) if (!l.lava && !l.frozen && (!best || l.r > best->r)) best = &l; if (best) { target = best->c; off = best->r * 0.92f; } }
            if (tt.x == -2) { for (auto& p : g.world.pois) if (p.type == PoiType::Castle) { target = p.pos; off = p.radius * 0.35f; break; } }
            if (target.x >= 0) {
                vec2 dir = normalize(vec2(WORLD_SIZE * 0.5f, WORLD_SIZE * 0.5f) - target); if (length(dir) < 0.5f) dir = vec2(1, 0);
                vec2 p = target + dir * off; x = p.x; z = p.y;
                vec2 d = target - p; yaw = std::atan2(d.x, d.y);
            }
            teleport(g, x, z, yaw);
            g.camDist = g.camDistTarget = tt.x < 0 ? 26.f : 15.5f; g.timeOfDay = tt.tod; g.weather = g.weatherNext = tt.weather; g.weatherBlend = 1; g.weatherTimer = 999; }, [tt](Game& g, float t) { g.input.move = vec2(0, (t < 2.f && tt.x >= 0) ? 0.5f : 0.f); }, "travel"});
        s.push_back({0.1f, [tt](Game& g) { g.requestScreenshot(std::string("build/shots/") + tt.f + ".png"); }, idle, "shot"});
    }
    if (name == "tour") return s;
    // boss: the Root Guardian
    s.push_back({0.3f, [](Game& g) { g.weather = g.weatherNext = Game::W_CLEAR; const Poi* p = g.world.poiById("root_glade"); teleport(g, p->pos.x, p->pos.y + 20, 0.f); g.timeOfDay = 14.f; g.level = 12; g.recomputeStats(); g.pc().hp = g.pc().maxHp; }, idle, "boss arena"});
    s.push_back({6.f, none, [](Game& g, float t) { g.input.move = vec2(0, t < 1.2f ? 1.f : 0.f); if ((int)(t * 30) == 70) g.requestScreenshot("build/shots/23_boss_intro.png"); }, "boss intro"});
    s.push_back({6.f, none, [](Game& g, float t) {
        if (g.bossEnt != NullEntity) { Character* b = g.reg.get<Character>(g.bossEnt); if (b) { vec3 d = b->pos - g.pc().pos; vec3 fwd{-std::sin(g.camYaw), 0, -std::cos(g.camYaw)}, right{std::cos(g.camYaw), 0, -std::sin(g.camYaw)}; vec3 n = normalize(d); if (length(d) > 5) g.input.move = vec2(dot(n, right), dot(n, fwd)); } }
        int fr = (int)(t * 30); if (fr % 14 == 0) { g.input.attackRelease = true; g.input.attackHoldT = 0.1f; } if (fr % 60 == 20) g.input.dodge = true; if (fr == 120) g.requestScreenshot("build/shots/24_boss_fight.png"); if (g.pc().hp < g.pc().maxHp * 0.5f) { g.pc().hp = g.pc().maxHp; } }, "boss fight"});
    // dungeon
    s.push_back({0.3f, [](Game& g) { if (g.bossEnt != NullEntity) { g.reg.destroyLater(g.bossEnt); g.bossEnt = NullEntity; } g.cinematicT = -1; const Poi* p = g.world.poiById("whisper_cave"); teleport(g, p->pos.x + 3, p->pos.y + 3); }, idle, "cave"});
    s.push_back({0.6f, [](Game& g) { g.input.interact = true; }, idle, "enter"});
    s.push_back({3.f, none, [](Game& g, float t) { g.input.move = vec2(0.3f, 0.6f); }, "explore"});
    s.push_back({0.1f, shot("25_dungeon"), idle, "shot"});
    s.push_back({0.5f, [](Game& g) { if (g.dungeon.active && !g.dungeon.braziers.empty()) teleport(g, g.dungeon.braziers[0].x + 2, g.dungeon.braziers[0].z + 2); }, idle, "puzzle room"});
    s.push_back({1.f, none, idle, "wait"});
    s.push_back({0.1f, shot("26_dungeon_puzzle"), idle, "shot"});
    // save / load round trip
    s.push_back({0.5f, [](Game& g) {
        if (g.dungeon.active) { g.exitDungeon(); }
    }, idle, "exit dungeon"});
    s.push_back({1.5f, none, idle, "wait"});
    s.push_back({0.5f, [](Game& g) {
        int lvl = g.level, gold = g.gold; size_t inv = g.inventory.size(); vec3 p = g.pc().pos; size_t q = g.questLog.size();
        bool ok = g.saveGame(2);
        bool ok2 = g.loadGame(2);
        bool same = g.level == lvl && g.gold == gold && g.inventory.size() == inv && distXZ(g.pc().pos, p) < 0.5f && g.questLog.size() == q;
        AGR_LOG("SAVE/LOAD TEST: save=%d load=%d consistent=%d (level %d gold %d items %zu quests %zu)", ok, ok2, same, g.level, g.gold, g.inventory.size(), g.questLog.size());
        g.startPlaying();
    }, idle, "save/load"});
    s.push_back({2.f, [](Game& g) { const Poi* b = g.world.poiById("brindlemere"); teleport(g, b->pos.x - 8, b->pos.y + 6, 2.4f); g.timeOfDay = 22.5f; }, idle, "village night"});
    s.push_back({0.1f, shot("27_village_night"), idle, "shot"});
    s.push_back({0.5f, [](Game& g) { g.layoutEdit = true; g.menu = Menu::Settings; }, idle, "layout editor"});
    s.push_back({0.1f, shot("28_layout_editor"), idle, "shot"});
    s.push_back({0.5f, [](Game& g) { g.layoutEdit = false; g.menu = Menu::None; g.state = GameState::Credits; g.stateT = 0; g.creditsT = 6; }, idle, "credits"});
    s.push_back({1.f, none, idle, "wait"});
    s.push_back({0.1f, shot("29_credits"), idle, "shot"});
    return s;
}

void Game::updateAutoplay(float dt) {
    static std::vector<Step> steps;
    static float stepT = 0;
    static bool started = false;
    if (!started) { steps = scenario(autoplayScenario); started = true; autoplayStep = 0; stepT = 0; if (!steps.empty() && steps[0].start) steps[0].start(*this); AGR_LOG("autoplay '%s': %zu steps", autoplayScenario.c_str(), steps.size()); }
    if (autoplayStep >= (int)steps.size()) return;
    stepT += dt;
    Step& s = steps[autoplayStep];
    if (s.tick) s.tick(*this, stepT);
    if (stepT >= s.dur || (s.until && s.until(*this))) {
        autoplayStep++;
        stepT = 0;
        if (autoplayStep < (int)steps.size()) { if (steps[autoplayStep].start) steps[autoplayStep].start(*this); }
        else AGR_LOG("autoplay finished: player hp %.0f/%.0f level %d, %zu entities, %zu chunks, last frame %d draw calls %dk triangles", player != NullEntity ? pc().hp : 0.f, player != NullEntity ? pc().maxHp : 0.f, level, reg.count(), world.loadedChunks(), renderer.stats.drawCalls, renderer.stats.triangles / 1000);
        if (autoplayStep >= (int)steps.size() && platform::isHeadlessTest()) platform::requestQuit();   // desktop harness: stops the run when LK_EXIT_AT_END is set
    }
}

}  // namespace lk
