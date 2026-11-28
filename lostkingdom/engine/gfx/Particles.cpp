#include "Particles.h"
#include <algorithm>

namespace agr {

void ParticleSystem::init(int maxP) {
    maxParts = maxP;
    parts.reserve(maxP);
}

void ParticleSystem::shutdown() {
    if (vao) glDeleteVertexArrays(1, &vao);
    if (ivbo) glDeleteBuffers(1, &ivbo);
    vao = ivbo = 0; capacity = 0;
    parts.clear();
}

void ParticleSystem::spawn(const Particle& p) {
    if ((int)parts.size() >= (int)(maxParts * budgetScale)) return;
    parts.push_back(p);
}

void ParticleSystem::update(float dt) {
    for (size_t i = 0; i < parts.size();) {
        Particle& p = parts[i];
        p.life += dt;
        if (p.life >= p.maxLife) { p = parts.back(); parts.pop_back(); continue; }
        p.vel.y -= p.gravity * dt;
        if (p.turbulence > 0) {
            float t = p.life * 2.3f + p.pos.x * 0.7f;
            p.vel.x += std::sin(t * 1.7f + p.pos.z) * p.turbulence * dt;
            p.vel.z += std::cos(t * 1.3f + p.pos.y) * p.turbulence * dt;
        }
        p.vel = p.vel * std::exp(-p.drag * dt);
        p.pos += p.vel * dt;
        p.rot += p.rotVel * dt;
        if (p.pos.y < p.groundKill) {
            if (p.bounce && p.vel.y < -0.5f) { p.pos.y = p.groundKill; p.vel.y *= -0.35f; p.vel.x *= 0.6f; p.vel.z *= 0.6f; }
            else if (!p.bounce) { p = parts.back(); parts.pop_back(); continue; }
            else { p.pos.y = p.groundKill; p.vel = vec3(0); }
        }
        i++;
    }
}

int ParticleSystem::draw(bool additive, const Mesh& quad) {
    inst.clear();
    int n = 0;
    for (auto& p : parts) {
        if (p.additive != additive) continue;
        float t = p.life / p.maxLife;
        float size = lerpf(p.size0, p.size1, t);
        vec4 c = lerp(p.c0, p.c1, t);
        // fade in quickly to avoid popping
        c.w *= saturate(p.life * 12.f);
        vec3 vd = p.stretch > 0 ? p.vel * 0.06f : vec3(0, 0, 0);
        float d[16] = {p.pos.x, p.pos.y, p.pos.z, size, c.x, c.y, c.z, c.w, p.rot, (float)p.frame, p.stretch, 0, vd.x, vd.y, vd.z, 0};
        inst.insert(inst.end(), d, d + 16);
        n++;
    }
    if (!n) return 0;
    if (!vao) {
        glGenVertexArrays(1, &vao);
        glGenBuffers(1, &ivbo);
        glBindVertexArray(vao);
        glBindBuffer(GL_ARRAY_BUFFER, quad.vbo);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)0);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, quad.ibo);
        glBindBuffer(GL_ARRAY_BUFFER, ivbo);
        for (int k = 0; k < 4; k++) {
            glEnableVertexAttribArray(8 + k);
            glVertexAttribPointer(8 + k, 4, GL_FLOAT, GL_FALSE, 64, (void*)(intptr_t)(k * 16));
            glVertexAttribDivisor(8 + k, 1);
        }
        glBindVertexArray(0);
    }
    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER, ivbo);
    if (n > capacity) {
        capacity = std::max(n, capacity * 2);
        glBufferData(GL_ARRAY_BUFFER, (size_t)capacity * 64, nullptr, GL_STREAM_DRAW);
    }
    glBufferSubData(GL_ARRAY_BUFFER, 0, inst.size() * sizeof(float), inst.data());
    glDrawElementsInstanced(GL_TRIANGLES, 6, GL_UNSIGNED_SHORT, nullptr, n);
    glBindVertexArray(0);
    return 1;
}

