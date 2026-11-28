// LOST KINGDOM 2.2.0 - dust teleport: the hero crumbles from the feet up into golden dust that drifts and rises to the
// sky (dissolve shader with a burning edge + a few hundred particles), the world fades and streams in at the
// destination, then the dust falls back from above and the hero re-forms from the head down.
#include "Game.h"

namespace lk {

static const float TELE_OUT = 1.6f, TELE_IN = 1.4f;

void Game::startDustTeleport(vec3 dest, const std::string& msg) {
    if (tele.active || player == NullEntity) return;
    tele = TeleFx(); tele.active = true; tele.phase = 0; tele.t = 0; tele.dest = dest; tele.msg = msg;
    lockTarget = NullEntity;
    play("heal", 0.7f, 0.75f);
    haptic(20, 120);
}

// a random point on the hero's body (a capsule from the feet to the head), weighted to the band that is dissolving
static vec3 bodyPoint(Rng& r, vec3 feet, float h, float band) {
    float y = clampf(band + r.range(-0.18f, 0.18f), 0.02f, 0.98f) * h;
    float a = r.range(0, TAU), rad = r.range(0.05f, 0.32f) * (y > h * 0.82f ? 0.6f : 1.f);
    return feet + vec3(std::cos(a) * rad, y, std::sin(a) * rad);
}

void Game::updateTeleportFx(float dt) {
    if (!tele.active || player == NullEntity) return;
    Character& c = pc();
    tele.t += dt;
    c.vel = vec3(0); c.invuln = std::max(c.invuln, 0.5f);
    const float h = 1.8f * c.vis.scale;
    Rng& r = particles.rng;
    float budget = renderer.settings.particleScale;
    vec3 gold(1.f, 0.78f, 0.38f);
    c.vis.dissolveColor = gold * 1.4f;
    if (tele.phase == 0) {
        float k = saturate(tele.t / TELE_OUT);
        c.vis.dissolve = std::max(c.vis.dissolve, k * k * (3.f - 2.f * k) * 1.02f);   // feet first: the noise is world-space, the edge glows
        int n = (int)(dt * 420.f * budget * (0.5f + k));
        for (int i = 0; i < n; i++) {
            Particle p;
            p.pos = bodyPoint(r, c.pos, h, k);
            vec3 out = normalize(vec3(p.pos.x - c.pos.x, 0.f, p.pos.z - c.pos.z) + vec3(0.001f));
            p.vel = out * r.range(0.2f, 0.9f) + vec3(env.windDir.x, 0, env.windDir.y) * r.range(0.3f, 1.2f) + vec3(0, r.range(1.2f, 3.4f), 0);
            p.drag = 0.15f; p.gravity = -0.6f; p.turbulence = r.range(0.6f, 1.8f);
            p.maxLife = r.range(2.2f, 3.8f);
            bool glow = r.chance(0.55f);
            p.size0 = glow ? r.range(0.05f, 0.1f) : r.range(0.06f, 0.12f); p.size1 = 0.f;
            if (r.chance(0.15f)) { p.stretch = 0.6f; p.vel.y *= 1.6f; }   // a few bright motes streak upwards
            p.c0 = glow ? vec4(gold * 3.2f, 1.f) : vec4(0.82f, 0.7f, 0.52f, 0.85f);
            p.c1 = glow ? vec4(gold * 0.6f, 0.f) : vec4(0.7f, 0.62f, 0.5f, 0.f);
            p.frame = glow ? PF_DOT : PF_DUST; p.additive = glow;
            particles.spawn(p);
        }
        if (r.f() < dt * 6.f) particles.magicTrail(c.pos + vec3(0, h * k, 0), gold, 1.2f);
        // a soft column of light rising from the feet while the body crumbles
        if (r.f() < dt * 30.f * budget) {
            Particle p; p.pos = c.pos + vec3(r.range(-0.3f, 0.3f), r.range(0.f, 0.4f), r.range(-0.3f, 0.3f));
            p.vel = vec3(0, r.range(4.f, 7.f), 0); p.maxLife = 1.4f; p.size0 = 0.35f; p.size1 = 0.05f; p.stretch = 1.2f;
            p.c0 = vec4(gold * 1.2f, 0.35f); p.c1 = vec4(gold * 0.4f, 0.f); p.frame = PF_DOT; p.additive = true;
            particles.spawn(p);
        }
        if (tele.t >= TELE_OUT) {
            // gone: move, stream the destination behind the loading screen, then re-form
            c.pos = tele.dest; c.pos.y = groundAt(tele.dest.x, tele.dest.z) + 0.05f; c.vel = vec3(0);
            camFocus = c.pos;
            loadGate = true; loadGateT = 0; loadGateReady = 0; loadGateFade = 0;
            tele.phase = 1; tele.t = 0;
            c.vis.dissolve = 1.f;
        }
    } else {
        if (loadGate) { tele.t = 0; c.vis.dissolve = 1.f; return; }   // still loading: wait
        float k = saturate(tele.t / TELE_IN);
        c.vis.dissolve = 1.f - k * k * (3.f - 2.f * k);
        // dust falls from above and gathers into the body, head first
        int n = (int)(dt * 200.f * budget * (1.f - k * 0.6f));
        for (int i = 0; i < n; i++) {
            Particle p;
            vec3 target = bodyPoint(r, c.pos, h, 1.f - k);
            float fall = r.range(1.6f, 3.2f);
            p.pos = target + vec3(r.range(-0.6f, 0.6f), fall, r.range(-0.6f, 0.6f));
            p.vel = (target - p.pos) / 0.55f; p.drag = 0.2f; p.turbulence = 0.4f;
            p.maxLife = 0.55f; p.size0 = r.range(0.03f, 0.06f); p.size1 = 0.01f;
            p.c0 = vec4(gold * 2.6f, 0.f); p.c1 = vec4(gold * 3.4f, 1.f);
            p.frame = PF_DOT; p.additive = true;
            particles.spawn(p);
        }
        if (tele.t >= TELE_IN) {
            c.vis.dissolve = 0.f;
            particles.shockwave(c.pos + vec3(0, 0.1f, 0), gold, 2.5f);
            if (!tele.msg.empty()) notify(tele.msg, vec3(0.8f, 0.9f, 1.f), "i_waypoint");
            tele.active = false;
        }
    }
}

// on-screen markers for the personal pins: a diamond with the distance, pinned to the screen edge when off-screen
void Game::drawPinsHud() {
    if (mapPins.empty() || player == NullEntity || dungeon.active || cine.active) return;
    UI& u = ui;
    Character& p = pc();
    // 3.1.0 pins pushed to the top edge used to sit on the quest tracker (y 10..76) and on each other: the top edge
    // is now below the tracker, and markers that would overlap are stacked under the previous one
    std::vector<vec2> placed;
    const float m = 60.f, top = 104.f;
    for (size_t i = 0; i < mapPins.size(); i++) {
        vec2 w = mapPins[i];
        float d = length(w - p.pos.xz());
        if (d < 4.f) continue;
        vec3 wp(w.x, world.height(w.x, w.y) + 2.2f, w.y);
        bool on;
        vec2 sp = worldToScreen(wp, &on);
        bool inside = on && sp.x > m && sp.x < u.W - m && sp.y > top && sp.y < u.H - m;
        if (!inside) {
            // direction on screen from the camera's forward: project onto the screen edge (the top edge sits under the tracker)
            vec2 dir = normalize(minimapProject(w - p.pos.xz(), camYaw) + vec2(1e-4f));
            vec2 cc{u.W / 2, (top + u.H - m) / 2};
            float hy = dir.y < 0 ? cc.y - top : (u.H - m) - cc.y;
            float tx = dir.x != 0 ? (u.W / 2 - m) / std::fabs(dir.x) : 1e9f, ty = dir.y != 0 ? hy / std::fabs(dir.y) : 1e9f;
            sp = cc + dir * std::min(tx, ty);
        }
        for (int guard = 0; guard < 8; guard++) {
            bool hit = false;
            for (const vec2& q : placed) if (std::fabs(q.x - sp.x) < 64.f && std::fabs(q.y - sp.y) < 36.f) { sp.y = q.y + (q.y > u.H * 0.5f ? -38.f : 38.f); hit = true; }   // away from the nearest edge
            if (!hit) break;
        }
        placed.push_back(sp);
        vec4 col(0.35f, 0.9f, 1.f, 0.95f);
        u.circle(sp, 13.f, vec4(0.03f, 0.08f, 0.12f, 0.85f), 4);
        u.circle(sp, 9.f, col, 4);
        char b[32]; snprintf(b, sizeof b, d < 1000 ? "%.0f m" : "%.1f km", d < 1000 ? d : d / 1000.f);
        u.text(b, sp.x, sp.y + 14, 15, rgba(0xd8f4ff), F_BOLD, A_CENTER, 0, true);
    }
}

}  // namespace lk
