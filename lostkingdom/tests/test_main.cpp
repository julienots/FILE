// LOST KINGDOM — unit tests (desktop only, built as lk_tests when tests/test_*.cpp exist).
// Pure CPU checks: ECS, maths, data integrity (content cross-references), world generation
// determinism, chunk generation, audio asset decoding. Run: ./build/desk/lk_tests [assetsDir]
#include "../engine/core/ECS.h"
#include "../engine/core/Math.h"
#include "../engine/audio/Audio.h"
#include "../engine/gfx/Model.h"
#include "../engine/gfx/Renderer.h"
#include "../game/Data.h"
#include "../game/World.h"
#include "../game/Props.h"
#include "../game/Game.h"
#include <cstdio>
#include <cmath>
#include <set>
#include <string>
#include <vector>
#include <fstream>
#include <iterator>

using namespace agr;
using namespace lk;

static int gFail = 0, gPass = 0;
#define CHECK(cond, ...) do { if (cond) gPass++; else { gFail++; std::printf("  FAIL %s:%d: %s  ", __FILE__, __LINE__, #cond); std::printf(__VA_ARGS__); std::printf("\n"); } } while (0)

static void testECS() {
    std::printf("[ECS]\n");
    Registry r;
    struct A { int v; }; struct B { float f; };
    std::vector<Entity> es;
    for (int i = 0; i < 1000; i++) { Entity e = r.create(); r.add<A>(e, {i}); if (i % 2) r.add<B>(e, {(float)i}); es.push_back(e); }
    CHECK(r.count() == 1000, "count");
    int both = 0; r.each<A, B>([&](Entity, A& a, B& b) { both++; CHECK((float)a.v == b.f, "pair mismatch"); });
    CHECK(both == 500, "each<A,B> = %d", both);
    // references must stay valid when adding during iteration (deque storage)
    A* first = r.get<A>(es[0]);
    for (int i = 0; i < 5000; i++) { Entity e = r.create(); r.add<A>(e, {-1}); }
    CHECK(r.get<A>(es[0]) == first && first->v == 0, "reference stability");
    // generation counter detects stale handles
    r.destroy(es[10]);
    CHECK(!r.valid(es[10]), "destroyed entity still valid");
    Entity re = r.create();
    CHECK(entityIndex(re) == entityIndex(es[10]) && re != es[10], "index reuse with new generation");
    CHECK(!r.has<A>(re), "recycled entity inherited component");
    // swap-remove keeps sparse map consistent
    for (int i = 0; i < 1000; i += 3) r.destroy(es[i]);
    int bad = 0; r.each<A>([&](Entity e, A& a) { if (a.v >= 0 && a.v < 1000 && es[a.v] != e) bad++; });
    CHECK(bad == 0, "sparse/dense mismatch %d", bad);
}

static void testMath() {
    std::printf("[Math]\n");
    CHECK(std::fabs(wrapAngle(3 * PI) - PI) < 1e-4f || std::fabs(wrapAngle(3 * PI) + PI) < 1e-4f, "wrapAngle");
    CHECK(std::fabs(dampAngle(PI - 0.1f, -PI + 0.1f, 1000.f, 1.f) - (PI + 0.1f)) < 1e-3f, "dampAngle takes the short way");
    vec3 n = normalize(vec3(3, 0, 4)); CHECK(std::fabs(length(n) - 1) < 1e-5f, "normalize");
    CHECK(length(normalize(vec3(0, 0, 0))) > 0.5f, "normalize(0) is finite");
    vec2 r = rotate(vec2(1, 0), PI / 2); CHECK(std::fabs(r.x) < 1e-5f && std::fabs(r.y - 1) < 1e-5f, "rotate");
    CHECK(smoothstepf(0, 1, 0.5f) == 0.5f && smoothstepf(0, 1, -1) == 0 && smoothstepf(0, 1, 2) == 1, "smoothstep");
}

