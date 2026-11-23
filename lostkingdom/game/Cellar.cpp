// LOST KINGDOM — 3.6.0 the underground base. Craft a "Trappe de souterrain" at the workbench and set it down
// anywhere outdoors: a ladder leads down to a small chamber hewn in the rock, and from there you dig your own
// halls. Strike the rock face in front of you (a pickaxe cuts a block in one blow, anything else in three): the
// block becomes floor and gives stone, and the veins in the rock give iron, coal, silver, gold or crystal. Torches
// light the galleries as they grow. The rock is a 40 x 40 grid of 2 m blocks (the outer ring is bedrock); it is
// saved with the game. The cellar uses the indoor machinery of the buildings (an off-map room).
#include "Game.h"
#include "Props.h"

namespace lk {

static const int CW = 40;
static const float CT = 2.f, CH = 3.4f;
enum : uint8_t { CR_ROCK = 0, CR_FLOOR = 1, CR_IRON = 2, CR_COAL = 3, CR_GOLD = 4, CR_CRYSTAL = 5, CR_SILVER = 6, CR_BEDROCK = 9 };

void Game::initCellar(uint32_t seed) {
    cellar.seed = seed;
    cellar.cells.assign(CW * CW, CR_ROCK);
    Rng r(seed);
    for (int z = 0; z < CW; z++)
        for (int x = 0; x < CW; x++) {
            uint8_t& c = cellar.cells[z * CW + x];
            if (x == 0 || z == 0 || x == CW - 1 || z == CW - 1) { c = CR_BEDROCK; continue; }
            float n = fbm2(x * 0.21f + seed % 97, z * 0.21f, 3, seed) * 0.5f + 0.5f, m = fbm2(x * 0.33f, z * 0.33f + seed % 53, 2, seed + 7) * 0.5f + 0.5f;
            int dc = std::max(std::abs(x - CW / 2), std::abs(z - CW / 2));   // richer veins further from the ladder
            if (n > 0.72f) c = CR_COAL;
            else if (m > 0.74f) c = dc > 9 ? CR_SILVER : CR_IRON;
            else if (n < 0.24f && dc > 6) c = CR_IRON;
            else if (m < 0.2f && dc > 11) c = CR_GOLD;
            else if (r.chance(0.012f + dc * 0.0012f)) c = CR_CRYSTAL;
        }
    for (int z = CW / 2 - 1; z <= CW / 2 + 1; z++) for (int x = CW / 2 - 1; x <= CW / 2 + 1; x++) cellar.cells[z * CW + x] = CR_FLOOR;   // the first chamber
    cellar.mined = 0;
}

static vec3 cellarOrigin() { return vec3(WORLD_SIZE + 5200.f, 0.f, 600.f); }

// Meshes and colliders of the cellar from its grid (rebuilt after every block dug: small, a few milliseconds)
void Game::rebuildCellar() {
    DungeonState& d = dungeon;
    const vec3 O = cellarOrigin();
    auto cell = [&](int x, int z) -> uint8_t { if (x < 0 || z < 0 || x >= CW || z >= CW) return CR_BEDROCK; return cellar.cells[z * CW + x]; };
    auto open = [&](int x, int z) { return cell(x, z) == CR_FLOOR; };
    MeshBuilder F, Wm, P;
    d.colliders.clear(); d.lights.clear(); d.emitters.clear();
    Mat dirt(hexColor(0x5a4a3a), 0.95f), flag(hexColor(0x6e665a), 0.9f), cap(hexColor(0x241e18), 0.95f);
    static const uint32_t oreCol[] = {0, 0, 0xc07a5a, 0x1e1e22, 0xf0c040, 0x7ae8ff, 0xe0e4ec};
    // a dark underlay below the whole cellar: where the rock is made see-through near the camera, you see ground, not sky
    F.box(O + vec3(CW * CT * 0.5f, -0.2f, CW * CT * 0.5f), {CW * CT * 0.5f + 4.f, 0.05f, CW * CT * 0.5f + 4.f}, Mat(hexColor(0x0e0c0a), 0.95f));
    for (int z = 0; z < CW; z++)
        for (int x = 0; x < CW; x++) {
            uint8_t c = cell(x, z);
            vec3 ctr = O + vec3((x + 0.5f) * CT, 0, (z + 0.5f) * CT);
            Rng r(hash2i(x, z, cellar.seed));
            if (c == CR_FLOOR) {   // a floor of packed earth with a few flagstones
                F.box(ctr - vec3(0, 0.06f, 0), {CT * 0.5f, 0.06f, CT * 0.5f}, Mat(dirt.color * r.range(0.85f, 1.1f), 0.95f));
                for (int k = 0; k < 2; k++) if (r.chance(0.6f)) F.bevelBox(ctr + vec3(r.range(-0.5f, 0.5f), 0.01f, r.range(-0.5f, 0.5f)), {r.range(0.25f, 0.4f), 0.03f, r.range(0.2f, 0.35f)}, 0.02f, Mat(flag.color * r.range(0.85f, 1.1f), 0.9f), quat::axisAngle({0, 1, 0}, r.range(0, 1.5f)));
                if (r.chance(0.08f)) P.rock(ctr + vec3(r.range(-0.6f, 0.6f), 0.08f, r.range(-0.6f, 0.6f)), vec3(0.15f), Mat(hexColor(0x6a645c), 0.9f), r.next(), 0);   // rubble
                continue;
            }
            bool face = open(x - 1, z) || open(x + 1, z) || open(x, z - 1) || open(x, z + 1);
            // the top of the rock mass: dark ground seen from above (the rock is solid all around)
            Wm.box(ctr + vec3(0, CH, 0), {CT * 0.5f, 0.05f, CT * 0.5f}, Mat(cap.color * r.range(0.85f, 1.15f), 0.95f));
            if (!face) continue;
            // a rock face: a block with rough lumps on the open sides, veins of ore glinting in it
            vec3 rc = c == CR_BEDROCK ? hexColor(0x3a3634) : lerp(hexColor(0x6a6058), hexColor(0x7a6a5a), r.f());
            Wm.bevelBox(ctr + vec3(0, CH * 0.5f, 0), {CT * 0.5f, CH * 0.5f, CT * 0.5f}, 0.18f, Mat(rc, 0.92f));
            const int dx[4] = {1, -1, 0, 0}, dz[4] = {0, 0, 1, -1};
            for (int s = 0; s < 4; s++) {
                if (!open(x + dx[s], z + dz[s])) continue;
                vec3 n(dx[s], 0, dz[s]);
                for (int k = 0; k < 3; k++) Wm.rock(ctr + n * (CT * 0.5f) + vec3(dz[s] * r.range(-0.7f, 0.7f), r.range(0.4f, CH - 0.4f), dx[s] * r.range(-0.7f, 0.7f)), vec3(r.range(0.25f, 0.45f), r.range(0.25f, 0.5f), r.range(0.25f, 0.45f)), Mat(rc * r.range(0.85f, 1.1f), 0.92f), r.next(), 0);
                if (c != CR_ROCK && c != CR_BEDROCK) {
                    vec3 oc = hexColor(oreCol[c]);
                    Mat om = c == CR_CRYSTAL ? Mat(oc, 0.08f, 0.1f, 1.2f) : (c == CR_COAL ? Mat(oc, 0.5f, 0.3f) : Mat(oc, 0.25f, 0.9f, 0.08f));
                    bool fl = Wm.flat; Wm.flat = true;
                    for (int k = 0; k < 5; k++) { vec3 p = ctr + n * (CT * 0.5f + 0.05f) + vec3(dz[s] * r.range(-0.75f, 0.75f), r.range(0.3f, CH - 0.5f), dx[s] * r.range(-0.75f, 0.75f));
                        if (c == CR_CRYSTAL) Wm.cone(p, p + n * 0.35f + vec3(0, 0.2f, 0), 0.08f, om, 5); else Wm.rock(p, vec3(r.range(0.08f, 0.15f)), om, r.next(), 0); }
                    Wm.flat = fl;
                    if (c == CR_CRYSTAL && s == 0) d.lights.push_back(vec4(ctr + n * CT * 0.7f + vec3(0, 1.4f, 0), LI_CRYSTAL));
                }
                // a torch on the wall every few blocks
                if (((x * 7 + z * 13) % 5) == 0 && c != CR_BEDROCK) {
                    vec3 tp = ctr + n * (CT * 0.5f + 0.12f) + vec3(0, 2.1f, 0);
                    P.tube(tp - vec3(0, 0.35f, 0), tp, 0.03f, 0.04f, Mat(hexColor(0x4a3220), 0.9f), 5, true);
                    P.ellipsoid(tp + vec3(0, 0.1f, 0), vec3(0.08f, 0.14f, 0.08f), Mat(hexColor(0xffa040), 0.5f, 0, 4.f), 6, 4);
                    d.lights.push_back(vec4(tp + n * 0.4f + vec3(0, 0.2f, 0), LI_FIRE));
                    d.emitters.push_back(vec4(tp + vec3(0, 0.15f, 0), EM_FIRE));
                }
            }
            Collider col; col.c = ctr.xz(); col.hx = CT * 0.5f; col.hz = CT * 0.5f; col.base = -1; col.top = CH + 1; d.colliders.push_back(col);
        }
    // the ladder up to the hatch, in a shaft of daylight
    vec3 L = O + vec3((CW / 2 + 0.5f) * CT, 0, (CW / 2 + 0.5f) * CT);
    Mat wood(hexColor(0x7a5434), 0.85f);
    for (int s = -1; s <= 1; s += 2) P.tube(L + vec3(s * 0.32f, 0, -0.3f), L + vec3(s * 0.32f, CH + 0.6f, -0.3f), 0.05f, 0.05f, wood, 6, true);
    for (int k = 0; k < 9; k++) P.box(L + vec3(0, 0.3f + k * 0.4f, -0.3f), {0.32f, 0.03f, 0.04f}, wood);
    P.box(L + vec3(0, CH + 0.02f, 0), {0.9f, 0.04f, 0.9f}, Mat(hexColor(0xfff4d8), 0.5f, 0, 1.2f));   // the open hatch above
    d.lights.push_back(vec4(L + vec3(0, 2.6f, 0), LI_LANTERN));
    d.exitPos = L + vec3(0, 0, 0.1f);
    if (d.floor.vao) d.floor.destroy();
    if (d.walls.vao) d.walls.destroy();
    if (d.props.vao) d.props.destroy();
    F.upload(d.floor); Wm.upload(d.walls); P.upload(d.props);
}

void Game::buildCellar() {
    DungeonState& d = dungeon;
    d = DungeonState();
    d.interior = true; d.interiorKind = -2; d.interiorStruct = -1; d.theme = 9;
    d.interiorRegion = world.regionId(cellar.hatch.x, cellar.hatch.z); d.interiorName = "Souterrain";
    d.origin = cellarOrigin();
    d.W = (int)(CW * CT / 4.f) + 2; d.H = d.W;
    d.tiles.assign(d.W * d.H, TileType::Floor);
    if (cellar.cells.size() != (size_t)(CW * CW)) initCellar(cellar.seed);
    rebuildCellar();
    d.entrance = d.exitPos + vec3(0, 0, 1.6f);
}

void Game::enterCellar() {
    if (!cellar.owned) return;
    if (riding) toggleMount();
    digMode = false;
    vec3 out = cellar.hatch + vec3(1.4f, 0, 0);
    fadeTransition([this, out] {
        buildCellar();
        dungeon.active = true;
        dungeon.returnPos = out;
        Character& c = pc();
        c.pos = dungeon.entrance; c.vel = vec3(0); c.yaw = 0;
        camFocus = c.pos; camYaw = 0.6f; camPitch = std::max(camPitch, 0.8f); camDist = camDistTarget = 9.f;
        regionBanner = "Souterrain"; regionBannerSub = "Frappez la roche pour creuser"; regionBannerT = 2.5f;
        play("door", 0.7f, 0.8f);
    });
}

bool Game::inCellar() const { return dungeon.active && dungeon.interior && dungeon.interiorKind == -2; }

// a blow at the rock face in front of the hero
bool Game::mineCellar(Character& c) {
    if (!inCellar()) return false;
    const vec3 O = cellarOrigin();
    vec3 fwd{std::sin(c.yaw), 0, std::cos(c.yaw)};
    int bx = -1, bz = -1;
    for (float reach = 0.8f; reach <= 3.0f && bx < 0; reach += 0.2f) {
        vec3 p = c.pos + fwd * reach;
        int x = (int)std::floor((p.x - O.x) / CT), z = (int)std::floor((p.z - O.z) / CT);
        if (x < 0 || z < 0 || x >= CW || z >= CW) break;
        if (cellar.cells[z * CW + x] != CR_FLOOR) { bx = x; bz = z; }
    }
    if (bx < 0) return false;
    uint8_t& cl = cellar.cells[bz * CW + bx];
    vec3 face = O + vec3((bx + 0.5f) * CT, 1.2f, (bz + 0.5f) * CT) - fwd * (CT * 0.5f);
    if (cl == CR_BEDROCK) { particles.sparks(face, -fwd, 6, vec3(0.8f, 0.8f, 0.9f)); play("hit_metal", 0.6f, 0.7f); floatText(face + vec3(0, 0.5f, 0), "Roche-mère", vec3(0.8f, 0.8f, 0.9f), 18); return true; }
    bool pick = c.vis.wt == WeaponType::Pickaxe || countItem("tool_pickaxe") > 0 || equipment[SL_WEAPON].id == "tool_pickaxe";   // you take out your pickaxe
    int need = pick ? 1 : 3;
    int key = bz * CW + bx;
    int& hits = cellarHits[key];
    hits++;
    particles.dust(face, 0.7f, vec3(0.5f, 0.45f, 0.4f));
    particles.sparks(face, -fwd + vec3(0, 0.4f, 0), 8, vec3(0.75f, 0.7f, 0.6f));
    play("harvest_stone", 0.7f, particles.rng.range(0.85f, 1.05f));
    addShake(0.06f); haptic(20, 110);
    if (hits < need) return true;
    cellarHits.erase(key);
    uint8_t was = cl;
    cl = CR_FLOOR;
    cellar.mined++;
    addItem("stone", 1 + (int)particles.rng.irange(0, 1));
    const char* ore = was == CR_IRON ? "iron_ore" : was == CR_COAL ? "coal" : was == CR_GOLD ? "gold_ore" : was == CR_CRYSTAL ? "crystal" : was == CR_SILVER ? "silver_ore" : nullptr;
    if (ore) { int n = particles.rng.irange(1, 3); addItem(ore, n); floatText(face + vec3(0, 0.6f, 0), "+" + std::to_string(n) + " " + item(ore)->name, vec3(1.f, 0.9f, 0.5f), 20); }
    gainXp(3 + (ore ? 4 : 0));
    particles.dust(face, 1.4f, vec3(0.5f, 0.45f, 0.4f));
    play("harvest_stone", 0.9f, 0.7f);
    rebuildCellar();
    return true;
}

bool Game::placeCellarHatch() {
    Character& c = pc();
    if (dungeon.active) { notify("Posez la trappe dehors, sur la terre ferme.", vec3(1.f, 0.7f, 0.5f), "i_lock"); return false; }
    float wl = waterAt(c.pos.x, c.pos.z);
    if (wl > c.pos.y - 0.3f || !c.grounded) { notify("Il faut un sol sec pour creuser.", vec3(1.f, 0.7f, 0.5f), "i_lock"); return false; }
    vec3 at = c.pos + vec3(std::sin(c.yaw), 0, std::cos(c.yaw)) * 1.5f;
    at.y = groundAt(at.x, at.z);
    bool first = !cellar.owned;
    cellar.owned = true; cellar.hatch = at;
    if (first) initCellar(hash2i((int)at.x, (int)at.z, 7717));
    spawnCellarHatch();
    particles.dust(at, 1.5f); play("harvest_stone", 0.8f);
    itemGet("i_pickaxe", first ? "Souterrain creusé" : "Trappe déplacée", "Votre souterrain", "Descendez par la trappe, puis frappez la roche pour creuser vos galeries. Les filons donnent du minerai.", vec3(0.9f, 0.75f, 0.5f), first);
    return true;
}

void Game::spawnCellarHatch() {
    if (reg.valid(cellarEnt)) reg.destroy(cellarEnt);
    cellarEnt = NullEntity;
    if (!cellar.owned) return;
    cellarEnt = reg.create();
    Interact it; it.kind = InterKind::Cellar; it.radius = 2.f; it.text = "Descendre au souterrain";
    reg.add<Interact>(cellarEnt, it);
}

void Game::renderCellarHatch() {
    if (!cellar.owned || dungeon.active || distXZ(cellar.hatch, camFocus) > 80.f) return;
    static std::shared_ptr<Mesh> m;
    if (!m) m = cachedMesh("cellar_hatch", [](MeshBuilder& b) {
        Mat wood(hexColor(0x7a5434), 0.85f), dark(hexColor(0x4a3220), 0.9f), iron(hexColor(0x3a3a40), 0.4f, 0.8f), stone(hexColor(0x7a746c), 0.9f);
        for (int s = 0; s < 4; s++) { float a = s * PI / 2; vec3 n(std::cos(a), 0, std::sin(a)); b.bevelBox(n * 0.95f + vec3(0, 0.08f, 0), {std::fabs(n.z) * 0.85f + 0.12f, 0.1f, std::fabs(n.x) * 0.85f + 0.12f}, 0.03f, stone); }
        for (int k = -3; k <= 3; k++) b.box({k * 0.24f, 0.12f, 0}, {0.11f, 0.04f, 0.82f}, k % 2 ? wood : dark);
        for (int s = -1; s <= 1; s += 2) b.box({0, 0.165f, s * 0.5f}, {0.82f, 0.012f, 0.05f}, iron);
        b.lathe({{0.09f, -0.012f}, {0.09f, 0.012f}, {0.07f, 0.012f}, {0.07f, -0.012f}}, iron, 14, {0.3f, 0.19f, 0}, quat::axisAngle({0, 0, 1}, 0.3f));
    });
    DrawItem d; d.mesh = m.get(); d.model = mat4::translate(cellar.hatch); renderer.submit(d);
}

std::string Game::saveCellar() const {
    if (!cellar.owned) return "";
    std::string s = "cellar " + std::to_string(cellar.hatch.x) + " " + std::to_string(cellar.hatch.y) + " " + std::to_string(cellar.hatch.z) + " " + std::to_string(cellar.seed) + " " + std::to_string(cellar.mined) + " ";
    for (uint8_t c : cellar.cells) s += (char)('0' + c);
    return s + "\n";
}

void Game::loadCellar(std::istringstream& l) {
    std::string cells;
    l >> cellar.hatch.x >> cellar.hatch.y >> cellar.hatch.z >> cellar.seed >> cellar.mined >> cells;
    cellar.owned = true;
    if (cells.size() == (size_t)(CW * CW)) { cellar.cells.assign(CW * CW, 0); for (int i = 0; i < CW * CW; i++) cellar.cells[i] = (uint8_t)std::max(0, cells[i] - '0'); }
    else initCellar(cellar.seed);
}

}  // namespace lk
