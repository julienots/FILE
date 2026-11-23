// LOST KINGDOM — 3.7.0 birds in the sky, in 3D: flocks of swallows that sweep across the meadows, gulls gliding
// along the coasts, and big birds of prey circling slowly over the mountains. Each bird flaps (three wing poses,
// instanced: three draw calls for every bird in the sky), banks its flight a little, and the flock keeps its
// shape loosely. They come by day (and not in storms), cross the view and leave.
#include "Game.h"

namespace lk {

static void buildBird(MeshBuilder& b, int pose) {
    Mat body(vec3(0.95f), 0.6f), dark(vec3(0.6f), 0.6f), beak(hexColor(0xe8b040), 0.5f);
    b.ellipsoid({0, 0, 0}, vec3(0.07f, 0.06f, 0.2f), body, 10, 6);
    b.ellipsoid({0, 0.03f, 0.18f}, vec3(0.05f), body, 8, 5);
    b.cone({0, 0.025f, 0.22f}, {0, 0.02f, 0.3f}, 0.018f, beak, 5);
    b.blade({0, 0.01f, -0.16f}, {0, 0.0f, -0.34f}, 0.16f, {1, 0, 0}, dark, dark);   // tail
    float lift = pose == 0 ? 0.75f : (pose == 1 ? 0.08f : -0.55f);   // wings up / level / down
    for (int s = -1; s <= 1; s += 2) {
        vec3 root{s * 0.05f, 0.02f, 0.02f};
        vec3 elbow = root + vec3(s * 0.26f, 0.26f * lift, -0.02f);
        vec3 tip = elbow + vec3(s * 0.28f, 0.3f * lift - 0.04f, -0.12f);
        b.quadP(root + vec3(0, 0, 0.08f), elbow + vec3(0, 0, 0.06f), elbow + vec3(0, 0, -0.1f), root + vec3(0, 0, -0.1f), body);
        b.quadP(root + vec3(0, 0, -0.1f), elbow + vec3(0, 0, -0.1f), elbow + vec3(0, 0, 0.06f), root + vec3(0, 0, 0.08f), body);
        b.triP(elbow + vec3(0, 0, 0.06f), tip, elbow + vec3(0, 0, -0.1f), dark);
        b.triP(elbow + vec3(0, 0, -0.1f), tip, elbow + vec3(0, 0, 0.06f), dark);
    }
}

void Game::spawnFlock(vec3 around) {
    Rng r((uint64_t)(realTime * 1000.f) + flocks.size() * 7);
    RegionWeights rw = world.regionAt(around.x, around.z);
    bool sea = false;
    for (int k = 0; k < 6 && !sea; k++) { float a = k * TAU / 6; if (waterAt(around.x + std::cos(a) * 60.f, around.z + std::sin(a) * 60.f) > -1e8f && world.height(around.x + std::cos(a) * 60.f, around.z + std::sin(a) * 60.f) < 0.f) sea = true; }
    bool peaks = rw.w[R_TITANS] + rw.w[R_HIGHCROWNS] + rw.w[R_FROSTHEIM] + rw.w[R_CANYON] > 0.5f;
    Flock f;
    f.kind = sea && r.chance(0.7f) ? 1 : (peaks && r.chance(0.6f) ? 2 : 0);
    float gy = world.height(around.x, around.z);
    if (f.kind == 2) {   // birds of prey: one or two, circling high and slow
        f.n = r.chance(0.3f) ? 2 : 1;
        f.anchor = around + vec3(r.range(-30.f, 30.f), 0, r.range(-30.f, 30.f)); f.anchor.y = gy + r.range(28.f, 42.f);
        f.radius = r.range(16.f, 28.f); f.speed = r.range(0.18f, 0.28f) * (r.chance(0.5f) ? 1.f : -1.f); f.life = r.range(28.f, 40.f);
        f.tint = hexColor(0x6a4a2e); f.scale = 2.4f;
    } else {
        vec3 dir = normalize(vec3(r.range(-1, 1), 0, r.range(-1, 1)) + vec3(1e-3f, 0, 0));
        f.n = f.kind == 1 ? r.irange(3, 7) : r.irange(8, 18);
        f.anchor = around - dir * 70.f; f.anchor.y = gy + (f.kind == 1 ? r.range(9.f, 16.f) : r.range(12.f, 22.f));
        f.vel = dir * (f.kind == 1 ? r.range(6.f, 8.f) : r.range(9.f, 12.f)); f.life = 140.f / length(f.vel);
        f.tint = f.kind == 1 ? hexColor(0xf0f0ec) : hexColor(0x2a2a30); f.scale = f.kind == 1 ? 1.6f : 0.95f;
    }
    for (int i = 0; i < f.n; i++) f.offs.push_back(vec4(r.range(-5.f, 5.f), r.range(-1.5f, 1.5f), r.range(-5.f, 5.f), r.range(0, TAU)));
    flocks.push_back(f);
    if (f.kind != 2 && r.chance(0.6f)) play("birds", 0.25f, f.kind == 1 ? 0.8f : r.range(0.95f, 1.1f));
}

void Game::updateBirds(float dt) {
    for (Flock& f : flocks) {
        f.t += dt;
        if (f.kind != 2) f.anchor += f.vel * dt;
    }
    flocks.erase(std::remove_if(flocks.begin(), flocks.end(), [](const Flock& f) { return f.t > f.life; }), flocks.end());
    if (dungeon.active || player == NullEntity || state != GameState::Playing) return;
    flockT -= dt;
    if (flockT <= 0) {
        flockT = particles.rng.range(9.f, 20.f);
        if ((int)flocks.size() < 3 && env.night < 0.3f && env.windStrength < 1.1f) spawnFlock(pc().pos);
    }
}

void Game::renderBirds() {
    if (flocks.empty() || dungeon.active) return;
    if (!birdMesh[0]) for (int p = 0; p < 3; p++) { MeshBuilder b; buildBird(b, p); birdMesh[p] = std::make_shared<Mesh>(); b.upload(*birdMesh[p]); }
    std::vector<InstanceData> inst[3];
    for (const Flock& f : flocks) {
        float fade = std::min(1.f, std::min(f.t, f.life - f.t) / 2.f);   // they fly in and out (no pop): scale up / down
        if (fade <= 0.01f) continue;
        for (int i = 0; i < f.n; i++) {
            const vec4& o = f.offs[i];
            vec3 p; float yaw;
            if (f.kind == 2) {   // circling on the thermals
                float a = f.t * f.speed + o.w;
                p = f.anchor + vec3(std::cos(a) * f.radius, std::sin(f.t * 0.3f + o.w) * 2.f, std::sin(a) * f.radius) + vec3(o.x, 0, o.z) * 0.5f * (float)i;
                vec2 tang = vec2(-std::sin(a), std::cos(a)) * (f.speed > 0 ? 1.f : -1.f);
                yaw = std::atan2(tang.x, tang.y);
            } else {
                vec3 wob{std::sin(f.t * 1.3f + o.w) * 1.2f, std::sin(f.t * 2.1f + o.w * 1.7f) * 0.6f, std::cos(f.t * 1.1f + o.w) * 1.2f};
                p = f.anchor + vec3(o.x, o.y, o.z) + wob;
                yaw = std::atan2(f.vel.x, f.vel.z) + std::sin(f.t * 1.3f + o.w) * 0.15f;
            }
            if (distXZ(p, camFocus) > 160.f) continue;
            // flapping: swallows beat fast, gulls slower with glides, raptors mostly soar
            float rate = f.kind == 0 ? 9.f : (f.kind == 1 ? 4.5f : 2.2f);
            float glide = f.kind == 0 ? 0.25f : (f.kind == 1 ? 0.55f : 0.85f);
            float cyc = std::fmod(f.t * 0.35f + o.w, 1.f);
            int pose = cyc < glide ? 1 : ((int)std::floor(f.t * rate + o.w * 3.f) % 3);
            InstanceData d; d.x = p.x; d.y = p.y; d.z = p.z; d.rotY = yaw; d.scale = f.scale * fade;
            Color8 c = toColor8(f.tint * 0.5f, 1); d.r = c.r; d.g = c.g; d.b = c.b; d.a = 255;
            inst[pose].push_back(d);
        }
    }
    for (int p = 0; p < 3; p++) {
        if (inst[p].empty()) continue;
        birdMesh[p]->setInstances(inst[p]);
        DrawItem it; it.mesh = birdMesh[p].get(); it.flags = LIT_INSTANCED; it.castShadow = false; it.tint = vec4(1, 1, 1, 0); it.instances = (int)inst[p].size();
        renderer.submit(it);
    }
}

}  // namespace lk