static void testData() {
    std::printf("[Data]\n");
    std::set<std::string> itemIds, npcIds, enemyIds, bossIds, dungeonIds;
    for (auto& it : allItems()) { CHECK(!itemIds.count(it.id), "duplicate item %s", it.id.c_str()); itemIds.insert(it.id); }
    for (auto& n : npcs()) { CHECK(!npcIds.count(n.id), "duplicate npc %s", n.id.c_str()); npcIds.insert(n.id); }
    int regular = 0;
    for (auto& e : enemies()) { enemyIds.insert(e.id); if (e.behavior != Behavior::Boss) regular++; CHECK(!e.attacks.empty(), "enemy %s has no attack", e.id.c_str()); }
    for (auto& b : bosses()) bossIds.insert(b.id);
    for (auto& d : dungeons()) dungeonIds.insert(d.id);
    std::printf("  items %zu, enemies %d (+%zu boss bodies), bosses %zu, npcs %zu, quests %zu, recipes %zu, spells %zu, skills %zu, mounts %zu, dungeons %zu, sets %zu\n",
                allItems().size(), regular, enemies().size() - regular, bosses().size(), npcs().size(), quests().size(), recipes().size(), spells().size(), skillTree().size(), mounts().size(), dungeons().size(), sets().size());
    CHECK(regular >= 25, "at least 25 regular enemy types (%d)", regular);
    CHECK(bosses().size() >= 8, "at least 8 bosses");
    // all rarities exist
    std::set<int> rar; for (auto& it : allItems()) rar.insert((int)it.rarity);
    CHECK(rar.size() == (size_t)Rarity::Count, "every rarity used (%zu)", rar.size());
    // bosses: valid enemy body, multi-phase, rewards exist
    for (auto& b : bosses()) {
        CHECK(b.enemy >= 0 && b.enemy < (int)enemies().size(), "boss %s body", b.id.c_str());
        CHECK(!b.phases.empty(), "boss %s has no phase", b.id.c_str());
        CHECK(!b.intro.empty(), "boss %s has no intro", b.id.c_str());
        for (auto& r : b.rewards) CHECK(itemIds.count(r), "boss %s reward %s", b.id.c_str(), r.c_str());
        if (!b.shard.empty()) CHECK(itemIds.count(b.shard), "boss %s shard %s", b.id.c_str(), b.shard.c_str());
    }
    for (auto& e : enemies()) for (auto& l : e.loot) CHECK(itemIds.count(l.first), "enemy %s loot %s", e.id.c_str(), l.first.c_str());
    for (auto& n : npcs()) for (auto& s : n.shop) CHECK(itemIds.count(s), "npc %s sells unknown %s", n.id.c_str(), s.c_str());
    for (auto& r : recipes()) {
        CHECK(itemIds.count(r.result), "recipe result %s", r.result.c_str());
        for (auto& in : r.inputs) CHECK(itemIds.count(in.first), "recipe %s input %s", r.result.c_str(), in.first.c_str());
    }
    for (auto& it : allItems()) if (it.set >= 0) CHECK(it.set < (int)sets().size(), "item %s set index", it.id.c_str());
    // quests: givers, step targets, rewards, prerequisites
    std::set<std::string> questIds; for (auto& q : quests()) questIds.insert(q.id);
    std::set<int> types;
    static const std::set<std::string> harvestTargets = {"wood", "stone", "ore", "herb", "crystal", "fish", "hunt"};
    for (auto& q : quests()) {
        types.insert((int)q.type);
        if (!q.giver.empty()) CHECK(npcIds.count(q.giver), "quest %s giver %s", q.id.c_str(), q.giver.c_str());
        if (!q.requires_.empty()) CHECK(questIds.count(q.requires_), "quest %s requires %s", q.id.c_str(), q.requires_.c_str());
        for (auto& r : q.rewards) CHECK(itemIds.count(r), "quest %s reward %s", q.id.c_str(), r.c_str());
        CHECK(!q.steps.empty(), "quest %s has no step", q.id.c_str());
        for (auto& s : q.steps) {
            switch (s.kind) {
            case StepKind::Talk: CHECK(npcIds.count(s.target), "quest %s talk %s", q.id.c_str(), s.target.c_str()); break;
            case StepKind::Deliver:  // item handed to the quest giver (or to an npc living at step.poi)
                CHECK(itemIds.count(s.target), "quest %s deliver item %s", q.id.c_str(), s.target.c_str());
                CHECK(!q.giver.empty() || !s.poi.empty(), "quest %s deliver has no recipient", q.id.c_str());
                break;
            case StepKind::Kill: CHECK(enemyIds.count(s.target), "quest %s kill %s", q.id.c_str(), s.target.c_str()); break;
            case StepKind::KillBoss: CHECK(bossIds.count(s.target), "quest %s boss %s", q.id.c_str(), s.target.c_str()); break;
            case StepKind::Collect: case StepKind::Craft: CHECK(itemIds.count(s.target), "quest %s item %s", q.id.c_str(), s.target.c_str()); break;
            case StepKind::Dungeon: CHECK(dungeonIds.count(s.target) || s.target == "graveyard", "quest %s dungeon %s", q.id.c_str(), s.target.c_str()); break;
            case StepKind::Harvest: CHECK(harvestTargets.count(s.target), "quest %s harvest %s", q.id.c_str(), s.target.c_str()); break;
            default: break;
            }
            CHECK(s.count >= 1, "quest %s count", q.id.c_str());
        }
    }
    CHECK(types.size() == 6, "all six quest types present (%zu)", types.size());
    for (auto& d : dungeons()) {
        for (auto& e : d.enemies) CHECK(enemyIds.count(e), "dungeon %s enemy %s", d.id.c_str(), e.c_str());
        if (d.boss >= 0) CHECK(d.boss < (int)bosses().size(), "dungeon %s boss", d.id.c_str());
        if (!d.reward.empty()) CHECK(itemIds.count(d.reward), "dungeon %s reward %s", d.id.c_str(), d.reward.c_str());
    }
    std::set<int> branches; for (auto& s : skillTree()) branches.insert(s.branch);
    CHECK(branches.size() == BR_COUNT, "six skill branches");
    std::set<int> elements; for (auto& s : spells()) elements.insert((int)s.school);
    CHECK(elements.size() >= 7, "at least 7 magic schools (%zu)", elements.size());
    return;
}