// ------------------------------------------------------------------------------------------------
void ParticleSystem::fire(vec3 p, float scale, float dt) {
    accum[0] += dt * 38.f * scale * budgetScale;
    while (accum[0] >= 1.f) {
        accum[0] -= 1.f;
        Particle f;
        vec2 d = rng.inDisk() * 0.22f * scale;
        f.pos = p + vec3(d.x, 0.05f, d.y);
        f.vel = {rng.range(-0.15f, 0.15f), rng.range(1.0f, 1.8f) * scale, rng.range(-0.15f, 0.15f)};
        f.maxLife = rng.range(0.45f, 0.8f);
        f.size0 = rng.range(0.35f, 0.55f) * scale; f.size1 = 0.08f * scale;
        f.c0 = {2.6f, 1.25f, 0.35f, 0.9f}; f.c1 = {1.2f, 0.25f, 0.05f, 0.f};
        f.frame = PF_FLAME; f.rot = rng.range(-0.25f, 0.25f); f.rotVel = rng.range(-0.6f, 0.6f);
        f.turbulence = 0.8f; f.additive = true;
        spawn(f);
        if (rng.chance(0.12f)) {
            Particle e = f;
            e.frame = PF_EMBER; e.size0 = 0.06f * scale; e.size1 = 0.02f;
            e.vel = {rng.range(-0.6f, 0.6f), rng.range(1.5f, 3.f), rng.range(-0.6f, 0.6f)};
            e.maxLife = rng.range(1.f, 2.f); e.turbulence = 2.5f;
            e.c0 = {3.f, 1.4f, 0.4f, 1.f}; e.c1 = {1.5f, 0.3f, 0.05f, 0.f};
            spawn(e);
        }
    }
}

void ParticleSystem::smoke(vec3 p, float scale, float dt, vec3 tint) {
    accum[1] += dt * 5.f * scale * budgetScale;
    while (accum[1] >= 1.f) {
        accum[1] -= 1.f;
        Particle s;
        s.pos = p + vec3(rng.range(-0.1f, 0.1f), 0, rng.range(-0.1f, 0.1f));
        s.vel = {rng.range(-0.15f, 0.15f) + 0.3f, rng.range(0.6f, 1.0f), rng.range(-0.15f, 0.15f)};
        s.maxLife = rng.range(3.f, 5.f);
        s.size0 = 0.4f * scale; s.size1 = 2.2f * scale;
        s.c0 = vec4(tint, 0.45f); s.c1 = vec4(tint * 1.3f, 0.f);
        s.frame = PF_SMOKE; s.rot = rng.range(0, TAU); s.rotVel = rng.range(-0.3f, 0.3f);
        s.drag = 0.15f; s.turbulence = 0.3f; s.additive = false;
        spawn(s);
    }
}

void ParticleSystem::sparks(vec3 p, vec3 dir, int n, vec3 color) {
    n = std::max(1, (int)(n * budgetScale));
    for (int i = 0; i < n; i++) {
        Particle s;
        s.pos = p;
        vec3 r = rng.onSphere();
        s.vel = normalize(dir + r * 0.8f) * rng.range(3.f, 8.f);
        s.maxLife = rng.range(0.2f, 0.5f);
        s.size0 = rng.range(0.05f, 0.09f); s.size1 = 0.01f;
        s.c0 = vec4(color * 3.f, 1.f); s.c1 = vec4(color, 0.f);
        s.frame = PF_SPARK; s.stretch = 1.5f; s.gravity = 9.f; s.drag = 1.f;
        s.groundKill = p.y - 1.5f;
        spawn(s);
    }
}

