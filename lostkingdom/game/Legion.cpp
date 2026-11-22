// LOST KINGDOM — 3.0.0 the Iron Legion: enemy fortresses held by a garrison and a warlord.
//   · Walk up to a fortress: its garrison (legionaries, crossbowmen, pikemen, a black knight) stands at its posts
//     inside the walls, the warlord before the keep. Leave without taking it and the garrison is back next time.
//   · Kill the warlord: the garrison surrenders, the fortress is yours (story flag "conq_<poi id>"). Your own
//     guards hold it from then on, it pays a tribute every morning, raises the size of your army by 4, and you can
//     fast-travel to it from the map.
#include "Game.h"
#include "../engine/core/Platform.h"
#include <algorithm>

namespace lk {

int Game::conqueredForts() const {
    int n = 0;
    for (const Poi& p : world.pois) if (p.hostile && fortConquered(p.id)) n++;
    return n;
}

// one soldier of the hero's army (the domain's roster, or the guard of a conquered fortress)
Entity Game::spawnArmyUnit(int unit, vec3 pos, int slot, int fort) {
    static const char* swords[] = {"Garde Odric", "Garde Ilse", "Garde Matthis", "Garde Sorenne", "Garde Brune", "Garde Aymar", "Garde Ysault", "Garde Thibaut", "Garde Alix"};
    static const char* bows[] = {"Archère Maëlys", "Archer Gauvain", "Archère Iseut", "Archer Renaud", "Archère Hermine", "Archer Lothaire"};
    static const char* knights[] = {"Chevalier Bertrand", "Dame Aliénor", "Chevalier Tristan", "Dame Mélisande", "Chevalier Arnaut"};
    Entity e = reg.create();
    Character c; c.team = Team::Player; c.pos = pos; c.pos.y = groundAt(pos.x, pos.z);
    c.name = unit == 1 ? bows[slot % 6] : unit == 2 ? knights[slot % 5] : swords[slot % 9];
    float lv = (float)level;
    c.maxHp = c.hp = (unit == 2 ? 320.f : unit == 1 ? 110.f : 140.f) + lv * (unit == 2 ? 26.f : 14.f);
    c.atk = (unit == 2 ? 18.f : unit == 1 ? 11.f : 12.f) + lv * (unit == 2 ? 2.f : 1.4f);
    c.def = (unit == 2 ? 14.f : unit == 1 ? 4.f : 6.f) + lv * (unit == 2 ? 1.1f : 0.6f);
    c.radius = 0.45f; c.maxPoise = c.poise = unit == 2 ? 120.f : 40.f;
    HumanoidLook L; L.seed = 300 + slot + unit * 37; L.female = slot % 3 == 1;
    L.c1 = hexColor(0x8a1f1f); L.c2 = hexColor(0x5a4030); L.trim = hexColor(0xd8b040);
    L.hair = L.female ? Hair::Braids : Hair::Short;
    WeaponType w = WeaponType::Sword, off = WeaponType::Shield;
    if (unit == 1) { L.body = Body::Leather; L.helm = Helm::Hood; w = WeaponType::Bow; off = WeaponType::None; }
    else if (unit == 2) { L.body = Body::Plate; L.helm = Helm::Great; L.cape = true; L.pauldrons = true; L.metal = hexColor(0xc8ccd4); w = WeaponType::Longsword; off = WeaponType::None; }
    else { L.body = Body::Chain; L.helm = Helm::Iron; }
    setupVisualHumanoid(c.vis, L, w, off, unit == 2 ? 1.06f : 1.f);
    c.vis.anim.stance = unit == 1 ? ST_BOW : unit == 2 ? ST_TWOHAND : ST_ONEHAND;
    c.height = 1.8f;
    reg.add<Character>(e, c);
    SoldierBrain sb; sb.slot = slot; sb.unit = unit; sb.fort = fort; sb.post = c.pos;
    reg.add<SoldierBrain>(e, sb);
    return e;
}

void Game::updateFortresses(float dt) {
    (void)dt;
    if (player == NullEntity || coop.mode == CoopState::Guest) return;
    // tribute of the conquered fortresses, each morning
    if (fortTributeDay != day) {
        int n = conqueredForts();
        if (fortTributeDay >= 0 && n > 0 && day > fortTributeDay) {
            int g = 180 * n * std::min(3, day - fortTributeDay);
            gold += g;
            notify("Tribut de vos forteresses : +" + std::to_string(g) + " or", vec3(1.f, 0.85f, 0.4f), "i_castle");
        }
        fortTributeDay = day;
    }
    if (dungeon.active) return;
    Character& p = pc();
    vec2 pp = p.pos.xz();
    for (int i = 0; i < (int)world.pois.size(); i++) {
        const Poi& P = world.pois[i];
        if (!P.hostile) continue;
        float d = length(P.pos - pp);
        bool conq = fortConquered(P.id);
        vec3 c3(P.pos.x, P.height, P.pos.y);
        // ---- the enemy garrison
        auto git = fortGarrison.find(i);
        if (git != fortGarrison.end()) {
            auto& v = git->second;
            v.erase(std::remove_if(v.begin(), v.end(), [&](Entity e) { return !reg.valid(e); }), v.end());
            bool engaged = false;
            for (Entity e : v) { AIBrain* ai = reg.get<AIBrain>(e); if (ai && (ai->state == AIState::Chase || ai->state == AIState::Attack)) engaged = true; }
            if (conq || (d > P.radius + 260.f && !engaged)) { for (Entity e : v) reg.destroyLater(e); fortGarrison.erase(git); }
        } else if (!conq && d < P.radius + 110.f && d > 25.f) {
            std::vector<Entity> v;
            const float R = P.radius;
            int n = R >= 95.f ? 10 : R >= 79.f ? 8 : 7;
            static const char* units[] = {"legion_soldier", "legion_crossbow", "legion_pikeman", "legion_soldier", "legion_crossbow", "legion_knight", "legion_soldier", "legion_pikeman", "legion_crossbow", "legion_soldier"};
            Rng r(hash32(i * 131 + day));
            for (int k = 0; k < n; k++) {
                int def = enemyIndex(units[k]);
                if (def < 0) continue;
                // crossbowmen by the inner towers and on the wall walk side, the others around the war camp and the gate
                float a = k * TAU / n + r.range(-0.2f, 0.2f);
                bool xbow = enemies()[def].behavior == Behavior::Ranged;
                vec2 q = P.pos + vec2(std::cos(a), std::sin(a)) * (R * (xbow ? 0.62f : r.range(0.3f, 0.48f)));
                vec3 sp(q.x, 0, q.y); sp.y = groundAt(sp.x, sp.z);
                Entity e = spawnEnemy(def, sp, false, hash3i(i, k, 4242));
                if (AIBrain* ai = reg.get<AIBrain>(e)) ai->home = sp;
                v.push_back(e);
            }
            int wd = enemyIndex("legion_warlord");
            if (wd >= 0) {
                vec3 sp(P.pos.x, 0, P.pos.y - R * 0.25f + 13.f); sp.y = groundAt(sp.x, sp.z);   // before the keep's door
                Entity e = spawnEnemy(wd, sp, false, hash3i(i, 99, 4242));
                if (AIBrain* ai = reg.get<AIBrain>(e)) ai->home = sp;
                v.push_back(e);
            }
            fortGarrison[i] = v;
        }
        // ---- the hero's guards in a conquered fortress
        auto fit = fortGuards.find(i);
        if (fit != fortGuards.end()) {
            auto& v = fit->second;
            v.erase(std::remove_if(v.begin(), v.end(), [&](Entity e) { Character* c = reg.valid(e) ? reg.get<Character>(e) : nullptr; return !c || c->dead; }), v.end());
            if (d > P.radius + 260.f) { for (Entity e : v) reg.destroyLater(e); fortGuards.erase(fit); }
        } else if (conq && d < P.radius + 150.f) {
            std::vector<Entity> v;
            for (int k = 0; k < 6; k++) {
                float a = k * TAU / 6 + 0.5f;
                vec3 sp = c3 + vec3(std::cos(a), 0, std::sin(a)) * (P.radius * (k % 2 ? 0.4f : 0.7f));
                v.push_back(spawnArmyUnit(k % 3 == 2 ? 1 : (k == 0 ? 2 : 0), sp, k, i));
            }
            fortGuards[i] = v;
        }
    }
}

void Game::onWarlordKilled(vec3 pos) {
    int best = -1; float bd = 1e9f;
    for (int i = 0; i < (int)world.pois.size(); i++) {
        const Poi& P = world.pois[i];
        if (!P.hostile || fortConquered(P.id)) continue;
        float d = length(P.pos - pos.xz());
        if (d < P.radius + 80.f && d < bd) { bd = d; best = i; }
    }
    if (best < 0) return;
    const Poi& P = world.pois[best];
    storyFlags.insert("conq_" + P.id);
    // the garrison throws down its arms
    auto git = fortGarrison.find(best);
    if (git != fortGarrison.end()) {
        for (Entity e : git->second) {
            Character* c = reg.valid(e) ? reg.get<Character>(e) : nullptr;
            if (c && !c->dead) particles.dissolveBurst(c->pos, vec3(0.8f, 0.75f, 0.7f), 0.6f);
            if (reg.valid(e) && c && !c->dead) reg.destroyLater(e);
        }
        fortGarrison.erase(git);
    }
    int g = 1500;
    gold += g;
    bool first = conqueredForts() == 1;
    addItem(first ? "axe_warlord" : "titan_steel", first ? 1 : 3, first ? Rarity::Legendary : Rarity::Epic);
    addItem("legion_insignia", 5);
    discover(P.id);
    advanceQuests(StepKind::Discover, "conq_" + P.id, 1);
    cinematicText = P.name + " est à vous ! La garnison se rend. Vos gardes tiendront la forteresse : tribut chaque matin, +4 soldats dans votre armée, voyage rapide depuis la carte.";
    cinematicT = 6.f;
    cineFrom = cam.pos; cineTo = pos + vec3(0, 7.f, 0) + normalize(vec3(cam.pos.x - pos.x, 0.f, cam.pos.z - pos.z) + vec3(0.001f, 0, 0)) * 13.f; cineLook = pos + vec3(0, 1.2f, 0);
    notify("Forteresse conquise : " + P.name + " (+" + std::to_string(g) + " or)", vec3(1.f, 0.85f, 0.35f), "i_castle");
    play("quest", 1.f); play("levelup", 0.7f);
    haptic(80, 220);
    particles.magicBurst(pos + vec3(0, 1.5f, 0), vec3(1.f, 0.8f, 0.4f), 3.f, 80);
    saveGame(saveSlot);
}

}  // namespace lk