static void testWorld(World& w) {
    std::printf("[World]\n");
    World w2; w2.buildPlan();
    CHECK(w.pois.size() == w2.pois.size() && w.roads.size() == w2.roads.size(), "plan is deterministic");
    bool samePos = true; for (size_t i = 0; i < w.pois.size() && i < w2.pois.size(); i++) if (length(w.pois[i].pos - w2.pois[i].pos) > 1e-3f) samePos = false;
    CHECK(samePos, "POI positions deterministic");
    std::set<int> regions; int nan = 0;
    for (int z = 0; z < 64; z++) for (int x = 0; x < 64; x++) {
        float px = (x + 0.5f) * WORLD_SIZE / 64, pz = (z + 0.5f) * WORLD_SIZE / 64;
        regions.insert(w.regionId(px, pz));
        float h = w.height(px, pz); if (!std::isfinite(h)) nan++;
    }
    CHECK(regions.size() == 6, "six regions present (%zu)", regions.size());
    CHECK(nan == 0, "%d non-finite heights", nan);
    // every dungeon / boss arena referenced by data exists as a POI
    for (auto& d : dungeons()) CHECK(w.poiById(d.poi) != nullptr, "dungeon poi %s", d.poi.c_str());
    for (auto& b : bosses()) if (b.dungeon < 0) CHECK(w.poiById(b.arenaPoi) != nullptr, "boss arena %s", b.arenaPoi.c_str());
    for (auto& n : npcs()) CHECK(w.poiById(n.home) != nullptr, "npc %s home %s", n.id.c_str(), n.home.c_str());
    // POIs flattened: height at centre close to recorded plateau height
    int off = 0; for (auto& p : w.pois) if (p.type == PoiType::Village && std::fabs(w.height(p.pos.x, p.pos.y) - p.height) > 1.5f) off++;
    CHECK(off == 0, "%d villages not flattened", off);
    // chunk generation at all LODs produces geometry and colliders near a village
    const Poi* v = nullptr; for (auto& p : w.pois) if (p.type == PoiType::Village) { v = &p; break; }
    CHECK(v != nullptr, "a village exists");
    if (v) {
        ChunkKey k{(int)(v->pos.x / CHUNK), (int)(v->pos.y / CHUNK)};
        for (int lod = 0; lod < 3; lod++) {
            ChunkBuild b; w.generateChunkSync(k, lod, b);
            CHECK(!b.terrain.idx.empty(), "lod %d terrain empty", lod);
            if (lod == 0) CHECK(!b.colliders.empty() || !b.props.idx.empty(), "village chunk has props/colliders");
        }
    }
}

