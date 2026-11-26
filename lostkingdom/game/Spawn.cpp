#include "Game.h"
#include "../engine/core/Platform.h"
#include <algorithm>

namespace lk {

// ============================================================================================ assets
Game::ImportedModel* Game::importedModel(const std::string& id, Arch arch) {
    auto it = importedModels.find(id);
    if (it != importedModels.end()) return it->second.get();
    std::unique_ptr<ImportedModel> m;
    std::vector<uint8_t> mb, tb;
    if (platform::readAsset("models/" + id + ".lkm", mb)) {
        m = std::make_unique<ImportedModel>();
        const Skeleton& base = skeleton(arch);   // 2.3.3 also quadrupeds (the imported horse)
        if (m->data.loadLKM(mb) && platform::readAsset("models/" + id + ".jpg", tb) && m->data.loadTexture(tb) && (int)m->data.joints.size() == base.size()) {
            // same bones as the game skeleton, offsets fitted to the model's proportions
            m->skel = base;
            for (int i = 0; i < m->skel.size(); i++) {
                Bone& b = m->skel.bones[i];
                b.offset = m->data.joints[i] - (b.parent >= 0 ? m->data.joints[b.parent] : vec3(0));
            }
            m->skel.finalize();
            AGR_LOG("model '%s': %zu vertices, %zu triangles, texture %dx%d", id.c_str(), m->data.verts.size(), m->data.idx.size() / 3, m->data.texW, m->data.texH);
        } else { AGR_WARN("model '%s' is invalid, using the procedural look", id.c_str()); m.reset(); }
    }
    ImportedModel* r = m.get();
    importedModels[id] = std::move(m);
    return r;
}

bool Game::applyImportedModel(Visual& v, const std::string& id, Arch arch) {
    ImportedModel* m = importedModel(id, arch);
    if (!m) return false;
    if (!m->mesh || !m->mesh->valid()) { m->mesh = std::make_shared<Mesh>(); m->mesh->upload(m->data.verts, m->data.idx); }
    if (!m->lodMesh || !m->lodMesh->valid()) {   // 2.3.6 far version for crowds (villagers, enemies seen from afar)
        vec3 lo(1e9f), hi(-1e9f);
        for (const Vertex& vv : m->data.verts) { lo = vmin(lo, vec3(vv.px, vv.py, vv.pz)); hi = vmax(hi, vec3(vv.px, vv.py, vv.pz)); }
        std::vector<Vertex> lv; std::vector<uint32_t> li;
        clusterDecimate(m->data.verts, m->data.idx, lo, hi, 22, lv, li);
        m->lodMesh = std::make_shared<Mesh>(); m->lodMesh->upload(lv, li);
    }
    if (!m->tex.id) m->tex.create2D(m->data.texW, m->data.texH, GL_RGBA8, GL_RGBA, GL_UNSIGNED_BYTE, m->data.rgba.data(), true, false);
    v.mesh = m->mesh; v.lodMesh = m->lodMesh; v.albedo = m->tex.id; v.anim.skeleton = &m->skel;
    v.cape = false;   // imported models carry their own cloth
    return true;
}

void Game::loadAssets() {
    importedModel("hero");   // preload (logs its size); the procedural hero is used when absent
    static const char* sounds[] = {"swing", "swing_heavy", "hit_flesh", "hit_metal", "hit_wood", "block", "parry", "step_grass", "step_stone", "step_snow", "step_wood", "step_water", "grass_rustle",
                                   "jump", "land", "roll", "bow", "arrow_hit", "fireball", "explosion", "ice", "lightning", "heal", "shadow", "arcane", "nature", "holy",
                                   "pickup", "coin", "chest", "levelup", "quest", "ui_click", "ui_open", "ui_close", "potion", "craft", "forge", "harvest_wood", "harvest_stone", "harvest_herb",
                                   "splash", "wolf_howl", "growl", "roar", "dragon_roar", "spider", "skeleton", "goblin", "troll", "golem", "ghost", "death", "player_hurt", "thunder", "birds",
                                   "door", "lever", "trap", "fish_bite", "horse", "whistle", "boss_intro", "discover", "fire_loop", "magic_burst", "engine",
                                   // 2.3.4 variants (a step never sounds twice the same) and the sand
                                   "step_grass2", "step_grass3", "step_stone2", "step_stone3", "step_snow2", "step_snow3", "step_wood2", "step_wood3",
                                   "step_water2", "step_water3", "step_sand", "step_sand2", "step_sand3", "horse2", "horse3",
                                   "bird_chirp", "bird_chirp2", "bird_chirp3", "gull", "eagle", "owl", "frog", "crow", "cuckoo", "bubbles", "chop", "pick_stone", "tree_fall", "leaves", "item_get", "voice", "hoof1", "hoof2", "hoof3", "horse_snort", "saddle_creak"};   // 3.7.1, 3.8.0, 3.8.1
    int loaded = 0;
    for (const char* s : sounds) {
        std::vector<uint8_t> b;
        if (platform::readAsset(std::string("audio/sfx/") + s + ".ogg", b) && audio.loadSound(s, b)) { sfx[s] = audio.id(s); loaded++; }
    }
    static const char* music[] = {"title", "greenhaven", "frostheim", "ashen", "elderwood", "shadows", "highcrowns", "village", "night", "combat", "boss", "dungeon", "credits", "studio", "sands", "coast", "realm", "steppe", "jungle"};
    for (const char* m : music) {
        std::vector<uint8_t> b;
        if (platform::readAsset(std::string("audio/music/") + m + ".ogg", b)) audio.addMusic(m, std::move(b));
    }
    static const char* amb[] = {"amb_forest_day", "amb_night", "amb_wind", "amb_rain", "amb_lava", "amb_swamp", "amb_cave", "amb_river", "amb_village", "amb_sea", "amb_meadow", "amb_underwater", "amb_desert", "amb_town"};   // 3.7.1 + meadow, underwater, desert; 3.9.0 + town
    for (const char* a : amb) {
        std::vector<uint8_t> b;
        if (platform::readAsset(std::string("audio/amb/") + a + ".ogg", b)) audio.addAmbience(a, std::move(b));
    }
    AGR_LOG("audio: %d sound effects loaded", loaded);
}

std::shared_ptr<Mesh> Game::cachedMesh(const std::string& key, const std::function<void(MeshBuilder&)>& build) {
    auto it = meshCache.find(key);
    if (it != meshCache.end()) return it->second;
    MeshBuilder mb;
    build(mb);
    auto m = std::make_shared<Mesh>();
    mb.upload(*m);
    meshCache[key] = m;
    return m;
}

std::shared_ptr<Mesh> Game::weaponMesh(WeaponType t, int tier, vec3 accent, float* tip) {
    char key[64];
    snprintf(key, sizeof key, "w%d_%d_%.2f_%.2f_%.2f", (int)t, tier, accent.x, accent.y, accent.z);
    float tp = 0.8f;
    auto m = cachedMesh(key, [&](MeshBuilder& mb) { buildWeapon(mb, t, tier, accent, &tp); });
    if (tip) { MeshBuilder tmp; buildWeapon(tmp, t, tier, accent, tip); }
    return m;
}

void Game::buildSharedMeshes() {
    MeshBuilder mb;
    buildWindmillBlades(mb);
    windmillBlades = std::make_shared<Mesh>(); mb.upload(*windmillBlades);
    auto mk = [&](std::function<void(MeshBuilder&)> f) { MeshBuilder b; f(b); auto m = std::make_shared<Mesh>(); b.upload(*m); return m; };
    Mat wood(hexColor(0x7a4a26), 0.8f), iron(hexColor(0x6a6a70), 0.35f, 1.f), gold(hexColor(0xd8b060), 0.3f, 1.f);
    auto chest = [&](MeshBuilder& b, bool open) {
        b.bevelBox({0, 0.32f, 0}, {0.55f, 0.32f, 0.38f}, 0.03f, wood);
        for (int s = -1; s <= 1; s += 2) b.box({s * 0.45f, 0.35f, 0}, {0.04f, 0.33f, 0.4f}, iron);
        quat lid = open ? quat::axisAngle({1, 0, 0}, -1.9f) : quat();
        vec3 hinge{0, 0.64f, -0.38f};
        b.xf = mat4::translate(hinge) * mat4::rotate(lid) * mat4::translate(-hinge);
        b.lathe({{0.38f, 0.0f}, {0.38f, 0.01f}}, wood, 10, {0, 0.64f, 0}, quat::axisAngle({0, 0, 1}, PI / 2), 1.f);
        b.box({0, 0.75f, 0}, {0.56f, 0.12f, 0.39f}, wood);
        b.box({0, 0.62f, 0.4f}, {0.07f, 0.08f, 0.02f}, gold);
        b.xf = mat4();
        if (open) b.ellipsoid({0, 0.6f, 0}, vec3(0.45f, 0.08f, 0.3f), Mat(hexColor(0xffd060), 0.2f, 1.f, 2.f), 10, 4);
    };
    chestMesh = mk([&](MeshBuilder& b) { chest(b, false); });
    chestOpenMesh = mk([&](MeshBuilder& b) { chest(b, true); });
    pickupMesh = mk([&](MeshBuilder& b) {
        b.lathe({{0.01f, 0}, {0.2f, 0.05f}, {0.24f, 0.2f}, {0.16f, 0.36f}, {0.07f, 0.42f}, {0.1f, 0.48f}, {0.01f, 0.5f}}, Mat(hexColor(0xa08060), 0.8f), 10);
        b.lathe({{0.075f, 0.4f}, {0.08f, 0.44f}}, Mat(hexColor(0xffffff), 0.3f, 0, 1.5f), 10);
    });
    arrowMesh = mk([&](MeshBuilder& b) {
        b.tube({0, 0, -0.45f}, {0, 0, 0.4f}, 0.012f, 0.012f, wood, 4, false);
        b.cone({0, 0, 0.4f}, {0, 0, 0.5f}, 0.03f, iron, 4);
        b.blade({0, 0, -0.45f}, {0, 0, -0.3f}, 0.06f, {1, 0, 0}, Mat(hexColor(0xe8e0d0)), Mat(hexColor(0xe8e0d0)));
    });
    boulderMesh = mk([&](MeshBuilder& b) { b.rock({0, 0, 0}, vec3(1.f), Mat(hexColor(0x7a7468), 0.9f), 77, 1); });
    orbMesh = mk([&](MeshBuilder& b) { b.ellipsoid({0, 0, 0}, vec3(1), Mat(vec3(1), 0.2f, 0, 3.f), 10, 6); });
    gateMesh = mk([&](MeshBuilder& b) {
        for (int i = -3; i <= 3; i++) b.box({i * 0.5f, 2.f, 0}, {0.06f, 2.f, 0.06f}, iron);
        for (int j = 0; j < 4; j++) b.box({0, 0.5f + j * 1.f, 0}, {1.7f, 0.05f, 0.05f}, iron);
    });
    brazierMesh = mk([&](MeshBuilder& b) {
        b.lathe({{0.25f, 0}, {0.12f, 0.6f}, {0.12f, 0.9f}, {0.4f, 1.1f}, {0.42f, 1.2f}, {0.01f, 1.05f}}, iron, 10);
        for (int i = 0; i < 4; i++) { float a = i * TAU / 4; b.box({std::cos(a) * 0.43f, 1.15f, std::sin(a) * 0.43f}, {0.04f, 0.08f, 0.04f}, Mat(hexColor(0x6ff0ff), 0.3f, 0, 1.5f)); }
    });
    bladeMesh = mk([&](MeshBuilder& b) {
        b.tube({0, 0, 0}, {0, -2.6f, 0}, 0.04f, 0.04f, iron, 5, false);
        b.flat = true;
        b.quadP({-0.9f, -2.5f, 0.02f}, {0.9f, -2.5f, 0.02f}, {0.6f, -2.9f, 0.02f}, {-0.6f, -2.9f, 0.02f}, Mat(hexColor(0xb8bcc4), 0.2f, 1.f));
        b.quadP({0.9f, -2.5f, -0.02f}, {-0.9f, -2.5f, -0.02f}, {-0.6f, -2.9f, -0.02f}, {0.6f, -2.9f, -0.02f}, Mat(hexColor(0xb8bcc4), 0.2f, 1.f));
    });
    spikeMesh = mk([&](MeshBuilder& b) {
        b.box({0, -0.02f, 0}, {0.95f, 0.04f, 0.95f}, Mat(hexColor(0x4a4440), 0.8f));
        for (int i = 0; i < 9; i++) b.cone({(i % 3 - 1) * 0.6f, 0, (i / 3 - 1) * 0.6f}, {(i % 3 - 1) * 0.6f, 0.9f, (i / 3 - 1) * 0.6f}, 0.1f, iron, 5);
    });
    leverMesh = mk([&](MeshBuilder& b) {
        b.bevelBox({0, 0.3f, 0}, {0.3f, 0.3f, 0.2f}, 0.04f, Mat(hexColor(0x6a6660)));
        b.tube({0, 0.5f, 0}, {0, 1.2f, 0.25f}, 0.04f, 0.03f, iron, 6, true);
        b.ellipsoid({0, 1.22f, 0.27f}, vec3(0.07f), Mat(hexColor(0x8e2a24), 0.5f), 8, 5);
    });
    // harvest nodes
    auto nodeMesh = [&](NodeType t) {
        return mk([&](MeshBuilder& b) {
            Rng r(31 + (int)t);
            switch (t) {
            case NodeType::Tree: for (int i = 0; i < 3; i++) b.tube({-0.8f, 0.22f + i * 0.38f * (i < 2 ? 1.f : 0.5f), (i == 2 ? 0.f : (i ? 0.22f : -0.22f))}, {0.8f, 0.22f + i * 0.38f * (i < 2 ? 1.f : 0.5f), (i == 2 ? 0.05f : (i ? 0.2f : -0.2f))}, 0.2f, 0.19f, Mat(hexColor(0x6a4a30), 0.9f), 8, true);
                for (auto& v : b.v) if (std::fabs(v.nx) > 100) { v.r = 200; v.g = 170; v.b = 120; } break;
            case NodeType::Rock: b.rock({0, 0.3f, 0}, vec3(0.7f, 0.5f, 0.6f), Mat(hexColor(0x8a8680)), 5, 1); break;
            case NodeType::IronOre: case NodeType::CopperOre: case NodeType::RareOre: case NodeType::GoldOre: case NodeType::SilverOre: case NodeType::Coal: {
                b.rock({0, 0.35f, 0}, vec3(0.75f, 0.55f, 0.7f), Mat(t == NodeType::Coal ? hexColor(0x4a4642) : hexColor(0x6a6660)), 7, 1);
                vec3 oc = t == NodeType::IronOre ? hexColor(0xb8a08a) : t == NodeType::CopperOre ? hexColor(0xd07a3a) : t == NodeType::GoldOre ? hexColor(0xf0c040)
                        : t == NodeType::SilverOre ? hexColor(0xdfe4ea) : t == NodeType::Coal ? hexColor(0x141312) : hexColor(0x8ad0ff);
                float metal = (t == NodeType::RareOre || t == NodeType::Coal) ? 0.f : 0.85f, rough = t == NodeType::Coal ? 0.9f : (t == NodeType::GoldOre || t == NodeType::SilverOre ? 0.18f : 0.3f);
                int veins = t == NodeType::Coal ? 9 : 6;
                for (int i = 0; i < veins; i++) { vec3 d = r.onSphere(); d.y = std::fabs(d.y); b.rock(vec3(d.x * 0.6f, 0.3f + d.y * 0.4f, d.z * 0.55f), vec3(t == NodeType::Coal ? 0.18f : 0.14f), Mat(oc, rough, metal, t == NodeType::RareOre ? 2.f : 0.f), r.next(), 0); }
                break;
            }
            case NodeType::Cactus: {   // saguaro: ribbed trunk, two arms, pink fruits on top
                Mat green(hexColor(0x4f7a3a), 0.7f), fruit(hexColor(0xd8508a), 0.5f);
                b.tube({0, 0, 0}, {0, 1.9f, 0}, 0.2f, 0.17f, green, 10, true);
                for (int s = -1; s <= 1; s += 2) {
                    float h = s < 0 ? 0.7f : 1.0f;
                    b.tube({s * 0.15f, h, 0}, {s * 0.45f, h + 0.08f, 0}, 0.11f, 0.11f, green, 8, false);
                    b.tube({s * 0.45f, h + 0.05f, 0}, {s * 0.47f, h + 0.65f, 0}, 0.11f, 0.09f, green, 8, true);
                }
                for (int i = 0; i < 4; i++) { float a = i * TAU / 4 + 0.4f; b.ellipsoid({std::cos(a) * 0.12f, 1.95f, std::sin(a) * 0.12f}, vec3(0.06f, 0.08f, 0.06f), fruit, 6, 4); }
                break;
            }
            case NodeType::Shells: {   // a few shells and a pearl oyster in the sand
                for (int i = 0; i < 6; i++) {
                    vec3 p{r.range(-0.45f, 0.45f), 0, r.range(-0.45f, 0.45f)}; float s = r.range(0.06f, 0.12f);
                    vec3 col = i % 3 == 0 ? hexColor(0xf0d8c8) : (i % 3 == 1 ? hexColor(0xe8a890) : hexColor(0xd8c0a0));
                    b.lathe({{0.0f, 0.0f}, {s, 0.01f}, {s * 0.8f, s * 0.5f}, {0.0f, s * 0.6f}}, Mat(col, 0.35f), 7, p);
                }
                b.lathe({{0.0f, 0.0f}, {0.2f, 0.02f}, {0.18f, 0.08f}, {0.0f, 0.1f}}, Mat(hexColor(0x8a8278), 0.6f), 9, {0.1f, 0, -0.05f});
                b.ellipsoid({0.1f, 0.11f, -0.05f}, vec3(0.045f), Mat(hexColor(0xf6f0ff), 0.08f, 0.2f, 0.4f), 8, 6);   // the pearl
                break;
            }
            case NodeType::Flax: {     // tall thin stems with small blue flowers
                for (int i = 0; i < 18; i++) {
                    vec3 p{r.range(-0.4f, 0.4f), 0, r.range(-0.4f, 0.4f)}; float h = r.range(0.7f, 1.0f);
                    vec3 top = p + vec3(r.range(-0.08f, 0.08f), h, r.range(-0.08f, 0.08f));
                    b.tube(p, top, 0.012f, 0.008f, Mat(hexColor(0x7a9a4a), 0.8f), 4, false);
                    b.ellipsoid(top, vec3(0.04f, 0.02f, 0.04f), Mat(hexColor(0x6a8ae0), 0.6f, 0, 0.3f), 6, 3);
                }
                break;
            }
            case NodeType::AmberRock: {   // sandstone with glowing amber lumps
                b.rock({0, 0.35f, 0}, vec3(0.8f, 0.5f, 0.7f), Mat(hexColor(0xc89a60), 0.9f), 9, 1);
                for (int i = 0; i < 6; i++) { vec3 d = r.onSphere(); d.y = std::fabs(d.y); b.ellipsoid(vec3(d.x * 0.62f, 0.3f + d.y * 0.38f, d.z * 0.55f), vec3(0.1f, 0.08f, 0.1f), Mat(hexColor(0xf09a20), 0.1f, 0, 1.4f), 7, 5); }
                break;
            }
            case NodeType::FireRoot: {    // red leaves, glowing ember bulbs
                for (int i = 0; i < 7; i++) { float a = i * TAU / 7; vec3 d{std::cos(a), 0, std::sin(a)}; b.blade({0, 0, 0}, d * 0.3f + vec3(0, 0.45f, 0), 0.12f, {-d.z, 0, d.x}, Mat(hexColor(0x6a1a14)), Mat(hexColor(0xc8402a), 0.6f)); }
                for (int i = 0; i < 3; i++) b.ellipsoid({r.range(-0.12f, 0.12f), 0.12f, r.range(-0.12f, 0.12f)}, vec3(0.07f), Mat(hexColor(0xff7a20), 0.3f, 0, 2.2f), 7, 5);
                break;
            }
            case NodeType::Herb: for (int i = 0; i < 8; i++) { float a = i * TAU / 8; vec3 d{std::cos(a), 0, std::sin(a)}; b.blade({0, 0, 0}, d * 0.35f + vec3(0, 0.5f, 0), 0.14f, {-d.z, 0, d.x}, Mat(hexColor(0x4a8a3a)), Mat(hexColor(0x8ac85a), 0.7f)); }
                for (int i = 0; i < 4; i++) b.ellipsoid({r.range(-0.2f, 0.2f), 0.55f, r.range(-0.2f, 0.2f)}, vec3(0.05f), Mat(hexColor(0xe8e0ff), 0.5f, 0, 0.6f), 6, 4); break;
            case NodeType::Crystal: b.flat = true; for (int i = 0; i < 5; i++) { vec3 d = normalize(vec3(r.range(-0.5f, 0.5f), 1, r.range(-0.5f, 0.5f))); b.cone({r.range(-0.2f, 0.2f), 0, r.range(-0.2f, 0.2f)}, d * r.range(0.6f, 1.1f), 0.14f, Mat(hexColor(0x9ad8ff), 0.1f, 0, 2.5f), 5); } break;
            case NodeType::AuroraCrystal: {   // 2.8.0 a cluster of glowing hexagonal crystals, turquoise to violet, on a pale rock
                b.flat = true;
                b.rock({0, 0.1f, 0}, vec3(0.55f, 0.28f, 0.5f), Mat(hexColor(0x6a6660), 0.85f), r.next(), 0);
                for (int i = 0; i < 7; i++) {
                    vec3 d = normalize(vec3(r.range(-0.6f, 0.6f), 1, r.range(-0.6f, 0.6f)));
                    float l = r.range(0.6f, 1.5f) * (i == 0 ? 1.4f : 1.f);
                    vec3 col = lerp(hexColor(0x9ae8dc), hexColor(0xc4a8f0), r.f());   // pale aqua to lilac, a soft inner light
                    vec3 base{r.range(-0.3f, 0.3f), 0.05f, r.range(-0.3f, 0.3f)};
                    b.tube(base, base + d * l * 0.8f, 0.13f, 0.11f, Mat(col, 0.1f, 0, 1.1f), 6, false);
                    b.cone(base + d * l * 0.8f, base + d * l, 0.12f, Mat(col * 1.1f, 0.08f, 0, 1.5f), 6);
                }
                b.flat = false;
                break;
            }
            case NodeType::Chest: chest(b, false); break;
            case NodeType::Relic: b.bevelBox({0, 0.4f, 0}, {0.35f, 0.4f, 0.35f}, 0.05f, Mat(hexColor(0xd8d0c0), 0.6f)); b.ellipsoid({0, 1.05f, 0}, vec3(0.16f, 0.22f, 0.16f), Mat(hexColor(0xffd060), 0.2f, 1.f, 2.5f), 10, 6); break;
            case NodeType::Mushroom: for (int i = 0; i < 5; i++) { vec3 p{r.range(-0.35f, 0.35f), 0, r.range(-0.35f, 0.35f)}; float h = r.range(0.2f, 0.4f); b.tube(p, p + vec3(0, h, 0), 0.04f, 0.035f, Mat(hexColor(0xe8e0cc)), 5, false); b.lathe({{0.02f, h}, {0.14f, h - 0.02f}, {0.1f, h + 0.07f}, {0.01f, h + 0.09f}}, Mat(hexColor(0x6ad0ff), 0.4f, 0, 2.f), 8, p); } break;
            case NodeType::Berries: b.flat = true; for (int i = 0; i < 4; i++) b.rock({r.range(-0.3f, 0.3f), 0.45f, r.range(-0.3f, 0.3f)}, vec3(0.45f, 0.35f, 0.45f), Mat(hexColor(0x3f6a2a)), r.next(), 0); b.flat = false;
                for (int i = 0; i < 14; i++) { vec3 d = r.onSphere(); d.y = std::fabs(d.y); b.ellipsoid(vec3(d.x * 0.5f, 0.4f + d.y * 0.35f, d.z * 0.5f), vec3(0.06f), Mat(hexColor(0x5a2a8a), 0.3f, 0, 0.2f), 5, 3); } break;
            case NodeType::Totem: {
                // 3.2.0 a totem of power: a stepped plinth with glowing runes, a carved monolith with three faces,
                // a crown of horns, a rune crystal floating above it, and a faint beam of light rising to the sky
                Mat stone(hexColor(0x6a665e), 0.85f), dark(hexColor(0x3c3a36), 0.9f), gold(hexColor(0xd8b058), 0.3f, 0.9f), rune(hexColor(0x7af0ff), 0.3f, 0, 3.5f);
                b.flat = true;
                b.lathe({{1.6f, 0}, {1.6f, 0.25f}, {1.25f, 0.25f}, {1.25f, 0.5f}, {0.95f, 0.5f}, {0.95f, 0.7f}, {0.01f, 0.7f}}, stone, 8);
                b.flat = false;
                for (int i = 0; i < 8; i++) { float a = i * TAU / 8; b.box({std::cos(a) * 1.42f, 0.27f, std::sin(a) * 1.42f}, {0.16f, 0.02f, 0.05f}, rune, quat::axisAngle({0, 1, 0}, -a)); }
                b.flat = true;
                b.lathe({{0.55f, 0.7f}, {0.5f, 2.6f}, {0.62f, 2.85f}, {0.4f, 3.05f}, {0.01f, 3.1f}}, stone, 6);
                b.flat = false;
                for (int f = 0; f < 3; f++) {   // three carved faces, glowing eyes
                    float a = f * TAU / 3; vec3 n{std::cos(a), 0, std::sin(a)};
                    quat q = quat::axisAngle({0, 1, 0}, -a + PI / 2);
                    b.box(n * 0.5f + vec3(0, 2.15f, 0), {0.28f, 0.06f, 0.05f}, dark, q);
                    for (int s2 = -1; s2 <= 1; s2 += 2) b.ellipsoid(n * 0.52f + vec3(0, 2.25f, 0) + vec3(n.z, 0, -n.x) * (s2 * 0.13f), vec3(0.05f, 0.035f, 0.05f), rune, 6, 4);
                    b.box(n * 0.5f + vec3(0, 1.6f, 0), {0.2f, 0.18f, 0.04f}, dark, q);
                    b.box(n * 0.51f + vec3(0, 1.1f, 0), {0.06f, 0.3f, 0.03f}, rune, q);
                }
                for (int i = 0; i < 4; i++) { float a = i * TAU / 4 + 0.4f; vec3 d{std::cos(a), 0, std::sin(a)}; b.cone(d * 0.42f + vec3(0, 2.95f, 0), d * 0.85f + vec3(0, 3.6f, 0), 0.09f, gold, 5); }
                b.flat = true;
                b.lathe({{0.01f, 3.6f}, {0.22f, 3.95f}, {0.01f, 4.45f}}, rune, 6);   // the floating crystal
                b.flat = false;
                b.tube({0, 4.5f, 0}, {0, 40.f, 0}, 0.12f, 0.35f, Mat(hexColor(0x9af4ff), 0.2f, 0, 1.6f), 6, false);   // the beam
                break;
            }
            case NodeType::Orb: {
                // 3.2.0 an ancient orb: a luminous sphere in a cage of gold rings, over a small rune stone
                Mat gold(hexColor(0xd8b058), 0.3f, 0.9f), glow(hexColor(0xffe08a), 0.2f, 0, 4.f);
                b.flat = true; b.lathe({{0.35f, -1.1f}, {0.3f, -0.85f}, {0.18f, -0.8f}, {0.01f, -0.8f}}, Mat(hexColor(0x6a665e), 0.8f), 6); b.flat = false;
                b.ellipsoid({0, 0, 0}, vec3(0.28f), glow, 12, 8);
                for (int i = 0; i < 3; i++) b.lathe({{0.4f, -0.015f}, {0.4f, 0.015f}, {0.37f, 0.015f}, {0.37f, -0.015f}}, gold, 16, {}, quat::axisAngle(normalize(vec3(i == 0, i == 1, i == 2)), PI / 2));
                b.tube({0, 0.35f, 0}, {0, 18.f, 0}, 0.05f, 0.18f, Mat(hexColor(0xffe8a0), 0.2f, 0, 1.2f), 6, false);   // a thin golden beam
                break;
            }
            default: break;
            }
            b.bakeHeightAO(0, 0.6f, 0.7f);
        });
    };
    for (int i = 0; i < (int)NodeType::NodeTypeCount; i++) nodeMeshes[i] = nodeMesh((NodeType)i);
}

// ============================================================================================ visuals
void Game::setupVisualHumanoid(Visual& v, const HumanoidLook& L, WeaponType wt, WeaponType ot, float scale) {
    v.arch = Arch::Humanoid;
    char key[160];
    snprintf(key, sizeof key, "h%d_%d_%d_%d_%u_%.2f_%.2f_%.3f_%.3f_%.3f_%d%d%d", (int)L.species, (int)L.body, (int)L.helm, (int)L.hair, L.seed, L.bulk, L.headSize, L.c1.x, L.c1.y, L.c1.z, L.female, L.beard, L.pauldrons);
    v.mesh = cachedMesh(key, [&](MeshBuilder& mb) { buildHumanoid(mb, L); });
    setupAnimator(v.anim, Arch::Humanoid);
    v.wt = wt; v.ot = ot;
    v.anim.stance = stanceFor(wt);
    if (wt != WeaponType::None) v.weapon = weaponMesh(wt, 1, L.glow, &v.tip);
    else v.weapon.reset();
    if (ot != WeaponType::None) v.offhand = weaponMesh(ot, 1, L.c1);
    else v.offhand.reset();
    v.scale = scale;
    v.cape = L.cape;
    v.capeColor = L.c1;
    v.cloth = VerletCloth();
}

void Game::setupVisualCreature(Visual& v, Arch a, const CreatureLook& L, float scale) {
    v.arch = a;
    char key[96];
    snprintf(key, sizeof key, "c%d_%d_%u_%.2f_%.2f_%.2f_%d%d%d%d", (int)a, L.kind, L.seed, L.c1.x, L.c1.y, L.c1.z, L.saddle, L.horns, L.crystal, L.armored);
    v.mesh = cachedMesh(key, [&](MeshBuilder& mb) {
        if (a == Arch::Spider) buildSpider(mb, L);
        else if (a == Arch::Dragon) buildDragon(mb, L);
        else buildQuadruped(mb, L);
    });
    setupAnimator(v.anim, a);
    v.scale = scale;
    v.footIK = false;
    // gait scales with body size so legs don't slide
    v.anim.gait.strideWalk *= scale; v.anim.gait.strideRun *= scale;
}

// ============================================================================================ entities
Character& Game::pc() { return *reg.get<Character>(player); }

Entity Game::spawnPlayer(vec3 pos) {
    Entity e = reg.create();
    Character c;
    c.team = Team::Player; c.name = "L'Errant"; c.pos = pos; c.radius = 0.42f; c.height = 1.8f;
    c.maxHp = c.hp = 120; c.atk = 10; c.def = 3; c.maxPoise = c.poise = 40;
    reg.add<Character>(e, c);
    player = e;
    rebuildPlayerVisual();
    return e;
}

Entity Game::spawnEnemy(int def, vec3 pos, bool elite, uint32_t uid) {
    const EnemyDef& d = enemies()[def];
    Entity e = reg.create();
    Character c;
    c.team = Team::Enemy; c.name = d.name; c.pos = pos; c.enemyDef = def; c.uid = uid; c.elite = elite;
    c.radius = d.radius * (elite ? 1.15f : 1.f); c.level = d.level + (elite ? 2 : 0);
    float hpm = elite ? 1.8f : 1.f;
    c.maxHp = c.hp = d.hp * hpm * difficultyEnemyHp(); c.atk = d.atk * (elite ? 1.3f : 1.f) * difficultyEnemyAtk(); c.def = d.def; c.maxPoise = c.poise = d.poise * hpm;   // 2.6.0 difficulty + mods
    float sc = d.scale * (elite ? 1.18f : 1.f) * gameTuning().enemyScale;   // 2.7.0 mods: giants / minis
    if (d.arch == Arch::Humanoid) {
        HumanoidLook L = d.hlook;
        setupVisualHumanoid(c.vis, L, d.weapon, d.offhand, sc);
        applyImportedModel(c.vis, d.id);   // models/<enemy id>.lkm when provided
        c.vis.anim.aux[0] = (d.behavior == Behavior::Tank || L.species == Species::Goblin || L.species == Species::Troll) ? 0.6f : 0.2f;
        c.height = 1.8f * sc;
    } else {
        setupVisualCreature(c.vis, d.arch, d.clook, sc);
        // 2.9.0 imported animals (Tripo gallery models): models/<enemy id>.lkm. 3.1.0 never for wolves: they are
        // all drawn by the game (wolfBody in Characters.cpp), the Tripo wolf skin is gone.
        if (d.arch == Arch::Quadruped && (d.family != "Loups" || d.modded)) applyImportedModel(c.vis, d.id, Arch::Quadruped);   // 3.1.1 a mod creature may bring its own
        c.height = (d.arch == Arch::Spider ? 0.8f : 1.1f) * sc;
    }
    c.vis.anim.gait.walkSpeed = std::min(1.6f, d.speed * 0.4f);
    c.vis.anim.gait.runSpeed = d.speed;
    c.vis.anim.gait.sprintSpeed = d.speed * 1.5f;
    c.vis.rim = 0.35f;
    c.vis.tint = d.tint;   // 3.1.1 mods
    if (elite) { c.vis.outlineColor = vec4(0.9f, 0.6f, 0.1f, 1); c.vis.emissiveBoost = 0.6f; c.name = "Élite : " + c.name; }
    c.mass = sc * sc;
    c.yaw = particles.rng.range(0, TAU);
    reg.add<Character>(e, c);
    AIBrain ai; ai.home = pos; ai.state = AIState::Idle; ai.timer = particles.rng.range(1.f, 4.f); ai.strafeDir = particles.rng.chance(0.5f) ? 1.f : -1.f;
    if (d.arch == Arch::Dragon && d.behavior != Behavior::Boss) ai.fly = 0;
    reg.add<AIBrain>(e, ai);
    return e;
}

Entity Game::spawnBoss(int bi, vec3 pos) {
    const BossDef& b = bosses()[bi];
    Entity e = spawnEnemy(b.enemy, pos, false, 0);
    Character& c = *reg.get<Character>(e);
    c.bossDef = bi; c.name = b.name;
    c.maxHp = c.hp = enemies()[b.enemy].hp * b.hpMult;
    c.vis.rim = 0.5f; c.vis.emissiveBoost = 0.4f;
    AIBrain& ai = *reg.get<AIBrain>(e);
    ai.state = AIState::Intro; ai.timer = 4.2f; ai.home = pos;
    bossEnt = e; bossBarName = b.name; bossBarShow = 0;
    // cinematic intro: the camera sweeps around the boss while it roars
    vec3 fwd = normalize(pc().pos - pos);
    // the last frame follows the player's camera pitch (low camera -> low shot, top view -> high shot),
    // then the camera glides back to the gameplay view (camCineBlend)
    float endR = 5 + c.vis.scale * 2.5f;
    float endH = clampf(std::tan(clampf(camPitch, -0.2f, 1.1f)) * endR * 0.55f + c.vis.scale * 0.6f, 0.8f, endR * 1.4f);
    startCinematic(pos + fwd * (8 + c.vis.scale * 3) + vec3(0, 3 + c.vis.scale, 0) + perpVec(fwd) * 6.f, pos + fwd * endR + vec3(0, endH, 0) - perpVec(fwd) * 3.f,
                   pos + vec3(0, c.height * 0.6f, 0), 4.f, b.intro);
    play("boss_intro", 1.f); play(enemies()[b.enemy].arch == Arch::Dragon ? "dragon_roar" : "roar", 1.f, 0.8f);
    haptic(120, 200);
    return e;
}

Entity Game::spawnNpc(int def, vec3 pos, int village) {
    const NpcDef& d = npcs()[def];
    Entity e = reg.create();
    Character c;
    c.team = Team::Neutral; c.name = d.name; c.pos = pos; c.npcDef = def; c.maxHp = c.hp = 200;
    WeaponType wt = d.role == NpcRole::Guard ? WeaponType::Spear : (d.role == NpcRole::Blacksmith ? WeaponType::Hammer : WeaponType::None);
    setupVisualHumanoid(c.vis, d.look, wt, WeaponType::None, d.look.species == Species::Dwarf ? 0.82f : (d.role == NpcRole::Child ? 0.7f : 1.f));
    if (!applyImportedModel(c.vis, "npc_" + d.id) && d.role != NpcRole::Child) applyImportedModel(c.vis, d.look.female ? "villager_f" : "villager_m");
    c.vis.rim = 0.4f;
    c.vis.anim.gait.walkSpeed = 1.3f;
    c.yaw = particles.rng.range(0, TAU);
    reg.add<Character>(e, c);
    NpcBrain nb;
    nb.home = pos; nb.work = pos; nb.social = pos; nb.bed = pos; nb.village = village; nb.generic = false; nb.dest = pos;
    nb.workClip = d.role == NpcRole::Blacksmith ? C_HAMMER_WORK : (d.role == NpcRole::Farmer ? C_GATHER_SWING : C_SWEEP_WORK);
    reg.add<NpcBrain>(e, nb);
    Interact it; it.kind = InterKind::Npc; it.param = def; it.text = "Parler"; it.radius = 2.6f;
    reg.add<Interact>(e, it);
    return e;
}

static const char* VILLAGER_NAMES[] = {"Arnaud", "Bérénice", "Colin", "Delphine", "Ernest", "Fleur", "Gaspard", "Héloïse", "Isidore", "Jeanne", "Léon", "Mathilde", "Noé", "Ophélie", "Perrin", "Rosalie", "Sacha", "Thibault", "Ursule", "Victor", "Yvette", "Zélie"};
static const char* VILLAGER_LINES[] = {"Belle journée, n'est-ce pas ?", "Les routes ne sont plus sûres…", "On raconte que tu as vaincu des monstres !", "Le marché ouvre à l'aube.",
                                       "Ma grand-mère a connu le temps des rois.", "Fais attention la nuit, les morts se lèvent.", "Tu as l'air fatigué, voyageur.", "As-tu goûté le ragoût de l'auberge ?",
                                       "Les étoiles brillent plus fort ces temps-ci.", "Mon fils veut devenir chevalier, comme toi."};

Entity Game::spawnVillager(vec3 pos, int village, uint32_t seed) {
    Rng r(seed);
    Entity e = reg.create();
    Character c;
    c.team = Team::Neutral; c.pos = pos; c.maxHp = c.hp = 60;
    c.name = VILLAGER_NAMES[seed % 22];
    HumanoidLook L;
    L.female = r.chance(0.5f); L.seed = seed;
    static const uint32_t cloth[] = {0x6a4a3a, 0x3a5a7a, 0x7a3a3a, 0x5a6a3a, 0x8a7a5a, 0x4a3a5a, 0xa08a6a};
    static const uint32_t hair[] = {0x2a1a10, 0x5a3a22, 0xa07040, 0xd8c090, 0x8a8a8a, 0x1a1a1a, 0x8a3a1a};
    static const uint32_t skins[] = {0xe8c0a0, 0xd8a888, 0xc08868, 0x9a6a4a, 0x7a4f36, 0xf0d0b8};
    L.c1 = hexColor(cloth[r.irange(0, 6)]); L.c2 = hexColor(cloth[r.irange(0, 6)]) * 0.8f; L.hairCol = hexColor(hair[r.irange(0, 6)]); L.skin = hexColor(skins[r.irange(0, 5)]);
    L.body = L.female ? (r.chance(0.6f) ? Body::Dress : Body::Tunic) : (r.chance(0.3f) ? Body::Apron : Body::Tunic);
    L.hair = L.female ? (r.chance(0.5f) ? Hair::Long : (r.chance(0.5f) ? Hair::Braids : Hair::Bun)) : (r.chance(0.8f) ? Hair::Short : Hair::None);
    L.beard = !L.female && r.chance(0.4f);
    L.eyes = hexColor(r.chance(0.5f) ? 0x4a3420 : (r.chance(0.5f) ? 0x3a5a7a : 0x4a6a3a));
    // hats: straw hats and caps for men, headscarves for women
    L.hat = L.female ? (r.chance(0.3f) ? 3 : 0) : (r.chance(0.25f) ? 1 : (r.chance(0.2f) ? 2 : 0));
    int reg_ = world.pois[village].region;
    if (reg_ == R_FROSTHEIM) { L.body = Body::Fur; }
    if (reg_ == R_ELDERWOOD) { L.species = Species::Elf; }
    if (reg_ == R_ASHEN && !L.female && r.chance(0.3f)) { L.species = Species::Dwarf; L.bulk = 1.3f; L.beard = true; }
    if (reg_ == R_HIGHCROWNS && r.chance(0.4f)) { L.body = Body::Robe; L.c1 = hexColor(0x8a8aa0); }
    bool child = r.chance(0.15f);
    setupVisualHumanoid(c.vis, L, WeaponType::None, WeaponType::None, child ? 0.68f : (L.species == Species::Dwarf ? 0.82f : r.range(0.95f, 1.05f)));
    if (!child) {
        // generic villager models when provided; up to 3 variants per sex (villager_m, villager_m2, villager_m3)
        // so a village is not a crowd of clones
        std::string base = L.female ? "villager_f" : "villager_m";
        int v = (int)(seed % 3);
        if (v == 0 || !applyImportedModel(c.vis, base + std::to_string(v + 1))) applyImportedModel(c.vis, base);
    }
    c.vis.anim.gait.walkSpeed = 1.3f;
    reg.add<Character>(e, c);
    NpcBrain nb;
    const Poi& p = world.pois[village];
    float a = r.range(0, TAU);
    nb.home = vec3(p.pos.x + std::cos(a) * p.radius * 0.5f, p.height, p.pos.y + std::sin(a) * p.radius * 0.5f);
    nb.work = vec3(p.pos.x + r.range(-15, 15), p.height, p.pos.y + r.range(-15, 15));
    nb.social = vec3(p.pos.x + r.range(-6, 6), p.height, p.pos.y + r.range(-6, 6));
    nb.bed = nb.home;
    nb.village = village; nb.generic = true; nb.dest = pos;
    nb.workClip = r.chance(0.5f) ? C_SWEEP_WORK : C_HAMMER_WORK;
    reg.add<NpcBrain>(e, nb);
    Interact it; it.kind = InterKind::Npc; it.param = -1 - (int)(seed % 10); it.text = "Parler"; it.radius = 2.4f;
    reg.add<Interact>(e, it);
    return e;
}

Entity Game::spawnAnimal(int kind, vec3 pos) {
    Entity e = reg.create();
    Character c;
    c.team = Team::Animal; c.pos = pos; c.animal = kind; c.maxHp = c.hp = kind == 5 ? 12 : 40;
    CreatureLook L; L.kind = kind; L.seed = (uint32_t)(pos.x * 13 + pos.z);
    if (kind == 4) { L.c1 = hexColor(0x8a6a4a); L.c2 = hexColor(0xd8c8a8); L.horns = true; }
    else if (kind == 9) { L.c1 = hexColor(0x6a4a30); L.c2 = hexColor(0x8a6a4a); L.horns = true; }
    else { L.c1 = hexColor(0x9a8a7a); L.c2 = hexColor(0xe8e0d8); }
    c.name = kind == 4 ? "Cerf" : kind == 9 ? "Mammouth" : "Lièvre";
    if (kind == 9) c.maxHp = c.hp = 420;
    setupVisualCreature(c.vis, Arch::Quadruped, L, kind == 5 ? 0.35f : kind == 9 ? 4.2f : 1.3f);
    if (kind == 4) applyImportedModel(c.vis, "deer", Arch::Quadruped);   // 2.9.0 the realistic stag (Tripo gallery)
    if (kind == 9) applyImportedModel(c.vis, "mammoth", Arch::Quadruped);   // 3.9.5 the woolly mammoth (the player's Tripo model)
    c.vis.anim.gait.runSpeed = kind == 5 ? 6.f : kind == 9 ? 6.5f : 8.f; c.vis.anim.gait.walkSpeed = kind == 9 ? 1.6f : 1.f;
    c.radius = kind == 5 ? 0.2f : kind == 9 ? 1.7f : 0.5f; c.height = kind == 5 ? 0.3f : kind == 9 ? 3.4f : 1.4f;
    c.vis.outline = false;
    reg.add<Character>(e, c);
    AnimalBrain ab; ab.home = pos; ab.dest = pos; ab.kind = kind;
    reg.add<AnimalBrain>(e, ab);
    return e;
}

Entity Game::spawnMount(int def, vec3 pos) {
    const MountDef& m = mounts()[def];
    Entity e = reg.create();
    Character c;
    c.team = Team::Mount; c.pos = pos; c.mountDef = def; c.name = m.name; c.maxHp = c.hp = 300;
    setupVisualCreature(c.vis, Arch::Quadruped, m.look, m.scale);
    if (m.id == "horse") applyImportedModel(c.vis, "horse_mount", Arch::Quadruped);   // 2.3.3 the detailed horse (falls back to the built one)
    c.vis.anim.gait.walkSpeed = 1.6f; c.vis.anim.gait.runSpeed = m.speed * 0.68f; c.vis.anim.gait.sprintSpeed = m.speed * 0.9f;   // 3.8.1 the speeds actually reached: walk, trot at the usual pace, gallop when spurred
    c.radius = 0.6f; c.height = 1.6f;
    reg.add<Character>(e, c);
    Interact it; it.kind = InterKind::Mount; it.param = def; it.text = "Monter"; it.radius = 2.5f;
    reg.add<Interact>(e, it);
    return e;
}

Entity Game::spawnPickup(const std::string& id, int count, Rarity r, vec3 pos) {
    Entity e = reg.create();
    Pickup p; p.item = id; p.count = count; p.rarity = r; p.pos = pos + vec3(0, 0.3f, 0);
    p.vel = vec3(particles.rng.range(-2, 2), 4.f, particles.rng.range(-2, 2));
    reg.add<Pickup>(e, p);
    const ItemDef* d = item(id);
    Interact it; it.kind = InterKind::Pickup; it.text = std::string("Ramasser ") + (d ? d->name : id); it.radius = 2.f;
    reg.add<Interact>(e, it);
    return e;
}

void Game::spawnNode(const NodeSpawn& n) {
    if (nodeEntities.count(n.uid)) return;
    if (n.type == NodeType::Chest && openedChests.count(n.uid)) return;
    if (n.type != NodeType::Chest && n.type != NodeType::FishingSpot && !world.nodeAvailable(n.uid, gameTime)) return;
    Entity e = reg.create();
    NodeComp nc; nc.node = n;
    if ((int)n.type < (int)NodeType::NodeTypeCount && n.type != NodeType::FishingSpot) nc.mesh = nodeMeshes[(int)n.type];
    reg.add<NodeComp>(e, nc);
    Interact it;
    switch (n.type) {
    case NodeType::Tree: it.kind = InterKind::Node; it.text = "Couper du bois"; break;
    case NodeType::Rock: case NodeType::IronOre: case NodeType::CopperOre: case NodeType::RareOre: case NodeType::GoldOre: case NodeType::SilverOre: case NodeType::Coal: case NodeType::AmberRock:
        it.kind = InterKind::Node; it.text = "Miner"; break;
    case NodeType::Herb: case NodeType::Mushroom: case NodeType::Berries: case NodeType::Crystal: case NodeType::Flax: case NodeType::FireRoot: it.kind = InterKind::Node; it.text = "Récolter"; break;
    case NodeType::AuroraCrystal: it.kind = InterKind::Node; it.text = "Détacher le cristal d'aurore"; break;   // 2.8.0
    case NodeType::Cactus: it.kind = InterKind::Node; it.text = "Cueillir les fruits"; break;
    case NodeType::Shells: it.kind = InterKind::Node; it.text = "Ramasser les coquillages"; break;
    case NodeType::Chest: it.kind = InterKind::Chest; it.text = "Ouvrir le coffre"; break;
    case NodeType::Relic: it.kind = InterKind::Node; it.text = "Examiner la relique"; break;
    case NodeType::Totem: it.kind = InterKind::Node; it.text = "Éveiller le totem de pouvoir"; it.radius = 3.2f; break;   // 3.2.0
    case NodeType::Orb: it.kind = InterKind::Node; it.text = "Prendre l'orbe ancien"; it.radius = 2.4f; break;
    case NodeType::FishingSpot: it.kind = InterKind::Fishing; it.text = "Pêcher"; it.radius = 4.5f; break;
    default: break;
    }
    reg.add<Interact>(e, it);
    nodeEntities[n.uid] = e;
}

// ============================================================================================ streaming spawner
static uint32_t chunkHash(ChunkKey k) { return (uint32_t)(k.x * 4096 + k.z); }

void Game::updateSpawning(float dt) {
    if (player == NullEntity) return;
    Character& p = pc();
    vec3 pp = p.pos;
    static float acc = 0;
    acc += dt;
    if (acc < 0.5f) return;
    acc = 0;
    if (dungeon.active && pp.x > WORLD_SIZE) return;
    // ---- populate chunks near the player
    ChunkKey pk = chunkOf(pp.x, pp.z);
    for (int dz = -3; dz <= 3; dz++)
        for (int dx = -3; dx <= 3; dx++) {
            ChunkKey k{pk.x + dx, pk.z + dz};
            vec2 cc{(k.x + 0.5f) * CHUNK, (k.z + 0.5f) * CHUNK};
            if (length(cc - pp.xz()) > 100.f) continue;
            Chunk* ch = world.chunkAt(cc.x, cc.y);
            if (!ch || !ch->ready || ch->lod != 0) continue;
            for (const NodeSpawn& n : ch->nodes) spawnNode(n);
            uint32_t h = chunkHash(k);
            if (spawnedChunks.count(h)) continue;
            spawnedChunks.insert(h);
            // creatures: never right next to the player, nor inside settlements
            if (length(cc - pp.xz()) < 40.f) continue;
            char key[32]; snprintf(key, sizeof key, "%d_%d", k.x, k.z);
            auto cl = clearedSpawns.find(key);
            if (cl != clearedSpawns.end() && cl->second > gameTime) continue;
            Rng r(hash2i(k.x, k.z, 31 + day / 3));
            int region = ch->region;
            int poi = world.nearestPoi(cc, 90.f);
            bool safe = poi >= 0 && (world.pois[poi].type == PoiType::Village || world.pois[poi].type == PoiType::Monastery || world.pois[poi].type == PoiType::Mill || world.pois[poi].type == PoiType::Farm || world.pois[poi].hostile);   // 3.0.0 fortresses: their garrison only
            if (safe) continue;
            bool camp = poi >= 0 && world.pois[poi].type == PoiType::Camp;
            float chance = camp ? 1.f : 0.3f + env.night * 0.15f;
            std::vector<Entity>& list = chunkSpawns[h];
            if (r.f() < chance && coop.mode != CoopState::Guest) {   // 1.13.0 co-op guest: the host's creatures
                // pick an enemy appropriate for the region (night creatures mostly at night)
                std::vector<int> cands;
                const auto& E = enemies();
                for (int i = 0; i < (int)E.size(); i++) {
                    if (E[i].behavior == Behavior::Boss || E[i].hunter || E[i].region != region) continue;
                    if (E[i].night && env.night < 0.5f && r.f() < 0.7f) continue;
                    if (camp && E[i].family != "Bandits" && E[i].family != "Pilleurs" && E[i].family != "Corsaires") continue;   // 2.0.0: desert raiders and corsairs man their camps too
                    cands.push_back(i);
                }
                if (!cands.empty()) {
                    // 3.1.1 weighted by spawnWeight (1 for the game's creatures; mods can make theirs rarer or commoner)
                    float tot = 0; for (int c : cands) tot += E[c].spawnWeight;
                    float pick = r.f() * tot; int def = cands.back();
                    for (int c : cands) { pick -= E[c].spawnWeight; if (pick <= 0) { def = c; break; } }
                    int n = std::max(1, std::min(4, E[def].group + r.irange(-1, 1)));
                    if (camp) n = 3;
                    for (int i = 0; i < n; i++) {
                        vec2 o = r.inDisk() * 6.f;
                        vec3 sp{cc.x + o.x, 0, cc.y + o.y};
                        sp.y = groundAt(sp.x, sp.z);
                        float wl = waterAt(sp.x, sp.z);
                        if (wl > -1e8f && sp.y < wl + 0.3f) continue;
                        uint32_t uid = hash3i(k.x, k.z, i, 77);
                        bool elite = r.chance(0.06f);
                        if (elite && killedUnique.count(uid)) elite = false;
                        list.push_back(spawnEnemy(def, sp, elite, uid));
                    }
                }
            }
            // wildlife
            if (r.f() < (region == R_GREENHAVEN || region == R_ELDERWOOD ? 0.25f : 0.08f) && !camp) {
                int kind = r.chance(0.6f) ? 5 : 4;
                int n = kind == 5 ? 2 : r.irange(1, 3);
                for (int i = 0; i < n; i++) {
                    vec2 o = r.inDisk() * 8.f;
                    vec3 sp{cc.x + o.x, 0, cc.y + o.y};
                    sp.y = groundAt(sp.x, sp.z);
                    float wl = waterAt(sp.x, sp.z);
                    if (wl > -1e8f && sp.y < wl + 0.3f) continue;
                    list.push_back(spawnAnimal(kind, sp));
                }
            }
            // 3.9.5 herds of mammoths in the cold north and on the high plateaus
            if ((region == R_FROSTHEIM || region == R_HIGHCROWNS) && !camp && r.f() < 0.07f) {
                int n = r.irange(2, 4);
                for (int i = 0; i < n; i++) {
                    vec2 o = r.inDisk() * 14.f;
                    vec3 sp{cc.x + o.x, 0, cc.y + o.y};
                    sp.y = groundAt(sp.x, sp.z);
                    float wl = waterAt(sp.x, sp.z);
                    if (wl > -1e8f && sp.y < wl + 0.3f) continue;
                    list.push_back(spawnAnimal(9, sp));
                }
            }
        }
    // ---- villages: named NPCs + generic villagers
    for (int i = 0; i < (int)world.pois.size(); i++) {
        const Poi& P = world.pois[i];
        if (P.type != PoiType::Village && P.type != PoiType::Monastery && P.type != PoiType::Mill && P.type != PoiType::Tower) continue;
        float d = length(P.pos - pp.xz());
        bool spawned = std::find(villageNpcsSpawned.begin(), villageNpcsSpawned.end(), i) != villageNpcsSpawned.end();
        if (d < 160.f && !spawned) {
            villageNpcsSpawned.push_back(i);
            const auto& N = npcs();
            for (int n = 0; n < (int)N.size(); n++)
                if (N[n].home == P.id && !(N[n].id == "alaric" && storyFlags.count("alaric_gone"))) {
                    vec3 sp{P.pos.x + N[n].offset.x, 0, P.pos.y + N[n].offset.y};
                    sp.y = groundAt(sp.x, sp.z);
                    spawnNpc(n, sp, i);
                }
            int generic = P.type == PoiType::Tower ? 0 : (P.type == PoiType::Village ? (P.city ? 30 : 12) : 5);   // 1.13.0 livelier villages (was 8); 3.0.0 the cities: 30
            if (P.id == "player_base") generic = base.residents;   // 1.9.2 the hero's domain: only the recruited inhabitants
            for (int g = 0; g < generic; g++) {
                uint32_t seed = hash2i(i, g, 555);
                Rng r(seed);
                vec3 sp{P.pos.x + r.range(-P.radius * 0.5f, P.radius * 0.5f), 0, P.pos.y + r.range(-P.radius * 0.5f, P.radius * 0.5f)};
                sp.y = groundAt(sp.x, sp.z);
                spawnVillager(sp, i, seed);
            }
            // a guard patrol at the edge of bigger settlements
        } else if (d > 240.f && spawned) {
            villageNpcsSpawned.erase(std::find(villageNpcsSpawned.begin(), villageNpcsSpawned.end(), i));
            reg.each<NpcBrain>([&](Entity e, NpcBrain& nb) { if (nb.village == i) reg.destroyLater(e); });
        }
    }
    // ---- boss arenas in the open world
    for (int b = 0; b < (int)bosses().size(); b++) {
        const BossDef& B = bosses()[b];
        if (B.dungeon >= 0 || defeatedBosses.count(B.id) || bossEnt != NullEntity || coop.mode == CoopState::Guest) continue;
        const Poi* P = world.poiById(B.arenaPoi);
        if (!P) continue;
        if (B.id == "fallen_knight" && !questActive("m6") && !questsDone.count("m6")) continue;  // Corwin waits for the crown bearer
        if (length(P->pos - pp.xz()) < P->radius * 0.75f) {
            vec3 bp{P->pos.x, P->height, P->pos.y};
            if (B.id == "fallen_knight") bp = vec3(P->pos.x, P->height, P->pos.y - P->radius * 0.3f);
            spawnBoss(b, bp);
        }
    }
    despawnFar();
}

void Game::despawnFar() {
    Character& p = pc();
    for (auto it = chunkSpawns.begin(); it != chunkSpawns.end();) {
        ChunkKey k{(int)(it->first / 4096), (int)(it->first % 4096)};
        vec2 cc{(k.x + 0.5f) * CHUNK, (k.z + 0.5f) * CHUNK};
        if (length(cc - p.pos.xz()) > 150.f) {
            bool engaged = false;
            for (Entity e : it->second) {
                AIBrain* ai = reg.get<AIBrain>(e);
                if (ai && (ai->state == AIState::Chase || ai->state == AIState::Attack)) engaged = true;
            }
            if (!engaged) {
                for (Entity e : it->second) reg.destroyLater(e);
                spawnedChunks.erase(it->first);
                it = chunkSpawns.erase(it);
                continue;
            }
        }
        ++it;
    }
    // harvest nodes / chests whose chunk was unloaded
    for (auto it = nodeEntities.begin(); it != nodeEntities.end();) {
        NodeComp* n = reg.get<NodeComp>(it->second);
        if (!n || distXZ(n->node.pos, p.pos) > 140.f) { reg.destroyLater(it->second); it = nodeEntities.erase(it); }
        else ++it;
    }
    // stray pickups
    reg.each<Pickup>([&](Entity e, Pickup& pk) { if (distXZ(pk.pos, p.pos) > 150.f || pk.t > 600.f) reg.destroyLater(e); });
}

}  // namespace lk