void ParticleSystem::impact(vec3 p, vec3 n, vec3 color, float scale) {
    sparks(p, n, (int)(10 * scale), color);
    Particle f;
    f.pos = p; f.maxLife = 0.14f; f.size0 = 0.9f * scale; f.size1 = 1.4f * scale;
    f.c0 = vec4(color * 3.f, 1.f); f.c1 = vec4(color, 0.f); f.frame = PF_STAR; f.rot = rng.range(0, TAU);
    spawn(f);
    Particle r = f; r.frame = PF_RING; r.maxLife = 0.25f; r.size0 = 0.3f * scale; r.size1 = 1.8f * scale; r.c0 = vec4(color * 1.5f, 0.8f);
    spawn(r);
}

void ParticleSystem::shockwave(vec3 p, vec3 color, float radius) {
    Particle r;
    r.pos = p + vec3(0, 0.15f, 0); r.maxLife = 0.45f; r.size0 = 0.5f; r.size1 = radius * 2.2f;
    r.c0 = vec4(color * 2.f, 0.9f); r.c1 = vec4(color, 0.f); r.frame = PF_RING;
    spawn(r);
    int n = (int)(18 * budgetScale);
    for (int i = 0; i < n; i++) {
        float a = TAU * i / n;
        Particle d;
        d.pos = p + vec3(std::cos(a) * 0.4f, 0.1f, std::sin(a) * 0.4f);
        d.vel = vec3(std::cos(a), 0.25f, std::sin(a)) * radius * 2.2f;
        d.maxLife = rng.range(0.5f, 0.9f); d.size0 = 0.4f; d.size1 = 1.3f; d.drag = 3.f;
        d.c0 = {0.5f, 0.45f, 0.38f, 0.5f}; d.c1 = {0.6f, 0.55f, 0.5f, 0.f}; d.frame = PF_DUST; d.additive = false;
        d.rot = rng.range(0, TAU);
        spawn(d);
    }
}

void ParticleSystem::dust(vec3 p, float scale, vec3 color) {
    int n = std::max(1, (int)(4 * scale * budgetScale));
    for (int i = 0; i < n; i++) {
        Particle d;
        d.pos = p + vec3(rng.range(-0.2f, 0.2f), 0.05f, rng.range(-0.2f, 0.2f));
        d.vel = {rng.range(-0.6f, 0.6f), rng.range(0.2f, 0.7f), rng.range(-0.6f, 0.6f)};
        d.maxLife = rng.range(0.6f, 1.1f); d.size0 = 0.2f * scale; d.size1 = 0.7f * scale; d.drag = 2.f;
        d.c0 = vec4(color, 0.45f); d.c1 = vec4(color, 0.f); d.frame = PF_DUST; d.additive = false; d.rot = rng.range(0, TAU);
        spawn(d);
    }
}

void ParticleSystem::magicBurst(vec3 p, vec3 color, float scale, int n) {
    n = std::max(4, (int)(n * budgetScale));
    for (int i = 0; i < n; i++) {
        Particle m;
        m.pos = p;
        m.vel = rng.onSphere() * rng.range(1.5f, 4.5f) * scale;
        m.maxLife = rng.range(0.4f, 0.9f); m.size0 = rng.range(0.1f, 0.25f) * scale; m.size1 = 0.f;
        m.c0 = vec4(color * 3.f, 1.f); m.c1 = vec4(color, 0.f); m.frame = i % 3 == 0 ? PF_STAR : PF_DOT; m.drag = 2.5f; m.turbulence = 2.f;
        spawn(m);
    }
    Particle f; f.pos = p; f.maxLife = 0.3f; f.size0 = 1.4f * scale; f.size1 = 2.4f * scale;
    f.c0 = vec4(color * 2.f, 0.8f); f.c1 = vec4(color, 0.f); f.frame = PF_DOT;
    spawn(f);
}

void ParticleSystem::magicTrail(vec3 p, vec3 color, float scale) {
    Particle m;
    m.pos = p + rng.onSphere() * 0.08f * scale;
    m.vel = rng.onSphere() * 0.3f;
    m.maxLife = rng.range(0.25f, 0.5f); m.size0 = 0.28f * scale; m.size1 = 0.02f;
    m.c0 = vec4(color * 2.5f, 0.9f); m.c1 = vec4(color, 0.f); m.frame = PF_DOT;
    spawn(m);
}

