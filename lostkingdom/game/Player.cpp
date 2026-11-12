#include "Game.h"
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
    c.maxHp = maxHp; c.hp = std::min(maxHp, hpRatio * maxHp);
    c.atk = atk * (1 + atkPct); c.def = def; c.element = elem; c.elemDmg = elemDmg;
    maxMp = mpMax; mp = std::min(mp, maxMp); maxStamina = stMax; stamina = std::min(stamina, maxStamina);
    critChance = std::min(0.75f, crit); critDmg = cdmg; moveSpeedMul = 1.f + speed;
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
    Character& c = pc();
    HumanoidLook L;
    L.hair = Hair::Ponytail; L.hairCol = hexColor(0x3a2418); L.skin = hexColor(0xd9a98a); L.eyes = hexColor(0x3a6a8a);
    L.c1 = hexColor(0x5a3a2a); L.c2 = hexColor(0x3a3028); L.c3 = hexColor(0x2a2018); L.seed = 7;
    L.body = Body::Tunic; L.belt = true; L.satchel = true;
    const ItemDef* head = item(equipment[SL_HEAD].id);
    const ItemDef* chest = item(equipment[SL_CHEST].id);
    const ItemDef* hands = item(equipment[SL_HANDS].id);
    const ItemDef* feet = item(equipment[SL_FEET].id);
    if (head) L.helm = head->helm;
    if (chest) { L.body = chest->body; L.c1 = chest->color; L.pauldrons = chest->pauldrons; L.cape = chest->cape; if (chest->body == Body::Plate) L.metal = lerp(hexColor(0xa8aab0), chest->color, 0.35f); }
    if (hands) L.gauntlets = hands->gauntlets || hands->tier >= 2;
    if (feet) { L.greaves = feet->greaves; L.c3 = lerp(hexColor(0x2a2018), feet->color, 0.4f); }
    if (!dye.empty()) { const ItemDef* d = item(dye); if (d) { L.c1 = d->color; } }
    if (chest && chest->set >= 0 && chest->set == 6) L.trim = hexColor(0xf0d080);
    L.cape = L.cape || (chest && chest->tier >= 3);
    const ItemDef* w = item(equipment[SL_WEAPON].id);
    const ItemDef* o = item(equipment[SL_OFFHAND].id);
    WeaponType wt = w ? w->weapon : WeaponType::None;
    WeaponType ot = o && wt != WeaponType::Bow && wt != WeaponType::Longsword && wt != WeaponType::Hammer && wt != WeaponType::Spear && wt != WeaponType::Staff ? o->weapon : WeaponType::None;
    Animator keep = c.vis.anim;
    bool had = c.vis.mesh != nullptr;
    setupVisualHumanoid(c.vis, L, wt, ot, 1.f);
    if (w) c.vis.weapon = weaponMesh(wt, w->tier, elementColor(w->element), &c.vis.tip);
    if (o && ot != WeaponType::None) c.vis.offhand = weaponMesh(ot, o->tier, o->color);
    if (had) { int st = c.vis.anim.stance; c.vis.anim = keep; c.vis.anim.stance = st; }
    c.vis.capeColor = L.c1 * 0.8f;
    c.vis.rim = 0.65f;
    c.vis.outlineColor = vec4(0.05f, 0.04f, 0.03f, 1);
    c.vis.trailColor = w ? elementColor(w->element) : vec3(1, 0.9f, 0.7f);
}

void Game::gainXp(int amount) {
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
    default: return &clip(step % 2 == 0 ? C_FIST_A1 : C_FIST_A2);
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
    default: return &clip(C_FIST_A2);
    }
}

void Game::playerAttack(bool heavy, bool charged) {
    Character& c = pc();
    if (c.dead || riding || c.swimming || c.hitStun > 0) return;
    Animator& a = c.vis.anim;
    if (c.attackClip && !(a.actionTime() >= c.attackClip->comboOpen)) { comboTimer = 0.4f; return; }  // buffer
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
}

void Game::playerDodge() {
    Character& c = pc();
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
}