static void testAudio(const std::string& assets) {
    std::printf("[Audio assets]\n");
    static const char* names[] = {"music/title", "music/greenhaven", "music/frostheim", "music/ashen", "music/elderwood", "music/shadows", "music/highcrowns", "music/village",
                                  "music/night", "music/combat", "music/boss", "music/dungeon", "music/credits", "music/studio", "sfx/swing", "sfx/hit_flesh", "sfx/parry",
                                  "sfx/fireball", "sfx/levelup", "amb/amb_forest_day", "amb/amb_wind", "amb/amb_cave"};
    for (const char* n : names) {
        std::ifstream f(assets + "/audio/" + n + ".ogg", std::ios::binary);
        std::vector<uint8_t> bytes((std::istreambuf_iterator<char>(f)), {});
        SoundBuffer sb;
        bool ok = !bytes.empty() && decodeOgg(bytes, sb);
        CHECK(ok && sb.frames() > 1000 && sb.rate == 48000, "decode %s", n);
    }
}


static void testMapOrientation() {
    std::printf("[Map orientation]\n");
    auto near2 = [](vec2 a, vec2 b) { return length(a - b) < 1e-4f; };
    for (float c : {0.f, 0.7f, 2.3f, -1.4f, 3.1f}) {
        vec2 fwd{-std::sin(c), -std::cos(c)}, right{std::cos(c), -std::sin(c)};   // camera basis in world x/z
        CHECK(near2(minimapProject(fwd, c), vec2(0, -1)), "camera forward must be up on the minimap (camYaw %.2f)", c);
        CHECK(near2(minimapProject(right, c), vec2(1, 0)), "camera right must be right on the minimap (camYaw %.2f)", c);
        // map image sampling (UI::imageCircle with angle -camYaw) must agree with the marker projection
        vec2 d{37.f, -12.f};
        CHECK(near2(rotate(minimapProject(d, c), -c), d), "map image and markers disagree (camYaw %.2f)", c);
        for (float y : {0.f, 1.f, -2.5f, 3.f}) {
            vec2 heading{std::sin(y), std::cos(y)};
            vec2 arrow = rotate(vec2(0, -1), minimapArrowAngle(y, c));    // the arrow sprite points up
            CHECK(near2(arrow, minimapProject(heading, c)), "arrow does not match heading (yaw %.2f camYaw %.2f)", y, c);
        }
    }
    // player walking straight "forward" with the camera behind: arrow points up
    float c = 1.1f; float yaw = std::atan2(-std::sin(c), -std::cos(c));
    CHECK(near2(rotate(vec2(0, -1), minimapArrowAngle(yaw, c)), vec2(0, -1)), "forward walk arrow not up");
}

static void testTouchLayout() {
    std::printf("[Touch layout]\n");
    const vec2* P = defaultLayoutPos();
    for (float aspect : {4.f / 3.f, 16.f / 9.f, 2.f, 20.f / 9.f, 21.f / 9.f}) {
        float H = 720.f, W = H * aspect;
        auto centre = [&](int i) { return vec2(W + P[i].x, H + P[i].y); };
        for (int i = 0; i < 10; i++) {
            vec2 c = centre(i); float r = ctlSize(i) * 0.5f;
            CHECK(c.x - r > 0 && c.x + r < W - 20 && c.y - r > 0 && c.y + r < H - 20, "button %d leaves the safe area (aspect %.2f)", i, aspect);
            // minimap {W-196, 80, 176, 176} and top-right shortcut buttons (y < 72) must stay free
            bool overMini = c.x + r > W - 196 && c.x - r < W - 20 && c.y - r < 80 + 176 && c.y + r > 80;
            CHECK(!overMini, "button %d overlaps the minimap (aspect %.2f)", i, aspect);
            CHECK(c.y - r > 72, "button %d overlaps the top bar", i);
            for (int j = i + 1; j < 10; j++) {
                float d = length(centre(i) - centre(j));
                // hit areas are circles of radius size/2 + 10: they must not intersect
                CHECK(d > ctlSize(i) * 0.5f + ctlSize(j) * 0.5f + 4.f, "buttons %d and %d overlap (distance %.0f)", i, j, d);
            }
        }
    }
}

