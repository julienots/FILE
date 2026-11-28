#include "Game.h"
#include <cstring>
#include "../engine/core/Platform.h"
#include <algorithm>

namespace lk {

static int xpForLevel(int l) { return (int)(100.f * std::pow((float)l, 1.55f)); }

// ============================================================================================ stats & visuals
void Game::recomputeStats() {
    if (player == NullEntity) return;
    Character& c = pc();
    float hpRatio = c.maxHp > 0 ? c.hp / c.maxHp : 1.f;
    float maxHp = 120 + 16.f * (level - 1), atk = 10 + 2.2f * (level - 1), def = 3 + 1.2f * level;
    float mpMax = 50 + 6.f * level, stMax = 100 + 2.f * level;
    float crit = 0.05f, cdmg = 1.6f, speed = 0.f, elemDmg = 0;
    Element elem = Element::Physical;
    float atkPct = 0;
    for (int i = 0; i < (int)Element::Count; i++) resist[i] = 0;
    int setCount[16] = {0};
    for (int s = 0; s < SL_COUNT; s++) {
        const ItemStack& st = equipment[s];
        if (st.id.empty()) continue;
        const ItemDef* d = item(st.id);
        if (!d) continue;
        float m = rarityMult(st.rarity) * (1.f + 0.12f * st.level);
        atk += d->atk * m; def += d->def * m; maxHp += d->hp * m; mpMax += d->mp * m; stMax += d->stamina * m;
        crit += d->crit; speed += d->speed;
        if (d->elemDmg > 0 && s == SL_WEAPON) { elem = d->element; elemDmg += d->elemDmg * m; }
        if (d->element != Element::Physical && s >= SL_HEAD && s <= SL_FEET) resist[(int)d->element] += 0.08f;
        if (d->set >= 0) setCount[d->set]++;
    }
    const auto& S = sets();
    for (int i = 0; i < (int)S.size(); i++) {
        if (setCount[i] >= 2) { atk += S[i].atk2; def += S[i].def2; maxHp += S[i].hp2; }
        if (setCount[i] >= 4) { crit += S[i].crit4; atk += S[i].atk4; mpMax += S[i].mp4; def += S[i].def4; speed += S[i].speed4; if (S[i].elemDmg4 > 0) { if (elem == Element::Physical) elem = S[i].elem4; elemDmg += S[i].elemDmg4; } }
    }
    for (const auto& sk : skills) {
        int i = skillIndex(sk);
        if (i < 0) continue;
        const SkillNode& n = skillTree()[i];
        atkPct += n.atk; def += n.def; maxHp += n.hp; mpMax += n.mp; stMax += n.stamina; crit += n.crit; cdmg += n.critDmg; speed += n.speed;
    }
    {   // 3.2.0 totems of power, ancient orbs, the wind hoof
        TreasureBonus tb = treasureBonus();
        maxHp += tb.hp; stMax += tb.stamina; mpMax += tb.mp; atkPct += tb.atk;
        if (hasPower(KP_SWIFT)) speed += 0.1f;
    }
    c.maxHp = maxHp; c.hp = std::min(maxHp, hpRatio * maxHp);
    c.atk = atk * (1 + atkPct); c.def = def; c.element = elem; c.elemDmg = elemDmg;
    maxMp = mpMax; mp = std::min(mp, maxMp); maxStamina = stMax; stamina = std::min(stamina, maxStamina);
    critChance = std::min(0.75f, crit); critDmg = cdmg; moveSpeedMul = (1.f + speed) * gameTuning().playerSpeed;
    // hero of the wheel
    const HeroDef& hd = hero();
    c.maxHp *= hd.hpMul; c.hp = std::min(c.maxHp, hpRatio * c.maxHp); c.atk *= hd.atkMul; moveSpeedMul *= hd.speedMul;
}

float skillBonus(const std::set<std::string>& skills, float SkillNode::*field) {
    float v = 0;
    for (const auto& s : skills) { int i = skillIndex(s); if (i >= 0) v += skillTree()[i].*field; }
    return v;
}

float skillBonusPublic(const std::set<std::string>& s, int which) {
    return skillBonus(s, which == 0 ? &SkillNode::bowDmg : (which == 1 ? &SkillNode::daggerDmg : &SkillNode::petDmg));
}

void Game::rebuildPlayerVisual() {
    if (player == NullEntity) return;
    std::string eq[SL_COUNT];
    for (int i = 0; i < SL_COUNT; i++) eq[i] = equipment[i].id;
    buildHeroLook(pc(), activeHero, eq, dye);
}

// 1.13.0 the hero's look from a hero index and equipment ids: the player, and the co-op partner's puppet
void Game::buildHeroLook(Character& c, int heroIdx, const std::string* eq, const std::string& dyeId) {
    HumanoidLook L;
    L.hair = Hair::Ponytail; L.hairCol = hexColor(0x3a2418); L.skin = hexColor(0xd9a98a); L.eyes = hexColor(0x3a6a8a);
    L.c1 = hexColor(0x5a3a2a); L.c2 = hexColor(0x3a3028); L.c3 = hexColor(0x2a2018); L.seed = 7;
    L.body = Body::Tunic; L.belt = true; L.satchel = true;
    const ItemDef* head = item(eq[SL_HEAD]);
    const ItemDef* chest = item(eq[SL_CHEST]);
    const ItemDef* hands = item(eq[SL_HANDS]);
    const ItemDef* feet = item(eq[SL_FEET]);
    if (head) L.helm = head->helm;
    if (chest) { L.body = chest->body; L.c1 = chest->color; L.pauldrons = chest->pauldrons; L.cape = chest->cape; if (chest->body == Body::Plate) L.metal = lerp(hexColor(0xa8aab0), chest->color, 0.35f); }
    if (hands) L.gauntlets = hands->gauntlets || hands->tier >= 2;
    if (feet) { L.greaves = feet->greaves; L.c3 = lerp(hexColor(0x2a2018), feet->color, 0.4f); }
    if (!dyeId.empty()) { const ItemDef* d = item(dyeId); if (d) { L.c1 = d->color; } }
    if (chest && chest->set >= 0 && chest->set == 6) L.trim = hexColor(0xf0d080);
    L.cape = L.cape || (chest && chest->tier >= 3);
    const ItemDef* w = item(eq[SL_WEAPON]);
    const ItemDef* o = item(eq[SL_OFFHAND]);
    WeaponType wt = w ? w->weapon : WeaponType::None;
    WeaponType ot = o && wt != WeaponType::Bow && wt != WeaponType::Longsword && wt != WeaponType::Hammer && wt != WeaponType::Spear && wt != WeaponType::Staff ? o->weapon : WeaponType::None;
    // hero wheel: Kaede / Astra have their own look and fighting style (the gear still gives the stats)
    const HeroDef& hd = heroes()[std::max(0, std::min((int)heroes().size() - 1, heroIdx))];
    bool altHero = heroIdx > 0;
    if (altHero) { L = hd.look; if (hd.fixedWeapon) { wt = hd.weapon; ot = hd.weapon == WeaponType::Dagger ? WeaponType::Dagger : WeaponType::None; } }
    Animator keep = c.vis.anim;
    bool had = c.vis.mesh != nullptr;
    setupVisualHumanoid(c.vis, L, wt, ot, 1.f);
    if (altHero && hd.fixedWeapon) {
        int tier = w ? w->tier : 1;
        if (wt != WeaponType::None) c.vis.weapon = weaponMesh(wt, tier, hd.color, &c.vis.tip); else c.vis.weapon.reset();
        if (ot != WeaponType::None) c.vis.offhand = weaponMesh(ot, tier, hd.color); else c.vis.offhand.reset();
        w = nullptr; o = nullptr;
    }
    else if (w) c.vis.weapon = weaponMesh(wt, w->tier, elementColor(w->element), &c.vis.tip);
    if (o && ot != WeaponType::None) c.vis.offhand = weaponMesh(ot, o->tier, o->color);
    if (had) { int st = c.vis.anim.stance; c.vis.anim = keep; c.vis.anim.stance = st; }
    c.vis.albedo = 0;
    c.vis.anim.skeleton = &skeleton(Arch::Humanoid);
    if (altHero) {
        // the hero's imported model when provided (assets/models/hero_ninja.lkm, hero_flyer.lkm), else procedural
        if (!hd.model.empty()) applyImportedModel(c.vis, hd.model);
    } else if (useHeroModel()) {
        // imported model: fitted skeleton (same bones, so every animation plays), own cape in the mesh
        applyImportedModel(c.vis, "hero");
    }
    // 1.6.2 gait matched to the hero's real speeds (walk 2.1, jog 4.9, sprint 7.2 m/s): ~155 steps/min walking,
    // ~195 running, long bounds when sprinting (it used to scurry at 280-350 steps/min)
    c.vis.anim.gait.strideWalk = 1.65f; c.vis.anim.gait.strideRun = 3.0f;
    c.vis.anim.gait.walkSpeed = 2.1f; c.vis.anim.gait.runSpeed = 4.9f; c.vis.anim.gait.sprintSpeed = 7.2f;
    c.vis.anim.gait.ampWalk = 1.15f; c.vis.anim.gait.ampRun = 1.1f; c.vis.anim.gait.sprintStride = 0.45f;
    c.vis.capeColor = L.c1 * 0.8f;
    c.vis.rim = 0.65f;
    c.vis.outlineColor = vec4(0.05f, 0.04f, 0.03f, 1);
    c.vis.trailColor = w ? elementColor(w->element) : vec3(1, 0.9f, 0.7f);
}

void Game::gainXp(int amount) {
    amount = std::max(1, (int)std::round(amount * gameTuning().xp));   // 2.6.0 mods
    xp += amount;
    while (xp >= xpForLevel(level)) {
        xp -= xpForLevel(level);
        level++;
        skillPoints++;
        recomputeStats();
        Character& c = pc();
        c.hp = c.maxHp; mp = maxMp; stamina = maxStamina;
        particles.levelUp(c.pos);
        play("levelup");
        haptic(80, 160);
        levelUpT = 3.f;
        char b[64]; snprintf(b, sizeof b, "Niveau %d ! +1 point de compétence", level);
        notify(b, vec3(1.f, 0.85f, 0.35f), "i_star");
    }
}

// ============================================================================================ actions
static const Clip* lightClip(int stance, int step) {
    switch (stance) {
    case ST_ONEHAND: return &clip(step == 0 ? C_1H_A1 : step == 1 ? C_1H_A2 : C_1H_A3);
    case ST_TWOHAND: return &clip(step == 0 ? C_2H_A1 : step == 1 ? C_2H_A2 : C_2H_A3);
    case ST_DAGGER: return &clip(step == 0 ? C_DG_A1 : step == 1 ? C_DG_A2 : C_DG_A3);
    case ST_SPEAR: return &clip(step % 2 == 0 ? C_SP_A1 : C_SP_A2);
    case ST_BOW: return &clip(C_BOW_SHOT);
    case ST_STAFF: return &clip(C_STAFF_CAST);
    case ST_TOOL: return &clip(C_GATHER_SWING);
    default: return &clip(step == 0 ? C_FIST_A1 : step == 1 ? C_FIST_A2 : C_FIST_A3);   // jab, hook, uppercut
    }
}
static const Clip* heavyClip(int stance) {
    switch (stance) {
    case ST_ONEHAND: return &clip(C_1H_HEAVY);
    case ST_TWOHAND: return &clip(C_2H_HEAVY);
    case ST_DAGGER: return &clip(C_DG_HEAVY);
    case ST_SPEAR: return &clip(C_SP_HEAVY);
    case ST_BOW: return &clip(C_BOW_HEAVY);
    case ST_STAFF: return &clip(C_STAFF_HEAVY);
    default: return &clip(C_FIST_HEAVY);   // spinning roundhouse kick
    }
}

void Game::playerAttack(bool heavy, bool charged) {
    Character& c = pc();
    if (c.dead || riding || c.swimming || c.hitStun > 0) return;
    if (digMode && inCellar()) digMode = false;   // 3.6.0 in your cellar the pickaxe cuts the rock face (no surface digging)
    // 2.4.0 dig mode: the sword button digs; a charged press throws a bomb (if you carry one)
    if (digMode && !flying) {
        if (charged && countItem("bomb") > 0) { removeItem("bomb", 1); throwBomb(); }
        else playerDig();
        return;
    }
    if (flying) {   // Astra: attacking from the sky is a dive punch
        flying = false; meteorPending = true; meteorPower = c.atk * (heavy || charged ? 1.8f : 1.2f);
        c.vel.y = -26.f; c.invuln = 0.6f;
        Entity t = lockTarget != NullEntity ? lockTarget : findTarget(c.pos, c.yaw, 18.f, Team::Enemy, -0.2f);
        if (Character* tc = t != NullEntity ? reg.get<Character>(t) : nullptr) { vec3 d = tc->pos - c.pos; d.y = 0; c.vel.x = d.x * 1.4f; c.vel.z = d.z * 1.4f; }
        play("swing_heavy", 0.9f, 0.8f);
        return;
    }
    Animator& a = c.vis.anim;
    if (c.attackClip && !(a.actionTime() >= c.attackClip->comboOpen)) { attackBuf = 0.45f; attackBufHeavy = heavy; return; }  // buffered
    float cost = heavy ? 18.f : 6.f;
    if (stamina < cost * 0.5f) { static float lastTired = -10; if (realTime - lastTired > 1.5f) { floatText(c.pos + vec3(0, 2.2f, 0), "Épuisé", vec3(0.9f, 0.8f, 0.3f), 18); lastTired = realTime; } return; }
    stamina -= cost; staminaRegenDelay = 0.8f;
    int stance = a.stance;
    const Clip* cl;
    float mult = 1.f;
    if (heavy || charged) { cl = (charged && stance == ST_ONEHAND) ? &clip(C_1H_CHARGED) : heavyClip(stance); mult = charged ? 2.4f : 1.8f; comboStep = 0; }
    else {
        cl = lightClip(stance, comboStep);
        mult = comboStep == 2 ? 1.5f : 1.f;
        comboStep = (comboStep + 1) % (stance == ST_SPEAR ? 2 : 3);
    }
    comboTimer = 1.1f;
    // auto-aim: face the best target
    Entity t = lockTarget != NullEntity ? lockTarget : findTarget(c.pos, c.yaw, stance == ST_BOW || stance == ST_STAFF ? 22.f : 6.f, Team::Enemy, 0.2f);
    if (t != NullEntity && settings.autoTarget) {
        Character* tc = reg.get<Character>(t);
        if (tc) c.yaw = std::atan2(tc->pos.x - c.pos.x, tc->pos.z - c.pos.z);
    }
    float spd = 1.f;
    if (const ItemDef* w = item(equipment[SL_WEAPON].id)) spd = 1.f + w->speed;
    startAttack(player, c, cl, mult, c.element, spd);
    play(heavy ? "swing_heavy" : "swing", 0.8f, particles.rng.range(0.9f, 1.1f));
    if (t == NullEntity && stance != ST_BOW && stance != ST_STAFF && !mineCellar(c)) tryChopTree(c);   // 3.6.0 a swing at a tree, or at the rock of your cellar
    // 3.0.1 heavy and charged blows leave a glowing crescent in the air (the weapon's element, or steel)
    if ((heavy || charged || comboStep == 0) && stance != ST_BOW && stance != ST_STAFF && stance != ST_UNARMED) {
        vec3 ec = c.element == Element::Physical ? vec3(0.75f, 0.85f, 1.f) : elementColor(c.element);
        float r = stance == ST_TWOHAND ? 2.8f : stance == ST_SPEAR ? 3.2f : 2.3f;
        vec3 at = c.pos + vec3(0, 1.1f, 0) + vec3(std::sin(c.yaw), 0, std::cos(c.yaw)) * 0.3f;
        vfxSlash(at, c.yaw, charged ? 0.f : (heavy ? -0.5f : 0.35f), ec, r * (charged ? 1.3f : 1.f), charged ? 0.38f : 0.3f);
        if (charged) { vfxShockwave(c.pos, ec, 4.f, 0.5f); vfxSlash(at, c.yaw, 1.2f, lerp(ec, vec3(1.f), 0.4f), r * 1.1f, 0.42f); }
    }
}

void Game::playerDodge() {
    Character& c = pc();
    // 2.4.0 in the water the dodge button dives (and surfaces again)
    if (c.swimming && !c.dead) {
        if (dodgeCD > 0) return;
        dodgeCD = 0.5f;
        float wl = waterAt(c.pos.x, c.pos.z), gy = groundAt(c.pos.x, c.pos.z);
        if (!diving) {
            if (wl - gy < 2.4f) { floatText(c.pos + vec3(0, 2.f, 0), "Trop peu profond", vec3(0.7f, 0.85f, 1.f), 18); return; }
            diving = true; particles.splash(c.pos + vec3(0, 1.2f, 0), 0.8f); play3D("splash", c.pos, 0.7f, 0.85f);
        } else { diving = false; play3D(variant("step_water"), c.pos, 0.5f); }
        return;
    }
    if (c.dead || riding || c.swimming || dodgeCD > 0 || !c.grounded) return;
    float cost = 22.f * (1.f - skillBonus(skills, &SkillNode::staminaCost));
    if (stamina < cost) { floatText(c.pos + vec3(0, 2.2f, 0), "Épuisé", vec3(0.9f, 0.8f, 0.3f), 18); return; }
    stamina -= cost; staminaRegenDelay = 0.9f;
    vec3 dir = c.vel; dir.y = 0;
    if (length(input.move) > 0.2f) {
        vec3 fwd{-std::sin(camYaw), 0, -std::cos(camYaw)}, right{std::cos(camYaw), 0, -std::sin(camYaw)};
        dir = right * input.move.x + fwd * input.move.y;
    }
    bool back = length2(dir) < 0.04f;
    if (!back) c.yaw = std::atan2(dir.x, dir.z);
    c.attackClip = nullptr; c.hitThisSwing.clear();
    const Clip* cl = &clip(back ? C_DODGE_BACK : C_ROLL);
    c.vis.anim.play(cl, 1.1f, false, 0.05f);
    c.attackClip = cl; c.attackIdx = -2; c.lastMotion = 0;   // movement action (root motion), no hit window
    c.invuln = back ? 0.3f : 0.45f;
    dodgeCD = 0.35f;
    play("roll", 0.7f);
    particles.dust(c.pos, 1.2f);
    // 3.10.5 perfect dodge: a creature close by was about to strike -> the time freezes around the hero
    if (perfectDodgeCheck()) {
        frozenTimeT = 2.2f;
        c.invuln = std::max(c.invuln, 0.6f);
        stamina = std::min(maxStamina, stamina + cost);   // the dodge is free
        floatText(c.pos + vec3(0, 2.4f, 0), "Esquive parfaite !", vec3(1.f, 0.78f, 0.32f), 26);
        particles.shockwave(c.pos, vec3(1.f, 0.72f, 0.28f), 5.f);
        play("ice", 0.7f, 0.6f); play("magic_burst", 0.5f, 0.7f);
        haptic(60, 180); hitStop = std::max(hitStop, 0.06f);
    }
}

bool Game::perfectDodgeCheck() {
    if (player == NullEntity || frozenTimeT > 0.f) return false;
    Character& p = pc();
    bool hit = false;
    reg.each<AIBrain, Character>([&](Entity, AIBrain& ai, Character& o) {
        if (hit || o.dead || o.team != Team::Enemy || ai.target != player) return;
        float d = distXZ(o.pos, p.pos);
        if (d > 5.f + o.radius) return;
        // the last instants of the wind-up (red outline), or a swing before its hit window ends
        bool windup = ai.state == AIState::Attack && ai.telegraphT > 0.f && ai.telegraphT < 0.4f;
        bool swing = o.attackClip && o.vis.anim.action() == o.attackClip && o.attackClip->hitStart >= 0 && o.vis.anim.actionTime() < o.attackClip->hitEnd;
        if (windup || swing) hit = true;
    });
    return hit;
}

bool Game::playerJump(bool buffered) {
    Character& c = pc();
    if (c.dead || riding) return true;
    if (!buffered && heroJump()) return true;   // double jump, take off / land (hero wheel)
    // 1.11.2 "coyote time": a jump pressed just after running off an edge still counts
    bool coyote = !c.grounded && !c.swimming && c.airTime < 0.13f && c.vel.y <= 0.5f && !flying;
    if ((!c.grounded && !coyote) || c.attackClip) return false;
    if (c.swimming) {
        // 2.4.0 jump: back to the surface when diving; at the surface, near the shore, climb out onto the bank
        if (diving) { diving = false; return true; }
        vec3 fwd{std::sin(c.yaw), 0, std::cos(c.yaw)};
        float wl = waterAt(c.pos.x, c.pos.z);
        for (float dd : {1.f, 1.6f, 2.2f}) {
            vec3 q = c.pos + fwd * dd;
            float g = groundAt(q.x, q.z);
            if (g > wl - 0.6f && g < wl + 1.6f) {   // a bank within reach: hop onto it
                c.pos = vec3(q.x, g + 0.05f, q.z); c.vel = fwd * 2.5f; c.vel.y = 0; c.swimming = false; c.grounded = true;
                c.vis.anim.play(&clip(C_LAND), 1.1f, false, 0.05f);
                particles.splash(c.pos, 0.7f); play3D("splash", c.pos, 0.5f, 1.2f);
                return true;
            }
        }
        particles.splash(c.pos + vec3(0, 1.2f, 0), 0.5f);
        return true;
    }
    // vault over low climbable obstacles in front
    vec3 fwd{std::sin(c.yaw), 0, std::cos(c.yaw)};
    vec3 probe = c.pos + fwd * 1.0f;
    float step = 0;
    vec3 tmp = probe;
    if (collideStatic(tmp, 0.3f, c.pos.y, &step) && stamina > 12) {
        Chunk* ch = world.chunkAt(probe.x, probe.z);
        if (ch) for (auto& col : ch->colliders) {
            if (!col.climb) continue;
            if (length(col.c - probe.xz()) < std::max(col.hx, col.hz) + 1.f && col.base + col.top - c.pos.y < 2.2f) {
                c.vis.anim.play(&clip(C_VAULT), 1.f, false, 0.06f);
                c.attackClip = &clip(C_VAULT); c.attackIdx = -2; c.lastMotion = 0;
                c.vel.y = 5.5f; c.grounded = false; stamina -= 12;
                play("jump", 0.6f);
                return true;
            }
        }
    }
    if (stamina < 8) return true;
    stamina -= 8; staminaRegenDelay = 0.5f;
    // 1.11.2 a running jump goes higher and further, and keeps the sprint speed in the air
    float hs = length(vec2(c.vel.x, c.vel.z));
    bool fast = hs > 5.8f;
    c.vel.y = (fast ? 7.1f : 6.4f) * gameTuning().jump;   // 2.7.0 mods: jump
    if (fast) { c.vel.x *= 1.08f; c.vel.z *= 1.08f; sprintJump = true; particles.dust(c.pos, 0.8f); }
    c.grounded = false;
    play("jump", 0.6f, fast ? 0.92f : 1.f);
    return true;
}

void Game::usePotion() {
    if (potionCD > 0) return;
    static const char* order[] = {"potion_great", "potion_heal", "potion_minor", "stew", "fish_grilled", "bread", "berries"};
    for (const char* id : order) {
        for (int i = 0; i < (int)inventory.size(); i++)
            if (inventory[i].id == id) { useItem(i); potionCD = 1.2f; return; }
    }
    notify("Aucune potion de soin", vec3(1, 0.6f, 0.5f), "i_potion");
}

void Game::castSpell(int slot) {
    Character& c = pc();
    int si = equippedSpells[slot];
    if (si < 0 || c.dead || riding || c.swimming) return;
    const SpellDef& s = spells()[si];
    if (spellCD[si] > 0) return;
    if (mp < s.mana) { floatText(c.pos + vec3(0, 2.2f, 0), "Mana insuffisant", vec3(0.5f, 0.6f, 1.f), 18); play("ui_close", 0.4f); return; }
    mp -= s.mana;
    spellCD[si] = s.cooldown;
    float power = c.atk * s.power * (1.f + skillBonus(skills, &SkillNode::spellDmg) + level * 0.02f);
    vec3 col = elementColor(s.school);
    vec3 fwd{std::sin(c.yaw), 0, std::cos(c.yaw)};
    Entity t = lockTarget != NullEntity ? lockTarget : findTarget(c.pos, c.yaw, 24.f, Team::Enemy, 0.0f);
    Character* tc = t != NullEntity ? reg.get<Character>(t) : nullptr;
    if (tc) { c.yaw = std::atan2(tc->pos.x - c.pos.x, tc->pos.z - c.pos.z); fwd = vec3(std::sin(c.yaw), 0, std::cos(c.yaw)); }
    vec3 hand = c.pos + vec3(0, 1.4f, 0) + fwd * 0.6f;
    static const char* snd[] = {"", "fireball", "ice", "lightning", "nature", "shadow", "holy", "arcane"};
    play(snd[(int)s.school], 0.9f);
    switch (s.kind) {
    case SpellKind::Projectile: {
        c.vis.anim.play(&clip(C_CAST_FORWARD), 1.3f, true);
        vfxCircle(hand + fwd * 0.3f, col, 0.8f, 0.5f, NullEntity, fwd);   // 3.0.1 a rune opens before the hand
        if (s.id == "missiles") {
            for (int i = 0; i < 3; i++) fireProjectile(player, ProjKind::Arcane, hand + perpVec(fwd) * (i - 1) * 0.4f, normalize(fwd + perpVec(fwd) * (i - 1) * 0.3f + vec3(0, 0.1f, 0)), 16.f, power, s.school, Team::Player, t);
        } else {
            ProjKind k = s.school == Element::Fire ? ProjKind::Fireball : ProjKind::IceSpike;
            vec3 dir = tc ? normalize(tc->pos + vec3(0, tc->height * 0.5f, 0) - hand) : fwd;
            fireProjectile(player, k, hand, dir, s.school == Element::Ice ? 26.f : 20.f, power, s.school, Team::Player);
        }
        break;
    }
    case SpellKind::Area: {
        c.vis.anim.play(&clip(C_CAST_AREA), 1.2f, true);
        vec3 center = s.id == "flamewave" ? c.pos + fwd * 3.5f : c.pos;
        areaDamage(player, center, s.radius, power, s.school, Team::Player, 30);
        particles.shockwave(center, col, s.radius);
        // 3.0.1 the circle of the spell, its wave, and a shape per school
        vfxCircle(center, col, s.radius, 1.2f);
        vfxShockwave(center, col, s.radius * 1.05f, 0.75f);
        if (s.school == Element::Fire) vfxGround(center, vec3(1.f, 0.6f, 0.15f), vec3(0.8f, 0.15f, 0.02f), s.radius * 0.9f, 2.2f);
        else if (s.school == Element::Ice) vfxSphere(center, vec3(0.75f, 0.95f, 1.f), vec3(0.3f, 0.6f, 1.f), s.radius * 0.35f, 0.9f, 0, 1.8f);
        else if (s.school == Element::Nature) vfxVortex(center, col, s.radius * 0.5f, 3.5f, 1.2f);
        else vfxRays(center, col, s.radius, 0.8f);
        particles.magicBurst(center + vec3(0, 0.8f, 0), col, 2.f, 40);
        if (s.id == "flamewave") for (int i = 0; i < 6; i++) particles.explosion(c.pos + fwd * (1.5f + i * 0.9f) + perpVec(fwd) * particles.rng.range(-1.5f, 1.5f), 0.4f, col);
        if (s.id == "frostnova") particles.iceShards(c.pos + vec3(0, 0.5f, 0), 30);
        if (s.id == "roots") reg.each<Character>([&](Entity, Character& e) { if (e.team == Team::Enemy && !e.dead && distXZ(e.pos, center) < s.radius) e.status.root = s.duration; });
        if (s.id == "frostnova") reg.each<Character>([&](Entity, Character& e) { if (e.team == Team::Enemy && !e.dead && distXZ(e.pos, center) < s.radius) e.status.freeze = s.duration; });
        if (s.id == "arcanenova") reg.each<Character>([&](Entity, Character& e) { if (e.team == Team::Enemy && !e.dead && distXZ(e.pos, center) < s.radius) e.knock += normalize(e.pos - center) * (14.f / std::max(1.f, e.mass)); });
        addShake(0.4f);
        break;
    }
    case SpellKind::Self: {
        c.vis.anim.play(&clip(C_CAST_SELF), 1.2f, true);
        float healMul = 1.f + skillBonus(skills, &SkillNode::heal);
        vfxCircle(c.pos, col, 2.2f, 1.3f, player);                   // 3.0.1 healing light: a circle at the feet, a column of light
        vfxPillar(c.pos, col, 1.1f, 5.f, 1.2f);
        vfxSphere(c.pos + vec3(0, 1.f, 0), lerp(col, vec3(1.f), 0.4f), col, 1.1f, 1.2f, 0, 0.3f, player);
        if (s.id == "regrowth") { c.status.regen = s.duration; particles.heal(c.pos, col); }
        else { c.hp = std::min(c.maxHp, c.hp + power * 1.2f * healMul); particles.heal(c.pos, col); areaDamage(player, c.pos, s.radius, power, s.school, Team::Player, 20); particles.shockwave(c.pos, col, s.radius); floatText(c.pos + vec3(0, 2.4f, 0), "+" + std::to_string((int)(power * 1.2f * healMul)), vec3(0.4f, 1.f, 0.5f)); }
        break;
    }
    case SpellKind::Shield: c.vis.anim.play(&clip(C_CAST_SELF), 1.2f, true); c.status.shield = c.maxHp * 0.35f; particles.magicBurst(c.pos + vec3(0, 1, 0), col, 1.5f, 30);
        vfxSphere(c.pos + vec3(0, 1.f, 0), lerp(col, vec3(1.f), 0.3f), col, 1.35f, 6.f, 0, 0.f, player);   // 3.0.1 a visible barrier around the hero
        vfxCircle(c.pos, col, 1.8f, 1.f, player);
        break;
    case SpellKind::Dash: {
        vec3 from = c.pos;
        vec3 to = c.pos + fwd * s.radius;
        vec3 tmp = to; collideStatic(tmp, c.radius, c.pos.y);
        to = tmp; to.y = groundAt(to.x, to.z);
        for (int i = 0; i < 10; i++) particles.magicTrail(lerp(from, to, i / 9.f) + vec3(0, 1, 0), col, 1.5f);
        vfxVortex(from, col, 1.f, 2.4f, 0.6f);                                          // 3.0.1
        vfxBeam(from + vec3(0, 1.f, 0), to + vec3(0, 1.f, 0), col, 0.3f, 0.35f);
        vfxShockwave(to, col, 3.f, 0.5f);
        reg.each<Character>([&](Entity e, Character& o) {
            if (o.team != Team::Enemy || o.dead) return;
            float tt; float d = distToSegment(o.pos.xz(), from.xz(), to.xz(), &tt);
            if (d < 1.5f) dealDamage(player, e, power, s.school, fwd, 40, true, true);
        });
        c.pos = to; c.invuln = 0.4f;
        break;
    }
    case SpellKind::Chain: {
        c.vis.anim.play(&clip(C_CAST_FORWARD), 1.3f, true);
        Entity cur = t; vec3 prev = hand; std::vector<Entity> hit;
        for (int j = 0; j < 4 && cur != NullEntity; j++) {
            Character* cc = reg.get<Character>(cur);
            if (!cc) break;
            particles.lightningArc(prev, cc->pos + vec3(0, cc->height * 0.6f, 0), col);
            vfxBeam(prev, cc->pos + vec3(0, cc->height * 0.6f, 0), col, 0.12f, 0.35f);            // 3.0.1
            vfxSphere(cc->pos + vec3(0, cc->height * 0.6f, 0), lerp(col, vec3(1.f), 0.5f), col, 0.6f, 0.4f, 1, 1.2f);
            dealDamage(player, cur, power * (1.f - j * 0.15f), s.school, normalize(cc->pos - prev), 25, true, true);
            hit.push_back(cur);
            prev = cc->pos + vec3(0, cc->height * 0.6f, 0);
            Entity next = NullEntity; float best = s.radius * s.radius;
            reg.each<Character>([&](Entity e, Character& o) {
                if (o.team != Team::Enemy || o.dead || std::find(hit.begin(), hit.end(), e) != hit.end()) return;
                float d = length2(o.pos - cc->pos);
                if (d < best) { best = d; next = e; }
            });
            cur = next;
        }
        if (hit.empty()) particles.lightningArc(hand, hand + fwd * 8.f, col);
        break;
    }
    case SpellKind::Rain: {
        c.vis.anim.play(&clip(C_CAST_AREA), 1.2f, true);
        for (int i = 0; i < 6; i++) {
            vec2 o = particles.rng.inDisk() * s.radius;
            vec3 p = c.pos + vec3(o.x, 0, o.y);
            particles.lightningArc(p + vec3(0, 12, 0), p, col);
            vfxPillar(p, col, 0.35f, 14.f, 0.5f);                                       // 3.0.1 each bolt is a column of light
            vfxShockwave(p, col, 2.2f, 0.45f);
        }
        vfxCircle(c.pos, col, s.radius, 1.3f);
        areaDamage(player, c.pos, s.radius, power, s.school, Team::Player, 50);
        particles.shockwave(c.pos, col, s.radius);
        env.lightning = 0.6f; lightningFlash = 0.6f;
        addShake(0.6f);
        break;
    }
    case SpellKind::Beam: {
        c.vis.anim.play(&clip(C_CAST_FORWARD), 1.0f, true);
        if (tc) {
            particles.lightningArc(hand, tc->pos + vec3(0, tc->height * 0.5f, 0), col);
            vfxBeam(hand, tc->pos + vec3(0, tc->height * 0.5f, 0), col, 0.25f, 0.45f);   // 3.0.1
            float dmg = dealDamage(player, t, power, s.school, fwd, 10, true, true);
            c.hp = std::min(c.maxHp, c.hp + dmg * 0.5f);
            particles.heal(c.pos, col);
        }
        break;
    }
    }
    haptic(25, 90);
}

void Game::toggleMount() {
    Character& c = pc();
    if (riding) {
        riding = false;
        c.vis.tiltPitch = c.vis.tiltRoll = c.vis.rideY = 0; for (float& r : c.vis.anim.ride) r = 0; rideBob = 0;   // 3.8.1
        if (reg.valid(mountEnt)) if (Character* mm = reg.get<Character>(mountEnt)) { mm->vis.tiltPitch = mm->vis.tiltRoll = mm->vis.rideY = 0; mm->vis.anim.ride[5] = 0; }
        c.vis.anim.setState(BodyState::Locomotion, 0.25f);
        c.vis.anim.play(&clip(C_DISMOUNT), 1.f);
        if (reg.valid(mountEnt)) {
            Character* m = reg.get<Character>(mountEnt);
            if (m) c.pos = m->pos + perpVec(vec3(std::sin(m->yaw), 0, std::cos(m->yaw))) * 1.4f;
        }
        c.pos.y = groundAt(c.pos.x, c.pos.z);
        return;
    }
    if (activeMount < 0) {
        if (ownedMounts.empty()) { notify("Vous ne possédez pas de monture. Odo vend des chevaux à Brindlemere.", vec3(1, 0.8f, 0.6f), "i_mount"); return; }
        activeMount = mounts().empty() ? -1 : 0;
        for (int i = 0; i < (int)mounts().size(); i++) if (ownedMounts.back() == mounts()[i].id) activeMount = i;
    }
    if (dungeon.active) { notify("Impossible d'appeler une monture ici.", vec3(1, 0.7f, 0.6f)); return; }
    if (!reg.valid(mountEnt)) {
        vec3 sp = c.pos - vec3(std::sin(c.yaw), 0, std::cos(c.yaw)) * 6.f;
        sp.y = groundAt(sp.x, sp.z);
        mountEnt = spawnMount(activeMount, sp);
        play("whistle", 0.8f); play3D("horse", sp, 0.8f);
        particles.dust(sp, 2.f);
    }
    Character* m = reg.get<Character>(mountEnt);
    if (m && distXZ(m->pos, c.pos) >= 3.5f) { mountCalled = true; play("whistle", 0.6f); return; }  // the mount trots over and you hop on
    if (m && distXZ(m->pos, c.pos) < 3.5f) {
        mountCalled = false;
        riding = true;
        c.vis.anim.setState(BodyState::Mounted, 0.3f);
        c.vis.anim.play(&clip(C_MOUNT), 1.2f);
        m->yaw = c.yaw;
    }
}

// ============================================================================================ interaction
void Game::findInteractTarget() {
    Character& c = pc();
    Entity best = NullEntity;
    float bestScore = 1e9f;
    vec3 fwd{std::sin(c.yaw), 0, std::cos(c.yaw)};
    reg.each<Interact>([&](Entity e, Interact& it) {
        vec3 p;
        if (Character* o = reg.get<Character>(e)) { if (o->dead || e == player || !o->vis.visible) return; p = o->pos; if (e == mountEnt && riding) return; }
        else if (NodeComp* n = reg.get<NodeComp>(e)) { p = n->node.pos; if (n->node.type == NodeType::Chest && openedChests.count(n->node.uid)) return; if (n->node.type != NodeType::Chest && n->node.type != NodeType::FishingSpot && !world.nodeAvailable(n->node.uid, gameTime)) return; }
        else if (Pickup* pk = reg.get<Pickup>(e)) p = pk->pos;
        else if (e == cellarEnt) { if (!cellar.owned || dungeon.active) return; p = cellar.hatch; }   // 3.6.0
        else if (AmberSpot* as = reg.get<AmberSpot>(e)) { if (as->kind == 1 && (!amber || loreFound.count("ambre_" + std::to_string(as->idx + 1)))) return; p = as->pos; }   // 3.10.0
        else if (VehicleRef* vr = reg.get<VehicleRef>(e)) { if (driving >= 0 || vr->idx >= (int)vehicles.size() || !vehicleUnlocked(vr->idx)) return; p = vehicles[vr->idx].pos; }
        else return;
        float d = distXZ(p, c.pos);
        if (d > it.radius || std::fabs(p.y - c.pos.y) > 3.f) return;
        vec3 to = normalize(vec3(p.x - c.pos.x, 0, p.z - c.pos.z));
        float score = d - dot(to, fwd) * 0.8f;
        if (score < bestScore) { bestScore = score; best = e; }
    });
    // dungeon interactables are virtual (handled in Dungeon.cpp) — static stations in villages:
    interactTarget = best;
    // 1.11.0 at the wheel: the action button gets you out
    if (driving >= 0 && driving < (int)vehicleEnts.size() && reg.valid(vehicleEnts[driving])) {
        interactTarget = vehicleEnts[driving];
        if (Interact* it = reg.get<Interact>(interactTarget)) it->text = vehicleDefs()[vehicles[driving].def].fly ? "Poser le vaisseau" : (vehicleDefs()[vehicles[driving].def].moto ? "Descendre de la moto" : "Sortir de la voiture");
    }
}

void Game::playerInteract() {
    Character& c = pc();
    if (fishingT >= 0) {
        if (fishBite > 0) {
            Rng r((uint64_t)(gameTime * 100));
            std::string fish = r.chance(0.08f) ? "treasure_map" : "fish";
            addItem(fish, 1);
            play("splash"); particles.splash(c.pos + vec3(std::sin(c.yaw), 0, std::cos(c.yaw)) * 4.f, 1.f);
            advanceQuests(StepKind::Harvest, "fish", 1);
            gainXp(8);
        } else notify("Trop tôt… le poisson s'est enfui.", vec3(0.8f, 0.8f, 1.f), "i_fishing");
        fishingT = -1; fishBite = -1;
        c.vis.anim.stopAction();
        return;
    }
    if (dungeon.active) { updateDungeon(0); }  // dungeon interactions are resolved by proximity in updateDungeon
    if (interactTarget == NullEntity || !reg.valid(interactTarget)) return;
    Interact* it = reg.get<Interact>(interactTarget);
    if (!it) return;
    Entity e = interactTarget;
    switch (it->kind) {
    case InterKind::Npc: {
        Character* o = reg.get<Character>(e);
        if (o) o->yaw = std::atan2(c.pos.x - o->pos.x, c.pos.z - o->pos.z);
        openDialog(e);
        break;
    }
    case InterKind::Pickup: {
        Pickup* pk = reg.get<Pickup>(e);
        if (pk && addItem(pk->item, pk->count, pk->rarity)) { reg.destroyLater(e); play("pickup", 0.7f); }
        break;
    }
    case InterKind::Chest: {
        NodeComp* n = reg.get<NodeComp>(e);
        if (!n || openedChests.count(n->node.uid)) break;
        c.vis.anim.play(&clip(C_OPEN_CHEST), 1.f);
        openedChests.insert(n->node.uid);
        play("chest");
        haptic(40, 120);
        Rng r(n->node.uid);
        int tier = n->node.tier + (world.regionId(n->node.pos.x, n->node.pos.z) == R_GREENHAVEN ? 0 : 1);
        gold += r.irange(20, 60) * (1 + tier);
        play("coin", 0.7f);
        if (n->node.uid & 0x80000000u) { shards += 3; itemGet("i_crystal", "Coffre du ciel", "+3 éclats d'étoile", "", rarityColor(Rarity::Epic), false); }   // 3.2.0 sky chests
        // treasure: weapon/armour of the region tier with rolled rarity
        const auto& items = allItems();
        std::vector<int> pool;
        for (int i = 0; i < (int)items.size(); i++) {
            const ItemDef& d = items[i];
            if ((d.kind == ItemKind::Weapon || d.kind == ItemKind::Head || d.kind == ItemKind::Chest || d.kind == ItemKind::Hands || d.kind == ItemKind::Feet || d.kind == ItemKind::Ring || d.kind == ItemKind::Amulet || d.kind == ItemKind::Shield) && d.tier <= tier + 1 && (int)d.rarity <= (int)Rarity::Epic) pool.push_back(i);
        }
        if (!pool.empty()) {
            const ItemDef& d = items[pool[r.irange(0, (int)pool.size() - 1)]];
            float roll = r.f();
            Rarity rr = roll < 0.5f ? Rarity::Common : roll < 0.8f ? Rarity::Rare : roll < 0.95f ? Rarity::Epic : Rarity::Legendary;
            if ((int)rr < (int)d.rarity) rr = d.rarity;
            addItem(d.id, 1, rr);
        }
        addItem(r.chance(0.5f) ? "potion_heal" : "potion_mana", r.irange(1, 2));
        if (n->node.pos.y > world.height(n->node.pos.x, n->node.pos.z) + 8.f) {   // 2.8.0 a chest on a floating island
            addItem("sky_shard", 1); addItem("aurora_crystal", 2);
            notify("Un éclat du ciel !", vec3(0.6f, 1.f, 0.95f), "i_crystal");
        }
        particles.magicBurst(n->node.pos + vec3(0, 0.8f, 0), vec3(1, 0.85f, 0.4f), 1.2f, 30);
        gainXp(25 + tier * 15);
        // treasure quests resolve at their POI
        int pi = world.nearestPoi(n->node.pos.xz(), 60.f);
        if (pi >= 0) advanceQuests(StepKind::Discover, world.pois[pi].id);
        break;
    }
    case InterKind::Node: {
        NodeComp* n = reg.get<NodeComp>(e);
        if (!n) break;
        const NodeSpawn& ns = n->node;
        c.yaw = std::atan2(ns.pos.x - c.pos.x, ns.pos.z - c.pos.z);
        std::string give; int count = 1; const char* snd = "harvest_herb"; ClipId cl = C_HERB_PICK;
        Rng r(ns.uid + (uint32_t)gameTime);
        if (ns.type == NodeType::Totem || ns.type == NodeType::Orb) { collectTreasure(ns); return; }   // 3.2.0 (TreasureHunt.cpp)
        switch (ns.type) {
        case NodeType::Tree: give = "wood"; count = r.irange(2, 4); snd = "harvest_wood"; cl = C_GATHER_SWING; break;
        case NodeType::Rock: give = "stone"; count = r.irange(2, 3); snd = "harvest_stone"; cl = C_GATHER_SWING; break;
        case NodeType::IronOre: give = "iron_ore"; count = r.irange(1, 3); snd = "harvest_stone"; cl = C_GATHER_SWING; break;
        case NodeType::CopperOre: give = "copper_ore"; count = r.irange(1, 3); snd = "harvest_stone"; cl = C_GATHER_SWING; break;
        case NodeType::RareOre: give = "rare_ore"; count = r.irange(1, 2); snd = "harvest_stone"; cl = C_GATHER_SWING; break;
        case NodeType::Herb: give = world.regionId(ns.pos.x, ns.pos.z) == R_FROSTHEIM && r.chance(0.4f) ? "frost_lily" : "herb"; count = r.irange(1, 3); break;
        case NodeType::Mushroom: give = "glowcap"; count = r.irange(1, 3); break;
        case NodeType::Berries: give = "berries"; count = r.irange(2, 5); break;
        case NodeType::Crystal: give = world.regionId(ns.pos.x, ns.pos.z) == R_FROSTHEIM ? "frost_crystal" : "crystal"; count = 1; snd = "harvest_stone"; cl = C_GATHER_SWING; break;
        // 2.3.5 more resources
        case NodeType::GoldOre: give = "gold_ore"; count = r.irange(1, 2); snd = "harvest_stone"; cl = C_GATHER_SWING; break;
        case NodeType::SilverOre: give = "silver_ore"; count = r.irange(1, 3); snd = "harvest_stone"; cl = C_GATHER_SWING; break;
        case NodeType::Coal: give = "coal"; count = r.irange(2, 4); snd = "harvest_stone"; cl = C_GATHER_SWING; break;
        case NodeType::AmberRock: give = "amber"; count = 1; if (r.chance(0.5f)) addItem("stone", 1); snd = "harvest_stone"; cl = C_GATHER_SWING; break;
        case NodeType::Cactus: give = "cactus_fruit"; count = r.irange(2, 4); break;
        case NodeType::Shells: give = "seashell"; count = r.irange(2, 4); if (r.chance(0.25f)) { addItem("pearl", 1); notify("Une perle !", vec3(0.95f, 0.95f, 1.f), "i_crystal"); } if (r.chance(0.5f)) addItem("sea_salt", 1); break;
        case NodeType::Flax: give = "flax"; count = r.irange(2, 4); break;
        case NodeType::FireRoot: give = "fire_root"; count = r.irange(1, 2); break;
        case NodeType::AuroraCrystal: give = "aurora_crystal"; count = r.irange(1, 2); snd = "harvest_stone"; cl = C_GATHER_SWING; if (r.chance(0.3f)) addItem("crystal", 1); break;   // 2.8.0
        case NodeType::Relic: {
            static const char* relics[] = {"relic_elven_harp", "relic_star_glass", "relic_kings_seal"};
            int pi = world.nearestPoi(ns.pos.xz(), 80.f);
            std::string rid = relics[ns.uid % 3];
            if (pi >= 0 && world.pois[pi].id == "elven_ruins") rid = "relic_elven_harp";
            if (!relicsFound.count(rid)) { relicsFound.insert(rid); addItem(rid, 1); }
            // relics hold the legend pages (l4..l11) not given by the prologue
            static const char* relicLore[] = {"l4", "l5", "l6", "l7", "l8", "l9", "l10", "l11"};
            std::string lid = relicLore[ns.uid % 8];
            for (const char* cand : relicLore) if (!loreFound.count(cand)) { if (ns.uid % 2 == 0 || loreFound.count(lid)) lid = cand; break; }
            unlockLore(lid);
            world.harvested[ns.uid] = 0;  // permanent
            gainXp(60);
            c.vis.anim.play(&clip(C_INTERACT), 1.f);
            return;
        }
        default: break;
        }
        bool needTool = ns.type == NodeType::Tree || ns.type == NodeType::Rock || ns.type == NodeType::IronOre || ns.type == NodeType::CopperOre || ns.type == NodeType::RareOre
                     || ns.type == NodeType::GoldOre || ns.type == NodeType::SilverOre || ns.type == NodeType::Coal || ns.type == NodeType::AmberRock;
        if (needTool && !countItem(ns.type == NodeType::Tree ? "tool_woodaxe" : "tool_pickaxe")) {
            notify(ns.type == NodeType::Tree ? "Il vous faut une hachette (forgeronne de Brindlemere)." : "Il vous faut une pioche (forgeronne de Brindlemere).", vec3(1, 0.75f, 0.5f), ns.type == NodeType::Tree ? "i_woodaxe" : "i_pickaxe");
            return;
        }
        c.vis.anim.play(&clip(cl), 1.2f);
        n->shake = 1.f;
        play3D(snd, ns.pos, 0.9f);
        if (ns.type == NodeType::Tree) particles.dust(ns.pos + vec3(0, 0.5f, 0), 1.f, vec3(0.6f, 0.45f, 0.3f));
        else if (needTool) particles.sparks(ns.pos + vec3(0, 0.6f, 0), {0, 1, 0}, 8, vec3(1, 0.85f, 0.6f));
        else particles.dust(ns.pos + vec3(0, 0.3f, 0), 0.6f, vec3(0.5f, 0.7f, 0.3f));
        addItem(give, count);
        static const char* htype[] = {"wood", "stone", "ore", "ore", "ore", "herb", "crystal", "", "", "", "herb", "herb",
                                      "ore", "ore", "ore", "herb", "", "herb", "crystal", "herb"};
        advanceQuests(StepKind::Harvest, htype[(int)ns.type], count);
        gainXp(4);
        world.harvested[ns.uid] = gameTime + 300.0;  // respawns after 5 minutes (2.3.5, was 7)
        break;
    }
    case InterKind::Fishing: {
        if (!countItem("tool_rod")) { notify("Il vous faut une canne à pêche (marchande Lise).", vec3(1, 0.75f, 0.5f), "i_fishing"); return; }
        NodeComp* n = reg.get<NodeComp>(e);
        if (n) c.yaw = std::atan2(n->node.pos.x - c.pos.x, n->node.pos.z - c.pos.z);
        c.vis.anim.play(&clip(C_FISH_CAST), 1.f);
        fishingT = 0; fishBite = -1;
        play("splash", 0.5f);
        break;
    }
    case InterKind::Mount: toggleMount(); break;
    case InterKind::Cellar: enterCellar(); break;   // 3.6.0
    case InterKind::Amber: onAmberSpot(e); break;   // 3.10.0
    case InterKind::Vehicle:
        if (driving >= 0) exitVehicle();
        else enterVehicle(it->param);
        break;
    default: break;
    }
}

void Game::updateFishing(float dt) {
    if (fishingT < 0) return;
    Character& c = pc();
    fishingT += dt;
    if (length(input.move) > 0.3f || c.hitStun > 0) { fishingT = -1; fishBite = -1; return; }
    vec3 bob = c.pos + vec3(std::sin(c.yaw), 0, std::cos(c.yaw)) * 4.5f;
    bob.y = waterAt(bob.x, bob.z) > -1e8f ? waterAt(bob.x, bob.z) : bob.y;
    Rng r((uint64_t)(gameTime * 3));
    if (fishBite < 0 && fishingT > 2.f && r.f() < dt * 0.35f) { fishBite = 1.4f; play("fish_bite"); haptic(60, 200); particles.splash(bob, 0.4f); }
    if (fishBite > 0) {
        fishBite -= dt;
        floatText(c.pos + vec3(0, 2.5f, 0), "!", vec3(1, 0.9f, 0.3f), 34);
        if (fishBite <= 0) { notify("Le poisson s'est échappé.", vec3(0.8f, 0.8f, 1.f), "i_fishing"); fishingT = 0; fishBite = -1; }
    }
    if (fishingT > 18.f) { fishingT = -1; notify("Rien ne mord…", vec3(0.8f, 0.8f, 1.f), "i_fishing"); }
}

// ============================================================================================ update
void Game::setTorch(bool on) {
    if (on == torchOn) return;
    if (on && player != NullEntity && pc().swimming) { notify("Impossible d'allumer une torche dans l'eau.", vec3(1, 0.8f, 0.6f), "i_campfire"); return; }
    torchOn = on;
    play(on ? "fireball" : "ui_close", on ? 0.35f : 0.5f, on ? 1.5f : 1.f);
    if (player != NullEntity && on) particles.sparks(pc().pos + vec3(0, 1.4f, 0), vec3(0, 1, 0), 10, vec3(1.f, 0.7f, 0.3f));
}

void Game::updatePlayer(float dt) {
    if (player == NullEntity) return;
    Character& c = pc();
    Animator& a = c.vis.anim;
    // 3.10.3 the torch: lit and put out smoothly; water puts it out
    if (torchOn && c.swimming) { torchOn = false; play("splash", 0.4f); notify("La torche s'est éteinte dans l'eau.", vec3(1, 0.8f, 0.6f), "i_campfire"); }
    torchK = damp(torchK, torchOn ? 1.f : 0.f, 6.f, dt);
    // ---- timers & regen
    potionCD = std::max(0.f, potionCD - dt); dodgeCD = std::max(0.f, dodgeCD - dt);
    for (float& cd : spellCD) cd = std::max(0.f, cd - dt);
    if (comboTimer > 0) { comboTimer -= dt; if (comboTimer <= 0) comboStep = 0; }
    staminaRegenDelay -= dt;
    bool sprinting = false;
    if (c.dead) {
        deathT += dt;
        if (deathT > 2.8f && menu == Menu::None) menu = Menu::Death;
        return;
    }
    mp = std::min(maxMp, mp + dt * (1.2f + level * 0.08f));
    if (c.status.regen > 0) c.hp = std::min(c.maxHp, c.hp + dt * c.maxHp * 0.03f);
    if (bossEnt == NullEntity && c.hp < c.maxHp) c.hp = std::min(c.maxHp, c.hp + dt * (c.status.wellFed > 0 ? 3.f : 0.6f));
    findInteractTarget();
    updateFishing(dt);

    // ---- discovery of places, regions, map fog
    int fx = (int)(c.pos.x / WORLD_SIZE * 128), fz = (int)(c.pos.z / WORLD_SIZE * 128);
    for (int j = -3; j <= 3; j++) for (int i = -3; i <= 3; i++) if (i * i + j * j <= 10 && fx + i >= 0 && fx + i < 128 && fz + j >= 0 && fz + j < 128) fog[(fz + j) * 128 + fx + i] = 255;
    if (!dungeon.active) {
        int pi = world.nearestPoi(c.pos.xz(), 40.f);
        if (pi >= 0 && length(world.pois[pi].pos - c.pos.xz()) < world.pois[pi].radius + 10.f) discover(world.pois[pi].id);
        int r = world.regionId(c.pos.x, c.pos.z);
        if (r != lastRegion) {
            if (lastRegion >= 0 || true) { regionBanner = regionName(r); regionBannerSub = regionSubtitle(r); regionBannerT = 5.f; }
            lastRegion = r;
            if (!cine.active) unlockLore("r_" + std::to_string(r));
            if (r == R_FROSTHEIM && c.status.frostRes <= 0 && countItem("elixir_frost") == 0) toastHint = "Le froid de Frostheim est mordant : l'élixir de résistance au froid aide.";
        }
        // 1.9.1 tall grass: cover from enemies, a one-time hint
        float tg = (riding || c.swimming) ? 0.f : std::max(world.tallGrassAmount(c.pos.x, c.pos.z), world.wheatAmount(c.pos.x, c.pos.z) * 0.9f);   // 2.9.1 the wheat hides you too
        tallGrassHere = approach(tallGrassHere, tg, dt * 3.f);
        birdFlushCD = std::max(0.f, birdFlushCD - dt);
        if (tallGrassHere > 0.5f && !storyFlags.count("hint_tallgrass") && !cine.active) {
            storyFlags.insert("hint_tallgrass");
            toastHint = "Hautes herbes : les ennemis vous repèrent de moins loin, tant que vous n'attaquez pas.";
        }
        // cold drains stamina slowly in Frostheim without protection
        if (r == R_FROSTHEIM && c.status.frostRes <= 0 && resist[(int)Element::Ice] < 0.15f && snowAmt > 0.5f) staminaRegenDelay = std::max(staminaRegenDelay, 0.1f);
    }

    // ---- input → intent
    vec3 fwd{-std::sin(camYaw), 0, -std::cos(camYaw)}, right{std::cos(camYaw), 0, -std::sin(camYaw)};
    vec2 mv = input.move;
    float mag = std::min(1.f, length(mv));
    vec3 want = (right * mv.x + fwd * mv.y);
    if (length2(want) > 1e-4f) want = normalize(want);
    bool busy = c.attackClip != nullptr || c.hitStun > 0 || a.action() == &clip(C_OPEN_CHEST) || a.action() == &clip(C_GATHER_SWING) || a.action() == &clip(C_HERB_PICK);
    if (busy && c.attackClip && a.actionTime() > c.attackClip->cancelable && mag > 0.3f && c.attackIdx != -2) { a.stopAction(0.12f); c.attackClip = nullptr; busy = false; }

    if (input.interact) playerInteract();
    if (input.mount) toggleMount();
    if (input.potion) usePotion();
    for (int i = 0; i < 3; i++) if (input.skill[i]) { if (heroHasPowers()) castHeroPower(i); else castSpell(i); }
    updateHeroAbilities(dt);

    // ---- 1.11.0 at the wheel of a vehicle
    if (driving >= 0 && driving < (int)vehicles.size()) { updateDriving(dt, want, mag); return; }

    // ---- mounted
    if (riding && reg.valid(mountEnt)) {
        Character& m = *reg.get<Character>(mountEnt);
        const MountDef& md = mounts()[m.mountDef];
        sprinting = input.sprint && stamina > 5 && mag > 0.5f;
        float spd = mag * (sprinting ? md.sprint : md.speed) * gameTuning().mountSpeed;   // 2.7.0 mods
        if (sprinting) { stamina -= dt * 10.f; staminaRegenDelay = 0.5f; }
        vec3 dv = want * spd;
        m.vel.x = damp(m.vel.x, dv.x, 3.f, dt); m.vel.z = damp(m.vel.z, dv.z, 3.f, dt);
        float hs = length(vec2(m.vel.x, m.vel.z));
        if (hs > 0.3f) {
            float ty = std::atan2(m.vel.x, m.vel.z);
            float prev = m.yaw;
            m.yaw = dampAngle(m.yaw, ty, 5.f, dt);
            m.vis.anim.turnRate = wrapAngle(m.yaw - prev) / std::max(dt, 1e-4f);
        }
        m.vis.anim.speed = damp(m.vis.anim.speed, hs, 10.f, dt);
        float seat = (md.look.kind == 1 ? 0.95f : 1.25f) * m.vis.scale * 0.62f;
        if (ImportedModel* im = importedModel("horse_mount", Arch::Quadruped); im && im->mesh && im->mesh == m.vis.mesh) {
            // 2.3.3 the imported horse: the rider's pelvis sits on its real back (0.72 of the unit skeleton + the saddle)
            float hip = c.vis.anim.skeleton ? c.vis.anim.skeleton->bones[0].offset.y : 0.98f;
            seat = (0.72f + 0.05f) * m.vis.scale - hip * 0.92f;
        }
        // 3.8.1 a real horse's motion: walk (4 beats, the back swaying), trot (2 beats, jolts: the rider posts),
        // gallop (3 beats, the body rocking fore and aft: the rider leans forward in the stirrups). The horse's body
        // carries it (height, pitch, roll on the model), the rider sits on that moving back and reacts to it.
        const GaitParams& gp = m.vis.anim.gait;
        float asp = m.vis.anim.speed;
        float moving = smoothstepf(0.05f, 0.6f, asp);
        float trotK = saturate((asp - gp.walkSpeed * 0.8f) / gp.walkSpeed);
        float galK = saturate((asp - gp.runSpeed) / std::max(0.1f, gp.sprintSpeed - gp.runSpeed));
        float walkW = moving * (1.f - trotK), trotW = moving * trotK * (1.f - galK), galW = moving * galK;
        float gaitC = moving * (1.f + trotK + galK);   // 0 stand, 1 walk, 2 trot, 3 gallop
        float ph = m.vis.anim.phase * TAU;
        float S = m.vis.scale;
        // the back: height, pitch (nose up +), roll (+X side up) — in metres and radians
        float trotJolt = std::fabs(std::sin(ph)) - 0.64f;                       // two sharp landings per stride
        float galRock = std::sin(ph), galRock2 = std::cos(ph);
        float bob = (walkW * 0.018f * std::cos(2.f * ph) + trotW * 0.075f * trotJolt + galW * 0.065f * galRock) * S;
        float pitch = (walkW * 1.2f * std::sin(2.f * ph) + trotW * 1.0f * std::sin(2.f * ph) + galW * 7.5f * galRock2) * DEG;
        float roll = (walkW * 3.2f * std::sin(ph) + trotW * 1.6f * std::sin(ph) + galW * 2.f * std::sin(ph + 0.6f)) * DEG;
        // leaning into turns, climbing and descending
        float turn = clampf(m.vis.anim.turnRate * hs * 0.035f, -0.32f, 0.32f);
        roll -= turn;
        {
            vec3 f{std::sin(m.yaw), 0, std::cos(m.yaw)};
            float L = 1.1f * S;
            float sl = (groundAt(m.pos.x + f.x * L, m.pos.z + f.z * L) - groundAt(m.pos.x - f.x * L, m.pos.z - f.z * L)) / (2.f * L);
            rideSlope = damp(rideSlope, clampf(std::atan(sl), -0.4f, 0.4f), 6.f, dt);
        }
        pitch += rideSlope;
        m.vis.tiltPitch = pitch; m.vis.tiltRoll = roll; m.vis.rideY = bob;
        m.vis.anim.ride[5] = 1.f;
        rideBob = bob;
        // the rider's inertia: thrown back when the horse surges, forward when it brakes (a spring with a little overshoot)
        float acc = (hs - rideHsPrev) / std::max(dt, 1e-4f);
        rideHsPrev = hs;
        float tgt = clampf(-acc * 0.09f, -1.f, 1.f);
        rideLeanV += ((tgt - rideLean) * 38.f - rideLeanV * 7.f) * dt;
        rideLean = clampf(rideLean + rideLeanV * dt, -1.2f, 1.2f);
        quat rh = quat::axisAngle({0, 1, 0}, m.yaw) * quat::axisAngle({1, 0, 0}, -pitch) * quat::axisAngle({0, 0, 1}, roll);
        c.pos = m.pos + rh.rotate(vec3(0, seat, 0.05f * S));
        c.vis.tiltPitch = pitch; c.vis.tiltRoll = roll; c.vis.rideY = bob;
        c.yaw = m.yaw; c.vel = m.vel; c.grounded = true;
        a.aux[1] = saturate(hs / md.speed);
        a.ride[0] = gaitC; a.ride[1] = turn / 0.32f; a.ride[2] = rideLean; a.ride[3] = pitch / DEG; a.ride[4] = roll / DEG;
        a.phase = m.vis.anim.phase;
        a.speed = 0;
        // hoof beats in time with the legs (each hoof lands when its leg passes under the body)
        {
            static const float walkOff[4] = {0.25f, 0.75f, 0.0f, 0.5f}, trotOff[4] = {0.0f, 0.5f, 0.5f, 0.0f}, galOff[4] = {0.0f, 0.12f, 0.55f, 0.65f};
            float p0 = ridePhasePrev, p1 = m.vis.anim.phase;
            if (moving > 0.3f && p1 != p0) {
                for (int l = 0; l < 4; l++) {
                    float off = lerpf(lerpf(walkOff[l], trotOff[l], trotK), galOff[l], galK);
                    float a0 = p0 + off - 0.25f, a1 = p1 + off - 0.25f;
                    if (p1 < p0) a1 += 1.f;
                    if (std::floor(a1) > std::floor(a0)) {
                        bool front = l < 2;
                        float vol = (0.22f + 0.25f * trotW + 0.4f * galW) * (front ? 1.f : 0.85f);
                        if (trotW > 0.5f && (l == 1 || l == 3)) continue;   // a diagonal pair lands as one beat
                        if (m.inWater) play3D(variant("step_water"), m.pos, vol, 0.8f);
                        else play3D("hoof" + std::to_string(1 + ((l + (int)(gameTime * 7)) % 3)), m.pos, vol, 0.9f + 0.05f * l + (front ? 0.f : -0.06f));
                    }
                }
            }
            ridePhasePrev = p1;
        }
        // leather and breath
        rideCreakT -= dt * (0.5f + trotW * 2.f + galW * 1.5f) * moving;
        if (rideCreakT < 0.f) { rideCreakT = 1.6f + particles.rng.f() * 1.4f; play3D("saddle_creak", c.pos, 0.25f + 0.2f * trotW, 0.9f + particles.rng.f() * 0.2f); }
        rideSnortT -= dt * (0.25f + galW * 1.2f + (moving < 0.1f ? 0.4f : 0.f));
        if (rideSnortT < 0.f) { rideSnortT = 6.f + particles.rng.f() * 8.f; play3D("horse_snort", m.pos + vec3(std::sin(m.yaw), 1.4f, std::cos(m.yaw)) * S, 0.45f, 0.9f + particles.rng.f() * 0.2f); }
        if (galW > 0.6f && particles.rng.f() < dt * 6.f) particles.dust(m.pos - vec3(std::sin(m.yaw), 0, std::cos(m.yaw)) * S, 0.4f);   // dust thrown by the hooves
        if (input.jump) toggleMount();
        if (input.attack) { notify("Descendez de monture pour combattre.", vec3(1, 0.8f, 0.6f)); }
        if (mag > 0.1f && footstepT <= 0) { footstepT = 0.35f; }
        a.update(dt);
        stamina = std::min(maxStamina, stamina + (staminaRegenDelay > 0 ? 0 : dt * 22.f));
        return;
    }

    // ---- block / parry
    bool blocking = input.block && !busy && c.grounded && !c.swimming && stamina > 1;
    if (blocking && blockT == 0) parryT = 0.22f + skillBonus(skills, &SkillNode::parryWindow);
    blockT = blocking ? blockT + dt : 0.f;
    parryT = std::max(0.f, parryT - dt);
    a.blocking = blocking;

    // ---- attack input: tap = light, hold = charge -> release = charged/heavy
    if (input.attackRelease) { attackBuf = 0.45f; attackBufHeavy = input.attackHoldT > 0.45f; attackBufCharged = input.attackHoldT > 0.9f; }
    attackBuf = std::max(0.f, attackBuf - dt);
    if (!busy || (c.attackClip && a.actionTime() >= c.attackClip->comboOpen && c.attackIdx != -2)) {
        if (input.heavy) { playerAttack(true, false); attackBuf = 0; }
        else if (attackBuf > 0) {
            float keep = attackBuf; attackBuf = 0;
            playerAttack(attackBufHeavy, attackBufCharged && a.stance == ST_ONEHAND);
            if (attackBuf > 0) attackBuf = std::min(attackBuf, keep);  // re-buffered (clip not open yet)
            attackBufHeavy = attackBufCharged = false;
        }
    }
    charging = input.attackHeld && input.attackHoldT > 0.45f && !busy;
    if (charging && particles.rng.f() < dt * 20) particles.magicTrail(c.pos + vec3(0, 1.2f, 0) + vec3(std::sin(c.yaw), 0, std::cos(c.yaw)) * 0.5f, input.attackHoldT > 0.9f ? vec3(1, 0.6f, 0.2f) : vec3(1, 0.9f, 0.6f), 0.6f);
    if (input.dodge) dodgeBuf = 0.3f;
    dodgeBuf = std::max(0.f, dodgeBuf - dt);
    if (dodgeBuf > 0 && dodgeCD <= 0) { dodgeBuf = 0; playerDodge(); }
    // 1.11.2 jump buffer: a press up to 0.15 s before landing jumps on touchdown
    if (input.jump) { if (playerJump()) jumpBuf = 0; else jumpBuf = 0.15f; }
    else if (jumpBuf > 0) { jumpBuf -= dt; if (c.grounded && playerJump(true)) jumpBuf = 0; }
    if (c.grounded && c.vel.y <= 0.1f) sprintJump = false;
    if (input.interact && lockTarget == NullEntity) {}

    // ---- locomotion
    float baseSpeed = mag < 0.55f ? 2.1f : 4.9f;
    sprinting = input.sprint && mag > 0.6f && stamina > 3 && !blocking;
    if (sprinting) { baseSpeed = 7.2f; stamina -= dt * (c.swimming ? 9.f : 13.f) * (hasPower(KP_SWIFT) ? 0.5f : 1.f); staminaRegenDelay = 0.6f; }   // 3.2.0 wind hoof   // in the water: the front crawl
    if (blocking) baseSpeed = std::min(baseSpeed, 1.8f);
    if (sprintJump && !c.grounded && mag > 0.5f) baseSpeed = std::max(baseSpeed, 7.2f);   // momentum of a running jump
    if (flying) { baseSpeed = sprinting ? 17.f : 11.f; sprinting = false; }   // Astra's flight (no sprint stamina cost)
    float speed = baseSpeed * moveSpeedMul * (c.status.slow > 0 ? 0.55f : 1.f) * (c.swimming ? (diving ? 0.62f : (sprinting ? 0.72f : 0.6f)) : 1.f);
    if (charging) speed *= 0.4f;
    // 1.11.2 slopes: climbing is slower, running downhill a little faster
    slopeMul = 1.f;
    if (c.grounded && !flying && !c.swimming && mag > 0.15f && length2(want) > 1e-4f) {
        vec3 w = normalize(vec3(want.x, 0, want.z));
        float sl = (groundAt(c.pos.x + w.x * 0.9f, c.pos.z + w.z * 0.9f) - groundAt(c.pos.x, c.pos.z)) / 0.9f;
        slopeMul = clampf(1.f - sl * 0.45f, 0.85f, 1.15f);   // 3.9.1 climbing hardly slows you down any more
        speed *= slopeMul;
    }
    // 3.9.1 leaving the water: swimming into a bank (up to 1.6 m above the surface) climbs out by itself
    if (c.swimming && !diving && !flying && mag > 0.3f && length2(want) > 1e-4f && !c.attackClip) {
        vec3 w = normalize(vec3(want.x, 0, want.z));
        float wl = waterAt(c.pos.x, c.pos.z);
        for (float dd : {0.9f, 1.4f}) {
            vec3 q = c.pos + w * dd;
            float g = groundAt(q.x, q.z);
            if (g > wl - 0.6f && g < wl + 1.6f && waterAt(q.x, q.z) - g < 0.9f) {
                c.pos = vec3(q.x, g + 0.05f, q.z); c.vel = w * 2.5f; c.vel.y = 0; c.swimming = false; c.grounded = true;
                c.vis.anim.play(&clip(C_LAND), 1.1f, false, 0.05f);
                particles.splash(c.pos, 0.6f); play3D("splash", c.pos, 0.4f, 1.2f);
                break;
            }
        }
    }
    if (fishingT >= 0) speed = 0;
    vec3 dv = (busy || c.status.root > 0 || c.status.freeze > 0) ? vec3(0) : want * speed * (mag < 0.15f ? 0 : 1);
    float rate = length2(dv) > length2(vec3(c.vel.x, 0, c.vel.z)) ? 9.f : 12.f;
    if (!c.grounded) rate = flying ? 4.f : 2.f;
    if (geyserT > 0.f && !c.grounded) rate = mag > 0.15f ? 0.7f : 0.f;   // 2.8.0 the geyser's push carries you (a little steering)
    c.vel.x = damp(c.vel.x, dv.x, rate, dt);
    c.vel.z = damp(c.vel.z, dv.z, rate, dt);
    float hs = length(vec2(c.vel.x, c.vel.z));
    if (fpActive && !busy && driving < 0 && !riding && lockTarget == NullEntity) {   // 3.6.0 first person: the body faces where you look, you walk and strafe
        float prev = c.yaw;
        c.yaw = dampAngle(c.yaw, fpYaw, 20.f, dt);
        a.turnRate = wrapAngle(c.yaw - prev) / std::max(dt, 1e-4f);
    } else if (!busy && mag > 0.15f) {
        float target = std::atan2(want.x, want.z);
        if (lockTarget != NullEntity && reg.valid(lockTarget)) { Character* t = reg.get<Character>(lockTarget); if (t) target = std::atan2(t->pos.x - c.pos.x, t->pos.z - c.pos.z); }
        float prev = c.yaw;
        c.yaw = dampAngle(c.yaw, target, sprinting ? 8.f : 13.f, dt);
        a.turnRate = wrapAngle(c.yaw - prev) / std::max(dt, 1e-4f);
    } else a.turnRate = damp(a.turnRate, 0, 8, dt);
    a.speed = damp(a.speed, hs, hs > a.speed ? 9.f : 12.f, dt);   // 3.4.0 eased: no snap of the gait on starts and stops
    // combat readiness (weapon guard vs relaxed carry): threats nearby, lock-on, attacking or blocking
    {
        bool threat = lockTarget != NullEntity || bossEnt != NullEntity || input.attackHeld || input.block || c.attackClip != nullptr || comboTimer > 0.05f;
        if (!threat) reg.each<AIBrain, Character>([&](Entity, AIBrain& ai, Character& o) {
            if (!o.dead && o.team == Team::Enemy && (ai.state == AIState::Chase || ai.state == AIState::Attack || ai.state == AIState::Strafe) && dist2XZ(o.pos, c.pos) < 16 * 16) threat = true;
        });
        a.aux[3] = approach(a.aux[3], threat ? 1.f : 0.f, dt * (threat ? 5.f : 0.7f));
    }
    a.breath = saturate(1.f - stamina / std::max(1.f, maxStamina)) + (sprinting ? 0.5f : 0.f);
    // 2.4.0 swimming: styles for the animation, air, wake, bubbles, stroke sounds
    if (!c.swimming) diving = false;
    a.swimCrawl = approach(a.swimCrawl, c.swimming && sprinting && !diving ? 1.f : 0.f, dt * 3.f);
    a.swimDive = approach(a.swimDive, diving ? 1.f : 0.f, dt * 2.f);
    if (diving) {
        if (!hasPower(KP_BREATH)) breath -= dt / BREATH_SECONDS;   // 3.2.0 triton scale
        if (particles.rng.f() < dt * 4.f) particles.magicTrail(c.pos + vec3(particles.rng.range(-0.2f, 0.2f), 1.5f, particles.rng.range(-0.2f, 0.2f)), vec3(0.75f, 0.9f, 1.f), 0.25f);
        if (breath <= 0.25f && breath + dt / BREATH_SECONDS > 0.25f) notify("Vous manquez d'air : remontez (Saut ou Esquive) !", vec3(0.6f, 0.85f, 1.f), "i_info");
        if (breath <= 0) {
            breath = 0;
            c.hp -= c.maxHp * 0.08f * dt;
            if (c.hp <= 0) killCharacter(player, c, NullEntity);
        }
    } else breath = std::min(1.f, breath + dt / 3.f);
    if (c.swimming && !diving && hs > 1.f) {   // wake at the surface, a stroke sound now and then
        swimFxT -= dt;
        if (swimFxT <= 0) { swimFxT = sprinting ? 0.18f : 0.32f; particles.splash(c.pos + vec3(std::sin(c.yaw), 0, std::cos(c.yaw)) * 0.6f + vec3(0, 1.2f, 0), sprinting ? 0.35f : 0.22f); }
        swimStrokeT -= dt;
        if (swimStrokeT <= 0) { swimStrokeT = sprinting ? 0.55f : 0.95f; play3D(variant("step_water"), c.pos, sprinting ? 0.3f : 0.2f, particles.rng.range(0.85f, 1.f)); }
    }
    stamina = std::min(maxStamina, stamina + (staminaRegenDelay > 0 ? 0.f : dt * (c.status.wellFed > 0 ? 34.f : 24.f) * (blocking ? 0.3f : 1.f) * gameTuning().staminaRegen));
    if (stamina < 0) stamina = 0;

    // ---- footsteps by surface
    if (a.footstepEvent > 0 && c.grounded) {
        a.footstepEvent = 0;
        const char* step = "step_grass";
        if (c.inWater) step = "step_water";
        else if (dungeon.active) step = "step_stone";
        else {
            int r = world.regionId(c.pos.x, c.pos.z);
            if (world.roadInfluence(c.pos.x, c.pos.z) > 0.4f) step = "step_stone";
            else if (r == R_FROSTHEIM || snowAmt > 0.5f) step = "step_snow";
            else if (r == R_ASHEN || r == R_HIGHCROWNS) step = "step_stone";
            else if (r == R_AMBER) step = "step_sand";
        }
        play(variant(step), sprinting ? 0.5f : 0.32f, particles.rng.range(0.94f, 1.06f));
        // brushing through the grass of the green regions (not on roads, not mounted)
        if (std::strcmp(step, "step_grass") == 0 && !dungeon.active && !riding) {
            int r = world.regionId(c.pos.x, c.pos.z);
            if ((r == R_GREENHAVEN || r == R_ELDERWOOD || r == R_SHADOWS) && particles.rng.chance(a.speed > 3.f ? 0.6f : 0.3f)) {
                play("grass_rustle", a.speed > 3.f ? 0.32f : 0.2f, particles.rng.range(0.85f, 1.15f));
                if (a.speed > 3.f && particles.rng.chance(0.5f)) particles.dust(c.pos + vec3(0, 0.25f, 0), 0.35f, vec3(0.42f, 0.55f, 0.25f));
            }
        }
        // 1.9.1 wading through tall grass: louder rustle, grasshoppers leaping away, sometimes birds take off
        if (tallGrassHere > 0.3f && !riding) {
            play("grass_rustle", (a.speed > 3.f ? 0.45f : 0.3f) * tallGrassHere, particles.rng.range(0.8f, 1.1f));
            if (particles.rng.chance(0.35f * tallGrassHere)) {
                for (int i = 0; i < 2; i++) {
                    Particle p;
                    float ang = particles.rng.range(0, TAU);
                    p.pos = c.pos + vec3(std::cos(ang) * 0.6f, 0.4f, std::sin(ang) * 0.6f);
                    p.vel = vec3(std::cos(ang) * 1.6f, particles.rng.range(2.2f, 3.2f), std::sin(ang) * 1.6f);
                    p.gravity = 9.f; p.maxLife = 0.75f; p.size0 = p.size1 = 0.035f; p.stretch = 0.02f;
                    vec3 gc = particles.rng.chance(0.5f) ? vec3(0.42f, 0.55f, 0.2f) : vec3(0.55f, 0.48f, 0.25f);
                    p.c0 = vec4(gc, 1.f); p.c1 = vec4(gc, 1.f); p.frame = PF_DOT; p.additive = false;
                    p.groundKill = groundAt(p.pos.x, p.pos.z); p.bounce = true;
                    particles.spawn(p);
                }
            }
            if (a.speed > 3.5f && birdFlushCD <= 0 && particles.rng.chance(0.12f) && env.night < 0.5f) {
                birdFlushCD = 25.f;
                vec3 fwd(std::sin(c.yaw), 0, std::cos(c.yaw));
                for (int i = 0; i < 5; i++) {
                    Particle b;
                    b.pos = c.pos + fwd * particles.rng.range(3.f, 6.f) + vec3(particles.rng.range(-1.5f, 1.5f), 0.6f, particles.rng.range(-1.5f, 1.5f));
                    b.vel = fwd * particles.rng.range(3.f, 5.f) + vec3(particles.rng.range(-2.f, 2.f), particles.rng.range(4.f, 6.f), particles.rng.range(-2.f, 2.f));
                    b.maxLife = 4.f; b.size0 = b.size1 = 0.4f; b.frame = PF_BIRD; b.additive = false; b.c0 = b.c1 = vec4(0.15f, 0.13f, 0.12f, 1.f); b.drag = 0.2f;
                    particles.spawn(b);
                }
                play("birds", 0.5f, particles.rng.range(1.05f, 1.2f));
            }
        }
        if (sprinting && particles.rng.chance(0.5f)) particles.dust(c.pos, 0.6f);
        if (c.inWater) particles.splash(c.pos, 0.3f);
    }
    // lock-on management: drop dead / far targets
    if (lockTarget != NullEntity) {
        Character* t = reg.valid(lockTarget) ? reg.get<Character>(lockTarget) : nullptr;
        if (!t || t->dead || distXZ(t->pos, c.pos) > 26.f) lockTarget = NullEntity;
    }
    // pickups: magnet & auto-collect
    reg.each<Pickup>([&](Entity e, Pickup& pk) {
        float d = distXZ(pk.pos, c.pos);
        if (pk.t > 0.6f && d < (hasPower(KP_MAGNET) ? 3.2f : 2.4f)) {
            pk.pos = lerp(pk.pos, c.pos + vec3(0, 0.8f, 0), std::min(1.f, dt * 8));
            if (d < 0.7f && addItem(pk.item, pk.count, pk.rarity)) { reg.destroyLater(e); play("pickup", 0.6f, 1.1f); }
        }
    });
}

}  // namespace lk
