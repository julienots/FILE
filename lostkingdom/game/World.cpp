#include "World.h"
#include "Props.h"
#include "../engine/core/Platform.h"
#include <algorithm>
#include <cstring>

namespace lk {

const char* regionName(int r) {
    static const char* n[] = {"Greenhaven", "Frostheim", "Ashen Lands", "Elderwood", "Royaume des Ombres", "High Crowns"};
    return (r >= 0 && r < R_COUNT) ? n[r] : "?";
}
const char* regionSubtitle(int r) {
    static const char* n[] = {"Les plaines verdoyantes", "Le royaume gelé", "Les terres de cendre", "La forêt ancienne", "Les terres maudites", "Les cimes couronnées"};
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
    terrain.destroy(); props.destroy(); foliage.destroy(); water.destroy(); lava.destroy(); grass.destroy(); flowers.destroy();
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
};

RegionWeights World::regionAt(float x, float z) const {
    RegionWeights rw;
    // domain warp for organic borders
    float wx = x + 140.f * fbm2(x * 0.0025f, z * 0.0025f, 3, 501);
    float wz = z + 140.f * fbm2(x * 0.0025f + 31.7f, z * 0.0025f - 12.3f, 3, 502);
    float d[R_COUNT];
    for (int i = 0; i < R_COUNT; i++) d[i] = 1e9f;
    for (const Seed& s : SEEDS) {
        float dd = length(vec2(wx, wz) - s.p);
        if (dd < d[s.r]) d[s.r] = dd;
    }
    float dmin = 1e9f; int best = 0;
    for (int i = 0; i < R_COUNT; i++) if (d[i] < dmin) { dmin = d[i]; best = i; }
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

static float regionHeight(int r, float x, float z) {
    switch (r) {
    case R_GREENHAVEN:
        return 7.f + 11.f * fbm2(x * 0.0035f, z * 0.0035f, 4, 11) + 3.f * fbm2(x * 0.014f, z * 0.014f, 3, 12);
    case R_FROSTHEIM: {
        float mask = smoothstepf(-0.15f, 0.35f, fbm2(x * 0.0016f, z * 0.0016f, 3, 21));
        float ridge = ridged2(x * 0.0028f, z * 0.0028f, 5, 22);
        return 10.f + 62.f * ridge * mask + 5.f * fbm2(x * 0.01f, z * 0.01f, 3, 23);
    }
    case R_ASHEN: {
        float h = 9.f + 16.f * ridged2(x * 0.004f, z * 0.004f, 4, 31) * smoothstepf(-0.2f, 0.4f, fbm2(x * 0.002f, z * 0.002f, 2, 32)) + 3.f * fbm2(x * 0.02f, z * 0.02f, 2, 33);
        float dv = length(vec2(x, z) - VOLCANO);
        float cone = 95.f * std::pow(saturate(1.f - dv / 300.f), 1.6f);
        float crater = 32.f * (1.f - smoothstepf(40.f, 75.f, dv));
        return std::max(h, h * 0.4f + cone - crater + 4.f * fbm2(x * 0.03f, z * 0.03f, 2, 34) * saturate(1.f - dv / 300.f));
    }
    case R_ELDERWOOD:
        return 9.f + 14.f * fbm2(x * 0.0045f, z * 0.0045f, 4, 41) + 4.f * fbm2(x * 0.02f, z * 0.02f, 2, 42);
    case R_SHADOWS:
        return 1.1f + 4.5f * fbm2(x * 0.006f, z * 0.006f, 4, 51) + 1.f * fbm2(x * 0.03f, z * 0.03f, 2, 52);
    case R_HIGHCROWNS: {
        float base = 26.f + 88.f * ridged2(x * 0.0024f, z * 0.0024f, 5, 61) * smoothstepf(-0.3f, 0.3f, fbm2(x * 0.0017f, z * 0.0017f, 2, 62));
        return terrace(base, 9.f, 0.12f) + 2.f * fbm2(x * 0.02f, z * 0.02f, 2, 63);
    }
    }
    return 0;
}

float World::baseHeight(float x, float z, const RegionWeights& rw) const {
    float h = 0;
    for (int i = 0; i < R_COUNT; i++) if (rw.w[i] > 0.002f) h += rw.w[i] * regionHeight(i, x, z);
    // coastline: the kingdom is an island; the sea forms a natural boundary
    float e = std::min(std::min(x, z), std::min(WORLD_SIZE - x, WORLD_SIZE - z));
    e += 40.f * fbm2(x * 0.004f, z * 0.004f, 3, 71);
    float coast = smoothstepf(40.f, 230.f, e);
    h = lerpf(-14.f, h, coast);
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

    // Lakes
    lakes.push_back({{1700, 1390}, 85, 0, false, false});      // Lac Miroir (level computed below)
    lakes.push_back({{1180, 360}, 120, 0, false, true});       // frozen lake (boss arena on ice)
    lakes.push_back({VOLCANO, 48, 0, true, false});            // crater lava lake
    lakes.push_back({{2560, 2320}, 30, 0, true, false});       // lava pool
    lakes.push_back({{2820, 1900}, 26, 0, true, false});
    lakes.push_back({{420, 1800}, 38, 0, false, false});       // magic spring
    lakes.push_back({{760, 420}, 40, 0, false, true});
    // Rivers (from the mountains to the southern sea)
    River r1; r1.width = 14;
    r1.pts = {{1080, 690}, {1220, 980}, {1390, 1250}, {1430, 1480}, {1370, 1760}, {1260, 2050}, {1130, 2330}, {990, 2700}, {930, 3060}};
    River r2; r2.width = 10;
    r2.pts = {{2280, 820}, {2120, 1040}, {1930, 1240}, {1760, 1360}};
    River r3; r3.width = 9;
    r3.pts = {{380, 1000}, {470, 1250}, {430, 1500}, {400, 1780}};
    rivers = {r1, r2, r3};

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
        switch (p.type) {
        case PoiType::Village: case PoiType::Mill: case PoiType::Monastery: {
            S(S_WELL, p.pos, 0, pi);
            int house = reg == R_FROSTHEIM ? S_LONGHOUSE : (reg == R_SHADOWS ? S_STILT_HUT : (reg == R_HIGHCROWNS || reg == R_ASHEN ? S_STONEHOUSE : S_HOUSE));
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
            break;
        }
        case PoiType::Castle: {
            int n = 8;
            for (int i = 0; i < n; i++) {
                float a = i * TAU / n;
                vec2 t = p.pos + vec2(std::cos(a), std::sin(a)) * p.radius * 0.8f;
                S(S_CASTLE_TOWER, t, a, pi, 1.f, reg == R_SHADOWS ? 1 : (reg == R_FROSTHEIM ? 2 : 0));
                float a2 = (i + 0.5f) * TAU / n;
                vec2 m = p.pos + vec2(std::cos(a2), std::sin(a2)) * p.radius * 0.8f * std::cos(PI / n);
                if (i == 1) S(S_GATE, m, -a2 + PI / 2, pi, 1.f, reg == R_SHADOWS ? 1 : 0);
                else S(S_CASTLE_WALL, m, -a2 + PI / 2, pi, 2.f * p.radius * 0.8f * std::sin(PI / n) / 10.f, reg == R_SHADOWS ? 1 : (reg == R_FROSTHEIM ? 2 : 0));
            }
            if (p.boss < 0) S(S_KEEP, p.pos + vec2(0, -p.radius * 0.25f), 0, pi, 1.f, reg == R_FROSTHEIM ? 2 : 0);
            else S(S_STATUE, p.pos + vec2(0, -p.radius * 0.45f), 0, pi, 1.6f);
            for (int i = 0; i < 4; i++) S(S_LANTERN, p.pos + vec2(i < 2 ? -12.f : 12.f, i % 2 ? -12.f : 12.f), 0, pi);
            break;
        }
        case PoiType::Ruins: case PoiType::ElvenRuins: {
            for (int i = 0; i < 7; i++) {
                float a = rng.range(0, TAU), d = rng.range(6, p.radius * 0.8f);
                vec2 q = p.pos + vec2(std::cos(a), std::sin(a)) * d;
                if (p.type == PoiType::ElvenRuins) S(i % 3 == 0 ? S_ELVEN_ARCH : (i % 3 == 1 ? S_OBELISK : S_RUIN_WALL), q, rng.range(0, TAU), pi, rng.range(0.8f, 1.2f), 1);
                else S(i % 2 ? S_RUIN_WALL : S_OBELISK, q, rng.range(0, TAU), pi, rng.range(0.8f, 1.3f), 0);
            }
            S(S_ALTAR, p.pos, 0, pi);
            break;
        }
        case PoiType::Cave: S(S_CAVE, p.pos, rng.range(0, TAU), pi, p.id == std::string("abyss") ? 1.6f : 1.f, p.id == std::string("abyss") ? 1 : 0); break;
        case PoiType::Mine: S(S_MINE, p.pos, rng.range(0, TAU), pi); S(S_CAMPFIRE, p.pos + vec2(8, 6), 0, pi); S(S_TENT, p.pos + vec2(-10, 8), 0.5f, pi); break;
        case PoiType::Tower: S(S_TOWER, p.pos, 0, pi, 1.6f, 1); S(S_CAMPFIRE, p.pos + vec2(9, 7), 0, pi); break;
        case PoiType::Waystone: S(S_WAYSTONE, p.pos, 0, pi); break;
        case PoiType::Camp: for (int i = 0; i < 4; i++) S(S_TENT, p.pos + vec2(std::cos(i * 1.6f), std::sin(i * 1.6f)) * 12.f, i * 1.6f + PI, pi); S(S_CAMPFIRE, p.pos, 0, pi); for (int i = 0; i < 8; i++) { float a = i * TAU / 8; S(S_PALISADE, p.pos + vec2(std::cos(a), std::sin(a)) * p.radius * 0.95f, -a, pi); } break;
        case PoiType::Graveyard: S(S_CRYPT, p.pos, 0, pi); S(S_CHAPEL, p.pos + vec2(-26, -20), 0.3f, pi, 0.9f, 1); for (int i = 0; i < 4; i++) S(S_LANTERN, p.pos + vec2(i < 2 ? -9.f : 9.f, i % 2 ? 9.f : 4.f), 0, pi); break;
        case PoiType::BossArena: {
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
    indexPlan();
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
            h = lerpf(std::max(h, l.level + 0.35f), h, t);
            h = std::max(h, lerpf(l.level + 0.35f, -100.f, t));
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
        if (bestD < rw2 + 8 && ri < 0.5f) {
            float w = (1.f - smoothstepf(rw2 * 0.5f, rw2 * 0.5f + 8.f, bestD)) * (1.f - ri * 2.f);
            h = lerpf(h, std::max(target, h - 1.5f), w * 0.85f);
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
    float e = std::min(std::min(x, z), std::min(WORLD_SIZE - x, WORLD_SIZE - z));
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
                if (s.kind != S_BRIDGE) continue;
                vec2 l = rotate(vec2(x, z) - s.pos, s.rot);  // into bridge space (z along the bridge)
                float halfLen = 8.f * s.scale + 4.f;
                if (std::fabs(l.x) < 2.6f && std::fabs(l.y) < halfLen) {
                    float deck = s.variant / 100.f + 2.0f;
                    float arch = 0.8f * (1.f - (l.y / halfLen) * (l.y / halfLen));
                    float endH = height(s.pos.x + std::sin(s.rot) * l.y, s.pos.y + std::cos(s.rot) * l.y);
                    float d = std::max(endH, deck + arch);
                    if (std::fabs(l.y) > halfLen - 4.f) d = lerpf(d, endH, (std::fabs(l.y) - (halfLen - 4.f)) / 4.f);
                    h = std::max(h, d);
                }
            }
        }
    return h;
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
    default: return 0.12f + smoothstepf(0.5f, 0.75f, n) * 0.4f;
    }
}

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
                }
            }
            // slopes become rock, altitude snow anywhere
            wRock += smoothstepf(0.22f, 0.4f, slope) * 0.8f;
            wSnow = std::max(wSnow, smoothstepf(85.f, 110.f, y));
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
            v.b0 = q(wMud); v.b1 = q(wCob); v.w0 = q(wAsh); v.ex = 0;
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

    // ---- vegetation & props scatter (deterministic per chunk)
    Rng rng(hash2i(k.x, k.z, 4242));
    const float cell = 4.f;
    const int CN = (int)(CHUNK / cell);
    auto blocked = [&](float x, float z, float margin) {
        if (roadInfluence(x, z) > 0.05f) return true;
        if (ci >= 0)
            for (int pi : chunkPois[ci]) {
                const Poi& p = pois[pi];
                float rr = p.type == PoiType::Waystone ? 8.f : (p.type == PoiType::Cave || p.type == PoiType::Mine ? p.radius + 4 : p.radius * 0.95f);
                if (length(vec2(x, z) - p.pos) < rr + margin) return true;
            }
        for (auto& c : out.colliders) {
            float rr = c.r > 0 ? c.r : std::max(c.hx, c.hz) * 1.2f;
            if (length(vec2(x, z) - c.c) < rr + margin) return true;
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
        };
        static const int attempts[R_COUNT] = {16, 8, 6, 14, 10, 12};
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
    int nNodes = rng.irange(1, 3);
    for (int n = 0; n < nNodes; n++) {
        float x = x0 + rng.range(2, CHUNK - 2), z = z0 + rng.range(2, CHUNK - 2);
        if (blocked(x, z, 1.5f)) continue;
        float wl = waterLevel(x, z);
        float y = height(x, z);
        if (wl > -1e8f && y < wl + 0.3f) continue;
        if (normal(x, z).y < 0.82f) continue;
        int r = regionId(x, z);
        float p = rng.f();
        NodeType t = NodeType::Rock;
        int tier = 0;
        switch (r) {
        case R_GREENHAVEN: t = p < 0.3f ? NodeType::Tree : p < 0.5f ? NodeType::Herb : p < 0.62f ? NodeType::Berries : p < 0.75f ? NodeType::Rock : p < 0.88f ? NodeType::CopperOre : NodeType::IronOre; break;
        case R_FROSTHEIM: t = p < 0.25f ? NodeType::Tree : p < 0.45f ? NodeType::Rock : p < 0.7f ? NodeType::IronOre : p < 0.85f ? NodeType::Crystal : NodeType::Herb; tier = 1; break;
        case R_ASHEN: t = p < 0.25f ? NodeType::Rock : p < 0.55f ? NodeType::CopperOre : p < 0.8f ? NodeType::RareOre : NodeType::Crystal; tier = 2; break;
        case R_ELDERWOOD: t = p < 0.3f ? NodeType::Herb : p < 0.5f ? NodeType::Mushroom : p < 0.7f ? NodeType::Crystal : p < 0.85f ? NodeType::Tree : NodeType::Berries; tier = 1; break;
        case R_SHADOWS: t = p < 0.35f ? NodeType::Mushroom : p < 0.6f ? NodeType::Herb : p < 0.8f ? NodeType::Tree : NodeType::IronOre; tier = 1; break;
        default: t = p < 0.3f ? NodeType::IronOre : p < 0.5f ? NodeType::RareOre : p < 0.7f ? NodeType::Rock : p < 0.85f ? NodeType::Tree : NodeType::Crystal; tier = 2; break;
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
    if (lod == 0) {
        Rng g(hash2i(k.x, k.z, 777));
        float spacing = 0.9f / std::sqrt(std::max(0.05f, grassDensity));
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
                float gd = r == R_ASHEN ? 0.12f : (r == R_FROSTHEIM ? 0.35f : (r == R_HIGHCROWNS ? 0.6f : 1.f));
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
                d.x = x; d.y = y - 0.03f; d.z = z; d.rotY = g.range(0, TAU); d.scale = g.range(0.6f, 1.15f) * (0.7f + hn * 0.6f);
                Color8 c = toColor8(col * 0.5f, 1);
                d.r = c.r; d.g = c.g; d.b = c.b; d.a = 255;
                bool flower = g.f() < (r == R_GREENHAVEN ? 0.04f : (r == R_ELDERWOOD ? 0.05f : 0.012f)) * (0.5f + hn);
                if (flower) {
                    static const uint32_t fc[] = {0xe8d24a, 0xd94b5f, 0xf2f0e6, 0x8f6fd8, 0xf28a3a, 0x6fb8f2};
                    Color8 f = toColor8(hexColor(fc[g.irange(0, 5)]) * 0.5f, 1);
                    d.r = f.r; d.g = f.g; d.b = f.b;
                    out.flowers.push_back(d);
                } else out.grass.push_back(d);
            }
    }
}

// ---------------------------------------------------------------------------------------- streaming
void World::startWorkers(int n) {
    if (running) return;
    running = true;
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
    Chunk& c = *slot;
    c.destroy();
    c.lod = b.lod;
    if (!b.terrain.v.empty()) b.terrain.upload(c.terrain);
    if (!b.props.v.empty()) b.props.upload(c.props);
    if (!b.foliage.v.empty()) b.foliage.upload(c.foliage);
    if (!b.water.v.empty()) b.water.upload(c.water);
    if (!b.lava.v.empty()) b.lava.upload(c.lava);
    if (!b.grass.empty() || !b.flowers.empty()) {
        // grass blade / flower templates are shared meshes; per-chunk VAO with instance buffer
        static MeshBuilder blade, flower;
        if (blade.v.empty()) {
            // a clump of 7 blades (one instance) - denser look for fewer instances
            Mat root(hexColor(0x3a5f22), 0.9f, 0, 0, 0.f), tip(hexColor(0x9cc25a), 0.7f, 0, 0, 1.f);
            Rng br(5);
            for (int i = 0; i < 7; i++) {
                float a = i * TAU / 7 + br.range(-0.3f, 0.3f);
                vec3 side{std::cos(a), 0, std::sin(a)};
                vec2 off = br.inDisk() * 0.28f;
                vec3 base{off.x, 0, off.y};
                vec3 lean = vec3(off.x, 0, off.y) * 0.6f + vec3(-side.z, 0, side.x) * 0.1f;
                blade.blade(base, base + vec3(0, br.range(0.45f, 0.75f), 0) + lean, 0.12f, side, root, tip);
            }
            Mat stem(hexColor(0x3f6a2a), 0.9f, 0, 0, 0.f), petal(vec3(1.f), 0.6f, 0, 0.05f, 1.f);
            flower.blade({0, 0, 0}, {0, 0.42f, 0}, 0.05f, {1, 0, 0}, stem, stem.withWind(0.8f));
            for (int i = 0; i < 5; i++) {
                float a = i * TAU / 5;
                vec3 d{std::cos(a), 0.35f, std::sin(a)};
                flower.blade({0, 0.42f, 0}, vec3(0, 0.42f, 0) + d * 0.12f, 0.09f, vec3(-d.z, 0, d.x), petal, petal);
            }
            for (auto& v : blade.v) {
                v.r = v.g = v.b = (uint8_t)clampf(150 + v.py * 160, 0, 255);  // tinted per instance (x2 in shader)
                // ground-like normals so blades shade like the terrain they grow on (no dark "hairs")
                vec3 n = normalize(unpackNormal(v) * 0.25f + vec3(0, 1, 0));
                v.nx = (int8_t)(n.x * 127); v.ny = (int8_t)(n.y * 127); v.nz = (int8_t)(n.z * 127);
                v.a = (uint8_t)clampf(140 + v.py * 200, 0, 255);  // AO darker at the root
            }
            for (auto& v : flower.v) { if (v.py < 0.4f) { v.r = 70; v.g = 120; v.b = 50; } vec3 n = normalize(unpackNormal(v) * 0.4f + vec3(0, 1, 0)); v.nx = (int8_t)(n.x * 127); v.ny = (int8_t)(n.y * 127); v.nz = (int8_t)(n.z * 127); }
        }
        if (!b.grass.empty()) { blade.upload(c.grass); c.grass.setInstances(b.grass); }
        if (!b.flowers.empty()) { flower.upload(c.flowers); c.flowers.setInstances(b.flowers); }
    }
    c.colliders = std::move(b.colliders);
    c.nodes = std::move(b.nodes);
    c.lights = std::move(b.lights);
    c.emitters = std::move(b.emitters);
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
            static const vec3 rc[R_COUNT] = {hexColor(0x7d9a4e), hexColor(0xd9e2ea), hexColor(0x5a4a44), hexColor(0x3f7d6a), hexColor(0x5b5a4e), hexColor(0x9a9384)};
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