void Game::playerJump() {
    Character& c = pc();
    if (c.dead || riding || !c.grounded || c.attackClip) return;
    if (c.swimming) { c.vel.y = 3.f; return; }
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
                return;
            }
        }
    }
    if (stamina < 8) return;
    stamina -= 8; staminaRegenDelay = 0.5f;
    c.vel.y = 6.4f;
    c.grounded = false;
    play("jump", 0.6f);
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
        if (s.id == "regrowth") { c.status.regen = s.duration; particles.heal(c.pos, col); }
        else { c.hp = std::min(c.maxHp, c.hp + power * 1.2f * healMul); particles.heal(c.pos, col); areaDamage(player, c.pos, s.radius, power, s.school, Team::Player, 20); particles.shockwave(c.pos, col, s.radius); floatText(c.pos + vec3(0, 2.4f, 0), "+" + std::to_string((int)(power * 1.2f * healMul)), vec3(0.4f, 1.f, 0.5f)); }
        break;
    }
    case SpellKind::Shield: c.vis.anim.play(&clip(C_CAST_SELF), 1.2f, true); c.status.shield = c.maxHp * 0.35f; particles.magicBurst(c.pos + vec3(0, 1, 0), col, 1.5f, 30); break;
    case SpellKind::Dash: {
        vec3 from = c.pos;
        vec3 to = c.pos + fwd * s.radius;
        vec3 tmp = to; collideStatic(tmp, c.radius, c.pos.y);
        to = tmp; to.y = groundAt(to.x, to.z);
        for (int i = 0; i < 10; i++) particles.magicTrail(lerp(from, to, i / 9.f) + vec3(0, 1, 0), col, 1.5f);
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
        }
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
        if (Character* o = reg.get<Character>(e)) { if (o->dead || e == player) return; p = o->pos; if (e == mountEnt && riding) return; }
        else if (NodeComp* n = reg.get<NodeComp>(e)) { p = n->node.pos; if (n->node.type == NodeType::Chest && openedChests.count(n->node.uid)) return; if (n->node.type != NodeType::Chest && n->node.type != NodeType::FishingSpot && !world.nodeAvailable(n->node.uid, gameTime)) return; }
        else if (Pickup* pk = reg.get<Pickup>(e)) p = pk->pos;
        else return;
        float d = distXZ(p, c.pos);
        if (d > it.radius || std::fabs(p.y - c.pos.y) > 3.f) return;
        vec3 to = normalize(vec3(p.x - c.pos.x, 0, p.z - c.pos.z));
        float score = d - dot(to, fwd) * 0.8f;
        if (score < bestScore) { bestScore = score; best = e; }
    });
    // dungeon interactables are virtual (handled in Dungeon.cpp) — static stations in villages:
    interactTarget = best;
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
        case NodeType::Relic: {
            static const char* relics[] = {"relic_elven_harp", "relic_star_glass", "relic_kings_seal"};
            int pi = world.nearestPoi(ns.pos.xz(), 80.f);
            std::string rid = relics[ns.uid % 3];
            if (pi >= 0 && world.pois[pi].id == "elven_ruins") rid = "relic_elven_harp";
            if (!relicsFound.count(rid)) { relicsFound.insert(rid); addItem(rid, 1); }
            const auto& L = lore();
            const LoreDef& ld = L[ns.uid % L.size()];
            if (!loreFound.count(ld.id)) { loreFound.insert(ld.id); notify("Page de chronique : " + ld.title, vec3(0.9f, 0.8f, 1.f), "i_quest"); }
            world.harvested[ns.uid] = 0;  // permanent
            gainXp(60);
            c.vis.anim.play(&clip(C_INTERACT), 1.f);
            return;
        }
        default: break;
        }
        bool needTool = ns.type == NodeType::Tree || ns.type == NodeType::Rock || ns.type == NodeType::IronOre || ns.type == NodeType::CopperOre || ns.type == NodeType::RareOre;
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
        static const char* htype[] = {"wood", "stone", "ore", "ore", "ore", "herb", "crystal", "", "", "", "herb", "herb"};
        advanceQuests(StepKind::Harvest, htype[(int)ns.type], count);
        gainXp(4);
        world.harvested[ns.uid] = gameTime + 420.0;  // respawns after 7 minutes
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
void Game::updatePlayer(float dt) {
    if (player == NullEntity) return;
    Character& c = pc();
    Animator& a = c.vis.anim;
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
            if (r == R_FROSTHEIM && c.status.frostRes <= 0 && countItem("elixir_frost") == 0) toastHint = "Le froid de Frostheim est mordant : l'élixir de résistance au froid aide.";
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
    for (int i = 0; i < 3; i++) if (input.skill[i]) castSpell(i);

    // ---- mounted
    if (riding && reg.valid(mountEnt)) {
        Character& m = *reg.get<Character>(mountEnt);
        const MountDef& md = mounts()[m.mountDef];
        sprinting = input.sprint && stamina > 5 && mag > 0.5f;
        float spd = mag * (sprinting ? md.sprint : md.speed);
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
        m.vis.anim.speed = hs;
        c.pos = m.pos + vec3(0, (md.look.kind == 1 ? 0.95f : 1.25f) * m.vis.scale * 0.62f, 0) + vec3(std::sin(m.yaw), 0, std::cos(m.yaw)) * 0.05f * m.vis.scale;
        c.yaw = m.yaw; c.vel = m.vel; c.grounded = true;
        a.aux[1] = saturate(hs / md.speed);
        a.phase = m.vis.anim.phase;
        a.speed = 0;
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
    if (!busy || (c.attackClip && a.actionTime() >= c.attackClip->comboOpen && c.attackIdx != -2)) {
        if (input.heavy) playerAttack(true, false);
        else if (input.attackRelease) {
            if (input.attackHoldT > 0.45f) playerAttack(true, input.attackHoldT > 0.9f && a.stance == ST_ONEHAND);
            else playerAttack(false, false);
        }
    } else if (input.attackRelease && comboTimer > 0) { /* buffered press handled when the clip opens */ }
    if (comboTimer > 0.39f && comboTimer < 0.41f && c.attackClip && a.actionTime() >= c.attackClip->comboOpen) playerAttack(false, false);
    charging = input.attackHeld && input.attackHoldT > 0.45f && !busy;
    if (charging && particles.rng.f() < dt * 20) particles.magicTrail(c.pos + vec3(0, 1.2f, 0) + vec3(std::sin(c.yaw), 0, std::cos(c.yaw)) * 0.5f, input.attackHoldT > 0.9f ? vec3(1, 0.6f, 0.2f) : vec3(1, 0.9f, 0.6f), 0.6f);
    if (input.dodge) playerDodge();
    if (input.jump) playerJump();
    if (input.interact && lockTarget == NullEntity) {}

    // ---- locomotion
    float baseSpeed = mag < 0.55f ? 2.1f : 4.9f;
    sprinting = input.sprint && mag > 0.6f && stamina > 3 && !blocking && !c.swimming;
    if (sprinting) { baseSpeed = 7.2f; stamina -= dt * 13.f; staminaRegenDelay = 0.6f; }
    if (blocking) baseSpeed = std::min(baseSpeed, 1.8f);
    float speed = baseSpeed * moveSpeedMul * (c.status.slow > 0 ? 0.55f : 1.f) * (c.swimming ? 0.55f : 1.f);
    if (charging) speed *= 0.4f;
    if (fishingT >= 0) speed = 0;
    vec3 dv = (busy || c.status.root > 0 || c.status.freeze > 0) ? vec3(0) : want * speed * (mag < 0.15f ? 0 : 1);
    float rate = length2(dv) > length2(vec3(c.vel.x, 0, c.vel.z)) ? 9.f : 12.f;
    if (!c.grounded) rate = 2.f;
    c.vel.x = damp(c.vel.x, dv.x, rate, dt);
    c.vel.z = damp(c.vel.z, dv.z, rate, dt);
    float hs = length(vec2(c.vel.x, c.vel.z));
    if (!busy && mag > 0.15f) {
        float target = std::atan2(want.x, want.z);
        if (lockTarget != NullEntity && reg.valid(lockTarget)) { Character* t = reg.get<Character>(lockTarget); if (t) target = std::atan2(t->pos.x - c.pos.x, t->pos.z - c.pos.z); }
        float prev = c.yaw;
        c.yaw = dampAngle(c.yaw, target, sprinting ? 8.f : 13.f, dt);
        a.turnRate = wrapAngle(c.yaw - prev) / std::max(dt, 1e-4f);
    } else a.turnRate = damp(a.turnRate, 0, 8, dt);
    a.speed = hs;
    a.breath = saturate(1.f - stamina / std::max(1.f, maxStamina)) + (sprinting ? 0.5f : 0.f);
    stamina = std::min(maxStamina, stamina + (staminaRegenDelay > 0 ? 0.f : dt * (c.status.wellFed > 0 ? 34.f : 24.f) * (blocking ? 0.3f : 1.f)));
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
        }
        play(step, sprinting ? 0.5f : 0.32f, particles.rng.range(0.9f, 1.1f));
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
        if (pk.t > 0.6f && d < 2.4f) {
            pk.pos = lerp(pk.pos, c.pos + vec3(0, 0.8f, 0), std::min(1.f, dt * 8));
            if (d < 0.7f && addItem(pk.item, pk.count, pk.rarity)) { reg.destroyLater(e); play("pickup", 0.6f, 1.1f); }
        }
    });
}

}  // namespace lk
