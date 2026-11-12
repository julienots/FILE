// LOST KINGDOM — unit tests (desktop only, built as lk_tests when tests/test_*.cpp exist).
// Pure CPU checks: ECS, maths, data integrity (content cross-references), world generation
// determinism, chunk generation, audio asset decoding. Run: ./build/desk/lk_tests [assetsDir]
#include "../engine/core/ECS.h"
#include "../engine/core/Math.h"
#include "../engine/audio/Audio.h"
#include "../game/Data.h"
#include "../game/World.h"
#include "../game/Props.h"
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

int lkRunTests(int argc, char** argv) {
    std::string assets = argc > 1 ? argv[1] : "assets";
    testECS();
    testMath();
    testData();
    initPropTemplates();
    World w; w.buildPlan();
    testWorld(w);
    testAudio(assets);
    std::printf("\n%d passed, %d failed\n", gPass, gFail);
    return gFail ? 1 : 0;
}
