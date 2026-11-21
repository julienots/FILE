// LOST KINGDOM — 2.8.0 the Lands of Aurora: launch geysers. Step into the glowing pool and the spring throws you
// high into the air; the geysers at the foot of a floating island aim at it, so the arc lands on its deck.
#include "Game.h"

namespace lk {

void Game::updateGeysers(float dt) {
    geyserCD -= dt;
    if (geyserT > 0.f) geyserT -= dt;
    if (player == NullEntity || dungeon.active || riding || flying) return;
    Character& c = pc();
    if (c.dead || c.swimming || geyserCD > 0.f) return;
    if (!c.grounded && c.pos.y - c.groundY > 0.4f) return;
    // a geyser re-arms only once you have stepped out of it (you used to fall back into the pool and fly again, forever)
    if (geyserLast >= 0 && world.geyserNear(c.pos.xz(), 2.6f) != geyserLast) geyserLast = -1;
    int gi = world.geyserNear(c.pos.xz(), 1.25f);
    if (gi < 0 || gi == geyserLast) return;
    geyserLast = gi;
    const Structure& s = world.structures[gi];
    float g = 20.f * gameTuning().gravity;
    vec3 v;
    if (s.variant > 0) {
        // aimed: the top of the arc 2.5 m above the island deck, landing about 9 m in from the geyser
        float H = s.variant / 100.f + 2.5f;
        float vy = std::sqrt(2.f * g * H);
        float t = vy / g + std::sqrt(2.f * 2.5f / g);
        vec2 dir{std::sin(s.rot), std::cos(s.rot)};
        float dist = 3.5f + SKY_ISLAND_R * 0.6f + (length(s.pos - c.pos.xz()) * 0.f);
        v = vec3(dir.x * dist / t, vy, dir.y * dist / t);
        // the launch starts from the centre of the pool, so every jump lands in the same place
        c.pos.x = s.pos.x; c.pos.z = s.pos.y;
    } else {
        // free geyser: about 7 m up, and a push forward so you land beside the pool
        vec3 f{std::sin(c.yaw), 0, std::cos(c.yaw)};
        v = vec3(c.vel.x * 0.5f + f.x * 2.6f, std::sqrt(2.f * g * 7.f), c.vel.z * 0.5f + f.z * 2.6f);
    }
    c.vel = v; c.grounded = false; c.airTime = 0;
    geyserT = s.variant > 0 ? 3.f : 0.8f; geyserCD = 1.2f; fallSafeT = std::max(fallSafeT, 6.f);
    geyserLaunches++;
    particles.splash(c.pos + vec3(0, 0.6f, 0), 2.2f);
    particles.dust(c.pos, 1.4f);
    play3D("splash", c.pos, 1.f);
    play("jump", 0.8f, 0.75f);
    haptic(60, 200);
}

}  // namespace lk
