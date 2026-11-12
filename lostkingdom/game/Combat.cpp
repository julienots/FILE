#include "Game.h"
#include "../engine/core/Platform.h"
#include <algorithm>

namespace lk {

static float stanceReach(int stance) {
    switch (stance) {
    case ST_ONEHAND: return 2.3f;
    case ST_TWOHAND: return 2.9f;
    case ST_DAGGER: return 1.8f;
    case ST_SPEAR: return 3.3f;
    case ST_STAFF: return 2.2f;
    case ST_TOOL: return 1.8f;
    default: return 1.6f;
    }
}

static bool isSpin(const Clip* c) { return c == &clip(C_1H_HEAVY) || c == &clip(C_B_SPIN) || c == &clip(C_D_TAIL) || c == &clip(C_SP_A2); }

Entity Game::findTarget(vec3 pos, float yaw, float range, Team team, float cone) {
    Entity best = NullEntity;
    float bestScore = 1e9f;
    vec3 fwd{std::sin(yaw), 0, std::cos(yaw)};
    reg.each<Character>([&](Entity e, Character& c) {
        if (c.team != team || c.dead) return;
        vec3 d = c.pos - pos; d.y = 0;
        float dist = length(d);
        if (dist > range + c.radius) return;
        float facing = dist > 0.01f ? dot(d / dist, fwd) : 1.f;
        if (facing < cone - 1.f) return;
        float score = dist * (1.6f - facing);
        if (score < bestScore) { bestScore = score; best = e; }
    });
    return best;
}

void Game::startAttack(Entity e, Character& c, const Clip* cl, float mult, Element elem, float speed) {
    c.attackClip = cl; c.attackMult = mult; c.attackElem = elem; c.hitThisSwing.clear(); c.lastMotion = 0; c.attackIdx = 0;
    c.vis.anim.play(cl, speed, false, 0.06f);
    c.vis.trail.clear();
    (void)e;
}

void Game::hitFx(vec3 p, vec3 dir, Element e, float scale, bool metal) {
    vec3 col = e == Element::Physical ? (metal ? vec3(1.f, 0.85f, 0.5f) : vec3(1.f, 0.55f, 0.35f)) : elementColor(e);
    particles.impact(p, -dir + vec3(0, 0.4f, 0), col, scale);
    if (e == Element::Fire) particles.explosion(p, 0.3f * scale, col);
    if (e == Element::Ice) particles.iceShards(p, 8);
    if (e == Element::Lightning) particles.lightningArc(p + vec3(0, 1.5f, 0), p, col);
    if (e == Element::Shadow || e == Element::Arcane) particles.magicBurst(p, col, 0.6f * scale, 14);
    if (e == Element::Light) particles.magicBurst(p, col, 0.7f * scale, 18);
    if (e == Element::Nature) particles.magicBurst(p, col, 0.5f * scale, 10);
}

float Game::dealDamage(Entity src, Entity dst, float amount, Element elem, vec3 dir, float poiseDmg, bool canCrit, bool isSpell) {
    Character* t = reg.get<Character>(dst);
    Character* s = reg.valid(src) ? reg.get<Character>(src) : nullptr;
    if (!t || t->dead || t->invuln > 0) return 0;
    if (t->team == Team::Neutral || t->team == Team::Mount) return 0;
    vec3 hitPos = t->pos + vec3(0, t->height * 0.55f, 0);
    // ---- block & parry (player)
    if (dst == player && s) {
        vec3 to = normalize(vec3(s->pos.x - t->pos.x, 0, s->pos.z - t->pos.z));
        bool front = dot(to, vec3(std::sin(t->yaw), 0, std::cos(t->yaw))) > 0.15f;
        if (t->vis.anim.blocking && front) {
            if (parryT > 0) {
                // perfect parry: attacker staggered, time slows, riposte available
                s->hitStun = 1.6f; s->poise = 0; s->attackClip = nullptr;
                s->vis.anim.play(&clip(s->vis.arch == Arch::Humanoid ? C_HIT_HEAVY : (s->vis.arch == Arch::Spider ? C_S_HIT : C_Q_HIT)), 0.8f);
                particles.sparks(hitPos + to * 0.4f, to, 26, vec3(1, 0.9f, 0.5f));
                particles.shockwave(t->pos, vec3(1, 0.9f, 0.6f), 2.f);
                play("parry", 1.f); haptic(50, 255);
                hitStop = 0.25f; addShake(0.25f);
                floatText(hitPos + vec3(0, 0.8f, 0), "PARADE !", vec3(1, 0.9f, 0.4f), 28);
                pc().vis.anim.play(&clip(C_PARRY), 1.2f, true);
                comboStep = 0; comboTimer = 1.2f;
                lockTarget = src;
                // riposte follows automatically
                startAttack(player, pc(), &clip(C_RIPOSTE), 2.6f, pc().element, 1.f);
                return 0;
            }
            float stamCost = amount * 0.6f;
            stamina -= stamCost; staminaRegenDelay = 0.9f;
            play("block", 0.9f); particles.sparks(hitPos + to * 0.4f, to, 10, vec3(1, 0.85f, 0.5f));
            haptic(20, 120);
            t->knock += -to * 3.f;
            if (stamina > 0) { amount *= 0.2f; poiseDmg *= 0.2f; }
            else { stamina = 0; t->hitStun = 1.2f; t->vis.anim.play(&clip(C_HIT_HEAVY), 1.f); floatText(hitPos, "Garde brisée", vec3(1, 0.5f, 0.3f)); }
        }
    }
    // ---- damage model
    Rng r((uint64_t)(realTime * 7919) + dst);
    float variance = r.range(0.92f, 1.08f);
    float def = t->def;
    float dmg = amount * variance * (100.f / (100.f + def * 1.6f));
    bool crit = false;
    if (src == player && canCrit) {
        float cc = critChance;
        Animator& a = pc().vis.anim;
        if (a.action() == &clip(C_RIPOSTE) || a.action() == &clip(C_DG_HEAVY)) cc = 1.f;   // ripostes & ambushes always crit
        if (r.f() < cc) { crit = true; dmg *= critDmg; }
        // class affinities
        int st = pc().vis.anim.stance;
        if (st == ST_BOW) dmg *= 1.f + skillBonusPublic(skills, 0);
        if (st == ST_DAGGER) dmg *= 1.f + skillBonusPublic(skills, 1);
        if (t->vis.arch != Arch::Humanoid) dmg *= 1.f + skillBonusPublic(skills, 2);
    }
    // elemental affinity
    std::string tag;
    if (t->enemyDef >= 0) {
        const EnemyDef& d = enemies()[t->enemyDef];
        if (elem == d.weak && elem != Element::Physical) { dmg *= 1.6f; tag = "Faiblesse !"; }
        else if (elem == d.resist && elem != Element::Physical) { dmg *= 0.45f; tag = "Résiste"; }
    }
    if (dst == player) dmg *= 1.f - std::min(0.6f, resist[(int)elem]);
    if (t->status.freeze > 0 && elem == Element::Physical) dmg *= 1.3f;  // shatter
    // shield absorbs first
    if (t->status.shield > 0) { float ab = std::min(t->status.shield, dmg); t->status.shield -= ab; dmg -= ab; if (ab > 0) particles.magicBurst(hitPos, elementColor(Element::Light), 0.5f, 8); }
    dmg = std::max(1.f, dmg);
    t->hp -= dmg;
    t->vis.flash = 0.35f;
    t->lastHitBy = (float)realTime;
    // poise / stagger
    t->poise -= poiseDmg * (crit ? 1.5f : 1.f);
    bool hyper = t->attackClip && t->bossDef >= 0;
    if (t->poise <= 0 && !hyper) {
        t->poise = t->maxPoise;
        t->hitStun = t->bossDef >= 0 ? 1.2f : 0.75f;
        t->attackClip = nullptr;
        ClipId hc = t->vis.arch == Arch::Humanoid ? C_HIT_HEAVY : (t->vis.arch == Arch::Spider ? C_S_HIT : (t->vis.arch == Arch::Dragon ? C_D_HIT : C_Q_HIT));
        t->vis.anim.play(&clip(hc), 1.f, false, 0.05f);
        if (src == player) floatText(hitPos + vec3(0, 0.6f, 0), "Déséquilibré", vec3(1, 0.85f, 0.4f), 18);
    } else if (!t->attackClip && t->vis.arch == Arch::Humanoid && dst != player) {
        t->vis.anim.play(&clip(C_HIT_LIGHT), 1.2f, true, 0.04f);
    }
    t->knock += vec3(dir.x, 0, dir.z) * ((crit ? 5.f : 3.f) / std::max(0.6f, t->mass));
    // status from element
    if (elem != Element::Physical) applyStatus(*t, elem, dmg / std::max(1.f, t->maxHp));
    // feedback
    bool metal = t->vis.arch == Arch::Humanoid && t->enemyDef >= 0 && (enemies()[t->enemyDef].hlook.body == Body::Plate || enemies()[t->enemyDef].hlook.species == Species::Golem);
    hitFx(hitPos, dir, elem, crit ? 1.4f : 1.f, metal);
    play3D(metal ? "hit_metal" : (t->vis.arch == Arch::Humanoid && enemies().size() && t->enemyDef >= 0 && enemies()[t->enemyDef].hlook.species == Species::Treant ? "hit_wood" : "hit_flesh"), hitPos, crit ? 1.f : 0.8f, r.range(0.9f, 1.1f));
    char b[32]; snprintf(b, sizeof b, "%d", (int)std::round(dmg));
    vec3 tc = dst == player ? vec3(1.f, 0.35f, 0.3f) : (crit ? vec3(1.f, 0.8f, 0.2f) : (elem != Element::Physical ? elementColor(elem) : vec3(1, 1, 1)));
    floatText(hitPos + vec3(r.range(-0.3f, 0.3f), 0.3f, r.range(-0.3f, 0.3f)), crit ? std::string(b) + " !" : b, tc, crit ? 30.f : 22.f);
    if (!tag.empty() && src == player) floatText(hitPos + vec3(0, 0.9f, 0), tag, elementColor(elem), 18);
    if (src == player) { hitStop = std::max(hitStop, crit ? 0.09f : 0.045f); haptic(crit ? 30 : 15, crit ? 200 : 90); if (crit) addShake(0.25f); }
    if (dst == player) { haptic(45, 200); addShake(0.3f); play("player_hurt", 0.7f); deathT = 0; }
    if (t->hp <= 0) killCharacter(dst, *t, src);
    // aggro
    if (AIBrain* ai = reg.get<AIBrain>(dst)) { if (ai->state != AIState::Intro) { ai->target = src; if (ai->state == AIState::Idle || ai->state == AIState::Patrol || ai->state == AIState::Return) ai->state = AIState::Chase; } }
    if (AnimalBrain* ab = reg.get<AnimalBrain>(dst)) ab->fleeing = true;
    (void)isSpell;
    return dmg;
}

void Game::areaDamage(Entity src, vec3 center, float radius, float amount, Element elem, Team team, float poise) {
    std::vector<Entity> hits;
    reg.each<Character>([&](Entity e, Character& c) {
        if (c.dead || e == src) return;
        bool hostile = (team == Team::Player) ? (c.team == Team::Enemy || c.team == Team::Animal) : (c.team == Team::Player);
        if (!hostile) return;
        float d = distXZ(c.pos, center);
        if (d < radius + c.radius && std::fabs(c.pos.y - center.y) < 4.f + radius * 0.3f) hits.push_back(e);
    });
    for (Entity e : hits) {
        Character* c = reg.get<Character>(e);
        if (!c) continue;
        if (e == player && c->invuln > 0) continue;
        vec3 dir = normalize(vec3(c->pos.x - center.x, 0, c->pos.z - center.z) + vec3(0.001f, 0, 0));
        float fall = 1.f - 0.4f * saturate(distXZ(c->pos, center) / std::max(radius, 0.1f));
        dealDamage(src, e, amount * fall, elem, dir, poise, team == Team::Player, true);
    }
}

void Game::applyStatus(Character& c, Element e, float power) {
    float p = clampf(power * 6.f, 0.3f, 1.f);
    switch (e) {
    case Element::Fire: c.status.burn = std::max(c.status.burn, 3.f * p); break;
    case Element::Ice: c.status.slow = std::max(c.status.slow, 2.5f * p); if (power > 0.12f) c.status.freeze = std::max(c.status.freeze, 1.2f); break;
    case Element::Lightning: c.status.stun = std::max(c.status.stun, 0.35f); break;
    case Element::Nature: c.status.poison = std::max(c.status.poison, 4.f * p); break;
    case Element::Shadow: c.status.slow = std::max(c.status.slow, 1.f); break;
    default: break;
    }
}

void Game::updateStatus(Entity e, Character& c, float dt) {
    Status& s = c.status;
    auto dec = [&](float& v) { v = std::max(0.f, v - dt); };
    if (s.burn > 0) { c.hp -= dt * std::max(2.f, c.maxHp * 0.012f); if (particles.rng.f() < dt * 12) particles.fire(c.pos + vec3(0, c.height * 0.5f, 0), 0.4f, 0.03f); }
    if (s.poison > 0) { c.hp -= dt * std::max(1.5f, c.maxHp * 0.008f); if (particles.rng.f() < dt * 6) particles.magicTrail(c.pos + vec3(0, c.height * 0.7f, 0), elementColor(Element::Nature), 0.5f); }
    dec(s.burn); dec(s.poison); dec(s.freeze); dec(s.slow); dec(s.root); dec(s.stun); dec(s.regen); dec(s.strength); dec(s.frostRes); dec(s.wellFed);
    c.vis.tint = s.freeze > 0 ? vec3(0.6f, 0.85f, 1.3f) : (s.poison > 0 ? vec3(0.8f, 1.1f, 0.7f) : vec3(1));
    if (c.hp <= 0 && !c.dead) killCharacter(e, c, NullEntity);
}

void Game::killCharacter(Entity e, Character& c, Entity killer) {
    if (c.dead) return;
    c.dead = true; c.hp = 0; c.deathT = 0; c.attackClip = nullptr;
    ClipId dc = c.vis.arch == Arch::Humanoid ? C_DEATH : (c.vis.arch == Arch::Spider ? C_S_DEATH : (c.vis.arch == Arch::Dragon ? C_D_DEATH : C_Q_DEATH));
    c.vis.anim.play(&clip(dc), 1.f, false, 0.05f);
    c.vis.anim.setState(BodyState::Dead, 1.0f);
    if (e == player) {
        play("death"); haptic(200, 255); addShake(0.5f);
        deathT = 0; lockTarget = NullEntity;
        if (riding) riding = false;
        return;
    }
    if (c.team == Team::Enemy) {
        const EnemyDef& d = enemies()[c.enemyDef];
        static const char* deathSnd[] = {"growl", "skeleton", "goblin", "troll", "golem", "spider", "ghost"};
        const char* snd = d.family == "Squelettes" ? deathSnd[1] : d.family == "Gobelins" ? deathSnd[2] : d.family == "Trolls" ? deathSnd[3] : (d.family == "Golems" || d.family == "Élémentaires") ? deathSnd[4] : d.family == "Araignées géantes" ? deathSnd[5] : d.family == "Morts-vivants" ? deathSnd[6] : deathSnd[0];
        play3D(snd, c.pos, 0.9f, 0.85f);
        bestiary[d.id]++;
        if (bestiary[d.id] == 1) notify("Bestiaire : " + d.name, vec3(0.8f, 0.9f, 1.f), "i_bestiary");
        int xpGain = (int)(d.xp * (c.elite ? 2.5f : 1.f) * (c.bossDef >= 0 ? 1.f : 1.f));
        gainXp(xpGain);
        floatText(c.pos + vec3(0, c.height + 0.5f, 0), "+" + std::to_string(xpGain) + " XP", vec3(0.6f, 0.9f, 1.f), 18);
        advanceQuests(StepKind::Kill, d.id, 1);
        if (c.elite) killedUnique.insert(c.uid);
        dropLoot(c, c.pos);
        // remember cleared chunks so groups do not respawn instantly (10 minutes)
        ChunkKey k = chunkOf(c.pos.x, c.pos.z);
        char key[32]; snprintf(key, sizeof key, "%d_%d", k.x, k.z);
        clearedSpawns[key] = gameTime + 600.0;
        if (lockTarget == e) lockTarget = NullEntity;
        if (c.bossDef >= 0) {
            const BossDef& b = bosses()[c.bossDef];
            defeatedBosses.insert(b.id);
            bossEnt = NullEntity;
            advanceQuests(StepKind::KillBoss, b.id, 1);
            for (const auto& r : b.rewards) addItem(r, 1, item(r) ? std::max(item(r)->rarity, Rarity::Epic) : Rarity::Epic);
            if (!b.shard.empty()) { addItem(b.shard, 1); notify("Fragment de Couronne obtenu ! (" + std::to_string(countItem("crown_shard")) + "/6)", vec3(1.f, 0.85f, 0.35f), "i_collection"); }
            cinematicText = b.outro; cinematicT = 4.f;
            cineFrom = cam.pos; cineTo = c.pos + vec3(0, c.height * 1.2f + 4.f, 0) + normalize(cam.pos - c.pos) * 10.f; cineLook = c.pos + vec3(0, c.height * 0.4f, 0);
            pc().vis.anim.setState(BodyState::Victory, 0.4f);
            particles.dissolveBurst(c.pos, c.vis.dissolveColor, c.radius * 2.f);
            play("magic_burst");
            saveGame(99);
            if (b.id == "abyss_lord") { questsDone.insert("m7"); creditsT = 0; fadeTransition([this] { state = GameState::Credits; stateT = 0; creditsT = 0; }); }
        }
    } else if (c.team == Team::Animal) {
        addItem("meat", 1); if (c.animal == 4) addItem("leather", 1);
        advanceQuests(StepKind::Harvest, "hunt", 1);
        gainXp(6);
    }
    (void)killer;
}

void Game::dropLoot(Character& c, vec3 pos) {
    const EnemyDef& d = enemies()[c.enemyDef];
    Rng r((uint64_t)(realTime * 1000) + c.uid);
    int g = d.gold + r.irange(0, d.gold);
    if (c.elite) g *= 3;
    gold += g;
    if (g > 0) play("coin", 0.4f, 1.1f);
    for (auto& l : d.loot) {
        float ch = l.second * (c.elite ? 2.f : 1.f);
        if (r.f() < ch) spawnPickup(l.first, 1, item(l.first) ? item(l.first)->rarity : Rarity::Common, pos + vec3(r.range(-0.5f, 0.5f), 0, r.range(-0.5f, 0.5f)));
    }
    // random equipment with rolled rarity
    float eqChance = c.elite ? 0.6f : 0.07f;
    if (r.f() < eqChance) {
        const auto& items = allItems();
        std::vector<int> pool;
        int tier = std::min(5, d.level / 4 + 1);
        for (int i = 0; i < (int)items.size(); i++) {
            const ItemDef& it = items[i];
            if ((it.kind == ItemKind::Weapon || it.kind == ItemKind::Head || it.kind == ItemKind::Chest || it.kind == ItemKind::Hands || it.kind == ItemKind::Feet || it.kind == ItemKind::Ring || it.kind == ItemKind::Amulet) && it.tier <= tier && it.tier >= tier - 2 && (int)it.rarity <= (int)Rarity::Epic) pool.push_back(i);
        }
        if (!pool.empty()) {
            const ItemDef& it = items[pool[r.irange(0, (int)pool.size() - 1)]];
            float roll = r.f() + (c.elite ? 0.25f : 0.f);
            Rarity rr = roll < 0.55f ? Rarity::Common : roll < 0.85f ? Rarity::Rare : roll < 0.97f ? Rarity::Epic : roll < 1.15f ? Rarity::Legendary : Rarity::Mythic;
            if ((int)rr < (int)it.rarity) rr = it.rarity;
            spawnPickup(it.id, 1, rr, pos + vec3(0, 0.2f, 0));
        }
    }
    if (r.f() < 0.12f) spawnPickup(r.chance(0.6f) ? "potion_minor" : "potion_mana", 1, Rarity::Common, pos);
}

// ============================================================================================ attacks
void Game::updateAttack(Entity e, Character& c, float dt) {
    Animator& a = c.vis.anim;
    if (!c.attackClip) return;
    if (a.action() != c.attackClip) { c.attackClip = nullptr; return; }
    float t = a.actionTime();
    // root motion
    float m = c.attackClip->motionAt(std::min(t, 1.f));
    float dm = (m - c.lastMotion) * c.vis.scale;
    c.lastMotion = m;
    if (std::fabs(dm) > 0) {
        vec3 fwd{std::sin(c.yaw), 0, std::cos(c.yaw)};
        // don't lunge through the target
        if (dm > 0 && e != player) { AIBrain* ai = reg.get<AIBrain>(e); Character* tg = ai && reg.valid(ai->target) ? reg.get<Character>(ai->target) : nullptr; if (tg && distXZ(tg->pos, c.pos) < c.radius + tg->radius + 0.4f) dm = 0; }
        if (dm > 0 && e == player && lockTarget != NullEntity) { Character* tg = reg.get<Character>(lockTarget); if (tg && distXZ(tg->pos, c.pos) < c.radius + tg->radius + 0.3f) dm = 0; }
        c.pos += fwd * dm;
    }
    // events
    for (int ev : a.firedEvents) {
        vec3 fwd{std::sin(c.yaw), 0, std::cos(c.yaw)};
        if (ev == EV_RELEASE && e == player) {
            int stance = a.stance;
            Entity t2 = lockTarget != NullEntity ? lockTarget : findTarget(c.pos, c.yaw, 26.f, Team::Enemy, 0.3f);
            Character* tc = t2 != NullEntity ? reg.get<Character>(t2) : nullptr;
            vec3 from = c.pos + vec3(0, 1.45f, 0) + fwd * 0.5f;
            vec3 dir = tc ? normalize(tc->pos + vec3(0, tc->height * 0.55f, 0) - from) : fwd;
            float bow = 1.f + skillBonusPublic(skills, 0);
            if (stance == ST_BOW) { fireProjectile(player, ProjKind::Arrow, from, dir, c.attackMult > 1.5f ? 42.f : 34.f, c.atk * c.attackMult * bow, c.element, Team::Player); play("bow", 0.8f); }
            else if (stance == ST_STAFF) { fireProjectile(player, c.element == Element::Fire ? ProjKind::Fireball : (c.element == Element::Shadow ? ProjKind::Shadow : ProjKind::Arcane), from, dir, 20.f, (c.atk + c.elemDmg) * c.attackMult, c.element == Element::Physical ? Element::Arcane : c.element, Team::Player, t2); play("arcane", 0.7f); }
        }
        if (ev == EV_IMPACT) {
            vec3 p = c.pos + fwd * (1.4f * c.vis.scale);
            particles.shockwave(p, vec3(0.8f, 0.7f, 0.55f), 1.5f * c.vis.scale);
            particles.dust(p, 1.5f * c.vis.scale);
            play3D("explosion", p, 0.45f, 1.3f);
            if (e == player) {
                addShake(0.35f); haptic(40, 180);
                if (a.stance == ST_TWOHAND || a.stance == ST_STAFF) areaDamage(player, p, 2.6f, c.atk * c.attackMult * 0.6f, a.stance == ST_STAFF ? Element::Arcane : c.element, Team::Player, 35);
            } else if (distXZ(p, pc().pos) < 15) addShake(0.25f * c.vis.scale);
        }
        if (ev == EV_SOUND_WHOOSH && e != player) play3D("swing", c.pos, 0.6f, 0.8f / std::sqrt(c.vis.scale));
    }
    // hit window: weapon sweep against hostiles in front
    if (a.actionInHitWindow() && c.attackIdx != -2) {
        float reach = (e == player ? stanceReach(a.stance) : 1.8f) * (e == player ? 1.f : c.vis.scale);
        if (e != player) { AIBrain* ai = reg.get<AIBrain>(e); if (ai && ai->pendingAttack >= 0 && c.enemyDef >= 0) reach = enemies()[c.enemyDef].attacks[ai->pendingAttack].range; }
        bool spin = isSpin(c.attackClip);
        vec3 fwd{std::sin(c.yaw), 0, std::cos(c.yaw)};
        Team hostileA = c.team == Team::Player ? Team::Enemy : Team::Player;
        std::vector<Entity> victims;
        reg.each<Character>([&](Entity o, Character& oc) {
            if (o == e || oc.dead) return;
            bool hostile = oc.team == hostileA || (c.team == Team::Player && oc.team == Team::Animal);
            if (!hostile) return;
            if (std::find(c.hitThisSwing.begin(), c.hitThisSwing.end(), o) != c.hitThisSwing.end()) return;
            vec3 d = oc.pos - c.pos; d.y = 0;
            float dist = length(d);
            if (dist > reach + oc.radius) return;
            if (std::fabs(oc.pos.y - c.pos.y) > 2.5f + c.vis.scale) return;
            if (!spin && dist > 0.3f && dot(d / dist, fwd) < 0.25f) return;
            victims.push_back(o);
        });
        for (Entity o : victims) {
            c.hitThisSwing.push_back(o);
            Character* oc = reg.get<Character>(o);
            vec3 dir = normalize(vec3(oc->pos.x - c.pos.x, 0, oc->pos.z - c.pos.z) + vec3(0.001f, 0, 0));
            float dmg = c.atk * c.attackMult * (c.status.strength > 0 ? 1.2f : 1.f);
            dealDamage(e, o, dmg, Element::Physical, dir, 18.f * c.attackMult, true, false);
            if (c.elemDmg > 0 && reg.valid(o)) dealDamage(e, o, c.elemDmg * c.attackMult, c.element, dir, 0, false, false);
        }
    }
    if (a.actionDone() || (t >= 1.f)) c.attackClip = nullptr;
}

void Game::updateTrails(Character& c, float dt) {
    Visual& v = c.vis;
    bool active = c.attackClip && v.anim.actionInHitWindow() && v.weapon && v.anim.stance != ST_BOW;
    if (active || (c.attackClip && v.anim.actionTime() > (c.attackClip->hitStart - 0.08f) && v.anim.actionTime() < c.attackClip->hitEnd + 0.05f && v.weapon)) {
        mat4 model = mat4::trs(c.pos, quat::axisAngle({0, 1, 0}, c.yaw), vec3(v.scale));
        mat4 hand = model * v.pw.world[H_PROPR];
        v.trail.push_back({hand.transformPoint({0, 0, v.tip * 0.25f}), hand.transformPoint({0, 0, v.tip})});
        while (v.trail.size() > 12) v.trail.pop_front();
    } else if (!v.trail.empty()) {
        v.trail.pop_front();
        if (!v.trail.empty()) v.trail.pop_front();
    }
    (void)dt;
}

// ============================================================================================ projectiles
void Game::fireProjectile(Entity owner, ProjKind k, vec3 from, vec3 dir, float speed, float dmg, Element e, Team team, Entity homing) {
    Entity pe = reg.create();
    Projectile p;
    p.kind = k; p.pos = from; p.vel = dir * speed; p.dmg = dmg; p.elem = e; p.owner = owner; p.team = team; p.homing = homing;
    switch (k) {
    case ProjKind::Arrow: p.gravity = 4.f; p.radius = 0.25f; p.life = 2.5f; break;
    case ProjKind::Fireball: p.splash = 2.6f; p.radius = 0.4f; break;
    case ProjKind::IceSpike: p.pierce = true; p.radius = 0.35f; p.slow = 2.f; break;
    case ProjKind::Boulder: p.gravity = 9.f; p.radius = 0.9f; p.splash = 2.5f; p.life = 4.f; break;
    case ProjKind::Web: p.radius = 0.5f; p.slow = 3.f; p.gravity = 3.f; break;
    default: p.radius = 0.35f; break;
    }
    reg.add<Projectile>(pe, p);
}

void Game::updateProjectiles(float dt) {
    reg.each<Projectile>([&](Entity pe, Projectile& p) {
        p.life -= dt;
        if (p.homing != NullEntity && reg.valid(p.homing)) {
            Character* t = reg.get<Character>(p.homing);
            if (t && !t->dead) {
                vec3 want = normalize(t->pos + vec3(0, t->height * 0.5f, 0) - p.pos) * length(p.vel);
                p.vel = lerp(p.vel, want, std::min(1.f, dt * 4.f));
            }
        }
        p.vel.y -= p.gravity * dt;
        vec3 prev = p.pos;
        p.pos += p.vel * dt;
        vec3 col = elementColor(p.elem);
        // trail fx
        switch (p.kind) {
        case ProjKind::Fireball: particles.magicTrail(p.pos, col, 1.2f); if (particles.rng.f() < 0.5f) particles.fire(p.pos, 0.35f, dt); break;
        case ProjKind::IceSpike: particles.magicTrail(p.pos, col, 0.8f); break;
        case ProjKind::Arcane: case ProjKind::Shadow: case ProjKind::Holy: case ProjKind::Nature: case ProjKind::Lightning: particles.magicTrail(p.pos, col, 0.9f); break;
        case ProjKind::Web: particles.magicTrail(p.pos, vec3(0.8f, 0.9f, 0.7f), 0.7f); break;
        case ProjKind::Arrow: if (p.elem != Element::Physical) particles.magicTrail(p.pos, col, 0.4f); break;
        default: break;
        }
        bool done = p.life <= 0;
        vec3 hitPoint = p.pos;
        // terrain / walls
        float gy = groundAt(p.pos.x, p.pos.z);
        if (p.pos.y < gy) { done = true; hitPoint = vec3(p.pos.x, gy, p.pos.z); }
        vec3 tmp = p.pos;
        if (!done && collideStatic(tmp, 0.05f, p.pos.y - 0.5f)) { done = true; hitPoint = prev; }
        // characters
        if (!done) {
            reg.each<Character>([&](Entity e, Character& c) {
                if (done || c.dead || e == p.owner) return;
                bool hostile = p.team == Team::Player ? (c.team == Team::Enemy || c.team == Team::Animal) : (c.team == Team::Player);
                if (!hostile) return;
                if (std::find(p.hit.begin(), p.hit.end(), e) != p.hit.end()) return;
                vec3 center = c.pos + vec3(0, c.height * 0.5f, 0);
                float r = c.radius + p.radius + 0.2f;
                vec3 seg = p.pos - prev;
                float tt = saturate(dot(center - prev, seg) / std::max(1e-5f, dot(seg, seg)));
                vec3 q = prev + seg * tt;
                float dy = std::fabs(q.y - center.y);
                if (length(vec2(q.x - center.x, q.z - center.z)) < r && dy < c.height * 0.6f + p.radius) {
                    p.hit.push_back(e);
                    if (p.splash <= 0) dealDamage(p.owner, e, p.dmg, p.elem, normalize(p.vel), p.kind == ProjKind::Boulder ? 60.f : 15.f, p.team == Team::Player, true);
                    if (p.slow > 0) { c.status.slow = std::max(c.status.slow, p.slow); if (p.kind == ProjKind::Web) c.status.root = 1.2f; }
                    if (p.kind == ProjKind::Arrow) play3D("arrow_hit", q, 0.7f);
                    if (!p.pierce) { done = true; hitPoint = q; }
                }
            });
        }
        if (done) {
            if (p.splash > 0) {
                areaDamage(p.owner, hitPoint, p.splash, p.dmg, p.elem, p.team, p.kind == ProjKind::Boulder ? 60.f : 25.f);
                particles.explosion(hitPoint, p.kind == ProjKind::Boulder ? 0.8f : 0.7f, p.kind == ProjKind::Boulder ? vec3(0.6f, 0.55f, 0.5f) : col);
                play3D("explosion", hitPoint, 0.8f);
                if (distXZ(hitPoint, pc().pos) < 12) addShake(0.3f);
            } else if (p.kind != ProjKind::Arrow) particles.magicBurst(hitPoint, col, 0.6f, 12);
            else particles.dust(hitPoint, 0.4f);
            reg.destroyLater(pe);
        }
    });
}

// ============================================================================================ physics & character update
void Game::physicsStep(Entity e, Character& c, float dt) {
    bool frozen = c.status.freeze > 0 || c.status.stun > 0;
    if (!c.grounded || c.swimming) c.vel.y -= (c.swimming ? 0.f : 20.f) * dt;
    vec3 move = vec3(c.vel.x, 0, c.vel.z) * (frozen ? 0.f : 1.f) + c.knock;
    c.pos += move * dt + vec3(0, c.vel.y * dt, 0);
    c.knock = c.knock * std::exp(-8.f * dt);
    // static collision
    float step = 0;
    collideStatic(c.pos, c.radius, c.pos.y, &step);
    // ground, water, slopes
    float gy = groundAt(c.pos.x, c.pos.z);
    if (step > gy) gy = step;
    bool lava = false, frozenW = false;
    float wl = waterAt(c.pos.x, c.pos.z, &lava, &frozenW);
    if (frozenW && wl > gy) gy = wl;  // walk on ice
    c.groundY = gy;
    float depth = (wl > -1e8f && !frozenW) ? wl - gy : 0.f;
    c.inWater = depth > 0.15f && c.pos.y < wl + 0.1f;
    bool wasSwim = c.swimming;
    c.swimming = depth > 1.3f && !lava && c.team != Team::Mount;
    if (lava && depth > 0.1f && c.pos.y < wl + 0.2f) {
        // lava: burns and throws you back to solid ground
        if (c.invuln <= 0) { c.hp -= c.maxHp * 0.15f; c.status.burn = 3; c.invuln = 0.6f; c.vel.y = 7.f; c.knock = -normalize(vec3(c.vel.x, 0, c.vel.z) + vec3(0.01f, 0, 0)) * 6.f; particles.explosion(c.pos, 0.4f); if (e == player) { play("player_hurt"); haptic(80, 255); } if (c.hp <= 0) killCharacter(e, c, NullEntity); }
    }
    if (c.swimming) {
        float surf = wl - (c.team == Team::Player ? 1.25f : c.height * 0.6f);
        c.pos.y = damp(c.pos.y, surf, 6.f, dt);
        c.vel.y = 0; c.grounded = true;
        if (!wasSwim) { particles.splash(c.pos + vec3(0, 1.2f, 0), 1.f); play3D("splash", c.pos, 0.8f); }
        if (e == player) { stamina -= dt * 2.f; staminaRegenDelay = 0.2f; if (stamina <= 0) { c.hp -= dt * 5.f; if (c.hp <= 0) killCharacter(e, c, NullEntity); } }
    } else if (c.pos.y <= gy + 0.02f || (c.grounded && c.pos.y - gy < 0.45f && c.vel.y <= 0.1f)) {
        if (!c.grounded && c.airTime > 0.35f) {
            if (e == player) { c.vis.anim.play(&clip(C_LAND), 1.f, false, 0.03f); play("land", 0.6f); particles.dust(c.pos, 1.f); haptic(20, 80); if (c.airTime > 1.6f && !dungeon.active) { float fd = (c.airTime - 1.6f) * c.maxHp * 0.25f; dealDamage(NullEntity, e, fd, Element::Physical, vec3(0), 0, false); } }
        }
        c.pos.y = gy; c.vel.y = 0; c.grounded = true; c.airTime = 0;
        // steep slopes push characters down (no climbing cliffs)
        if (!dungeon.active) {
            vec3 n = world.normal(c.pos.x, c.pos.z);
            if (n.y < 0.66f) { c.pos.x += n.x * dt * 6.f; c.pos.z += n.z * dt * 6.f; }
        }
    } else {
        c.grounded = false;
        c.airTime += dt;
    }
    // world bounds (the sea is the boundary)
    if (!dungeon.active || c.pos.x < WORLD_SIZE) {
        c.pos.x = clampf(c.pos.x, 60.f, WORLD_SIZE - 60.f);
        c.pos.z = clampf(c.pos.z, 60.f, WORLD_SIZE - 60.f);
    }
    // animator state
    Animator& a = c.vis.anim;
    a.grounded = c.grounded;
    a.verticalVel = c.vel.y;
    if (!c.dead && !(e == player && riding)) {
        BodyState want = c.swimming ? BodyState::Swim : (!c.grounded && c.airTime > 0.12f ? BodyState::Airborne : (c.hitStun > 0.3f && c.vis.arch == Arch::Humanoid && !a.action() ? BodyState::Stagger : BodyState::Locomotion));
        if (a.state == BodyState::Victory && e == player && cinematicT > 0) want = BodyState::Victory;
        if (a.state != want && a.state != BodyState::Sit && a.state != BodyState::Sleep && a.state != BodyState::Work) a.setState(want, want == BodyState::Airborne ? 0.15f : 0.25f);
    }
}

void Game::updateCharacters(float dt) {
    // separation between characters (cheap O(n^2) on the active set)
    std::vector<std::pair<Entity, Character*>> act;
    reg.each<Character>([&](Entity e, Character& c) { if (!c.dead && distXZ(c.pos, camFocus) < 70.f) act.push_back({e, &c}); });
    for (size_t i = 0; i < act.size(); i++)
        for (size_t j = i + 1; j < act.size(); j++) {
            Character& a = *act[i].second; Character& b = *act[j].second;
            if ((act[i].first == player && act[j].first == mountEnt) || (act[j].first == player && act[i].first == mountEnt)) continue;
            vec2 d = b.pos.xz() - a.pos.xz();
            float l = length(d), r = a.radius + b.radius;
            if (l < r && l > 1e-4f && std::fabs(a.pos.y - b.pos.y) < 2.f) {
                vec2 push = d * ((r - l) / l);
                float wa = b.mass / (a.mass + b.mass), wb = a.mass / (a.mass + b.mass);
                a.pos.x -= push.x * wa; a.pos.z -= push.y * wa;
                b.pos.x += push.x * wb; b.pos.z += push.y * wb;
            }
        }
    reg.each<Character>([&](Entity e, Character& c) {
        float d2 = dist2XZ(c.pos, camFocus);
        bool near = d2 < 90.f * 90.f || c.bossDef >= 0;
        if (!near && e != player) return;
        c.invuln = std::max(0.f, c.invuln - dt);
        c.hitStun = std::max(0.f, c.hitStun - dt);
        c.vis.flash = std::max(0.f, c.vis.flash - dt * 3.5f);
        if (!c.dead && c.poise < c.maxPoise && realTime - c.lastHitBy > 3.f) c.poise = std::min(c.maxPoise, c.poise + dt * c.maxPoise * 0.3f);
        updateStatus(e, c, dt);
        if (e != player || !riding) {
            if (!c.dead) {
                if (AIBrain* ai = reg.get<AIBrain>(e)) { if (c.bossDef >= 0) updateBossAI(e, c, *ai, dt); else updateEnemyAI(e, c, *ai, dt); }
                else if (NpcBrain* nb = reg.get<NpcBrain>(e)) updateNpc(e, c, *nb, dt);
                else if (AnimalBrain* ab = reg.get<AnimalBrain>(e)) updateAnimal(e, c, *ab, dt);
                else if (c.team == Team::Mount && !(riding && e == mountEnt)) {
                    Character& pl = pc();
                    float d = distXZ(c.pos, pl.pos);
                    vec2 mv{0, 0};
                    if (e == mountEnt && mountCalled && d > 2.2f) mv = steer(c, pl.pos, dt) * std::min(c.vis.anim.gait.runSpeed, 2.f + d);
                    c.vel.x = damp(c.vel.x, mv.x, 4, dt); c.vel.z = damp(c.vel.z, mv.y, 4, dt);
                    float hs = length(vec2(c.vel.x, c.vel.z));
                    if (hs > 0.3f) c.yaw = dampAngle(c.yaw, std::atan2(c.vel.x, c.vel.z), 6, dt);
                    c.vis.anim.speed = hs;
                    if (e == mountEnt && mountCalled && d < 2.6f && !pl.dead) toggleMount();
                }
            }
            updateAttack(e, c, dt);
            if (!(e == player && riding)) physicsStep(e, c, dt);
        }
        if (e == mountEnt && riding) { c.pos.y = groundAt(c.pos.x, c.pos.z); vec3 tmp = c.pos; collideStatic(c.pos, c.radius, c.pos.y); c.pos += vec3(c.vel.x, 0, c.vel.z) * dt; (void)tmp;
            if (c.vis.anim.speed > 0.5f && c.vis.anim.footstepEvent) { c.vis.anim.footstepEvent = 0; play3D("horse", c.pos, 0.25f, 1.4f); } }
        if (c.dead) {
            c.deathT += dt;
            if (e != player && c.deathT > 1.4f) {
                if (c.vis.dissolve == 0) { particles.dissolveBurst(c.pos, c.vis.dissolveColor, c.radius * c.vis.scale); }
                c.vis.dissolve = std::min(1.f, (c.deathT - 1.4f) / 1.6f);
                if (particles.rng.f() < dt * 20) particles.dissolveBurst(c.pos, c.vis.dissolveColor, c.radius * 0.6f);
                if (c.vis.dissolve >= 1.f) reg.destroyLater(e);
            }
        }
        c.vis.anim.update(dt);
        computeWorld(*c.vis.anim.skeleton, c.vis.anim.pose, c.vis.pw);
        updateTrails(c, dt);
    });
}

}  // namespace lk