void ParticleSystem::heal(vec3 p, vec3 color) {
    int n = (int)(14 * budgetScale) + 2;
    for (int i = 0; i < n; i++) {
        Particle h;
        vec2 d = rng.inDisk() * 0.6f;
        h.pos = p + vec3(d.x, rng.range(0.f, 1.2f), d.y);
        h.vel = {0, rng.range(0.6f, 1.4f), 0};
        h.maxLife = rng.range(0.8f, 1.4f); h.size0 = rng.range(0.12f, 0.22f); h.size1 = 0.f;
        h.c0 = vec4(color * 2.f, 1.f); h.c1 = vec4(color, 0.f); h.frame = i % 2 ? PF_CROSS : PF_DOT;
        spawn(h);
    }
}

void ParticleSystem::explosion(vec3 p, float scale, vec3 color) {
    Particle f; f.pos = p; f.maxLife = 0.35f; f.size0 = 2.f * scale; f.size1 = 4.f * scale;
    f.c0 = vec4(color * 4.f, 1.f); f.c1 = vec4(color, 0.f); f.frame = PF_DOT; spawn(f);
    int n = (int)(26 * budgetScale) + 4;
    for (int i = 0; i < n; i++) {
        Particle e;
        e.pos = p;
        e.vel = rng.onSphere() * rng.range(2.f, 7.f) * scale; e.vel.y = std::fabs(e.vel.y);
        e.maxLife = rng.range(0.4f, 0.8f); e.size0 = rng.range(0.6f, 1.1f) * scale; e.size1 = 0.2f;
        e.c0 = vec4(color * 2.5f, 1.f); e.c1 = {0.4f, 0.1f, 0.02f, 0.f}; e.frame = PF_FLAME; e.drag = 3.f; e.rot = rng.range(0, TAU);
        spawn(e);
        Particle s = e; s.additive = false; s.frame = PF_SMOKE; s.maxLife = rng.range(1.5f, 2.5f); s.size0 = 0.8f * scale; s.size1 = 2.5f * scale;
        s.c0 = {0.2f, 0.18f, 0.16f, 0.5f}; s.c1 = {0.3f, 0.28f, 0.26f, 0.f}; s.vel = s.vel * 0.4f; s.vel.y += 1.f;
        if (i % 2 == 0) spawn(s);
    }
    sparks(p, {0, 1, 0}, 16, color);
    shockwave(p, color, 3.f * scale);
}

void ParticleSystem::splash(vec3 p, float scale) {
    int n = (int)(14 * scale * budgetScale) + 2;
    for (int i = 0; i < n; i++) {
        Particle s;
        s.pos = p;
        vec2 d = rng.inDisk();
        s.vel = vec3(d.x * 1.5f, rng.range(2.f, 4.f), d.y * 1.5f) * scale;
        s.maxLife = 0.8f; s.size0 = 0.08f; s.size1 = 0.03f; s.gravity = 9.81f;
        s.c0 = {0.8f, 0.9f, 1.f, 0.8f}; s.c1 = {0.8f, 0.9f, 1.f, 0.f}; s.frame = PF_RAIN; s.stretch = 0.8f; s.additive = false;
        s.groundKill = p.y - 0.05f;
        spawn(s);
    }
    Particle r; r.pos = p + vec3(0, 0.02f, 0); r.maxLife = 0.6f; r.size0 = 0.3f; r.size1 = 1.6f * scale;
    r.c0 = {0.9f, 0.95f, 1.f, 0.6f}; r.c1 = {0.9f, 0.95f, 1.f, 0.f}; r.frame = PF_RING; r.additive = false; spawn(r);
}

