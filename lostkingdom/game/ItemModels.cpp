// LOST KINGDOM — 3.5.0 a 3D model for every item. Weapons, shields and tools use the models the hero holds
// (buildWeapon); everything else is modelled here, recognised by its id: potions in coloured flasks with a cork
// and a highlight, elixirs in cut-glass bottles caged in gold, food, ores with veins of metal, ingots, crystals,
// hides and cloth, rings and amulets with cut gems, helmets, cuirasses, gauntlets and boots in the item's colour,
// relics, quest objects, key items and the bags. Used for the inventory icons (rendered once into an atlas), the
// rotating showcase of the selected item and the items lying on the ground.
#include "Game.h"
#include "Characters.h"
#include <cstring>

namespace lk {

namespace {

bool has(const std::string& s, const char* k) { return s.find(k) != std::string::npos; }

const Mat GOLD(hexColor(0xe0b85a), 0.28f, 0.95f), SILVER(hexColor(0xd8dce4), 0.22f, 0.95f), IRON(hexColor(0x6a6c74), 0.4f, 0.85f);
const Mat BRASS(hexColor(0xc89a4a), 0.3f, 0.9f), WOODM(hexColor(0x8a5a32), 0.75f), DARKWOOD(hexColor(0x5a3a22), 0.8f);
const Mat LEATHER(hexColor(0x7a4a2a), 0.85f), CLOTHW(hexColor(0xe8dcc0), 0.9f), CORK(hexColor(0xb08a5a), 0.9f);
const Mat GLASS(hexColor(0xdff4f4), 0.04f, 0.1f, 0.05f);

Mat glow(vec3 c, float e) { return Mat(c, 0.15f, 0.f, e); }
Mat gem(vec3 c) { return Mat(c, 0.08f, 0.2f, 0.45f); }

// a cut gem: faceted, pointed below
void cutGem(MeshBuilder& b, vec3 c, float r, vec3 col, quat q = quat()) {
    bool f = b.flat; b.flat = true;
    b.lathe({{0.01f, -r * 0.9f}, {r, -r * 0.05f}, {r * 0.95f, r * 0.12f}, {r * 0.55f, r * 0.42f}, {0.01f, r * 0.44f}}, gem(col), 8, c, q);
    b.flat = f;
}

void torus(MeshBuilder& b, vec3 c, float R, float r, const Mat& m, quat q = quat(), int seg = 24) {
    std::vector<vec2> prof;
    for (int i = 0; i <= 10; i++) { float a = i * TAU / 10; prof.push_back({R + std::cos(a) * r, std::sin(a) * r}); }
    b.lathe(prof, m, seg, c, q);
}

// ---- potions & elixirs
vec3 liquidColour(const std::string& id) {
    if (has(id, "mana") || has(id, "frost")) return hexColor(0x3a8aff);
    if (has(id, "stamina") || has(id, "tea") || has(id, "cactus")) return hexColor(0x5ad060);
    if (has(id, "strength") || has(id, "ember")) return hexColor(0xff7a2a);
    if (has(id, "sun")) return hexColor(0xffd040);
    if (has(id, "titan")) return hexColor(0x9a5aff);
    if (has(id, "phoenix")) return hexColor(0xff5020);
    if (has(id, "venom")) return hexColor(0x9ae040);
    return hexColor(0xe8283a);
}

void potion(MeshBuilder& b, vec3 col, int size) {   // size 0 small, 1 normal, 2 great
    float k = 0.85f + 0.15f * size;
    Mat liq(col, 0.12f, 0.f, 0.55f);
    b.lathe({{0.01f, 0}, {0.09f * k, 0.004f}, {0.135f * k, 0.06f * k}, {0.14f * k, 0.12f * k}, {0.11f * k, 0.18f * k}, {0.05f, 0.21f * k}, {0.01f, 0.215f * k}}, liq, 16);
    b.lathe({{0.045f, 0.2f * k}, {0.042f, 0.27f * k}, {0.055f, 0.28f * k}, {0.05f, 0.295f * k}, {0.01f, 0.296f * k}}, GLASS, 12);
    b.tube({0, 0.27f * k, 0}, {0, 0.33f * k, 0}, 0.04f, 0.044f, CORK, 10, true);
    b.ellipsoid({-0.075f * k, 0.12f * k, 0.09f * k}, vec3(0.022f, 0.05f, 0.012f) * k, Mat(vec3(1.f), 0.05f, 0, 1.2f), 6, 4, quat::axisAngle({0, 0, 1}, 0.3f));   // highlight
    b.lathe({{0.142f * k, 0.085f * k}, {0.145f * k, 0.1f * k}, {0.142f * k, 0.115f * k}}, size == 2 ? GOLD : Mat(hexColor(0xe8d8b0), 0.8f), 16);   // label band
    if (size == 2) cutGem(b, {0, 0.1f * k, 0.145f * k}, 0.03f, col * 1.2f, quat::axisAngle({1, 0, 0}, PI / 2));
}

void elixir(MeshBuilder& b, vec3 col) {   // a cut-glass bottle in a gold cage, a star stopper
    bool f = b.flat; b.flat = true;
    b.lathe({{0.01f, 0}, {0.08f, 0.01f}, {0.12f, 0.12f}, {0.08f, 0.26f}, {0.04f, 0.3f}, {0.01f, 0.31f}}, Mat(col, 0.08f, 0.1f, 0.6f), 6);
    b.flat = f;
    for (int i = 0; i < 6; i++) { float a = i * TAU / 6; b.tube({std::cos(a) * 0.085f, 0.01f, std::sin(a) * 0.085f}, {std::cos(a) * 0.125f, 0.12f, std::sin(a) * 0.125f}, 0.008f, 0.008f, GOLD, 4, false);
        b.tube({std::cos(a) * 0.125f, 0.12f, std::sin(a) * 0.125f}, {std::cos(a) * 0.045f, 0.29f, std::sin(a) * 0.045f}, 0.008f, 0.008f, GOLD, 4, false); }
    torus(b, {0, 0.12f, 0}, 0.124f, 0.012f, GOLD);
    b.tube({0, 0.29f, 0}, {0, 0.36f, 0}, 0.03f, 0.035f, GOLD, 8, true);
    for (int i = 0; i < 5; i++) { float a = i * TAU / 5; b.cone({0, 0.42f, 0}, vec3(std::cos(a) * 0.07f, 0.42f + std::sin(a) * 0.07f, 0), 0.025f, glow(lerp(col, vec3(1.f), 0.5f), 1.5f), 4); }
    b.ellipsoid({0, 0.42f, 0}, vec3(0.03f), glow(vec3(1.f, 0.95f, 0.8f), 2.f), 8, 5);
}

void vial(MeshBuilder& b, vec3 col) {
    b.tube({0, 0, 0}, {0, 0.22f, 0}, 0.05f, 0.05f, Mat(col, 0.1f, 0, 0.6f), 12, true);
    b.tube({0, 0.2f, 0}, {0, 0.27f, 0}, 0.04f, 0.04f, GLASS, 12, false);
    b.tube({0, 0.25f, 0}, {0, 0.3f, 0}, 0.045f, 0.045f, CORK, 10, true);
    torus(b, {0, 0.24f, 0}, 0.052f, 0.01f, BRASS);
}

// ---- food
void bread(MeshBuilder& b) {
    Mat crust(hexColor(0xb87a3a), 0.85f), top(hexColor(0xd89a50), 0.8f);
    b.ellipsoid({0, 0.09f, 0}, vec3(0.2f, 0.1f, 0.12f), crust, 16, 10);
    for (int i = -1; i <= 1; i++) b.box({i * 0.08f, 0.185f, 0}, {0.012f, 0.01f, 0.07f}, Mat(hexColor(0xf0d8a0), 0.9f), quat::axisAngle({0, 1, 0}, 0.5f));
    b.ellipsoid({0, 0.13f, 0}, vec3(0.17f, 0.06f, 0.1f), top, 12, 6);
}
void bowl(MeshBuilder& b, vec3 soup, bool fish) {
    Mat clay(hexColor(0x8a4a2a), 0.7f);
    b.lathe({{0.01f, 0}, {0.1f, 0.0f}, {0.16f, 0.06f}, {0.2f, 0.14f}, {0.19f, 0.15f}, {0.14f, 0.07f}, {0.01f, 0.06f}}, clay, 18);
    b.lathe({{0.18f, 0.12f}, {0.01f, 0.125f}}, Mat(soup, 0.3f), 18);
    Rng r(fish ? 7 : 3);
    for (int i = 0; i < 6; i++) { vec2 p = r.inDisk() * 0.12f; b.ellipsoid({p.x, 0.13f, p.y}, vec3(0.025f, 0.018f, 0.02f), Mat(i % 2 ? hexColor(0xe89a3a) : (fish ? hexColor(0xf0e0d0) : hexColor(0x7a3a2a)), 0.6f), 6, 4); }
    b.tube({0.05f, 0.13f, -0.06f}, {0.22f, 0.32f, -0.12f}, 0.012f, 0.01f, WOODM, 6, true);   // spoon
    b.ellipsoid({0.05f, 0.13f, -0.06f}, vec3(0.04f, 0.012f, 0.03f), WOODM, 8, 4);
}
void fishModel(MeshBuilder& b, vec3 col, bool cooked) {
    Mat skin(col, cooked ? 0.6f : 0.25f, cooked ? 0.f : 0.4f), belly(lerp(col, vec3(1.f), 0.55f), 0.4f);
    b.ellipsoid({0, 0.08f, 0}, vec3(0.06f, 0.08f, 0.2f), skin, 14, 8);
    b.ellipsoid({0, 0.055f, 0.02f}, vec3(0.05f, 0.04f, 0.16f), belly, 12, 6);
    b.blade({0, 0.08f, -0.18f}, {0, 0.17f, -0.3f}, 0.1f, {1, 0, 0}, skin, skin);
    b.blade({0, 0.08f, -0.18f}, {0, -0.01f, -0.3f}, 0.1f, {1, 0, 0}, skin, skin);
    b.blade({0, 0.15f, 0}, {0, 0.2f, -0.1f}, 0.09f, {0, 0, 1}, skin, skin);
    for (int s = -1; s <= 1; s += 2) b.ellipsoid({s * 0.045f, 0.1f, 0.13f}, vec3(0.015f), Mat(cooked ? hexColor(0xe8e0d0) : hexColor(0x101010), 0.2f), 6, 4);
    if (cooked) for (int i = -2; i <= 2; i++) b.box({0, 0.08f, i * 0.05f}, {0.062f, 0.075f, 0.005f}, Mat(hexColor(0x3a2010), 0.9f));   // grill marks
}
void meat(MeshBuilder& b, bool cooked) {
    b.ellipsoid({0, 0.08f, 0.05f}, vec3(0.11f, 0.09f, 0.13f), Mat(cooked ? hexColor(0x8a4a24) : hexColor(0xc0444a), 0.6f), 14, 8);
    b.tube({0, 0.08f, -0.06f}, {0, 0.1f, -0.22f}, 0.025f, 0.022f, Mat(hexColor(0xf0e8d8), 0.5f), 8, false);
    for (int s = -1; s <= 1; s += 2) b.ellipsoid({s * 0.02f, 0.1f, -0.23f}, vec3(0.03f), Mat(hexColor(0xf0e8d8), 0.5f), 8, 5);
}
void cluster(MeshBuilder& b, vec3 col, int n, float r, uint32_t seed, bool leaf) {
    Rng rg(seed);
    for (int i = 0; i < n; i++) { vec3 p = rg.onSphere() * r * 0.9f; p.y = std::fabs(p.y) * 0.8f + r; b.ellipsoid(p, vec3(r * 0.55f), Mat(col * rg.range(0.85f, 1.1f), 0.3f), 8, 5); }
    if (leaf) { b.blade({0, r * 1.6f, 0}, {0.12f, r * 2.2f, 0.05f}, 0.08f, {0, 0, 1}, Mat(hexColor(0x3a7a2a)), Mat(hexColor(0x5aa03a)));
                b.blade({0, r * 1.6f, 0}, {-0.1f, r * 2.3f, -0.04f}, 0.07f, {0, 0, 1}, Mat(hexColor(0x3a7a2a)), Mat(hexColor(0x5aa03a))); }
}

// ---- materials
void logs(MeshBuilder& b) {
    Mat bark(hexColor(0x6a4428), 0.9f), ring(hexColor(0xd8b07a), 0.8f);
    vec3 P[3] = {{-0.09f, 0.08f, 0}, {0.09f, 0.08f, 0}, {0, 0.22f, 0}};
    for (vec3 p : P) {
        b.tube(p + vec3(0, 0, -0.25f), p + vec3(0, 0, 0.25f), 0.085f, 0.085f, bark, 10, false);
        for (int s = -1; s <= 1; s += 2) { b.lathe({{0.08f, 0}, {0.05f, 0.003f}, {0.01f, 0.004f}}, ring, 10, p + vec3(0, 0, s * 0.25f), quat::axisAngle({1, 0, 0}, s * PI / 2)); }
    }
    b.tube({-0.2f, 0.15f, 0}, {0.2f, 0.15f, 0}, 0.012f, 0.012f, Mat(hexColor(0xc8a870), 0.9f), 6, false);   // a twine
}
void ore(MeshBuilder& b, vec3 vein, bool metal, uint32_t seed) {
    b.rock({0, 0.13f, 0}, vec3(0.2f, 0.14f, 0.17f), Mat(hexColor(0x6a6660), 0.9f), seed, 1);
    Rng r(seed * 7 + 1);
    bool f = b.flat; b.flat = true;
    for (int i = 0; i < 6; i++) { vec3 d = r.onSphere(); d.y = std::fabs(d.y) * 0.7f + 0.2f; d = normalize(d);
        b.rock(vec3(0, 0.13f, 0) + vec3(d.x * 0.18f, d.y * 0.12f, d.z * 0.15f), vec3(r.range(0.04f, 0.07f)), metal ? Mat(vein, 0.25f, 0.95f, 0.05f) : Mat(vein, 0.3f, 0.f, 0.35f), seed + i, 0); }
    b.flat = f;
}
void ingot(MeshBuilder& b, const Mat& m, bool glowing) {
    for (int k = 0; k < 2; k++) {
        float y = k * 0.085f; quat q = quat::axisAngle({0, 1, 0}, k * 0.25f);
        b.xf = mat4::trs({k * 0.03f, y, k * 0.02f}, q, vec3(1));
        b.quadP({-0.2f, 0.08f, -0.08f}, {-0.2f, 0.08f, 0.08f}, {0.2f, 0.08f, 0.08f}, {0.2f, 0.08f, -0.08f}, m);   // top
        b.quadP({-0.24f, 0, 0.11f}, {0.24f, 0, 0.11f}, {0.2f, 0.08f, 0.08f}, {-0.2f, 0.08f, 0.08f}, m);
        b.quadP({0.24f, 0, -0.11f}, {-0.24f, 0, -0.11f}, {-0.2f, 0.08f, -0.08f}, {0.2f, 0.08f, -0.08f}, m);
        b.quadP({0.24f, 0, 0.11f}, {0.24f, 0, -0.11f}, {0.2f, 0.08f, -0.08f}, {0.2f, 0.08f, 0.08f}, m);
        b.quadP({-0.24f, 0, -0.11f}, {-0.24f, 0, 0.11f}, {-0.2f, 0.08f, 0.08f}, {-0.2f, 0.08f, -0.08f}, m);
        b.box({0, 0.082f, 0}, {0.06f, 0.004f, 0.03f}, Mat(m.color * 0.7f, m.rough, m.metal, glowing ? 2.f : 0.f));   // the smith's stamp
    }
    b.xf = mat4();
}
void crystals(MeshBuilder& b, vec3 col, float emis, uint32_t seed) {
    Rng r(seed);
    b.rock({0, 0.03f, 0}, vec3(0.16f, 0.06f, 0.13f), Mat(hexColor(0x5a5660), 0.9f), seed, 0);
    bool f = b.flat; b.flat = true;
    for (int i = 0; i < 6; i++) {
        vec3 d = i == 0 ? vec3(0, 1, 0) : normalize(vec3(r.range(-0.7f, 0.7f), 1.f, r.range(-0.7f, 0.7f)));
        float h = i == 0 ? 0.36f : r.range(0.14f, 0.26f), w = i == 0 ? 0.06f : r.range(0.03f, 0.05f);
        vec3 base = vec3(d.x * 0.05f, 0.04f, d.z * 0.05f);
        b.tube(base, base + d * h * 0.75f, w, w, Mat(col * r.range(0.85f, 1.1f), 0.06f, 0.1f, emis), 6, false);
        b.cone(base + d * h * 0.75f, base + d * h, w, Mat(lerp(col, vec3(1.f), 0.3f), 0.06f, 0.1f, emis * 1.2f), 6);
    }
    b.flat = f;
}
void starShard(MeshBuilder& b) {
    bool f = b.flat; b.flat = true;
    for (int i = 0; i < 5; i++) { float a = i * TAU / 5 + PI / 2; vec3 tip(std::cos(a) * 0.2f, 0.2f + std::sin(a) * 0.2f, 0);
        for (int s = -1; s <= 1; s += 2) b.triP({0, 0.2f, s * 0.05f}, vec3(std::cos(a - 0.6f) * 0.08f, 0.2f + std::sin(a - 0.6f) * 0.08f, 0), tip, glow(hexColor(0xb8a0ff), 1.6f)),
            b.triP({0, 0.2f, s * 0.05f}, tip, vec3(std::cos(a + 0.6f) * 0.08f, 0.2f + std::sin(a + 0.6f) * 0.08f, 0), glow(hexColor(0xd8c8ff), 1.8f)); }
    b.flat = f;
    b.ellipsoid({0, 0.2f, 0}, vec3(0.05f), glow(vec3(1.f), 2.5f), 8, 5);
}
void hide(MeshBuilder& b, vec3 col, bool fur) {
    Rng r(fur ? 5 : 9);
    b.xf = mat4();
    for (int k = 0; k < 3; k++) b.bevelBox({k * 0.01f, 0.025f + k * 0.04f, k * -0.01f}, {0.2f - k * 0.02f, 0.02f, 0.15f - k * 0.015f}, 0.015f, Mat(col * (1.f - k * 0.08f), 0.9f), quat::axisAngle({0, 1, 0}, k * 0.15f));
    if (fur) for (int i = 0; i < 14; i++) { vec2 p = r.inDisk(); b.cone({p.x * 0.15f, 0.14f, p.y * 0.1f}, {p.x * 0.17f, 0.2f, p.y * 0.12f}, 0.025f, Mat(col * 1.15f, 0.95f), 4); }
    b.tube({-0.2f, 0.09f, 0}, {0.2f, 0.09f, 0}, 0.01f, 0.01f, Mat(hexColor(0x3a2a1a)), 5, false);
}
void clothRoll(MeshBuilder& b, vec3 col) {
    b.tube({-0.2f, 0.1f, 0}, {0.2f, 0.1f, 0}, 0.1f, 0.1f, Mat(col, 0.95f), 16, true);
    b.box({0.05f, 0.005f, 0.08f}, {0.16f, 0.005f, 0.1f}, Mat(col, 0.95f));
    for (int s = -1; s <= 1; s += 2) torus(b, {s * 0.12f, 0.1f, 0}, 0.102f, 0.008f, Mat(hexColor(0x8a5a2a)), quat::axisAngle({0, 0, 1}, PI / 2), 16);
}
void shell(MeshBuilder& b, vec3 col) {
    for (int i = 0; i < 9; i++) { float a = (i - 4) * 0.22f; b.ellipsoid({std::sin(a) * 0.1f, 0.03f, std::cos(a) * 0.1f - 0.05f}, vec3(0.035f, 0.025f, 0.13f), Mat(col * (i % 2 ? 1.f : 0.88f), 0.35f), 8, 5, quat::axisAngle({0, 1, 0}, a)); }
    b.ellipsoid({0, 0.02f, -0.12f}, vec3(0.05f, 0.03f, 0.03f), Mat(col * 0.8f, 0.4f), 8, 4);
}
void coins(MeshBuilder& b, const Mat& m) {
    for (int i = 0; i < 5; i++) b.lathe({{0.09f, 0}, {0.09f, 0.018f}, {0.075f, 0.022f}, {0.01f, 0.022f}}, m, 18, {i * 0.006f, i * 0.02f, i * -0.004f});
    b.lathe({{0.09f, 0}, {0.09f, 0.018f}, {0.075f, 0.022f}, {0.01f, 0.022f}}, m, 18, {0.17f, 0.06f, 0.02f}, quat::axisAngle({0, 0, 1}, 1.2f));
    b.box({0.01f, 0.102f, 0}, {0.03f, 0.003f, 0.03f}, Mat(m.color * 0.7f, 0.4f, 0.9f), quat::axisAngle({0, 1, 0}, 0.78f));
}
void sack(MeshBuilder& b, vec3 col, bool starry) {
    Mat c(col, 0.95f);
    b.lathe({{0.01f, 0}, {0.13f, 0.01f}, {0.17f, 0.1f}, {0.15f, 0.2f}, {0.07f, 0.26f}, {0.05f, 0.28f}}, c, 14);
    for (int i = 0; i < 7; i++) { float a = i * TAU / 7; b.cone({std::cos(a) * 0.05f, 0.27f, std::sin(a) * 0.05f}, {std::cos(a) * 0.1f, 0.34f, std::sin(a) * 0.1f}, 0.03f, c, 4); }
    torus(b, {0, 0.27f, 0}, 0.055f, 0.012f, Mat(hexColor(0xc8a870), 0.8f), quat(), 12);
    b.tube({0.05f, 0.27f, 0}, {0.1f, 0.16f, 0.05f}, 0.007f, 0.007f, Mat(hexColor(0xc8a870), 0.8f), 4, true);
    if (starry) { Rng r(3); for (int i = 0; i < 12; i++) { vec3 d = r.onSphere(); d.y = std::fabs(d.y) * 0.6f; b.ellipsoid(vec3(d.x * 0.155f, 0.1f + d.y * 0.1f, d.z * 0.155f), vec3(0.012f), glow(vec3(1.f, 0.95f, 0.8f), 3.f), 4, 3); } }
    else b.box({0, 0.12f, 0.16f}, {0.05f, 0.04f, 0.005f}, LEATHER);
}
void scroll(MeshBuilder& b, vec3 seal) {
    Mat paper(hexColor(0xead8b0), 0.9f);
    b.tube({-0.2f, 0.06f, 0}, {0.2f, 0.06f, 0}, 0.06f, 0.06f, paper, 14, true);
    for (int s = -1; s <= 1; s += 2) { b.tube({s * 0.2f, 0.06f, 0}, {s * 0.24f, 0.06f, 0}, 0.02f, 0.02f, DARKWOOD, 8, true); b.ellipsoid({s * 0.25f, 0.06f, 0}, vec3(0.03f), DARKWOOD, 8, 5); }
    b.box({0.06f, 0.003f, 0.09f}, {0.16f, 0.003f, 0.08f}, paper);
    b.tube({0, 0.06f, -0.062f}, {0, 0.06f, 0.062f}, 0.07f, 0.07f, Mat(hexColor(0xa82a2a), 0.7f), 14, false);   // ribbon
    b.lathe({{0.035f, 0}, {0.03f, 0.012f}, {0.01f, 0.014f}}, Mat(seal, 0.5f), 10, {0, 0.06f, 0.07f}, quat::axisAngle({1, 0, 0}, PI / 2));
}
void book(MeshBuilder& b, vec3 cover) {
    b.bevelBox({0, 0.045f, 0}, {0.17f, 0.045f, 0.22f}, 0.01f, Mat(cover, 0.8f));
    b.box({0.008f, 0.045f, 0}, {0.16f, 0.035f, 0.21f}, Mat(hexColor(0xf0e4c8), 0.9f));
    for (int s = -1; s <= 1; s += 2) b.box({-0.15f, 0.045f, s * 0.12f}, {0.025f, 0.05f, 0.012f}, GOLD);
    cutGem(b, {0, 0.095f, 0}, 0.035f, hexColor(0xd83a3a));
    torus(b, {0, 0.092f, 0}, 0.06f, 0.008f, GOLD);
}
void lantern(MeshBuilder& b, vec3 light) {
    b.lathe({{0.1f, 0}, {0.1f, 0.03f}, {0.08f, 0.04f}, {0.01f, 0.04f}}, IRON, 8);
    for (int i = 0; i < 4; i++) { float a = i * TAU / 4 + PI / 4; b.tube({std::cos(a) * 0.08f, 0.04f, std::sin(a) * 0.08f}, {std::cos(a) * 0.08f, 0.24f, std::sin(a) * 0.08f}, 0.01f, 0.01f, IRON, 4, false); }
    b.tube({0, 0.04f, 0}, {0, 0.24f, 0}, 0.065f, 0.065f, glow(light, 2.5f), 10, false);
    b.lathe({{0.1f, 0.24f}, {0.06f, 0.3f}, {0.02f, 0.33f}, {0.01f, 0.33f}}, IRON, 8);
    torus(b, {0, 0.37f, 0}, 0.04f, 0.008f, IRON, quat::axisAngle({1, 0, 0}, PI / 2), 12);
}
void keyModel(MeshBuilder& b, const Mat& m, vec3 gemc) {
    torus(b, {0, 0.05f, -0.15f}, 0.07f, 0.018f, m, quat::axisAngle({1, 0, 0}, PI / 2));
    b.tube({0, 0.05f, -0.08f}, {0, 0.05f, 0.2f}, 0.018f, 0.018f, m, 8, true);
    b.box({0, 0.02f, 0.15f}, {0.012f, 0.04f, 0.015f}, m); b.box({0, 0.01f, 0.19f}, {0.012f, 0.05f, 0.015f}, m);
    cutGem(b, {0, 0.05f, -0.15f}, 0.035f, gemc, quat::axisAngle({1, 0, 0}, PI / 2));
}
void crown(MeshBuilder& b, vec3 gemc, float glowE) {
    b.lathe({{0.16f, 0}, {0.17f, 0.04f}, {0.165f, 0.09f}, {0.15f, 0.09f}, {0.15f, 0.0f}}, GOLD, 24);
    for (int i = 0; i < 8; i++) { float a = i * TAU / 8; vec3 d(std::cos(a), 0, std::sin(a));
        b.cone(d * 0.16f + vec3(0, 0.08f, 0), d * 0.17f + vec3(0, i % 2 ? 0.17f : 0.24f, 0), 0.035f, GOLD, 4);
        if (i % 2 == 0) b.ellipsoid(d * 0.17f + vec3(0, 0.255f, 0), vec3(0.018f), Mat(vec3(1.f, 0.95f, 0.85f), 0.1f, 0.3f, 0.3f), 6, 4);
        cutGem(b, d * 0.168f + vec3(0, 0.05f, 0), 0.022f, i % 2 ? hexColor(0x3a7aff) : gemc, quat::axisAngle({0, 1, 0}, -a + PI / 2) * quat::axisAngle({1, 0, 0}, PI / 2)); }
    if (glowE > 0) b.ellipsoid({0, 0.12f, 0}, vec3(0.12f, 0.02f, 0.12f), glow(gemc, glowE), 12, 4);
}

// ---- jewellery
void ring(MeshBuilder& b, const Mat& band, vec3 gemc) {
    torus(b, {0, 0.1f, 0}, 0.085f, 0.02f, band, quat::axisAngle({1, 0, 0}, PI / 2));
    b.lathe({{0.045f, 0}, {0.04f, 0.025f}, {0.01f, 0.03f}}, band, 10, {0, 0.18f, 0});
    for (int i = 0; i < 4; i++) { float a = i * TAU / 4 + PI / 4; b.cone(vec3(std::cos(a) * 0.035f, 0.19f, std::sin(a) * 0.035f), vec3(std::cos(a) * 0.03f, 0.235f, std::sin(a) * 0.03f), 0.008f, band, 4); }
    cutGem(b, {0, 0.225f, 0}, 0.042f, gemc);
}
void amulet(MeshBuilder& b, vec3 gemc, int style) {
    for (int i = 0; i < 14; i++) { float a = PI * 0.15f + i * PI * 0.7f / 13; torus(b, vec3(std::cos(a) * 0.17f, 0.17f + std::sin(a) * 0.17f, 0), 0.012f, 0.004f, GOLD, quat::axisAngle({0, 1, 0}, i % 2 ? PI / 2 : 0.f), 8); }
    vec3 c(0, 0.08f, 0);
    quat face = quat::axisAngle({1, 0, 0}, PI / 2);
    if (style == 1) {   // sun
        b.lathe({{0.075f, 0}, {0.075f, 0.015f}, {0.01f, 0.02f}}, GOLD, 16, c, face);
        for (int i = 0; i < 12; i++) { float a = i * TAU / 12; b.cone(c + vec3(std::cos(a) * 0.07f, std::sin(a) * 0.07f, 0), c + vec3(std::cos(a) * 0.12f, std::sin(a) * 0.12f, 0), 0.018f, GOLD, 4); }
    } else if (style == 2) {   // a leaf (oak)
        b.ellipsoid(c, vec3(0.06f, 0.09f, 0.012f), Mat(hexColor(0x5a8a3a), 0.6f, 0.3f), 10, 6);
        b.tube(c - vec3(0, 0.09f, 0), c + vec3(0, 0.07f, 0), 0.005f, 0.003f, GOLD, 4, false);
    } else {
        torus(b, c, 0.065f, 0.012f, GOLD, face, 18);
        b.lathe({{0.058f, 0}, {0.01f, 0.006f}}, Mat(hexColor(0x2a2030), 0.5f), 16, c, face);
    }
    cutGem(b, c + vec3(0, 0, 0.02f), 0.04f, gemc, face);
    b.ellipsoid(c + vec3(0, 0.1f, 0), vec3(0.012f, 0.02f, 0.012f), GOLD, 6, 4);
}

// ---- armour (in the item's colour)
void helm(MeshBuilder& b, const ItemDef& d) {
    const std::string& id = d.id;
    Mat m(d.color, has(id, "leather") || has(id, "sylvan") || has(id, "shadow") ? 0.85f : 0.3f, has(id, "leather") || has(id, "sylvan") || has(id, "shadow") ? 0.f : 0.85f);
    if (has(id, "crown")) { crown(b, hexColor(0xd83a3a), 0); return; }
    if (has(id, "sylvan") || has(id, "shadow")) {   // a hood
        b.ellipsoid({0, 0.17f, 0}, vec3(0.16f, 0.18f, 0.17f), m, 16, 10);
        b.cone({0, 0.3f, -0.08f}, {0, 0.36f, -0.22f}, 0.07f, m, 8);
        b.ellipsoid({0, 0.15f, 0.08f}, vec3(0.11f, 0.13f, 0.12f), Mat(hexColor(0x14100c), 0.9f), 12, 8);
        if (has(id, "shadow")) for (int s = -1; s <= 1; s += 2) b.ellipsoid({s * 0.04f, 0.16f, 0.19f}, vec3(0.018f, 0.01f, 0.01f), glow(hexColor(0xb06cff), 3.f), 6, 4);
        else for (int i = 0; i < 5; i++) { float a = -0.8f + i * 0.4f; b.blade({std::sin(a) * 0.15f, 0.28f, std::cos(a) * 0.05f}, {std::sin(a) * 0.22f, 0.38f, std::cos(a) * 0.08f}, 0.06f, {0, 0, 1}, Mat(hexColor(0x4a8a3a)), Mat(hexColor(0x8ac05a))); }
        return;
    }
    // a helmet: dome, brow band, cheek guards, nasal, crest or horns
    b.lathe({{0.01f, 0.3f}, {0.08f, 0.29f}, {0.14f, 0.24f}, {0.165f, 0.16f}, {0.17f, 0.08f}, {0.165f, 0.05f}}, m, 18);
    torus(b, {0, 0.075f, 0}, 0.168f, 0.018f, has(id, "chain") ? IRON : GOLD);
    for (int s = -1; s <= 1; s += 2) b.bevelBox({s * 0.13f, 0.0f, 0.06f}, {0.03f, 0.07f, 0.07f}, 0.01f, m, quat::axisAngle({0, 1, 0}, s * 0.5f));
    b.box({0, 0.03f, 0.17f}, {0.016f, 0.07f, 0.012f}, m);
    if (has(id, "chain")) { for (int i = 0; i < 20; i++) { float a = PI * 0.25f + i * PI * 1.5f / 19; torus(b, vec3(std::sin(a) * 0.16f, -0.02f, std::cos(a) * 0.16f), 0.018f, 0.005f, IRON, quat::axisAngle({0, 1, 0}, a), 6); } }
    if (has(id, "frost")) for (int s = -1; s <= 1; s += 2) { b.tube({s * 0.15f, 0.18f, 0}, {s * 0.27f, 0.28f, -0.02f}, 0.035f, 0.025f, Mat(hexColor(0xe8e4d8), 0.5f), 8, false); b.cone({s * 0.27f, 0.28f, -0.02f}, {s * 0.3f, 0.42f, -0.06f}, 0.025f, Mat(hexColor(0xe8e4d8), 0.5f), 6); }
    if (has(id, "ember")) for (int i = 0; i < 5; i++) b.cone({0, 0.28f, 0.06f - i * 0.05f}, {0, 0.38f + (2 - std::abs(i - 2)) * 0.04f, 0.04f - i * 0.05f}, 0.03f, glow(hexColor(0xff7a2a), 1.6f), 5);
    if (!has(id, "frost") && !has(id, "ember") && !has(id, "leather")) { b.tube({0, 0.3f, 0.1f}, {0, 0.32f, -0.15f}, 0.02f, 0.02f, Mat(hexColor(0xa8282a), 0.9f), 6, true); }
}
void cuirass(MeshBuilder& b, const ItemDef& d) {
    bool soft = d.body == Body::Robe || d.body == Body::Leather || d.body == Body::Fur || d.body == Body::Tunic;
    Mat m(d.color, soft ? 0.85f : 0.3f, soft ? 0.f : 0.85f), trim = soft ? Mat(d.color * 0.6f, 0.8f) : GOLD;
    b.ellipsoid({0, 0.24f, 0}, vec3(0.2f, 0.25f, 0.13f), m, 18, 10);
    b.lathe({{0.2f, 0.0f}, {0.19f, 0.06f}, {0.17f, 0.09f}}, m, 18, {0, 0.02f, 0}, quat(), 0.65f);
    b.box({0, 0.08f, 0}, {0.19f, 0.025f, 0.125f}, Mat(hexColor(0x4a2e1a), 0.8f));   // belt
    b.box({0, 0.08f, 0.128f}, {0.03f, 0.025f, 0.008f}, GOLD);
    torus(b, {0, 0.46f, 0}, 0.085f, 0.02f, trim, quat(), 14);
    if (d.pauldrons || !soft) for (int s = -1; s <= 1; s += 2) b.ellipsoid({s * 0.2f, 0.41f, 0}, vec3(0.09f, 0.06f, 0.1f), m, 12, 7, quat::axisAngle({0, 0, 1}, s * -0.4f));
    if (!soft) { b.box({0, 0.28f, 0.125f}, {0.012f, 0.14f, 0.01f}, trim); cutGem(b, {0, 0.33f, 0.135f}, 0.03f, rarityColor(d.rarity), quat::axisAngle({1, 0, 0}, PI / 2)); }
    else for (int i = 0; i < 4; i++) b.ellipsoid({0, 0.36f - i * 0.07f, 0.128f}, vec3(0.012f), BRASS, 6, 4);
    if (d.body == Body::Fur) for (int i = 0; i < 12; i++) { float a = i * TAU / 12; b.cone({std::cos(a) * 0.1f, 0.45f, std::sin(a) * 0.08f}, {std::cos(a) * 0.15f, 0.48f, std::sin(a) * 0.12f}, 0.03f, Mat(hexColor(0xd8ccb8), 0.95f), 4); }
}
void gauntlet(MeshBuilder& b, const ItemDef& d) {
    bool soft = has(d.id, "leather") || has(d.id, "sylvan") || has(d.id, "shadow");
    Mat m(d.color, soft ? 0.85f : 0.3f, soft ? 0.f : 0.85f);
    b.lathe({{0.075f, 0}, {0.085f, 0.06f}, {0.07f, 0.16f}, {0.06f, 0.18f}}, m, 14);
    b.bevelBox({0, 0.23f, 0}, {0.06f, 0.06f, 0.035f}, 0.015f, m);
    for (int i = 0; i < 4; i++) { float x = -0.045f + i * 0.03f; b.tube({x, 0.28f, 0}, {x, 0.36f - std::abs(i - 1.5f) * 0.012f, 0.01f}, 0.014f, 0.012f, m, 6, true); }
    b.tube({0.06f, 0.22f, 0}, {0.1f, 0.29f, 0.01f}, 0.016f, 0.013f, m, 6, true);
    torus(b, {0, 0.16f, 0}, 0.074f, 0.012f, soft ? Mat(d.color * 0.6f) : GOLD, quat(), 14);
}
void boot(MeshBuilder& b, const ItemDef& d) {
    bool soft = has(d.id, "leather") || has(d.id, "sylvan") || has(d.id, "shadow") || has(d.id, "fur");
    Mat m(d.color, soft ? 0.85f : 0.35f, soft ? 0.f : 0.8f);
    b.tube({0, 0.08f, -0.03f}, {0, 0.32f, -0.04f}, 0.07f, 0.075f, m, 14, true);
    b.ellipsoid({0, 0.06f, 0.06f}, vec3(0.07f, 0.06f, 0.14f), m, 14, 8);
    b.bevelBox({0, 0.01f, 0.04f}, {0.075f, 0.012f, 0.16f}, 0.008f, Mat(hexColor(0x2a1a12), 0.9f));
    torus(b, {0, 0.3f, -0.04f}, 0.078f, 0.016f, soft ? Mat(hexColor(0xd8ccb8), 0.95f) : GOLD, quat(), 14);
    if (!soft) b.ellipsoid({0, 0.2f, 0.04f}, vec3(0.06f, 0.08f, 0.03f), m, 10, 6);
}

// ---- key items (the powers of the totems)
void compass(MeshBuilder& b) {
    b.lathe({{0.15f, 0}, {0.16f, 0.03f}, {0.15f, 0.05f}, {0.13f, 0.05f}, {0.01f, 0.045f}}, BRASS, 24);
    b.lathe({{0.13f, 0.046f}, {0.01f, 0.047f}}, Mat(hexColor(0xf0e8d0), 0.6f), 24);
    for (int i = 0; i < 8; i++) { float a = i * TAU / 8; b.box(vec3(std::cos(a) * 0.105f, 0.05f, std::sin(a) * 0.105f), {i % 2 ? 0.008f : 0.015f, 0.003f, 0.004f}, Mat(hexColor(0x3a2a1a)), quat::axisAngle({0, 1, 0}, -a)); }
    b.triP({0, 0.058f, 0.1f}, {-0.02f, 0.058f, 0}, {0.02f, 0.058f, 0}, Mat(hexColor(0xd82a2a), 0.4f, 0, 0.6f));
    b.triP({0, 0.058f, -0.1f}, {0.02f, 0.058f, 0}, {-0.02f, 0.058f, 0}, Mat(hexColor(0xf0f0f0), 0.4f));
    b.ellipsoid({0, 0.06f, 0}, vec3(0.015f, 0.008f, 0.015f), GOLD, 8, 4);
    torus(b, {0, 0.03f, 0.17f}, 0.025f, 0.008f, BRASS, quat::axisAngle({1, 0, 0}, PI / 2), 12);
}
void wing(MeshBuilder& b, vec3 root, float s, const Mat& m) {
    for (int i = 0; i < 5; i++) b.blade(root, root + vec3(s * (0.12f + i * 0.02f), 0.08f + i * 0.045f, -0.06f - i * 0.02f), 0.06f, {0, 0, 1}, m, Mat(m.color * 1.1f, m.rough, m.metal, m.emis));
}
void feather(MeshBuilder& b, vec3 col, float emis) {
    Mat vane(col, 0.6f, 0.f, emis), q(hexColor(0xf0e8d8), 0.5f);
    b.tube({0, 0, -0.25f}, {0, 0.06f, 0.26f}, 0.008f, 0.004f, q, 6, true);
    for (int i = 0; i < 12; i++) { float t = 0.1f + i * 0.075f; vec3 p = lerp(vec3(0, 0, -0.25f), vec3(0, 0.06f, 0.26f), t);
        for (int s = -1; s <= 1; s += 2) b.blade(p, p + vec3(s * (0.07f + 0.04f * std::sin(t * PI)), 0.01f, 0.06f), 0.05f, {0, 1, 0}, vane, Mat(lerp(col, vec3(1.f), 0.3f), 0.6f, 0.f, emis)); }
}

}  // namespace

// The 3D model of an item, built at the origin, about 0.3-0.8 m (weapons: real size). Returns false when the
// item has no model (the 2D icon stays).
bool buildItemModel(MeshBuilder& b, const ItemDef& d) {
    const std::string& id = d.id;
    vec3 rc = rarityColor(d.rarity);
    switch (d.kind) {
    case ItemKind::Weapon: case ItemKind::Shield: case ItemKind::Tool:
        if (id == "tool_shovel") {
            b.tube({0, 0, -0.5f}, {0, 0, 0.35f}, 0.022f, 0.022f, WOODM, 8, true);
            b.tube({-0.07f, 0, -0.5f}, {0.07f, 0, -0.5f}, 0.018f, 0.018f, WOODM, 6, true);
            b.bevelBox({0, 0, 0.5f}, {0.11f, 0.012f, 0.15f}, 0.01f, IRON);
            b.cone({0, 0, 0.62f}, {0, 0, 0.72f}, 0.1f, IRON, 4);
            b.tube({0, 0, 0.32f}, {0, 0, 0.38f}, 0.03f, 0.03f, IRON, 8, true);
            return true;
        }
        if (d.weapon == WeaponType::None) return false;
        buildWeapon(b, d.weapon, d.tier, d.weapon == WeaponType::Shield ? d.color : elementColor(d.element));
        return true;
    case ItemKind::Head: helm(b, d); return true;
    case ItemKind::Chest: cuirass(b, d); return true;
    case ItemKind::Hands: gauntlet(b, d); return true;
    case ItemKind::Feet: boot(b, d); return true;
    case ItemKind::Ring: {
        Mat band = has(id, "copper") ? Mat(hexColor(0xc87a4a), 0.3f, 0.9f) : has(id, "silver") || has(id, "tide") ? SILVER : has(id, "abyss") ? Mat(hexColor(0x3a3044), 0.3f, 0.8f) : GOLD;
        vec3 g = has(id, "sapphire") || has(id, "tide") ? hexColor(0x3a6aff) : has(id, "wolf") ? hexColor(0xb8d8ff) : has(id, "abyss") ? hexColor(0xa04aff) : has(id, "scarab") ? hexColor(0x3ad08a) : has(id, "legion") ? hexColor(0xd83a3a) : rc;
        ring(b, band, g); return true;
    }
    case ItemKind::Amulet: {
        vec3 g = has(id, "ember") || has(id, "sun") ? hexColor(0xff8a2a) : has(id, "star") ? hexColor(0xb8a0ff) : has(id, "pearl") ? hexColor(0xf4f0ff) : has(id, "aurora") ? hexColor(0x5affc8) : has(id, "silence") ? hexColor(0x6a3aa8) : has(id, "crown") ? hexColor(0xd83a3a) : hexColor(0x6ad06a);
        amulet(b, g, has(id, "sun") ? 1 : (has(id, "oak") ? 2 : 0)); return true;
    }
    case ItemKind::Consumable:
        if (has(id, "bag_")) { sack(b, has(id, "bottomless") ? hexColor(0x2a2050) : (has(id, "adventurer") ? hexColor(0x5a6a3a) : hexColor(0x8a5a32)), has(id, "bottomless")); return true; }
        if (id == "bomb") { b.ellipsoid({0, 0.15f, 0}, vec3(0.15f), Mat(hexColor(0x1c1c22), 0.35f, 0.6f), 16, 10); b.tube({0, 0.28f, 0}, {0, 0.32f, 0}, 0.04f, 0.04f, IRON, 8, true);
            b.tube({0, 0.32f, 0}, {0.05f, 0.42f, 0.02f}, 0.012f, 0.01f, Mat(hexColor(0x8a6a3a)), 6, true); b.ellipsoid({0.05f, 0.43f, 0.02f}, vec3(0.03f), glow(hexColor(0xffb040), 3.f), 6, 4); return true; }
        if (id == "bread") { bread(b); return true; }
        if (id == "stew" || id == "fish_stew") { bowl(b, id == "stew" ? hexColor(0x8a4a24) : hexColor(0xd8a860), id == "fish_stew"); return true; }
        if (id == "fish_grilled" || id == "salted_fish") { fishModel(b, id == "fish_grilled" ? hexColor(0xb8742a) : hexColor(0xb8b0a0), true); return true; }
        if (id == "berries") { cluster(b, hexColor(0x4a2a8a), 9, 0.05f, 11, true); return true; }
        if (id == "dates") { Rng r(4); for (int i = 0; i < 6; i++) { vec2 p = r.inDisk() * 0.1f; b.ellipsoid({p.x, 0.04f + i * 0.01f, p.y}, vec3(0.03f, 0.03f, 0.05f), Mat(hexColor(0x6a3418), 0.4f), 8, 5, quat::axisAngle({0, 1, 0}, r.range(0, TAU))); } return true; }
        if (id == "coco") { b.ellipsoid({0, 0.12f, 0}, vec3(0.14f, 0.12f, 0.14f), Mat(hexColor(0x5a3a22), 0.95f), 14, 8); b.lathe({{0.13f, 0.17f}, {0.01f, 0.175f}}, Mat(hexColor(0xf4f0e8), 0.6f), 14); return true; }
        if (id == "bandage") { b.tube({-0.1f, 0.08f, 0}, {0.1f, 0.08f, 0}, 0.08f, 0.08f, CLOTHW, 14, true); b.box({0.02f, 0.005f, 0.1f}, {0.09f, 0.004f, 0.08f}, CLOTHW); b.box({0.05f, 0.162f, 0}, {0.02f, 0.003f, 0.03f}, Mat(hexColor(0xd82a2a), 0.6f)); b.box({0.05f, 0.162f, 0}, {0.03f, 0.003f, 0.02f}, Mat(hexColor(0xd82a2a), 0.6f)); return true; }
        if (has(id, "elixir")) { elixir(b, liquidColour(id)); return true; }
        if (has(id, "juice") || has(id, "tonic") || has(id, "tea")) { vial(b, liquidColour(id)); return true; }
        potion(b, liquidColour(id), has(id, "minor") ? 0 : (has(id, "great") ? 2 : 1));
        return true;
    case ItemKind::Material:
        if (id == "wood") { logs(b); return true; }
        if (id == "stone") { b.rock({0, 0.12f, 0}, vec3(0.2f, 0.13f, 0.17f), Mat(hexColor(0x8a867e), 0.9f), 21, 1); b.rock({0.18f, 0.06f, 0.1f}, vec3(0.08f), Mat(hexColor(0x7a766e), 0.9f), 22, 0); return true; }
        if (id == "coal") { b.rock({0, 0.11f, 0}, vec3(0.17f, 0.12f, 0.15f), Mat(hexColor(0x1e1e22), 0.6f, 0.3f), 31, 1); return true; }
        if (has(id, "_ore")) {
            vec3 v = has(id, "iron") ? hexColor(0xc07a5a) : has(id, "copper") ? hexColor(0xd8803a) : has(id, "gold") ? hexColor(0xf0c040) : has(id, "silver") ? hexColor(0xe0e4ec) : hexColor(0xb06cff);
            ore(b, v, !has(id, "rare"), (uint32_t)id.size() * 13); return true;
        }
        if (has(id, "_bar") || id == "titan_steel") {
            Mat m = has(id, "copper") ? Mat(hexColor(0xd8804a), 0.3f, 0.95f) : has(id, "gold") ? GOLD : has(id, "silver") ? SILVER : has(id, "steel") && !has(id, "titan") ? Mat(hexColor(0x9aa8b8), 0.25f, 0.95f) : has(id, "star") ? Mat(hexColor(0xb8a8ff), 0.2f, 0.8f, 0.6f) : has(id, "titan") ? Mat(hexColor(0x3a4a6a), 0.2f, 0.95f, 0.15f) : Mat(hexColor(0x8a8c94), 0.4f, 0.9f);
            ingot(b, m, has(id, "star") || has(id, "titan")); return true;
        }
        if (id == "crystal") { crystals(b, hexColor(0x7ae8ff), 0.6f, 41); return true; }
        if (id == "frost_crystal") { crystals(b, hexColor(0xbfe8ff), 0.5f, 42); return true; }
        if (id == "ember_core") { b.ellipsoid({0, 0.12f, 0}, vec3(0.1f), glow(hexColor(0xff6a1a), 2.5f), 12, 8); b.rock({0, 0.12f, 0}, vec3(0.13f), Mat(hexColor(0x2a1a14), 0.9f), 43, 0); return true; }
        if (id == "aurora_crystal") { crystals(b, hexColor(0x5affc8), 1.2f, 44); return true; }
        if (id == "sky_shard" || id == "star_shard") { starShard(b); return true; }
        if (id == "amber") { b.flat = true; b.rock({0, 0.1f, 0}, vec3(0.13f, 0.1f, 0.11f), Mat(hexColor(0xf0a030), 0.1f, 0.f, 0.6f), 45, 0); b.flat = false; b.ellipsoid({0.02f, 0.11f, 0.03f}, vec3(0.02f, 0.012f, 0.03f), Mat(hexColor(0x2a1a0a), 0.5f), 6, 4); return true; }
        if (id == "coral_shard") { Mat c(hexColor(0xff7a8a), 0.6f); b.tube({0, 0, 0}, {0, 0.2f, 0}, 0.03f, 0.02f, c, 8, true); for (int i = 0; i < 5; i++) { float a = i * TAU / 5; b.tube({0, 0.06f + i * 0.025f, 0}, {std::cos(a) * 0.12f, 0.16f + i * 0.03f, std::sin(a) * 0.12f}, 0.02f, 0.012f, c, 6, true); } return true; }
        if (id == "sand_glass") { b.lathe({{0.01f, 0}, {0.1f, 0.0f}, {0.1f, 0.02f}, {0.08f, 0.04f}, {0.02f, 0.15f}, {0.08f, 0.26f}, {0.1f, 0.28f}, {0.1f, 0.3f}, {0.01f, 0.3f}}, GLASS, 16);
            b.lathe({{0.075f, 0.04f}, {0.01f, 0.1f}}, Mat(hexColor(0xe0b870), 0.9f), 12); b.lathe({{0.01f, 0.2f}, {0.07f, 0.26f}}, Mat(hexColor(0xe0b870), 0.9f), 12);
            for (int i = 0; i < 3; i++) { float a = i * TAU / 3; b.tube({std::cos(a) * 0.1f, 0.02f, std::sin(a) * 0.1f}, {std::cos(a) * 0.1f, 0.28f, std::sin(a) * 0.1f}, 0.01f, 0.01f, DARKWOOD, 5, false); } return true; }
        if (id == "herb") { for (int i = 0; i < 5; i++) { float a = i * 1.2f; b.blade({0, 0, 0}, {std::cos(a) * 0.1f, 0.25f, std::sin(a) * 0.1f}, 0.07f, {std::sin(a), 0, -std::cos(a)}, Mat(hexColor(0x3a7a2a)), Mat(hexColor(0x7ac04a)), 0.5f); } b.tube({0, 0, 0}, {0, 0.06f, 0}, 0.02f, 0.02f, Mat(hexColor(0xc8a870)), 6, false); return true; }
        if (id == "frost_lily" || id == "glowcap" || id == "fire_root" || id == "flax" || id == "cactus_fruit") {
            if (id == "glowcap") { for (int i = 0; i < 3; i++) { vec3 p(i * 0.07f - 0.07f, 0, (i % 2) * 0.05f); float h = 0.12f + i * 0.05f; b.tube(p, p + vec3(0, h, 0), 0.02f, 0.015f, Mat(hexColor(0xe8e0cc)), 6, false); b.lathe({{0.01f, h - 0.02f}, {0.08f, h - 0.01f}, {0.06f, h + 0.04f}, {0.01f, h + 0.05f}}, glow(hexColor(0x6ad0ff), 1.6f), 10, p); } return true; }
            if (id == "frost_lily") { b.tube({0, 0, 0}, {0, 0.2f, 0}, 0.01f, 0.008f, Mat(hexColor(0x4a7a5a)), 5, false); for (int i = 0; i < 6; i++) { float a = i * TAU / 6; b.blade({0, 0.2f, 0}, vec3(std::cos(a) * 0.1f, 0.27f, std::sin(a) * 0.1f), 0.06f, {-std::sin(a), 0, std::cos(a)}, Mat(hexColor(0xc8e8ff), 0.4f, 0, 0.3f), Mat(hexColor(0x7ab8ff), 0.4f, 0, 0.6f)); } b.ellipsoid({0, 0.21f, 0}, vec3(0.02f), glow(hexColor(0xffffff), 1.5f), 6, 4); return true; }
            if (id == "fire_root") { Mat r(hexColor(0xc83a1a), 0.6f, 0.f, 0.5f); b.tube({0, 0.2f, 0}, {0.02f, 0.02f, 0.01f}, 0.04f, 0.01f, r, 8, true); for (int i = 0; i < 4; i++) { float a = i * 1.6f; b.tube({0, 0.12f, 0}, {std::cos(a) * 0.12f, 0.02f, std::sin(a) * 0.12f}, 0.015f, 0.004f, r, 5, true); } for (int i = 0; i < 3; i++) b.blade({0, 0.2f, 0}, {(i - 1) * 0.08f, 0.35f, 0}, 0.05f, {0, 0, 1}, Mat(hexColor(0x5a8a2a)), Mat(hexColor(0x8ab84a))); return true; }
            if (id == "flax") { for (int i = 0; i < 9; i++) { float x = (i - 4) * 0.012f; b.tube({x, 0, 0}, {x * 3.f, 0.35f, (i % 3 - 1) * 0.02f}, 0.006f, 0.004f, Mat(hexColor(0xc8b870), 0.8f), 4, false); b.ellipsoid({x * 3.f, 0.36f, (i % 3 - 1) * 0.02f}, vec3(0.015f), Mat(hexColor(0x6a8ad8), 0.6f), 5, 3); } torus(b, {0, 0.12f, 0}, 0.04f, 0.01f, Mat(hexColor(0x8a5a2a)), quat(), 10); return true; }
            b.ellipsoid({0, 0.1f, 0}, vec3(0.08f, 0.1f, 0.08f), Mat(hexColor(0xd8306a), 0.5f), 12, 8); for (int i = 0; i < 3; i++) b.cone({(i - 1) * 0.03f, 0.19f, 0}, {(i - 1) * 0.05f, 0.25f, 0}, 0.02f, Mat(hexColor(0x5a8a2a)), 4); return true;
        }
        if (id == "leather" || id == "troll_hide") { hide(b, id == "leather" ? hexColor(0x8a5a32) : hexColor(0x5a7a4a), false); return true; }
        if (id == "fur") { hide(b, hexColor(0x9a8470), true); return true; }
        if (id == "linen") { clothRoll(b, hexColor(0xece4d0)); return true; }
        if (id == "spider_silk") { b.tube({0, 0, 0}, {0, 0.22f, 0}, 0.04f, 0.04f, DARKWOOD, 10, true); b.lathe({{0.04f, 0.03f}, {0.1f, 0.06f}, {0.11f, 0.11f}, {0.1f, 0.16f}, {0.04f, 0.19f}}, Mat(hexColor(0xf4f4f8), 0.5f, 0.f, 0.2f), 16); return true; }
        if (id == "bone") { b.tube({0, 0.05f, -0.18f}, {0, 0.05f, 0.18f}, 0.025f, 0.025f, Mat(hexColor(0xece4d0), 0.6f), 8, false); for (int s = -1; s <= 1; s += 2) for (int t = -1; t <= 1; t += 2) b.ellipsoid({t * 0.03f, 0.05f, s * 0.2f}, vec3(0.04f), Mat(hexColor(0xece4d0), 0.6f), 8, 5); return true; }
        if (id == "shadow_essence") { b.ellipsoid({0, 0.14f, 0}, vec3(0.11f), Mat(hexColor(0x1a0a2a), 0.2f, 0.f, 0.3f), 14, 8); for (int i = 0; i < 5; i++) { float a = i * TAU / 5; b.blade({0, 0.14f, 0}, vec3(std::cos(a) * 0.2f, 0.2f + i * 0.02f, std::sin(a) * 0.2f), 0.06f, {0, 1, 0}, glow(hexColor(0x8a3aff), 1.5f), glow(hexColor(0x3a0a6a), 0.5f), 0.6f); } return true; }
        if (id == "dragon_scale") { b.flat = true; b.lathe({{0.01f, 0}, {0.15f, 0.02f}, {0.12f, 0.15f}, {0.01f, 0.25f}}, Mat(hexColor(0x2a8a5a), 0.25f, 0.6f, 0.1f), 5, {}, quat(), 0.25f); b.flat = false; return true; }
        if (id == "fish") { fishModel(b, hexColor(0x6a8aa0), false); return true; }
        if (id == "meat") { meat(b, false); return true; }
        if (id == "scorpion_venom") { vial(b, liquidColour(id)); return true; }
        if (id == "pirate_coin") { coins(b, GOLD); return true; }
        if (id == "sea_salt") { sack(b, hexColor(0xe8e4d8), false); return true; }
        if (id == "seashell") { shell(b, hexColor(0xf0c8a8)); return true; }
        if (id == "pearl") { shell(b, hexColor(0xe8d8d0)); b.ellipsoid({0, 0.07f, 0}, vec3(0.05f), Mat(hexColor(0xf8f4ff), 0.08f, 0.3f, 0.2f), 14, 8); return true; }
        if (id == "legion_insignia") { b.lathe({{0.12f, 0}, {0.12f, 0.02f}, {0.01f, 0.025f}}, GOLD, 6, {0, 0.12f, 0}, quat::axisAngle({1, 0, 0}, PI / 2)); b.cone({0, 0.12f, 0.02f}, {0, 0.24f, 0.02f}, 0.05f, Mat(hexColor(0xa82a2a), 0.5f), 4); cutGem(b, {0, 0.12f, 0.03f}, 0.035f, hexColor(0xd83a3a), quat::axisAngle({1, 0, 0}, PI / 2)); return true; }
        return false;
    case ItemKind::Quest: case ItemKind::Relic: {
        if (id == "echo_stone") { b.ellipsoid({0, 0.1f, 0}, vec3(0.16f, 0.1f, 0.13f), Mat(hexColor(0x5a6a7a), 0.3f), 16, 10); for (int i = 0; i < 16; i++) { float t = i / 15.f, a = t * TAU * 1.5f; b.ellipsoid(vec3(std::cos(a) * t * 0.1f, 0.2f, std::sin(a) * t * 0.08f), vec3(0.012f, 0.004f, 0.012f), glow(hexColor(0x7af0ff), 2.5f), 4, 3); } return true; }
        if (id == "crown_shard") { b.flat = true; b.triP({-0.12f, 0, 0}, {0.12f, 0, 0}, {0.03f, 0.28f, 0}, GOLD); b.triP({0.12f, 0, 0}, {-0.12f, 0, 0}, {0.03f, 0.28f, 0}, GOLD); b.flat = false; b.box({0, 0.03f, 0}, {0.12f, 0.03f, 0.015f}, GOLD); cutGem(b, {0, 0.06f, 0.02f}, 0.035f, hexColor(0xd83a3a), quat::axisAngle({1, 0, 0}, PI / 2)); return true; }
        if (id == "mother_crown") { crown(b, hexColor(0xb06cff), 0.8f); return true; }
        if (id == "miners_badge") { b.lathe({{0.11f, 0}, {0.11f, 0.015f}, {0.01f, 0.02f}}, BRASS, 8, {0, 0.11f, 0}, quat::axisAngle({1, 0, 0}, PI / 2)); b.tube({-0.06f, 0.06f, 0.025f}, {0.06f, 0.16f, 0.025f}, 0.01f, 0.01f, IRON, 5, true); b.tube({0.06f, 0.06f, 0.025f}, {-0.06f, 0.16f, 0.025f}, 0.01f, 0.01f, IRON, 5, true); return true; }
        if (id == "grave_lantern") { lantern(b, hexColor(0x7aff9a)); return true; }
        if (id == "relic_miner_lamp") { lantern(b, hexColor(0xffc060)); return true; }
        if (has(id, "chart") || has(id, "map")) { scroll(b, has(id, "star") ? hexColor(0x3a4aa8) : hexColor(0xa82a2a)); return true; }
        if (has(id, "letter")) { b.box({0, 0.01f, 0}, {0.18f, 0.008f, 0.12f}, Mat(hexColor(0xf0e4c8), 0.9f)); b.triP({-0.18f, 0.02f, -0.12f}, {0.18f, 0.02f, -0.12f}, {0, 0.02f, 0.03f}, Mat(hexColor(0xe0d0b0), 0.9f)); b.lathe({{0.035f, 0}, {0.03f, 0.012f}, {0.01f, 0.014f}}, Mat(hexColor(0xa82a2a), 0.5f), 10, {0, 0.02f, 0.03f}); return true; }
        if (has(id, "ledger")) { book(b, hexColor(0x6a3a22)); return true; }
        if (id == "tide_pearl") { shell(b, hexColor(0xd8e8f0)); b.ellipsoid({0, 0.09f, 0}, vec3(0.07f), glow(hexColor(0x9adcff), 0.9f), 14, 8); return true; }
        if (id == "amber_key") { keyModel(b, Mat(hexColor(0xf0a030), 0.2f, 0.6f, 0.2f), hexColor(0xffd060)); return true; }
        if (id == "sun_sigil" || id == "relic_sun_mask") {
            b.lathe({{0.13f, 0}, {0.13f, 0.03f}, {0.08f, 0.05f}, {0.01f, 0.05f}}, GOLD, 20, {0, 0.18f, 0}, quat::axisAngle({1, 0, 0}, PI / 2));
            for (int i = 0; i < 16; i++) { float a = i * TAU / 16; b.cone(vec3(std::cos(a) * 0.12f, 0.18f + std::sin(a) * 0.12f, 0), vec3(std::cos(a) * (i % 2 ? 0.17f : 0.22f), 0.18f + std::sin(a) * (i % 2 ? 0.17f : 0.22f), 0), 0.03f, GOLD, 4); }
            if (id == "relic_sun_mask") for (int s = -1; s <= 1; s += 2) b.ellipsoid({s * 0.045f, 0.2f, 0.05f}, vec3(0.03f, 0.012f, 0.01f), Mat(hexColor(0x1a1008)), 6, 4);
            else cutGem(b, {0, 0.18f, 0.05f}, 0.04f, hexColor(0xff8a2a), quat::axisAngle({1, 0, 0}, PI / 2));
            return true;
        }
        if (id == "silence_jewel") { b.flat = true; b.lathe({{0.01f, 0}, {0.09f, 0.12f}, {0.07f, 0.2f}, {0.01f, 0.32f}}, Mat(hexColor(0x3a1a5a), 0.05f, 0.2f, 0.6f), 7); b.flat = false; torus(b, {0, 0.12f, 0}, 0.092f, 0.012f, SILVER); return true; }
        if (id == "relic_elven_harp") { Mat w(hexColor(0xd8c8a0), 0.5f, 0.3f); b.tube({0, 0, 0}, {0, 0.4f, 0}, 0.02f, 0.015f, w, 8, true); for (int i = 0; i < 6; i++) { float t = i / 5.f; b.tube(vec3(0, 0.4f * t, 0) + vec3(0.2f * std::sin(t * PI), 0, 0) * 0.f, vec3(0.02f + 0.25f * std::sin(t * 2.6f), 0.4f * t + 0.05f, 0), 0.012f, 0.012f, GOLD, 5, false); }
            b.tube({0, 0.4f, 0}, {0.27f, 0.3f, 0}, 0.018f, 0.018f, w, 8, true); b.tube({0, 0.02f, 0}, {0.27f, 0.3f, 0}, 0.015f, 0.015f, w, 8, true);
            for (int i = 1; i < 7; i++) { float x = i * 0.036f; b.tube({x, 0.02f + x * 1.0f, 0}, {x, 0.4f - x * 0.37f, 0}, 0.003f, 0.003f, glow(hexColor(0xe8f4ff), 0.8f), 3, false); } return true; }
        if (id == "relic_jarl_horn") { for (int i = 0; i < 8; i++) { float t0 = i / 8.f, t1 = (i + 1) / 8.f; auto P = [](float t) { return vec3(std::sin(t * 2.2f) * 0.2f, 0.05f + t * 0.15f, -std::cos(t * 2.2f) * 0.2f); };
            b.tube(P(t0), P(t1), 0.07f * (1 - t0) + 0.015f, 0.07f * (1 - t1) + 0.015f, i % 3 == 0 ? SILVER : Mat(hexColor(0xe8dcc0), 0.5f), 10, false); } return true; }
        if (id == "relic_kings_seal") { b.lathe({{0.08f, 0}, {0.08f, 0.04f}, {0.04f, 0.07f}, {0.035f, 0.2f}, {0.05f, 0.24f}, {0.01f, 0.26f}}, GOLD, 16); b.lathe({{0.07f, 0.0f}, {0.01f, -0.005f}}, Mat(hexColor(0xa82a2a), 0.5f), 16); return true; }
        if (id == "relic_star_glass") { b.tube({0, 0.06f, -0.2f}, {0, 0.06f, 0.0f}, 0.05f, 0.05f, BRASS, 14, true); b.tube({0, 0.06f, 0.0f}, {0, 0.06f, 0.15f}, 0.042f, 0.042f, Mat(hexColor(0x2a3a6a), 0.4f, 0.6f), 14, true); b.tube({0, 0.06f, 0.15f}, {0, 0.06f, 0.26f}, 0.034f, 0.034f, BRASS, 14, true); b.lathe({{0.034f, 0}, {0.01f, 0.002f}}, glow(hexColor(0xb8c8ff), 1.5f), 12, {0, 0.06f, 0.261f}, quat::axisAngle({1, 0, 0}, PI / 2)); return true; }
        if (id == "relic_aldric_ring") { ring(b, GOLD, hexColor(0xb06cff)); return true; }
        if (id == "relic_sea_bell") { b.lathe({{0.01f, 0.3f}, {0.05f, 0.29f}, {0.08f, 0.22f}, {0.1f, 0.08f}, {0.15f, 0.02f}, {0.14f, 0.0f}, {0.12f, 0.02f}}, Mat(hexColor(0x5a9a8a), 0.3f, 0.8f), 18); torus(b, {0, 0.33f, 0}, 0.03f, 0.01f, Mat(hexColor(0x5a9a8a), 0.3f, 0.8f), quat::axisAngle({1, 0, 0}, PI / 2), 10); b.ellipsoid({0, 0.03f, 0}, vec3(0.03f), BRASS, 8, 5); return true; }
        if (id == "relic_sphinx_eye") { b.ellipsoid({0, 0.12f, 0}, vec3(0.16f, 0.08f, 0.04f), GOLD, 16, 8); b.ellipsoid({0, 0.12f, 0.025f}, vec3(0.12f, 0.06f, 0.03f), Mat(hexColor(0xf0ecd8), 0.4f), 14, 8); b.ellipsoid({0, 0.12f, 0.045f}, vec3(0.05f, 0.05f, 0.02f), gem(hexColor(0x3ad08a)), 12, 6); b.ellipsoid({0, 0.12f, 0.06f}, vec3(0.02f, 0.03f, 0.01f), Mat(hexColor(0x0a0a0a)), 8, 4); return true; }
        if (id == "relic_comet") { b.rock({0, 0.12f, 0}, vec3(0.11f), Mat(hexColor(0x3a3a4a), 0.6f, 0.3f), 77, 1); b.ellipsoid({0, 0.12f, 0}, vec3(0.09f), glow(hexColor(0x9ad8ff), 2.f), 10, 6); for (int i = 0; i < 3; i++) b.cone({0, 0.12f, -0.05f}, {(i - 1) * 0.05f, 0.18f + i * 0.02f, -0.45f}, 0.07f - i * 0.015f, glow(hexColor(0x7ab8ff), 1.2f), 8); return true; }
        if (id == "sky_shard") { starShard(b); return true; }
        // other quest objects: a glowing relic orb on a gold stand
        b.lathe({{0.1f, 0}, {0.1f, 0.02f}, {0.05f, 0.05f}, {0.04f, 0.12f}, {0.08f, 0.14f}, {0.01f, 0.14f}}, GOLD, 14);
        b.ellipsoid({0, 0.22f, 0}, vec3(0.08f), glow(rc, 1.5f), 14, 8);
        return true;
    }
    case ItemKind::MountToken: {   // a lucky golden horseshoe with a ribbon in the mount's colours
        vec3 rib = has(id, "wolf") ? hexColor(0x9ac8ff) : has(id, "elk") ? hexColor(0x6ad0a0) : has(id, "boar") ? hexColor(0x8a5a32) : has(id, "spectral") ? hexColor(0xb06cff) : has(id, "dune") ? hexColor(0xffc060) : hexColor(0xd83a3a);
        for (int i = 0; i < 12; i++) { float a0 = -0.6f + i * (PI + 1.2f) / 12, a1 = a0 + (PI + 1.2f) / 12; b.tube(vec3(std::cos(a0) * 0.13f, 0.15f + std::sin(a0) * 0.13f, 0), vec3(std::cos(a1) * 0.13f, 0.15f + std::sin(a1) * 0.13f, 0), 0.025f, 0.025f, GOLD, 8, false); }
        for (int i = 0; i < 6; i++) { float a = 0.0f + i * 0.55f; b.ellipsoid(vec3(std::cos(a) * 0.13f, 0.15f + std::sin(a) * 0.13f, 0.02f), vec3(0.01f), IRON, 5, 3); }
        b.blade({0, 0.28f, 0.02f}, {-0.08f, 0.0f, 0.04f}, 0.05f, {0, 0, 1}, Mat(rib, 0.8f), Mat(rib * 0.8f, 0.8f));
        b.blade({0, 0.28f, 0.02f}, {0.07f, 0.02f, 0.04f}, 0.05f, {0, 0, 1}, Mat(rib, 0.8f), Mat(rib * 0.8f, 0.8f));
        return true;
    }
    case ItemKind::Cosmetic: {   // a jar of dye
        vec3 c = has(id, "royal") ? hexColor(0x2a4ab8) : has(id, "crimson") ? hexColor(0xb8202a) : has(id, "forest") ? hexColor(0x2a7a3a) : has(id, "ivory") ? hexColor(0xf0ead8) : has(id, "night") ? hexColor(0x1a1a3a) : hexColor(0xe0b040);
        b.lathe({{0.01f, 0}, {0.1f, 0}, {0.12f, 0.05f}, {0.12f, 0.15f}, {0.09f, 0.19f}, {0.09f, 0.21f}}, Mat(hexColor(0xd8c8a8), 0.4f), 16);
        b.lathe({{0.09f, 0.2f}, {0.06f, 0.25f}, {0.01f, 0.27f}}, Mat(c, 0.95f), 14);
        torus(b, {0, 0.1f, 0}, 0.122f, 0.012f, Mat(c, 0.6f), quat(), 16);
        return true;
    }
    case ItemKind::Key: {
        if (id == "key_compass") { compass(b); return true; }
        if (id == "key_zephyr") { ItemDef bd = d; bd.color = hexColor(0xe8e0d0); boot(b, bd); for (int s = -1; s <= 1; s += 2) wing(b, {s * 0.07f, 0.24f, -0.05f}, (float)s, Mat(hexColor(0xffffff), 0.6f, 0.f, 0.3f)); return true; }
        if (id == "key_feather") { feather(b, hexColor(0xf4f0ff), 0.4f); return true; }
        if (id == "key_phoenix") { feather(b, hexColor(0xff6a1a), 1.8f); return true; }
        if (id == "key_magnet") { Mat red(hexColor(0xd82a2a), 0.4f, 0.4f); for (int i = 0; i < 10; i++) { float a0 = i * PI / 10, a1 = (i + 1) * PI / 10; b.tube(vec3(std::cos(a0) * 0.12f, 0.12f + std::sin(a0) * 0.12f, 0), vec3(std::cos(a1) * 0.12f, 0.12f + std::sin(a1) * 0.12f, 0), 0.035f, 0.035f, red, 10, false); }
            for (int s = -1; s <= 1; s += 2) { b.tube({s * 0.12f, 0.12f, 0}, {s * 0.12f, 0.0f, 0}, 0.035f, 0.035f, red, 10, false); b.tube({s * 0.12f, 0.0f, 0}, {s * 0.12f, -0.06f, 0}, 0.035f, 0.035f, SILVER, 10, true); } return true; }
        if (id == "key_triton") { b.tube({0, -0.3f, 0}, {0, 0.25f, 0}, 0.015f, 0.015f, Mat(hexColor(0x5ab8c8), 0.3f, 0.8f), 8, true); b.tube({-0.08f, 0.25f, 0}, {0.08f, 0.25f, 0}, 0.018f, 0.018f, GOLD, 8, true);
            for (int i = -1; i <= 1; i++) { b.tube({i * 0.08f, 0.25f, 0}, {i * 0.08f, 0.38f + (i == 0) * 0.04f, 0}, 0.014f, 0.01f, GOLD, 6, false); b.cone({i * 0.08f, 0.38f + (i == 0) * 0.04f, 0}, {i * 0.08f, 0.45f + (i == 0) * 0.04f, 0}, 0.025f, GOLD, 4); }
            cutGem(b, {0, 0.22f, 0.02f}, 0.03f, hexColor(0x3ad0ff), quat::axisAngle({1, 0, 0}, PI / 2)); return true; }
        if (id == "key_clover") { Mat lf(hexColor(0x3ab84a), 0.4f, 0.1f, 0.25f); for (int i = 0; i < 4; i++) { float a = i * TAU / 4 + PI / 4; for (int s = -1; s <= 1; s += 2) b.ellipsoid(vec3(std::cos(a) * 0.08f + std::cos(a + s * 0.6f) * 0.03f, 0.2f + std::sin(a) * 0.08f + std::sin(a + s * 0.6f) * 0.03f, 0), vec3(0.05f, 0.05f, 0.012f), lf, 10, 5); }
            b.tube({0, 0.2f, 0}, {0.03f, 0.0f, 0}, 0.01f, 0.008f, lf, 6, true); torus(b, {0, 0.2f, 0}, 0.2f, 0.01f, GOLD, quat::axisAngle({1, 0, 0}, PI / 2), 28); return true; }
        if (id == "key_swift") { for (int i = 0; i < 12; i++) { float a0 = -0.6f + i * (PI + 1.2f) / 12, a1 = a0 + (PI + 1.2f) / 12; b.tube(vec3(std::cos(a0) * 0.12f, 0.15f + std::sin(a0) * 0.12f, 0), vec3(std::cos(a1) * 0.12f, 0.15f + std::sin(a1) * 0.12f, 0), 0.022f, 0.022f, SILVER, 8, false); }
            for (int s = -1; s <= 1; s += 2) wing(b, {s * 0.1f, 0.2f, 0}, (float)s, Mat(hexColor(0x9ad8ff), 0.5f, 0.f, 0.5f)); return true; }
        b.ellipsoid({0, 0.15f, 0}, vec3(0.1f), glow(rc, 1.5f), 12, 8); return true;
    }
    default: return false;
    }
}

}  // namespace lk
