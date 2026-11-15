// LOST KINGDOM — 1.9.2 the player's domain: found it anywhere in the wild, build houses, fields, a forge, a
// watchtower, palisades and a livestock pen, recruit inhabitants and soldiers, raise pigs and horses.
// The domain is a real settlement of the world: a POI (levelled ground, on the map, safe at night once it has a
// watchtower) and structures built by the chunk builder like any village (meshes, colliders, lights, smoke).
#include "Game.h"
#include "../engine/core/Platform.h"
#include <algorithm>
#include <sstream>

namespace lk {

static const float BASE_RADIUS = 30.f;     // levelled ground of the domain
static const float BUILD_RADIUS = 34.f;    // buildings must fit inside this circle

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
    });
    return d;
}
int baseBuildIndex(const std::string& id) { auto& d = baseBuildings(); for (int i = 0; i < (int)d.size(); i++) if (d[i].id == id) return i; return -1; }

int Game::baseCount(const std::string& id) const { int i = baseBuildIndex(id), n = 0; for (auto& b : base.built) if (b.def == i) n++; return n; }
int Game::baseHousing() const { int n = 0; for (auto& b : base.built) n += baseBuildings()[b.def].housing; return n; }
int Game::baseGuardCap() const { int n = 0; for (auto& b : base.built) n += baseBuildings()[b.def].guards; return n; }
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
    Poi p; p.id = "player_base"; p.name = base.name; p.type = PoiType::Village; p.pos = base.center; p.radius = BASE_RADIUS;
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
    if (length(buildPos - base.center) + d.radius > BUILD_RADIUS) { buildOk = false; buildWhy = "Hors du domaine (approchez de l'étendard)."; }
    else if (length(buildPos - base.center) < d.radius + 2.2f) { buildOk = false; buildWhy = "Trop près de l'étendard."; }
    for (const BaseBuilt& b : base.built) {
        const BaseBuildDef& o = baseBuildings()[b.def];
        float overlap = (d.id == "palisade" && o.id == "palisade") ? 0.6f : 0.f;
        if (length(b.pos - buildPos) < d.radius + o.radius - overlap) { buildOk = false; buildWhy = "Trop près : " + o.name + "."; break; }
    }
    if (buildOk && d.unique && baseCount(d.id) > 0) { buildOk = false; buildWhy = "Déjà construit."; }
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
    add("bread", (int)std::round(fields * 2 * mult));
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
    soldierEnts.erase(std::remove_if(soldierEnts.begin(), soldierEnts.end(), [&](Entity e) {
        Character* c = reg.valid(e) ? reg.get<Character>(e) : nullptr;
        if (c && c->dead) { base.soldiers = std::max(0, base.soldiers - 1); notify(c->name + " est tombé au combat.", vec3(1.f, 0.6f, 0.5f), "i_village"); return true; }
        return !c;
    }), soldierEnts.end());
    bool wantSoldiers = !dungeon.active && (d < 200.f || base.escort);
    if (!wantSoldiers) { for (Entity e : soldierEnts) reg.destroyLater(e); soldierEnts.clear(); }
    else while ((int)soldierEnts.size() < base.soldiers) {
        int slot = (int)soldierEnts.size();
        static const char* names[] = {"Garde Odric", "Garde Ilse", "Garde Matthis", "Garde Sorenne", "Garde Brune", "Garde Aymar", "Garde Ysault", "Garde Thibaut", "Garde Alix"};
        vec3 sp = (d < 200.f) ? c3 + vec3(std::cos(slot * 2.1f) * 4.f, 0, std::sin(slot * 2.1f) * 4.f) : p.pos + vec3(std::cos(slot * 2.1f) * 3.f, 0, std::sin(slot * 2.1f) * 3.f);
        sp.y = groundAt(sp.x, sp.z);
        Entity e = reg.create();
        Character c; c.team = Team::Player; c.pos = sp; c.name = names[slot % 9];
        c.maxHp = c.hp = 140.f + level * 14.f; c.atk = 12.f + level * 1.4f; c.def = 6.f + level * 0.6f; c.radius = 0.45f; c.maxPoise = c.poise = 40;
        HumanoidLook L; L.seed = 300 + slot; L.female = slot % 3 == 1; L.body = Body::Chain; L.helm = Helm::Iron; L.c1 = hexColor(0x8a1f1f); L.c2 = hexColor(0x5a4030); L.trim = hexColor(0xd8b040);
        L.hair = L.female ? Hair::Braids : Hair::Short;
        setupVisualHumanoid(c.vis, L, WeaponType::Sword, WeaponType::Shield, 1.f);
        c.vis.anim.stance = ST_ONEHAND;
        c.height = 1.8f;
        reg.add<Character>(e, c);
        SoldierBrain sb; sb.slot = slot;
        float a = slot * TAU / std::max(1, base.soldiers) + 0.4f;
        sb.post = c3 + vec3(std::cos(a) * 12.f, 0, std::sin(a) * 12.f);
        reg.add<SoldierBrain>(e, sb);
        soldierEnts.push_back(e);
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
                    else { L.c1 = hexColor(r.chance(0.5f) ? 0x6a4a30 : 0x3a2a20); L.c2 = hexColor(0x8a6a4a); L.mane = true; c->name = "Cheval"; setupVisualCreature(c->vis, Arch::Quadruped, L, 1.9f); c->height = 1.9f; c->radius = 0.7f; }
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
    vec3 anchor = base.escort ? p.pos : sb.post;
    if (sb.think <= 0) {
        sb.think = 0.3f;
        sb.target = NullEntity;
        float best = 1e9f;
        reg.each<Character>([&](Entity o, Character& oc) {
            if (oc.dead || oc.team != Team::Enemy) return;
            float dd = distXZ(oc.pos, c.pos);
            if (dd < 22.f && distXZ(oc.pos, anchor) < (base.escort ? 20.f : 36.f) && dd < best) { best = dd; sb.target = o; }
        });
    }
    vec2 mv{0, 0}; float spd = 0;
    Character* t = sb.target != NullEntity && reg.valid(sb.target) ? reg.get<Character>(sb.target) : nullptr;
    if (t && !t->dead) {
        float dd = distXZ(t->pos, c.pos);
        if (dd > 1.7f + t->radius) { mv = steer(c, t->pos, dt); spd = 4.6f; }
        c.yaw = dampAngle(c.yaw, std::atan2(t->pos.x - c.pos.x, t->pos.z - c.pos.z), 10, dt);
        if (dd < 2.2f + t->radius && sb.cd <= 0 && !c.attackClip && c.hitStun <= 0) {
            const Clip* cl = &clip(particles.rng.chance(0.5f) ? C_1H_A1 : C_1H_A2);
            an.play(cl, 1.f, false, 0.06f);
            c.attackClip = cl; c.attackMult = 1.f; c.hitThisSwing.clear(); c.attackIdx = 0; c.lastMotion = 0;
            sb.cd = 1.1f + particles.rng.range(0.f, 0.5f);
        }
    } else {
        vec3 goal = anchor;
        if (base.escort) { float a = sb.slot * 1.9f + 2.4f; goal = p.pos + vec3(std::cos(a + p.yaw) * 2.8f, 0, std::sin(a + p.yaw) * 2.8f); }
        float dd = distXZ(goal, c.pos);
        if (dd > 1.2f) { mv = steer(c, goal, dt); spd = dd > 8.f ? 5.f : 1.9f; }
        if (base.escort && dd > 60.f) { c.pos = goal; c.pos.y = groundAt(goal.x, goal.z); }   // caught up (teleports, fast travel)
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
    for (auto& b : base.built) o << "base_b " << baseBuildings()[b.def].id << " " << b.pos.x << " " << b.pos.y << " " << b.rot << " " << b.variant << "\n";
    for (auto& s : base.storage) o << "base_store " << s.first << " " << s.second << "\n";
}

bool Game::loadBaseLine(const std::string& k, std::istream& l) {
    if (k == "base_found") { base.founded = true; l >> base.center.x >> base.center.y; return true; }
    if (k == "base_name") { std::string s; std::getline(l, s); if (!s.empty() && s[0] == ' ') s.erase(0, 1); if (!s.empty()) base.name = s.substr(0, 40); return true; }
    if (k == "base_n") { int esc = 0; l >> base.residents >> base.soldiers >> base.pigs >> base.horses >> base.lastDay >> base.foalDays >> esc; base.escort = esc != 0; return true; }
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
    snprintf(b, sizeof b, "Habitants %d/%d · Soldats %d/%d · Cochons %d · Chevaux %d · Bâtiments %zu", base.residents, baseHousing(), base.soldiers, baseGuardCap(), base.pigs, base.horses, base.built.size());
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
    float y = R.y;
    auto act = [&](const std::string& label, bool en, const std::string& tag) { Rect r{R.x, y, R.w, 56}; u.tag(r, tag); y += 64; return u.fancyButton(r, label, en, "button_dark", 18); };
    snprintf(b, sizeof b, "Recruter un habitant (40 or)");
    if (act(b, gold >= 40 && base.residents < baseHousing(), "recruit")) { gold -= 40; base.residents++; refreshBaseNpcs(); play("coin", 0.6f); notify("Un nouvel habitant s'installe au domaine.", vec3(0.8f, 1.f, 0.7f), "i_village"); }
    if (act("Recruter un soldat (120 or)", gold >= 120 && base.soldiers < baseGuardCap(), "soldier")) { gold -= 120; base.soldiers++; play("coin", 0.6f); notify("Un soldat rejoint votre garde.", vec3(0.8f, 0.9f, 1.f), "i_attack"); }
    if (act("Acheter un cochon (35 or)", gold >= 35 && base.pigs < baseCount("pen") * 4, "pig")) { gold -= 35; base.pigs++; refreshBaseNpcs(); play("coin", 0.6f); }
    if (act("Acheter un cheval (250 or)", gold >= 250 && base.horses < baseCount("pen") * 2, "horse")) { gold -= 250; base.horses++; refreshBaseNpcs(); play("horse", 0.6f); }
    if (act(base.escort ? "Soldats : m'escorter (actif)" : "Soldats : garder le domaine", base.soldiers > 0, "escort")) { base.escort = !base.escort; notify(base.escort ? "Vos soldats vous suivent." : "Vos soldats gardent le domaine.", vec3(0.8f, 0.9f, 1.f), "i_attack"); }
    std::string st;
    for (auto& s : base.storage) { const ItemDef* it = item(s.first); st += (st.empty() ? "" : ", ") + std::to_string(s.second) + " " + (it ? it->name : s.first); }
    if (act(st.empty() ? "Rien à récolter" : "Récolter : " + st, !base.storage.empty(), "collect")) {
        for (auto& s : base.storage) addItem(s.first, s.second);
        base.storage.clear(); play("pickup", 0.8f);
    }
    u.text("La production arrive chaque matin. Un habitant cultive un champ ou tient un étal.", R.x, y + 4, 15, DIM, F_BODY, A_LEFT, R.w);
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
