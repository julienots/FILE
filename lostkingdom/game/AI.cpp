#include "Game.h"
#include <algorithm>

namespace lk {

vec3 rotate3(vec3 v, float a);

bool Game::canSee(const Character& a, const Character& b, float range, float fov) {
    vec3 d = b.pos - a.pos; d.y = 0;
    float dist = length(d);
    if (dist > range) return false;
    if (dist < 5.f) return true;  // too close to miss
    vec3 fwd{std::sin(a.yaw), 0, std::cos(a.yaw)};
    if (dot(d / dist, fwd) < std::cos(fov * 0.5f * DEG)) return false;
    if (dungeon.active) return true;
    return !world.lineBlocked(a.pos + vec3(0, a.height * 0.8f, 0), b.pos + vec3(0, 1.2f, 0));
}

// Local steering: seek + obstacle probing + water avoidance + separation from allies.
vec2 Game::steer(Character& c, vec3 target, float) {
    vec2 to = target.xz() - c.pos.xz();
    float d = length(to);
    if (d < 0.01f) return {0, 0};
    vec2 dir = to / d;
    // probe ahead; rotate away from obstacles and deep water
    for (int attempt = 0; attempt < 5; attempt++) {
        float ang = attempt == 0 ? 0.f : ((attempt % 2) ? 1.f : -1.f) * (0.5f + 0.35f * (attempt / 2));
        vec2 cand = rotate(dir, ang);
        vec3 probe = c.pos + vec3(cand.x, 0, cand.y) * (1.6f + c.radius);
        vec3 tmp = probe;
        bool blocked = collideStatic(tmp, c.radius * 0.8f, c.pos.y);
        float wl = waterAt(probe.x, probe.z);
        float gy = groundAt(probe.x, probe.z);
        if (wl > -1e8f && wl - gy > 1.0f) blocked = true;
        if (!dungeon.active && gy - c.pos.y > 2.2f) blocked = true;  // cliff
        if (!blocked) return cand;
    }
    return dir * 0.3f;
}

static float preferredRange(Behavior b, float atkRange) {
    switch (b) {
    case Behavior::Ranged: return 11.f;
    case Behavior::Caster: return 10.f;
    case Behavior::Charger: return 9.f;
    case Behavior::Flyer: return 7.f;
    default: return std::max(1.6f, atkRange * 0.8f);
    }
}

void Game::executeEnemyAttack(Entity e, Character& c, AIBrain& ai, const AttackDef& a) {
    Character* t = reg.valid(ai.target) ? reg.get<Character>(ai.target) : nullptr;
    const Clip* cl = &clip(a.clip);
    startAttack(e, c, cl, a.mult, a.element, 1.f);
    vec3 fwd{std::sin(c.yaw), 0, std::cos(c.yaw)};
    switch (a.type) {
    case AttackType::Summon: {
        const EnemyDef& d = enemies()[c.enemyDef];
        int n = c.bossDef >= 0 ? 3 : 1;
        if (ai.summons >= (c.bossDef >= 0 ? 9 : 3)) break;
        std::string minion = d.id;
        if (d.id == "goblin_shaman") minion = "goblin";
        else if (d.id == "b_root_guardian") minion = "wisp";
        else if (d.id == "b_skeleton_king") minion = "skeleton";
        else if (d.id == "b_spider_queen") minion = "forest_spider";
        else if (d.id == "b_abyss_lord") minion = "specter";
        int mi = enemyIndex(minion);
        for (int i = 0; i < n && mi >= 0; i++) {
            vec3 sp = c.pos + rotate3(fwd, (i - 1) * 1.2f) * (3.f + c.radius);
            sp.y = groundAt(sp.x, sp.z);
            Entity m = spawnEnemy(mi, sp, false, 0);
            AIBrain* mai = reg.get<AIBrain>(m);
            mai->state = AIState::Chase; mai->target = ai.target;
            particles.magicBurst(sp + vec3(0, 1, 0), elementColor(a.element), 1.2f, 24);
            ai.summons++;
        }
        play3D(d.arch == Arch::Quadruped ? "wolf_howl" : "magic_burst", c.pos, 0.9f);
        break;
    }
    default: break;
    }
    ai.pendingAttack = (int)(&a - &enemies()[c.enemyDef].attacks[0]);
    ai.cooldowns[ai.pendingAttack] = a.cooldown;
    (void)t;
}

vec3 rotate3(vec3 v, float a) { vec2 r = rotate(v.xz(), a); return {r.x, v.y, r.y}; }

// Handles the ongoing effects of an enemy attack (projectiles at release, breath, areas, charges).
static void attackEffects(Game& g, Entity e, Character& c, AIBrain& ai) {
    if (ai.pendingAttack < 0 || c.enemyDef < 0) return;
    const AttackDef& a = enemies()[c.enemyDef].attacks[ai.pendingAttack];
    Animator& an = c.vis.anim;
    if (!c.attackClip) { if (!(ai.state == AIState::Attack && ai.telegraphT > 0)) ai.pendingAttack = -1; return; }
    float t = an.actionTime();
    vec3 fwd{std::sin(c.yaw), 0, std::cos(c.yaw)};
    Character* tg = g.reg.valid(ai.target) ? g.reg.get<Character>(ai.target) : nullptr;
    vec3 col = elementColor(a.element);
    bool released = std::find(an.firedEvents.begin(), an.firedEvents.end(), (int)EV_RELEASE) != an.firedEvents.end();
    bool impact = std::find(an.firedEvents.begin(), an.firedEvents.end(), (int)EV_IMPACT) != an.firedEvents.end();
    switch (a.type) {
    case AttackType::Projectile: {
        bool fire = released || (an.firedEvents.empty() && c.attackClip->events.empty() && t > 0.5f && !c.hitThisSwing.size());
        if (released || (c.attackClip->events.empty() && t > 0.5f && c.hitThisSwing.empty())) {
            if (c.attackClip->events.empty()) c.hitThisSwing.push_back(NullEntity);  // fire once
            vec3 from = c.pos + vec3(0, c.height * (c.vis.arch == Arch::Humanoid ? 0.8f : 0.6f), 0) + fwd * (c.radius + 0.4f);
            vec3 aim = tg ? tg->pos + vec3(0, 1.f, 0) + tg->vel * 0.25f : from + fwd * 10.f;
            vec3 dir = normalize(aim - from);
            ProjKind k = a.clip == C_BOW_SHOT ? ProjKind::Arrow : (a.clip == C_B_THROW ? ProjKind::Boulder : (a.clip == C_S_SPIT ? ProjKind::Web : (a.element == Element::Fire ? ProjKind::Fireball : a.element == Element::Ice ? ProjKind::IceSpike : a.element == Element::Shadow ? ProjKind::Shadow : a.element == Element::Nature ? ProjKind::Nature : ProjKind::Arcane)));
            if (k == ProjKind::Boulder) { dir = normalize(dir + vec3(0, 0.25f, 0)); }
            float spd = k == ProjKind::Arrow ? 26.f : (k == ProjKind::Boulder ? 18.f : 14.f);
            int n = c.bossDef >= 0 && k != ProjKind::Boulder ? 3 : 1;
            for (int i = 0; i < n; i++) g.fireProjectile(e, k, from, normalize(rotate3(dir, (i - (n - 1) * 0.5f) * 0.22f)), spd, c.atk * a.mult, a.element, Team::Enemy);
            g.play3D(k == ProjKind::Arrow ? "bow" : (k == ProjKind::Boulder ? "swing_heavy" : "magic_burst"), c.pos, 0.7f);
        }
        (void)fire;
        break;
    }
    case AttackType::Area: case AttackType::Leap: {
        bool hitNow = impact || (c.attackClip->events.empty() && an.actionInHitWindow() && c.hitThisSwing.empty());
        if (impact || (c.attackClip->events.end() == std::find_if(c.attackClip->events.begin(), c.attackClip->events.end(), [](const AnimEvent& ev) { return ev.id == EV_IMPACT; }) && an.actionInHitWindow() && c.hitThisSwing.empty())) {
            c.hitThisSwing.push_back(NullEntity);
            vec3 center = a.type == AttackType::Leap ? c.pos + fwd * c.radius : (a.radius > 6.f ? c.pos : c.pos + fwd * (a.range * 0.5f));
            g.areaDamage(e, center, a.radius, c.atk * a.mult, a.element, Team::Enemy, 50);
            g.particles.shockwave(center, a.element == Element::Physical ? vec3(0.8f, 0.7f, 0.55f) : col, a.radius);
            g.particles.dust(center, a.radius * 0.6f);
            if (a.element == Element::Ice) g.particles.iceShards(center, 20);
            if (a.element == Element::Fire) g.particles.explosion(center, a.radius * 0.25f, col);
            if (a.element == Element::Nature) for (int i = 0; i < 8; i++) g.particles.magicBurst(center + vec3(g.particles.rng.range(-a.radius, a.radius) * 0.7f, 0.3f, g.particles.rng.range(-a.radius, a.radius) * 0.7f), col, 0.6f, 6);
            g.play3D("explosion", center, 0.8f, 0.8f);
            if (tg && distXZ(tg->pos, center) < a.radius * 2.f) g.addShake(0.4f);
        }
        (void)hitNow;
        break;
    }
    case AttackType::Breath: {
        if (an.actionInHitWindow()) {
            vec3 mouth = c.pos + vec3(0, c.height * 0.7f, 0) + fwd * (c.radius + 0.6f * c.vis.scale);
            if (c.vis.arch == Arch::Dragon) mouth = vec3(mat4::trs(c.pos, quat::axisAngle({0, 1, 0}, c.yaw), vec3(c.vis.scale)).transformPoint(c.vis.pw.wpos[Q_HEAD] + vec3(0, 0, 0.25f)));
            for (int i = 0; i < 3; i++) {
                Particle p;
                p.pos = mouth;
                p.vel = (fwd + g.particles.rng.onSphere() * 0.25f - vec3(0, 0.25f, 0)) * g.particles.rng.range(9.f, 15.f);
                p.maxLife = 0.6f; p.size0 = 0.4f; p.size1 = 1.6f; p.drag = 1.f;
                p.c0 = vec4(col * 3.f, 1); p.c1 = vec4(col * 0.5f, 0); p.frame = a.element == Element::Lightning ? PF_BOLT : PF_FLAME; p.rot = g.particles.rng.range(0, TAU);
                g.particles.spawn(p);
            }
            static float acc = 0;
            acc += 1.f / 60.f;
            if (acc > 0.25f) {
                acc = 0;
                g.reg.each<Character>([&](Entity o, Character& oc) {
                    if (oc.team != Team::Player || oc.dead) return;
                    vec3 d = oc.pos - c.pos; d.y = 0;
                    float dist = length(d);
                    if (dist < a.range && dist > 0.1f && dot(d / dist, fwd) > 0.75f) g.dealDamage(e, o, c.atk * a.mult, a.element, d / dist, 8, false, true);
                });
            }
        }
        break;
    }
    case AttackType::Beam: {
        if (an.actionTime() > 0.3f && an.actionTime() < 0.9f && tg) {
            vec3 from = c.pos + vec3(0, c.height * 0.75f, 0) + fwd;
            vec3 to = tg->pos + vec3(0, 1.f, 0);
            g.particles.lightningArc(from, to, col);
            static float acc = 0;
            acc += 1.f / 60.f;
            if (acc > 0.3f) { acc = 0; g.dealDamage(e, ai.target, c.atk * a.mult, a.element, normalize(to - from), 5, false, true); }
        }
        break;
    }
    default: break;
    }
}

void Game::updateEnemyAI(Entity e, Character& c, AIBrain& ai, float dt) {
    const EnemyDef& d = enemies()[c.enemyDef];
    Animator& an = c.vis.anim;
    for (float& cd : ai.cooldowns) cd = std::max(0.f, cd - dt);
    ai.timer -= dt;
    ai.think -= dt;
    attackEffects(*this, e, c, ai);
    Character* p = player != NullEntity ? reg.get<Character>(player) : nullptr;
    bool frozen = c.status.freeze > 0 || c.status.stun > 0 || c.hitStun > 0;
    float speed = d.speed * (c.status.slow > 0 ? 0.5f : 1.f) * (c.status.root > 0 ? 0.f : 1.f);
    vec2 moveDir{0, 0};
    float moveSpeed = 0;
    bool flyer = d.behavior == Behavior::Flyer;
    // ---- perception (cheap, a few times per second)
    if (ai.think <= 0) {
        ai.think = 0.25f;
        if (p && !p->dead && ai.state != AIState::Flee) {
            float range = d.aggro * (env.night > 0.5f && !d.night ? 0.75f : 1.f) * (c.elite ? 1.3f : 1.f);
            bool noisy = length(vec2(p->vel.x, p->vel.z)) > 6.f && distXZ(p->pos, c.pos) < range * 0.8f;
            bool sees = canSee(c, *p, range, d.fov) || noisy;
            if (d.behavior == Behavior::Ambusher && ai.state == AIState::Idle) sees = distXZ(p->pos, c.pos) < 7.f;
            if (sees) {
                ai.lastSeen = p->pos;
                ai.aggro = 6.f;
                if (ai.state == AIState::Idle || ai.state == AIState::Patrol || ai.state == AIState::Return || ai.state == AIState::Search) {
                    ai.state = AIState::Alert; ai.timer = 0.6f; ai.target = player;
                    // alert the group
                    reg.each<AIBrain, Character>([&](Entity o, AIBrain& oa, Character& oc) {
                        if (o != e && !oc.dead && oc.bossDef < 0 && distXZ(oc.pos, c.pos) < 16.f && (oa.state == AIState::Idle || oa.state == AIState::Patrol)) { oa.state = AIState::Chase; oa.target = player; oa.lastSeen = p->pos; }
                    });
                    if (d.arch == Arch::Quadruped && particles.rng.chance(0.4f)) { an.play(&clip(C_Q_HOWL), 1.4f, true); play3D("wolf_howl", c.pos, 0.6f); }
                    else if (d.arch == Arch::Humanoid && particles.rng.chance(0.3f)) an.play(&clip(C_TAUNT), 1.3f, true);
                    play3D(d.family == "Gobelins" ? "goblin" : (d.family == "Trolls" ? "troll" : "growl"), c.pos, 0.6f);
                }
            } else ai.aggro -= 0.25f;
        }
    }
    Character* t = (ai.target != NullEntity && reg.valid(ai.target)) ? reg.get<Character>(ai.target) : nullptr;
    if (t && t->dead) { ai.target = NullEntity; t = nullptr; ai.state = AIState::Return; }
    // leash
    if (distXZ(c.pos, ai.home) > 55.f && ai.state != AIState::Return) { ai.state = AIState::Return; ai.target = NullEntity; }
    // flee when badly hurt (cowardly families)
    if (c.hp < c.maxHp * 0.18f && ai.state != AIState::Flee && (d.family == "Gobelins" || d.family == "Bandits" || d.family == "Bêtes") && !c.elite) { ai.state = AIState::Flee; ai.timer = 4.f; }

    switch (ai.state) {
    case AIState::Idle:
        if (ai.timer <= 0) {
            ai.state = AIState::Patrol; Rng r(e * 7 + (uint32_t)(realTime * 10));
            vec2 o = r.inDisk() * 10.f;
            ai.patrolTarget = ai.home + vec3(o.x, 0, o.y);
            ai.timer = 8.f;
        }
        break;
    case AIState::Patrol:
        moveDir = steer(c, ai.patrolTarget, dt); moveSpeed = std::min(1.5f, speed);
        if (distXZ(c.pos, ai.patrolTarget) < 1.f || ai.timer <= 0) { ai.state = AIState::Idle; ai.timer = particles.rng.range(2.f, 6.f); }
        break;
    case AIState::Alert:
        if (t) c.yaw = dampAngle(c.yaw, std::atan2(t->pos.x - c.pos.x, t->pos.z - c.pos.z), 8, dt);
        if (ai.timer <= 0) ai.state = AIState::Chase;
        break;
    case AIState::Chase: case AIState::Strafe: {
        if (!t) { ai.state = AIState::Search; ai.timer = 5.f; break; }
        if (ai.aggro <= 0) { ai.state = AIState::Search; ai.timer = 5.f; break; }
        vec3 to = t->pos - c.pos; to.y = 0;
        float dist = length(to);
        float face = std::atan2(to.x, to.z);
        // choose an attack that is ready and in range
        int pick = -1; float best = -1;
        for (int i = 0; i < (int)d.attacks.size(); i++) {
            const AttackDef& a = d.attacks[i];
            if (ai.cooldowns[i] > 0 || a.phase > ai.phase) continue;
            float minR = (a.type == AttackType::Charge || a.type == AttackType::Leap) ? 4.f : 0.f;
            if (dist > a.range + t->radius || dist < minR) continue;
            if (a.type == AttackType::Summon && ai.summons >= 3) continue;
            float score = a.mult + particles.rng.f() * 0.6f + (a.type == AttackType::Melee ? 0.2f : 0.f);
            if (score > best) { best = score; pick = i; }
        }
        // global attack rhythm: wait between attacks so groups don't all swing at once
        bool canAttack = pick >= 0 && !c.attackClip && !frozen && ai.timer <= 0;
        if (canAttack) {
            int busy = 0;
            reg.each<AIBrain, Character>([&](Entity o, AIBrain& oa, Character& oc) { if (o != e && !oc.dead && (oa.state == AIState::Attack) && distXZ(oc.pos, t->pos) < 6) busy++; });
            if (busy >= 2 && d.attacks[pick].type == AttackType::Melee) canAttack = false;
        }
        if (canAttack) {
            ai.state = AIState::Attack;
            ai.pendingAttack = pick;
            ai.telegraphT = d.attacks[pick].telegraph * (c.elite ? 0.85f : 1.f);
            c.yaw = face;
            break;
        }
        float pref = preferredRange(d.behavior, d.attacks.empty() ? 2.f : d.attacks[0].range);
        if (d.behavior == Behavior::Pack) {
            // flank: approach at an angle depending on the entity id
            float side = (e % 2 ? 1.f : -1.f) * 0.9f;
            vec3 flank = t->pos - rotate3(normalize(to), side) * pref;
            moveDir = steer(c, flank, dt);
            moveSpeed = dist > pref + 1 ? speed : speed * 0.6f;
        } else if (dist > pref + 1.f) {
            moveDir = steer(c, t->pos, dt); moveSpeed = speed;
            ai.state = AIState::Chase;
        } else if (dist < pref - 2.f && (d.behavior == Behavior::Ranged || d.behavior == Behavior::Caster || d.behavior == Behavior::Flyer)) {
            moveDir = steer(c, c.pos - normalize(to) * 4.f, dt); moveSpeed = speed * 0.8f;   // keep distance (kiting)
        } else {
            // strafe around the target while waiting for an opening
            ai.state = AIState::Strafe;
            vec2 tangent = perp(normalize(to.xz())) * ai.strafeDir;
            moveDir = tangent; moveSpeed = speed * 0.45f;
            if (particles.rng.f() < dt * 0.4f) ai.strafeDir = -ai.strafeDir;
            // occasionally dodge back from player attacks (agile humanoids)
            Character* pc_ = reg.get<Character>(player);
            if (pc_ && pc_->attackClip && dist < 3.f && d.arch == Arch::Humanoid && d.behavior == Behavior::Melee && particles.rng.f() < dt * 1.2f && !c.attackClip) {
                an.play(&clip(C_DODGE_BACK), 1.1f, false); c.attackClip = &clip(C_DODGE_BACK); c.attackIdx = -2; c.lastMotion = 0; c.invuln = 0.25f;
            }
        }
        c.yaw = dampAngle(c.yaw, face, 7.f, dt);
        break;
    }
    case AIState::Attack: {
        if (!t) { ai.state = AIState::Chase; break; }
        if (ai.telegraphT > 0 && (ai.pendingAttack < 0 || ai.pendingAttack >= (int)d.attacks.size())) { ai.telegraphT = 0; ai.pendingAttack = -1; ai.state = AIState::Strafe; ai.timer = 0.3f; c.vis.outlineColor = c.elite ? vec4(0.9f, 0.6f, 0.1f, 1) : vec4(0.06f, 0.04f, 0.03f, 1); break; }
        if (ai.telegraphT > 0) {
            // wind-up: face target, glowing red outline = readable telegraph
            ai.telegraphT -= dt;
            c.yaw = dampAngle(c.yaw, std::atan2(t->pos.x - c.pos.x, t->pos.z - c.pos.z), 10, dt);
            c.vis.outlineColor = vec4(1.f, 0.12f + 0.12f * std::sin(realTime * 30), 0.f, 1);
            const AttackDef& a = d.attacks[ai.pendingAttack];
            if ((a.type == AttackType::Area || a.type == AttackType::Leap) && particles.rng.f() < dt * 10) {
                vec3 center = a.type == AttackType::Leap ? t->pos : (a.radius > 6 ? c.pos : c.pos + vec3(std::sin(c.yaw), 0, std::cos(c.yaw)) * a.range * 0.5f);
                Particle w; w.pos = center + vec3(0, 0.1f, 0); w.maxLife = 0.35f; w.size0 = a.radius * 2.f; w.size1 = a.radius * 2.1f; w.frame = PF_RING; w.c0 = vec4(1.f, 0.2f, 0.1f, 0.7f); w.c1 = vec4(1.f, 0.1f, 0.05f, 0);
                particles.spawn(w);
            }
            if (ai.telegraphT <= 0) {
                c.vis.outlineColor = c.elite ? vec4(0.9f, 0.6f, 0.1f, 1) : vec4(0.06f, 0.04f, 0.03f, 1);
                if (a.type == AttackType::Leap || a.type == AttackType::Charge) c.yaw = std::atan2(t->pos.x - c.pos.x, t->pos.z - c.pos.z);
                executeEnemyAttack(e, c, ai, a);
            }
            break;
        }
        if (!c.attackClip) { ai.state = AIState::Strafe; ai.timer = particles.rng.range(0.4f, 1.4f) * (c.bossDef >= 0 ? 0.6f : 1.f); }
        break;
    }
    case AIState::Flee: {
        if (t) { vec3 away = c.pos + normalize(c.pos - t->pos) * 6.f; moveDir = steer(c, away, dt); moveSpeed = speed * 1.1f; }
        if (ai.timer <= 0) { ai.state = AIState::Return; ai.target = NullEntity; }
        break;
    }
    case AIState::Search:
        moveDir = steer(c, ai.lastSeen, dt); moveSpeed = speed * 0.5f;
        if (distXZ(c.pos, ai.lastSeen) < 1.5f) moveSpeed = 0;
        if (ai.timer <= 0) { ai.state = AIState::Return; ai.target = NullEntity; }
        break;
    case AIState::Return:
        moveDir = steer(c, ai.home, dt); moveSpeed = speed * 0.7f;
        c.hp = std::min(c.maxHp, c.hp + dt * c.maxHp * 0.1f);
        if (distXZ(c.pos, ai.home) < 2.f) { ai.state = AIState::Idle; ai.timer = 3.f; ai.summons = 0; }
        break;
    default: break;
    }
    if (frozen || c.attackClip) moveSpeed = 0;
    if (c.attackClip && c.attackClip->name == "dagger_flurry") moveSpeed = 0;
    vec3 wantV = vec3(moveDir.x, 0, moveDir.y) * moveSpeed;
    c.vel.x = damp(c.vel.x, wantV.x, 7.f, dt);
    c.vel.z = damp(c.vel.z, wantV.z, 7.f, dt);
    float hs = length(vec2(c.vel.x, c.vel.z));
    if (hs > 0.3f && !c.attackClip && ai.state != AIState::Strafe && ai.state != AIState::Attack) c.yaw = dampAngle(c.yaw, std::atan2(c.vel.x, c.vel.z), 8.f, dt);
    an.speed = hs;
    an.aux[0] = (ai.state == AIState::Strafe || ai.state == AIState::Chase) ? (d.arch == Arch::Humanoid ? 0.5f : 1.f) : 0.2f;
    if (flyer) { c.pos.y = groundAt(c.pos.x, c.pos.z) + 1.4f + std::sin(realTime * 2.f + e) * 0.3f; c.vel.y = 0; c.grounded = true; if (particles.rng.f() < dt * 15) particles.magicTrail(c.pos + vec3(0, 1.2f, 0), elementColor(Element::Arcane) * 0.6f + c.vis.dissolveColor * 0.4f, 0.8f); }
}

// ============================================================================================ bosses
void Game::updateBossAI(Entity e, Character& c, AIBrain& ai, float dt) {
    const BossDef& b = bosses()[c.bossDef];
    const EnemyDef& d = enemies()[b.enemy];
    Character* p = reg.get<Character>(player);
    bossBarShow = std::min(1.f, bossBarShow + dt * 2);
    if (ai.state == AIState::Intro) {
        ai.timer -= dt;
        c.yaw = dampAngle(c.yaw, std::atan2(p->pos.x - c.pos.x, p->pos.z - c.pos.z), 4, dt);
        if (!ai.introDone) {
            ai.introDone = true;
            ClipId roar = d.arch == Arch::Dragon ? C_D_ROAR : (d.arch == Arch::Spider ? C_S_SPIT : (d.arch == Arch::Quadruped ? C_Q_HOWL : C_B_ROAR));
            c.vis.anim.play(&clip(roar), 0.8f);
            c.invuln = 4.5f;
        }
        if (ai.timer <= 0) { ai.state = AIState::Chase; ai.target = player; ai.aggro = 99; }
        c.vis.anim.speed = 0;
        return;
    }
    // phases
    float hpf = c.hp / c.maxHp;
    int phase = 0;
    for (float th : b.phases) if (hpf < th) phase++;
    if (phase > ai.phase) {
        ai.phase = phase;
        c.invuln = 1.8f; c.attackClip = nullptr; ai.pendingAttack = -1; ai.state = AIState::Strafe;
        ClipId roar = d.arch == Arch::Dragon ? C_D_ROAR : (d.arch == Arch::Spider ? C_S_SPIT : C_B_ROAR);
        c.vis.anim.play(&clip(roar), 0.9f);
        c.vis.emissiveBoost = 0.4f + phase * 0.8f;
        c.speedMul = 1.f + 0.2f * phase;
        particles.shockwave(c.pos, c.vis.dissolveColor, 8.f);
        particles.magicBurst(c.pos + vec3(0, c.height * 0.5f, 0), elementColor(d.attacks.empty() ? Element::Physical : d.attacks.back().element), 3.f, 60);
        play3D(d.arch == Arch::Dragon ? "dragon_roar" : "roar", c.pos, 1.f, 0.85f);
        addShake(0.7f); haptic(100, 255);
        notify(b.name + (phase == 1 ? " entre en rage !" : " libère toute sa puissance !"), vec3(1.f, 0.4f, 0.3f), "i_boss");
        for (auto& cd : ai.cooldowns) cd *= 0.3f;
    }
    // arena leash: if the player runs away the boss resets
    const Poi* arena = world.poiById(b.arenaPoi);
    vec3 center = ai.home;
    float leash = arena && b.dungeon < 0 ? arena->radius * 1.8f : 60.f;
    if (distXZ(p->pos, center) > leash + 25.f || p->dead) {
        c.hp = c.maxHp; ai.phase = 0; c.speedMul = 1; ai.summons = 0;
        if (distXZ(p->pos, center) > leash + 60.f || p->dead) { reg.destroyLater(e); bossEnt = NullEntity; return; }
    }
    // dragon flight phases: take off, rain fire, land with a stomp
    if (d.arch == Arch::Dragon) {
        Animator& an = c.vis.anim;
        if (ai.phase >= 1 && ai.fly <= 0 && particles.rng.f() < dt * 0.06f && !c.attackClip) { ai.fly = 0.01f; ai.timer = 9.f; play3D("dragon_roar", c.pos, 0.8f); }
        if (ai.fly > 0) {
            ai.timer -= dt;
            float target = ai.timer > 1.5f ? 1.f : 0.f;
            ai.fly = std::max(0.f, ai.fly + (target > ai.fly ? dt * 0.7f : -dt * 0.8f));
            an.aux[1] = std::min(1.f, ai.fly); an.aux[2] = 5.f;
            // circle the arena and spit fireballs
            float ang = realTime * 0.6f;
            vec3 goal = center + vec3(std::cos(ang) * 14.f, 0, std::sin(ang) * 14.f);
            vec2 dir = normalize(goal.xz() - c.pos.xz());
            c.vel = vec3(dir.x, 0, dir.y) * 9.f;
            c.yaw = dampAngle(c.yaw, std::atan2(dir.x, dir.y), 3, dt);
            an.speed = 4.f;
            if (particles.rng.f() < dt * 1.3f) {
                vec3 from = c.pos + vec3(0, c.height + 5.f, 0);
                fireProjectile(e, ProjKind::Fireball, from, normalize(p->pos + p->vel * 0.4f - from), 17.f, c.atk * 0.9f, Element::Fire, Team::Enemy);
                play3D("fireball", from, 0.8f);
            }
            if (ai.fly <= 0) {
                an.aux[1] = 0;
                executeEnemyAttack(e, c, ai, d.attacks[d.attacks.size() - 1]);
                addShake(0.6f);
            }
            if (particles.rng.f() < dt * 6) particles.dust(c.pos, 2.f);
            return;
        }
    }
    updateEnemyAI(e, c, ai, dt * c.speedMul);
    c.vis.anim.aux[0] = 0.4f;
    // keep within the arena
    if (distXZ(c.pos, center) > leash) { vec2 back = normalize(center.xz() - c.pos.xz()); c.pos.x += back.x * dt * 4; c.pos.z += back.y * dt * 4; }
    // boss auras
    if (particles.rng.f() < dt * 8 * (1 + ai.phase)) particles.dissolveBurst(c.pos, c.vis.dissolveColor * (0.5f + ai.phase * 0.4f), c.radius);
}

// ============================================================================================ villagers
void Game::updateNpc(Entity e, Character& c, NpcBrain& nb, float dt) {
    Animator& an = c.vis.anim;
    nb.timer -= dt;
    nb.waveCD -= dt;
    float h = timeOfDay;
    // threat: monsters close by → run home
    bool threat = false;
    if (realTime - c.lastHitBy < 0.f) threat = true;
    if (nb.generic && nb.act != NpcActivity::Flee && nb.timer <= 0) {
        reg.each<AIBrain, Character>([&](Entity, AIBrain& ai, Character& oc) { if (!oc.dead && (ai.state == AIState::Chase || ai.state == AIState::Attack) && distXZ(oc.pos, c.pos) < 14.f) threat = true; });
        if (threat) { nb.act = NpcActivity::Flee; nb.dest = nb.home; nb.timer = 12.f; an.setState(BodyState::Locomotion); }
    }
    bool night = h < 6.f || h > 21.5f;
    bool raining = rainAmt > 0.6f || stormAmt > 0.4f;
    if (nb.timer <= 0 && nb.act != NpcActivity::Flee) {
        Rng r(e * 31 + (uint32_t)(gameTime * 2));
        nb.timer = r.range(8.f, 20.f);
        an.setState(BodyState::Locomotion, 0.3f);
        an.stopAction();
        if (night && nb.generic) { nb.act = NpcActivity::Sleep; nb.dest = nb.bed; }
        else if (night) { nb.act = NpcActivity::Idle; nb.dest = nb.home + vec3(r.range(-2, 2), 0, r.range(-2, 2)); }
        else if (raining && nb.generic && r.chance(0.6f)) { nb.act = NpcActivity::Sleep; nb.dest = nb.home; }
        else if ((h > 12.f && h < 13.5f) || (h > 18.f && h < 21.5f)) {
            nb.act = r.chance(0.5f) ? NpcActivity::Talk : (r.chance(0.5f) ? NpcActivity::Sit : NpcActivity::Walk);
            nb.dest = nb.social + vec3(r.range(-3, 3), 0, r.range(-3, 3));
        } else {
            float roll = r.f();
            nb.act = roll < 0.45f ? NpcActivity::Work : (roll < 0.75f ? NpcActivity::Walk : NpcActivity::Idle);
            nb.dest = nb.act == NpcActivity::Work ? nb.work : (nb.act == NpcActivity::Walk ? nb.social + vec3(r.range(-14, 14), 0, r.range(-14, 14)) : c.pos);
            if (!nb.generic && nb.act == NpcActivity::Walk) nb.dest = nb.home + vec3(r.range(-4, 4), 0, r.range(-4, 4));  // shopkeepers stay near their stall
        }
        nb.dest.y = groundAt(nb.dest.x, nb.dest.z);
    }
    float d = distXZ(c.pos, nb.dest);
    vec2 mv{0, 0};
    float spd = 0;
    if (d > 0.8f) {
        mv = steer(c, nb.dest, dt);
        spd = nb.act == NpcActivity::Flee ? 4.5f : 1.3f;
        nb.indoors = false;
        if (an.state != BodyState::Locomotion) an.setState(BodyState::Locomotion, 0.3f);
    } else {
        switch (nb.act) {
        case NpcActivity::Work: if (!an.action() || an.actionDone()) an.play(&clip(nb.workClip), 1.f, true); break;
        case NpcActivity::Talk: if (!an.action() || an.actionDone()) an.play(&clip(C_TALK), 1.f, true); break;
        case NpcActivity::Sit: if (an.state != BodyState::Sit) an.setState(BodyState::Sit, 0.5f); break;
        case NpcActivity::Sleep: case NpcActivity::Flee: nb.indoors = nb.generic; break;  // goes inside
        default: break;
        }
    }
    // greet the player
    if (player != NullEntity && nb.waveCD <= 0 && !nb.indoors && nb.act != NpcActivity::Flee) {
        Character& p = pc();
        float pd = distXZ(p.pos, c.pos);
        if (pd < 6.f && pd > 2.f && !an.action()) {
            c.yaw = std::atan2(p.pos.x - c.pos.x, p.pos.z - c.pos.z);
            an.play(&clip(C_WAVE), 1.2f, true);
            nb.waveCD = 60.f;
        }
    }
    vec3 w = vec3(mv.x, 0, mv.y) * spd;
    c.vel.x = damp(c.vel.x, w.x, 6, dt); c.vel.z = damp(c.vel.z, w.z, 6, dt);
    float hs = length(vec2(c.vel.x, c.vel.z));
    if (hs > 0.2f) c.yaw = dampAngle(c.yaw, std::atan2(c.vel.x, c.vel.z), 6, dt);
    an.speed = hs;
    if (dialog.open && dialog.ent == e) { c.vel = vec3(0); an.speed = 0; if (!an.action() || an.actionDone()) an.play(&clip(C_TALK), 1.f, true); }
}

void Game::updateAnimal(Entity e, Character& c, AnimalBrain& ab, float dt) {
    Animator& an = c.vis.anim;
    ab.timer -= dt;
    Character& p = pc();
    float pd = distXZ(p.pos, c.pos);
    float flee = ab.kind == 5 ? 8.f : 13.f;
    if (pd < flee && length(vec2(p.vel.x, p.vel.z)) > 1.f) ab.fleeing = true;
    vec2 mv{0, 0}; float spd = 0;
    if (ab.fleeing) {
        vec3 away = c.pos + normalize(c.pos - p.pos + vec3(0.01f, 0, 0)) * 8.f;
        mv = steer(c, away, dt); spd = an.gait.runSpeed;
        if (pd > flee * 2.5f) ab.fleeing = false;
    } else {
        if (ab.timer <= 0) {
            Rng r(e * 13 + (uint32_t)(gameTime * 3));
            ab.timer = r.range(3.f, 9.f);
            vec2 o = r.inDisk() * 12.f;
            ab.dest = r.chance(0.4f) ? c.pos : ab.home + vec3(o.x, 0, o.y);
        }
        if (distXZ(c.pos, ab.dest) > 0.7f) { mv = steer(c, ab.dest, dt); spd = ab.kind == 5 ? 2.5f : 1.2f; }
    }
    vec3 w = vec3(mv.x, 0, mv.y) * spd;
    c.vel.x = damp(c.vel.x, w.x, 5, dt); c.vel.z = damp(c.vel.z, w.z, 5, dt);
    float hs = length(vec2(c.vel.x, c.vel.z));
    if (hs > 0.2f) c.yaw = dampAngle(c.yaw, std::atan2(c.vel.x, c.vel.z), 7, dt);
    an.speed = hs;
    an.aux[0] = hs < 0.2f ? -1.2f : 0.f;   // grazing: head lowered
}

}  // namespace lk
