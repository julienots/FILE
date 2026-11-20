// LOST KINGDOM 2.4.0 — diggable ground: the shovel (dig mode) and bombs. The terrain offset layer lives in
// World (digOffset / dig); here are the player actions, the thrown bombs and the explosions.
#include "Game.h"
#include <algorithm>

namespace lk {

void Game::playerDig() {
    Character& c = pc();
    if (c.dead || riding || c.swimming || driving >= 0 || dungeon.active || c.attackClip) return;
    if (stamina < 6) { floatText(c.pos + vec3(0, 2.2f, 0), "Épuisé", vec3(0.9f, 0.8f, 0.3f), 18); return; }
    vec3 fwd{std::sin(c.yaw), 0, std::cos(c.yaw)};
    vec2 at = (c.pos + fwd * 1.7f).xz();
    if (!world.dig(at, 1.6f, 0.55f, false)) { notify("Impossible de creuser ici (trop près des bâtiments).", vec3(1.f, 0.75f, 0.5f), "i_lock"); return; }
    stamina -= 6; staminaRegenDelay = 0.4f;
    c.vis.anim.play(&clip(C_GATHER_SWING), 1.35f);
    c.attackClip = &clip(C_GATHER_SWING); c.attackIdx = -2;
    vec3 p(at.x, groundAt(at.x, at.y), at.y);
    int r = world.regionId(at.x, at.y);
    vec3 soil = r == R_AMBER ? vec3(0.85f, 0.7f, 0.45f) : (r == R_FROSTHEIM ? vec3(0.9f, 0.92f, 0.95f) : (r == R_ASHEN ? vec3(0.3f, 0.27f, 0.25f) : vec3(0.45f, 0.34f, 0.24f)));
    for (int k = 0; k < 3; k++) particles.dust(p + vec3(particles.rng.range(-0.6f, 0.6f), 0.2f, particles.rng.range(-0.6f, 0.6f)), 1.1f, soil);
    play3D(variant(r == R_AMBER || r == R_CORAL ? "step_sand" : "step_stone"), p, 0.9f, 0.7f);
    play3D("harvest_stone", p, 0.35f, 0.75f);
    haptic(18, 90);
    digsDone++;
    // what the earth gives back
    Rng g((uint32_t)(gameTime * 1000.0) ^ (uint32_t)(at.x * 31 + at.y * 17));
    float roll = g.f();
    if (roll < 0.015f) {
        int gg = 30 + level * 8 + g.irange(0, 40); gold += gg;
        static const char* gems[] = {"crystal", "amber", "pearl", "rare_ore"};
        addItem(gems[g.irange(0, 3)], 1);
        notify("Un trésor enfoui ! (+" + std::to_string(gg) + " or)", vec3(1.f, 0.85f, 0.4f), "i_chest");
        play("coin", 0.8f); particles.magicTrail(p + vec3(0, 0.5f, 0), vec3(1.f, 0.85f, 0.4f), 1.f);
    } else if (roll < 0.06f) {
        const char* ore = r == R_FROSTHEIM ? "silver_ore" : r == R_AMBER || r == R_JUNGLE ? "gold_ore" : r == R_ASHEN || r == R_SHADOWS ? "coal" : "copper_ore";
        addItem(ore, 1);
    } else if (roll < 0.26f) {
        addItem(r == R_AMBER && g.chance(0.3f) ? "sand_glass" : (r == R_CORAL && g.chance(0.5f) ? "seashell" : "stone"), 1);
    }
}

void Game::throwBomb() {
    if (player == NullEntity) return;
    Character& c = pc();
    vec3 fwd{std::sin(c.yaw), 0, std::cos(c.yaw)};
    Bomb b; b.pos = c.pos + vec3(0, 1.6f, 0) + fwd * 0.6f; b.vel = fwd * 9.5f + vec3(0, 6.f, 0) + vec3(c.vel.x, 0, c.vel.z) * 0.5f;
    bombs.push_back(b);
    c.vis.anim.play(&clip(C_INTERACT), 1.4f);
    play("swing", 0.6f, 0.8f);
    notify("Bombe lancée !", vec3(1.f, 0.6f, 0.3f), "i_crystal");
}

void Game::explodeAt(vec3 p, float radius, float depth, float dmg, Entity src) {
    world.dig(p.xz(), radius, depth, true);   // refused near buildings: the blast still hurts
    particles.explosion(p + vec3(0, 0.4f, 0), radius * 0.55f);
    particles.shockwave(p + vec3(0, 0.15f, 0), vec3(1.f, 0.7f, 0.4f), radius * 2.2f);
    for (int k = 0; k < 8; k++) particles.dust(p + vec3(particles.rng.range(-radius, radius), 0.3f, particles.rng.range(-radius, radius)), 2.2f, vec3(0.42f, 0.34f, 0.26f));
    play3D("explosion", p, 1.f, 0.9f, 140.f);
    float dPlayer = player != NullEntity ? distXZ(pc().pos, p) : 1e9f;
    addShake(dPlayer < 25.f ? 0.55f : 0.2f);
    if (dPlayer < 30.f) haptic(90, 255);
    float reach = radius * 1.7f;
    std::vector<Entity> hit;
    reg.each<Character>([&](Entity e, Character& o) { if (!o.dead && distXZ(o.pos, p) < reach && std::fabs(o.pos.y - p.y) < 5.f && o.team != Team::Mount) hit.push_back(e); });
    for (Entity e : hit) {
        Character* o = reg.get<Character>(e);
        if (!o) continue;
        float k = 1.f - distXZ(o->pos, p) / reach;
        float amount = dmg * (0.4f + 0.6f * k) * (e == player ? 0.35f : 1.f);   // your own bomb hurts, less
        vec3 dir = normalize(vec3(o->pos.x - p.x, 0, o->pos.z - p.z) + vec3(0.001f, 0, 0));
        dealDamage(src, e, amount, Element::Fire, dir, 14.f * k, true, false);
        if (reg.valid(e)) { Character* oo = reg.get<Character>(e); if (oo) oo->vel.y = std::max(oo->vel.y, 5.f * k); }
    }
}

void Game::updateBombs(float dt) {
    for (size_t i = 0; i < bombs.size();) {
        Bomb& b = bombs[i];
        b.vel.y -= 18.f * dt;
        b.pos += b.vel * dt;
        float gy = groundAt(b.pos.x, b.pos.z);
        vec3 tmp = b.pos; collideStatic(tmp, 0.15f, b.pos.y); if (length(tmp.xz() - b.pos.xz()) > 0.01f) { b.pos = tmp; b.vel.x *= -0.4f; b.vel.z *= -0.4f; }
        if (b.pos.y < gy + 0.15f) {
            b.pos.y = gy + 0.15f;
            if (b.vel.y < -1.5f) { b.vel.y = -b.vel.y * 0.35f; b.bounces++; play3D("hit_wood", b.pos, 0.35f, 1.5f); }
            else b.vel.y = 0;
            b.vel.x *= 0.6f; b.vel.z *= 0.6f;
        }
        b.fuse -= dt;
        if (particles.rng.f() < dt * 25.f) particles.sparks(b.pos + vec3(0, 0.2f, 0), {0, 1, 0}, 2, vec3(1.f, 0.7f, 0.3f));
        if (b.fuse <= 0) {
            vec3 at = b.pos;
            bombs.erase(bombs.begin() + i);
            explodeAt(at, 3.4f, 2.4f, 70.f + level * 7.f, player);
            continue;
        }
        i++;
    }
}

}  // namespace lk
