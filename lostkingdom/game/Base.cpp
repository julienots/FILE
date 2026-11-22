// LOST KINGDOM — 1.9.2 the player's domain: found it anywhere in the wild, build houses, fields, a forge, a
// watchtower, palisades and a livestock pen, recruit inhabitants and soldiers, raise pigs and horses.
// The domain is a real settlement of the world: a POI (levelled ground, on the map, safe at night once it has a
// watchtower) and structures built by the chunk builder like any village (meshes, colliders, lights, smoke).
#include "Game.h"
#include "../engine/core/Platform.h"
#include <algorithm>
#include <sstream>

namespace lk {

static const float BASE_RADIUS = 44.f;     // levelled ground of the domain (fully flat up to 33 m: every building of the 34 m circle)
// 3.0.0 the domain grows: village (44 m), town (62 m), castle (80 m); buildings fit 10 m inside
float Game::baseRadius() const { static const float R[3] = {44.f, 62.f, 80.f}; return R[std::min(2, std::max(0, base.level))]; }
float Game::baseBuildRadius() const { return baseRadius() - 10.f; }
static bool wallLike(const std::string& id) { return id == "palisade" || id == "stonewall" || id == "castletower" || id == "gatehouse"; }

const std::vector<BaseBuildDef>& baseBuildings() {
    static std::vector<BaseBuildDef> d;
    static std::once_flag once;
    std::call_once(once, [] {
        auto B = [&](const char* id, const char* name, int kind, int vars, std::vector<std::pair<std::string, int>> cost, float r, const char* desc) -> BaseBuildDef& {
            BaseBuildDef b; b.id = id; b.name = name; b.kind = kind; b.variants = vars; b.cost = cost; b.radius = r; b.desc = desc;
            d.push_back(b); return d.back();
        };
        B("house", "Maison", S_HOUSE, 3, {{"wood", 30}, {"stone", 10}}, 5.2f, "Loge 3 habitants.").housing = 3;
        B("tent", "Tente", S_TENT, 2, {{"wood", 8}, {"leather", 2}}, 3.4f, "Loge 1 habitant. Bon marché.").housing = 1;
        B("field", "Champ", S_FIELD, 3, {{"wood", 10}}, 10.f, "2 pains par jour, si un habitant le cultive.");
        B("well", "Puits", S_WELL, 1, {{"stone", 15}}, 1.8f, "+25 % de production.").unique = true;
        B("forge", "Forge", S_FORGE, 1, {{"wood", 20}, {"stone", 25}, {"iron_bar", 3}}, 3.6f, "Forger et améliorer l'équipement au domaine.").unique = true;
        B("tower", "Tour de guet", S_TOWER, 2, {{"stone", 40}, {"wood", 20}}, 3.6f, "+3 soldats. Avec une tour, le domaine est sûr la nuit.").guards = 3;
        B("palisade", "Palissade", S_PALISADE, 1, {{"wood", 6}}, 1.0f, "Un pan de mur de 4 m.");
        B("pen", "Enclos", S_PEN, 1, {{"wood", 24}}, 7.2f, "Élevage : 4 cochons et 2 chevaux par enclos.");
        B("stall", "Étal de marché", S_STALL, 2, {{"wood", 12}}, 2.2f, "15 pièces d'or par jour, si un habitant le tient.");
        B("inn", "Grande maison", S_INN, 1, {{"wood", 60}, {"stone", 30}}, 6.8f, "Loge 6 habitants.").housing = 6;
        // 3.0.0 the castle: ramparts, towers, a gatehouse, the keep, barracks, a chapel, the domain's own dungeon
        B("stonewall", "Muraille de pierre", S_CASTLE_WALL, 1, {{"stone", 30}}, 5.f, "Un pan de rempart de 10 m. Alignez-les entre des tours.");
        B("castletower", "Tour de rempart", S_CASTLE_TOWER, 1, {{"stone", 50}, {"wood", 10}}, 3.6f, "+2 soldats. À poser aux angles des murailles.").guards = 2;
        B("gatehouse", "Porte fortifiée", S_GATE, 1, {{"stone", 40}, {"wood", 20}, {"iron_bar", 4}}, 5.2f, "L'entrée du château, entre deux pans de muraille.");
        { auto& k = B("keep", "Donjon", S_KEEP, 1, {{"stone", 120}, {"wood", 60}, {"iron_bar", 10}}, 9.5f, "La tour maîtresse de votre château : +4 soldats, loge 4 habitants. Il faut un domaine agrandi."); k.unique = true; k.guards = 4; k.housing = 4; }
        B("barracks", "Caserne", S_LONGHOUSE, 2, {{"wood", 50}, {"stone", 20}}, 6.8f, "+5 soldats. Permet de former des chevaliers.").guards = 5;
        { auto& c = B("chapel", "Chapelle", S_CHAPEL, 1, {{"stone", 40}, {"wood", 20}}, 6.2f, "Chaque matin, deux soldats tombés au combat reviennent dans vos rangs."); c.unique = true; }
        { auto& c = B("crypt", "Donjon du domaine", S_CRYPT, 1, {{"stone", 80}, {"iron_bar", 6}, {"crystal", 4}}, 4.8f, "Votre propre donjon sous le domaine : des salles, des monstres et un trésor, au niveau que vous choisissez."); c.unique = true; }
        B("windmill", "Moulin", S_WINDMILL, 1, {{"wood", 40}, {"stone", 10}}, 4.f, "+50 % de pain récolté dans les champs.").unique = true;
        B("statue", "Statue", S_STATUE, 1, {{"stone", 25}}, 1.8f, "Un héros de pierre sur la place.");
        B("lantern", "Lanterne", S_LANTERN, 1, {{"iron_bar", 1}, {"wood", 2}}, 0.8f, "Éclaire les rues la nuit.");
    });
    return d;
}
int baseBuildIndex(const std::string& id) { auto& d = baseBuildings(); for (int i = 0; i < (int)d.size(); i++) if (d[i].id == id) return i; return -1; }

int Game::baseCount(const std::string& id) const { int i = baseBuildIndex(id), n = 0; for (auto& b : base.built) if (b.def == i) n++; return n; }
int Game::baseHousing() const { int n = 0; for (auto& b : base.built) n += baseBuildings()[b.def].housing; return n; }
int Game::baseGuardCap() const { int n = 0; for (auto& b : base.built) n += baseBuildings()[b.def].guards; return n + 4 * conqueredForts(); }   // 3.0.0 + 4 per conquered fortress
bool Game::baseCanAfford(const BaseBuildDef& d) const { for (auto& c : d.cost) if (countItem(c.first) < c.second) return false; return true; }

// ------------------------------------------------------------------------------------------ founding
bool Game::canFoundBase(std::string& why) {
    if (base.founded) { why = "Vous avez déjà un domaine."; return false; }
    if (dungeon.active || player == NullEntity) { why = "Impossible ici."; return false; }
    Character& c = pc();
    vec2 at = c.pos.xz() + vec2(std::sin(c.yaw), std::cos(c.yaw)) * 6.f;
    for (const Poi& p : world.pois) if (length(p.pos - at) < p.radius + BASE_RADIUS + 40.f) { why = "Trop près de « " + p.name + " »."; return false; }
    if (world.roadInfluence(at.x, at.y) > 0.3f) { why = "Pas sur une route."; return false; }
    float lo = 1e9f, hi = -1e9f;
    for (int i = 0; i < 16; i++) {
        float a = i * TAU / 16;
        for (float r : {8.f, 18.f, 28.f}) {
            vec2 p = at + vec2(std::cos(a), std::sin(a)) * r;
            float h = world.height(p.x, p.y), wl = world.waterLevel(p.x, p.y);
            if (wl > -1e8f && h < wl + 0.5f) { why = "Trop près de l'eau."; return false; }
            lo = std::min(lo, h); hi = std::max(hi, h);
        }
    }
    if (hi - lo > 16.f) { why = "Le terrain est trop pentu."; return false; }
    if (countItem("wood") < 10) { why = "Il faut 10 bois pour l'étendard."; return false; }
    return true;
}

void Game::foundBase() {
    std::string why;
    if (!canFoundBase(why)) { notify(why, vec3(1.f, 0.7f, 0.5f), "i_village"); return; }
    removeItem("wood", 10);
    Character& c = pc();
    base = PlayerBase();
    base.founded = true;
    base.center = c.pos.xz() + vec2(std::sin(c.yaw), std::cos(c.yaw)) * 6.f;
    base.lastDay = day;
    applyBaseToWorld();
    discover("player_base");
    respawnPos = vec3(base.center.x + 3.f, world.groundHeight(base.center.x + 3.f, base.center.y), base.center.y);
    particles.magicBurst(vec3(base.center.x, c.pos.y + 1.5f, base.center.y), vec3(1.f, 0.8f, 0.4f), 2.f, 50);
    play("quest", 0.9f);
    notify("Domaine fondé ! Construisez depuis le Journal, onglet Domaine.", vec3(1.f, 0.85f, 0.4f), "i_village");
    unlockLore("domain");
}

// adds the domain (POI + banner + buildings) to the world; used when founding and after loading a save
void Game::applyBaseToWorld() {
    if (!base.founded) return;
    Poi p; p.id = "player_base"; p.name = base.name; p.type = PoiType::Village; p.pos = base.center; p.radius = baseRadius();
    base.poi = world.addRuntimePoi(p);
    Structure s; s.kind = S_BANNER; s.pos = base.center; s.rot = 0; s.region = world.pois[base.poi].region; s.poi = base.poi;
    world.addRuntimeStructure(s);
    for (const BaseBuilt& b : base.built) {
        Structure t; t.kind = baseBuildings()[b.def].kind; t.pos = b.pos; t.rot = b.rot; t.variant = b.variant; t.region = s.region; t.poi = base.poi;
        world.addRuntimeStructure(t);
    }
}

void Game::clearBase() {
    world.resetRuntime();
    base = PlayerBase();
    soldierEnts.clear(); livestockEnts.clear();
    buildMode = false; buildGhost.reset(); buildGhostModels.clear();
}

// 3.0.0 the domain grows: village -> town (62 m) -> castle (80 m). The levelled ground and the building circle
// grow with it; the neighbours (villages, roads...) must leave room.
bool Game::upgradeBase(std::string& why, bool doIt) {
    if (!base.founded) { why = "Pas de domaine."; return false; }
    if (base.level >= 2) { why = "Votre domaine est déjà un château."; return false; }
    int next = base.level + 1;
    static const float R[3] = {44.f, 62.f, 80.f};
    int g = next == 1 ? 1500 : 4000, st = next == 1 ? 100 : 250, wd = next == 1 ? 80 : 150, ib = next == 1 ? 0 : 12;
    for (const Poi& p : world.pois) if (p.id != "player_base" && length(p.pos - base.center) < p.radius + R[next] + 30.f) { why = "Trop près de « " + p.name + " » pour s'agrandir."; return false; }
    for (int i = 0; i < 24; i++) {
        float a = i * TAU / 24; vec2 q = base.center + vec2(std::cos(a), std::sin(a)) * R[next];
        float h = world.height(q.x, q.y), wl = world.waterLevel(q.x, q.y);
        if (wl > -1e8f && h < wl + 0.5f) { why = "L'eau est trop proche pour s'agrandir."; return false; }
        if (world.roadInfluence(q.x, q.y) > 0.5f) { why = "Une route passe trop près."; return false; }
    }
    if (gold < g || countItem("stone") < st || countItem("wood") < wd || countItem("iron_bar") < ib) {
        why = "Il faut " + std::to_string(g) + " or, " + std::to_string(st) + " pierre, " + std::to_string(wd) + " bois" + (ib ? ", " + std::to_string(ib) + " lingots de fer" : "") + ".";
        return false;
    }
    if (!doIt) return true;
    gold -= g; removeItem("stone", st); removeItem("wood", wd); if (ib) removeItem("iron_bar", ib);
    base.level = next;
    world.resetRuntime();
    applyBaseToWorld();
    world.refreshArea(base.center, R[next] + 40.f);
    refreshBaseNpcs();
    for (Entity e : soldierEnts) reg.destroyLater(e);
    soldierEnts.clear();
    particles.magicBurst(vec3(base.center.x, world.pois[base.poi].height + 2.f, base.center.y), vec3(1.f, 0.85f, 0.45f), 4.f, 90);
    play("quest", 1.f);
    notify(next == 1 ? "Votre domaine devient un bourg : plus de place pour bâtir, et le donjon (tour maîtresse) se débloque."
                     : "Votre domaine devient un château ! Bâtissez murailles, tours et porte fortifiée tout autour.", vec3(1.f, 0.85f, 0.4f), "i_castle");
    return true;
}

// ------------------------------------------------------------------------------------------ building
void Game::startBuilding(int def) {
    if (!base.founded || def < 0) return;
    buildDef = def; buildMode = true; buildRot = pc().yaw + PI;   // entrance towards the hero
    buildVariant = (int)(base.built.size() * 7 + def) % std::max(1, baseBuildings()[def].variants);
    buildGhostKey = vec3(-1);
    menu = Menu::None;
    camDistTarget = std::max(camDistTarget, 16.f + baseBuildings()[def].radius); camPitch = std::max(camPitch, 0.7f);   // see the whole footprint
    toastHint = "Marchez pour placer le bâtiment, puis « Construire ».";
}

void Game::updateBuildMode() {
    if (!buildMode || buildDef < 0) return;
    const BaseBuildDef& d = baseBuildings()[buildDef];
    Character& c = pc();
    vec2 fwd(std::sin(c.yaw), std::cos(c.yaw));
    buildPos = c.pos.xz() + fwd * (d.radius + 2.2f);
    buildOk = true; buildWhy.clear();
    if (length(buildPos - base.center) + d.radius > baseBuildRadius()) { buildOk = false; buildWhy = "Hors du domaine (approchez de l'étendard)."; }
    else if (length(buildPos - base.center) < d.radius + 2.2f) { buildOk = false; buildWhy = "Trop près de l'étendard."; }
    for (const BaseBuilt& b : base.built) {
        const BaseBuildDef& o = baseBuildings()[b.def];
        float overlap = (d.id == "palisade" && o.id == "palisade") ? 0.6f : 0.f;
        if (wallLike(d.id) && wallLike(o.id)) overlap = std::min(d.radius, o.radius) * 1.4f;   // 3.0.0 ramparts join their towers
        if (length(b.pos - buildPos) < d.radius + o.radius - overlap) { buildOk = false; buildWhy = "Trop près : " + o.name + "."; break; }
    }
    if (buildOk && d.unique && baseCount(d.id) > 0) { buildOk = false; buildWhy = "Déjà construit."; }
    if (buildOk && d.id == "keep" && base.level < 1) { buildOk = false; buildWhy = "Agrandissez d'abord le domaine (bourg)."; }
    if (buildOk && !baseCanAfford(d)) { buildOk = false; buildWhy = "Ressources insuffisantes."; }
}

bool Game::placeBuilding() {
    updateBuildMode();
    if (!buildMode || !buildOk) { if (!buildWhy.empty()) notify(buildWhy, vec3(1.f, 0.6f, 0.5f), "i_lock"); return false; }
    const BaseBuildDef& d = baseBuildings()[buildDef];
    for (auto& c : d.cost) removeItem(c.first, c.second);
    BaseBuilt b{buildDef, buildPos, buildRot, buildVariant};
    base.built.push_back(b);
    Structure t; t.kind = d.kind; t.pos = b.pos; t.rot = b.rot; t.variant = b.variant; t.region = world.pois[base.poi].region; t.poi = base.poi;
    world.addRuntimeStructure(t);
    vec3 p(b.pos.x, world.pois[base.poi].height, b.pos.y);
    particles.dust(p, 2.5f); particles.magicBurst(p + vec3(0, 1, 0), vec3(1.f, 0.85f, 0.5f), 1.2f, 24);
    play("craft", 0.9f); play("hit_wood", 0.6f, 0.8f);
    notify(d.name + " construite.", vec3(0.8f, 1.f, 0.7f), "i_village");
    buildMode = false; buildGhost.reset(); buildGhostModels.clear();
    if (d.housing > 0 || d.id == "pen") refreshBaseNpcs();
    return true;
}

void Game::renderBuildGhost() {
    if (!buildMode || buildDef < 0 || !base.founded) return;
    const BaseBuildDef& d = baseBuildings()[buildDef];
    vec3 key(std::round(buildPos.x * 2) / 2, std::round(buildPos.y * 2) / 2, std::round(buildRot * 8) / 8);
    if (!buildGhost || key.x != buildGhostKey.x || key.y != buildGhostKey.y || key.z != buildGhostKey.z) {
        buildGhostKey = key;
        ChunkBuild cb;
        Structure s; s.kind = d.kind; s.pos = vec2(key.x, key.y); s.rot = buildRot; s.variant = buildVariant; s.region = world.pois[base.poi].region; s.poi = base.poi;
        buildStructure(cb, s, world);
        cb.props.append(cb.foliage);
        buildGhost = std::make_shared<Mesh>();
        if (!cb.props.v.empty()) buildGhost->upload(cb.props.v, cb.props.idx);
        buildGhostModels = cb.models;
    }
    float pulse = 0.75f + 0.25f * std::sin(realTime * 5.f);
    vec4 tint = buildOk ? vec4(0.55f, 1.2f, 0.6f, 0) : vec4(1.3f, 0.45f, 0.4f, 0);
    if (buildGhost && buildGhost->valid()) {
        DrawItem it; it.mesh = buildGhost.get(); it.flags = 0; it.tint = tint; it.fx = vec4(0, 0, 0.8f, pulse); it.castShadow = false;
        it.outline = true; it.outlineColor = buildOk ? vec4(0.5f, 1.f, 0.5f, 1) : vec4(1.f, 0.4f, 0.3f, 1); it.outlineWidth = 0.04f;
        renderer.submit(it);
    }
    for (const auto& [mi, xf] : buildGhostModels) {
        PropModel* P = propModel(mi);
        if (!P || !P->mesh || !P->mesh->valid()) continue;
        DrawItem bi; bi.mesh = P->mesh.get(); bi.model = xf; bi.flags = LIT_TEXTURED; bi.albedo = P->tex.id; bi.tint = tint; bi.castShadow = false;
        renderer.submit(bi);
    }
}

// ------------------------------------------------------------------------------------------ life of the domain
void Game::refreshBaseNpcs() {
    if (base.poi < 0) return;
    auto it = std::find(villageNpcsSpawned.begin(), villageNpcsSpawned.end(), base.poi);
    if (it != villageNpcsSpawned.end()) villageNpcsSpawned.erase(it);
    reg.each<NpcBrain>([&](Entity e, NpcBrain& nb) { if (nb.village == base.poi) reg.destroyLater(e); });
    for (Entity e : livestockEnts) if (reg.valid(e)) reg.destroyLater(e);
    livestockEnts.clear();
}

void Game::baseNewDay() {
    auto add = [&](const std::string& id, int n) {
        if (n <= 0) return;
        for (auto& s : base.storage) if (s.first == id) { s.second = std::min(999, s.second + n); return; }
        base.storage.push_back({id, n});
    };
    float mult = baseCount("well") > 0 ? 1.25f : 1.f;
    int workers = base.residents;
    int fields = std::min(baseCount("field"), workers);
    add("bread", (int)std::round(fields * 2 * mult * (baseCount("windmill") > 0 ? 1.5f : 1.f)));   // 3.0.0 the mill
    if (baseCount("chapel") > 0 && base.fallen > 0) {   // 3.0.0 the chapel: the fallen come back (if there is room in the ranks)
        int room = baseGuardCap() - armySize();
        int n = std::min(std::min(2, base.fallen), std::max(0, room));
        if (n > 0) { base.soldiers += n; base.fallen -= n; notify(std::to_string(n) + (n > 1 ? " soldats soignés à la chapelle reprennent leur poste." : " soldat soigné à la chapelle reprend son poste."), vec3(0.9f, 0.95f, 1.f), "i_attack"); }
    }
    workers -= fields;
    int stalls = std::min(baseCount("stall"), std::max(0, workers));
    int coins = (int)std::round(stalls * 15 * mult);
    add("meat", base.pigs);
    add("leather", base.pigs / 2);
    if (base.horses > 0) {
        base.foalDays++;
        if (base.foalDays >= 3) {
            base.foalDays = 0;
            if (std::find(ownedMounts.begin(), ownedMounts.end(), "horse") == ownedMounts.end()) { ownedMounts.push_back("horse"); notify("Un poulain du domaine est prêt : vous avez un cheval !", vec3(1.f, 0.9f, 0.6f), "i_mount"); }
            else if (base.horses < baseCount("pen") * 2) { base.horses++; refreshBaseNpcs(); notify("Un poulain est né au domaine.", vec3(1.f, 0.9f, 0.6f), "i_mount"); }
        }
    }
    if (coins > 0) { gold += coins; }
    if (!base.storage.empty() || coins > 0) notify("Le domaine a produit des ressources (onglet Domaine).", vec3(0.9f, 0.95f, 0.7f), "i_village");
}

void Game::updateBase(float dt) {
    if (!base.founded || player == NullEntity || base.poi < 0) return;
    if (day != base.lastDay) { int n = std::min(3, day - base.lastDay); base.lastDay = day; for (int i = 0; i < n; i++) baseNewDay(); }
    if (buildMode) updateBuildMode();
    Character& p = pc();
    float d = length(p.pos.xz() - base.center);
    const Poi& P = world.pois[base.poi];
    vec3 c3(base.center.x, P.height, base.center.y);
    // soldiers: keep the roster spawned while the hero is around (or escorting him)
    // 3.0.0 the army: swordsmen, archers and knights; they follow the hero into the dungeons on the escort order
    soldierEnts.erase(std::remove_if(soldierEnts.begin(), soldierEnts.end(), [&](Entity e) {
        Character* c = reg.valid(e) ? reg.get<Character>(e) : nullptr;
        if (c && c->dead) {
            SoldierBrain* sb = reg.get<SoldierBrain>(e);
            int u = sb ? sb->unit : 0;
            int& n = u == 1 ? base.archers : u == 2 ? base.knights : base.soldiers;
            n = std::max(0, n - 1); base.fallen++;
            notify(c->name + " a succombé au combat.", vec3(1.f, 0.6f, 0.5f), "i_village");
            return true;
        }
        return !c;
    }), soldierEnts.end());
    bool inInterior = dungeon.active && dungeon.interior;
    bool wantSoldiers = !inInterior && ((!dungeon.active && d < 200.f) || base.escort);
    int army = armySize();
    if (!wantSoldiers || (int)soldierEnts.size() > army) { for (Entity e : soldierEnts) reg.destroyLater(e); soldierEnts.clear(); }
    if (wantSoldiers) while ((int)soldierEnts.size() < army) {
        int slot = (int)soldierEnts.size();
        int unit = slot < base.soldiers ? 0 : (slot < base.soldiers + base.archers ? 1 : 2);
        bool home = !dungeon.active && d < 200.f && !base.escort;
        vec3 sp = home ? c3 + vec3(std::cos(slot * 2.1f) * 4.f, 0, std::sin(slot * 2.1f) * 4.f) : p.pos + vec3(std::cos(slot * 2.1f) * 3.f, 0, std::sin(slot * 2.1f) * 3.f);
        Entity e = spawnArmyUnit(unit, sp, slot, -1);
        if (SoldierBrain* sb = reg.get<SoldierBrain>(e)) {
            float a = slot * TAU / std::max(1, army) + 0.4f;
            float rr = std::min(baseBuildRadius() - 4.f, 12.f + base.level * 8.f) * (unit == 1 ? 0.7f : 1.f);
            sb->post = c3 + vec3(std::cos(a) * rr, 0, std::sin(a) * rr);
        }
        soldierEnts.push_back(e);
    }
    // 3.0.0 escort: soldiers left far behind (fast travel, a dungeon's stairs) are out of the AI's update range:
    // bring them to the hero here, in formation
    if (base.escort)
        for (size_t k = 0; k < soldierEnts.size(); k++) {
            Character* c = reg.valid(soldierEnts[k]) ? reg.get<Character>(soldierEnts[k]) : nullptr;
            if (!c || c->dead || distXZ(c->pos, p.pos) < 60.f) continue;
            int row = (int)k / 3, col = (int)k % 3;
            vec2 back{-std::sin(p.yaw), -std::cos(p.yaw)}, side{std::cos(p.yaw), -std::sin(p.yaw)};
            vec2 g2 = p.pos.xz() + back * (2.6f + row * 1.9f) + side * ((col - 1) * 1.9f);
            if (dungeon.active) { float a = k * 2.4f; g2 = p.pos.xz() + vec2(std::cos(a), std::sin(a)) * (1.4f + 0.25f * (k % 3)); }   // inside: stay on the hero's floor
            c->pos = vec3(g2.x, groundAt(g2.x, g2.y), g2.y); c->vel = vec3(0);
        }
    // livestock in the pens
    livestockEnts.erase(std::remove_if(livestockEnts.begin(), livestockEnts.end(), [&](Entity e) { return !reg.valid(e); }), livestockEnts.end());
    if (d > 220.f || dungeon.active) { for (Entity e : livestockEnts) reg.destroyLater(e); livestockEnts.clear(); }
    else if (livestockEnts.empty() && (base.pigs + base.horses) > 0) {
        std::vector<vec2> pens; for (auto& b : base.built) if (baseBuildings()[b.def].id == "pen") pens.push_back(b.pos);
        if (!pens.empty()) {
            int k = 0;
            auto put = [&](int kind) {
                vec2 pc2 = pens[k % pens.size()]; k++;
                Rng r(k * 31 + 7);
                vec3 sp(pc2.x + r.range(-3.f, 3.f), 0, pc2.y + r.range(-3.f, 3.f)); sp.y = groundAt(sp.x, sp.z);
                Entity e = spawnAnimal(kind, sp);
                Character* c = reg.get<Character>(e);
                AnimalBrain* ab = reg.get<AnimalBrain>(e);
                if (c && ab) {
                    ab->tame = true; ab->home = vec3(pc2.x, sp.y, pc2.y); c->team = Team::Neutral;
                    CreatureLook L; L.kind = kind; L.seed = (uint32_t)(k * 17);
                    if (kind == 1) { L.c1 = hexColor(0xd89a8a); L.c2 = hexColor(0xe8b8a8); L.c3 = hexColor(0x8a5a50); L.fur = false; c->name = "Cochon"; setupVisualCreature(c->vis, Arch::Quadruped, L, 0.75f); }
                    else { L.c1 = hexColor(r.chance(0.5f) ? 0x6a4a30 : 0x3a2a20); L.c2 = hexColor(0x8a6a4a); L.mane = true; c->name = "Cheval"; setupVisualCreature(c->vis, Arch::Quadruped, L, 1.9f); applyImportedModel(c->vis, "horse_mount", Arch::Quadruped); c->height = 1.9f; c->radius = 0.7f; }
                    c->vis.outline = false;
                }
                livestockEnts.push_back(e);
            };
            for (int i = 0; i < base.pigs; i++) put(1);
            for (int i = 0; i < base.horses; i++) put(3);
        }
    }
}

// Soldiers guard the domain (or follow the hero in escort mode) and fight the enemies that come near.
void Game::updateSoldier(Entity e, Character& c, SoldierBrain& sb, float dt) {
    Animator& an = c.vis.anim;
    sb.cd = std::max(0.f, sb.cd - dt);
    sb.think -= dt;
    Character& p = pc();
    bool follow = base.escort && sb.fort < 0;            // 3.0.0 the guards of a conquered fortress hold their post
    bool charge = follow && base.charge;                  // "attack at will": they range further from the hero
    vec3 anchor = follow ? p.pos : sb.post;
    if (sb.think <= 0) {
        sb.think = 0.3f;
        sb.target = NullEntity;
        float best = 1e9f;
        float seek = charge ? 34.f : (sb.unit == 1 ? 26.f : 22.f), leash = charge ? 40.f : (follow ? 20.f : 36.f);
        reg.each<Character>([&](Entity o, Character& oc) {
            if (oc.dead || oc.team != Team::Enemy) return;
            float dd = distXZ(oc.pos, c.pos);
            if (dd < seek && distXZ(oc.pos, anchor) < leash && dd < best) { best = dd; sb.target = o; }
        });
    }
    vec2 mv{0, 0}; float spd = 0;
    Character* t = sb.target != NullEntity && reg.valid(sb.target) ? reg.get<Character>(sb.target) : nullptr;
    if (t && !t->dead && sb.unit == 1) {
        // 3.0.0 archers keep their distance and shoot
        float dd = distXZ(t->pos, c.pos);
        if (dd > 18.f) { mv = steer(c, t->pos, dt); spd = 4.4f; }
        else if (dd < 6.f) { vec2 away = normalize(c.pos.xz() - t->pos.xz()); mv = away; spd = 3.6f; }
        c.yaw = dampAngle(c.yaw, std::atan2(t->pos.x - c.pos.x, t->pos.z - c.pos.z), 10, dt);
        if (dd < 22.f && sb.cd <= 0 && !c.attackClip && c.hitStun <= 0) {
            const Clip* cl = &clip(C_BOW_SHOT);
            an.play(cl, 1.f, false, 0.06f);
            vec3 from = c.pos + vec3(0, 1.4f, 0);
            vec3 to = t->pos + vec3(0, t->height * 0.55f, 0) + t->vel * (dd / 34.f);
            fireProjectile(e, ProjKind::Arrow, from, normalize(to - from), 34.f, c.atk * 1.1f, Element::Physical, Team::Player);
            play3D("bow", c.pos, 0.5f, 1.05f);
            sb.cd = 1.5f + particles.rng.range(0.f, 0.5f);
        }
    } else if (t && !t->dead) {
        float dd = distXZ(t->pos, c.pos);
        if (dd > 1.7f + t->radius) { mv = steer(c, t->pos, dt); spd = 4.6f; }
        c.yaw = dampAngle(c.yaw, std::atan2(t->pos.x - c.pos.x, t->pos.z - c.pos.z), 10, dt);
        if (dd < 2.2f + t->radius && sb.cd <= 0 && !c.attackClip && c.hitStun <= 0) {
            const Clip* cl = &clip(sb.unit == 2 ? (particles.rng.chance(0.5f) ? C_2H_A1 : C_2H_A3) : (particles.rng.chance(0.5f) ? C_1H_A1 : C_1H_A2));
            an.play(cl, 1.f, false, 0.06f);
            c.attackClip = cl; c.attackMult = sb.unit == 2 ? 1.3f : 1.f; c.hitThisSwing.clear(); c.attackIdx = 0; c.lastMotion = 0;
            sb.cd = (sb.unit == 2 ? 1.5f : 1.1f) + particles.rng.range(0.f, 0.5f);
        }
    } else {
        vec3 goal = anchor;
        if (follow) {   // 3.0.0 a formation behind the hero: rows of three, archers at the back
            int row = sb.slot / 3, col = sb.slot % 3;
            vec2 back{-std::sin(p.yaw), -std::cos(p.yaw)}, side{std::cos(p.yaw), -std::sin(p.yaw)};
            vec2 g2 = p.pos.xz() + back * (2.6f + row * 1.9f + (sb.unit == 1 ? 1.5f : 0.f)) + side * ((col - 1) * 1.9f);
            goal = vec3(g2.x, p.pos.y, g2.y);
        }
        float dd = distXZ(goal, c.pos);
        if (dd > 1.2f) { mv = steer(c, goal, dt); spd = dd > 8.f ? 5.f : 1.9f; }
        if (follow && dd > 60.f) { c.pos = goal; c.pos.y = groundAt(goal.x, goal.z); }   // caught up (teleports, fast travel, dungeons)
        c.hp = std::min(c.maxHp, c.hp + dt * 4.f);
    }
    vec3 w = vec3(mv.x, 0, mv.y) * spd;
    c.vel.x = damp(c.vel.x, w.x, 6, dt); c.vel.z = damp(c.vel.z, w.z, 6, dt);
    float hs = length(vec2(c.vel.x, c.vel.z));
    if (hs > 0.3f && !t) c.yaw = dampAngle(c.yaw, std::atan2(c.vel.x, c.vel.z), 7, dt);
    an.speed = hs;
    (void)e;
}

// ------------------------------------------------------------------------------------------ save
void Game::saveBase(std::ostream& o) const {
    if (!base.founded) return;
    o << "base_found " << base.center.x << " " << base.center.y << "\n";
    o << "base_name " << base.name << "\n";
    o << "base_n " << base.residents << " " << base.soldiers << " " << base.pigs << " " << base.horses << " " << base.lastDay << " " << base.foalDays << " " << (base.escort ? 1 : 0) << "\n";
    o << "base_army " << base.archers << " " << base.knights << " " << (base.charge ? 1 : 0) << " " << base.level << " " << base.dunLevel << " " << base.fallen << "\n";   // 3.0.0
    for (auto& b : base.built) o << "base_b " << baseBuildings()[b.def].id << " " << b.pos.x << " " << b.pos.y << " " << b.rot << " " << b.variant << "\n";
    for (auto& s : base.storage) o << "base_store " << s.first << " " << s.second << "\n";
}

bool Game::loadBaseLine(const std::string& k, std::istream& l, PlayerBase* into) {
    PlayerBase& base = into ? *into : this->base;   // 1.13.0 also reads the co-op partner's domain
    if (k == "base_found") { base.founded = true; l >> base.center.x >> base.center.y; return true; }
    if (k == "base_name") { std::string s; std::getline(l, s); if (!s.empty() && s[0] == ' ') s.erase(0, 1); if (!s.empty()) base.name = s.substr(0, 40); return true; }
    if (k == "base_n") { int esc = 0; l >> base.residents >> base.soldiers >> base.pigs >> base.horses >> base.lastDay >> base.foalDays >> esc; base.escort = esc != 0; return true; }
    if (k == "base_army") { int ch = 0; l >> base.archers >> base.knights >> ch >> base.level >> base.dunLevel >> base.fallen; base.charge = ch != 0; base.level = std::min(2, std::max(0, base.level)); return true; }
    if (k == "base_b") {
        std::string id; BaseBuilt b; l >> id >> b.pos.x >> b.pos.y >> b.rot >> b.variant;
        b.def = baseBuildIndex(id);
        if (b.def >= 0) base.built.push_back(b);
        return true;
    }
    if (k == "base_store") { std::string id; int n = 0; l >> id >> n; if (item(id) && n > 0) base.storage.push_back({id, std::min(n, 999)}); return true; }
    return false;
}

// ------------------------------------------------------------------------------------------ UI
void Game::drawBaseMenu(Rect c) {
    UI& u = ui;
    const vec4 GOLDT = rgba(0xf4d58a), DIM = rgba(0xb0a48c), PALE = rgba(0xf4e7c4);
    if (!base.founded) {
        u.text("Fonder un domaine", c.x, c.y, 28, GOLDT, F_TITLE);
        float y = c.y + 50;
        static const char* lines[] = {
            "Plantez votre étendard dans les terres sauvages et bâtissez votre propre village :",
            "· maisons, tentes et grande maison pour loger vos habitants ;",
            "· champs, étals, puits et enclos pour produire pain, or, viande et cuir chaque jour ;",
            "· forge pour travailler l'acier, tour de guet et palissades pour vous défendre ;",
            "· des soldats qui gardent le domaine ou vous escortent ; des cochons et des chevaux à élever.",
            "La nuit, les traqueurs n'entrent pas dans un domaine protégé par une tour de guet.",
            "Le domaine est fondé quelques pas devant vous. Il faut 10 bois, loin des villages, des routes et de l'eau."};
        for (const char* l : lines) { u.text(l, c.x, y, 19, l[0] == '\xc2' ? PALE : DIM, F_BODY, A_LEFT, c.w * 0.9f); y += u.textHeight(l, 19, F_BODY, c.w * 0.9f) + 8; }
        std::string why;
        bool ok = canFoundBase(why);
        if (u.fancyButton({c.x, y + 20, 380, 64}, "Fonder mon domaine ici", ok, "button", 22)) { foundBase(); menu = Menu::None; }
        if (!ok) u.text(why, c.x + 400, y + 40, 19, rgba(0xe89a7a), F_BOLD, A_LEFT, c.w - 420);
        return;
    }
    char b[160];
    u.text(base.name, c.x, c.y - 4, 28, GOLDT, F_TITLE);
    static const char* lvName[3] = {"Village", "Bourg", "Château"};
    snprintf(b, sizeof b, "%s · Habitants %d/%d · Armée %d/%d (%d épées, %d arcs, %d chevaliers) · Bétail %d · Bâtiments %zu", lvName[std::min(2, base.level)], base.residents, baseHousing(), armySize(), baseGuardCap(), base.soldiers, base.archers, base.knights, base.pigs + base.horses, base.built.size());
    u.text(b, c.x, c.y + 36, 18, PALE, F_BODY);
    // left: buildings
    Rect L{c.x, c.y + 74, c.w * 0.6f - 10, c.h - 74};
    u.pushClip(L);
    const auto& D = baseBuildings();
    float rowH = 92;
    float so = u.scrollArea("baseBuild", L, D.size() * rowH);
    for (int i = 0; i < (int)D.size(); i++) {
        Rect r{L.x, L.y + i * rowH - so, L.w - 8, rowH - 8};
        u.nine("panel_plain", r, vec4(1, 1, 1, 0.85f), 0.5f);
        u.text(D[i].name + (baseCount(D[i].id) ? "  ×" + std::to_string(baseCount(D[i].id)) : ""), r.x + 16, r.y + 10, 21, GOLDT, F_SERIF);
        u.text(D[i].desc, r.x + 16, r.y + 40, 16, DIM, F_BODY, A_LEFT, r.w - 210);
        std::string cost;
        for (auto& cc : D[i].cost) { const ItemDef* it = item(cc.first); cost += (cost.empty() ? "" : " · ") + std::to_string(countItem(cc.first)) + "/" + std::to_string(cc.second) + " " + (it ? it->name : cc.first); }
        u.text(cost, r.x + 16, r.y + 62, 15, baseCanAfford(D[i]) ? rgba(0x9ae08a) : rgba(0xe89a7a), F_BODY, A_LEFT, r.w - 210);
        bool can = baseCanAfford(D[i]) && !(D[i].unique && baseCount(D[i].id));
        u.tag({r.x + r.w - 180, r.y + 16, 164, 52}, "build:" + D[i].id);
        if (u.fancyButton({r.x + r.w - 180, r.y + 16, 164, 52}, "Construire", can, "button", 19)) { startBuilding(i); break; }
    }
    u.popClip();
    // right: people, livestock, production
    Rect R{c.x + c.w * 0.6f + 10, c.y + 74, c.w * 0.4f - 10, c.h - 74};
    u.pushClip(R);
    float rso = u.scrollArea("baseActs", R, 15 * 64 + 120);   // 3.0.0 more orders: the panel scrolls
    float y = R.y - rso;
    auto act = [&](const std::string& label, bool en, const std::string& tag) { Rect r{R.x, y, R.w - 8, 56}; u.tag(r, tag); y += 64; return u.fancyButton(r, label, en, "button_dark", 18); };
    snprintf(b, sizeof b, "Recruter un habitant (40 or)");
    if (act(b, gold >= 40 && base.residents < baseHousing(), "recruit")) { gold -= 40; base.residents++; refreshBaseNpcs(); play("coin", 0.6f); notify("Un nouvel habitant s'installe au domaine.", vec3(0.8f, 1.f, 0.7f), "i_village"); }
    bool room = armySize() < baseGuardCap();
    if (act("Recruter un soldat (120 or)", gold >= 120 && room, "soldier")) { gold -= 120; base.soldiers++; play("coin", 0.6f); notify("Un soldat rejoint votre armée.", vec3(0.8f, 0.9f, 1.f), "i_attack"); }
    // 3.0.0 the army: archers (ranged) and knights (heavy, need barracks)
    if (act("Recruter un archer (150 or)", gold >= 150 && room, "archer")) { gold -= 150; base.archers++; play("coin", 0.6f); notify("Un archer rejoint votre armée.", vec3(0.8f, 0.9f, 1.f), "i_cl_archer"); }
    if (act(baseCount("barracks") ? "Former un chevalier (400 or, 2 lingots)" : "Chevalier : il faut une caserne", baseCount("barracks") > 0 && gold >= 400 && countItem("iron_bar") >= 2 && room, "knight")) { gold -= 400; removeItem("iron_bar", 2); base.knights++; play("coin", 0.6f); notify("Un chevalier prête serment.", vec3(0.8f, 0.9f, 1.f), "i_block"); }
    if (act("Acheter un cochon (35 or)", gold >= 35 && base.pigs < baseCount("pen") * 4, "pig")) { gold -= 35; base.pigs++; refreshBaseNpcs(); play("coin", 0.6f); }
    if (act("Acheter un cheval (250 or)", gold >= 250 && base.horses < baseCount("pen") * 2, "horse")) { gold -= 250; base.horses++; refreshBaseNpcs(); play("horse", 0.6f); }
    {   // 3.0.0 three orders: guard the domain / escort me (also into the dungeons) / attack at will
        const char* ord = !base.escort ? "Ordre : garder le domaine" : (base.charge ? "Ordre : attaquer à volonté" : "Ordre : m'escorter (donjons aussi)");
        if (act(ord, armySize() > 0, "escort")) {
            if (!base.escort) { base.escort = true; base.charge = false; } else if (!base.charge) base.charge = true; else { base.escort = false; base.charge = false; }
            notify(!base.escort ? "Votre armée garde le domaine." : base.charge ? "Votre armée attaque tout ennemi à portée !" : "Votre armée vous suit, même dans les donjons.", vec3(0.8f, 0.9f, 1.f), "i_attack");
        }
    }
    std::string st;
    for (auto& s : base.storage) { const ItemDef* it = item(s.first); st += (st.empty() ? "" : ", ") + std::to_string(s.second) + " " + (it ? it->name : s.first); }
    if (act(st.empty() ? "Rien à récolter" : "Récolter : " + st, !base.storage.empty(), "collect")) {
        for (auto& s : base.storage) addItem(s.first, s.second);
        base.storage.clear(); play("pickup", 0.8f);
    }
    {   // 3.0.0 grow the domain (town, castle)
        std::string why; bool ok = upgradeBase(why, false);
        std::string lab = base.level == 0 ? "Agrandir en bourg (1500 or)" : base.level == 1 ? "Agrandir en château (4000 or)" : "Votre domaine est un château";
        if (act(lab, ok, "upgrade")) upgradeBase(why, true);
        if (!ok && base.level < 2) { u.text(why, R.x, y - 6, 14, rgba(0xe89a7a), F_BODY, A_LEFT, R.w); y += u.textHeight(why, 14, F_BODY, R.w) + 4; }
    }
    if (baseCount("crypt") > 0) {   // 3.0.0 the domain's dungeon: pick its level
        int lv = std::max(5, base.dunLevel > 0 ? base.dunLevel : level);
        Rect r1{R.x, y, 90, 56}, r2{R.x + R.w - 98, y, 90, 56};
        u.tag(r1, "dun-"); u.tag(r2, "dun+");
        if (u.fancyButton(r1, "-", lv > 5, "button_dark", 22)) base.dunLevel = std::max(5, lv - 5);
        if (u.fancyButton(r2, "+", lv < 50, "button_dark", 22)) base.dunLevel = std::min(50, lv + 5);
        snprintf(b, sizeof b, "Donjon du domaine : niveau %d", lv);
        u.text(b, R.x + R.w * 0.5f - 4, y + 16, 17, PALE, F_BOLD, A_CENTER);
        y += 64;
    }
    u.text("La production arrive chaque matin. Un habitant cultive un champ ou tient un étal. Chaque forteresse conquise ajoute 4 places à l'armée.", R.x, y + 4, 15, DIM, F_BODY, A_LEFT, R.w - 8);
    u.popClip();
}

void Game::drawBuildHud() {
    if (!buildMode || buildDef < 0) return;
    UI& u = ui;
    const BaseBuildDef& d = baseBuildings()[buildDef];
    float W_ = u.W;
    Rect p{W_ * 0.5f - 330, 84, 660, 128};   // under the quest tracker: the action buttons stay free
    u.nine("panel_plain", p, vec4(1, 1, 1, 0.92f), 0.6f);
    u.text(d.name, p.x + 20, p.y + 12, 24, rgba(0xf4d58a), F_SERIF);
    u.text(buildOk ? "Emplacement libre." : buildWhy, p.x + 20, p.y + 46, 17, buildOk ? rgba(0x9ae08a) : rgba(0xe89a7a), F_BODY, A_LEFT, 300);
    Rect r1{p.x + 330, p.y + 30, 96, 68}, r2{p.x + 432, p.y + 30, 120, 68}, r3{p.x + 558, p.y + 30, 90, 68};
    u.tag(r1, "Tourner"); u.tag(r2, "Construire"); u.tag(r3, "Annuler");
    if (u.fancyButton(r1, "Tourner", true, "button_dark", 17)) buildRot += PI * 0.25f;
    if (u.fancyButton(r2, "Construire", buildOk, "button", 18)) placeBuilding();
    if (u.fancyButton(r3, "Annuler", true, "button_dark", 17)) { buildMode = false; buildGhost.reset(); buildGhostModels.clear(); }
}

}  // namespace lk