static void testHeroModel(const std::string& assets) {
    std::printf("[Imported hero model]\n");
    auto readAll = [](const std::string& p) { std::ifstream f(p, std::ios::binary); return std::vector<uint8_t>((std::istreambuf_iterator<char>(f)), {}); };
    std::vector<uint8_t> mb = readAll(assets + "/models/hero.lkm"), tb = readAll(assets + "/models/hero.jpg");
    SkinnedModelData m;
    CHECK(!mb.empty() && m.loadLKM(mb), "hero.lkm loads");
    CHECK(!tb.empty() && m.loadTexture(tb) && m.texW >= 256 && m.texH >= 256, "hero texture decodes");
    CHECK((int)m.joints.size() == H_COUNT, "joint count %zu", m.joints.size());
    CHECK(m.height > 1.5f && m.height < 2.2f, "height %.2f", m.height);
    // rest pose sanity: feet near the ground, head on top, arms down beside the body
    float minY = 1e9f, maxY = -1e9f; for (auto& v : m.verts) { minY = std::min(minY, v.py); maxY = std::max(maxY, v.py); }
    CHECK(minY > -0.05f && minY < 0.08f && maxY > 1.5f, "vertical extent %.2f..%.2f", minY, maxY);
    CHECK(m.joints[H_HEAD].y > m.joints[H_CHEST].y && m.joints[H_CHEST].y > m.joints[H_ROOT].y, "spine order");
    CHECK(m.joints[H_HANDL].y < m.joints[H_UARML].y - 0.3f && m.joints[H_HANDR].y < m.joints[H_UARMR].y - 0.3f, "arms lowered into the rest pose");
    CHECK(m.joints[H_HANDL].x > 0 && m.joints[H_HANDR].x < 0, "left on +X");
    // regression (1.2.0 feedback "things under the models when they walk"): nothing near the feet may be
    // skinned to the pelvis / torso, otherwise those faces stretch between the hips and the ground
    int badFeet = 0, cloth = 0;
    for (auto& v : m.verts) {
        if (v.py < 0.3f && (v.b0 == H_ROOT || v.b0 == H_SPINE || v.b0 == H_CHEST || v.b0 == H_NECK || v.b0 == H_HEAD) && v.w0 > 128) badFeet++;
        if (v.ex == 255) cloth++;
    }
    CHECK(badFeet == 0, "%d foot-level vertices bound to the torso", badFeet);
    CHECK(cloth > 500, "cloth flags present (%d)", cloth);
    // corrupted / truncated files are rejected without crashing
    std::vector<uint8_t> cut(mb.begin(), mb.begin() + mb.size() / 2);
    SkinnedModelData bad;
    CHECK(!bad.loadLKM(cut), "truncated model rejected");
    std::vector<uint8_t> junk(64, 0xff); junk[0] = 'L'; junk[1] = 'K'; junk[2] = 'M'; junk[3] = '1';
    CHECK(!bad.loadLKM(junk), "garbage header rejected");
}


// ---- free camera (CameraRig::compute, pure function) -------------------------------------------------
// Projects a world point with the camera the game would build from the rig output (same fov, 16:9).
static vec3 rigProject(const CameraRigOut& o, vec3 p, float fovY = 38.f * DEG) {
    Camera c; c.pos = o.eye; c.target = o.look; c.fovY = fovY; c.zNear = 0.3f; c.zFar = 540.f; c.update(16.f / 9.f);
    vec4 h = c.viewProj * vec4(p, 1.f);
    if (h.w <= 1e-3f) return vec3(9, 9, -1);
    return vec3(h.x / h.w, h.y / h.w, h.w);
}
static bool onScreen(vec3 n, float m = 1.f) { return n.z > 0 && std::fabs(n.x) <= m && std::fabs(n.y) <= m; }

