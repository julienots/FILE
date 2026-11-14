// Automated play-test (desktop harness): drives the real game through a scripted session,
// validates core systems and captures screenshots. Enabled with --autoplay=<scenario>.
#include "Game.h"
#include "../engine/core/Platform.h"
#include <functional>

namespace lk {

struct Step { float dur; std::function<void(Game&)> start; std::function<void(Game&, float)> tick; const char* name; };

Rect ctlRect(const Game& g, int id);

// Inject a real touch tap (down + up in the SAME frame, like a fast tap at 30 FPS) at a UI position.
static void tapAt(Game& g, vec2 uiPos, int id, bool release = true) {
    float k = g.ui.scale;
    g.touch(TouchEvent{TouchEvent::Down, id, uiPos.x * k, uiPos.y * k});
    if (release) g.touch(TouchEvent{TouchEvent::Up, id, uiPos.x * k, uiPos.y * k});
}
static vec2 ctlCentre(Game& g, int id) { Rect r = ctlRect(g, id); return {r.x + r.w / 2, r.y + r.h / 2}; }

static void teleport(Game& g, float x, float z, float yaw = 0.8f) {
    Character& c = g.pc();
    c.pos = vec3(x, g.world.groundHeight(x, z), z); c.vel = vec3(0); g.camFocus = c.pos; g.camYaw = yaw;
    bool lava = false, frozen = false; float wl = g.world.waterLevel(x, z, &lava, &frozen);
    AGR_LOG("autoplay teleport (%.0f, %.0f): region %s, ground %.1f, water %.1f%s%s", x, z, regionName(g.world.regionId(x, z)), c.pos.y, wl, frozen ? " (frozen)" : "", lava ? " (lava)" : "");
}

static std::vector<Step> scenario(const std::string& name) {
    std::vector<Step> s;
    auto shot = [](const char* f) { return [f](Game& g) { g.requestScreenshot(std::string("build/shots/") + f + ".png"); }; };
    auto none = [](Game&) {};
    auto idle = [](Game&, float) {};
    if (name == "splash" || name == "full") {
        s.push_back({2.5f, none, idle, "splash"});
        s.push_back({0.1f, shot("01_splash"), idle, "shot"});
        s.push_back({5.f, none, idle, "title"});
        s.push_back({0.1f, shot("02_title"), idle, "shot"});
    }
    s.push_back({0.5f, [](Game& g) { g.newGame(); g.startPlaying(); }, idle, "new game"});
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
        // toggle the mod off with a held tap on its switch (the only switch on this page)
        s.push_back({0.25f, [](Game& g) {
            for (auto& w : g.ui.lastWidgets) if (w.second.empty()) { float k = g.ui.scale; g.touch(TouchEvent{TouchEvent::Down, 610, (w.first.x + w.first.w / 2) * k, (w.first.y + w.first.h / 2) * k}); break; }
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
        s.push_back({0.1f, [](Game& g) { lastShot = -1; for (int& r : res) r = 0; res[0] = g.cine.active && g.cine.id == "prologue"; }, idle, "prologue started"});
        s.push_back({76.f, none, [](Game& g, float) {
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
    if (name == "hero") {
        // close-up checks of the hero model: idle, walk, run, attack combo, roll, equipment screen
        s.push_back({3.f, [](Game& g) { const Poi* p = g.world.poiById("brindlemere"); teleport(g, p->pos.x + 30, p->pos.y + 30, 0.6f); g.timeOfDay = 10.5f; g.weather = g.weatherNext = Game::W_CLEAR; g.weatherBlend = 1; g.weatherTimer = 999; g.camDist = g.camDistTarget = 7.f; g.toastHint.clear(); g.addItem("sword_knight", 1, Rarity::Epic); }, idle, "setup"});
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
        for (int i = 0; i < 6; i++) {
            s.push_back({0.2f, [](Game& g) { if (!g.dialog.open || g.dialog.line + 1 >= (int)g.dialog.lines.size()) return; float k = g.ui.scale; g.touch(TouchEvent{TouchEvent::Down, 480, g.ui.W * 0.4f * k, (g.ui.H - 150) * k}); }, idle, "hold box"});
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
        tapWidget("tap tab", [](Game&) { return std::string("Quêtes"); }, 0.2f, [](Game& g) { res[3] = g.menu == Menu::Quests; AGR_LOG("UI journal tab: menu %d (quests %d)", (int)g.menu, (int)Menu::Quests); g.requestScreenshot("build/shots/97_ui_quests.png"); });
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
        // axis lock (1.6.1): a horizontal drag with vertical jitter turns the camera without tilting it
        s.push_back({0.8f, [camPt](Game& g) { v0 = g.camPitch; yawStart = g.camYaw; vec2 p = camPt(g); float k = g.ui.scale; g.touch(TouchEvent{TouchEvent::Down, 309, p.x * k, p.y * k}); },
                     [camPt, drag](Game& g, float t) { vec2 p = camPt(g); drag(g, 309, p, p + vec2(-260, 45 * std::sin(t * 20.f)) + vec2(0, 40), t, 0.6f); }, "drag sideways"});
        s.push_back({0.3f, [camPt](Game& g) { vec2 p = camPt(g) + vec2(-260, 40); float k = g.ui.scale; g.touch(TouchEvent{TouchEvent::Up, 309, p.x * k, p.y * k}); res[11] = std::fabs(g.camPitch - v0) < 1e-3f && std::fabs(wrapAngle(g.camYaw - yawStart)) > 0.5f; AGR_LOG("CAMERA axis lock: pitch %.3f -> %.3f, yaw turned %.2f", v0, g.camPitch, wrapAngle(g.camYaw - yawStart)); }, idle, "release"});
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
            AGR_LOG("CAMERA TEST: grid=%d dragUp=%d invertedAxis=%d pinchOut=%d pinchIn=%d doubleTapReset=%d autoReturn=%d lookAhead=%d lockOn=%d collision=%d dungeon=%d axisLock=%d tightFollow=%d",
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
        s.push_back({0.1f, [](Game& g) {
            AGR_LOG("TOUCH TEST: attack=%d dodge=%d jump=%d blockIdle=%d holdCharge=%d releaseAttack=%d bufferedCombo=%d slideIgnored=%d", res[0], res[1], res[2], !res[3], res[4], res[5], res[6], res[7] == 0);
            bool ok = res[0] && res[1] && res[2] && !res[3] && res[4] && res[5] && res[6] && res[7] == 0;
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
    if (stepT >= s.dur) {
        autoplayStep++;
        stepT = 0;
        if (autoplayStep < (int)steps.size()) { if (steps[autoplayStep].start) steps[autoplayStep].start(*this); }
        else AGR_LOG("autoplay finished: player hp %.0f/%.0f level %d, %zu entities, %zu chunks, last frame %d draw calls %dk triangles", player != NullEntity ? pc().hp : 0.f, player != NullEntity ? pc().maxHp : 0.f, level, reg.count(), world.loadedChunks(), renderer.stats.drawCalls, renderer.stats.triangles / 1000);
    }
}

}  // namespace lk
