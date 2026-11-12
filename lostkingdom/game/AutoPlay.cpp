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
    if (name == "look") {
        // fixed beauty shots for visual work (same places every run)
        struct V { const char* poi; float dx, dz, yaw, tod, dist; const char* f; };
        static const V views[] = {{"brindlemere", 10, 26, 0.5f, 10.f, 15.5f, "50_village"}, {"brindlemere", 40, -60, 2.4f, 16.5f, 20.f, "51_meadow"},
                                  {"brindlemere", -70, 30, -0.8f, 7.5f, 15.5f, "52_forest_morning"}, {"root_glade", 0, 30, 0.f, 13.f, 18.f, "53_elderwood"},
                                  {"#river", 0, 0, 0.9f, 15.f, 15.5f, "54_river"}, {"brindlemere", -4, 6, 2.6f, 17.5f, 15.5f, "55_village_street"}};
        for (const V& v : views) {
            s.push_back({4.f, [v](Game& g) { vec2 at;
                if (std::string(v.poi) == "#river") { if (g.world.rivers.empty()) return; auto& pts = g.world.rivers[0].pts; vec2 a = pts[pts.size() / 2], b = pts[pts.size() / 2 + 1]; vec2 n = normalize(perp(b - a)); at = a + n * 9.f; }
                else { const Poi* p = g.world.poiById(v.poi); if (!p) return; at = p->pos + vec2(v.dx, v.dz); }
                teleport(g, at.x, at.y, v.yaw); g.timeOfDay = v.tod; g.weather = g.weatherNext = Game::W_CLEAR; g.weatherBlend = 1; g.weatherTimer = 999; g.camDist = g.camDistTarget = v.dist; g.toastHint.clear(); }, idle, "view"});
            s.push_back({0.1f, [v](Game& g) { g.requestScreenshot(std::string("build/shots/") + v.f + ".png"); }, idle, "shot"});
        }
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
