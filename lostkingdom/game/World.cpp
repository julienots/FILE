#include "World.h"
#include <sstream>
#include "Props.h"
#include "../engine/core/Platform.h"
#include <algorithm>
#include <cstring>

namespace lk {

const char* regionName(int r) {
    static const char* n[] = {"Greenhaven", "Frostheim", "Ashen Lands", "Elderwood", "Royaume des Ombres", "High Crowns", "Sables d'Ambre", "Côte de Corail", "Steppes de Valdor", "Jungle d'Émeraude", "Terres d'Aurore"};
    return (r >= 0 && r < R_COUNT) ? n[r] : "?";
}
const char* regionSubtitle(int r) {
    static const char* n[] = {"Les plaines verdoyantes", "Le royaume gelé", "Les terres de cendre", "La forêt ancienne", "Les terres maudites", "Les cimes couronnées", "L'océan de dunes", "Les lagons d'Azur", "Les herbes du vent", "Le cœur vert du monde", "Le pays des cristaux vivants"};
    return (r >= 0 && r < R_COUNT) ? n[r] : "";
}

float distToSegment(vec2 p, vec2 a, vec2 b, float* t) {
    vec2 ab = b - a;
    float l2 = dot(ab, ab);
    float tt = l2 > 1e-6f ? saturate(dot(p - a, ab) / l2) : 0.f;
    if (t) *t = tt;
    return length(p - (a + ab * tt));
}

void Chunk::destroy() {
    terrain.destroy(); props.destroy(); foliage.destroy(); water.destroy(); lava.destroy(); grass.destroy(); flowers.destroy(); grassNear.destroy(); tallGrass.destroy();
    ready = false;
}

// ---------------------------------------------------------------------------------------- regions
struct Seed { vec2 p; int r; };
static const Seed SEEDS[] = {
    {{1500, 1500}, R_GREENHAVEN}, {{1850, 1250}, R_GREENHAVEN}, {{1300, 1850}, R_GREENHAVEN}, {{1700, 1800}, R_GREENHAVEN},
    {{900, 420}, R_FROSTHEIM}, {{380, 560}, R_FROSTHEIM}, {{1450, 380}, R_FROSTHEIM},
    {{2520, 2150}, R_ASHEN}, {{2750, 1650}, R_ASHEN}, {{2350, 2650}, R_ASHEN},
    {{430, 1450}, R_ELDERWOOD}, {{480, 2000}, R_ELDERWOOD}, {{620, 1050}, R_ELDERWOOD},
    {{1150, 2650}, R_SHADOWS}, {{650, 2650}, R_SHADOWS}, {{1650, 2780}, R_SHADOWS},
    {{2600, 620}, R_HIGHCROWNS}, {{2200, 420}, R_HIGHCROWNS}, {{2780, 1150}, R_HIGHCROWNS},
    // 2.0.0 the north-east range (the mountains go on east of Highspire)
    {{3350, 560}, R_HIGHCROWNS}, {{3800, 420}, R_HIGHCROWNS}, {{4150, 900}, R_HIGHCROWNS}, {{3500, 1250}, R_HIGHCROWNS}, {{4350, 1500}, R_HIGHCROWNS},
    // 2.0.0 the Amber Sands: a desert of dunes, mesas and oases in the south-east
    {{3450, 2250}, R_AMBER}, {{3950, 2000}, R_AMBER}, {{4300, 2500}, R_AMBER}, {{3800, 2950}, R_AMBER}, {{3350, 3050}, R_AMBER},
    {{4150, 3500}, R_AMBER}, {{3650, 3750}, R_AMBER}, {{4250, 4150}, R_AMBER}, {{3150, 3650}, R_AMBER},
    // 2.0.0 the Coral Coast: lagoons, palm groves and harbour towns along the southern sea
    {{450, 3450}, R_CORAL}, {{1000, 3700}, R_CORAL}, {{1600, 3500}, R_CORAL}, {{2200, 3750}, R_CORAL}, {{2800, 3900}, R_CORAL},
    {{700, 4150}, R_CORAL}, {{1400, 4200}, R_CORAL}, {{2000, 4250}, R_CORAL}, {{2600, 4250}, R_CORAL},
    // 2.4.0 the Valdor Steppes: rolling golden grassland east of the mountains and of the desert
    {{5000, 500}, R_STEPPE}, {{5600, 700}, R_STEPPE}, {{5100, 1300}, R_STEPPE}, {{5750, 1500}, R_STEPPE}, {{5050, 2050}, R_STEPPE}, {{5700, 2300}, R_STEPPE},
    // 2.4.0 the Emerald Jungle: the south-east, hot and wet, rivers, ruins of a forgotten people
    {{5100, 3000}, R_JUNGLE}, {{5750, 3300}, R_JUNGLE}, {{4950, 3800}, R_JUNGLE}, {{5600, 4100}, R_JUNGLE}, {{4300, 4900}, R_JUNGLE},
    {{5000, 4800}, R_JUNGLE}, {{5700, 5000}, R_JUNGLE}, {{4100, 5600}, R_JUNGLE}, {{4900, 5650}, R_JUNGLE}, {{5650, 5700}, R_JUNGLE},
    // 2.8.0 the Lands of Aurora: the island of living crystals in the south-west sea
    {{1100, 5000}, R_AURORA}, {{600, 5350}, R_AURORA}, {{1300, 5450}, R_AURORA}, {{2000, 5250}, R_AURORA}, {{2700, 5300}, R_AURORA},
    {{3150, 5600}, R_AURORA}, {{900, 5800}, R_AURORA}, {{1700, 5800}, R_AURORA}, {{2450, 5800}, R_AURORA},
};

RegionWeights World::regionAt(float x, float z) const {
    RegionWeights rw;
    // domain warp for organic borders
    float wx = x + 140.f * fbm2(x * 0.0025f, z * 0.0025f, 3, 501);
    float wz = z + 140.f * fbm2(x * 0.0025f + 31.7f, z * 0.0025f - 12.3f, 3, 502);
    float d2[R_COUNT];
    for (int i = 0; i < R_COUNT; i++) d2[i] = 1e18f;
    for (const Seed& s : SEEDS) {
        float dx = wx - s.p.x, dz = wz - s.p.y;
        float dd = dx * dx + dz * dz;
        if (dd < d2[s.r]) d2[s.r] = dd;
    }
    float d[R_COUNT];
    float dmin = 1e9f; int best = 0;
    for (int i = 0; i < R_COUNT; i++) { d[i] = std::sqrt(d2[i]); if (d[i] < dmin) { dmin = d[i]; best = i; } }
    float sum = 0;
    for (int i = 0; i < R_COUNT; i++) { rw.w[i] = std::exp(-(d[i] - dmin) / 45.f); sum += rw.w[i]; }
    for (int i = 0; i < R_COUNT; i++) rw.w[i] /= sum;
    rw.dominant = best;
    return rw;
}

static float terrace(float h, float step, float sharp) {
    float k = h / step, f = k - std::floor(k);
    float s = smoothstepf(0.5f - sharp, 0.5f + sharp, f);
    return (std::floor(k) + s) * step;
}

static const vec2 VOLCANO{2640, 2060};

// Domain warp: bends the noise lookups so ridges and hills meander instead of following the noise grid.
static vec2 warp2(float x, float z, float amp, float f, uint32_t seed) {
    return {x + amp * fbm2(x * f, z * f, 2, seed), z + amp * fbm2(x * f + 5.2f, z * f + 1.3f, 2, seed + 1)};
}

// 1.6.1 mountains: warped ridged noise, sharpened crests and wide valleys (pow), and gullies running down the
// mid slopes (small ridged detail weighted by the height): reads like eroded rock instead of soft blobs.
static float mountainField(float x, float z, float scale, uint32_t seed) {
    vec2 w = warp2(x, z, 110.f, 0.0015f, seed + 100);
    float r = ridged2(w.x * scale, w.y * scale, 5, seed);
    r = std::pow(saturate(r), 1.45f);
    float g = ridged2(w.x * scale * 4.3f, w.y * scale * 4.3f, 3, seed + 7);
    return r + 0.07f * g * smoothstepf(0.12f, 0.55f, r);
}

static float regionHeight(int r, float x, float z) {
    switch (r) {
    case R_GREENHAVEN: {
        // rolling hills on warped coordinates, plus a few higher knolls; kept gentle (villages, roads, rivers)
        vec2 w = warp2(x, z, 70.f, 0.002f, 13);
        float hills = 11.f * fbm2(w.x * 0.0035f, w.y * 0.0035f, 4, 11);
        float knolls = 9.f * smoothstepf(0.25f, 0.7f, fbm2(w.x * 0.006f, w.y * 0.006f, 3, 14));
        return 7.f + hills + knolls + 3.f * fbm2(x * 0.014f, z * 0.014f, 3, 12);
    }
    case R_FROSTHEIM: {
        float mask = smoothstepf(-0.15f, 0.35f, fbm2(x * 0.0016f, z * 0.0016f, 3, 21));
        return 10.f + 82.f * mountainField(x, z, 0.0028f, 22) * mask + 5.f * fbm2(x * 0.01f, z * 0.01f, 3, 23);
    }
    case R_ASHEN: {
        float h = 9.f + 16.f * ridged2(x * 0.004f, z * 0.004f, 4, 31) * smoothstepf(-0.2f, 0.4f, fbm2(x * 0.002f, z * 0.002f, 2, 32)) + 3.f * fbm2(x * 0.02f, z * 0.02f, 2, 33);
        float dv = length(vec2(x, z) - VOLCANO);
        float cone = 95.f * std::pow(saturate(1.f - dv / 300.f), 1.6f);
        float crater = 32.f * (1.f - smoothstepf(40.f, 75.f, dv));
        return std::max(h, h * 0.4f + cone - crater + 4.f * fbm2(x * 0.03f, z * 0.03f, 2, 34) * saturate(1.f - dv / 300.f));
    }
    case R_ELDERWOOD: {
        vec2 w = warp2(x, z, 60.f, 0.0025f, 43);
        return 9.f + 14.f * fbm2(w.x * 0.0045f, w.y * 0.0045f, 4, 41) + 4.f * fbm2(x * 0.02f, z * 0.02f, 2, 42);
    }
    case R_SHADOWS:
        return 1.1f + 4.5f * fbm2(x * 0.006f, z * 0.006f, 4, 51) + 1.f * fbm2(x * 0.03f, z * 0.03f, 2, 52);
    case R_HIGHCROWNS: {
        float base = 26.f + 100.f * mountainField(x, z, 0.0024f, 61) * smoothstepf(-0.3f, 0.3f, fbm2(x * 0.0017f, z * 0.0017f, 2, 62));
        return terrace(base, 9.f, 0.12f) + 2.f * fbm2(x * 0.02f, z * 0.02f, 2, 63);
    }
    case R_AMBER: {
        // 2.0.0 the Amber Sands: long crescent dunes with sharp crests (1 - |sin| of a meandering phase), a finer set of
        // ripples across them, and flat-topped mesas where the canyon mask rises (terraces with steep walls)
        vec2 w = warp2(x, z, 90.f, 0.0016f, 81);
        float ph = (w.x * 0.83f + w.y * 0.56f) * 0.021f + 2.6f * fbm2(w.x * 0.003f, w.y * 0.003f, 2, 83);
        float dune1 = std::pow(1.f - std::fabs(std::sin(ph)), 1.25f);
        float ph2 = (w.x * 0.5f - w.y * 0.87f) * 0.058f + 3.f * fbm2(w.x * 0.004f, w.y * 0.004f, 2, 84);
        float dune2 = 1.f - std::fabs(std::sin(ph2));
        float m = smoothstepf(0.18f, 0.5f, fbm2(x * 0.0011f + 9.f, z * 0.0011f - 4.f, 3, 85));
        float mesa = terrace(14.f + 40.f * ridged2(w.x * 0.0035f, w.y * 0.0035f, 4, 86), 10.f, 0.05f);
        return 6.f + (dune1 * 13.f + dune2 * 2.2f) * (1.f - m) + m * mesa + 1.0f * fbm2(x * 0.03f, z * 0.03f, 2, 87);
    }
    case R_STEPPE: {
        // 2.4.0 the Valdor Steppes: long low swells of grassland, a few rocky kopjes and dry gullies
        vec2 w = warp2(x, z, 120.f, 0.0012f, 141);
        float swell = 9.f * fbm2(w.x * 0.0018f, w.y * 0.0018f, 3, 142);
        float kopje = 22.f * std::pow(saturate(ridged2(w.x * 0.004f, w.y * 0.004f, 3, 143) - 0.35f) * 1.5f, 2.f) * smoothstepf(0.3f, 0.7f, fbm2(x * 0.0015f, z * 0.0015f, 2, 144) * 0.5f + 0.5f);
        return 10.f + swell + kopje + 1.2f * fbm2(x * 0.02f, z * 0.02f, 2, 145);
    }
    case R_JUNGLE: {
        // 2.4.0 the Emerald Jungle: steep wooded hills cut by ravines (rivers run there), limestone towers (karsts)
        vec2 w = warp2(x, z, 80.f, 0.002f, 151);
        float hills = 14.f * fbm2(w.x * 0.004f, w.y * 0.004f, 4, 152);
        float karst = 46.f * std::pow(saturate(fbm2(w.x * 0.0065f, w.y * 0.0065f, 3, 153) - 0.18f) * 1.9f, 3.f);
        return 8.f + hills + terrace(karst, 7.f, 0.08f) + 2.f * fbm2(x * 0.02f, z * 0.02f, 3, 154);
    }
    case R_AURORA: {
        // 2.8.0 the Lands of Aurora: soft turquoise meadows in wide basins, broken by sharp crystal ridges with
        // stepped flanks, and a few lone pinnacles
        vec2 w = warp2(x, z, 90.f, 0.0018f, 171);
        float basins = 10.f * fbm2(w.x * 0.003f, w.y * 0.003f, 4, 172);
        float ridge = std::pow(saturate(ridged2(w.x * 0.0042f, w.y * 0.0042f, 4, 173) - 0.42f) * 1.75f, 2.2f) * 38.f;
        float pin = 26.f * std::pow(saturate(fbm2(x * 0.011f, z * 0.011f, 2, 174) - 0.34f) * 2.6f, 4.f);
        return 7.f + basins + terrace(ridge, 6.f, 0.1f) + pin + 1.5f * fbm2(x * 0.025f, z * 0.025f, 2, 175);
    }
    case R_CORAL: {
        // 2.0.0 the Coral Coast: soft jungle hills, kept above sea level (the lagoons are carved by lakes)
        vec2 w = warp2(x, z, 70.f, 0.002f, 91);
        float h = 6.f + 10.f * fbm2(w.x * 0.0042f, w.y * 0.0042f, 4, 92) + 7.f * smoothstepf(0.25f, 0.7f, fbm2(w.x * 0.008f, w.y * 0.008f, 3, 93)) + 1.5f * fbm2(x * 0.02f, z * 0.02f, 3, 94);
        if (h < 2.6f) h = 2.6f - (2.6f - h) * 0.12f;
        return h;
    }
    }
    return 0;
}

float World::baseHeight(float x, float z, const RegionWeights& rw) const {
    float h = 0;
    for (int i = 0; i < R_COUNT; i++) if (rw.w[i] > 0.002f) h += rw.w[i] * regionHeight(i, x, z);
    // coastline: the kingdom is an island; the sea forms a natural boundary (2.4.0: an L-shaped land, see landEdgeDistance)
    float e = landEdgeDistance(x, z);
    e += 40.f * fbm2(x * 0.004f, z * 0.004f, 3, 71);
    float coast = smoothstepf(40.f, 230.f, e);
    h = lerpf(-14.f, h, coast);
    // 2.0.0 tropical islets off the south and east coasts (reachable by swimming): palms, beaches, a hidden chest
    if (e < 400.f && (z > 3000.f || x > 3000.f) && !(x > 3600.f && z > 4500.f && x < 3900.f)) {
        float n = fbm2(x * 0.011f, z * 0.011f, 3, 73) * 0.5f + 0.5f;
        float mask = smoothstepf(30.f, 90.f, e) * (1.f - smoothstepf(300.f, 390.f, e));
        float ih = lerpf(-14.f, 13.f, smoothstepf(0.52f, 0.74f, n));
        h = std::max(h, lerpf(-14.f, ih, mask));
    }
    return h;
}

// ---------------------------------------------------------------------------------------- plan
static void addRoad(World& w, std::vector<vec2> pts, float width, bool cobble) {
    // subdivide with meanders
    std::vector<vec2> out;
    for (size_t i = 0; i + 1 < pts.size(); i++) {
        vec2 a = pts[i], b = pts[i + 1];
        int n = std::max(2, (int)(length(b - a) / 40.f));
        for (int k = 0; k < n; k++) {
            float t = (float)k / n;
            vec2 p = lerp(a, b, t);
            vec2 nrm = perp(normalize(b - a));
            float off = (k == 0) ? 0.f : 18.f * fbm2(p.x * 0.01f, p.y * 0.01f, 2, 91) * std::sin(t * PI);
            out.push_back(p + nrm * off);
        }
    }
    out.push_back(pts.back());
    w.roads.push_back({out, width, cobble});
}

World::World() { }
World::~World() { stopWorkers(); }

void World::buildPlan() {
    pois.clear(); roads.clear(); rivers.clear(); lakes.clear(); structures.clear();
    auto P = [&](const char* id, const char* name, PoiType t, float x, float z, float r, int dungeon = -1, int boss = -1) {
        Poi p; p.id = id; p.name = name; p.type = t; p.pos = {x, z}; p.radius = r; p.dungeon = dungeon; p.boss = boss;
        p.region = regionId(x, z);
        pois.push_back(p);
    };
    // Greenhaven
    P("brindlemere", "Brindlemere", PoiType::Village, 1480, 1620, 72);
    P("millbrook", "Millbrook", PoiType::Mill, 1790, 1730, 62);
    P("fort_aldren", "Fort Aldren", PoiType::Castle, 1240, 1290, 78);
    P("vael_ruins", "Ruines de Vael", PoiType::Ruins, 1890, 1360, 38);
    P("whisper_cave", "Grotte des Murmures", PoiType::Cave, 1660, 1900, 22, 0);
    P("bandit_camp", "Camp des Écorcheurs", PoiType::Camp, 1990, 1590, 28);
    P("ws_green", "Pierre de Brindlemere", PoiType::Waystone, 1545, 1560, 10);
    P("farm_west", "Ferme des Aulnes", PoiType::Farm, 1320, 1560, 40);
    // Frostheim
    P("frostvik", "Frostvik", PoiType::Village, 850, 560, 70);
    P("frozen_lake", "Lac de Givre", PoiType::BossArena, 1180, 360, 60, -1, 2);
    P("hrimgard", "Forteresse de Hrimgard", PoiType::Castle, 520, 310, 62);
    P("ice_caves", "Cavernes de Hrimgard", PoiType::Cave, 610, 760, 22, 4);
    P("ws_frost", "Pierre de Frostvik", PoiType::Waystone, 920, 620, 10);
    // Ashen Lands
    P("cinderwatch", "Cinderwatch", PoiType::Village, 2300, 1660, 58);
    P("ash_nest", "Le Nid de Cendres", PoiType::BossArena, 2460, 1960, 55, -1, 3);
    P("ember_mines", "Mines de Brasier", PoiType::Mine, 2760, 1640, 24, 3);
    P("ashen_ruins", "Bastion Calciné", PoiType::Ruins, 2400, 2500, 50);
    P("ws_ash", "Pierre de Cinderwatch", PoiType::Waystone, 2245, 1610, 10);
    // Elderwood
    P("sylvarin", "Sylvarin", PoiType::Village, 520, 1560, 62);
    P("elven_ruins", "Sanctuaire d'Aelindor", PoiType::ElvenRuins, 350, 1200, 46);
    P("root_glade", "Clairière du Gardien", PoiType::BossArena, 640, 1860, 55, -1, 0);
    P("ws_elder", "Pierre de Sylvarin", PoiType::Waystone, 585, 1500, 10);
    // Shadows
    P("gravemoor", "Gravemoor", PoiType::Village, 1160, 2440, 56);
    P("graveyard", "Cimetière des Rois", PoiType::Graveyard, 900, 2560, 50, 1);
    P("morneval", "Château de Morneval", PoiType::Castle, 1520, 2690, 82, -1, 5);
    P("spider_den", "Antre de la Reine", PoiType::Cave, 700, 2300, 22, 2);
    P("abyss", "L'Abîme", PoiType::Cave, 1080, 2840, 30, 5);
    P("ws_shadow", "Pierre de Gravemoor", PoiType::Waystone, 1225, 2400, 10);
    // High Crowns
    P("highspire", "Highspire", PoiType::Monastery, 2350, 820, 68);
    P("astronomer", "Tour de l'Astronome", PoiType::Tower, 2700, 460, 30);
    P("titan_cirque", "Cirque des Titans", PoiType::BossArena, 2650, 1060, 60, -1, 6);
    P("ws_crown", "Pierre de Highspire", PoiType::Waystone, 2290, 870, 10);
    P("crown_mine", "Mine des Cimes", PoiType::Mine, 2150, 460, 22);

    // Rivers (from the mountains to the southern sea)
    River r1; r1.width = 14;
    r1.pts = {{1080, 690}, {1220, 980}, {1390, 1250}, {1430, 1480}, {1370, 1760}, {1260, 2050}, {1130, 2330}, {990, 2700}, {930, 3060},
              {910, 3300}, {890, 3650}, {860, 4000}, {840, 4380}};   // 2.0.0: on through the Coral Coast to the southern sea
    River r2; r2.width = 10;
    r2.pts = {{2280, 820}, {2120, 1040}, {1930, 1240}, {1760, 1360}};
    River r3; r3.width = 9;
    r3.pts = {{380, 1000}, {470, 1250}, {430, 1500}, {400, 1780}};
    // 2.0.0 the Sekh, a river that runs down from the north-east range through the Amber Sands into the oasis of Kheft
    River r4; r4.width = 9;
    r4.pts = {{3330, 1500}, {3300, 1850}, {3390, 2250}, {3470, 2650}, {3510, 2950}, {3548, 3120}};
    rivers = {r1, r2, r3, r4};

    // =========================================================================================
    // 2.0.0 -- the world grows: the Amber Sands (south-east), the Coral Coast (south), the north-east range, and a
    // few more villages on the old island. Positions are nudged to level, dry ground (findFlat) so the plan stays
    // valid whatever the terrain does, and kept clear of lakes, rivers and one another.
    // =========================================================================================
    struct Keep { vec2 c; float r; };
    std::vector<Keep> keep;
    for (const Poi& q : pois) keep.push_back({q.pos, q.radius + 20.f});
    // lakes and rivers of the new lands, declared first so that settlements avoid them (pushed to `lakes` below)
    static const struct { float x, z, r; } NEWLAKES[] = {
        {3560, 3190, 75}, {4150, 3050, 45}, {1150, 3850, 130}, {2100, 3950, 110}, {420, 3750, 85}, {3390, 3720, 50}, {3980, 3480, 28},
        {5450, 1640, 60}, {5200, 4000, 72}, {4480, 5300, 64}, {5700, 3700, 40}};   // 2.4.0 a steppe lake, jungle lagoons
    for (auto& l : NEWLAKES) keep.push_back({{l.x, l.z}, l.r + 38.f});
    auto nearRiver = [&](vec2 q, float m) {   // the rivers still hold their raw control points here (they are densified below)
        for (const River& rv : rivers)
            for (size_t i = 0; i + 1 < rv.pts.size(); i++) if (distToSegment(q, rv.pts[i], rv.pts[i + 1]) < m + rv.width + 45.f) return true;
        return false;
    };
    auto findFlat = [&](vec2 want, float r, float search, bool allowSlope = false) {
        vec2 best = want; float bestScore = 1e9f;
        for (float dz = -search; dz <= search + 0.1f; dz += 24.f)
            for (float dx = -search; dx <= search + 0.1f; dx += 24.f) {
                vec2 c = want + vec2(dx, dz);
                if (c.x < 260 || c.y < 260 || c.x > WORLD_SIZE - 260 || c.y > WORLD_SIZE - 260) continue;
                bool bad = false;
                for (const Keep& k : keep) if (length(k.c - c) < k.r + r) { bad = true; break; }
                if (bad || nearRiver(c, r)) continue;
                float lo = 1e9f, hi = -1e9f;
                for (int i = 0; i < 13 && !bad; i++) {
                    vec2 q = c;
                    if (i > 0) { float a = (i - 1) * TAU / 6.f + (i > 6 ? 0.5f : 0.f); q += vec2(std::cos(a), std::sin(a)) * r * (i > 6 ? 0.85f : 0.45f); }
                    float h = baseHeight(q.x, q.y, regionAt(q.x, q.y));
                    if (h < 2.0f) bad = true;
                    lo = std::min(lo, h); hi = std::max(hi, h);
                }
                if (bad) continue;
                float score = (hi - lo) * (allowSlope ? 0.4f : 1.f) + 0.012f * length(vec2(dx, dz));
                if (score < bestScore) { bestScore = score; best = c; }
            }
        return best;
    };
    auto Pf = [&](const char* id, const char* name, PoiType t, float x, float z, float r, int dungeon = -1, int boss = -1, float search = 150.f) {
        vec2 c = findFlat({x, z}, r, search, t == PoiType::Castle || t == PoiType::BossArena || t == PoiType::Temple);
        P(id, name, t, c.x, c.y, r, dungeon, boss);
        keep.push_back({c, r + 22.f});
    };
    // the sea shore at a given x (first z, going south, where the natural land drops under the water)
    auto shoreZ = [&](float x) { for (float z = 3600.f; z < WORLD_SIZE; z += 4.f) if (baseHeight(x, z, regionAt(x, z)) < 0.2f) return z; return WORLD_SIZE - 150.f; };

    // ---- the north-east range: the mountains go on east of Highspire
    Pf("hautgel", "Hautgel", PoiType::Village, 3080, 640, 58, -1, -1, 220);
    Pf("rempart_nord", "Rempart-du-Nord", PoiType::Castle, 3650, 980, 66, -1, -1, 260);
    Pf("naine_mine", "Mine des Naines", PoiType::Mine, 3450, 1380, 24, -1, -1, 200);
    Pf("ws_hautgel", "Pierre de Hautgel", PoiType::Waystone, 3140, 700, 10);
    // 2.2.1 the portal to the celestial realm, on the meadow east of Lac Miroir
    Pf("portal_ether", "Portail Céleste", PoiType::Temple, 1830, 1470, 26, 9, -1, 140);
    // ---- more villages for the old island
    Pf("chenebrune", "Chênebrune", PoiType::Village, 1960, 1980, 62, -1, -1, 200);
    Pf("saule_rieur", "Saule-Rieur", PoiType::Village, 1130, 1800, 56, -1, -1, 160);
    Pf("givrebourg", "Givrebourg", PoiType::Village, 1420, 650, 60, -1, -1, 160);
    Pf("auberclat", "Aubéclat", PoiType::Village, 360, 2150, 56, -1, -1, 160);
    Pf("braise_pic", "Braise-Pic", PoiType::Village, 2800, 2750, 54, -1, -1, 200);
    Pf("marais_noir", "Marais-Noir", PoiType::Village, 640, 2900, 52, -1, -1, 200);
    // ---- the Amber Sands
    Pf("solmar", "Solmar, Cité des Caravanes", PoiType::Village, 3850, 2700, 104, -1, -1, 220);
    Pf("kheft", "Oasis de Kheft", PoiType::Village, 3470, 3010, 66, -1, -1, 120);
    Pf("tamrak", "Tamrak", PoiType::Village, 3250, 3540, 62, -1, -1, 160);
    Pf("zafir", "Caravansérail de Zafir", PoiType::Village, 3760, 3260, 42, -1, -1, 160);
    Pf("sphinx", "La Gardienne des Sables", PoiType::Temple, 3400, 2330, 46, -1, -1, 200);
    Pf("sun_temple", "Temple du Soleil Mort", PoiType::Temple, 4080, 3560, 70, -1, -1, 220);
    { vec2 t = pois.back().pos; P("sun_temple_door", "Portes du Temple", PoiType::Temple, t.x, t.y + 17.f, 8, 6); }   // the dungeon entrance stands before the stair
    Pf("scorpion_nest", "Le Nid des Scorpions", PoiType::BossArena, 4300, 2950, 58, -1, 8, 220);
    Pf("amber_fort", "Fort Zénith", PoiType::Castle, 4250, 2300, 72, -1, -1, 220);
    Pf("amber_mine", "Mines d'Ambre", PoiType::Mine, 4130, 2680, 24, -1, -1, 160);
    Pf("mesa_tower", "Tour du Veilleur", PoiType::Tower, 3520, 1870, 30, -1, -1, 200);
    Pf("karun_ruins", "Colonnades de Karun", PoiType::Ruins, 3900, 3080, 46, -1, -1, 160);
    Pf("dune_raiders", "Camp des Pilleurs de Dunes", PoiType::Camp, 4180, 3900, 30, -1, -1, 220);
    Pf("ws_solmar", "Pierre de Solmar", PoiType::Waystone, 3935, 2640, 10);
    Pf("ws_kheft", "Pierre de Kheft", PoiType::Waystone, 3520, 3100, 10);
    Pf("ws_temple", "Pierre du Soleil Mort", PoiType::Waystone, 4010, 3470, 10);
    // ---- the Coral Coast
    {
        float z = shoreZ(1500.f) - 110.f;
        Pf("port_azur", "Port-Azur", PoiType::Village, 1500, z, 100, -1, -1, 40);
        pois.back().coastal = true;
    }
    Pf("pilotis", "Corail-sur-Pilotis", PoiType::Village, 560, 3870, 70, -1, -1, 160);
    Pf("sel_de_mer", "Sel-de-Mer", PoiType::Village, 2850, 3720, 58, -1, -1, 160);
    Pf("shirokawa", "Temple de Shirokawa", PoiType::Temple, 2500, 3850, 66, -1, -1, 200);
    Pf("silence_gate", "Sanctuaire du Silence", PoiType::Cave, 2590, 3950, 22, 8, -1, 80);
    {
        float z = shoreZ(2150.f) - 40.f;
        Pf("phare", "Phare des Brumes", PoiType::Tower, 2150, z, 28, -1, -1, 40);
    }
    Pf("pirate_cove", "Crique des Corsaires", PoiType::Camp, 1880, 4240, 36, -1, -1, 120);
    Pf("storm_deck", "Le Pont du Tonnerre", PoiType::BossArena, 1700, 4290, 50, -1, 9, 120);
    Pf("tide_cave", "Grotte des Marées", PoiType::Cave, 1230, 4260, 22, 7, -1, 120);
    Pf("wreck", "L'Épave du Léviathan", PoiType::Ruins, 760, 4300, 34, -1, -1, 120);
    Pf("ws_port", "Pierre de Port-Azur", PoiType::Waystone, 1580, 4150, 10);
    // ---- 2.8.0 the Lands of Aurora
    Pf("lumenhaven", "Lumenhaven", PoiType::Village, 1300, 5330, 62, -1, -1, 240);
    Pf("ws_lumen", "Pierre de Lumenhaven", PoiType::Waystone, 1390, 5250, 10, -1, -1, 120);
    Pf("crystal_crater", "Cratère Cristallin", PoiType::Ruins, 2450, 5600, 40, -1, -1, 240);
    Pf("ws_shiro", "Pierre de Shirokawa", PoiType::Waystone, 2430, 3790, 10);
    // ---- 2.4.0 the Valdor Steppes
    Pf("valdor", "Valdor, Cité des Chevaux", PoiType::Village, 5300, 1200, 92, -1, -1, 220);
    Pf("yourtes", "Campement du Khan", PoiType::Village, 5650, 620, 58, -1, -1, 200);
    Pf("pierres_levees", "Cercle des Pierres Levées", PoiType::Shrine, 5050, 1820, 34, -1, -1, 200);
    Pf("kurgan", "Tertre du Roi Cavalier", PoiType::Ruins, 5800, 1950, 40, -1, -1, 200);
    Pf("cavaliers_noirs", "Camp des Cavaliers Noirs", PoiType::Camp, 5500, 2350, 32, -1, -1, 200);
    Pf("tour_vents", "Tour des Vents", PoiType::Tower, 4950, 720, 28, -1, -1, 200);
    Pf("ws_valdor", "Pierre de Valdor", PoiType::Waystone, 5390, 1150, 10);
    Pf("ws_yourtes", "Pierre du Khan", PoiType::Waystone, 5560, 700, 10);
    // ---- 2.4.0 the Emerald Jungle
    Pf("emeraude", "Émeraude-sur-Lianes", PoiType::Village, 5300, 3400, 82, -1, -1, 220);
    Pf("xochi", "Xochi, la Cité des Cascades", PoiType::Village, 4600, 4900, 72, -1, -1, 220);
    Pf("temple_jaguar", "Temple du Jaguar", PoiType::Temple, 5500, 4600, 66, -1, -1, 220);
    { vec2 t = pois.back().pos; P("jaguar_door", "Portes du Jaguar", PoiType::Temple, t.x, t.y + 17.f, 8, 10); }
    Pf("tlaloc", "Ruines de Tlaloc", PoiType::Ruins, 4340, 5560, 44, -1, -1, 200);
    Pf("grotte_cascade", "Grotte de la Cascade", PoiType::Cave, 5800, 5300, 22, -1, -1, 200);
    Pf("pilleurs_tombes", "Camp des Pilleurs de Tombes", PoiType::Camp, 5000, 4300, 30, -1, -1, 200);
    Pf("ws_emeraude", "Pierre d'Émeraude", PoiType::Waystone, 5390, 3330, 10);
    Pf("ws_xochi", "Pierre de Xochi", PoiType::Waystone, 4690, 4830, 10);
    Pf("ws_jaguar", "Pierre du Jaguar", PoiType::Waystone, 5420, 4500, 10);

    // Lakes
    lakes.push_back({{1700, 1390}, 85, 0, false, false});      // Lac Miroir (level computed below)
    lakes.push_back({{1180, 360}, 120, 0, false, true});       // frozen lake (boss arena on ice)
    lakes.push_back({VOLCANO, 48, 0, true, false});            // crater lava lake
    lakes.push_back({{2560, 2320}, 30, 0, true, false});       // lava pool
    lakes.push_back({{2820, 1900}, 26, 0, true, false});
    lakes.push_back({{420, 1800}, 38, 0, false, false});       // magic spring
    lakes.push_back({{760, 420}, 40, 0, false, true});
    for (auto& l : NEWLAKES) lakes.push_back({{l.x, l.z}, l.r, 0, false, false});   // 2.0.0 oases and lagoons

    // lake levels: slightly below surrounding base terrain
    for (auto& l : lakes) {
        float m = 1e9f;
        for (int i = 0; i < 16; i++) {
            float a = i * TAU / 16;
            vec2 p = l.c + vec2(std::cos(a), std::sin(a)) * (l.r + 12);
            m = std::min(m, baseHeight(p.x, p.y, regionAt(p.x, p.y)));
        }
        l.level = l.lava ? std::max(m - 6.f, 2.f) : std::max(m - 1.2f, 0.4f);
        if (length(l.c - VOLCANO) < 10) l.level = baseHeight(VOLCANO.x + 75, VOLCANO.y, regionAt(VOLCANO.x + 75, VOLCANO.y)) - 14.f;
    }
    // river surfaces: densify, sample base terrain, enforce monotonic descent
    for (auto& rv : rivers) {
        std::vector<vec2> dense;
        for (size_t i = 0; i + 1 < rv.pts.size(); i++) {
            vec2 a = rv.pts[i], b = rv.pts[i + 1];
            int n = std::max(1, (int)(length(b - a) / 10.f));
            for (int k = 0; k < n; k++) {
                vec2 p = lerp(a, b, (float)k / n);
                vec2 nn = perp(normalize(b - a));
                p += nn * (22.f * fbm2(p.x * 0.006f, p.y * 0.006f, 2, 95));
                dense.push_back(p);
            }
        }
        dense.push_back(rv.pts.back());
        rv.pts = dense;
        rv.surf.resize(rv.pts.size());
        float prev = 1e9f;
        for (size_t i = 0; i < rv.pts.size(); i++) {
            float h = 1e9f;
            for (int k = -2; k <= 2; k++) {
                size_t j = (size_t)clampf((float)i + k, 0, (float)rv.pts.size() - 1);
                h = std::min(h, baseHeight(rv.pts[j].x, rv.pts[j].y, regionAt(rv.pts[j].x, rv.pts[j].y)));
            }
            float s = std::min(prev, h - 1.6f);
            s = std::max(s, 0.25f);
            rv.surf[i] = s;
            prev = s;
        }
    }
    // POI heights
    for (auto& p : pois) {
        float h = baseHeight(p.pos.x, p.pos.y, regionAt(p.pos.x, p.pos.y));
        p.height = std::max(h, 2.0f);
        if (p.coastal) p.height = 3.0f;   // 2.0.0 harbour: a low plain, so the beach and the sea stay where the piers are
        if (p.type == PoiType::BossArena && p.boss == 2) p.height = lakes[1].level + 0.02f;  // on the ice
    }
    // Roads between settlements
    auto pos = [&](const char* id) { return poiById(id)->pos; };
    addRoad(*this, {pos("brindlemere"), pos("millbrook")}, 5, false);
    addRoad(*this, {pos("millbrook"), {2050, 1640}, pos("cinderwatch")}, 5, false);
    addRoad(*this, {pos("brindlemere"), {1350, 1450}, pos("fort_aldren")}, 6, false);
    addRoad(*this, {pos("fort_aldren"), {1100, 1000}, {950, 780}, pos("frostvik")}, 5, false);
    addRoad(*this, {pos("brindlemere"), {1050, 1600}, {760, 1580}, pos("sylvarin")}, 5, false);
    addRoad(*this, {pos("brindlemere"), {1400, 1950}, {1250, 2250}, pos("gravemoor")}, 5, false);
    addRoad(*this, {pos("gravemoor"), {1350, 2580}, pos("morneval")}, 5, false);
    addRoad(*this, {pos("millbrook"), {1950, 1400}, {2150, 1100}, pos("highspire")}, 5, false);
    addRoad(*this, {pos("frostvik"), {700, 420}, pos("hrimgard")}, 4, false);
    addRoad(*this, {pos("gravemoor"), {1000, 2520}, pos("graveyard")}, 4, false);
    addRoad(*this, {pos("highspire"), {2550, 650}, pos("astronomer")}, 4, false);

    // 2.0.0 roads: caravan trails to the Amber Sands, the coastal highway, the north-east passes, links to the new villages
    addRoad(*this, {pos("cinderwatch"), {2700, 1900}, {3150, 2150}, pos("sphinx"), {3650, 2500}, pos("solmar")}, 5, false);
    addRoad(*this, {pos("solmar"), {3700, 2900}, pos("kheft")}, 5, false);
    addRoad(*this, {pos("solmar"), pos("zafir"), {3900, 3350}, pos("sun_temple")}, 5, false);
    addRoad(*this, {pos("kheft"), {3350, 3300}, pos("tamrak")}, 4, false);
    addRoad(*this, {pos("solmar"), {4000, 2500}, pos("amber_fort")}, 5, false);
    addRoad(*this, {pos("solmar"), {4000, 2680}, pos("amber_mine")}, 4, false);
    addRoad(*this, {pos("solmar"), pos("karun_ruins")}, 4, false);
    addRoad(*this, {pos("sphinx"), {3450, 2100}, pos("mesa_tower")}, 4, false);
    addRoad(*this, {pos("gravemoor"), {1180, 2900}, {1300, 3400}, pos("port_azur")}, 5, false);
    addRoad(*this, {pos("port_azur"), {1950, 4100}, pos("shirokawa"), {2850, 3600}, pos("sel_de_mer"), pos("tamrak")}, 5, false);
    addRoad(*this, {pos("port_azur"), pos("pirate_cove")}, 4, false);
    addRoad(*this, {pos("port_azur"), {1000, 4130}, {780, 4040}, pos("pilotis")}, 4, false);
    addRoad(*this, {pos("port_azur"), {1350, 4220}, pos("tide_cave")}, 3, false);
    addRoad(*this, {pos("shirokawa"), pos("silence_gate")}, 3, false);
    addRoad(*this, {pos("shirokawa"), {2300, 4200}, pos("phare")}, 3, false);
    addRoad(*this, {pos("highspire"), {2800, 700}, pos("hautgel"), {3350, 800}, pos("rempart_nord")}, 5, false);
    addRoad(*this, {pos("rempart_nord"), pos("naine_mine")}, 4, false);
    addRoad(*this, {pos("brindlemere"), {1700, 1750}, pos("chenebrune")}, 4, false);
    addRoad(*this, {pos("brindlemere"), {1300, 1700}, pos("saule_rieur")}, 4, false);
    addRoad(*this, {pos("frostvik"), {1150, 600}, pos("givrebourg")}, 4, false);
    addRoad(*this, {pos("sylvarin"), {430, 1850}, pos("auberclat")}, 4, false);
    addRoad(*this, {pos("cinderwatch"), {2550, 2200}, pos("braise_pic")}, 4, false);
    addRoad(*this, {pos("braise_pic"), {3000, 2800}, {3350, 3000}, pos("kheft")}, 4, false);
    addRoad(*this, {pos("gravemoor"), {900, 2700}, pos("marais_noir")}, 4, false);
    // 2.4.0 the new lands
    addRoad(*this, {pos("rempart_nord"), {4300, 900}, pos("tour_vents"), {5100, 1000}, pos("valdor")}, 5, false);
    addRoad(*this, {pos("amber_fort"), {4700, 2000}, {5000, 1600}, pos("valdor")}, 5, false);
    addRoad(*this, {pos("valdor"), {5500, 900}, pos("yourtes")}, 4, false);
    addRoad(*this, {pos("valdor"), {5150, 1550}, pos("pierres_levees")}, 4, false);
    addRoad(*this, {pos("valdor"), {5600, 1600}, pos("kurgan")}, 4, false);
    addRoad(*this, {pos("amber_fort"), {4650, 2600}, {4950, 3000}, pos("emeraude")}, 5, false);
    addRoad(*this, {pos("emeraude"), {5450, 4000}, pos("temple_jaguar")}, 4, false);
    addRoad(*this, {pos("emeraude"), {5000, 3900}, pos("pilleurs_tombes"), {4750, 4500}, pos("xochi")}, 4, false);
    addRoad(*this, {pos("sun_temple"), {4250, 4100}, {4450, 4600}, pos("xochi")}, 4, false);
    addRoad(*this, {pos("xochi"), {4400, 5250}, pos("tlaloc")}, 3, false);
    addRoad(*this, {pos("ws_port"), {1380, 4230}, {1180, 4520}, {1080, 4800}, {1150, 5100}, pos("lumenhaven")}, 4, false);   // 2.8.0 across the isthmus (from the waystone, round the harbour)
    addRoad(*this, {pos("lumenhaven"), {1900, 5450}, pos("crystal_crater")}, 3, false);

    // ---- structures ----------------------------------------------------------------------
    auto S = [&](int kind, vec2 p, float rot, int poi, float scale = 1, int variant = 0) {
        Structure s; s.kind = kind; s.pos = p; s.rot = rot; s.poi = poi; s.scale = scale; s.variant = variant; s.region = regionId(p.x, p.y);
        structures.push_back(s);
    };
    for (int pi = 0; pi < (int)pois.size(); pi++) {
        const Poi& p = pois[pi];
        Rng rng(hash32(pi * 977 + 13));
        int reg = p.region;
        auto ring = [&](int n, float rad, std::function<int(int)> kindFn, float jitter = 0.15f) {
            for (int i = 0; i < n; i++) {
                float a = i * TAU / n + rng.range(-jitter, jitter);
                vec2 q = p.pos + vec2(std::cos(a), std::sin(a)) * rad * rng.range(0.92f, 1.08f);
                // face the plaza
                float rot = std::atan2(p.pos.x - q.x, p.pos.y - q.y);
                S(kindFn(i), q, rot, pi, 1.f, rng.irange(0, 3));
            }
        };

        // ---- 2.0.0 settlements of the new lands -------------------------------------------------
        auto polar = [&](float a, float d) { return p.pos + vec2(std::cos(a), std::sin(a)) * d; };
        auto faceCentre = [&](vec2 q) { return std::atan2(p.pos.x - q.x, p.pos.y - q.y); };
        auto faceOut = [&](vec2 q) { return std::atan2(q.x - p.pos.x, q.y - p.pos.y); };
        // direction (angle) to the nearest open water around a settlement: where the harbour and the piers go
        auto seaAngle = [&]() {
            float best = 0.f, bh = 1e9f;
            for (int k = 0; k < 24; k++) {
                float a = k * TAU / 24; vec2 q = polar(a, p.radius * 1.5f + 30.f);
                float h = baseHeight(q.x, q.y, regionAt(q.x, q.y));
                for (const Lake& l : lakes) if (length(q - l.c) < l.r) h = std::min(h, l.level - 2.f);
                if (h < bh) { bh = h; best = a; }
            }
            return best;
        };
        auto desertTown = [&]() {
            float R = p.radius; bool big = R > 90.f, small = R < 50.f;
            S(S_WELL, p.pos, 0, pi, 1.2f);
            for (int i = 0; i < 2; i++) { float a = 0.5f + i * PI; vec2 q = polar(a, R * 0.2f); S(S_BAZAAR, q, faceCentre(q), pi, 1.f, i); }
            if (!small) for (int i = 0; i < 4; i++) { float a = 1.57f * i + 1.0f; vec2 q = polar(a, R * 0.27f); S(S_STALL, q, faceCentre(q), pi, 1.f, i % 3); }
            for (int i = 0; i < 6; i++) S(S_LANTERN, polar(i * TAU / 6 + 0.2f, R * 0.33f), 0, pi);
            int nA = small ? 5 : (big ? 11 : 8), nB = small ? 6 : (big ? 17 : 12);
            for (int i = 0; i < nA; i++) { vec2 q = polar(i * TAU / nA + rng.range(-0.12f, 0.12f), R * rng.range(0.46f, 0.54f)); S(S_DOMEHOUSE, q, faceCentre(q), pi, rng.range(0.95f, 1.1f), (i + 1) % 3); }
            for (int i = 0; i < nB; i++) {
                float a = (i + 0.5f) * TAU / nB + rng.range(-0.1f, 0.1f); vec2 q = polar(a, R * rng.range(0.74f, 0.84f));
                int kind = i == 2 ? S_FORGE : (i == 5 && !small ? S_BAZAAR : S_DOMEHOUSE);
                S(kind, q, faceCentre(q), pi, 1.f, i % 3);
            }
            int nm = big ? 3 : (small ? 0 : 1);
            for (int i = 0; i < nm; i++) { vec2 q = polar(2.1f + i * 2.2f, R * 0.64f); S(S_MINARET, q, 0, pi, 1.f, i); }
            if (big) { vec2 q = polar(-1.1f, R * 0.62f); S(S_STATUE, q, faceCentre(q), pi, 1.5f); }
            if (!small) for (int i = 0; i < 2; i++) { vec2 q = polar(1.57f + i * PI, R * 0.99f); S(S_ARCHWAY, q, faceOut(q), pi, 1.f, i); }
            if (!small) for (int i = 0; i < 2; i++) { vec2 q = polar(0.9f + i * 3.4f, R * 0.95f); S(S_PEN, q, rng.range(0, TAU), pi); }
            for (int i = 0; i < (small ? 4 : 10); i++) { vec2 q = polar(rng.range(0, TAU), rng.range(R * 0.2f, R * 0.7f)); S(i % 3 == 0 ? S_LANTERN : S_STALL, q, rng.range(0, TAU), pi, 1.f, -1 - (i % 3)); }
        };
        auto coastTown = [&]() {
            float R = p.radius; bool lagoon = p.id != "port_azur";
            float sea = seaAngle();
            vec2 sd(std::cos(sea), std::sin(sea));
            S(S_WELL, p.pos - sd * (R * 0.1f), 0, pi, 1.1f);
            // fish market hall on the quay, an inn facing the water
            { vec2 q = p.pos + sd * (R * 0.42f); S(S_BAZAAR, q, faceOut(q), pi, 1.f, 1); }
            { vec2 q = p.pos + sd * (R * 0.2f) + vec2(-sd.y, sd.x) * (R * 0.34f); S(S_INN, q, faceOut(q), pi, 1.f, 1); }
            // houses in two arcs behind the waterfront (all around but the sea side)
            int nA = lagoon ? 7 : 10, nB = lagoon ? 10 : 16;
            for (int i = 0; i < nA + nB; i++) {
                bool inner = i < nA;
                int n = inner ? nA : nB;
                int k = inner ? i : i - nA;
                float span = inner ? 4.4f : 4.9f;                       // radians covered (the sea side stays open for the quay)
                float a = sea + PI + (k + 0.5f - n * 0.5f) / n * span + rng.range(-0.05f, 0.05f);
                vec2 q = polar(a, R * (inner ? 0.5f : 0.8f) * rng.range(0.94f, 1.06f));
                int kind = (i % 4 == 1) ? S_BEACHHUT : (i % 7 == 3 ? S_HOUSE : S_DOMEHOUSE);
                if (i == 2) kind = S_FORGE;
                if (i == 6 && !lagoon) kind = S_CHAPEL;
                S(kind, q, faceCentre(q), pi, 1.f, (i + 1) % 3);
            }
            // waterfront row of boathouses and net racks
            for (int i = 0; i < 5; i++) { float a = sea + (i - 2) * 0.33f; vec2 q = polar(a, R * 0.72f); S(i % 2 ? S_BEACHHUT : S_STILT_HUT, q, faceOut(q), pi, 1.f, i); }
            // quay and piers: chains of 14 m piers running out over the water, three rows
            // distance from the centre to the water's edge along the sea direction (natural terrain / lagoon rim)
            float shore = R * 0.9f;
            for (float d = R * 0.5f; d < R * 3.f; d += 3.f) {
                vec2 q = p.pos + sd * d; bool wet = baseHeight(q.x, q.y, regionAt(q.x, q.y)) < 0.4f;
                for (const Lake& l : lakes) if (length(q - l.c) < l.r * 0.97f) wet = true;
                if (wet) { shore = d; break; }
            }
            for (int row = -1; row <= 1; row++) {
                vec2 lat(-sd.y, sd.x);
                float rot = std::atan2(sd.x, sd.y);
                int len = (row == 0 && !lagoon) ? 4 : 3;
                for (int k = 0; k < len; k++) S(S_PIER, p.pos + sd * (shore - 3.f + k * 14.f) + lat * (row * 19.f), rot, pi, 1.f, row + k);
            }
            if (!lagoon) { vec2 q = p.pos + sd * (R * 0.6f) + vec2(-sd.y, sd.x) * (-R * 0.5f); S(S_TOWER, q, 0, pi, 1.f, 0); S(S_CHAPEL, polar(sea + PI + 0.3f, R * 0.35f), faceCentre(polar(sea + PI + 0.3f, R * 0.35f)), pi, 1.f, 0); }
            for (int i = 0; i < 8; i++) S(S_LANTERN, polar(sea + (i - 3.5f) * 0.38f, R * 0.56f), 0, pi);
            for (int i = 0; i < 10; i++) { vec2 q = polar(rng.range(0, TAU), rng.range(R * 0.2f, R * 0.7f)); S(i % 3 == 0 ? S_LANTERN : S_STALL, q, rng.range(0, TAU), pi, 1.f, -1 - (i % 3)); }
        };
        switch (p.type) {
        case PoiType::Village: case PoiType::Mill: case PoiType::Monastery: {
            if (p.type == PoiType::Village && reg == R_AMBER) { desertTown(); break; }
            if (p.type == PoiType::Village && reg == R_CORAL) { coastTown(); break; }
            S(S_WELL, p.pos, 0, pi);
            int house = reg == R_AURORA ? S_LONGHOUSE : reg == R_FROSTHEIM ? S_LONGHOUSE : (reg == R_SHADOWS ? S_STILT_HUT : (reg == R_HIGHCROWNS || reg == R_ASHEN ? S_STONEHOUSE : S_HOUSE));
            int n = p.type == PoiType::Village ? 9 : 6;
            ring(n, p.radius * 0.62f, [&](int i) {
                if (i == 0) return (int)S_INN;
                if (i == 1) return (int)S_FORGE;
                if (i == 2 && reg == R_ELDERWOOD) return (int)S_ELVEN_ARCH;
                if (i == 3 && p.type == PoiType::Village && reg == R_GREENHAVEN) return (int)S_CHAPEL;
                if (reg == R_ELDERWOOD) return i % 2 ? (int)S_TENT : (int)S_HOUSE;
                if (reg == R_ASHEN && i % 3 == 0) return (int)S_TENT;
                return i % 4 == 3 ? (int)S_STONEHOUSE : house;
            });
            // market stalls & lanterns around the plaza
            for (int i = 0; i < 4; i++) {
                float a = i * TAU / 4 + 0.4f;
                vec2 q = p.pos + vec2(std::cos(a), std::sin(a)) * 13.f;
                S(i < 2 ? S_STALL : S_LANTERN, q, std::atan2(p.pos.x - q.x, p.pos.y - q.y), pi);
            }
            for (int i = 0; i < 6; i++) {
                float a = i * TAU / 6 + 0.1f;
                S(S_LANTERN, p.pos + vec2(std::cos(a), std::sin(a)) * (p.radius * 0.38f), 0, pi);
            }
            for (int i = 0; i < 10; i++) {
                float a = rng.range(0, TAU), d = rng.range(15.f, p.radius * 0.45f);
                vec2 q = p.pos + vec2(std::cos(a), std::sin(a)) * d;
                S(i % 3 == 0 ? S_LANTERN : S_STALL, q, rng.range(0, TAU), pi, 1.f, -1 - (i % 3));  // variant<0 = clutter
            }
            if (p.type == PoiType::Mill) {
                S(S_WINDMILL, p.pos + vec2(26, -18), 0.6f, pi);
                for (int i = 0; i < 4; i++) S(S_FIELD, p.pos + vec2(-40.f + i * 22.f, 52.f), 0.f, pi, 1.f, i);
            }
            if (p.type == PoiType::Monastery) { S(S_MONASTERY, p.pos + vec2(0, -34), 0, pi); S(S_TOWER, p.pos + vec2(34, 20), 0, pi); }
            if (reg == R_GREENHAVEN && p.type == PoiType::Village) {
                for (int i = 0; i < 3; i++) S(S_FIELD, p.pos + vec2(70.f + i * 22.f, -10.f), 0.1f, pi, 1.f, i);
                S(S_WINDMILL, p.pos + vec2(78, 30), 0.3f, pi);
            }
            if (reg == R_SHADOWS || reg == R_ASHEN) for (int i = 0; i < 10; i++) {
                float a = i * TAU / 10;
                S(S_PALISADE, p.pos + vec2(std::cos(a), std::sin(a)) * p.radius * 0.95f, -a, pi);
            }
            // 2.0.0 livelier villages: a second ring of homes with their yards, a pen or two, and a bakery-sized forge
            if (p.type == PoiType::Village && p.radius >= 50.f) {
                int n2 = (int)(p.radius / 6.2f);
                for (int i = 0; i < n2; i++) {
                    float a = (i + 0.5f) * TAU / n2 + rng.range(-0.12f, 0.12f);
                    vec2 q = p.pos + vec2(std::cos(a), std::sin(a)) * p.radius * ((reg == R_SHADOWS || reg == R_ASHEN) ? rng.range(0.7f, 0.78f) : rng.range(0.86f, 0.96f));
                    int kind = reg == R_FROSTHEIM ? S_LONGHOUSE : (reg == R_SHADOWS ? S_STILT_HUT : (reg == R_ASHEN || reg == R_HIGHCROWNS ? S_STONEHOUSE : S_HOUSE));
                    if (i % 5 == 4) kind = S_STONEHOUSE;
                    if (reg == R_ELDERWOOD) kind = i % 2 ? S_HOUSE : S_TENT;
                    S(kind, q, std::atan2(p.pos.x - q.x, p.pos.y - q.y), pi, 1.f, rng.irange(0, 3));
                }
                for (int i = 0; i < 2; i++) { float a = rng.range(0, TAU); S(S_PEN, p.pos + vec2(std::cos(a), std::sin(a)) * p.radius * 0.8f, rng.range(0, TAU), pi); }
            }
            break;
        }
        case PoiType::Castle: {
            int n = 8;
            for (int i = 0; i < n; i++) {
                float a = i * TAU / n;
                vec2 t = p.pos + vec2(std::cos(a), std::sin(a)) * p.radius * 0.8f;
                S(S_CASTLE_TOWER, t, a, pi, 1.f, reg == R_SHADOWS ? 1 : (reg == R_FROSTHEIM ? 2 : (reg == R_AMBER ? 3 : 0)));
                float a2 = (i + 0.5f) * TAU / n;
                vec2 m = p.pos + vec2(std::cos(a2), std::sin(a2)) * p.radius * 0.8f * std::cos(PI / n);
                if (i == 1) S(S_GATE, m, -a2 + PI / 2, pi, 1.f, reg == R_SHADOWS ? 1 : 0);
                else S(S_CASTLE_WALL, m, -a2 + PI / 2, pi, 2.f * p.radius * 0.8f * std::sin(PI / n) / 10.f, reg == R_SHADOWS ? 1 : (reg == R_FROSTHEIM ? 2 : (reg == R_AMBER ? 3 : 0)));
            }
            if (p.boss < 0) S(S_KEEP, p.pos + vec2(0, -p.radius * 0.25f), 0, pi, 1.f, reg == R_FROSTHEIM ? 2 : (reg == R_AMBER ? 3 : 0));
            else S(S_STATUE, p.pos + vec2(0, -p.radius * 0.45f), 0, pi, 1.6f);
            for (int i = 0; i < 4; i++) S(S_LANTERN, p.pos + vec2(i < 2 ? -12.f : 12.f, i % 2 ? -12.f : 12.f), 0, pi);
            break;
        }
        case PoiType::Temple: {
            if (p.id == "sun_temple_door" || p.id == "jaguar_door") break;   // just the entrance trigger, in front of the ziggurat
            if (p.id == "portal_ether") { S(S_PORTAL, p.pos, 0, pi, 1.f); for (int i = 0; i < 6; i++) { float a = i * TAU / 6 + 0.5f; if (std::sin(a) > 0.4f) continue; S(S_OBELISK, p.pos + vec2(std::cos(a), std::sin(a)) * 15.f, -a, pi, 1.1f, 2); } break; }   // none in front of the gate
            if (p.id == "sphinx") {
                S(S_SPHINX, p.pos, 0.9f, pi, 1.f);
                for (int s2 = -1; s2 <= 1; s2 += 2) { vec2 q = p.pos + rotate(vec2(s2 * 14.f, 6.f), -0.9f); S(S_COLONNADE, q, 0.9f + PI * 0.5f, pi, 1.f, s2 + 1); S(S_OBELISK, rotate(vec2(s2 * 9.f, 15.f), -0.9f) + p.pos, 0, pi, 1.3f, 2); }
                S(S_ALTAR, p.pos + rotate(vec2(0, 13.f), -0.9f), 0.9f + PI, pi);
            } else if (p.id == "sun_temple") {
                S(S_ZIGGURAT, p.pos, 0, pi, 1.f);
                for (int i = 0; i < 4; i++) { float a = i * PI / 2 + PI / 4; S(S_OBELISK, p.pos + vec2(std::cos(a), std::sin(a)) * p.radius * 0.88f, 0, pi, 1.4f, i); }
                for (int i = 0; i < 4; i++) { float a = i * PI / 2 + PI / 4 + 0.6f; vec2 q = p.pos + vec2(std::cos(a), std::sin(a)) * p.radius * 0.8f; S(S_COLONNADE, q, std::atan2(p.pos.x - q.x, p.pos.y - q.y), pi, 1.f, i); }
                S(S_ARCHWAY, p.pos + vec2(0, 25.f), PI, pi, 1.f, 2);
                for (int i = 0; i < 6; i++) S(S_LANTERN, p.pos + vec2(i < 3 ? -7.5f : 7.5f, 14.f + (i % 3) * 5.f), 0, pi);
            } else if (p.id == "temple_jaguar") {   // 2.4.0 a stepped pyramid swallowed by the jungle
                S(S_ZIGGURAT, p.pos, 0, pi, 0.9f, 1);
                for (int i = 0; i < 6; i++) { float a = i * TAU / 6 + 0.3f; vec2 q = p.pos + vec2(std::cos(a), std::sin(a)) * p.radius * 0.85f; if (q.y > p.pos.y + 10.f) continue; S(S_STATUE, q, std::atan2(p.pos.x - q.x, p.pos.y - q.y), pi, 1.1f); }
                S(S_ARCHWAY, p.pos + vec2(0, 25.f), PI, pi, 1.f, 1);
                for (int i = 0; i < 4; i++) S(S_LANTERN, p.pos + vec2(i < 2 ? -6.5f : 6.5f, 14.f + (i % 2) * 6.f), 0, pi);
            } else if (p.id == "shirokawa") {
                S(S_PAGODA, p.pos, 0, pi, 1.f);
                for (int i = 0; i < 4; i++) { float a = 0.9f + i * 1.45f; vec2 q = p.pos + vec2(std::cos(a), std::sin(a)) * p.radius * 0.72f; S(S_HOUSE, q, std::atan2(p.pos.x - q.x, p.pos.y - q.y), pi, 1.f, i); }
                S(S_WELL, p.pos + vec2(14.f, 20.f), 0, pi);
                for (int i = 0; i < 8; i++) S(S_LANTERN, p.pos + vec2(i < 4 ? -4.f : 4.f, 16.f + (i % 4) * 4.5f), 0, pi);
                S(S_STATUE, p.pos + vec2(-16.f, 22.f), 0.4f, pi, 0.9f);
            } else {
                S(S_ALTAR, p.pos, 0, pi);
            }
            break;
        }
        case PoiType::Ruins: case PoiType::ElvenRuins: {
            if (p.type == PoiType::Ruins && reg == R_AMBER) {
                // Karun: a fallen colonnaded court
                for (int i = 0; i < 3; i++) { float a = i * TAU / 3 + 0.4f; vec2 q = p.pos + vec2(std::cos(a), std::sin(a)) * p.radius * 0.55f; S(S_COLONNADE, q, a + PI / 2, pi, 1.f, i); }
                S(S_ARCHWAY, p.pos + vec2(0, -p.radius * 0.72f), 0, pi, 1.f, 1);
                const Poi* from = poiById("solmar");   // keep the approach road clear of fallen walls
                float roadA = from ? std::atan2(from->pos.y - p.pos.y, from->pos.x - p.pos.x) : 0.f;
                for (int i = 0; i < 5; i++) { float a = rng.range(0, TAU), d = rng.range(8, p.radius * 0.85f); if (std::fabs(std::remainder(a - roadA, TAU)) < 0.8f) a += PI; S(i % 2 ? S_RUIN_WALL : S_OBELISK, p.pos + vec2(std::cos(a), std::sin(a)) * d, rng.range(0, TAU), pi, rng.range(0.8f, 1.3f), 0); }
                S(S_ALTAR, p.pos, 0, pi);
                break;
            }
            std::vector<vec2> placed;   // 2.4.0 no fallen wall on the approach road, none on top of another
            for (int i = 0; i < 7; i++) {
                vec2 q; bool ok = false;
                for (int tries = 0; tries < 14 && !ok; tries++) {
                    float a = rng.range(0, TAU), d = rng.range(6, p.radius * 0.8f);
                    q = p.pos + vec2(std::cos(a), std::sin(a)) * d;
                    ok = true;   // (the road index isn't built yet here: test the road polylines directly)
                    for (const Road& rd : roads) { for (size_t k = 0; k + 1 < rd.pts.size() && ok; k++) if (distToSegment(q, rd.pts[k], rd.pts[k + 1]) < rd.width * 0.5f + 6.f) ok = false; if (!ok) break; }
                    for (vec2 o : placed) if (length(o - q) < 8.f) ok = false;
                }
                if (!ok) continue;
                placed.push_back(q);
                if (p.type == PoiType::ElvenRuins) S(i % 3 == 0 ? S_ELVEN_ARCH : (i % 3 == 1 ? S_OBELISK : S_RUIN_WALL), q, rng.range(0, TAU), pi, rng.range(0.8f, 1.2f), 1);
                else S(i % 2 ? S_RUIN_WALL : S_OBELISK, q, rng.range(0, TAU), pi, rng.range(0.8f, 1.3f), 0);
            }
            S(S_ALTAR, p.pos, 0, pi);
            break;
        }
        case PoiType::Cave: S(S_CAVE, p.pos, rng.range(0, TAU), pi, p.id == std::string("abyss") ? 1.6f : 1.f, p.id == std::string("abyss") ? 1 : 0); break;
        case PoiType::Mine: S(S_MINE, p.pos, rng.range(0, TAU), pi); S(S_CAMPFIRE, p.pos + vec2(8, 6), 0, pi); S(S_TENT, p.pos + vec2(-10, 8), 0.5f, pi); break;
        case PoiType::Tower:
            if (p.id == "phare") { S(S_LIGHTHOUSE, p.pos, 0, pi, 1.f); S(S_BEACHHUT, p.pos + vec2(10.f, 8.f), 0.5f, pi, 1.f, 1); S(S_CAMPFIRE, p.pos + vec2(-9, 7), 0, pi); }
            else { S(S_TOWER, p.pos, 0, pi, 1.6f, 1); S(S_CAMPFIRE, p.pos + vec2(9, 7), 0, pi); }
            break;
        case PoiType::Waystone: S(S_WAYSTONE, p.pos, 0, pi); break;
        case PoiType::Camp: for (int i = 0; i < 4; i++) S(S_TENT, p.pos + vec2(std::cos(i * 1.6f), std::sin(i * 1.6f)) * 12.f, i * 1.6f + PI, pi); S(S_CAMPFIRE, p.pos, 0, pi); for (int i = 0; i < 8; i++) { float a = i * TAU / 8; S(S_PALISADE, p.pos + vec2(std::cos(a), std::sin(a)) * p.radius * 0.95f, -a, pi); } break;
        case PoiType::Graveyard: S(S_CRYPT, p.pos, 0, pi); S(S_CHAPEL, p.pos + vec2(-26, -20), 0.3f, pi, 0.9f, 1); for (int i = 0; i < 4; i++) S(S_LANTERN, p.pos + vec2(i < 2 ? -9.f : 9.f, i % 2 ? 9.f : 4.f), 0, pi); break;
        case PoiType::BossArena: {
            if (p.id == "storm_deck") {   // a wrecked jetty: two piers running into the sea and a ring of broken masts (obelisks)
                vec2 q = p.pos; float sea = 1.5707963f;
                for (int k = 0; k < 3; k++) S(S_PIER, q + vec2(-9.f, 7.f + k * 14.f), 0, pi, 1.f, k);
                for (int k = 0; k < 3; k++) S(S_PIER, q + vec2(9.f, 7.f + k * 14.f), 0, pi, 1.f, k + 1);
                for (int i = 0; i < 6; i++) { float a = i * TAU / 6 + 0.3f; if (std::sin(a) > 0.3f) continue; S(S_OBELISK, q + vec2(std::cos(a), std::sin(a)) * p.radius * 0.8f, -a, pi, 0.8f, 0); }
                S(S_ALTAR, q + vec2(0, -p.radius * 0.5f), 0, pi); (void)sea;
                break;
            }
            int n = 10;
            for (int i = 0; i < n; i++) {
                float a = i * TAU / n;
                if (p.boss == 2) continue;  // frozen lake: open
                S(p.boss == 0 ? S_ELVEN_ARCH : S_OBELISK, p.pos + vec2(std::cos(a), std::sin(a)) * p.radius * 0.95f, -a, pi, 0.9f, p.boss == 0 ? 1 : 0);
            }
            S(S_ALTAR, p.pos + vec2(0, p.radius * 0.6f), 0, pi);
            break;
        }
        case PoiType::Farm: S(S_HOUSE, p.pos, 0.4f, pi); S(S_WINDMILL, p.pos + vec2(-22, 10), 0, pi); for (int i = 0; i < 3; i++) S(S_FIELD, p.pos + vec2(14.f + i * 20.f, 22.f), 0, pi, 1.f, i); break;
        default: break;
        }
    }
    // Bridges where roads cross rivers
    for (auto& rd : roads)
        for (size_t i = 0; i + 1 < rd.pts.size(); i++)
            for (auto& rv : rivers)
                for (size_t j = 0; j + 1 < rv.pts.size(); j++) {
                    vec2 a = rd.pts[i], b = rd.pts[i + 1], c = rv.pts[j], d = rv.pts[j + 1];
                    vec2 r = b - a, s = d - c;
                    float den = r.x * s.y - r.y * s.x;
                    if (std::fabs(den) < 1e-5f) continue;
                    float t = ((c.x - a.x) * s.y - (c.y - a.y) * s.x) / den, u = ((c.x - a.x) * r.y - (c.y - a.y) * r.x) / den;
                    if (t >= 0 && t <= 1 && u >= 0 && u <= 1) {
                        vec2 x = a + r * t;
                        Structure st; st.kind = S_BRIDGE; st.pos = x; st.rot = std::atan2(r.x, r.y); st.scale = (rv.width + 14.f) / 16.f;
                        st.region = regionId(x.x, x.y); st.variant = (int)(rv.surf[j] * 100);  // deck reference height (cm)
                        structures.push_back(st);
                    }
                }
    // 2.8.0 the Lands of Aurora: floating islands, each with a launch geyser at its foot, and free geysers to play with.
    // Placed on dry land away from settlements and lakes (baseHeight only: the road index is not built yet).
    {
        Rng gr(0xA0A0A0u);
        auto dry = [&](vec2 p, float m) {
            if (regionId(p.x, p.y) != R_AURORA || baseHeight(p.x, p.y, regionAt(p.x, p.y)) < 4.f) return false;
            for (const Poi& q : pois) if (length(q.pos - p) < q.radius + m) return false;
            for (const Lake& l : lakes) if (length(l.c - p) < l.r + m) return false;
            for (const Road& rd : roads) for (size_t i = 0; i + 1 < rd.pts.size(); i++) if (distToSegment(p, rd.pts[i], rd.pts[i + 1]) < rd.width + std::min(m, 10.f)) return false;
            for (const Structure& s : structures) if ((s.kind == S_SKYISLAND || s.kind == S_GEYSER) && length(s.pos - p) < (s.kind == S_SKYISLAND ? 60.f : 25.f)) return false;
            return true;
        };
        int islands = 0;
        for (int tries = 0; tries < 400 && islands < 9; tries++) {
            vec2 p{gr.range(450.f, 3200.f), gr.range(5050.f, 5900.f)};
            if (!dry(p, 30.f)) continue;
            float a = gr.range(0, TAU);
            vec2 dir{std::sin(a), std::cos(a)};
            vec2 gp = p + dir * (SKY_ISLAND_R + 3.5f);
            if (!dry(gp, 12.f) && regionId(gp.x, gp.y) != R_AURORA) continue;
            float top = 0;
            for (int k = 0; k < 9; k++) { vec2 q = p + vec2(std::cos(k * 0.7f), std::sin(k * 0.7f)) * (k ? SKY_ISLAND_R : 0.f); top = std::max(top, baseHeight(q.x, q.y, regionAt(q.x, q.y))); }
            float gh = baseHeight(gp.x, gp.y, regionAt(gp.x, gp.y));
            float deck = std::max(top + 9.f, gh + gr.range(14.f, 17.f));
            if (deck - gh > 18.f) continue;   // the geyser must reach it
            Structure si; si.kind = S_SKYISLAND; si.pos = p; si.rot = gr.range(0, TAU); si.region = R_AURORA; si.variant = (int)(deck * 100.f);
            structures.push_back(si);
            Structure ge; ge.kind = S_GEYSER; ge.pos = gp; ge.rot = std::atan2(-dir.x, -dir.y); ge.region = R_AURORA; ge.variant = (int)((deck - gh) * 100.f);   // aims at the island
            structures.push_back(ge);
            islands++;
        }
        int free_ = 0;
        for (int tries = 0; tries < 300 && free_ < 14; tries++) {
            vec2 p{gr.range(400.f, 3300.f), gr.range(4950.f, 5950.f)};
            if (!dry(p, 15.f)) continue;
            Structure ge; ge.kind = S_GEYSER; ge.pos = p; ge.rot = 0; ge.region = R_AURORA; ge.variant = 0;   // straight up
            structures.push_back(ge);
            free_++;
        }
    }
    indexPlan();
    resolvePlacement();
    indexPlan();
    levelBuildings();
}

// 1.12.0 placement pass: the village layouts are drawn before the roads, so a house could stand in the middle of
// the road through the village, a lantern on the cobbles, two market stalls in each other. Each building is
// pushed sideways off the road (down the slope of the road influence) and away from its neighbours; what cannot
// find room within a few metres is removed (only small furniture: lanterns, stalls, wells, banners).
void World::resolvePlacement() {
    auto movable = [](int k) {
        return k != S_BRIDGE && k != S_FIELD && k != S_PEN && k != S_DOCK && k != S_CASTLE_WALL && k != S_GATE && k != S_PALISADE &&
               k != S_CAVE && k != S_MINE && k != S_WAYSTONE && k != S_RUIN_WALL && k != S_KEEP && k != S_PORTAL && k != S_FENCE &&
               k != S_PIER && k != S_GEYSER && k != S_SKYISLAND && k != S_ZIGGURAT && k != S_SPHINX && k != S_PAGODA && k != S_ARCHWAY && k != S_COLONNADE && k != S_LIGHTHOUSE;
    };
    auto small = [](int k) { return k == S_LANTERN || k == S_STALL || k == S_WELL || k == S_BANNER || k == S_CAMPFIRE || k == S_STATUE || k == S_OBELISK || k == S_ALTAR; };
    auto inside = [](const Structure& s, vec2 p, float m) {
        vec2 fp = structureFootprint(s.kind) * s.scale;
        vec2 l = rotate(p - s.pos, s.rot);
        return std::fabs(l.x) < fp.x + m && std::fabs(l.y) < fp.y + m;
    };
    auto roadOn = [&](const Structure& s) {   // worst road influence over the footprint
        vec2 fp = structureFootprint(s.kind) * s.scale;
        float worst = 0;
        for (int i = -1; i <= 1; i++)
            for (int j = -1; j <= 1; j++) worst = std::max(worst, roadInfluence(s.pos.x + rotate(vec2(i * fp.x, j * fp.y) * 0.9f, -s.rot).x, s.pos.y + rotate(vec2(i * fp.x, j * fp.y) * 0.9f, -s.rot).y));
        return worst;
    };
    auto overlapsAny = [&](size_t i) {
        const Structure& a = structures[i];
        vec2 fa = structureFootprint(a.kind) * a.scale;
        for (size_t j = 0; j < structures.size(); j++) {
            if (j == i) continue;
            const Structure& b = structures[j];
            if (!movable(b.kind) && b.kind != S_KEEP) continue;
            if (b.kind == S_BRIDGE || length(a.pos - b.pos) > 40.f) continue;
            vec2 fb = structureFootprint(b.kind) * b.scale;
            if (inside(a, b.pos, 0) || inside(b, a.pos, 0)) return true;
            for (int cx = -1; cx <= 1; cx += 2)
                for (int cz = -1; cz <= 1; cz += 2) {
                    if (inside(a, b.pos + rotate(vec2(cx * fb.x, cz * fb.y) * 0.8f, -b.rot), 0)) return true;
                    if (inside(b, a.pos + rotate(vec2(cx * fa.x, cz * fa.y) * 0.8f, -a.rot), 0)) return true;
                }
        }
        return false;
    };
    std::vector<bool> drop(structures.size(), false);
    int moved = 0, removed = 0;
    for (size_t i = 0; i < structures.size(); i++) {
        Structure& s = structures[i];
        if (!movable(s.kind) || s.poi < 0) continue;
        float limit = s.kind == S_LANTERN ? 0.55f : 0.3f;
        bool onRoad = roadOn(s) > limit, overlap = overlapsAny(i);
        if (!onRoad && !overlap) continue;
        vec2 start = s.pos;
        bool ok = false;
        // try rings of candidate positions, nearest first
        for (float r = 1.f; r <= 14.f && !ok; r += 1.f)
            for (int k = 0; k < 16 && !ok; k++) {
                float a = k * TAU / 16 + r * 0.37f;
                s.pos = start + vec2(std::cos(a), std::sin(a)) * r;
                const Poi& P = pois[s.poi];
                if (length(s.pos - P.pos) > P.radius * 1.05f + 4.f) continue;   // stay in its village
                if (waterLevel(s.pos.x, s.pos.y) > -1e8f) continue;
                ok = roadOn(s) <= limit && !overlapsAny(i);
            }
        if (ok) { moved++; continue; }
        s.pos = start;
        bool dwelling = s.kind == S_HOUSE || s.kind == S_STONEHOUSE || s.kind == S_LONGHOUSE || s.kind == S_STILT_HUT || s.kind == S_DOMEHOUSE || s.kind == S_BEACHHUT || s.kind == S_TENT || s.kind == S_BAZAAR;
        if (small(s.kind) || dwelling) { drop[i] = true; removed++; }
    }
    if (removed) {
        std::vector<Structure> keep;
        for (size_t i = 0; i < structures.size(); i++) if (!drop[i]) keep.push_back(structures[i]);
        structures.swap(keep);
    }
    AGR_LOG("World plan: placement pass moved %d buildings off roads / neighbours, removed %d small ones", moved, removed);
}


// 2.2.1 every building stands on level ground: its pad (footprint + 0.4 m) is flattened to one height, blending back into
// the terrain over 4 m. Before, a house on a slope was half buried uphill (up to 7.8 m) or raised on a stone plinth.
static bool levelsGround(int k) {
    switch (k) {
    case S_HOUSE: case S_INN: case S_STONEHOUSE: case S_LONGHOUSE: case S_CHAPEL: case S_FORGE: case S_TOWER: case S_WINDMILL:
    case S_CRYPT: case S_MONASTERY: case S_KEEP: case S_CASTLE_TOWER: case S_STILT_HUT: case S_TENT: case S_WELL: case S_STALL:
    case S_DOMEHOUSE: case S_BAZAAR: case S_MINARET: case S_ZIGGURAT: case S_LIGHTHOUSE: case S_PAGODA: case S_SPHINX: case S_BEACHHUT:
    case S_STATUE: case S_GATE: case S_ARCHWAY: case S_COLONNADE: case S_ALTAR: case S_WAYSTONE: case S_OBELISK: case S_PORTAL: return true;
    default: return false;
    }
}

void World::levelBuildings() {
    levelReady = false;
    chunkLevel.assign(CHUNKS * CHUNKS, {});
    for (int i = 0; i < (int)structures.size(); i++) {
        Structure& s = structures[i];
        s.baseY = -1e9f;
        if (!levelsGround(s.kind) || (s.kind == S_STALL && s.variant < 0)) continue;
        vec2 fp = structureFootprint(s.kind) * s.scale;
        float sum = 0; int n = 0;
        for (int j = -1; j <= 1; j++) for (int k = -1; k <= 1; k++) {
            vec2 q = rotate(vec2(fp.x * k, fp.y * j), -s.rot) + s.pos;
            sum += height(q.x, q.y); n++;
        }
        float y = sum / n;
        bool wet = false;   // by the water (river banks, lake shores): keep the old placement, the channel must not be filled
        for (int j = -1; j <= 1 && !wet; j++) for (int k = -1; k <= 1 && !wet; k++) { vec2 q = rotate(vec2(fp.x * k, fp.y * j) * 1.4f, -s.rot) + s.pos; if (waterLevel(q.x, q.y) > -1e8f) wet = true; }
        if (wet) continue;
        s.baseY = y;
        float R = length(fp) + 4.5f;
        for (int cz = (int)std::floor((s.pos.y - R) / CHUNK); cz <= (int)std::floor((s.pos.y + R) / CHUNK); cz++)
            for (int cx = (int)std::floor((s.pos.x - R) / CHUNK); cx <= (int)std::floor((s.pos.x + R) / CHUNK); cx++) {
                int ci = chunkIndex(cx, cz);
                if (ci >= 0) chunkLevel[ci].push_back(i);
            }
    }
    levelReady = true;
}

void World::indexPlan() {
    chunkStructures.assign(CHUNKS * CHUNKS, {});
    chunkRoads.assign(CHUNKS * CHUNKS, {});
    chunkPois.assign(CHUNKS * CHUNKS, {});
    chunkRivers.assign(CHUNKS * CHUNKS, {});
    for (int i = 0; i < (int)structures.size(); i++) {
        ChunkKey k = chunkOf(structures[i].pos.x, structures[i].pos.y);
        int ci = chunkIndex(k.x, k.z);
        if (ci >= 0) chunkStructures[ci].push_back(i);
    }
    auto addSeg = [&](std::vector<std::vector<int>>& idx, vec2 a, vec2 b, float margin, int code) {
        float x0 = std::min(a.x, b.x) - margin, x1 = std::max(a.x, b.x) + margin, z0 = std::min(a.y, b.y) - margin, z1 = std::max(a.y, b.y) + margin;
        for (int cz = (int)std::floor(z0 / CHUNK); cz <= (int)std::floor(z1 / CHUNK); cz++)
            for (int cx = (int)std::floor(x0 / CHUNK); cx <= (int)std::floor(x1 / CHUNK); cx++) {
                int ci = chunkIndex(cx, cz);
                if (ci >= 0) idx[ci].push_back(code);
            }
    };
    for (int r = 0; r < (int)roads.size(); r++)
        for (int s = 0; s + 1 < (int)roads[r].pts.size(); s++) addSeg(chunkRoads, roads[r].pts[s], roads[r].pts[s + 1], roads[r].width + 12, (r << 16) | s);
    for (int r = 0; r < (int)rivers.size(); r++)
        for (int s = 0; s + 1 < (int)rivers[r].pts.size(); s++) addSeg(chunkRivers, rivers[r].pts[s], rivers[r].pts[s + 1], rivers[r].width + 26, (r << 16) | s);
    for (int i = 0; i < (int)pois.size(); i++) addSeg(chunkPois, pois[i].pos, pois[i].pos, pois[i].radius + 30, i);
    plannedPois = (int)pois.size(); plannedStructures = (int)structures.size();
}

int World::addRuntimePoi(const Poi& p0) {
    int n = workerCount; bool was = running;
    stopWorkers();
    Poi p = p0;
    p.region = regionId(p.pos.x, p.pos.y);
    if (p.height == 0) p.height = std::max(height(p.pos.x, p.pos.y), 2.f);
    pois.push_back(p);
    int i = (int)pois.size() - 1;
    float m = p.radius + 30;
    for (int cz = (int)std::floor((p.pos.y - m) / CHUNK); cz <= (int)std::floor((p.pos.y + m) / CHUNK); cz++)
        for (int cx = (int)std::floor((p.pos.x - m) / CHUNK); cx <= (int)std::floor((p.pos.x + m) / CHUNK); cx++) {
            int ci = chunkIndex(cx, cz);
            if (ci >= 0) chunkPois[ci].push_back(i);
        }
    refreshArea(p.pos, p.radius + 60);
    if (was) startWorkers(n);
    return i;
}

int World::addRuntimeStructure(const Structure& s) {
    int n = workerCount; bool was = running;
    stopWorkers();
    structures.push_back(s);
    int i = (int)structures.size() - 1;
    ChunkKey k = chunkOf(s.pos.x, s.pos.y);
    int ci = chunkIndex(k.x, k.z);
    if (ci >= 0) chunkStructures[ci].push_back(i);
    refreshArea(s.pos, 20.f);
    if (was) startWorkers(n);
    return i;
}

void World::resetRuntime() {
    if (plannedPois < 0) return;
    if ((int)pois.size() == plannedPois && (int)structures.size() == plannedStructures) return;
    int n = workerCount; bool was = running;
    stopWorkers();
    for (int i = plannedPois; i < (int)pois.size(); i++) refreshArea(pois[i].pos, pois[i].radius + 60);
    for (int i = plannedStructures; i < (int)structures.size(); i++) refreshArea(structures[i].pos, 20.f);
    pois.resize(plannedPois); structures.resize(plannedStructures);
    for (auto& v : chunkPois) v.erase(std::remove_if(v.begin(), v.end(), [&](int i) { return i >= plannedPois; }), v.end());
    for (auto& v : chunkStructures) v.erase(std::remove_if(v.begin(), v.end(), [&](int i) { return i >= plannedStructures; }), v.end());
    if (was) startWorkers(n);
}

// ---------------------------------------------------------------------------------------- 2.4.0 diggable ground
float World::digOffset(float x, float z) const {
    int cx = (int)std::floor(x / CHUNK), cz = (int)std::floor(z / CHUNK);
    if (cx < 0 || cz < 0 || cx >= CHUNKS || cz >= CHUNKS) return 0.f;
    int ci = cz * CHUNKS + cx;
    if (!digFlag[ci].load(std::memory_order_relaxed)) return 0.f;
    std::shared_lock<std::shared_mutex> l(digMtx);
    auto it = digs.find(ci);
    if (it == digs.end()) return 0.f;
    float lx = x - cx * CHUNK, lz = z - cz * CHUNK;          // 0..32, 1 m cells
    int i = std::min(31, std::max(0, (int)lx)), j = std::min(31, std::max(0, (int)lz));
    float fx = clampf(lx - i, 0.f, 1.f), fz = clampf(lz - j, 0.f, 1.f);
    const float* h = it->second->h;
    float a = h[j * 33 + i], b = h[j * 33 + i + 1], c = h[(j + 1) * 33 + i], d = h[(j + 1) * 33 + i + 1];
    return lerpf(lerpf(a, b, fx), lerpf(c, d, fx), fz);
}

bool World::canDig(vec2 c, float radius) const {
    if (c.x < 80 || c.y < 80 || c.x > WORLD_SIZE - 80 || c.y > WORLD_SIZE - 80) return false;
    for (const Poi& p : pois) {   // not under houses, walls, temples, waystones
        // 2.5.1: villages and farms can be dug between the houses (the buildings themselves are checked below);
        // castles, temples, monasteries, waystones, towers and bridges stay untouchable
        bool built = p.type == PoiType::Castle || p.type == PoiType::Monastery || p.type == PoiType::Temple ||
                     p.type == PoiType::Waystone || p.type == PoiType::Tower || p.type == PoiType::Bridge;
        if (built && length(p.pos - c) < p.radius + radius + 2.f) return false;
        if ((p.type == PoiType::Village || p.type == PoiType::Farm || p.type == PoiType::Mill) && length(p.pos - c) < radius + 6.f) return false;   // not the village square
    }
    int ccx = (int)std::floor(c.x / CHUNK), ccz = (int)std::floor(c.y / CHUNK);
    for (int oz = -1; oz <= 1; oz++) for (int ox = -1; ox <= 1; ox++) {
    int ci = chunkIndex(ccx + ox, ccz + oz);
    if (ci >= 0 && ci < (int)chunkStructures.size())
        for (int si : chunkStructures[ci]) {   // not under a building: its footprint plus a margin
            const Structure& st = structures[si];
            vec2 fp = structureFootprint(st.kind) * st.scale;
            if (length(st.pos - c) < std::max(fp.x, fp.y) + radius + 1.5f) return false;
        }
    }
    return true;
}

bool World::dig(vec2 c, float radius, float depth, bool explosion) {
    if (!canDig(c, radius)) return false;
    std::unique_lock<std::shared_mutex> l(digMtx);
    int x0 = (int)std::floor(c.x - radius - 1.5f), x1 = (int)std::ceil(c.x + radius + 1.5f);
    int z0 = (int)std::floor(c.y - radius - 1.5f), z1 = (int)std::ceil(c.y + radius + 1.5f);
    auto grid = [&](int cx, int cz) -> float* {
        if (cx < 0 || cz < 0 || cx >= CHUNKS || cz >= CHUNKS) return nullptr;
        int ci = cz * CHUNKS + cx;
        auto& g = digs[ci];
        if (!g) g = std::make_unique<DigGrid>();
        digFlag[ci].store(1);
        return g->h;
    };
    for (int z = z0; z <= z1; z++)
        for (int x = x0; x <= x1; x++) {
            float d = length(vec2((float)x, (float)z) - c) / radius;
            float delta = 0;
            if (d < 1.f) { float k = 1.f - d * d; delta = -depth * k * k; }                         // the bowl
            else if (explosion && d < 1.45f) delta = depth * 0.16f * std::sin((d - 1.f) / 0.45f * PI);   // thrown-up rim
            if (delta == 0) continue;
            int cx = (int)std::floor(x / CHUNK), cz = (int)std::floor(z / CHUNK);
            int lx = x - cx * (int)CHUNK, lz = z - cz * (int)CHUNK;
            // the vertex belongs to its chunk and, on a border, to the neighbours' grids too (seamless)
            for (int ox = 0; ox <= (lx == 0 ? 1 : 0); ox++)
                for (int oz = 0; oz <= (lz == 0 ? 1 : 0); oz++) {
                    float* g = grid(cx - ox, cz - oz);
                    if (!g) continue;
                    int gi = ox ? 32 : lx, gj = oz ? 32 : lz;
                    float& v = g[gj * 33 + gi];
                    v = std::max(-14.f, std::min(4.f, v + delta));                                  // no bottomless pits
                }
        }
    l.unlock();
    refreshArea(c, radius + 3.f);
    return true;
}

std::string World::saveDigs() const {
    std::shared_lock<std::shared_mutex> l(digMtx);
    std::string out;
    char b[48];
    for (auto& kv : digs) {
        out += "digs " + std::to_string(kv.first);
        for (int i = 0; i < 33 * 33; i++) if (std::fabs(kv.second->h[i]) > 0.005f) { snprintf(b, sizeof b, " %d:%.2f", i, kv.second->h[i]); out += b; }
        out += "\n";
    }
    return out;
}

void World::loadDigs(const std::string& line) {
    std::istringstream in(line);
    std::string tag; int ci = -1;
    in >> tag >> ci;
    if (tag != "digs" || ci < 0 || ci >= CHUNKS * CHUNKS) return;
    std::unique_lock<std::shared_mutex> l(digMtx);
    auto& g = digs[ci];
    if (!g) g = std::make_unique<DigGrid>();
    std::string tok;
    while (in >> tok) { size_t p = tok.find(':'); if (p == std::string::npos) continue; int i = std::atoi(tok.c_str()); if (i >= 0 && i < 33 * 33) g->h[i] = (float)std::atof(tok.c_str() + p + 1); }
    digFlag[ci].store(1);
}

void World::clearDigs() {
    std::unique_lock<std::shared_mutex> l(digMtx);
    digs.clear();
    for (auto& f : digFlag) f.store(0);
}

int World::dugChunks() const { std::shared_lock<std::shared_mutex> l(digMtx); return (int)digs.size(); }

void World::refreshArea(vec2 c, float r) {
    // mark the loaded chunks stale: they keep drawing their old meshes until the rebuilt ones arrive
    for (auto& kv : chunks) {
        vec2 cc{(kv.first.x + 0.5f) * CHUNK, (kv.first.z + 0.5f) * CHUNK};
        if (length(cc - c) < r + CHUNK * 0.75f) kv.second->lod = -2;
    }
}

const Poi* World::poiById(const std::string& id) const {
    for (auto& p : pois) if (p.id == id) return &p;
    return nullptr;
}

int World::nearestPoi(vec2 p, float maxDist, PoiType* filter) const {
    int best = -1; float bd = maxDist;
    for (int i = 0; i < (int)pois.size(); i++) {
        if (filter && pois[i].type != *filter) continue;
        float d = length(pois[i].pos - p);
        if (d < bd) { bd = d; best = i; }
    }
    return best;
}

// ---------------------------------------------------------------------------------------- field
float World::riverInfluence(float x, float z, float& surf, vec2& flow) const {
    int ci = chunkIndex((int)std::floor(x / CHUNK), (int)std::floor(z / CHUNK));
    if (ci < 0) return 0;
    float bestD = 1e9f; int bestR = -1; int bestS = 0; float bestT = 0;
    for (int code : chunkRivers[ci]) {
        int r = code >> 16, s = code & 0xffff;
        float t;
        float d = distToSegment({x, z}, rivers[r].pts[s], rivers[r].pts[s + 1], &t);
        if (d < bestD) { bestD = d; bestR = r; bestS = s; bestT = t; }
    }
    if (bestR < 0) return 0;
    const River& rv = rivers[bestR];
    surf = lerpf(rv.surf[bestS], rv.surf[bestS + 1], bestT);
    vec2 dir = normalize(rv.pts[bestS + 1] - rv.pts[bestS]);
    float slope = std::max(0.f, rv.surf[bestS] - rv.surf[bestS + 1]) / std::max(1.f, length(rv.pts[bestS + 1] - rv.pts[bestS]));
    flow = dir * clampf(0.25f + slope * 12.f, 0.25f, 1.f);
    float half = rv.width * 0.5f;
    // 1 inside the channel, falls to 0 across the banks
    return 1.f - smoothstepf(half, half + 16.f, bestD);
}

float World::height(float x, float z) const {
    float dug = digOffset(x, z);   // 2.4.0 holes, trenches and craters
    return heightNoDig(x, z) + dug;
}

float World::heightNoDig(float x, float z) const {
    RegionWeights rw = regionAt(x, z);
    float h = baseHeight(x, z, rw);
    // rivers: carve channel, raise levees so water never spills outside the mesh
    float surf; vec2 flow;
    float ri = riverInfluence(x, z, surf, flow);
    if (ri > 0) {
        int ci = chunkIndex((int)std::floor(x / CHUNK), (int)std::floor(z / CHUNK));
        float bestD = 1e9f, wid = 10;
        for (int code : chunkRivers[ci]) {
            int r = code >> 16, s = code & 0xffff;
            float d = distToSegment({x, z}, rivers[r].pts[s], rivers[r].pts[s + 1]);
            if (d < bestD) { bestD = d; wid = rivers[r].width; }
        }
        float half = wid * 0.5f;
        float bed = surf - 1.9f * (1.f - smoothstepf(0, half, bestD)) - 0.35f;
        if (bestD < half) h = std::min(h, bed);
        else {
            float t = smoothstepf(half, half + 16.f, bestD);
            float levee = std::max(h, surf + 0.35f);
            h = lerpf(lerpf(bed, levee, smoothstepf(half, half + 3.f, bestD)), h, t);
            if (bestD < half + 5.f) h = std::max(h, std::min(surf + 0.35f, h + 2.f));
        }
    }
    // lakes
    for (const Lake& l : lakes) {
        float d = length(vec2(x, z) - l.c);
        if (d > l.r + 30) continue;
        float shore = l.r * (1.f + 0.12f * fbm2(x * 0.03f, z * 0.03f, 2, 97));
        if (d < shore) {
            float depth = (l.lava ? 3.f : 4.5f) * (1.f - (d / shore) * (d / shore)) + 0.4f;
            h = std::min(h, l.level - depth);
        } else {
            float t = smoothstepf(shore, shore + 24.f, d);
            float hb = h;
            h = lerpf(std::max(h, l.level + 0.35f), h, t);
            h = std::max(h, lerpf(l.level + 0.35f, -100.f, t));
            h = lerpf(h, std::min(h, hb), ri);   // 1.11.1: the shore bank does not dam the river that flows into the lake
        }
    }
    // settlements & landmarks are levelled
    int ci = chunkIndex((int)std::floor(x / CHUNK), (int)std::floor(z / CHUNK));
    if (ci >= 0) {
        for (int pi : chunkPois[ci]) {
            const Poi& p = pois[pi];
            if (p.type == PoiType::Lake) continue;
            float d = length(vec2(x, z) - p.pos);
            float w = 1.f - smoothstepf(p.radius * 0.75f, p.radius + 22.f, d);
            w *= 1.f - ri;   // 1.11.1: never fill a river channel (a village next to a river left a dry, stepped bed)
            if (w > 0) h = lerpf(h, p.height, w);
        }
        // roads flatten towards their smoothed profile
        float bestD = 1e9f, target = h, rw2 = 4;
        for (int code : chunkRoads[ci]) {
            int r = code >> 16, s = code & 0xffff;
            const Road& rd = roads[r];
            float t;
            float d = distToSegment({x, z}, rd.pts[s], rd.pts[s + 1], &t);
            if (d < bestD) {
                bestD = d; rw2 = rd.width;
                vec2 q = lerp(rd.pts[s], rd.pts[s + 1], t);
                RegionWeights qw = regionAt(q.x, q.y);
                target = baseHeight(q.x, q.y, qw);
            }
        }
        bool harbour = false;
        for (int pi : chunkPois[ci]) { const Poi& p = pois[pi]; if (p.coastal && length(vec2(x, z) - p.pos) < p.radius + 22.f) harbour = true; }
        if (bestD < rw2 + 8 && ri < 0.5f && !harbour) {
            float w = (1.f - smoothstepf(rw2 * 0.5f, rw2 * 0.5f + 8.f, bestD)) * (1.f - ri * 2.f);
            h = lerpf(h, std::max(target, h - 1.5f), w * 0.85f);
        }
        // 2.2.1 building pads
        if (levelReady) {   // the nearest pad wins (two neighbouring houses never fight over the same ground)
            float bw = 0.f, by = 0.f;
            for (int si : chunkLevel[ci]) {
                const Structure& st = structures[si];
                vec2 l = rotate(vec2(x, z) - st.pos, st.rot);
                vec2 fp = structureFootprint(st.kind) * st.scale + vec2(0.4f);
                vec2 q{std::max(std::fabs(l.x) - fp.x, 0.f), std::max(std::fabs(l.y) - fp.y, 0.f)};
                float d = length(q);
                if (d >= 4.f) continue;
                float w = 1.f - smoothstepf(0.f, 4.f, d);
                if (w > bw) { bw = w; by = st.baseY; }
            }
            if (bw > 0.f) h = lerpf(h, by, bw * (1.f - ri));
        }
    }
    return h;
}

vec3 World::normal(float x, float z) const {
    const float e = 0.6f;
    float hx = height(x + e, z) - height(x - e, z);
    float hz = height(x, z + e) - height(x, z - e);
    return normalize(vec3(-hx, 2 * e, -hz));
}

float World::waterLevel(float x, float z, bool* lava, bool* frozen, vec2* flow) const {
    if (lava) *lava = false;
    if (frozen) *frozen = false;
    if (flow) *flow = vec2(0, 0);
    float level = -1e9f;
    float e = landEdgeDistance(x, z);
    if (e < 400) level = SEA_LEVEL;  // sea near the coast
    // swamps of the Shadows region sit at sea level too
    if (level < SEA_LEVEL && regionAt(x, z).w[R_SHADOWS] > 0.5f) level = SEA_LEVEL;
    float surf; vec2 fl;
    if (riverInfluence(x, z, surf, fl) > 0.f && surf > level) { level = surf; if (flow) *flow = fl; }
    for (const Lake& l : lakes) {
        float d = length(vec2(x, z) - l.c);
        if (d < l.r * 1.3f + 10 && l.level > level) {
            level = l.level;
            if (lava) *lava = l.lava;
            if (frozen) *frozen = l.frozen;
            if (flow) *flow = vec2(0, 0);
        }
    }
    if (level > -1e8f && regionAt(x, z).dominant == R_FROSTHEIM && frozen && !*frozen && (!flow || length(*flow) < 0.01f)) *frozen = true;
    return level;
}

float World::roadInfluence(float x, float z, bool* cobble) const {
    int ci = chunkIndex((int)std::floor(x / CHUNK), (int)std::floor(z / CHUNK));
    if (ci < 0) return 0;
    float best = 0;
    for (int code : chunkRoads[ci]) {
        int r = code >> 16, s = code & 0xffff;
        const Road& rd = roads[r];
        float d = distToSegment({x, z}, rd.pts[s], rd.pts[s + 1]);
        float n = 1.2f * fbm2(x * 0.3f, z * 0.3f, 2, 99);
        float v = 1.f - smoothstepf(rd.width * 0.35f, rd.width * 0.5f + 1.2f, d + n);
        if (v > best) { best = v; if (cobble) *cobble = rd.cobble; }
    }
    return best;
}

float World::groundHeight(float x, float z) const {
    float h = height(x, z);
    int ci = chunkIndex((int)std::floor(x / CHUNK), (int)std::floor(z / CHUNK));
    if (ci < 0) return h;
    // bridges are walkable decks
    for (int dz = -1; dz <= 1; dz++)
        for (int dx = -1; dx <= 1; dx++) {
            int cj = chunkIndex((int)std::floor(x / CHUNK) + dx, (int)std::floor(z / CHUNK) + dz);
            if (cj < 0) continue;
            for (int si : chunkStructures[cj]) {
                const Structure& s = structures[si];
                if (s.kind == S_PIER) {   // 2.0.0 boardwalks over the water: a flat deck 0.75 m above the surface
                    vec2 lp = rotate(vec2(x, z) - s.pos, s.rot);
                    if (std::fabs(lp.x) < 1.15f && std::fabs(lp.y) < 7.f) { float lv = waterLevel(s.pos.x, s.pos.y); h = std::max(h, (lv > -1e8f ? lv : height(s.pos.x, s.pos.y)) + 0.75f); }
                    continue;
                }
                if (s.kind != S_BRIDGE) continue;
                vec2 l = rotate(vec2(x, z) - s.pos, s.rot);  // into bridge space (z along the bridge)
                // 1.11.2 the walkable deck is exactly the drawn one (it used to float 0.8 m above the planks)
                if (std::fabs(l.x) < 2.6f && std::fabs(l.y) < bridgeHalfLen(s)) h = std::max(h, bridgeDeck(s, l.y));
            }
        }
    return h;
}

bool World::skyDeck(vec3 p, float& deckY) const {
    int cx = (int)std::floor(p.x / CHUNK), cz = (int)std::floor(p.z / CHUNK);
    bool found = false;
    for (int dz = -1; dz <= 1; dz++)
        for (int dx = -1; dx <= 1; dx++) {
            int ci = chunkIndex(cx + dx, cz + dz);
            if (ci < 0) continue;
            for (int si : chunkStructures[ci]) {
                const Structure& s = structures[si];
                if (s.kind != S_SKYISLAND) continue;
                float deck = s.variant / 100.f;
                float d = length(vec2(p.x, p.z) - s.pos);
                if (d < SKY_ISLAND_R * 0.93f && p.y >= deck - 0.7f) { float q = d / SKY_ISLAND_R; float y = deck + 0.4f * (1.f - q * q); if (!found || y > deckY) deckY = y; found = true; }
            }
        }
    return found;
}

int World::geyserNear(vec2 p, float r) const {
    int cx = (int)std::floor(p.x / CHUNK), cz = (int)std::floor(p.y / CHUNK);
    for (int dz = -1; dz <= 1; dz++)
        for (int dx = -1; dx <= 1; dx++) {
            int ci = chunkIndex(cx + dx, cz + dz);
            if (ci < 0) continue;
            for (int si : chunkStructures[ci]) if (structures[si].kind == S_GEYSER && length(structures[si].pos - p) < r) return si;
        }
    return -1;
}

float World::bridgeDeck(const Structure& s, float along) const {
    float L = bridgeHalfLen(s);
    vec2 dir(std::sin(s.rot), std::cos(s.rot));
    vec2 a = s.pos - dir * L, b = s.pos + dir * L;
    float ha = height(a.x, a.y), hb = height(b.x, b.y);
    float t = clampf(along / L, -1.f, 1.f);
    float road = lerpf(ha, hb, (t + 1.f) * 0.5f);                 // straight line between the two banks
    {   // 2.0.0 a bank that bulges above the line carries the deck over it (no walking inside a levee); the lift is the
        // envelope of the bulge over +-3 m so the deck stays smooth
        float lift = 0.f;
        for (int k = -3; k <= 3; k++) {
            float a2 = clampf(along + k, -L, L);
            vec2 q = s.pos + dir * a2;
            float line2 = lerpf(ha, hb, (clampf(a2 / L, -1.f, 1.f) + 1.f) * 0.5f);
            lift = std::max(lift, height(q.x, q.y) - line2);
        }
        road += std::max(0.f, lift) * (1.f - smoothstepf(0.8f, 1.f, std::fabs(t)));   // no lift at the very ends: they meet the road
    }
    float span = s.variant / 100.f + 1.2f + 0.8f * (1.f - t * t);  // clearance over the water, gentle arch
    // near the ends the arch comes down to meet the road (no step when walking or driving on); the bigger the
    // drop, the longer the ramp (at most ~1 in 3)
    float drop = std::max(0.f, s.variant / 100.f + 1.6f - (t < 0 ? ha : hb));
    float frac = clampf(drop / (0.22f * L), 0.3f, 0.75f);
    float e = smoothstepf(1.f - frac, 1.f, std::fabs(t));
    span -= std::max(0.f, span - road) * e;
    return std::max(road, span);
}

bool World::nodeAvailable(uint32_t uid, double t) const {
    auto it = harvested.find(uid);
    if (it == harvested.end()) return true;
    return it->second > 0 && t >= it->second;
}

// ---------------------------------------------------------------------------------------- chunk generation
struct ScatterEntry { PropKind k; float weight, smin, smax; bool collide; };
static const std::vector<ScatterEntry>& scatterTable(int r) {
    static std::vector<ScatterEntry> T[R_COUNT] = {
        {{P_OAK, 3, 0.8f, 1.3f, true}, {P_BIRCH, 2, 0.8f, 1.2f, true}, {P_BUSH, 3, 0.7f, 1.3f, false}, {P_ROCK, 1, 0.6f, 1.4f, true}, {P_FERN, 1.2f, 0.8f, 1.2f, false}, {P_STUMP, 0.25f, 0.8f, 1.2f, true}, {P_LOG, 0.2f, 0.9f, 1.1f, true}, {P_SMALLSHROOMS, 0.3f, 0.8f, 1.2f, false}, {P_HAYBALE, 0.08f, 1, 1, true}},
        {{P_SNOWPINE, 4, 0.8f, 1.5f, true}, {P_PINE, 1, 0.8f, 1.3f, true}, {P_SNOWROCK, 2, 0.6f, 1.6f, true}, {P_ICECRYSTAL, 0.5f, 0.6f, 1.4f, true}, {P_DEADTREE, 0.3f, 0.8f, 1.2f, true}, {P_BONES, 0.1f, 1, 1, false}},
        {{P_CHARRED, 2, 0.8f, 1.4f, true}, {P_OBSIDIAN, 2, 0.6f, 1.6f, true}, {P_ROCK, 2, 0.7f, 1.6f, true}, {P_BONES, 0.4f, 0.8f, 1.2f, false}, {P_DEADTREE, 0.4f, 0.8f, 1.2f, true}, {P_CACTUSFERN, 0.8f, 0.7f, 1.2f, false}},
        {{P_ELDERTREE, 1.4f, 0.8f, 1.3f, true}, {P_OAK, 1, 0.9f, 1.3f, true}, {P_GIANTSHROOM, 1.0f, 0.7f, 1.4f, true}, {P_FERN, 3, 0.8f, 1.4f, false}, {P_MAGICCRYSTAL, 0.35f, 0.7f, 1.3f, true}, {P_SMALLSHROOMS, 1.2f, 0.8f, 1.3f, false}, {P_BUSH, 1.2f, 0.8f, 1.3f, false}, {P_LOG, 0.3f, 0.9f, 1.2f, true}},
        {{P_TWISTED, 2, 0.8f, 1.3f, true}, {P_WILLOW, 1.5f, 0.8f, 1.3f, true}, {P_DEADTREE, 1, 0.8f, 1.3f, true}, {P_GRAVESTONE, 0.25f, 0.8f, 1.2f, true}, {P_REEDS, 2.5f, 0.8f, 1.3f, false}, {P_BONES, 0.3f, 0.8f, 1.2f, false}, {P_SMALLSHROOMS, 0.6f, 0.8f, 1.3f, false}},
        {{P_PINE, 3, 0.8f, 1.4f, true}, {P_ROCK, 2, 0.8f, 1.8f, true}, {P_BOULDER, 1, 0.8f, 1.4f, true}, {P_BUSH, 1, 0.7f, 1.1f, false}, {P_RUINCOLUMN, 0.1f, 0.8f, 1.2f, true}, {P_SNOWROCK, 0.5f, 0.8f, 1.4f, true}},
        // Amber Sands: acacias, cacti, agaves, wind-carved rocks, tumbleweed, bleached bones
        {{P_ACACIA, 1.2f, 0.8f, 1.4f, true}, {P_CACTUS, 1.6f, 0.7f, 1.4f, true}, {P_AGAVE, 1.4f, 0.8f, 1.3f, false}, {P_SANDROCK, 2.f, 0.7f, 1.8f, true}, {P_TUMBLEWEED, 1.f, 0.8f, 1.2f, false}, {P_DRYBUSH, 1.2f, 0.7f, 1.2f, false}, {P_BONES, 0.25f, 0.8f, 1.2f, false}, {P_DATEPALM, 0.1f, 0.9f, 1.2f, true}},
        // Coral Coast: palms, ferns, flowering bushes, driftwood, shells, sea rocks
        {{P_PALM, 3.2f, 0.85f, 1.35f, true}, {P_BANANA, 1.2f, 0.8f, 1.2f, true}, {P_FERN, 2.f, 0.8f, 1.4f, false}, {P_TROPBUSH, 2.4f, 0.8f, 1.3f, false}, {P_SEAROCK, 0.7f, 0.7f, 1.5f, true}, {P_DRIFTWOOD, 0.3f, 0.8f, 1.2f, true}, {P_SHELLS, 0.5f, 0.9f, 1.2f, false}, {P_OAK, 0.25f, 0.8f, 1.1f, true}},
        // 2.4.0 Valdor Steppes: lone acacias and birches, boulders, standing stones, dry bushes, hay
        {{P_ACACIA, 0.7f, 0.9f, 1.5f, true}, {P_BIRCH, 0.4f, 0.8f, 1.2f, true}, {P_BOULDER, 0.9f, 0.7f, 1.6f, true}, {P_ROCK, 1.2f, 0.6f, 1.5f, true}, {P_DRYBUSH, 1.6f, 0.7f, 1.3f, false}, {P_STANDSTONE, 0.12f, 0.9f, 1.3f, true}, {P_HAYBALE, 0.1f, 1, 1, true}, {P_FLOWERS, 1.4f, 0.8f, 1.3f, false}, {P_BONES, 0.1f, 0.8f, 1.1f, false}},
        // 2.4.0 Emerald Jungle: giant canopy trees, banana plants, ferns everywhere, willows by the water, mossy ruins
        {{P_ELDERTREE, 2.4f, 1.0f, 1.6f, true}, {P_BANANA, 2.2f, 0.9f, 1.4f, true}, {P_PALM, 0.8f, 0.9f, 1.4f, true}, {P_FERN, 5.f, 0.9f, 1.6f, false}, {P_TROPBUSH, 3.f, 0.9f, 1.5f, false}, {P_WILLOW, 0.6f, 0.9f, 1.3f, true}, {P_GIANTSHROOM, 0.3f, 0.7f, 1.2f, true}, {P_RUINCOLUMN, 0.12f, 0.8f, 1.2f, true}, {P_LOG, 0.5f, 0.9f, 1.3f, true}, {P_SMALLSHROOMS, 0.8f, 0.8f, 1.3f, false}},
        // 2.8.0 (reworked) Lands of Aurora, a northern volcanic island: silver aurora birches, mossy boulders, pines
        // in the hollows, ferns and heather, and here and there an outcrop of pale quartz
        {{P_CRYSTALTREE, 2.4f, 0.8f, 1.3f, true}, {P_BIRCH, 1.2f, 0.8f, 1.2f, true}, {P_PINE, 0.6f, 0.7f, 1.1f, true}, {P_BOULDER, 0.9f, 0.6f, 1.4f, true}, {P_ROCK, 1.4f, 0.6f, 1.5f, true}, {P_AURORASPIRE, 0.22f, 0.6f, 1.2f, true}, {P_FERN, 2.2f, 0.8f, 1.3f, false}, {P_BUSH, 1.2f, 0.7f, 1.2f, false}, {P_SMALLSHROOMS, 0.4f, 0.8f, 1.2f, false}},
    };
    return T[r];
}

static float forestDensity(int r, float x, float z) {
    float n = fbm2(x * 0.006f, z * 0.006f, 3, 777 + r) * 0.5f + 0.5f;
    switch (r) {
    case R_GREENHAVEN: return smoothstepf(0.48f, 0.7f, n) * 0.85f + 0.06f;   // forests and meadows
    case R_FROSTHEIM: return smoothstepf(0.35f, 0.65f, n) * 0.6f + 0.05f;
    case R_ASHEN: return 0.18f + n * 0.12f;
    case R_ELDERWOOD: return 0.55f + n * 0.4f;
    case R_SHADOWS: return 0.25f + n * 0.35f;
    case R_AMBER: return 0.05f + smoothstepf(0.52f, 0.78f, n) * 0.28f;   // sparse: oases and wadis hold the green
    case R_CORAL: return 0.35f + smoothstepf(0.35f, 0.7f, n) * 0.5f;
    case R_STEPPE: return 0.06f + smoothstepf(0.6f, 0.8f, n) * 0.22f;   // open grassland: a few trees and rocks
    case R_JUNGLE: return 0.75f + n * 0.25f;                             // the densest vegetation of the world
    case R_AURORA: return 0.15f + smoothstepf(0.45f, 0.72f, n) * 0.55f;  // 2.8.0 crystal groves and open glowing meadows
    default: return 0.12f + smoothstepf(0.5f, 0.75f, n) * 0.4f;
    }
}

float World::forestAt(float x, float z) const { return forestDensity(regionId(x, z), x, z); }

void World::generateChunkSync(ChunkKey k, int lod, ChunkBuild& out) const {
    out.key = k; out.lod = lod; out.liteProps = lod > 0 || !detailedFoliage;
    const int step = 1 << lod;           // metres per quad
    const int N = (int)CHUNK / step;     // quads per side
    const float x0 = k.x * CHUNK, z0 = k.z * CHUNK;
    RegionWeights crw = regionAt(x0 + CHUNK / 2, z0 + CHUNK / 2);
    out.region = crw.dominant;
    out.snow = crw.w[R_FROSTHEIM];
    // ---- heights with a 1-vertex border for normals
    int G = N + 3;
    std::vector<float> H((size_t)G * G);
    for (int j = 0; j < G; j++)
        for (int i = 0; i < G; i++) H[(size_t)j * G + i] = height(x0 + (i - 1) * step, z0 + (j - 1) * step);
    auto hAt = [&](int i, int j) { return H[(size_t)(j + 1) * G + (i + 1)]; };
    // ---- terrain vertices
    MeshBuilder& T = out.terrain;
    T.v.reserve((N + 1) * (N + 1) + 4 * (N + 1));
    for (int j = 0; j <= N; j++)
        for (int i = 0; i <= N; i++) {
            float x = x0 + i * step, z = z0 + j * step, y = hAt(i, j);
            vec3 n = normalize(vec3(hAt(i - 1, j) - hAt(i + 1, j), 2.f * step, hAt(i, j - 1) - hAt(i, j + 1)));
            RegionWeights rw = regionAt(x, z);
            float slope = 1.f - n.y;
            // per-region splat
            float wRock = 0, wDirt = 0, wSnow = 0, wSand = 0, wMud = 0, wCob = 0, wAsh = 0;
            vec3 tint(0);
            float n1 = fbm2(x * 0.05f, z * 0.05f, 2, 1001) * 0.5f + 0.5f;
            float n2 = fbm2(x * 0.013f, z * 0.013f, 3, 1002) * 0.5f + 0.5f;
            for (int r = 0; r < R_COUNT; r++) {
                float w = rw.w[r];
                if (w < 0.003f) continue;
                tint += regionGroundTint(r) * w;
                switch (r) {
                case R_GREENHAVEN: wDirt += w * smoothstepf(0.62f, 0.78f, n1 * 0.6f + n2 * 0.5f) * 0.8f; break;
                case R_FROSTHEIM: wSnow += w * (0.75f + 0.25f * smoothstepf(0.4f, 0.7f, n1) - smoothstepf(0.65f, 0.85f, n2) * 0.5f); wRock += w * smoothstepf(0.6f, 0.75f, n1) * 0.3f; break;
                case R_ASHEN: wAsh += w * 0.75f; wRock += w * smoothstepf(0.5f, 0.7f, n1) * 0.35f; wDirt += w * smoothstepf(0.65f, 0.8f, n2) * 0.2f; break;
                case R_ELDERWOOD: wMud += w * smoothstepf(0.55f, 0.75f, n1) * 0.35f; wDirt += w * smoothstepf(0.7f, 0.85f, n2) * 0.3f; break;
                case R_SHADOWS: wMud += w * (0.35f + 0.4f * n1); wDirt += w * smoothstepf(0.6f, 0.8f, n2) * 0.25f; break;
                case R_HIGHCROWNS: wRock += w * (0.15f + smoothstepf(0.55f, 0.75f, n1) * 0.4f); wSnow += w * smoothstepf(70.f, 95.f, y + n1 * 12.f); wDirt += w * smoothstepf(0.7f, 0.85f, n2) * 0.2f; break;
                case R_AMBER: wSand += w * (0.82f + 0.16f * smoothstepf(0.35f, 0.7f, n1)); wRock += w * smoothstepf(0.62f, 0.85f, n2) * 0.45f; wDirt += w * smoothstepf(0.7f, 0.9f, n1) * 0.18f; break;
                case R_STEPPE: wDirt += w * smoothstepf(0.66f, 0.86f, n1) * 0.35f; wRock += w * smoothstepf(0.7f, 0.9f, n2) * 0.25f; break;
                case R_JUNGLE: wMud += w * smoothstepf(0.5f, 0.75f, n1) * 0.4f; wDirt += w * smoothstepf(0.72f, 0.88f, n2) * 0.2f; break;
                case R_AURORA: wRock += w * smoothstepf(0.62f, 0.84f, n1) * 0.35f; wDirt += w * smoothstepf(0.7f, 0.88f, n2) * 0.25f; break;   // 2.8.0 moss over lava rock, bare patches
                case R_CORAL: wSand += w * (1.f - smoothstepf(0.9f, 3.0f, y)) * 0.92f; wDirt += w * smoothstepf(0.68f, 0.85f, n2) * 0.22f; wMud += w * smoothstepf(0.75f, 0.9f, n1) * 0.12f; break;
                }
            }
            // slopes become rock, altitude snow anywhere
            wRock += smoothstepf(0.22f, 0.4f, slope) * 0.8f;
            wSnow = std::max(wSnow, smoothstepf(85.f, 110.f, y));
            // 2.4.0 dug ground: bare soil (mud deep down, dirt on the walls and the thrown-up rim), never paving
            {
                float dg = digOffset(x, z);
                if (std::fabs(dg) > 0.12f) {
                    float k = saturate(std::fabs(dg) * 1.4f);
                    wRock *= 1.f - 0.85f * k; wSnow *= 1.f - k; wSand *= 1.f - 0.5f * k; wAsh *= 1.f - 0.5f * k;
                    wDirt += k * 1.2f; wMud += saturate(-dg - 1.2f) * 0.8f;
                }
            }
            // shores
            float wl = waterLevel(x, z);
            if (wl > -1e8f && y < wl + 1.6f && y > wl - 3.f) {
                float s = 1.f - smoothstepf(wl + 0.4f, wl + 1.6f, y);
                if (rw.dominant == R_SHADOWS) wMud += s; else if (rw.dominant == R_ASHEN) wAsh += s * 0.5f; else wSand += s;
            }
            // roads & plazas
            bool cob = false;
            float road = roadInfluence(x, z, &cob);
            if (road > 0) { if (cob) wCob += road; else wDirt += road * 1.4f; }
            float plaza = 0;
            {
                int ci = chunkIndex(k.x, k.z);
                if (ci >= 0)
                    for (int pi : chunkPois[ci]) {
                        const Poi& p = pois[pi];
                        if (p.type != PoiType::Village && p.type != PoiType::Monastery && p.type != PoiType::Castle && p.type != PoiType::Mill) continue;
                        float d = length(vec2(x, z) - p.pos);
                        float pr = p.type == PoiType::Castle ? p.radius * 0.7f : (p.type == PoiType::BossArena ? p.radius * 0.8f : 13.f);
                        plaza = std::max(plaza, 1.f - smoothstepf(pr - 3.f, pr + 2.f, d + 2.f * fbm2(x * 0.2f, z * 0.2f, 2, 5)));
                        // packed dirt around houses
                        if (p.type != PoiType::BossArena) wDirt += (1.f - smoothstepf(p.radius * 0.45f, p.radius * 0.85f, d)) * 0.45f * smoothstepf(0.35f, 0.65f, n1);
                    }
            }
            wCob = std::max(wCob, plaza);
            float total = wRock + wDirt + wSnow + wSand + wMud + wCob + wAsh;
            if (total > 1.f) { wRock /= total; wDirt /= total; wSnow /= total; wSand /= total; wMud /= total; wCob /= total; wAsh /= total; }
            Vertex v{};
            v.px = x; v.py = y; v.pz = z;
            v.nx = (int8_t)(n.x * 127); v.ny = (int8_t)(n.y * 127); v.nz = (int8_t)(n.z * 127);
            Color8 c = toColor8(tint * 0.5f * (0.94f + 0.12f * n2), 1.f);
            v.r = c.r; v.g = c.g; v.b = c.b; v.a = 255;
            auto q = [](float f) { return (uint8_t)clampf(f * 255.f + 0.5f, 0, 255); };
            v.rough = q(wRock); v.metal = q(wDirt); v.emis = q(wSnow); v.wind = q(wSand);
            v.b0 = q(wMud); v.b1 = q(wCob); v.w0 = q(wAsh);
            v.ex = (uint8_t)(saturate(std::fabs(digOffset(x, z)) * 1.4f) * 255.f);   // 2.4.0 dug: the shader shows soil on the walls, not rock
            T.v.push_back(v);
        }
    for (int j = 0; j < N; j++)
        for (int i = 0; i < N; i++) {
            uint32_t a = j * (N + 1) + i, b = a + 1, c = a + (N + 1), d = c + 1;
            // alternate diagonal following the terrain for fewer artefacts
            if (std::fabs(hAt(i, j) - hAt(i + 1, j + 1)) < std::fabs(hAt(i + 1, j) - hAt(i, j + 1))) { T.tri(a, c, d); T.tri(a, d, b); }
            else { T.tri(a, c, b); T.tri(b, c, d); }
        }
    if (lod > 0) {
        // skirts hide cracks between LODs
        auto skirt = [&](int i0, int j0, int di, int dj) {
            for (int s = 0; s < N; s++) {
                int i = i0 + di * s, j = j0 + dj * s;
                uint32_t a = j * (N + 1) + i, b = (j + dj) * (N + 1) + (i + di);
                Vertex va = T.v[a], vb = T.v[b];
                va.py -= 2.5f; vb.py -= 2.5f;
                uint32_t ia = (uint32_t)T.v.size(); T.v.push_back(va);
                uint32_t ib = (uint32_t)T.v.size(); T.v.push_back(vb);
                T.tri(a, b, ib); T.tri(a, ib, ia);
                T.tri(a, ib, b); T.tri(a, ia, ib);  // both windings: cheap double-sided skirt
            }
        };
        skirt(0, 0, 1, 0); skirt(0, N, 1, 0); skirt(0, 0, 0, 1); skirt(N, 0, 0, 1);
    }

    // ---- water / lava surfaces (2 m grid, only where the surface is above the ground)
    {
        const int WN = 16; const float ws = CHUNK / WN;
        std::vector<float> lvl((WN + 1) * (WN + 1)), gnd((WN + 1) * (WN + 1));
        std::vector<uint8_t> lavaF((WN + 1) * (WN + 1)), frozenF((WN + 1) * (WN + 1));
        std::vector<vec2> flows((WN + 1) * (WN + 1));
        bool any = false;
        for (int j = 0; j <= WN; j++)
            for (int i = 0; i <= WN; i++) {
                float x = x0 + i * ws, z = z0 + j * ws;
                bool lv, fr; vec2 fl;
                float l = waterLevel(x, z, &lv, &fr, &fl);
                int id = j * (WN + 1) + i;
                lvl[id] = l; lavaF[id] = lv; frozenF[id] = fr; flows[id] = fl;
                gnd[id] = (lod == 0 && (i * (int)ws) % step == 0) ? hAt(std::min(N, (int)(i * ws / step)), std::min(N, (int)(j * ws / step))) : height(x, z);
                if (l > gnd[id] - 0.05f) any = true;
            }
        if (any) {
            for (int j = 0; j < WN; j++)
                for (int i = 0; i < WN; i++) {
                    int ids[4] = {j * (WN + 1) + i, j * (WN + 1) + i + 1, (j + 1) * (WN + 1) + i + 1, (j + 1) * (WN + 1) + i};
                    bool wet = false; float lv = -1e9f; bool isLava = false;
                    for (int id : ids) if (lvl[id] > gnd[id] - 0.05f) { wet = true; lv = std::max(lv, lvl[id]); isLava |= lavaF[id] != 0; }
                    if (!wet) continue;
                    MeshBuilder& M = isLava ? out.lava : out.water;
                    uint32_t base = (uint32_t)M.v.size();
                    float cx[4] = {0, 1, 1, 0}, cz[4] = {0, 0, 1, 1};
                    for (int c = 0; c < 4; c++) {
                        int id = ids[c];
                        float level = lvl[id] > -1e8f ? lvl[id] : lv;
                        Vertex v{};
                        v.px = x0 + (i + cx[c]) * ws; v.py = level; v.pz = z0 + (j + cz[c]) * ws;
                        v.ny = 127;
                        float depth = std::max(0.f, level - gnd[id]);
                        v.a = (uint8_t)clampf(depth / 8.f * 255.f, 0, 255);
                        v.r = frozenF[id] ? 255 : 0;
                        vec2 f = flows[id];
                        float fm = std::min(1.f, length(f));
                        vec2 fd = fm > 0.001f ? f / fm : vec2(0, 0);
                        v.rough = (uint8_t)clampf((fd.x * 0.5f + 0.5f) * 255, 0, 255);
                        v.metal = (uint8_t)clampf((fd.y * 0.5f + 0.5f) * 255, 0, 255);
                        v.emis = (uint8_t)(fm / 3.f * 255.f);
                        M.v.push_back(v);
                    }
                    M.tri(base, base + 3, base + 2); M.tri(base, base + 2, base + 1);
                    if (isLava && ((i + j) % 5 == 0)) out.lights.push_back(vec4(x0 + (i + 0.5f) * ws, lv + 1.5f, z0 + (j + 0.5f) * ws, LI_LAVA));
                }
        }
    }
    if (lod >= 2) return;  // distant chunks: terrain + water only

    // ---- structures
    int ci = chunkIndex(k.x, k.z);
    if (ci >= 0) for (int si : chunkStructures[ci]) buildStructure(out, structures[si], *this);
    if (ci >= 0 && lod == 0) for (int si : chunkStructures[ci]) dressStructure(out, structures[si], *this);   // 2.0.0 yards, jars, nets, boats, palms

    // ---- vegetation & props scatter (deterministic per chunk)
    Rng rng(hash2i(k.x, k.z, 4242));
    const float cell = 4.f;
    const int CN = (int)(CHUNK / cell);
    auto blocked = [&](float x, float z, float margin) {
        if (roadInfluence(x, z) > 0.05f) return true;
        if (ci >= 0)
            for (int pi : chunkPois[ci]) {
                const Poi& p = pois[pi];
                float rr = p.type == PoiType::Waystone ? 8.f : (p.type == PoiType::Cave || p.type == PoiType::Mine ? p.radius + 4 : (p.type == PoiType::Village || p.type == PoiType::Monastery || p.type == PoiType::Mill ? p.radius * 0.5f : p.radius * 0.95f));   // 2.0.0 villages keep their plaza and houses clear, trees grow in between
                if (length(vec2(x, z) - p.pos) < rr + margin) return true;
            }
        for (auto& c : out.colliders) {
            float rr = c.r > 0 ? c.r : std::max(c.hx, c.hz) * 1.2f;
            if (length(vec2(x, z) - c.c) < rr + margin) return true;
        }
        if (ci >= 0)   // 2.0.0 the whole footprint of this chunk's own buildings (a forge's collider covers only its hearth), piers included
            for (int si : chunkStructures[ci]) {
                const Structure& s = structures[si];
                if (s.kind == S_FIELD || s.kind == S_PEN) continue;
                vec2 fp = s.kind == S_PIER ? vec2(1.6f, 7.5f) : structureFootprint(s.kind) * s.scale;
                vec2 l = rotate(vec2(x, z) - s.pos, s.rot);
                if (std::fabs(l.x) < fp.x + margin && std::fabs(l.y) < fp.y + margin) return true;
            }
        // 1.12.0 buildings of the neighbouring chunks too (a house on a chunk border used to get a tree inside)
        for (int dz = -1; dz <= 1; dz++)
            for (int dx = -1; dx <= 1; dx++) {
                int cj = chunkIndex(k.x + dx, k.z + dz);
                if (cj < 0 || (dx == 0 && dz == 0)) continue;
                for (int si : chunkStructures[cj]) {
                    const Structure& s = structures[si];
                    if (s.kind == S_FIELD || s.kind == S_PEN) continue;
                    vec2 fp = structureFootprint(s.kind) * s.scale;
                    vec2 l = rotate(vec2(x, z) - s.pos, s.rot);
                    if (std::fabs(l.x) < fp.x + margin && std::fabs(l.y) < fp.y + margin) return true;
                }
            }
        return false;
    };
    for (int j = 0; j < CN; j++)
        for (int i = 0; i < CN; i++) {
            float x = x0 + (i + rng.f()) * cell, z = z0 + (j + rng.f()) * cell;
            RegionWeights rw = regionAt(x, z);
            int r = rw.dominant;
            // regions intermix vegetation near borders
            float pick = rng.f(), acc = 0;
            for (int rr = 0; rr < R_COUNT; rr++) { acc += rw.w[rr]; if (pick <= acc) { r = rr; break; } }
            float dens = forestDensity(r, x, z);
            if (rng.f() > dens) continue;
            float y = height(x, z);
            vec3 n = normal(x, z);
            if (n.y < 0.8f) continue;
            float wl = waterLevel(x, z);
            const auto& tab = scatterTable(r);
            float tw = 0;
            for (auto& e : tab) tw += e.weight;
            float pv = rng.f() * tw;
            const ScatterEntry* ent = &tab[0];
            for (auto& e : tab) { pv -= e.weight; if (pv <= 0) { ent = &e; break; } }
            bool aquatic = ent->k == P_REEDS || ent->k == P_LILYPADS;
            if (wl > -1e8f) {
                if (aquatic) { if (y > wl + 0.6f || y < wl - 0.8f) continue; }
                else if (y < wl + 0.5f) continue;
            } else if (aquatic) continue;
            if (blocked(x, z, ent->collide ? 2.5f : 1.0f)) continue;
            float sc = rng.range(ent->smin, ent->smax);
            vec3 tint = vec3(1.f) * rng.range(0.88f, 1.1f);
            stampProp(out, ent->k, rng.irange(0, propVariants(ent->k) - 1), {x, y - 0.05f, z}, rng.range(0, TAU), sc, tint, ent->collide);
            if ((ent->k == P_GIANTSHROOM || ent->k == P_MAGICCRYSTAL) && rng.chance(0.5f)) out.lights.push_back(vec4(x, y + 2.f * sc, z, ent->k == P_GIANTSHROOM ? LI_MAGIC : LI_CRYSTAL));
        }
    // Small decorations only avoid roads, buildings and the core of settlements (the full POI radius covers
    // the surrounding fields, which is exactly where flowers and fences belong).
    auto clutterBlocked = [&](float x, float z, float margin) {
        if (roadInfluence(x, z) > 0.05f) return true;
        if (ci >= 0)
            for (int pi : chunkPois[ci]) { const Poi& p = pois[pi]; if (length(vec2(x, z) - p.pos) < p.radius * 0.55f + margin) return true; }
        for (auto& c : out.colliders) { float rr = c.r > 0 ? c.r : std::max(c.hx, c.hz) * 1.2f; if (length(vec2(x, z) - c.c) < rr + margin) return true; }
        return false;
    };
    // ---- ground clutter (1.5): small non-blocking life between the trees (flowers, tall grass, pebbles...)
    // Near chunks only (LOD0): invisible further away, and it keeps the triangle budget for mobile.
    if (lod == 0 && grassDensity > 0.2f) {   // off on the battery-saver profile
        struct Clutter { PropKind k; float w, smin, smax; };
        static const std::vector<Clutter> CT[R_COUNT] = {
            {{P_FLOWERS, 3.f, 0.8f, 1.3f}, {P_TALLGRASS, 2.5f, 0.8f, 1.3f}, {P_PEBBLES, 1.f, 0.8f, 1.3f}, {P_BRANCHES, 0.5f, 0.8f, 1.2f}, {P_BUSH, 0.6f, 0.5f, 0.8f}, {P_SMALLSHROOMS, 0.3f, 0.8f, 1.2f}},
            {{P_PEBBLES, 2.f, 0.8f, 1.4f}, {P_TALLGRASS, 1.f, 0.7f, 1.1f}, {P_BRANCHES, 0.5f, 0.8f, 1.2f}},
            {{P_PEBBLES, 2.f, 0.8f, 1.5f}, {P_BRANCHES, 0.8f, 0.8f, 1.2f}, {P_BONES, 0.2f, 0.7f, 1.f}},
            {{P_FERN, 2.f, 0.6f, 1.f}, {P_SMALLSHROOMS, 2.f, 0.8f, 1.3f}, {P_FLOWERS, 1.f, 0.8f, 1.2f}, {P_BRANCHES, 0.6f, 0.8f, 1.2f}},
            {{P_TALLGRASS, 2.f, 0.8f, 1.3f}, {P_SMALLSHROOMS, 1.f, 0.8f, 1.3f}, {P_BRANCHES, 1.f, 0.8f, 1.2f}, {P_PEBBLES, 0.6f, 0.8f, 1.2f}},
            {{P_FLOWERS, 1.5f, 0.7f, 1.1f}, {P_TALLGRASS, 2.f, 0.7f, 1.1f}, {P_PEBBLES, 2.f, 0.8f, 1.5f}},
            {{P_PEBBLES, 2.5f, 0.8f, 1.5f}, {P_DRYBUSH, 1.2f, 0.5f, 0.9f}, {P_BONES, 0.15f, 0.7f, 1.f}, {P_BRANCHES, 0.5f, 0.8f, 1.2f}},
            {{P_SHELLS, 2.f, 0.8f, 1.3f}, {P_FLOWERS, 1.5f, 0.8f, 1.3f}, {P_FERN, 2.f, 0.6f, 1.f}, {P_PEBBLES, 1.f, 0.8f, 1.3f}, {P_TALLGRASS, 1.2f, 0.8f, 1.2f}},
            {{P_FLOWERS, 2.f, 0.7f, 1.2f}, {P_TALLGRASS, 3.f, 0.8f, 1.3f}, {P_PEBBLES, 1.f, 0.8f, 1.3f}, {P_BONES, 0.1f, 0.7f, 1.f}},           // 2.4.0 steppe
            {{P_FERN, 3.f, 0.7f, 1.2f}, {P_SMALLSHROOMS, 1.2f, 0.8f, 1.2f}, {P_FLOWERS, 1.f, 0.8f, 1.2f}, {P_BRANCHES, 1.f, 0.8f, 1.2f}},    // 2.4.0 jungle
            {{P_FLOWERS, 2.5f, 0.8f, 1.3f}, {P_SMALLSHROOMS, 2.f, 0.8f, 1.3f}, {P_TALLGRASS, 1.5f, 0.8f, 1.2f}, {P_PEBBLES, 1.f, 0.8f, 1.3f}},  // 2.8.0 aurora
        };
        static const int attempts[R_COUNT] = {16, 8, 6, 14, 10, 12, 7, 15, 14, 16, 14};
        Rng cr(hash2i(k.x, k.z, 9191));
        int n = (int)(attempts[crw.dominant] * clampf(grassDensity * 1.25f, 0.3f, 1.f));
        for (int t = 0; t < n; t++) {
            float x = x0 + cr.f() * CHUNK, z = z0 + cr.f() * CHUNK;
            int r = regionId(x, z);
            const auto& tab = CT[r];
            float tw = 0; for (auto& e : tab) tw += e.w;
            float pv = cr.f() * tw; const Clutter* e = &tab[0];
            for (auto& c : tab) { pv -= c.w; if (pv <= 0) { e = &c; break; } }
            float y = height(x, z);
            if (normal(x, z).y < 0.85f || waterLevel(x, z) > y - 0.3f || clutterBlocked(x, z, 0.6f)) continue;
            // flower colour follows large patches (fields of daisies, poppies, lavender)
            int var = e->k == P_FLOWERS ? (int)clampf((fbm2(x * 0.01f, z * 0.01f, 2, 515) * 0.5f + 0.5f) * 3.f, 0.f, 2.99f)
                    : (e->k == P_TALLGRASS && r == R_HIGHCROWNS ? 2 : cr.irange(0, propVariants(e->k) - 1));
            if (e->k == P_FLOWERS || e->k == P_TALLGRASS) {
                // patches: several clumps grouped over a few metres read as a field from the game camera
                int clumps = e->k == P_FLOWERS ? cr.irange(5, 10) : cr.irange(3, 6);
                for (int q = 0; q < clumps; q++) {
                    vec2 o = cr.inDisk() * (e->k == P_FLOWERS ? 2.6f : 1.8f);
                    float px = x + o.x, pz = z + o.y, py = height(px, pz);
                    if (waterLevel(px, pz) > py - 0.3f || clutterBlocked(px, pz, 0.3f)) continue;
                    stampProp(out, e->k, var, {px, py - 0.03f, pz}, cr.range(0, TAU), cr.range(e->smin, e->smax) * 1.2f, vec3(1.f) * cr.range(0.9f, 1.08f), false);
                }
            } else stampProp(out, e->k, var, {x, y - 0.03f, z}, cr.range(0, TAU), cr.range(e->smin, e->smax), vec3(1.f) * cr.range(0.9f, 1.08f), false);
        }
    }

    // ---- road furniture (1.5): fences along country roads, lanterns and carts near settlements, signposts
    auto blockedNoRoad = [&](float x, float z, float margin) {   // POIs and structures only (furniture sits beside roads)
        if (ci >= 0)
            for (int pi : chunkPois[ci]) { const Poi& p = pois[pi]; if (length(vec2(x, z) - p.pos) < p.radius * 0.6f + margin) return true; }
        for (auto& c : out.colliders) { float rr = c.r > 0 ? c.r : std::max(c.hx, c.hz) * 1.2f; if (length(vec2(x, z) - c.c) < rr + margin) return true; }
        return false;
    };
    if (ci >= 0 && lod < 2) {
        Rng rr(hash2i(k.x, k.z, 7373));
        for (int code : chunkRoads[ci]) {
            const Road& rd = roads[code >> 16];
            int sgi = code & 0xffff;
            vec2 a = rd.pts[sgi], b = rd.pts[sgi + 1];
            float len = length(b - a);
            if (len < 1.f) continue;
            vec2 dir = (b - a) / len, side = vec2(-dir.y, dir.x);
            float yaw = std::atan2(dir.x, dir.y);
            for (float s = 4.f; s < len - 2.f; s += 9.f) {
                vec2 p = a + dir * s;
                if (p.x < x0 || p.x >= x0 + CHUNK || p.y < z0 || p.y >= z0 + CHUNK) continue;   // each chunk owns its points
                int r = regionId(p.x, p.y);
                float nearVillage = 1e9f;
                for (auto& P : pois) if (P.type == PoiType::Village || P.type == PoiType::Castle || P.type == PoiType::Farm) nearVillage = std::min(nearVillage, length(P.pos - p) - P.radius);
                float off = rd.width * 0.5f + 1.3f;
                for (int sd = -1; sd <= 1; sd += 2) {
                    vec2 q = p + side * (off * sd);
                    float y = height(q.x, q.y);
                    if (waterLevel(q.x, q.y) > y - 0.3f || normal(q.x, q.y).y < 0.88f || roadInfluence(q.x, q.y) > 0.6f || blockedNoRoad(q.x, q.y, 0.8f)) continue;
                    bool farmland = (r == R_GREENHAVEN || r == R_HIGHCROWNS) && fbm2(q.x * 0.008f, q.y * 0.008f, 2, 4141) > 0.05f;
                    if (nearVillage < 70.f && nearVillage > 2.f && ((int)(s / 9.f) + (sd > 0)) % 2 == 0 && rr.chance(0.7f)) {
                        stampProp(out, P_LANTERNPOST, 0, {q.x, y - 0.05f, q.y}, yaw + (sd > 0 ? PI : 0.f), 1.f, vec3(1), true);
                        vec2 lp = q + side * (sd * -0.5f);
                        out.lights.push_back(vec4(lp.x, y + 2.15f, lp.y, LI_LANTERN));
                    } else if (farmland && nearVillage < 220.f && nearVillage > 3.f && rr.chance(0.75f)) {
                        stampProp(out, P_FENCE, 0, {q.x, y - 0.05f, q.y}, yaw + PI / 2, 1.f, vec3(1) * rr.range(0.9f, 1.05f), false);
                    } else if (nearVillage < 45.f && nearVillage > 4.f && rr.chance(0.06f)) {
                        vec2 c = q + side * (sd * 1.8f);
                        stampProp(out, P_CART, rr.irange(0, 2), {c.x, height(c.x, c.y) - 0.05f, c.y}, yaw + rr.range(-0.4f, 0.4f), 1.f, vec3(1), true);
                    }
                }
                // signposts where roads leave settlements and at long intervals
                if (rr.chance(nearVillage < 120.f && nearVillage > 20.f ? 0.06f : 0.012f)) {
                    vec2 q = p + side * (off + 0.6f);
                    float y = height(q.x, q.y);
                    if (waterLevel(q.x, q.y) < y - 0.3f && !blockedNoRoad(q.x, q.y, 0.8f) && roadInfluence(q.x, q.y) < 0.6f) stampProp(out, P_SIGNPOST, rr.irange(0, 1), {q.x, y - 0.05f, q.y}, yaw, 1.f, vec3(1), true);
                }
            }
        }
    }

    // ---- wild landmarks (1.5): small scenes to stumble upon (camps, stone circles, fairy rings, ruins)
    if (lod < 2) {
        Rng vr(hash2i(k.x, k.z, 5151));
        if (vr.chance(0.05f)) {
            vec2 c{x0 + vr.range(8.f, CHUNK - 8.f), z0 + vr.range(8.f, CHUNK - 8.f)};
            float cy = height(c.x, c.y);
            bool ok = normal(c.x, c.y).y > 0.93f && waterLevel(c.x, c.y) < cy - 0.5f && !blocked(c.x, c.y, 6.f) && roadInfluence(c.x, c.y) < 0.01f;
            for (int t = 0; ok && t < 6; t++) { float a = t * TAU / 6; vec2 q = c + vec2(std::cos(a), std::sin(a)) * 5.f; if (std::fabs(height(q.x, q.y) - cy) > 1.2f || roadInfluence(q.x, q.y) > 0.05f) ok = false; }
            if (ok) {
                int r = regionId(c.x, c.y);
                auto put = [&](PropKind pk, int var, vec2 q, float yaw, float sc, bool col) { float y = height(q.x, q.y); if (waterLevel(q.x, q.y) < y - 0.3f) stampProp(out, pk, var, {q.x, y - 0.05f, q.y}, yaw, sc, vec3(1), col); };
                int kind = vr.irange(0, 2);
                if (r == R_ELDERWOOD) {
                    // fairy ring of glowing mushrooms around a crystal
                    for (int t = 0; t < 12; t++) { float a = t * TAU / 12 + vr.range(-0.1f, 0.1f); put(P_SMALLSHROOMS, 1, c + vec2(std::cos(a), std::sin(a)) * 3.2f, vr.range(0, TAU), 1.1f, false); }
                    put(P_MAGICCRYSTAL, vr.irange(0, 2), c, vr.range(0, TAU), 0.8f, true);
                    out.lights.push_back(vec4(c.x, cy + 1.5f, c.y, LI_MAGIC)); out.emitters.push_back(vec4(c.x, cy + 0.5f, c.y, EM_SPARKLES));
                } else if (r == R_SHADOWS) {
                    // forgotten graves around a rune stone
                    put(P_STANDSTONE, 2, c, vr.range(0, TAU), 1.f, true);
                    for (int t = 0; t < 6; t++) { vec2 q = c + vr.inDisk() * 6.f; if (length(q - c) > 2.f) put(P_GRAVESTONE, vr.irange(0, 2), q, vr.range(-0.3f, 0.3f), 1.f, true); }
                    out.lights.push_back(vec4(c.x, cy + 1.6f, c.y, LI_MAGIC));
                } else if (kind == 0 || r == R_ASHEN) {
                    // stone circle (ash lands: obsidian)
                    int nS = vr.irange(6, 8);
                    for (int t = 0; t < nS; t++) { float a = t * TAU / nS; vec2 q = c + vec2(std::cos(a), std::sin(a)) * 5.5f; if (r == R_ASHEN) put(P_OBSIDIAN, vr.irange(0, 2), q, a, 1.f, true); else put(P_STANDSTONE, vr.irange(0, 2), q, a + PI / 2, vr.range(0.85f, 1.1f), true); }
                    put(P_STANDSTONE, 2, c, vr.range(0, TAU), 1.25f, true);
                    out.lights.push_back(vec4(c.x, cy + 2.f, c.y, LI_MAGIC));
                } else if (kind == 1) {
                    // abandoned camp: fire, tents, crates, a log to sit on
                    put(P_CAMPFIRE, 0, c, 0, 1.f, true);
                    out.lights.push_back(vec4(c.x, cy + 0.8f, c.y, LI_FIRE)); out.emitters.push_back(vec4(c.x, cy + 0.3f, c.y, EM_FIRE));
                    for (int t = 0; t < 2; t++) { float a = vr.range(0, TAU); vec2 q = c + vec2(std::cos(a), std::sin(a)) * 4.2f; put(P_TENT, r == R_FROSTHEIM ? 2 : vr.irange(0, 2), q, a + PI / 2, 1.f, true); }
                    put(P_LOG, 0, c + vec2(1.8f, -0.6f), 0.4f, 0.8f, true);
                    put(P_CRATE, 0, c + vec2(-2.6f, 2.2f), 0.3f, 1.f, true);
                    put(P_BARREL, 0, c + vec2(-3.2f, 1.4f), 0.f, 1.f, true);
                } else {
                    // ruins: broken columns and fallen stones
                    for (int t = 0; t < 5; t++) { vec2 q = c + vr.inDisk() * 6.f; put(t % 2 ? P_BOULDER : P_RUINCOLUMN, vr.irange(0, 2), q, vr.range(0, TAU), vr.range(0.7f, 1.1f), true); }
                    put(P_TALLGRASS, 0, c + vec2(1.f, 1.f), 0, 1.2f, false);
                }
            }
        }
    }

    // lily pads on calm water in swamps & Elderwood
    if (crw.dominant == R_SHADOWS || crw.dominant == R_ELDERWOOD)
        for (int n = 0; n < 10; n++) {
            float x = x0 + rng.f() * CHUNK, z = z0 + rng.f() * CHUNK;
            vec2 fl; bool lv, fr;
            float wl = waterLevel(x, z, &lv, &fr, &fl);
            float y = height(x, z);
            if (wl > -1e8f && !lv && !fr && length(fl) < 0.01f && wl - y > 0.4f && wl - y < 2.5f) stampProp(out, P_LILYPADS, rng.irange(0, 2), {x, wl + 0.02f, z}, rng.range(0, TAU), rng.range(0.8f, 1.3f), vec3(1), false);
        }

    // ---- harvest / interaction nodes
    auto addNode = [&](NodeType t, float x, float z, int tier) {
        float y = height(x, z);
        uint32_t uid = hash3i(k.x, k.z, (int)out.nodes.size() * 31 + (int)t, 999);
        out.nodes.push_back({t, {x, y, z}, rng.range(0, TAU), uid, tier});
    };
    int nNodes = rng.irange(2, 4);   // 2.3.5 more resources (was 1-3)
    for (int n = 0; n < nNodes; n++) {
        float x = x0 + rng.range(2, CHUNK - 2), z = z0 + rng.range(2, CHUNK - 2);
        if (blocked(x, z, 1.5f)) continue;
        float wl = waterLevel(x, z);
        float y = height(x, z);
        if (wl > -1e8f && y < wl + 0.3f) {
            // 2.4.0 something to dive for: shell beds (pearls) on the bottom of lakes and coves 2.5 to 8 m deep
            bool lavaW = false, frozenW = false; waterLevel(x, z, &lavaW, &frozenW);
            if (!lavaW && !frozenW && wl - y > 2.5f && wl - y < 8.f && rng.chance(0.35f)) addNode(NodeType::Shells, x, z, 2);
            continue;
        }
        if (normal(x, z).y < 0.82f) continue;
        int r = regionId(x, z);
        float p = rng.f();
        NodeType t = NodeType::Rock;
        int tier = 0;
        switch (r) {
        // 2.3.5: each region also has its own new resources (flax, silver, coal, fire root, gold, cactus, amber, shells)
        case R_GREENHAVEN: t = p < 0.26f ? NodeType::Tree : p < 0.42f ? NodeType::Herb : p < 0.52f ? NodeType::Berries : p < 0.64f ? NodeType::Flax : p < 0.75f ? NodeType::Rock : p < 0.88f ? NodeType::CopperOre : NodeType::IronOre; break;
        case R_FROSTHEIM: t = p < 0.22f ? NodeType::Tree : p < 0.38f ? NodeType::Rock : p < 0.58f ? NodeType::IronOre : p < 0.74f ? NodeType::SilverOre : p < 0.87f ? NodeType::Crystal : NodeType::Herb; tier = 1; break;
        case R_ASHEN: t = p < 0.2f ? NodeType::Rock : p < 0.4f ? NodeType::CopperOre : p < 0.58f ? NodeType::Coal : p < 0.76f ? NodeType::RareOre : p < 0.88f ? NodeType::FireRoot : NodeType::Crystal; tier = 2; break;
        case R_ELDERWOOD: t = p < 0.28f ? NodeType::Herb : p < 0.46f ? NodeType::Mushroom : p < 0.62f ? NodeType::Crystal : p < 0.8f ? NodeType::Tree : p < 0.9f ? NodeType::Flax : NodeType::Berries; tier = 1; break;
        case R_SHADOWS: t = p < 0.3f ? NodeType::Mushroom : p < 0.52f ? NodeType::Herb : p < 0.7f ? NodeType::Tree : p < 0.82f ? NodeType::Coal : p < 0.92f ? NodeType::FireRoot : NodeType::IronOre; tier = 1; break;
        case R_AMBER: t = p < 0.18f ? NodeType::Rock : p < 0.34f ? NodeType::CopperOre : p < 0.48f ? NodeType::GoldOre : p < 0.62f ? NodeType::Cactus : p < 0.74f ? NodeType::AmberRock : p < 0.86f ? NodeType::RareOre : p < 0.94f ? NodeType::Crystal : NodeType::Herb; tier = 3; break;
        case R_STEPPE: t = p < 0.18f ? NodeType::Flax : p < 0.34f ? NodeType::Herb : p < 0.48f ? NodeType::Berries : p < 0.62f ? NodeType::Rock : p < 0.74f ? NodeType::CopperOre : p < 0.86f ? NodeType::SilverOre : NodeType::Tree; tier = 3; break;
        case R_JUNGLE: t = p < 0.24f ? NodeType::Tree : p < 0.4f ? NodeType::Herb : p < 0.52f ? NodeType::Mushroom : p < 0.64f ? NodeType::Berries : p < 0.74f ? NodeType::FireRoot : p < 0.84f ? NodeType::GoldOre : p < 0.93f ? NodeType::Crystal : NodeType::RareOre; tier = 3; break;
        case R_AURORA: t = p < 0.3f ? NodeType::AuroraCrystal : p < 0.45f ? NodeType::Crystal : p < 0.6f ? NodeType::Herb : p < 0.72f ? NodeType::Mushroom : p < 0.84f ? NodeType::SilverOre : p < 0.93f ? NodeType::Tree : NodeType::RareOre; tier = 3; break;   // 2.8.0
        case R_CORAL: t = p < 0.24f ? NodeType::Tree : p < 0.42f ? NodeType::Berries : p < 0.58f ? NodeType::Shells : p < 0.72f ? NodeType::Herb : p < 0.86f ? NodeType::Rock : NodeType::Crystal; tier = 2; break;
        default: t = p < 0.24f ? NodeType::IronOre : p < 0.38f ? NodeType::SilverOre : p < 0.5f ? NodeType::GoldOre : p < 0.62f ? NodeType::RareOre : p < 0.76f ? NodeType::Rock : p < 0.88f ? NodeType::Tree : NodeType::Crystal; tier = 2; break;
        }
        addNode(t, x, z, tier);
    }
    // rare treasure chests away from settlements; relics near ruins
    if (rng.chance(0.045f)) {
        float x = x0 + rng.range(4, CHUNK - 4), z = z0 + rng.range(4, CHUNK - 4);
        float wl = waterLevel(x, z);
        if (!blocked(x, z, 1.f) && !(wl > -1e8f && height(x, z) < wl + 0.3f)) addNode(NodeType::Chest, x, z, regionId(x, z) == R_GREENHAVEN ? 0 : 1);
    }
    if (ci >= 0)
        for (int pi : chunkPois[ci]) {
            const Poi& p = pois[pi];
            if ((p.type == PoiType::Ruins || p.type == PoiType::ElvenRuins) && chunkOf(p.pos.x, p.pos.y) == k) { addNode(NodeType::Relic, p.pos.x + 3, p.pos.y + 2, 1); addNode(NodeType::Chest, p.pos.x - 4, p.pos.y + 5, 1); }
        }
    // fishing spots along shores
    for (int n = 0; n < 2; n++) {
        float x = x0 + rng.range(2, CHUNK - 2), z = z0 + rng.range(2, CHUNK - 2);
        bool lv, fr;
        float wl = waterLevel(x, z, &lv, &fr);
        float y = height(x, z);
        if (wl > -1e8f && !lv && !fr && wl - y > 0.8f && rng.chance(0.4f)) { out.nodes.push_back({NodeType::FishingSpot, {x, wl, z}, 0, hash3i(k.x, k.z, 700 + n, 999), 0}); }
    }

    // ---- grass & flowers (instanced, near LOD only)
    // 1.6.1: denser base layer, plus a "near" layer of taller clumps drawn only close to the hero (Render.cpp),
    // so the meadows look full where the player walks without paying for it in the distance.
    auto scatter = [&](uint32_t seed, float spacing, std::vector<InstanceData>& dst, std::vector<InstanceData>* flowersOut, float sMin, float sMax) {
        Rng g(hash2i(k.x, k.z, seed));
        int GN = (int)(CHUNK / spacing);
        for (int j = 0; j < GN; j++)
            for (int i = 0; i < GN; i++) {
                float x = x0 + (i + g.f()) * spacing, z = z0 + (j + g.f()) * spacing;
                // grass weight from terrain splats (bilinear lookup in our vertex grid)
                int vi = std::min(N, (int)((x - x0) / step + 0.5f)), vj = std::min(N, (int)((z - z0) / step + 0.5f));
                const Vertex& tv = T.v[vj * (N + 1) + vi];
                float wg = 1.f - (tv.rough + tv.metal + tv.emis + tv.wind + tv.b0 + tv.b1 + tv.w0) / 255.f;
                if (tv.ny < 105) continue;
                int r = regionId(x, z);
                float gd = r == R_ASHEN ? 0.12f : (r == R_FROSTHEIM ? 0.35f : (r == R_HIGHCROWNS ? 0.6f : (r == R_AMBER ? 0.05f : (r == R_CORAL ? 0.8f : 1.f))));
                if (r == R_STEPPE) gd = 1.15f;   // 2.4.0 the steppe is a sea of grass
                if (g.f() > wg * gd * 1.1f) continue;
                float y = hAt(vi, vj);
                float wl = waterLevel(x, z);
                if (wl > -1e8f && y < wl + 0.15f) continue;
                float hn = fbm2(x * 0.08f, z * 0.08f, 2, 123) * 0.5f + 0.5f;
                // colour follows large patches (lush hollows / dry crests) with per-blade jitter
                float patch = fbm2(x * 0.021f, z * 0.021f, 3, 321) * 0.5f + 0.5f;
                vec3 col = foliageColor(r, clampf(patch * 0.75f + g.f() * 0.35f - 0.05f, 0.f, 1.f));
                InstanceData d;
                // re-sample exact height across the quad (cheap bilinear)
                y = height(x, z);
                d.x = x; d.y = y - 0.03f; d.z = z; d.rotY = g.range(0, TAU); d.scale = g.range(sMin, sMax) * (0.7f + hn * 0.6f);
                Color8 c = toColor8(col * 0.5f, 1);
                d.r = c.r; d.g = c.g; d.b = c.b; d.a = 255;
                bool flower = flowersOut && g.f() < (r == R_GREENHAVEN ? 0.04f : (r == R_ELDERWOOD ? 0.05f : 0.012f)) * (0.5f + hn);
                if (flower) {
                    static const uint32_t fc[] = {0xe8d24a, 0xd94b5f, 0xf2f0e6, 0x8f6fd8, 0xf28a3a, 0x6fb8f2};
                    Color8 f = toColor8(hexColor(fc[g.irange(0, 5)]) * 0.5f, 1);
                    d.r = f.r; d.g = f.g; d.b = f.b;
                    flowersOut->push_back(d);
                } else dst.push_back(d);
            }
    };
    if (lod == 0) {
        // 1.11.1: denser (-12 % spacing) and about 30 % taller, as asked
        float spacing = 0.8f / std::sqrt(std::max(0.05f, grassDensity));   // 2.7.0 fuller tufts, slightly fewer of them
        scatter(777, spacing, out.grass, &out.flowers, 0.8f, 1.5f);
        if (grassDensity >= 0.5f) scatter(778, spacing * 1.05f, out.grassNear, nullptr, 1.05f, 1.85f);
        // 2.3.6 shuffled: drawing the first N instances thins the whole chunk evenly (distance LOD of the grass)
        Rng sh(hash2i(k.x, k.z, 9001));
        for (auto* v : {&out.grass, &out.flowers})
            for (int i = (int)v->size() - 1; i > 0; i--) std::swap((*v)[i], (*v)[sh.irange(0, i)]);
    }
    // ---- 1.9.1 tall grass meadows: waist-high clumps, dense inside the patches, shorter at their edges
    if (lod == 0 && grassDensity >= 0.3f) {
        Rng g(hash2i(k.x, k.z, 4243));
        float sp = 0.62f / std::sqrt(std::min(1.f, grassDensity));
        int GN = (int)(CHUNK / sp);
        for (int j = 0; j < GN; j++)
            for (int i = 0; i < GN; i++) {
                float x = x0 + (i + g.f()) * sp, z = z0 + (j + g.f()) * sp;
                float a = tallGrassAmount(x, z);
                if (a <= 0.f || g.f() > 0.35f + a * 0.65f) continue;
                int vi = std::min(N, (int)((x - x0) / step + 0.5f)), vj = std::min(N, (int)((z - z0) / step + 0.5f));
                const Vertex& tv = T.v[vj * (N + 1) + vi];
                float wg = 1.f - (tv.rough + tv.metal + tv.emis + tv.wind + tv.b0 + tv.b1 + tv.w0) / 255.f;
                if (tv.ny < 108 || wg < 0.45f) continue;   // not on rock, sand or steep slopes
                int r = regionId(x, z);
                float patch = fbm2(x * 0.03f, z * 0.03f, 2, 777) * 0.5f + 0.5f;
                vec3 col = foliageColor(r, clampf(patch * 0.6f + g.f() * 0.3f, 0.f, 1.f));
                vec3 straw = r == R_SHADOWS ? hexColor(0x6a6440) : hexColor(0xc8b46a);
                col = lerp(col, straw, clampf((r == R_HIGHCROWNS ? 0.55f : 0.22f) + (patch - 0.5f) * 0.5f + g.range(-0.08f, 0.12f), 0.f, 0.8f));
                InstanceData d;
                d.x = x; d.y = height(x, z) - 0.04f; d.z = z; d.rotY = g.range(0, TAU);
                d.scale = g.range(0.85f, 1.2f) * (0.55f + 0.45f * a);
                Color8 c = toColor8(col * 0.5f, 1);
                d.r = c.r; d.g = c.g; d.b = c.b; d.a = 255;
                out.tallGrass.push_back(d);
            }
    }
}

float World::tallGrassAmount(float x, float z) const {
    int r = regionId(x, z);
    float rw = r == R_GREENHAVEN ? 1.f : r == R_SHADOWS ? 0.75f : r == R_ELDERWOOD ? 0.6f : r == R_HIGHCROWNS ? 0.45f : r == R_STEPPE ? 1.f : r == R_JUNGLE ? 0.55f : r == R_AURORA ? 0.5f : 0.f;
    if (rw <= 0.f) return 0.f;
    float n = fbm2(x * 0.011f, z * 0.011f, 3, 4242) * 0.5f + 0.5f;          // meadows ~60-120 m across
    float a = (r == R_STEPPE ? smoothstepf(0.4f, 0.5f, n) : smoothstepf(0.55f, 0.64f, n)) * rw;   // 2.4.0 the steppe: waist-high grass almost everywhere
    if (a < 0.02f) return 0.f;
    if (roadInfluence(x, z) > 0.02f) return 0.f;
    for (const Poi& p : pois) if (length(p.pos - vec2(x, z)) < p.radius * 0.9f + 6.f) return 0.f;   // not in villages, camps, ruins
    // nor in ploughed fields or right against a building / fence
    int cx = (int)std::floor(x / CHUNK), cz = (int)std::floor(z / CHUNK);
    for (int dz = -1; dz <= 1; dz++)
        for (int dx = -1; dx <= 1; dx++) {
            int cj = chunkIndex(cx + dx, cz + dz);
            if (cj < 0 || cj >= (int)chunkStructures.size()) continue;
            for (int si : chunkStructures[cj]) {
                const Structure& st = structures[si];
                float rr = (st.kind == S_FIELD ? 16.f : (st.kind == S_FENCE ? 5.f : 9.f)) * std::max(1.f, st.scale);
                if (length(st.pos - vec2(x, z)) < rr) return 0.f;
            }
        }
    float wl = waterLevel(x, z);
    if (wl > -1e8f && height(x, z) < wl + 0.3f) return 0.f;
    return a;
}

// ---------------------------------------------------------------------------------------- streaming
void World::startWorkers(int n) {
    if (running) return;
    running = true;
    workerCount = n;
    for (int i = 0; i < n; i++) workers.emplace_back([this] { workerLoop(); });
}

void World::stopWorkers() {
    if (!running) return;
    {
        std::lock_guard<std::mutex> l(qmtx);
        running = false;
        jobs.clear();
    }
    qcv.notify_all();
    for (auto& t : workers) t.join();
    workers.clear();
    std::lock_guard<std::mutex> l(qmtx);
    done.clear(); inFlight.clear();
}

void World::workerLoop() {
    while (true) {
        std::pair<ChunkKey, int> job;
        {
            std::unique_lock<std::mutex> l(qmtx);
            qcv.wait(l, [&] { return !running || !jobs.empty(); });
            if (!running) return;
            job = jobs.front(); jobs.pop_front();
        }
        double t0 = platform::timeSeconds();
        auto b = std::make_unique<ChunkBuild>();
        generateChunkSync(job.first, job.second, *b);
        double ms = (platform::timeSeconds() - t0) * 1000;
        std::lock_guard<std::mutex> l(qmtx);
        genMsTotal += ms;
        chunksGenerated++;
        done.push_back(std::move(b));
    }
}

size_t World::pendingChunks() { std::lock_guard<std::mutex> l(qmtx); return jobs.size() + inFlight.size(); }

void World::uploadChunk(ChunkBuild& b) {
    auto& slot = chunks[b.key];
    if (!slot) { slot = std::make_unique<Chunk>(); slot->key = b.key; }
    uploadBuild(b, *slot);
}

// 2.2.2 also used for scenery built outside the world grid (the celestial realm)
void World::uploadBuild(ChunkBuild& b, Chunk& c) {
    c.destroy();
    c.lod = b.lod;
    if (!b.terrain.v.empty()) b.terrain.upload(c.terrain);
    if (!b.props.v.empty()) b.props.upload(c.props);
    if (!b.foliage.v.empty()) b.foliage.upload(c.foliage);
    if (!b.water.v.empty()) b.water.upload(c.water);
    if (!b.lava.v.empty()) b.lava.upload(c.lava);
    if (!b.tallGrass.empty()) {
        // tall clump: 8 thin blades up to 1.35 m, two seed heads, leaning out like a real tussock (~60 triangles)
        static MeshBuilder tall;
        if (tall.v.empty()) {
            Mat root(hexColor(0x3a5a22), 0.9f, 0, 0, 0.f), tip(hexColor(0xb8c470), 0.75f, 0, 0, 1.f), seed(hexColor(0xd8c890), 0.8f, 0, 0, 1.f);
            Rng br(11);
            for (int i = 0; i < 8; i++) {
                float a = i * TAU / 8 + br.range(-0.3f, 0.3f);
                vec3 side{std::cos(a), 0, std::sin(a)};
                vec2 off = br.inDisk() * 0.26f;
                vec3 base{off.x, 0, off.y};
                float h = br.range(0.8f, 1.35f);
                vec3 lean = vec3(off.x, 0, off.y) * 0.9f + vec3(-side.z, 0, side.x) * 0.06f + vec3(br.range(-0.12f, 0.12f), 0, br.range(-0.12f, 0.12f));
                tall.blade(base, base + vec3(0, h, 0) + lean, 0.075f, side, root, tip);
                if (i % 4 == 0) {   // seed head on a stem
                    vec3 top = base + vec3(0, h + 0.12f, 0) + lean * 1.1f;
                    tall.blade(top - vec3(0, 0.2f, 0) - lean * 0.1f, top, 0.05f, side, seed.withWind(0.85f), seed);
                }
            }
            for (auto& v : tall.v) {
                v.r = v.g = v.b = (uint8_t)clampf(140 + v.py * 90, 0, 255);
                vec3 n = normalize(unpackNormal(v) * 0.3f + vec3(0, 1, 0));
                v.nx = (int8_t)(n.x * 127); v.ny = (int8_t)(n.y * 127); v.nz = (int8_t)(n.z * 127);
                v.a = (uint8_t)clampf(120 + v.py * 140, 0, 255);   // darker at the root (ambient occlusion)
            }
        }
        tall.upload(c.tallGrass); c.tallGrass.setInstances(b.tallGrass);
    }
    if (!b.grass.empty() || !b.flowers.empty() || !b.grassNear.empty()) {
        // grass blade / flower templates are shared meshes; per-chunk VAO with instance buffer
        static MeshBuilder blade, bladeNear, flower;
        if (blade.v.empty()) {
            // 2.7.0 a tuft grows from one root: thin blades that rise from a tight base, arc over and lean outwards
            // (9 blades; 13 plus seed stalks for the layer drawn close to the hero). The root is dark and sits in
            // the soil, the tips are lighter and warmer, each blade has its own shade.
            auto tuft = [](MeshBuilder& m, int n, float hMin, float hMax, int seeds, uint32_t seed) {
                Rng br(seed);
                for (int i = 0; i < n; i++) {
                    float a = i * TAU / n + br.range(-0.35f, 0.35f);
                    vec3 side{std::cos(a), 0, std::sin(a)};
                    vec2 off = br.inDisk() * 0.12f;
                    vec3 base{off.x, -0.02f, off.y};
                    float h = br.range(hMin, hMax);
                    vec3 outw = vec3(-side.z, 0, side.x) * (br.chance(0.5f) ? 1.f : -1.f);
                    vec3 lean = outw * h * br.range(0.18f, 0.5f) + vec3(off.x, 0, off.y) * 1.2f;
                    float sh = br.range(0.82f, 1.12f);
                    Mat root(hexColor(0x2c4a1a) * sh, 0.9f, 0, 0, 0.f), tip(hexColor(0xa8c860) * sh, 0.7f, 0, 0, 1.f);
                    m.blade(base, base + vec3(0, h, 0) + lean, br.range(0.055f, 0.085f), side, root, tip, 1.f);
                }
                for (int i = 0; i < seeds; i++) {   // a few seed stalks above the tuft
                    float a = br.range(0, TAU);
                    vec3 side{std::cos(a), 0, std::sin(a)};
                    vec3 base{br.range(-0.05f, 0.05f), -0.02f, br.range(-0.05f, 0.05f)};
                    vec3 top = base + vec3(br.range(-0.12f, 0.12f), hMax * br.range(1.15f, 1.35f), br.range(-0.12f, 0.12f));
                    Mat stem(hexColor(0x5a7a32), 0.9f, 0, 0, 0.f), head(hexColor(0xd8c890), 0.8f, 0, 0, 1.f);
                    m.blade(base, top, 0.025f, side, stem, stem.withWind(0.9f));
                    m.blade(top - (top - base) * 0.16f, top + (top - base) * 0.04f, 0.06f, side, head.withWind(0.95f), head);
                }
            };
            tuft(blade, 8, 0.4f, 0.72f, 0, 5);
            tuft(bladeNear, 10, 0.4f, 0.7f, 0, 6);
            // 2.7.0 a flower: a thin bent stem with two leaves, a cup of 6 petals turned up, a yellow heart
            Mat stem(hexColor(0x3f6a2a), 0.9f, 0, 0, 0.f), petal(vec3(1.f), 0.6f, 0, 0.05f, 1.f), heart(hexColor(0xf0c030), 0.6f, 0, 0.05f, 1.f);
            vec3 head{0.04f, 0.4f, 0.02f};
            flower.blade({0, -0.02f, 0}, head, 0.022f, {1, 0, 0}, stem, stem.withWind(0.8f), 1.f);
            flower.blade({0, 0, 0}, {0.13f, 0.14f, 0.03f}, 0.05f, {0, 0, 1}, stem, stem.withWind(0.3f), 1.f);
            flower.blade({0, 0, 0}, {-0.1f, 0.12f, -0.06f}, 0.045f, {0, 0, 1}, stem, stem.withWind(0.3f), 1.f);
            for (int i = 0; i < 6; i++) {
                float a = i * TAU / 6;
                vec3 d{std::cos(a), 0.75f, std::sin(a)};
                flower.blade(head, head + d * 0.075f, 0.055f, vec3(-d.z, 0, d.x), petal, petal);
            }
            flower.ellipsoid(head + vec3(0, 0.015f, 0), vec3(0.022f, 0.016f, 0.022f), heart, 5, 3);
            for (MeshBuilder* m : {&blade, &bladeNear})
                for (auto& v : m->v) {
                    // tinted per instance (x2 in shader): keep the blade's own shade, darker at the root, a touch
                    // warmer (yellower) at the tip
                    float k = clampf(v.py / 0.8f, 0.f, 1.f);
                    float lum = (v.r + v.g + v.b) / (3.f * 255.f);
                    float base = (120 + k * 150) * (0.75f + 0.5f * lum);
                    v.r = (uint8_t)clampf(base * (1.f + 0.1f * k), 0, 255); v.g = (uint8_t)clampf(base, 0, 255); v.b = (uint8_t)clampf(base * (1.f - 0.18f * k), 0, 255);
                    // ground-like normals so blades shade like the terrain they grow on (no dark "hairs")
                    vec3 n = normalize(unpackNormal(v) * 0.3f + vec3(0, 1, 0));
                    v.nx = (int8_t)(n.x * 127); v.ny = (int8_t)(n.y * 127); v.nz = (int8_t)(n.z * 127);
                    v.a = (uint8_t)clampf(95 + k * 230, 0, 255);  // AO: the root is in the shade of the tuft
                }
            for (auto& v : flower.v) { if (v.py < 0.39f) { v.r = 70; v.g = 120; v.b = 50; } else if (v.g > 150 && v.b < 90) { v.r = 255; v.g = 210; v.b = 60; } vec3 n = normalize(unpackNormal(v) * 0.4f + vec3(0, 1, 0)); v.nx = (int8_t)(n.x * 127); v.ny = (int8_t)(n.y * 127); v.nz = (int8_t)(n.z * 127); }
        }
        if (!b.grass.empty()) { blade.upload(c.grass); c.grass.setInstances(b.grass); }
        if (!b.grassNear.empty()) { bladeNear.upload(c.grassNear); c.grassNear.setInstances(b.grassNear); }
        if (!b.flowers.empty()) { flower.upload(c.flowers); c.flowers.setInstances(b.flowers); }
    }
    c.colliders = std::move(b.colliders);
    c.nodes = std::move(b.nodes);
    c.lights = std::move(b.lights);
    c.emitters = std::move(b.emitters);
    c.models = std::move(b.models);
    c.region = b.region;
    c.snow = b.snow;
    c.bounds = AABB();
    for (const Mesh* m : {&c.terrain, &c.props, &c.foliage, &c.water}) if (m->valid()) c.bounds.add(m->bounds);
    if (c.grass.valid()) { c.bounds.add(c.grass.bounds.lo + vec3(b.key.x * CHUNK, -50, b.key.z * CHUNK)); }
    c.ready = true;
}

void World::update(vec3 focus, float viewRadius, int uploadBudget, double now) {
    ChunkKey fc = chunkOf(focus.x, focus.z);
    int R = (int)std::ceil(viewRadius / CHUNK);
    // desired chunks with LOD by distance; nearest first
    std::vector<std::pair<float, std::pair<ChunkKey, int>>> want;
    for (int dz = -R; dz <= R; dz++)
        for (int dx = -R; dx <= R; dx++) {
            ChunkKey k{fc.x + dx, fc.z + dz};
            if (k.x < 0 || k.z < 0 || k.x >= CHUNKS || k.z >= CHUNKS) continue;
            vec2 cc{(k.x + 0.5f) * CHUNK, (k.z + 0.5f) * CHUNK};
            float d = length(cc - focus.xz());
            if (d > viewRadius + CHUNK * 0.7f) continue;
            int lod = d < 56.f ? 0 : (d < 100.f ? 1 : 2);
            want.push_back({d, {k, lod}});
        }
    std::sort(want.begin(), want.end(), [](auto& a, auto& b) { return a.first < b.first; });
    {
        std::lock_guard<std::mutex> l(qmtx);
        // drop jobs that are no longer relevant
        jobs.erase(std::remove_if(jobs.begin(), jobs.end(), [&](const std::pair<ChunkKey, int>& j) {
            vec2 cc{(j.first.x + 0.5f) * CHUNK, (j.first.z + 0.5f) * CHUNK};
            bool drop = length(cc - focus.xz()) > viewRadius + CHUNK * 1.5f;
            if (drop) inFlight.erase(j.first);
            return drop;
        }), jobs.end());
        for (auto& w : want) {
            ChunkKey k = w.second.first;
            int lod = w.second.second;
            auto it = chunks.find(k);
            bool have = it != chunks.end() && it->second->ready && it->second->lod == lod;
            if (it != chunks.end()) it->second->lastUsed = now;
            if (have || inFlight.count(k)) continue;
            inFlight.insert(k);
            jobs.push_back({k, lod});
        }
    }
    qcv.notify_all();
    // Upload finished builds within a per-frame budget (avoids hitches)
    for (int n = 0; n < uploadBudget; n++) {
        std::unique_ptr<ChunkBuild> b;
        {
            std::lock_guard<std::mutex> l(qmtx);
            if (done.empty()) break;
            b = std::move(done.front()); done.pop_front();
            inFlight.erase(b->key);
        }
        vec2 cc{(b->key.x + 0.5f) * CHUNK, (b->key.z + 0.5f) * CHUNK};
        if (length(cc - focus.xz()) > viewRadius + CHUNK * 2.f) continue;  // moved away meanwhile
        uploadChunk(*b);
        chunks[b->key]->lastUsed = now;
    }
    // Unload chunks far away (memory budget)
    for (auto it = chunks.begin(); it != chunks.end();) {
        vec2 cc{(it->first.x + 0.5f) * CHUNK, (it->first.z + 0.5f) * CHUNK};
        if (length(cc - focus.xz()) > viewRadius + CHUNK * 2.5f) { it->second->destroy(); it = chunks.erase(it); }
        else ++it;
    }
}

void World::unloadAll() {
    for (auto& kv : chunks) kv.second->destroy();
    chunks.clear();
    std::lock_guard<std::mutex> l(qmtx);
    jobs.clear(); done.clear(); inFlight.clear();
}

void World::forEachVisibleChunk(const std::function<void(Chunk&)>& fn) {
    for (auto& kv : chunks) if (kv.second->ready) fn(*kv.second);
}

float World::areaReady(vec3 focus, float radius) const {
    ChunkKey fc = chunkOf(focus.x, focus.z);
    int R = (int)std::ceil(radius / CHUNK), total = 0, ready = 0;
    for (int dz = -R; dz <= R; dz++)
        for (int dx = -R; dx <= R; dx++) {
            ChunkKey k{fc.x + dx, fc.z + dz};
            if (k.x < 0 || k.z < 0 || k.x >= CHUNKS || k.z >= CHUNKS) continue;
            vec2 cc{(k.x + 0.5f) * CHUNK, (k.z + 0.5f) * CHUNK};
            if (length(cc - focus.xz()) > radius) continue;
            total++;
            auto it = chunks.find(k);
            if (it != chunks.end() && it->second->ready) ready++;
        }
    return total ? (float)ready / total : 1.f;
}

Chunk* World::chunkAt(float x, float z) {
    auto it = chunks.find(chunkOf(x, z));
    return it == chunks.end() ? nullptr : it->second.get();
}

// ---------------------------------------------------------------------------------------- collision
bool World::collide(vec3& pos, float radius, float feetY, float* stepUp) const {
    bool hit = false;
    ChunkKey k = chunkOf(pos.x, pos.z);
    for (int dz = -1; dz <= 1; dz++)
        for (int dx = -1; dx <= 1; dx++) {
            auto it = chunks.find({k.x + dx, k.z + dz});
            if (it == chunks.end() || !it->second->ready) continue;
            for (const Collider& c : it->second->colliders) {
                float topY = c.base + c.top;
                if (topY < feetY + 0.35f) continue;            // we can walk over it
                if (c.base > feetY + 1.9f) continue;            // overhead (bridge decks, arches)
                vec2 p = pos.xz();
                if (c.hx <= 0) {
                    vec2 d = p - c.c;
                    float l = length(d), rr = c.r + radius;
                    if (l < rr && l > 1e-5f) {
                        if (stepUp && topY - feetY < 0.6f) { *stepUp = std::max(*stepUp, topY); continue; }
                        vec2 np = c.c + d * (rr / l);
                        pos.x = np.x; pos.z = np.y; hit = true;
                    }
                } else {
                    vec2 l = rotate(p - c.c, -c.rot);
                    vec2 q{clampf(l.x, -c.hx, c.hx), clampf(l.y, -c.hz, c.hz)};
                    vec2 d = l - q;
                    float dl = length(d);
                    if (dl < radius) {
                        if (stepUp && topY - feetY < 0.6f) { *stepUp = std::max(*stepUp, topY); continue; }
                        vec2 push;
                        if (dl > 1e-5f) push = d * ((radius - dl) / dl);
                        else {  // inside the box: push out along the smallest axis
                            float px = c.hx - std::fabs(l.x) + radius, pz = c.hz - std::fabs(l.y) + radius;
                            push = px < pz ? vec2(sign(l.x) * px, 0) : vec2(0, sign(l.y) * pz);
                        }
                        vec2 wpush = rotate(push, c.rot);
                        pos.x += wpush.x; pos.z += wpush.y; hit = true;
                    }
                }
            }
        }
    return hit;
}

bool World::lineBlocked(vec3 a, vec3 b) const {
    vec2 A = a.xz(), B = b.xz();
    float len = length(B - A);
    int steps = std::max(1, (int)(len / 1.5f));
    for (int i = 1; i < steps; i++) {
        vec2 p = lerp(A, B, (float)i / steps);
        float y = lerpf(a.y, b.y, (float)i / steps);
        if (height(p.x, p.y) > y + 0.6f) return true;  // terrain occludes
        vec3 q{p.x, y, p.y};
        if (collide(q, 0.05f, y - 0.5f)) return true;
    }
    return false;
}

bool World::cameraBlocked(vec3 p, float margin) const {
    ChunkKey k = chunkOf(p.x, p.z);
    for (int dz = -1; dz <= 1; dz++)
        for (int dx = -1; dx <= 1; dx++) {
            auto it = chunks.find({k.x + dx, k.z + dz});
            if (it == chunks.end() || !it->second->ready) continue;
            for (const Collider& c : it->second->colliders) {
                if (c.hx <= 0 || c.top < 2.5f || std::max(c.hx, c.hz) < 1.2f) continue;
                if (p.y < c.base - 0.5f || p.y > c.base + c.top + margin) continue;
                vec2 l = rotate(p.xz() - c.c, -c.rot);
                if (std::fabs(l.x) < c.hx + margin && std::fabs(l.y) < c.hz + margin) return true;
            }
        }
    return false;
}

void World::buildFarMesh(MeshBuilder& b, int res) const {
    const float cell = WORLD_SIZE / res;
    const int ext = 10;                       // sea skirt beyond the coast
    const int n = res + 2 * ext + 1;
    std::vector<float> hs((size_t)n * n);
    std::vector<vec3> cols((size_t)n * n);
    auto inside = [](float x, float z) { return x >= 0 && z >= 0 && x <= WORLD_SIZE && z <= WORLD_SIZE; };
    for (int j = 0; j < n; j++)
        for (int i = 0; i < n; i++) {
            float x = (i - ext) * cell, z = (j - ext) * cell;
            float h = SEA_LEVEL - 14.f;
            vec3 c = hexColor(0x2e6a80);
            if (inside(x, z)) {
                // lowest of the vertex and four half-cell neighbours: the coarse surface stays under the fine one
                h = height(x, z);
                float hc = h;
                for (int k = 0; k < 4; k++) {
                    float a = k * PI * 0.5f;
                    float qx = x + std::cos(a) * cell * 0.5f, qz = z + std::sin(a) * cell * 0.5f;
                    if (inside(qx, qz)) h = std::min(h, height(qx, qz));
                }
                float slope = std::fabs(height(std::min(WORLD_SIZE, x + cell * 0.5f), z) - height(std::max(0.f, x - cell * 0.5f), z)) +
                              std::fabs(height(x, std::min(WORLD_SIZE, z + cell * 0.5f)) - height(x, std::max(0.f, z - cell * 0.5f)));
                RegionWeights rw = regionAt(x, z);
                static const uint32_t pal[R_COUNT] = {0x6b8a3e, 0x8f9c94, 0x4a3e3a, 0x3f6b3a, 0x4a5236, 0x8c8a7e, 0xc79a52, 0x5fa05a, 0xa89a52, 0x2f6a34, 0x5e7a48};
                c = vec3(0);
                for (int r = 0; r < R_COUNT; r++) c += hexColor(pal[r]) * rw.w[r];
                c = lerp(c, hexColor(0x7a756c), smoothstepf(6.f, 16.f, slope));                       // cliffs: bare rock
                float snowLine = 52.f - 22.f * rw.w[R_FROSTHEIM];
                c = lerp(c, hexColor(0xe6edf2), smoothstepf(snowLine, snowLine + 12.f, hc) * (rw.w[R_FROSTHEIM] + rw.w[R_HIGHCROWNS]));
                float e = landEdgeDistance(x, z);
                if (hc < SEA_LEVEL + 2.f && e < 400.f) c = lerp(c, hexColor(0xc8b88a), 0.7f);           // beaches
                bool lv = false, fr = false;
                float wl = waterLevel(x, z, &lv, &fr);
                if (wl > -1e8f && wl > hc - 0.2f) { h = wl - 0.4f; c = lv ? hexColor(0xd0501a) : (fr ? hexColor(0xbfd8e8) : hexColor(0x2e6a80)); }
                h -= 1.2f;
            } else {
                h = SEA_LEVEL - 0.6f;   // open sea surface
            }
            hs[(size_t)j * n + i] = h;
            cols[(size_t)j * n + i] = c;
        }
    uint32_t base = (uint32_t)b.v.size();
    for (int j = 0; j < n; j++)
        for (int i = 0; i < n; i++) {
            auto H = [&](int a, int c2) { a = std::max(0, std::min(n - 1, a)); c2 = std::max(0, std::min(n - 1, c2)); return hs[(size_t)c2 * n + a]; };
            vec3 nr = normalize(vec3(H(i - 1, j) - H(i + 1, j), 2.f * cell, H(i, j - 1) - H(i, j + 1)));
            Mat m(cols[(size_t)j * n + i], 0.95f);
            b.vert(vec3((i - ext) * cell, hs[(size_t)j * n + i], (j - ext) * cell), nr, m);
        }
    for (int j = 0; j + 1 < n; j++)
        for (int i = 0; i + 1 < n; i++) {
            uint32_t a = base + j * n + i;
            b.quad(a, a + n, a + n + 1, a + 1);
        }
}

// ---------------------------------------------------------------------------------------- map image
void World::generateMapImage() {
    int R = mapRes;
    std::vector<uint8_t> img((size_t)R * R * 4);
    float s = WORLD_SIZE / R;
    std::vector<float> hh((size_t)R * R);
    for (int j = 0; j < R; j++)
        for (int i = 0; i < R; i++) hh[(size_t)j * R + i] = height((i + 0.5f) * s, (j + 0.5f) * s);
    for (int j = 0; j < R; j++)
        for (int i = 0; i < R; i++) {
            float x = (i + 0.5f) * s, z = (j + 0.5f) * s;
            float h = hh[(size_t)j * R + i];
            RegionWeights rw = regionAt(x, z);
            static const vec3 rc[R_COUNT] = {hexColor(0x7d9a4e), hexColor(0xd9e2ea), hexColor(0x5a4a44), hexColor(0x3f7d6a), hexColor(0x5b5a4e), hexColor(0x9a9384), hexColor(0xe0b66a), hexColor(0x6fb888), hexColor(0xc8b468), hexColor(0x3f8a48), hexColor(0x7a9a62)};
            vec3 c(0);
            for (int r = 0; r < R_COUNT; r++) c += rc[r] * rw.w[r];
            float hx = hh[(size_t)j * R + std::min(R - 1, i + 1)] - h, hz = hh[(size_t)std::min(R - 1, j + 1) * R + i] - h;
            float shade = clampf(1.f - (hx + hz) * 0.08f, 0.6f, 1.35f);
            c = c * shade * (0.85f + clampf(h / 120.f, 0, 0.4f));
            bool lv, fr;
            float wl = waterLevel(x, z, &lv, &fr);
            if (wl > -1e8f && h < wl) {
                float depth = wl - h;
                c = lv ? hexColor(0xe0571a) : (fr ? hexColor(0xbfd8e8) : lerp(hexColor(0x5d9cb5), hexColor(0x1e4d68), saturate(depth / 8.f)));
            }
            if (roadInfluence(x, z) > 0.4f) c = lerp(c, hexColor(0xc2a46e), 0.75f);
            // parchment-like ink tone
            c = lerp(c, hexColor(0xd8c49a) * (c.x + c.y + c.z) / 2.2f, 0.18f);
            Color8 k = toColor8(c, 1);
            uint8_t* p = &img[((size_t)j * R + i) * 4];
            p[0] = k.r; p[1] = k.g; p[2] = k.b; p[3] = 255;
        }
    mapImage.swap(img);
    mapReady = true;
}

}  // namespace lk