// Checks one rig configuration: eye above the terrain, hero (feet, chest, head) inside the image.
static void checkRig(const char* what, const CameraRigIn& in, int& bad) {
    CameraRigOut o = CameraRig::compute(in);
    bool finite = std::isfinite(o.eye.x) && std::isfinite(o.eye.y) && std::isfinite(o.eye.z) && std::isfinite(o.look.y);
    float gy = in.ground ? in.ground(o.eye.x, o.eye.z) : -1e9f;
    vec3 feet = in.focus - vec3(0, in.feetDrop, 0), head = feet + vec3(0, 1.8f, 0), chest = feet + vec3(0, 1.2f, 0);
    vec3 nf = rigProject(o, feet), nh = rigProject(o, head), nc = rigProject(o, chest);
    bool ok = finite && o.eye.y >= gy + o.minClear - 1e-3f && length(o.eye - o.look) > 0.5f && onScreen(nf) && onScreen(nh) && onScreen(nc);
    if (!ok) { bad++; if (bad < 8) std::printf("  rig %s pitch %.2f dist %.1f: eye.y %.2f ground %.2f clear %.2f feet(%.2f,%.2f) head(%.2f,%.2f)\n", what, in.pitch, in.dist, o.eye.y, gy, o.minClear, nf.x, nf.y, nh.x, nh.y); }
}

