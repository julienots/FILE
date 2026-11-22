// LOST KINGDOM — 3.2.0 the treasure hunt (Game side): key items and their powers, bags and slots, star shards,
// the totems of power and the ancient orbs, the "item obtained" pop-ups, the compass of the Ancients, gliding.
#include "Game.h"
#include "../engine/core/Platform.h"
#include <algorithm>

namespace lk {

// what each totem gives, by rank (rank 0 = the nearest to Brindlemere). Past the list: hearts, stamina, skill points, shards.
static const char* kTotemRewards[] = {
    "key:key_compass", "key:key_zephyr", "heart", "spell:skyfall", "key:key_magnet", "key:key_feather", "stamina", "heart",
    "key:key_triton", "spell:dragonfire", "key:key_swift", "heart", "key:key_clover", "stamina", "spell:glacier", "key:key_phoenix", "heart"};
static std::string totemReward(int rank) {
    const int n = (int)(sizeof kTotemRewards / sizeof *kTotemRewards);
    if (rank >= 0 && rank < n) return kTotemRewards[rank];
    static const char* cycle[] = {"heart", "stamina", "skill", "shards"};
    return cycle[(rank - n) % 4];
}
std::string totemRewardForRank(int rank) { return totemReward(rank); }

bool Game::hasPower(int kp) const {
    for (const ItemStack& s : inventory) { const ItemDef* d = item(s.id); if (d && d->kind == ItemKind::Key && d->power == kp) return true; }
    return false;
}

int Game::bagUsed() const {
    int n = 0;
    for (const ItemStack& s : inventory) { const ItemDef* d = item(s.id); if (!d || d->kind != ItemKind::Key) n++; }
    return n;
}

int Game::orbsTaken() const {
    int n = 0;
    for (const World::Special& sp : world.specials) if (sp.type == 1 && world.harvested.count(sp.uid)) n++;
    return n;
}
int Game::totemsTaken() const {
    int n = 0;
    for (const World::Special& sp : world.specials) if (sp.type == 0 && world.harvested.count(sp.uid)) n++;
    return n;
}
int Game::treasureTotal(int type) const { int n = 0; for (const World::Special& sp : world.specials) if (sp.type == type) n++; return n; }

// the bag: the bought / found slots + the orb milestones
int Game::bagCapacity() const {
    int orbs = orbsTaken();
    return std::min(200, invCap + (orbs >= 5 ? 10 : 0) + (orbs >= 15 ? 10 : 0));
}

Game::TreasureBonus Game::treasureBonus() const {
    TreasureBonus b;
    for (const World::Special& sp : world.specials) {
        if (sp.type != 0 || !world.harvested.count(sp.uid)) continue;
        std::string r = totemReward(sp.reward);
        if (r == "heart") b.hp += 15;
        else if (r == "stamina") b.stamina += 15;
    }
    int orbs = orbsTaken();
    if (orbs >= 10) b.hp += 20;
    if (orbs >= 20) b.atk += 0.1f;
    if (orbs >= 30) { b.stamina += 20; b.mp += 20; }
    return b;
}

void Game::itemGet(const std::string& icon, const std::string& title, const std::string& name, const std::string& desc, vec3 color, bool big) {
    ItemGetFx f; f.icon = icon; f.title = title; f.name = name; f.desc = desc; f.color = color; f.big = big;
    if (itemGets.size() < 8) itemGets.push_back(f);
}

// ---------------------------------------------------------------------------------------------- totems and orbs
void Game::collectTreasure(const NodeSpawn& ns) {
    if (world.harvested.count(ns.uid)) return;
    world.harvested[ns.uid] = 0;   // permanent (saved with the harvested nodes)
    Character& c = pc();
    auto it = nodeEntities.find(ns.uid);
    if (it != nodeEntities.end()) { reg.destroyLater(it->second); nodeEntities.erase(it); }
    c.vis.anim.play(&clip(C_INTERACT), 1.f);
    haptic(80, 220);
    if (ns.type == NodeType::Orb) {
        int n = orbsTaken(), tot = treasureTotal(1);
        shards += 2;
        particles.magicBurst(ns.pos, vec3(1.f, 0.85f, 0.4f), 1.2f, 40);
        play("discover", 0.9f, 1.15f);
        char b[96]; snprintf(b, sizeof b, "Orbe ancien %d / %d", n, tot);
        std::string desc = "+2 éclats d'étoile.";
        if (n == 5) desc = "Palier 5 : +10 emplacements dans le sac !";
        else if (n == 10) desc = "Palier 10 : +20 points de vie !";
        else if (n == 15) desc = "Palier 15 : +10 emplacements dans le sac !";
        else if (n == 20) desc = "Palier 20 : +10 % d'attaque !";
        else if (n == 25) { desc = "Palier 25 : vous apprenez Pluie d'étoiles !"; learnSpell("starstorm"); }
        else if (n == 30) desc = "Palier 30 : +20 endurance et +20 mana !";
        else if (n == 40) { desc = "Palier 40 : la Lame des Anciens est à vous !"; addItem("sword_ancients", 1, Rarity::Count, false); }
        else { int next = n < 5 ? 5 : n < 10 ? 10 : n < 15 ? 15 : n < 20 ? 20 : n < 25 ? 25 : n < 30 ? 30 : 40; if (n < 40) desc += " Prochain palier : " + std::to_string(next) + " orbes."; }
        itemGet("i_crystal", "Orbe ancien", b, desc, vec3(1.f, 0.85f, 0.4f), n % 5 == 0);
        recomputeStats();
        return;
    }
    // a totem of power wakes up
    std::string r = totemReward(ns.tier);
    shards += 5;
    particles.magicBurst(ns.pos + vec3(0, 3.f, 0), vec3(0.5f, 0.95f, 1.f), 2.2f, 90);
    particles.shockwave(ns.pos + vec3(0, 0.2f, 0), vec3(0.5f, 0.95f, 1.f), 6.f);
    addShake(0.25f);
    play("boss_intro", 0.5f, 1.4f); play("discover", 1.f);
    char head[96]; snprintf(head, sizeof head, "Totem de pouvoir %d / %d", totemsTaken(), treasureTotal(0));
    if (r.rfind("key:", 0) == 0) {
        std::string id = r.substr(4);
        const ItemDef* d = item(id);
        if (d) { if (countItem(id) == 0) addItem(id, 1, Rarity::Count, false); itemGet(d->icon, head, d->name, d->desc + " (+5 éclats)", rarityColor(d->rarity), true); }
    } else if (r.rfind("spell:", 0) == 0) {
        std::string id = r.substr(6);
        int si = spellIndex(id);
        if (si >= 0) { learnSpell(id); itemGet(spells()[si].icon, head, "Sort : " + spells()[si].name, spells()[si].desc + " Équipez-le dans le grimoire.", vec3(0.7f, 0.6f, 1.f), true); }
    } else if (r == "heart") itemGet("i_heart", head, "Cœur de totem", "+15 points de vie maximum, pour toujours. (+5 éclats)", vec3(1.f, 0.4f, 0.45f), true);
    else if (r == "stamina") itemGet("i_stamina", head, "Souffle de totem", "+15 endurance maximum, pour toujours. (+5 éclats)", vec3(0.5f, 1.f, 0.5f), true);
    else if (r == "skill") { skillPoints += 2; itemGet("i_skills", head, "Sagesse de totem", "+2 points de compétence. (+5 éclats)", vec3(1.f, 0.85f, 0.4f), true); }
    else { shards += 10; itemGet("i_crystal", head, "Trésor de totem", "+15 éclats d'étoile.", vec3(0.7f, 0.85f, 1.f), true); }
    recomputeStats();
    pc().hp = pc().maxHp; stamina = maxStamina;   // the totem restores you
    gainXp(120);
}

// ---------------------------------------------------------------------------------------------- per frame
void Game::updateTreasures(float dt) {
    if (player == NullEntity) return;
    Character& c = pc();
    // pop-ups
    if (!itemGets.empty()) {
        ItemGetFx& f = itemGets.front();
        if (f.t == 0.f) play(f.big ? "levelup" : "pickup", f.big ? 0.7f : 0.5f, f.big ? 1.f : 1.25f);
        f.t += dt;
        if (f.t > (f.big ? 4.2f : 2.1f)) itemGets.pop_front();
    }
    // the magnet ring pulls the loot from 10 m
    if (hasPower(KP_MAGNET)) {
        reg.each<Pickup>([&](Entity, Pickup& pk) {
            float d = distXZ(pk.pos, c.pos);
            if (pk.t > 0.4f && d < 10.f && d > 2.f) pk.pos = lerp(pk.pos, c.pos + vec3(0, 0.8f, 0), std::min(1.f, dt * 4.f));
        });
    }
    // the compass of the Ancients: nearest treasure still hidden
    compassTarget = -1;
    if (hasPower(KP_COMPASS) && !dungeon.active) {
        float best = 1e9f;
        for (int i = 0; i < (int)world.specials.size(); i++) {
            const World::Special& sp = world.specials[i];
            if (sp.type == 2 || world.harvested.count(sp.uid)) continue;
            float d = distXZ(sp.pos, c.pos);
            if (d < best) { best = d; compassTarget = i; }
        }
    }
}

// physics hook (with the flight): the feather cape glides while the jump button is held
void Game::treasurePhysics(Character& c, float dt) {
    gliding = false;
    if (!hasPower(KP_GLIDE) || c.grounded || c.swimming || flying || riding || c.dead) return;
    if (!input.jumpHeld || c.vel.y > -1.f || c.airTime < 0.25f) return;
    gliding = true;
    c.vel.y = std::max(c.vel.y, -2.4f) + 20.f * gameTuning().gravity * dt * 0.85f;   // most of the gravity is held off
    vec3 fwd{std::sin(c.yaw), 0, std::cos(c.yaw)};
    vec2 h{c.vel.x, c.vel.z};
    float sp = length(h);
    if (sp < 7.5f) { c.vel.x += fwd.x * dt * 6.f; c.vel.z += fwd.z * dt * 6.f; }
    fallSafeT = std::max(fallSafeT, 0.6f);
    if (particles.rng.f() < dt * 10.f) particles.magicTrail(c.pos + vec3(0, 1.2f, 0) - fwd * 0.4f, vec3(1.f, 0.95f, 0.85f), 0.4f);
}

// the phoenix feather: once a day, back on your feet
bool Game::tryPhoenix(Character& c) {
    if (!hasPower(KP_PHOENIX) || phoenixDay == day) return false;
    phoenixDay = day;
    c.hp = c.maxHp * 0.5f;
    particles.magicBurst(c.pos + vec3(0, 1.f, 0), vec3(1.f, 0.55f, 0.2f), 2.f, 80);
    particles.shockwave(c.pos, vec3(1.f, 0.5f, 0.2f), 5.f);
    play("magic_burst", 1.f, 0.8f);
    notify("La plume de phénix vous relève !", vec3(1.f, 0.6f, 0.3f), "i_heart");
    return true;
}

// ---------------------------------------------------------------------------------------------- HUD
void Game::drawTreasureHud() {
    UI& u = ui;
    const float W_ = u.W, H_ = u.H;
    // compass arrow under the quest tracker
    if (compassTarget >= 0 && compassTarget < (int)world.specials.size() && menu == Menu::None && !cine.active && player != NullEntity) {
        const World::Special& sp = world.specials[compassTarget];
        Character& p = pc();
        float bearing = std::atan2(sp.pos.x - p.pos.x, sp.pos.z - p.pos.z);
        float d = distXZ(sp.pos, p.pos);
        Rect r{W_ / 2 - 95, 82, 190, 34};
        u.nine("panel_plain", r, vec4(1, 1, 1, 0.7f), 0.5f);
        u.spriteRot("i_player_arrow", {r.x + 20, r.y + 17}, vec2(22, 22), minimapArrowAngle(bearing, camYaw), sp.type == 0 ? rgba(0x8af0ff) : rgba(0xffd870));
        char b[64]; snprintf(b, sizeof b, "%s  %s", sp.type == 0 ? "Totem" : "Orbe", d < 1000 ? (std::to_string((int)d) + " m").c_str() : (std::to_string((int)(d / 100) / 10.f).substr(0, 3) + " km").c_str());
        u.text(b, r.x + 38, r.y + 7, 16, sp.type == 0 ? rgba(0x8af0ff) : rgba(0xffd870), F_BOLD);
    }
    if (itemGets.empty()) return;
    const ItemGetFx& f = itemGets.front();
    float dur = f.big ? 4.2f : 2.1f;
    float in = smoothstepf(0.f, 0.22f, f.t), out = 1.f - smoothstepf(dur - 0.35f, dur, f.t);
    float a = in * out;
    vec4 col(f.color, 1);
    if (!f.big) {
        // a small card that rises above the action buttons: the icon pops, a ring of light, the name in its colour
        float w = std::max(300.f, u.textWidth(f.name, 20, F_BOLD) + 110), h = 66;
        Rect r{W_ / 2 - w / 2, H_ - 190 + (1 - in) * 40, w, h};
        u.nine("panel_plain", r, vec4(1, 1, 1, 0.88f * a), 0.55f);
        u.rect({r.x + 6, r.y + h - 6, (w - 12), 3}, vec4(f.color, 0.9f * a));
        float pop = 1.f + 0.35f * std::exp(-f.t * 7.f) * std::sin(f.t * 22.f);
        vec2 ic{r.x + 38, r.y + h / 2};
        u.sprite("glow", {ic.x - 34, ic.y - 34, 68, 68}, vec4(f.color, 0.5f * a));
        u.circle(ic, 24 + 14 * smoothstepf(0.f, 0.5f, f.t), vec4(f.color, 0.35f * a * (1 - smoothstepf(0.f, 0.5f, f.t))), 24);
        u.sprite(f.icon, {ic.x - 20 * pop, ic.y - 20 * pop, 40 * pop, 40 * pop}, vec4(1, 1, 1, a));
        u.text(f.title, r.x + 70, r.y + 8, 14, vec4(0.85f, 0.8f, 0.7f, a), F_BODY);
        u.text(f.name, r.x + 70, r.y + 28, 20, vec4(f.color * 1.1f, a), F_BOLD);
        return;
    }
    // the fanfare: a centred banner with rays behind the icon
    float w = std::min(W_ - 40, 640.f), h = 210;
    Rect r{W_ / 2 - w / 2, H_ * 0.2f + (1 - in) * 30, w, h};
    u.rect({0, 0, W_, H_}, vec4(0, 0, 0, 0.25f * a));
    u.nine("panel", r, vec4(1, 1, 1, 0.95f * a), 0.8f);
    vec2 ic{r.x + w / 2, r.y + 62};
    for (int i = 0; i < 12; i++) {   // rotating rays
        float ang = i * TAU / 12 + f.t * 0.6f;
        vec2 d{std::cos(ang), std::sin(ang)};
        float len = 36.f + 8.f * std::sin(f.t * 4 + i);
        u.spriteRot("bar_fill", ic + d * (34.f + len * 0.5f), vec2(len, 5.f), ang, vec4(f.color, 0.4f * a));
    }
    float pop = 1.f + 0.4f * std::exp(-f.t * 5.f) * std::sin(f.t * 18.f);
    u.sprite("glow", {ic.x - 60, ic.y - 60, 120, 120}, vec4(f.color, 0.6f * a));
    u.sprite(f.icon, {ic.x - 30 * pop, ic.y - 30 * pop, 60 * pop, 60 * pop}, vec4(1, 1, 1, a));
    u.text(f.title, r.x + w / 2, r.y + 104, 17, vec4(0.9f, 0.85f, 0.72f, a), F_SERIF, A_CENTER);
    u.text(f.name, r.x + w / 2, r.y + 126, 26, vec4(f.color * 1.15f, a), F_SERIF, A_CENTER);
    u.text(f.desc, r.x + w / 2, r.y + 162, 16, vec4(0.92f, 0.9f, 0.85f, a), F_BODY, A_CENTER, w - 60);
}

}  // namespace lk
