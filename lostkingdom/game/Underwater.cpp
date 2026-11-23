// LOST KINGDOM — 3.5.0 life under the water: schools of fish circling over the sea floor and in the lakes.
// Each 22 m cell of deep enough water may hold a school (deterministic), anchored half-way between the floor and
// the surface; its fish swim in loose circles around the anchor, each on its own radius, height and speed, with a
// slow drift up and down. All fish are one instanced mesh, tinted per school (the tint keeps the stripes).
#include "Game.h"

namespace lk {

static void buildFish(MeshBuilder& b) {
    Mat body(vec3(0.92f), 0.3f, 0.35f), belly(vec3(1.f), 0.35f, 0.2f), fin(vec3(0.8f), 0.5f), dark(vec3(0.35f), 0.4f), eye(vec3(0.04f), 0.1f);
    b.ellipsoid({0, 0, 0}, vec3(0.055f, 0.08f, 0.2f), body, 12, 8);
    b.ellipsoid({0, -0.025f, 0.02f}, vec3(0.045f, 0.05f, 0.16f), belly, 10, 6);
    for (int i = 0; i < 2; i++) b.ellipsoid({0, 0.01f, -0.02f + i * 0.1f}, vec3(0.057f, 0.07f, 0.018f), dark, 10, 6);   // two stripes
    b.blade({0, 0, -0.17f}, {0, 0.09f, -0.3f}, 0.09f, {1, 0, 0}, fin, fin);   // tail
    b.blade({0, 0, -0.17f}, {0, -0.08f, -0.3f}, 0.09f, {1, 0, 0}, fin, fin);
    b.blade({0, 0.07f, 0.02f}, {0, 0.13f, -0.08f}, 0.08f, {0, 0, 1}, fin, fin);   // dorsal fin
    for (int s = -1; s <= 1; s += 2) {
        b.blade({s * 0.04f, -0.02f, 0.06f}, {s * 0.1f, -0.05f, 0.0f}, 0.04f, {0, 0, 1}, fin, fin);   // pectoral fins
        b.ellipsoid({s * 0.04f, 0.02f, 0.14f}, vec3(0.014f), eye, 6, 4);
    }
}

void Game::renderFish() {
    if (dungeon.active || state != GameState::Playing) return;
    vec3 f = camFocus;
    // only near water (the fish are seen from the shore in clear lakes too)
    if (env.waterLevel < -1e8f && !underwaterView) return;
    if (!fishMesh) { MeshBuilder b; buildFish(b); fishMesh = std::make_shared<Mesh>(); b.upload(*fishMesh); }
    static const uint32_t pal[] = {0x9ab8d8, 0xffa040, 0xffe060, 0x6ad0ff, 0xd8e0e8, 0xff6a7a, 0x8ae0a0, 0xc890ff};
    fishInst.clear();
    const float CELL = 22.f;
    int cx0 = (int)std::floor(f.x / CELL), cz0 = (int)std::floor(f.z / CELL);
    float t = realTime;
    for (int dz = -2; dz <= 2; dz++)
        for (int dx = -2; dx <= 2; dx++) {
            int cx = cx0 + dx, cz = cz0 + dz;
            uint32_t h = hash2i(cx, cz, 6161);
            Rng r(h);
            if (r.f() > 0.55f) continue;
            vec2 a{(cx + r.range(0.25f, 0.75f)) * CELL, (cz + r.range(0.25f, 0.75f)) * CELL};
            if (distXZ(vec3(a.x, 0, a.y), f) > 48.f) continue;
            bool lv = false, fr = false;
            float wl = waterAt(a.x, a.y, &lv, &fr);
            if (wl < -1e8f || lv || fr) continue;
            float gy = groundAt(a.x, a.y), depth = wl - gy;
            if (depth < 2.2f) continue;
            int n = r.irange(6, 15);
            float ay = gy + depth * r.range(0.3f, 0.65f), R = r.range(2.5f, 5.5f), spd = r.range(0.5f, 1.1f) * (r.chance(0.5f) ? 1.f : -1.f);
            vec3 tint = hexColor(pal[r.irange(0, 7)]);
            float sc = r.range(0.7f, 1.4f) * (depth > 6.f ? 1.2f : 1.f);
            for (int i = 0; i < n; i++) {
                float ri = R * r.range(0.6f, 1.2f), ph = r.range(0, TAU), yo = r.range(-0.8f, 0.8f), wob = r.range(0.6f, 1.4f);
                float ang = ph + t * spd / ri * 2.f;
                vec3 p{a.x + std::cos(ang) * ri, ay + yo + std::sin(t * wob + ph) * 0.35f, a.y + std::sin(ang) * ri};
                p.y = clampf(p.y, gy + 0.4f, wl - 0.35f);
                vec2 dir = vec2(-std::sin(ang), std::cos(ang)) * (spd > 0 ? 1.f : -1.f);
                InstanceData d;
                d.x = p.x; d.y = p.y; d.z = p.z;
                d.rotY = std::atan2(dir.x, dir.y) + std::sin(t * 9.f + ph) * 0.12f;   // a wiggle as it swims
                d.scale = sc * r.range(0.85f, 1.15f);
                Color8 c = toColor8(tint * 0.5f * r.range(0.85f, 1.1f), 1);
                d.r = c.r; d.g = c.g; d.b = c.b; d.a = 255;
                fishInst.push_back(d);
            }
        }
    if (fishInst.empty()) return;
    fishMesh->setInstances(fishInst);
    DrawItem it; it.mesh = fishMesh.get(); it.flags = LIT_INSTANCED; it.castShadow = false; it.tint = vec4(1, 1, 1, 0);
    it.instances = (int)fishInst.size();
    renderer.submit(it);
}

}  // namespace lk