static void testCameraRig() {
    std::printf("[Free camera rig]\n");
    // factors and clamps
    CHECK(CameraRig::clampPitch(-2.f) == CameraRig::PITCH_MIN && CameraRig::clampPitch(3.f) == CameraRig::PITCH_MAX, "pitch clamp");
    CHECK(CameraRig::clampDist(1.f) == 4.f && CameraRig::clampDist(99.f) == 40.f, "zoom range 4..40");
    CHECK(CameraRig::lowFactor(0.66f) == 0.f && CameraRig::lowFactor(0.25f) == 0.f && CameraRig::lowFactor(-0.35f) == 1.f, "low factor");
    CHECK(CameraRig::mapFactor(15.5f) == 0.f && CameraRig::mapFactor(40.f) == 1.f, "map factor");
    CHECK(CameraRig::nearFactor(15.5f) == 0.f && CameraRig::nearFactor(4.f) == 1.f, "near factor");
    CHECK(CameraRig::minClearance(0.66f, 15.5f) == 3.f && std::fabs(CameraRig::minClearance(-0.35f, 9.f) - 0.6f) < 1e-4f && CameraRig::minClearance(-0.35f, 40.f) == 2.f, "clearance depends on pitch");
    // default view is unchanged from 1.x (same eye as the old formula on flat ground)
    CameraRigIn in; in.focus = vec3(100, 1.1f, 100); in.yaw = 0.78f; in.pitch = 0.66f; in.dist = 15.5f;
    in.ground = [](float, float) { return 0.f; };
    CameraRigOut o = CameraRig::compute(in);
    vec3 old = in.focus + vec3(std::sin(0.78f) * std::cos(0.66f), std::sin(0.66f), std::cos(0.78f) * std::cos(0.66f)) * 15.5f;
    CHECK(length(o.eye - old) < 1e-3f && std::fabs(o.look.y - (1.1f + 0.9f)) < 1e-4f, "default framing identical to 1.5");
    // out-of-range inputs are clamped
    in.pitch = -3.f; CameraRigOut lo = CameraRig::compute(in); in.pitch = CameraRig::PITCH_MIN; CameraRigOut lo2 = CameraRig::compute(in);
    CHECK(length(lo.eye - lo2.eye) < 1e-4f, "pitch below the minimum is clamped");
    // low camera: closer (x0.6), aim above the head, looking UP, eye 0.6 m above flat ground, hero in the lower half
    in.pitch = -0.35f; o = CameraRig::compute(in);
    CHECK(std::fabs(o.dist - 15.5f * 0.6f) < 1e-3f, "low camera pulled in (dist %.2f)", o.dist);
    CHECK(o.look.y > o.eye.y + 1.f, "low camera looks up at the sky (eye %.2f look %.2f)", o.eye.y, o.look.y);
    CHECK(std::fabs(o.eye.y - 0.6f) < 1e-3f, "eye kept 0.6 m above the ground (%.2f)", o.eye.y);
    vec3 nChest = rigProject(o, vec3(100, 1.2f, 100)), nHorizon = rigProject(o, o.eye + normalize(vec3(o.look.x - o.eye.x, 0, o.look.z - o.eye.z)) * 300.f);
    CHECK(nChest.y < -0.33f && nChest.y > -1.f, "hero framed in the lower third when looking up (chest ndc %.2f)", nChest.y);
    CHECK(nHorizon.y < -0.2f, "horizon low in the frame: sky fills the image (ndc %.2f)", nHorizon.y);
    // top view: looking almost straight down
    in.pitch = 1.3f; in.dist = 40.f; o = CameraRig::compute(in);
    CHECK(normalize(o.look - o.eye).y < -0.85f, "pitch 1.3 / zoom 40 looks down at the map (%.2f)", normalize(o.look - o.eye).y);
    CHECK(o.mapK == 1.f, "map view engaged at zoom 40");
    // zoom is monotonic
    float prev = 0; bool mono = true;
    for (float d = 4; d <= 40; d += 2) { in.dist = d; in.pitch = 0.66f; float e = CameraRig::compute(in).dist; if (e < prev) mono = false; prev = e; }
    CHECK(mono, "zoom distance monotonic");
    // building behind the hero: the camera is pulled in front of the wall
    in.pitch = 0.3f; in.dist = 15.5f; in.yaw = 0.f;   // camera on +Z of the hero
    in.blocked = [](vec3 p) { return p.z > 106.f && p.z < 110.f && p.y < 9.f; };
    o = CameraRig::compute(in);
    CHECK(o.collided && o.eye.z < 106.f && o.dist < 7.f, "camera stops in front of the wall (eye z %.2f, dist %.2f)", o.eye.z, o.dist);
    CHECK(o.nearK > 0.5f && onScreen(rigProject(o, vec3(100, 0.05f, 100)), 0.9f), "hero fully framed after the camera was pulled in");
    in.pitch = -0.35f; in.dist = 40.f; in.blocked = nullptr; o = CameraRig::compute(in);
    CHECK(o.dist <= 18.01f, "low sky view stays close enough to see the hero (%.1f m)", o.dist);
    // smoothing (1.6.1): a sudden bump under the camera lifts it progressively, never below the hard floor
    {
        CameraRigIn r; r.focus = vec3(100, 1.1f, 100); r.yaw = 0.f; r.pitch = 0.2f; r.dist = 15.5f; r.dt = 1.f / 30.f;
        r.ground = [](float, float z) { return z > 108.f ? 5.f : 0.f; };   // a 5 m step behind the hero
        CameraRigOut a = CameraRig::compute(r);
        CameraRigIn r0 = r; r0.dt = 0; CameraRigOut full = CameraRig::compute(r0);
        float gy = r.ground(a.eye.x, a.eye.z);
        CHECK(a.eye.y >= gy + CameraRig::HARD_CLEARANCE - 1e-3f, "hard floor kept while smoothing (%.2f over %.2f)", a.eye.y, gy);
        CHECK(a.lift < full.lift - 0.2f, "terrain lift is smoothed, not instant (%.2f vs %.2f)", a.lift, full.lift);
        for (int i = 0; i < 90; i++) { r.prevLift = a.lift; r.prevCollisionDist = a.collisionDist; a = CameraRig::compute(r); }
        CHECK(std::fabs(a.lift - full.lift) < 0.05f, "lift converges (%.2f vs %.2f)", a.lift, full.lift);
        // collision: snaps in at once, backs out slowly
        r.ground = [](float, float) { return 0.f; }; r.prevLift = 0; r.prevCollisionDist = 3.f;
        CameraRigOut b = CameraRig::compute(r);
        CHECK(b.dist < 4.f && b.dist > 3.f, "camera backs out of a wall slowly (%.2f)", b.dist);
        r.blocked = [](vec3 q) { return q.z > 103.f; }; r.prevCollisionDist = 15.f;
        b = CameraRig::compute(r);
        CHECK(b.dist < 3.f, "camera snaps in front of a wall at once (%.2f)", b.dist);
    }
    // terrain: every pitch x zoom on flat ground, a slope rising behind the camera and rough hills
    std::function<float(float, float)> grounds[3] = {[](float, float) { return 0.f; }, [](float x, float z) { return std::max(0.f, (z - 100.f) * 0.6f) + std::max(0.f, (x - 100.f) * 0.3f); },
                                                     [](float x, float z) { return 2.5f * std::sin(x * 0.21f) * std::cos(z * 0.17f) + 1.5f * std::sin(x * 0.05f + z * 0.08f); }};
    const char* gname[3] = {"flat", "slope", "hills"};
    int bad = 0, n = 0;
    for (int gi = 0; gi < 3; gi++)
        for (float pitch : {-0.35f, -0.2f, 0.f, 0.25f, 0.66f, 1.f, 1.3f, 1.35f})
            for (float dist : {4.f, 9.f, 15.5f, 24.f, 40.f})
                for (float yaw : {0.f, 1.3f, 2.9f, 4.4f}) {
                    CameraRigIn r; r.ground = grounds[gi]; r.focus = vec3(100, r.ground(100, 100) + 1.1f, 100);
                    r.yaw = yaw; r.pitch = pitch; r.dist = dist; checkRig(gname[gi], r, bad); n++;
                }
    CHECK(bad == 0, "%d of %d camera configurations put the eye under the ground or lose the hero", bad, n);
}

