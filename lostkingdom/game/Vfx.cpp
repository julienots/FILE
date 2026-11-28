// LOST KINGDOM — 3.0.1 shader effects: the visible side of the powers and spells.
// Each effect is one procedural shape (disc, cylinder, sphere, arc, cone) drawn by the FX shader of the
// renderer (animated noise, fresnel, procedural runes, additive HDR so the bloom makes it glow), and lights the
// ground and the characters around it with a coloured point light while it lives.
#include "Game.h"
#include <algorithm>

namespace lk {

Game::Vfx& Game::vfxAdd(int shape, int style, vec3 pos, vec3 col, vec3 col2, float radius, float life) {
    if (vfxList.size() > 96) vfxList.erase(vfxList.begin());   // a hard cap (big fights)
    Vfx v; v.shape = shape; v.style = style; v.pos = pos; v.col = col; v.col2 = col2; v.radius = radius; v.life = std::max(0.05f, life);
    v.seed = particles.rng.range(0.f, 10.f);
    vfxList.push_back(v);
    return vfxList.back();
}
void Game::vfxShockwave(vec3 p, vec3 col, float radius, float life) { vfxAdd(FX_DISC, 0, p + vec3(0, 0.08f, 0), col, col * 0.5f, radius, life).light = 1.2f; }
void Game::vfxCircle(vec3 p, vec3 col, float radius, float life, Entity follow, vec3 dir) {
    Vfx& v = vfxAdd(FX_DISC, 1, p + vec3(0, 0.06f, 0), col, lerp(col, vec3(1.f), 0.25f), radius, life);
    v.follow = follow; v.followOff = vec3(0, 0.06f, 0); v.dir = normalize(dir); v.light = 0.8f;
}
void Game::vfxBeam(vec3 a, vec3 b, vec3 col, float width, float life, float light) {
    Vfx& v = vfxAdd(FX_CYLINDER, 0, a, vec3(1.f, 0.97f, 0.9f), col, width, life);
    v.col = lerp(col, vec3(1.f), 0.2f); v.col2 = col;
    v.dir = normalize(b - a); v.length = length(b - a); v.intensity = 1.8f; v.light = light;
}
void Game::vfxPillar(vec3 p, vec3 col, float radius, float height, float life) { Vfx& v = vfxAdd(FX_CYLINDER, 1, p, lerp(col, vec3(1.f), 0.15f), col, radius, life); v.length = height; v.light = 1.2f; }
void Game::vfxVortex(vec3 p, vec3 col, float radius, float height, float life, Entity follow) {
    Vfx& v = vfxAdd(FX_CONE, 2, p, lerp(col, vec3(1.f), 0.25f), col, radius, life); v.length = height; v.follow = follow; v.light = 0.9f;
}
void Game::vfxSphere(vec3 p, vec3 col, vec3 col2, float radius, float life, int style, float grow, Entity follow) {
    Vfx& v = vfxAdd(FX_SPHERE, style, p, col, col2, radius, life); v.grow = grow; v.follow = follow;
    if (follow != NullEntity) { if (Character* c = reg.get<Character>(follow)) v.followOff = p - c->pos; }
    v.light = style == 1 ? 1.5f : 0.8f; if (style == 1) v.intensity = 1.6f; if (style == 0) v.intensity = 1.1f;
}
void Game::vfxSlash(vec3 p, float yaw, float tilt, vec3 col, float radius, float life) {
    Vfx& v = vfxAdd(FX_ARC, 0, p, lerp(col, vec3(1.f), 0.15f), col, radius, life); v.yaw = yaw; v.tilt = tilt; v.intensity = 2.f; v.light = 0.6f;
}
void Game::vfxGround(vec3 p, vec3 col, vec3 col2, float radius, float life) { vfxAdd(FX_DISC, 2, p + vec3(0, 0.07f, 0), col, col2, radius, life).light = 1.f; }
void Game::vfxRays(vec3 p, vec3 col, float radius, float life) { vfxAdd(FX_DISC, 3, p + vec3(0, 0.09f, 0), lerp(col, vec3(1.f), 0.2f), col, radius, life).light = 0.8f; }

void Game::updateVfx(float dt) {
    for (Vfx& v : vfxList) {
        v.age += dt;
        if (v.follow != NullEntity) {
            Character* c = reg.valid(v.follow) ? reg.get<Character>(v.follow) : nullptr;
            if (c && !c->dead) v.pos = c->pos + v.followOff; else v.follow = NullEntity;
        }
    }
    vfxList.erase(std::remove_if(vfxList.begin(), vfxList.end(), [](const Vfx& v) { return v.age >= v.life; }), vfxList.end());
}

// rotation that takes +Y onto d
static quat upTo(vec3 d) {
    d = normalize(d);
    float c = d.y;
    if (c > 0.9999f) return quat();
    if (c < -0.9999f) return quat::axisAngle({1, 0, 0}, PI);
    vec3 ax = normalize(cross(vec3(0, 1, 0), d));
    return quat::axisAngle(ax, std::acos(clampf(c, -1.f, 1.f)));
}

void Game::renderVfx(std::vector<std::pair<float, PointLight>>& lights) {
    vec3 focus = camFocus;
    // auras that last as long as their state: Astra in flight (a golden shell and a sun circle under her),
    // Kaede in the shadows (a violet veil)
    if (player != NullEntity && !pc().dead) {
        Character& c = pc();
        auto aura = [&](vec3 col, vec3 col2, float r, float inten) {
            FxItem f; f.shape = FX_SPHERE; f.style = 0; f.color = vec4(col, 1); f.color2 = vec4(col2, 1); f.age = 0.5f; f.time = realTime; f.seed = 3.f; f.intensity = inten;
            f.model = mat4::trs(c.pos + vec3(0, 1.f, 0), quat::axisAngle({0, 1, 0}, realTime), vec3(r, r * 1.25f, r));
            renderer.submitFx(f);
        };
        if (flying) {
            aura(vec3(1.f, 0.85f, 0.5f), vec3(1.f, 0.6f, 0.2f), 1.1f, 1.6f);
            FxItem f; f.shape = FX_DISC; f.style = 1; f.color = vec4(1.f, 0.8f, 0.35f, 1); f.color2 = vec4(1.f, 0.95f, 0.7f, 1); f.age = 0.5f; f.time = realTime; f.seed = 1.f; f.intensity = 1.6f;
            vec3 gp = c.pos; gp.y = groundAt(c.pos.x, c.pos.z) + 0.06f;
            f.model = mat4::trs(gp, quat(), vec3(1.6f, 1, 1.6f));
            renderer.submitFx(f);
        }
        if (stealthT > 0) aura(vec3(0.5f, 0.3f, 0.8f), vec3(0.3f, 0.1f, 0.5f), 0.95f, 1.0f);
    }
    for (const Vfx& v : vfxList) {
        float k = v.age / v.life;
        FxItem f; f.shape = v.shape; f.style = v.style; f.age = k; f.time = v.age; f.seed = v.seed;
        f.color = vec4(v.col, 1); f.color2 = vec4(v.col2, 1); f.intensity = v.intensity;
        float r = v.radius * (1.f + v.grow * k);
        switch (v.shape) {
        case FX_DISC: f.model = mat4::trs(v.pos, upTo(v.dir), vec3(r, 1, r)); break;
        case FX_CYLINDER: case FX_CONE: f.model = mat4::trs(v.pos, upTo(v.dir), vec3(r, v.length, r)); break;
        case FX_SPHERE: f.model = mat4::trs(v.pos, quat::axisAngle({0, 1, 0}, v.seed), vec3(r)); break;
        case FX_ARC: f.model = mat4::trs(v.pos, quat::axisAngle({0, 1, 0}, v.yaw) * quat::axisAngle({0, 0, 1}, v.tilt), vec3(r, 1, r)); break;
        }
        renderer.submitFx(f);
        if (v.light > 0) {
            float fade = (1.f - k) * std::min(1.f, v.age * 12.f);
            PointLight pl; pl.pos = v.pos + vec3(0, 1.f, 0); if (v.shape == FX_CYLINDER) pl.pos = v.pos + v.dir * std::min(v.length * 0.5f, 6.f);
            pl.radius = std::max(4.f, r * 2.2f); pl.color = v.col2 * (1.4f * v.light * fade); pl.flicker = 0.2f;
            lights.push_back({length2(pl.pos - focus) * 0.25f, pl});   // effects win over lanterns
        }
    }
}

}  // namespace lk