namespace lk {
// ============================================================================================ night hunters (1.9.2)
// After dark, outside settlements, creatures of the Abyss pick up the hero's trail: a howl warns, then one to
// three hunters arrive from out of sight and track him wherever he goes. Villages are safe; at dawn they dissolve.
bool Game::playerInSafeZone() {
    if (player == NullEntity) return true;
    vec2 p = reg.get<Character>(player)->pos.xz();
    for (const Poi& P : world.pois) {
        bool safe = P.type == PoiType::Village || P.type == PoiType::Monastery || P.type == PoiType::Mill || P.type == PoiType::Tower || P.type == PoiType::Farm;
        if (P.id == "player_base") safe = baseCount("tower") > 0;   // the domain is safe once a watchtower stands
        if (safe && length(P.pos - p) < P.radius + 8.f) return true;
    }
    return false;
}

void Game::updateNightHunters(float dt) {
    if (player == NullEntity || dungeon.active || state != GameState::Playing || coop.mode == CoopState::Guest) return;
    Character& pc_ = pc();
    // forget the dead / destroyed ones
    nightHunters.erase(std::remove_if(nightHunters.begin(), nightHunters.end(), [&](Entity h) { Character* c = reg.valid(h) ? reg.get<Character>(h) : nullptr; return !c || c->dead; }), nightHunters.end());
    bool night = env.night > 0.62f;
    bool safe = playerInSafeZone();
    hunterSafeT = safe ? hunterSafeT + dt : 0.f;
    // dawn, or the hero reached a village: the hunters dissolve into the dark
    if (!nightHunters.empty() && (env.night < 0.35f || hunterSafeT > 8.f || pc_.dead)) {
        for (Entity h : nightHunters) {
            Character* c = reg.get<Character>(h);
            if (!c) continue;
            c->dead = true; c->deathT = 1.4f; c->vis.dissolveColor = vec3(0.35f, 0.1f, 0.5f);
            c->vis.anim.setState(BodyState::Dead, 0.3f);
        }
        if (env.night < 0.35f && !pc_.dead && !safe) { huntsSurvived++; notify("L'aube chasse les traqueurs.", vec3(1.f, 0.85f, 0.6f), "i_star"); }
        nightHunters.clear();
        return;
    }
    // they always know where the hero is (scent), while he is out in the open at night
    for (Entity h : nightHunters) {
        AIBrain* ai = reg.get<AIBrain>(h);
        Character* c = reg.get<Character>(h);
        if (!ai || !c) continue;
        ai->lastSeen = pc_.pos; ai->aggro = 6.f; ai->target = player;
        if (ai->state == AIState::Idle || ai->state == AIState::Patrol || ai->state == AIState::Return || ai->state == AIState::Search) ai->state = AIState::Chase;
    }
    if (!night || safe || pc_.dead || riding) { hunterT = std::max(hunterT, 15.f); return; }
    hunterT -= dt;
    if (hunterT > 0 || nightHunters.size() >= 4) return;
    hunterT = particles.rng.range(70.f, 120.f);
    // a wave: howl first, then the hunters come out of sight, behind the hero
    int n = 1 + (level >= 6 ? 1 : 0) + (level >= 12 && particles.rng.chance(0.5f) ? 1 : 0);
    int hound = enemyIndex("abyss_hound"), stalker = enemyIndex("night_stalker");
    vec3 back = -vec3(std::sin(pc_.yaw), 0, std::cos(pc_.yaw));
    int spawned = 0;
    for (int i = 0; i < n * 4 && spawned < n; i++) {
        float a = std::atan2(back.x, back.z) + particles.rng.range(-1.2f, 1.2f);
        float d = particles.rng.range(36.f, 46.f);
        vec3 sp = pc_.pos + vec3(std::sin(a), 0, std::cos(a)) * d;
        sp.y = groundAt(sp.x, sp.z);
        float wl = waterAt(sp.x, sp.z);
        if (wl > -1e8f && sp.y < wl + 0.3f) continue;
        if (std::fabs(sp.y - pc_.pos.y) > 14.f) continue;
        int def = (spawned == 0 || particles.rng.chance(0.6f)) ? hound : stalker;
        Entity e = spawnEnemy(def, sp, false, 0);
        Character* c = reg.get<Character>(e);
        if (!c) continue;
        // they grow with the hero: a level 20 hero meets level 20 hunters
        const EnemyDef& D = enemies()[def];
        float lv = (float)std::max(D.level, level);
        float k = 1.f + 0.11f * (lv - D.level);
        c->level = (int)lv; c->maxHp = c->hp = D.hp * k; c->atk = D.atk * (1.f + 0.07f * (lv - D.level));
        c->yaw = std::atan2(pc_.pos.x - sp.x, pc_.pos.z - sp.z);
        if (AIBrain* ai = reg.get<AIBrain>(e)) { ai->state = AIState::Chase; ai->target = player; ai->lastSeen = pc_.pos; ai->aggro = 6.f; }
        particles.dissolveBurst(sp, vec3(0.35f, 0.1f, 0.5f), 1.2f);
        nightHunters.push_back(e);
        spawned++;
    }
    if (spawned) {
        play("wolf_howl", 0.9f, 0.75f);
        notify(spawned > 1 ? "Des traqueurs de la nuit ont trouvé votre piste…" : "Un traqueur de la nuit a trouvé votre piste…", vec3(0.75f, 0.45f, 1.f), "i_night");
        if (!storyFlags.count("hint_hunters")) { storyFlags.insert("hint_hunters"); toastHint = "La nuit, les traqueurs vous pistent partout. Les villages sont sûrs ; à l'aube, ils disparaissent."; }
        addShake(0.2f);
    }
}
}  // namespace lk