// The same checks on the real terrain of the world (mountains, valleys, shores) at many places.
static void testCameraOnWorld(World& w) {
    std::printf("[Free camera on the world terrain]\n");
    int bad = 0, n = 0;
    Rng rng(4242);
    for (int i = 0; i < 160; i++) {
        float x = rng.range(80.f, WORLD_SIZE - 80.f), z = rng.range(80.f, WORLD_SIZE - 80.f);
        for (float pitch : {-0.35f, 0.f, 0.66f, 1.3f})
            for (float dist : {4.f, 15.f, 40.f}) {
                CameraRigIn r; r.ground = [&w](float a, float b) { return w.height(a, b); };
                r.focus = vec3(x, w.height(x, z) + 1.1f, z); r.yaw = rng.range(0.f, TAU); r.pitch = pitch; r.dist = dist;
                checkRig("world", r, bad); n++;
            }
    }
    CHECK(bad == 0, "%d of %d world camera placements invalid", bad, n);
}

static void testGpuProfiles() {
    std::printf("[GPU profile suggestion]\n");
    struct C { const char* gpu; QualityProfile want; };
    C cases[] = {{"Adreno (TM) 740", QualityProfile::Ultra}, {"Adreno (TM) 650", QualityProfile::High}, {"Adreno (TM) 619", QualityProfile::Medium},
                 {"Adreno (TM) 506", QualityProfile::Low}, {"Adreno (TM) 308", QualityProfile::Battery}, {"Mali-G710 MC10", QualityProfile::High},
                 {"Mali-G610 MC6", QualityProfile::Medium}, {"Mali-G78 MP20", QualityProfile::Medium}, {"Mali-G57 MC2", QualityProfile::Low},
                 {"Mali-G52", QualityProfile::Low}, {"Mali-G31 MP2", QualityProfile::Battery}, {"Mali-T880", QualityProfile::Battery},
                 {"PowerVR Rogue GE8320", QualityProfile::Low}, {"Samsung Xclipse 920", QualityProfile::High}, {"Immortalis-G715", QualityProfile::High},
                 {"Unknown GPU", QualityProfile::Medium}};
    for (auto& c : cases) CHECK(suggestProfileForGpu(c.gpu) == c.want, "%s -> %d, expected %d", c.gpu, (int)suggestProfileForGpu(c.gpu), (int)c.want);
    // every profile keeps a sane configuration
    for (int p = 0; p <= 4; p++) {
        GraphicsSettings g = GraphicsSettings::forProfile((QualityProfile)p);
        CHECK(g.renderScale >= 0.5f && g.renderScale <= 1.f && g.fpsCap >= 30 && g.shadowInterval >= 1 && g.bloomStart >= 0, "profile %d settings", p);
    }
}

int lkRunTests(int argc, char** argv) {
    std::string assets = argc > 1 ? argv[1] : "assets";
    testECS();
    testMath();
    testMapOrientation();
    testGpuProfiles();
    testTouchLayout();
    testCameraRig();
    testData();
    initPropTemplates();
    World w; w.buildPlan();
    testWorld(w);
    testCameraOnWorld(w);
    testAudio(assets);
    testHeroModel(assets);
    std::printf("\n%d passed, %d failed\n", gPass, gFail);
    return gFail ? 1 : 0;
}