void ParticleSystem::iceShards(vec3 p, int n) {
    n = (int)(n * budgetScale) + 2;
    for (int i = 0; i < n; i++) {
        Particle s;
        s.pos = p;
        s.vel = rng.onSphere() * rng.range(2.f, 5.f); s.vel.y = std::fabs(s.vel.y) + 1.f;
        s.maxLife = rng.range(0.6f, 1.f); s.size0 = rng.range(0.12f, 0.25f); s.size1 = 0.05f; s.gravity = 9.f;
        s.c0 = {1.4f, 2.f, 2.6f, 1.f}; s.c1 = {0.6f, 0.9f, 1.2f, 0.f}; s.frame = PF_SHARD; s.rot = rng.range(0, TAU); s.rotVel = rng.range(-8.f, 8.f);
        s.groundKill = p.y - 1.f; s.bounce = true;
        spawn(s);
    }
}

void ParticleSystem::lightningArc(vec3 a, vec3 b, vec3 color) {
    vec3 d = b - a;
    int segs = 8;
    vec3 prev = a;
    for (int i = 1; i <= segs; i++) {
        float t = (float)i / segs;
        vec3 p = a + d * t + (i < segs ? rng.onSphere() * 0.35f : vec3(0));
        int sub = 4;
        for (int k = 0; k < sub; k++) {
            Particle l;
            l.pos = lerp(prev, p, (k + 0.5f) / sub);
            l.maxLife = rng.range(0.08f, 0.16f); l.size0 = 0.22f; l.size1 = 0.1f;
            l.c0 = vec4(color * 4.f, 1.f); l.c1 = vec4(color * 2.f, 0.f); l.frame = PF_DOT;
            spawn(l);
        }
        prev = p;
    }
    Particle f; f.pos = b; f.maxLife = 0.15f; f.size0 = 1.5f; f.size1 = 2.f; f.c0 = vec4(color * 3.f, 1.f); f.c1 = vec4(color, 0.f); f.frame = PF_STAR;
    spawn(f);
}

void ParticleSystem::levelUp(vec3 p) {
    int n = (int)(40 * budgetScale) + 8;
    for (int i = 0; i < n; i++) {
        float a = TAU * i / n;
        Particle l;
        l.pos = p + vec3(std::cos(a) * 0.8f, 0.1f, std::sin(a) * 0.8f);
        l.vel = vec3(-std::sin(a) * 0.8f, rng.range(1.5f, 3.f), std::cos(a) * 0.8f);
        l.maxLife = rng.range(1.f, 1.6f); l.size0 = 0.2f; l.size1 = 0.f; l.drag = 0.5f;
        l.c0 = {3.f, 2.4f, 0.9f, 1.f}; l.c1 = {1.f, 0.6f, 0.2f, 0.f}; l.frame = i % 3 ? PF_DOT : PF_STAR;
        spawn(l);
    }
    Particle r; r.pos = p + vec3(0, 0.1f, 0); r.maxLife = 1.f; r.size0 = 0.5f; r.size1 = 4.f;
    r.c0 = {2.5f, 1.9f, 0.7f, 1.f}; r.c1 = {1.f, 0.6f, 0.2f, 0.f}; r.frame = PF_RUNE; spawn(r);
}

void ParticleSystem::dissolveBurst(vec3 p, vec3 color, float radius) {
    int n = (int)(30 * budgetScale) + 4;
    for (int i = 0; i < n; i++) {
        Particle d;
        vec2 o = rng.inDisk() * radius;
        d.pos = p + vec3(o.x, rng.range(0.f, radius * 2.f), o.y);
        d.vel = vec3(rng.range(-0.3f, 0.3f), rng.range(0.8f, 2.f), rng.range(-0.3f, 0.3f));
        d.maxLife = rng.range(0.8f, 1.6f); d.size0 = rng.range(0.06f, 0.14f); d.size1 = 0.f; d.turbulence = 1.5f;
        d.c0 = vec4(color * 3.f, 1.f); d.c1 = vec4(color, 0.f); d.frame = PF_EMBER;
        spawn(d);
    }
}

}  // namespace agr
