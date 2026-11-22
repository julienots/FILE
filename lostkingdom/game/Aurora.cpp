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
    if (geyserLast >= 0 && world.geyserNear(c.pos.xz(), 2.6f, c.pos.y) != geyserLast) geyserLast = -1;
    int gi = world.geyserNear(c.pos.xz(), 1.25f, c.pos.y);
    if (gi < 0 || gi == geyserLast) return;
    geyserLast = gi;
    const Structure& s = world.structures[gi];
    float g = 20.f * gameTuning().gravity;
    vec3 v;
    if (s.variant > 0) {
        // aimed: the top of the arc 2.5 m above the island deck, landing about 9 m in from the geyser
        // 3.2.0 deck geysers (S_SKYGEYSER) carry their own climb and distance, to the next island up
        bool deck = s.kind == S_SKYGEYSER;
        float climb = s.variant / 100.f, dist = 3.5f + SKY_ISLAND_R * 0.6f;
        vec2 dir{std::sin(s.rot), std::cos(s.rot)};
        if (deck) {   // aim at the next island up, ahead of the spring
            climb = 0; float best = 1e9f;
            for (const Structure& o : world.structures) {
                if (o.kind != S_SKYISLAND) continue;
                vec2 to = o.pos - s.pos; float d = length(to);
                float dy = o.variant / 100.f - s.variant / 100.f;
                if (d < 5.f || d > 45.f || dy < 2.f || dot(to / d, dir) < 0.7f) continue;
                if (d < best) { best = d; climb = dy + 0.4f; dist = d - SKY_ISLAND_R * 0.35f; }
            }
            if (climb <= 0) { climb = 6.f; dist = 3.f; }
        }
        float H = climb + 2.5f;
        float vy = std::sqrt(2.f * g * H);
        float t = vy / g + std::sqrt(2.f * 2.5f / g);
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
