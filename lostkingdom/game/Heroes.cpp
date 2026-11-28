// LOST KINGDOM — the hero wheel (1.8.0): switching heroes, their signature powers, flight and double jump.
//   The Wanderer (Aldric's soul): equipped weapon + learnt spells.
//   Kaede, Lame des Brumes (ninja): two daggers, double jump, Shuriken / Pas de l'ombre / Bombe fumigène.
//   Astra, l'Étoile tombée: fists, flight, Rayon stellaire / Frappe météore / Onde solaire.
#include "Game.h"
#include "../engine/core/Platform.h"

namespace lk {

const HeroDef& Game::hero() const {
    const auto& H = heroes();
    return H[(activeHero >= 0 && activeHero < (int)H.size()) ? activeHero : 0];
}

bool Game::heroUnlocked(int i) const {
    const auto& H = heroes();
    if (i < 0 || i >= (int)H.size()) return false;
    return H[i].unlockQuest.empty() || unlockedHeroes.count(H[i].id) > 0 || gameTuning().allHeroes;   // 3.9.6 mod "Tous les héros"
}

void Game::unlockHero(const std::string& id, bool announce) {
    int i = heroIndex(id);
    if (i < 0 || unlockedHeroes.count(id)) return;
    unlockedHeroes.insert(id);
    unlockLore(heroes()[i].lore, false);
    if (announce) {
        notify("Nouveau héros : " + heroes()[i].name + " — touchez votre portrait pour ouvrir la roue des héros", heroes()[i].color, "i_character");
        play("levelup", 0.8f);
        haptic(60, 200);
    }
}

bool Game::switchHero(int i) {
    if (i == activeHero || !heroUnlocked(i) || player == NullEntity) return false;
    Character& c = pc();
    if (c.dead || riding) return false;
    activeHero = i;
    flying = false; meteorPending = false; stealthT = 0; usedDoubleJump = false;
    for (float& cd : heroCD) cd = 0;
    rebuildPlayerVisual();
    recomputeStats();
    const HeroDef& h = hero();
    particles.magicBurst(c.pos + vec3(0, 1.f, 0), h.color, 1.6f, 40);
    particles.shockwave(c.pos, h.color, 2.5f);
    play("magic_burst", 0.8f);
    floatText(c.pos + vec3(0, 2.4f, 0), h.name, h.color, 26);
    unlockLore(h.lore, false);
    return true;
}

// ------------------------------------------------------------------------------------------ powers
bool Game::heroHasPowers() const { return hero().powers[0].kind != HeroPower::None; }

void Game::castHeroPower(int slot) {
    if (slot < 0 || slot > 2 || player == NullEntity) return;
    const HeroPowerDef& pw = hero().powers[slot];
    if (pw.kind == HeroPower::None || heroCD[slot] > 0) return;
    Character& c = pc();
    if (c.dead || riding || c.swimming) return;
    if (stamina < pw.stamina) { floatText(c.pos + vec3(0, 2.2f, 0), "Épuisé", vec3(0.9f, 0.8f, 0.3f), 18); return; }
    if (mp < pw.mana) { floatText(c.pos + vec3(0, 2.2f, 0), "Mana insuffisant", vec3(0.5f, 0.6f, 1.f), 18); return; }
    stamina -= pw.stamina; mp -= pw.mana; staminaRegenDelay = 0.6f;
    heroCD[slot] = pw.cooldown;
    float power = c.atk * pw.power * (1.f + level * 0.02f);
    Entity t = lockTarget != NullEntity ? lockTarget : findTarget(c.pos, c.yaw, 20.f, Team::Enemy, 0.0f);
    Character* tc = t != NullEntity ? reg.get<Character>(t) : nullptr;
    if (tc && pw.kind != HeroPower::SmokeBomb && pw.kind != HeroPower::SolarWave) c.yaw = std::atan2(tc->pos.x - c.pos.x, tc->pos.z - c.pos.z);
    vec3 fwd{std::sin(c.yaw), 0, std::cos(c.yaw)};
    vec3 chest = c.pos + vec3(0, 1.3f, 0);
    const vec3 col = hero().color;
    switch (pw.kind) {
    case HeroPower::Shuriken: {
        c.vis.anim.play(&clip(C_DG_A1), 1.6f, true);
        for (int i = -1; i <= 1; i++) {
            vec3 dir = normalize(fwd + perpVec(fwd) * (i * 0.14f) + vec3(0, tc ? 0.02f : 0.04f, 0));
            if (tc && i == 0) dir = normalize(tc->pos + vec3(0, tc->height * 0.55f, 0) - chest);
            fireProjectile(player, ProjKind::Arrow, chest + fwd * 0.4f, dir, 32.f, power, Element::Physical, Team::Player, i == 0 ? t : NullEntity);
        }
        // 3.0.1 a rune flares at her hand and a violet wind slash leaves with the blades
        vfxCircle(chest + fwd * 0.7f, vec3(0.62f, 0.32f, 1.f), 0.75f, 0.45f, NullEntity, fwd);
        vfxSlash(chest + fwd * 0.6f - vec3(0, 0.2f, 0), c.yaw, 0.35f, vec3(0.62f, 0.32f, 1.f), 2.4f, 0.3f);
        vfxSlash(chest + fwd * 0.6f - vec3(0, 0.2f, 0), c.yaw, -0.25f, vec3(0.4f, 0.8f, 1.f), 2.0f, 0.34f);
        play("bow", 0.6f, 1.6f);
        break;
    }
    case HeroPower::ShadowStep: {
        vec3 from = c.pos, to;
        if (tc && distXZ(tc->pos, c.pos) < 16.f) {
            vec3 tf{std::sin(tc->yaw), 0, std::cos(tc->yaw)};
            to = tc->pos - tf * (tc->radius + 0.9f);
        } else to = c.pos + fwd * 8.f;
        vec3 tmp = to; collideStatic(tmp, c.radius, c.pos.y); to = tmp; to.y = groundAt(to.x, to.z);
        for (int i = 0; i < 12; i++) particles.magicTrail(lerp(from, to, i / 11.f) + vec3(0, 1.f, 0), vec3(0.35f, 0.25f, 0.6f), 1.2f);
        particles.smoke(from + vec3(0, 0.8f, 0), 1.2f, 0.2f, vec3(0.15f, 0.12f, 0.2f));
        // 3.0.1 a shadow vortex swallows her, a violet streak, and she bursts out of a dark star behind the target
        vfxVortex(from, vec3(0.62f, 0.32f, 1.f), 1.1f, 2.8f, 0.7f);
        vfxBeam(from + vec3(0, 1.f, 0), to + vec3(0, 1.f, 0), vec3(0.62f, 0.32f, 1.f), 0.22f, 0.32f);
        vfxSphere(to + vec3(0, 1.f, 0), vec3(0.8f, 0.5f, 1.f), vec3(0.25f, 0.05f, 0.45f), 0.8f, 0.45f, 1, 1.6f);
        vfxCircle(to, vec3(0.62f, 0.32f, 1.f), 1.6f, 0.7f);
        c.pos = to; c.vel = vec3(0); c.invuln = 0.5f;
        if (tc) {
            c.yaw = std::atan2(tc->pos.x - c.pos.x, tc->pos.z - c.pos.z);
            c.vis.anim.play(&clip(C_DG_HEAVY), 1.4f, false, 0.04f);
            dealDamage(player, t, power * (stealthT > 0 ? 1.5f : 1.f), Element::Shadow, normalize(tc->pos - c.pos), 45, true, true);
            vec3 hp = tc->pos + vec3(0, tc->height * 0.55f, 0);   // a crossed slash on the target
            vfxSlash(hp - fwd * 0.4f, c.yaw, 0.8f, vec3(0.62f, 0.32f, 1.f), 2.2f, 0.3f);
            vfxSlash(hp - fwd * 0.4f, c.yaw + 0.2f, -0.8f, vec3(0.9f, 0.6f, 1.f), 2.2f, 0.34f);
            hitStop = std::max(hitStop, 0.08f);
        }
        play("shadow", 0.8f);
        stealthT = 0;
        break;
    }
    case HeroPower::SmokeBomb: {
        for (int i = 0; i < 4; i++) particles.smoke(c.pos + vec3(particles.rng.range(-1.5f, 1.5f), 0.5f, particles.rng.range(-1.5f, 1.5f)), 2.2f, 0.25f, vec3(0.42f, 0.42f, 0.46f));
        particles.shockwave(c.pos, vec3(0.6f, 0.6f, 0.65f), 5.5f);
        // 3.0.1 a dome of smoke swells around her, a violet circle seals it
        vfxSphere(c.pos + vec3(0, 0.4f, 0), vec3(0.55f, 0.5f, 0.7f), vec3(0.35f, 0.2f, 0.6f), 1.8f, 1.6f, 0, 1.6f);
        vfxShockwave(c.pos, vec3(0.7f, 0.6f, 0.9f), 5.5f, 0.75f);
        vfxCircle(c.pos, vec3(0.62f, 0.32f, 1.f), 3.2f, 1.5f);
        reg.each<Character>([&](Entity e, Character& o) {
            if (o.team != Team::Enemy || o.dead || distXZ(o.pos, c.pos) > 5.5f) return;
            o.status.stun = std::max(o.status.stun, 2.f);
            if (AIBrain* ai = reg.get<AIBrain>(e)) ai->aggro = 0;
        });
        if (lockTarget != NullEntity) lockTarget = NullEntity;
        stealthT = 3.f;
        play("explosion", 0.5f, 1.4f);
        break;
    }
    case HeroPower::StarBeam: {
        c.vis.anim.play(&clip(C_CAST_FORWARD), 1.5f, true);
        vec3 dir = tc ? normalize(tc->pos + vec3(0, tc->height * 0.5f, 0) - chest) : fwd;
        vec3 end = chest + dir * 22.f;
        particles.lightningArc(chest + dir * 0.5f, end, vec3(1.f, 0.9f, 0.5f));
        // 3.0.1 a golden rune circle opens before her hands and a beam of starlight tears through it
        vfxCircle(chest + dir * 1.0f, vec3(1.f, 0.78f, 0.32f), 1.4f, 0.8f, NullEntity, dir);
        vfxBeam(chest + dir * 1.0f, end, vec3(1.f, 0.78f, 0.32f), 0.6f, 0.7f);
        vfxSphere(end, vec3(1.f, 0.95f, 0.7f), vec3(1.f, 0.55f, 0.15f), 1.2f, 0.55f, 1, 1.2f);
        vfxSphere(chest + dir * 1.0f, vec3(1.f, 1.f, 0.9f), vec3(1.f, 0.78f, 0.32f), 0.5f, 0.5f, 2);
        for (int i = 0; i < 14; i++) particles.magicTrail(lerp(chest, end, i / 13.f), vec3(1.f, 0.85f, 0.4f), 1.2f);
        reg.each<Character>([&](Entity e, Character& o) {
            if (o.team != Team::Enemy || o.dead) return;
            float tt; float d = distToSegment(o.pos.xz(), chest.xz(), end.xz(), &tt);
            if (d < 1.4f + o.radius) dealDamage(player, e, power, Element::Light, dir, 30, true, true);
        });
        play("holy", 0.9f, 1.2f);
        addShake(0.15f);
        break;
    }
    case HeroPower::MeteorStrike: {
        // airborne: dive at once; on the ground: leap first. The impact happens on landing (updateHeroAbilities).
        meteorPending = true; meteorPower = power;
        if (flying || !c.grounded) { flying = false; c.vel.y = -28.f; if (tc) { vec3 d = tc->pos - c.pos; d.y = 0; c.vel.x = d.x * 1.5f; c.vel.z = d.z * 1.5f; } }
        else { c.vel.y = 10.f; c.grounded = false; c.vel.x = fwd.x * 5.f; c.vel.z = fwd.z * 5.f; }
        c.invuln = 1.f;
        particles.magicBurst(chest, vec3(1.f, 0.6f, 0.25f), 1.f, 24);
        vfxCircle(c.pos, vec3(1.f, 0.55f, 0.2f), 2.2f, 0.9f);                                      // 3.0.1 take-off rune
        vfxSphere(chest, vec3(1.f, 0.8f, 0.4f), vec3(1.f, 0.4f, 0.1f), 1.2f, 1.2f, 0, 0.f, player);   // burning aura while she falls
        play("fireball", 0.8f, 0.8f);
        break;
    }
    case HeroPower::SolarWave: {
        c.vis.anim.play(&clip(C_CAST_AREA), 1.3f, true);
        areaDamage(player, c.pos, 8.f, power, Element::Light, Team::Player, 35);
        reg.each<Character>([&](Entity, Character& o) { if (o.team == Team::Enemy && !o.dead && distXZ(o.pos, c.pos) < 8.f) o.status.stun = std::max(o.status.stun, 1.5f); });
        float heal = c.maxHp * 0.15f;
        c.hp = std::min(c.maxHp, c.hp + heal);
        floatText(c.pos + vec3(0, 2.4f, 0), "+" + std::to_string((int)heal), vec3(0.5f, 1.f, 0.6f));
        particles.shockwave(c.pos, vec3(1.f, 0.85f, 0.4f), 8.f);
        particles.magicBurst(chest, vec3(1.f, 0.9f, 0.5f), 2.4f, 60);
        // 3.0.1 a sun circle on the ground, a column of light, rays and two rings of fire rolling outwards
        vfxCircle(c.pos, vec3(1.f, 0.78f, 0.32f), 8.f, 1.6f);
        vfxPillar(c.pos, vec3(1.f, 0.78f, 0.32f), 1.8f, 14.f, 1.1f);
        vfxRays(c.pos, vec3(1.f, 0.78f, 0.32f), 9.f, 0.9f);
        vfxShockwave(c.pos, vec3(1.f, 0.7f, 0.25f), 8.5f, 0.8f);
        vfxShockwave(c.pos, vec3(1.f, 0.9f, 0.5f), 6.f, 1.1f);
        vfxSphere(chest, vec3(1.f, 1.f, 0.85f), vec3(1.f, 0.6f, 0.2f), 1.f, 0.55f, 1, 2.2f);
        play("holy", 1.f, 0.8f);
        addShake(0.35f);
        break;
    }
    default: break;
    }
}

// Called every frame from updatePlayer: cooldowns, stealth, flight control, meteor landing.
void Game::updateHeroAbilities(float dt) {
    if (player == NullEntity) return;
    Character& c = pc();
    for (float& cd : heroCD) cd = std::max(0.f, cd - dt);
    if (stealthT > 0) {
        stealthT -= dt;
        c.vis.dissolve = stealthT > 0 ? 0.32f + 0.05f * std::sin(realTime * 9.f) : 0.f;
        c.vis.dissolveColor = vec3(0.3f, 0.25f, 0.45f);
    }
    if (c.grounded) usedDoubleJump = false;
    // flight: holds an altitude above the ground, drains stamina, lands when it runs out
    if (flying) {
        if (c.dead || riding || c.swimming || !hero().canFly) flying = false;
        stamina -= dt * 3.5f; staminaRegenDelay = 0.4f;
        if (stamina <= 0) { flying = false; floatText(c.pos + vec3(0, 2.2f, 0), "Épuisée", vec3(0.9f, 0.8f, 0.3f), 18); }
        c.airTime = 0.5f;          // "in the air" for the animation (the flight pose), below the fall-damage threshold
        fallSafeT = 3.f;           // and no fall damage for a moment after the flight ends
        if (particles.rng.f() < dt * 12) particles.magicTrail(c.pos + vec3(0, 0.6f, 0), vec3(1.f, 0.85f, 0.45f), 0.5f);
    }
    fallSafeT = std::max(0.f, fallSafeT - dt);
    if (meteorPending) fallSafeT = 3.f;
    // meteor landing
    if (meteorPending && c.grounded) {
        meteorPending = false;
        c.airTime = 0;
        areaDamage(player, c.pos, 6.f, meteorPower, Element::Fire, Team::Player, 60);
        reg.each<Character>([&](Entity, Character& o) {
            if (o.team != Team::Enemy || o.dead) return;
            float d = distXZ(o.pos, c.pos);
            if (d < 6.f) o.knock += normalize(o.pos - c.pos + vec3(0.01f, 0, 0)) * (16.f / std::max(1.f, o.mass)) * (1.f - d / 6.f);
        });
        particles.explosion(c.pos, 1.2f, vec3(1.f, 0.6f, 0.25f));
        particles.shockwave(c.pos, vec3(1.f, 0.7f, 0.3f), 6.f);
        // 3.0.1 the impact: a fireball, a ring of flame, the ground cracked and glowing, a pillar of fire
        vfxSphere(c.pos + vec3(0, 0.8f, 0), vec3(1.f, 0.85f, 0.5f), vec3(1.f, 0.35f, 0.05f), 1.5f, 0.75f, 1, 1.8f);
        vfxShockwave(c.pos, vec3(1.f, 0.55f, 0.15f), 9.f, 0.8f);
        vfxGround(c.pos, vec3(1.f, 0.6f, 0.15f), vec3(0.8f, 0.15f, 0.02f), 6.5f, 3.f);
        vfxPillar(c.pos, vec3(1.f, 0.5f, 0.12f), 2.2f, 12.f, 0.9f);
        vfxRays(c.pos, vec3(1.f, 0.6f, 0.2f), 7.f, 0.7f);
        particles.dust(c.pos, 2.f);
        play("explosion", 1.f);
        addShake(0.6f); hitStop = std::max(hitStop, 0.1f);
        haptic(90, 255);
    }
    c.vis.anim.aux[2] = approach(c.vis.anim.aux[2], flying ? 1.f : 0.f, dt * 4.f);   // flight pose weight
    // Astra's energy aura: glows in flight, flares for a moment when a power fires
    const HeroDef& h = hero();
    float want = 0;
    if (h.canFly && !c.dead) {
        want = 0.55f * c.vis.anim.aux[2];
        for (int i = 0; i < 3; i++) if (h.powers[i].cooldown > 0 && heroCD[i] > h.powers[i].cooldown - 0.7f) want = 1.f;
        if (meteorPending) want = 1.f;
    }
    heroAura = approach(heroAura, want, dt * (want > heroAura ? 6.f : 1.5f));
    if (heroAura > 0.01f) {
        mat4 model = mat4::trs(c.pos, quat::axisAngle({0, 1, 0}, c.yaw), vec3(c.vis.scale));
        vec3 chest = model.transformPoint(c.vis.pw.wpos[H_CHEST]);
        vec3 gold(1.f, 0.82f, 0.38f), white(1.f, 0.97f, 0.85f);
        // rising motes around the body, flickering flame tongues at full power
        float rate = 26.f * heroAura;
        for (float k = particles.rng.f(); k < rate * dt; k += 1.f) {
            vec3 o(particles.rng.range(-0.45f, 0.45f), particles.rng.range(-0.8f, 0.6f), particles.rng.range(-0.45f, 0.45f));
            particles.magicTrail(chest + o, particles.rng.f() < 0.3f ? white : gold, 0.35f + 0.35f * heroAura);
        }
        if (heroAura > 0.8f && particles.rng.f() < dt * 6.f) particles.sparks(chest, vec3(0, 1, 0), 3, gold);
        float pulse = 0.75f + 0.25f * std::sin(realTime * 11.f);
        c.vis.outlineColor = vec4(lerp(vec3(0.05f, 0.04f, 0.03f), gold * 1.4f, heroAura * pulse), 1);
        c.vis.rim = 0.65f + 0.8f * heroAura;
    } else if (h.canFly) {
        c.vis.outlineColor = vec4(0.05f, 0.04f, 0.03f, 1); c.vis.rim = 0.65f;
    }
}

// Jump button: double jump (Kaede), take off / land (Astra). Returns true when the hero handled it.
bool Game::heroJump() {
    Character& c = pc();
    const HeroDef& h = hero();
    if (h.canFly) {
        if (flying) { flying = false; play("jump", 0.5f, 0.8f); return true; }                     // land
        if (!c.grounded && !c.swimming && stamina > 10) {                                         // take off
            flying = true; flightAlt = clampf(c.pos.y - groundAt(c.pos.x, c.pos.z) + 4.f, 5.f, 9.f);
            c.vel.y = 2.f; play("magic_burst", 0.5f, 1.4f);
            particles.magicBurst(c.pos + vec3(0, 0.8f, 0), h.color, 1.f, 24);
            return true;
        }
    }
    if ((h.doubleJump || hasPower(KP_DOUBLEJUMP)) && !c.grounded && !c.swimming && !usedDoubleJump && c.airTime > 0.08f && stamina > 6) {   // 3.2.0 zephyr boots
        usedDoubleJump = true; stamina -= 6;
        c.vel.y = 7.2f;
        c.vis.anim.play(&clip(C_ROLL), 1.6f, false, 0.04f);
        particles.smoke(c.pos + vec3(0, 0.3f, 0), 0.6f, 0.2f, vec3(0.5f, 0.5f, 0.55f));
        play("jump", 0.6f, 1.3f);
        return true;
    }
    return false;
}

// Physics hook (physicsStep, before integration): flight replaces gravity with an altitude hold.
void Game::heroFlightPhysics(Character& c, float dt) {
    if (!flying) return;
    float gy = groundAt(c.pos.x, c.pos.z);
    float wl = waterAt(c.pos.x, c.pos.z);
    float base = std::max(gy, wl > -1e8f ? wl : gy);
    float targetY = base + flightAlt;
    c.vel.y = clampf((targetY - c.pos.y) * 3.f, -8.f, 8.f) + 20.f * dt;   // + gravity compensation
    c.grounded = false;
}

// ------------------------------------------------------------------------------------------ wheel UI
void Game::drawHeroWheel() {
    UI& u = ui;
    float W_ = u.W, H_ = u.H;
    u.rect({0, 0, W_, H_}, vec4(0.02f, 0.015f, 0.03f, 0.72f));
    vec2 c{W_ / 2, H_ / 2 + 10};
    const auto& Hs = heroes();
    int slots = (int)Hs.size();   // exactly the three heroes of the wheel
    float R = 225.f;
    u.ring(c, R - 4, R + 4, 0, TAU, rgba(0xc9a45c, 0.35f), 64);
    // centre: active hero
    const HeroDef& ah = hero();
    u.circle(c, 118, vec4(0.06f, 0.05f, 0.07f, 0.95f), 48);
    u.ring(c, 114, 120, 0, TAU, vec4(ah.color, 0.9f), 48);
    float ns = std::min(32.f, 32.f * 200.f / std::max(1.f, u.textWidth(ah.name, 32, F_TITLE)));   // one line, fits the circle
    u.text(ah.name, c.x, c.y - 40, ns, vec4(ah.color, 1), F_TITLE, A_CENTER, 0, true);
    u.text(ah.title, c.x, c.y + 4, 17, rgba(0xe8dcc0), F_SERIF, A_CENTER, 200, true);
    u.text("Roue des héros", c.x, c.y + 50, 15, rgba(0xb0a48c), F_BODY, A_CENTER);
    for (int i = 0; i < slots; i++) {
        float a = -PI / 2 + i * TAU / slots;
        vec2 p = c + vec2(std::cos(a), std::sin(a)) * R;
        Rect r{p.x - 72, p.y - 72, 144, 144};
        bool open = heroUnlocked(i);
        vec3 col = Hs[i].color;
        bool sel = i == activeHero;
        u.circle(p, 66, vec4(0.08f, 0.07f, 0.09f, 0.95f), 40);
        u.ring(p, sel ? 60 : 62, 68, 0, TAU, vec4(col, open ? (sel ? 1.f : 0.75f) : 0.3f), 40);
        if (open) {
            u.text(Hs[i].name.substr(0, 1), p.x, p.y - 40, 54, vec4(col, 1), F_TITLE, A_CENTER, 0, true);
            u.text(Hs[i].name, p.x, p.y + 18, 18, rgba(0xf4e7c4), F_BOLD, A_CENTER, 130, true);
        } else {
            u.sprite("i_lock", {p.x - 22, p.y - 40, 44, 44}, vec4(1, 1, 1, 0.6f));
            u.text(Hs[i].name, p.x, p.y + 14, 17, rgba(0x8a8070), F_BOLD, A_CENTER, 130, true);
        }
        u.tag(r, "hero:" + Hs[i].id);
        if (u.button(r)) {
            if (!open) notify("Verrouillé — " + Hs[i].unlockHint, vec3(1.f, 0.85f, 0.6f), "i_lock");
            else { if (switchHero(i)) menu = Menu::None; else if (i == activeHero) menu = Menu::None; }
        }
    }
    if (u.iconButton({W_ - 90, 24, 64, 64}, "close")) menu = Menu::None;
    u.consumeAll();
}

// ------------------------------------------------------------------------------------------ 3.10.6 ultimates
// One per hero, unlocked by the gauge (the hero's blows fill it). The world slows to a quarter around the hero
// while it plays; the hero cannot be hurt.
//   0 the Wanderer, "Couronne brisée": six golden shards of the crown circle up above him, then crash down together
//   1 Kaede, "Mille lames": she blinks from foe to foe (up to eight) cutting each, then the smoke bursts
//   2 Astra, "Jugement céleste": she rises, stars rain on every foe around, a pillar of light ends it
bool Game::castUltimate() {
    if (player == NullEntity || ultT >= 0 || ultCharge < 100.f) return false;
    Character& c = pc();
    if (c.dead || riding || c.swimming || driving >= 0) return false;
    const std::string& id = hero().id;
    ultKind = id == "kaede" ? 1 : (id == "astra" ? 2 : 0);
    static const char* names[] = {"Couronne brisée", "Mille lames", "Jugement céleste"};
    ultName = names[ultKind];
    ultT = 0; ultStep = 0; ultStepT = 0; ultCharge = 0; ultCenter = c.pos; ultBannerT = 1.6f;
    ultTargets.clear();
    reg.each<Character>([&](Entity e, Character& o) {
        if (o.dead || (o.team != Team::Enemy && o.team != Team::Animal) || e == player) return;
        if (distXZ(o.pos, c.pos) < (ultKind == 2 ? 18.f : 14.f)) ultTargets.push_back(e);
    });
    std::sort(ultTargets.begin(), ultTargets.end(), [&](Entity a, Entity b) { return distXZ(reg.get<Character>(a)->pos, c.pos) < distXZ(reg.get<Character>(b)->pos, c.pos); });
    if (ultTargets.size() > 8) ultTargets.resize(8);
    ultSlowT = ultKind == 2 ? 3.6f : 2.2f;
    c.invuln = std::max(c.invuln, ultSlowT + 0.4f);
    c.vel = vec3(0, c.vel.y, 0);
    const vec3 col = hero().color;
    particles.shockwave(c.pos, col, 4.f);
    particles.magicBurst(c.pos + vec3(0, 1.2f, 0), col, 2.f, 60);
    play("boss_intro", 0.8f, 1.2f); play("magic_burst", 1.f, 0.7f);
    addShake(0.35f); haptic(120, 255); hitStop = std::max(hitStop, 0.08f);
    c.vis.anim.play(&clip(C_ROAR_H), 1.2f, false, 0.06f);
    AGR_LOG("ultimate %s: %zu targets", ultName.c_str(), ultTargets.size());
    return true;
}

void Game::updateUltimate(float dt) {
    ultSlowT = std::max(0.f, ultSlowT - dt);
    ultBannerT = std::max(0.f, ultBannerT - dt);
    if (ultT < 0 || player == NullEntity) return;
    Character& c = pc();
    ultT += dt; ultStepT += dt;
    const vec3 col = hero().color;
    float power = c.atk * (1.f + level * 0.03f);
    auto alive = [&](Entity e) { Character* o = reg.valid(e) ? reg.get<Character>(e) : nullptr; return o && !o->dead ? o : nullptr; };
    if (ultKind == 0) {
        // ---- Couronne brisée: 1.0 s rising crown, then the crash
        vec3 top = ultCenter + vec3(0, 4.5f, 0);
        if (ultT < 1.0f) {
            float k = ultT / 1.0f;
            for (int i = 0; i < 6; i++) {
                float a = i * TAU / 6.f + ultT * 5.f;
                vec3 p = ultCenter + vec3(std::cos(a) * (3.2f - k * 1.2f), 1.f + k * 3.5f, std::sin(a) * (3.2f - k * 1.2f));
                if (particles.rng.f() < dt * 50.f) particles.magicTrail(p, vec3(1.f, 0.82f, 0.35f), 0.9f);
                if (ultStepT > 0.08f) vfxSphere(p, vec3(1.f, 0.85f, 0.45f), vec3(1.f, 0.6f, 0.15f), 0.22f, 0.12f, 1);
            }
            if (ultStepT > 0.08f) ultStepT = 0;
            c.vel = vec3(0, c.vel.y, 0);
        } else if (ultStep == 0) {
            ultStep = 1;
            vfxPillar(ultCenter, vec3(1.f, 0.8f, 0.35f), 2.2f, 14.f, 0.9f);
            for (int i = 0; i < 6; i++) { float a = i * TAU / 6.f; vec3 g = ultCenter + vec3(std::cos(a) * 5.f, 0, std::sin(a) * 5.f); g.y = groundAt(g.x, g.z);
                vfxBeam(top, g, vec3(1.f, 0.78f, 0.3f), 0.7f, 0.5f, i == 0 ? 1.5f : 0.f); particles.explosion(g, 2.5f, vec3(1.f, 0.7f, 0.3f)); }
            particles.shockwave(ultCenter, vec3(1.f, 0.8f, 0.4f), 11.f);
            particles.shockwave(ultCenter, vec3(1.f, 0.95f, 0.8f), 6.f);
            areaDamage(player, ultCenter, 10.f, power * 10.f, Element::Light, Team::Player, 120);
            play("explosion", 1.f, 0.8f); play("thunder", 0.8f, 1.3f);
            addShake(0.9f); hitStop = std::max(hitStop, 0.14f); haptic(200, 255);
            lightningFlash = std::max(lightningFlash, 0.7f);
        } else if (ultT > 1.6f) ultT = -1;
    } else if (ultKind == 1) {
        // ---- Mille lames: one blink every 0.14 s
        if (ultStepT >= 0.14f) {
            ultStepT = 0;
            Character* o = nullptr;
            while (ultStep < (int)ultTargets.size() && !(o = alive(ultTargets[ultStep]))) ultStep++;
            if (o && ultStep < (int)ultTargets.size()) {
                vec3 from = c.pos;
                vec3 side = normalize(vec3(o->pos.x - c.pos.x + 0.01f, 0, o->pos.z - c.pos.z));
                c.pos = o->pos - side * (o->radius + 0.9f); c.pos.y = groundAt(c.pos.x, c.pos.z);
                c.yaw = std::atan2(side.x, side.z);
                vfxBeam(from + vec3(0, 1.f, 0), c.pos + vec3(0, 1.f, 0), vec3(0.65f, 0.5f, 1.f), 0.35f, 0.35f, ultStep == 0 ? 1.f : 0.f);
                particles.magicBurst(c.pos + vec3(0, 1.f, 0), vec3(0.45f, 0.35f, 0.9f), 1.2f, 20);
                c.vis.anim.play(&clip(ultStep % 2 ? C_DG_A2 : C_DG_A1), 2.2f, false, 0.02f);
                dealDamage(player, ultTargets[ultStep], power * 6.f, Element::Shadow, side, 60, true, false);
                play("swing", 0.9f, 1.2f + 0.05f * ultStep); hitStop = std::max(hitStop, 0.05f); addShake(0.2f);
                ultStep++;
            } else if (ultStep >= (int)ultTargets.size()) {
                // the end: the smoke bursts around her, everything near is cut once more
                particles.explosion(c.pos, 3.f, vec3(0.35f, 0.3f, 0.5f)); particles.dust(c.pos, 5.f, vec3(0.3f, 0.28f, 0.35f));
                particles.shockwave(c.pos, vec3(0.6f, 0.45f, 1.f), 7.f);
                areaDamage(player, c.pos, 5.f, power * 5.f, Element::Shadow, Team::Player, 80);
                play("explosion", 0.8f, 1.2f); addShake(0.5f);
                ultT = -1;
            }
        }
    } else {
        // ---- Jugement céleste: rise, 2.8 s of falling stars, then the pillar
        if (ultT < 0.5f) { c.vel.y = std::max(c.vel.y, 4.5f); c.grounded = false; }
        else if (ultT < 3.3f) {
            c.vel.y = std::max(c.vel.y, 0.f);   // hovers
            if (ultStepT >= 0.13f) {
                ultStepT = 0;
                vec3 g;
                std::vector<Entity> live;
                for (Entity e : ultTargets) if (alive(e)) live.push_back(e);
                bool onFoe = !live.empty() && particles.rng.f() < 0.75f;
                Entity hit = onFoe ? live[particles.rng.irange(0, (int)live.size() - 1)] : NullEntity;
                if (hit != NullEntity) g = reg.get<Character>(hit)->pos;
                else { float a = particles.rng.range(0.f, TAU), r = particles.rng.range(3.f, 14.f); g = ultCenter + vec3(std::cos(a) * r, 0, std::sin(a) * r); g.y = groundAt(g.x, g.z); }
                vec3 sky = g + vec3(particles.rng.range(-4.f, 4.f), 30.f, particles.rng.range(-4.f, 4.f));
                vfxBeam(sky, g, vec3(1.f, 0.9f, 0.55f), 0.5f, 0.25f, 0.8f);
                particles.explosion(g, 1.8f, vec3(1.f, 0.85f, 0.45f));
                if (hit != NullEntity) dealDamage(player, hit, power * 3.f, Element::Light, normalize(g - c.pos + vec3(0.01f, 0, 0)), 40, true, true);
                else areaDamage(player, g, 2.5f, power * 1.2f, Element::Light, Team::Player, 20);
                play("lightning", 0.5f, 1.3f + particles.rng.f() * 0.3f); addShake(0.15f);
            }
        } else if (ultStep == 0) {
            ultStep = 1;
            vec3 p = c.pos; p.y = groundAt(p.x, p.z);
            vfxPillar(p, vec3(1.f, 0.9f, 0.6f), 3.f, 25.f, 1.f);
            particles.shockwave(p, vec3(1.f, 0.9f, 0.6f), 10.f);
            areaDamage(player, p, 8.f, power * 8.f, Element::Light, Team::Player, 100);
            play("holy", 1.f, 0.8f); play("explosion", 0.8f, 0.9f); addShake(0.8f); hitStop = std::max(hitStop, 0.12f);
            lightningFlash = std::max(lightningFlash, 0.6f);
        } else if (ultT > 3.8f) ultT = -1;
    }
}

}  // namespace lk
