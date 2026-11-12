#include "Game.h"
#include "../engine/core/Platform.h"
#include <algorithm>

namespace lk {

// ============================================================================================ camera
void Game::updateCamera(float dt) {
    vec3 target;
    if (state == GameState::Playing && player != NullEntity) {
        Character& p = pc();
        vec3 lead = vec3(p.vel.x, 0, p.vel.z) * 0.25f;
        target = p.pos + vec3(0, 1.1f, 0) + lead;
        // lock-on: frame both the player and the target
        if (lockTarget != NullEntity && reg.valid(lockTarget)) {
            Character* t = reg.get<Character>(lockTarget);
            if (t && !t->dead) target = lerp(target, t->pos + vec3(0, 1.f, 0), 0.3f);
        }
        if (bossEnt != NullEntity && reg.valid(bossEnt)) {
            Character* b = reg.get<Character>(bossEnt);
            if (b && !b->dead) { target = lerp(target, b->pos, 0.25f); camDistTarget = std::max(camDistTarget, 24.f + b->vis.scale * 1.5f); }
        }
        camYaw += input.camDrag.x * 0.006f * settings.camSensitivity * (settings.invertCamera ? -1.f : 1.f);
        camDistTarget = clampf(camDistTarget - input.pinch * 0.04f, 9.f, 32.f);
        if (bossEnt == NullEntity && input.pinch == 0) camDistTarget = damp(camDistTarget, dungeon.active ? 14.f : 15.5f, 0.3f, dt);
    } else {
        // title / credits: slow cinematic fly-over above Greenhaven
        titleCamT += dt;
        float a = titleCamT * 0.035f;
        vec2 c{1480.f + std::cos(a) * 60.f, 1600.f + std::sin(a) * 60.f};
        target = vec3(c.x, world.height(c.x, c.y) + 4.f, c.y);
        camYaw = a + 2.2f;
        camDistTarget = 36.f;
    }
    camFocus = damp(camFocus, target, state == GameState::Playing ? 7.f : 1.5f, dt);
    if (length2(camFocus - target) > 60 * 60) camFocus = target;  // teleports
    camDist = damp(camDist, camDistTarget, 4.f, dt);
    float pitch = camPitch - (camDist - 15.5f) * 0.004f;
    vec3 off = vec3(std::sin(camYaw) * std::cos(pitch), std::sin(pitch), std::cos(camYaw) * std::cos(pitch)) * camDist;
    vec3 eye = camFocus + off;
    vec3 look = camFocus + vec3(0, state == GameState::Playing ? 0.9f : 0.f, 0);  // aim at the chest: more landscape in view
    if (cinematicT > 0) {
        float u = 1.f - cinematicT / 4.f;
        eye = lerp(cineFrom, cineTo, easeInOut(saturate(u)));
        look = cineLook;
    }
    // keep the camera above ground and out of mountains
    if (!dungeon.active) {
        float gy = world.height(eye.x, eye.z);
        if (eye.y < gy + 3.f) eye.y = gy + 3.f;
    }
    if (shake > 0) {
        Rng r((uint64_t)(realTime * 977));
        eye += r.onSphere() * shake * 0.35f;
        shake = std::max(0.f, shake - dt * 2.5f);
    }
    cam.pos = eye; cam.target = look;
    cam.fovY = (dungeon.active ? 40.f : 38.f) * DEG;
    cam.zNear = 0.8f; cam.zFar = 420.f;
    cam.update((float)W / std::max(1, H));
    dofFocus = length(look - eye);
}

vec2 Game::worldToScreen(vec3 p, bool* on) const {
    vec4 c = cam.viewProj * vec4(p, 1.f);
    if (on) *on = c.w > 0.1f;
    if (c.w <= 0.001f) return {-1000, -1000};
    vec2 n{c.x / c.w, c.y / c.w};
    return {(n.x * 0.5f + 0.5f) * ui.W, (1.f - (n.y * 0.5f + 0.5f)) * ui.H};
}

// ============================================================================================ world
void Game::renderWorld(float dt) {
    updateCamera(dt);
    renderer.begin(cam, env);
    vec3 focus = camFocus;
    const GraphicsSettings& gs = renderer.settings;
    // stream the world around the focus point (not while inside a dungeon)
    if (!dungeon.active || focus.x < WORLD_SIZE) world.update(focus, gs.drawDistance, state == GameState::Playing ? 3 : 6, realTime);
    std::vector<std::pair<float, PointLight>> lights;
    if (!dungeon.active || focus.x < WORLD_SIZE) {
        world.forEachVisibleChunk([&](Chunk& ch) {
            if (!cam.frustum.visible(ch.bounds)) return;
            float d = distXZ(ch.bounds.center(), focus);
            if (d > gs.drawDistance + CHUNK) return;
            DrawItem it; it.tint = vec4(1, 1, 1, 0);
            if (ch.terrain.valid()) { it.mesh = &ch.terrain; it.flags = LIT_TERRAIN; it.castShadow = d < 70.f; renderer.submit(it); }
            if (ch.props.valid()) { it.mesh = &ch.props; it.flags = 0; it.tint.w = ch.snow * 0.55f; it.castShadow = true; renderer.submit(it); }
            if (ch.foliage.valid()) { it.mesh = &ch.foliage; it.flags = LIT_WIND; it.tint.w = ch.snow * 0.5f; renderer.submit(it); }
            if (d < 48.f && gs.grassDensity > 0.01f) {
                it.castShadow = false; it.tint.w = 0;
                if (ch.grass.valid()) { it.mesh = &ch.grass; it.flags = LIT_INSTANCED | LIT_WIND | LIT_GRASS; renderer.submit(it); }
                if (ch.flowers.valid()) { it.mesh = &ch.flowers; it.flags = LIT_INSTANCED | LIT_WIND | LIT_GRASS; renderer.submit(it); }
            }
            if (ch.water.valid()) {
                vec3 deep = srgb(0x164a58), shallow = srgb(0x4a9488);
                if (ch.region == R_SHADOWS) { deep = srgb(0x14200e); shallow = srgb(0x3a4a26); }
                else if (ch.region == R_ELDERWOOD) { deep = srgb(0x0c3a3a); shallow = srgb(0x2fa090); }
                else if (ch.region == R_FROSTHEIM) { deep = srgb(0x1a3a5a); shallow = srgb(0x6ab0d0); }
                renderer.submitWater({&ch.water, mat4(), deep, shallow, 0.f, false});
            }
            if (ch.lava.valid()) renderer.submitWater({&ch.lava, mat4(), vec3(0), vec3(0), 0.f, true});
            // lights
            for (const vec4& l : ch.lights) {
                int kind = (int)l.w;
                if (kind == LI_LANTERN && env.night < 0.3f) continue;
                vec3 p{l.x, l.y, l.z};
                float dd = length2(p - focus);
                if (dd > 45 * 45) continue;
                PointLight pl;
                pl.pos = p;
                switch (kind) {
                case LI_LANTERN: pl.color = srgb(0xffb060) * 4.f * env.night; pl.radius = 9; pl.flicker = 0.5f; break;
                case LI_FIRE: pl.color = srgb(0xff8a3a) * 6.f; pl.radius = 10; pl.flicker = 1.f; break;
                case LI_LAVA: pl.color = srgb(0xff5a1a) * 5.f; pl.radius = 12; pl.flicker = 0.3f; break;
                case LI_MAGIC: pl.color = srgb(0x6ff0ff) * 3.f; pl.radius = 8; pl.flicker = 0.2f; break;
                default: pl.color = srgb(0xb06cff) * 2.5f; pl.radius = 6; pl.flicker = 0.2f; break;
                }
                lights.push_back({dd, pl});
            }
            // animated windmills
            for (const vec4& e : ch.emitters) {
                if ((int)e.w != EM_WINDMILL || !windmillBlades) continue;
                float rot = (e.w - EM_WINDMILL) * 1000.f;
                DrawItem w; w.mesh = windmillBlades.get();
                w.model = mat4::trs({e.x, e.y, e.z}, quat::axisAngle({0, 1, 0}, rot) * quat::axisAngle({0, 0, 1}, realTime * 0.7f), vec3(1));
                renderer.submit(w);
            }
        });
    }
    if (dungeon.active) {
        renderDungeon();
        for (const vec4& l : dungeon.lights) {
            vec3 p{l.x, l.y, l.z};
            float dd = length2(p - focus);
            if (dd > 40 * 40) continue;
            PointLight pl; pl.pos = p; pl.radius = 11; pl.flicker = 1;
            pl.color = (int)l.w == LI_MAGIC ? srgb(0x9a6aff) * 3.5f : srgb(0xff9a4a) * 5.5f;
            lights.push_back({dd, pl});
        }
    }
    // characters
    reg.each<Character>([&](Entity e, Character& c) { renderCharacter(e, c); });
    // spell / weapon lights from projectiles
    reg.each<Projectile>([&](Entity, Projectile& p) {
        if (p.kind == ProjKind::Arrow || p.kind == ProjKind::Boulder) return;
        PointLight pl; pl.pos = p.pos; pl.radius = 6; pl.flicker = 0.2f; pl.color = elementColor(p.elem) * 4.f;
        lights.push_back({length2(p.pos - focus) * 0.5f, pl});
        if (p.kind == ProjKind::Boulder && boulderMesh) {
            DrawItem d; d.mesh = boulderMesh.get(); d.model = mat4::trs(p.pos, quat::euler(realTime * 3, realTime * 2, 0), vec3(p.radius)); renderer.submit(d);
        }
    });
    reg.each<Projectile>([&](Entity, Projectile& p) {
        if (p.kind == ProjKind::Arrow && arrowMesh) {
            DrawItem d; d.mesh = arrowMesh.get(); d.castShadow = false;
            vec3 dir = normalize(p.vel);
            d.model = mat4::trs(p.pos, fromTo({0, 0, 1}, dir), vec3(1));
            renderer.submit(d);
        } else if (p.kind == ProjKind::Boulder && boulderMesh) {
            DrawItem d; d.mesh = boulderMesh.get(); d.model = mat4::trs(p.pos, quat::euler(realTime * 3, realTime * 2, 0), vec3(p.radius * 1.2f)); renderer.submit(d);
        }
    });
    // pickups (loot bags glow with their rarity colour)
    reg.each<Pickup>([&](Entity e, Pickup& pk) {
        vec3* pos = &pk.pos;
        if (!pos || !pickupMesh) return;
        DrawItem d; d.mesh = pickupMesh.get();
        float bob = std::sin(realTime * 3.f + e) * 0.08f;
        d.model = mat4::trs(*pos + vec3(0, 0.25f + bob, 0), quat::axisAngle({0, 1, 0}, realTime * 1.5f), vec3(1));
        vec3 rc = rarityColor(pk.rarity);
        d.tint = vec4(rc, 0); d.fx.w = 1.5f; d.outline = true; d.outlineColor = vec4(rc * 0.6f, 1); d.outlineWidth = 0.02f;
        renderer.submit(d);
        if ((int)pk.rarity >= (int)Rarity::Rare && particles.rng.f() < dt * 5) particles.magicTrail(*pos + vec3(0, 0.4f, 0), rc, 0.6f);
        PointLight pl; pl.pos = *pos + vec3(0, 0.6f, 0); pl.radius = 3; pl.color = srgb(0xffffff) * rc * 1.5f; pl.flicker = 0;
        lights.push_back({length2(*pos - focus) + 50.f, pl});
    });
    // harvest nodes & chests
    reg.each<NodeComp>([&](Entity e, NodeComp& n) {
        if (!n.mesh) return;
        if (!world.nodeAvailable(n.node.uid, gameTime) && n.node.type != NodeType::Chest) return;
        DrawItem d; d.mesh = n.mesh.get();
        float sh = n.shake > 0 ? std::sin(realTime * 60.f) * n.shake * 0.08f : 0.f;
        n.shake = std::max(0.f, n.shake - dt * 3.f);
        bool open = n.node.type == NodeType::Chest && openedChests.count(n.node.uid);
        if (open && chestOpenMesh) d.mesh = chestOpenMesh.get();
        d.model = mat4::trs(n.node.pos + vec3(sh, 0, 0), quat::axisAngle({0, 1, 0}, n.node.rot), vec3(1));
        d.outline = e == interactTarget; d.outlineColor = vec4(1.f, 0.85f, 0.35f, 1); d.outlineWidth = 0.035f;
        d.fx.w = e == interactTarget ? 0.6f + 0.4f * std::sin(realTime * 6) : 0.f;
        renderer.submit(d);
        if (n.node.type == NodeType::Chest && !open && particles.rng.f() < dt * 2) particles.magicTrail(n.node.pos + vec3(0, 0.8f, 0), vec3(1, 0.85f, 0.4f), 0.5f);
    });
    // nearest lights win (renderer budget)
    std::sort(lights.begin(), lights.end(), [](auto& a, auto& b) { return a.first < b.first; });
    for (size_t i = 0; i < lights.size() && i < 8; i++) renderer.addLight(lights[i].second);
    env.grassPushRadius = 1.3f;
    renderer.env = env;
    renderer.render(&particles);
}

// ============================================================================================ characters
static void buildCapeMesh(Visual& v) {
    const VerletCloth& c = v.cloth;
    if (c.p.empty()) return;
    std::vector<Vertex> verts;
    std::vector<uint32_t> idx;
    Color8 col = toColor8(v.capeColor, 1);
    Color8 trim = toColor8(v.capeColor * 0.55f + vec3(0.25f, 0.2f, 0.1f), 1);
    for (int pass = 0; pass < 2; pass++) {
        uint32_t base = (uint32_t)verts.size();
        for (int y = 0; y < c.rows; y++)
            for (int x = 0; x < c.cols; x++) {
                vec3 p = c.at(x, y);
                vec3 dx = c.at(std::min(x + 1, c.cols - 1), y) - c.at(std::max(x - 1, 0), y);
                vec3 dy = c.at(x, std::min(y + 1, c.rows - 1)) - c.at(x, std::max(y - 1, 0));
                vec3 n = normalize(cross(dy, dx)) * (pass ? -1.f : 1.f);
                Vertex vv{};
                vv.px = p.x; vv.py = p.y; vv.pz = p.z;
                vv.nx = (int8_t)(n.x * 127); vv.ny = (int8_t)(n.y * 127); vv.nz = (int8_t)(n.z * 127);
                Color8 cc = (y == c.rows - 1) ? trim : col;
                vv.r = cc.r; vv.g = cc.g; vv.b = cc.b; vv.a = (uint8_t)(255 * (0.6f + 0.4f * (1.f - (float)y / c.rows)));
                vv.rough = 220; vv.metal = 0;
                verts.push_back(vv);
            }
        for (int y = 0; y + 1 < c.rows; y++)
            for (int x = 0; x + 1 < c.cols; x++) {
                uint32_t a = base + y * c.cols + x, b = a + 1, d = a + c.cols, e = d + 1;
                if (pass == 0) { idx.insert(idx.end(), {a, d, e, a, e, b}); }
                else { idx.insert(idx.end(), {a, e, d, a, b, e}); }
            }
    }
    if (!v.capeMesh) { v.capeMesh = std::make_shared<Mesh>(); v.capeMesh->upload(verts, idx, true); }
    else v.capeMesh->updateVertices(verts);
}

void Game::renderCharacter(Entity e, Character& c) {
    Visual& v = c.vis;
    if (!v.visible || !v.mesh) return;
    if (c.npcDef >= 0) { NpcBrain* nb = reg.get<NpcBrain>(e); if (nb && nb->indoors) return; }
    if (distXZ(c.pos, camFocus) > renderer.settings.drawDistance * 0.75f) return;
    mat4 model = mat4::trs(c.pos + vec3(0, v.pelvisAdj * 0.f, 0), quat::axisAngle({0, 1, 0}, c.yaw), vec3(v.scale));
    // frustum test with a generous sphere
    if (!cam.frustum.visibleSphere(c.pos + vec3(0, c.height * 0.5f, 0), c.height * 1.2f + 2.f)) return;

    // ---- foot IK: plant feet on uneven ground (humanoids walking / idle)
    const Skeleton& sk = *v.anim.skeleton;
    computeWorld(sk, v.anim.pose, v.pw);
    if (v.arch == Arch::Humanoid && v.footIK && c.grounded && !c.swimming && !c.dead && v.anim.state == BodyState::Locomotion && distXZ(c.pos, camFocus) < 30.f) {
        mat4 inv = model.inverse();
        float offs[2]; vec3 targets[2];
        int feet[2] = {H_FOOTL, H_FOOTR};
        for (int i = 0; i < 2; i++) {
            vec3 fw = model.transformPoint(v.pw.wpos[feet[i]]);
            float g = groundAt(fw.x, fw.z);
            offs[i] = clampf(g - c.pos.y, -0.45f, 0.45f);
            targets[i] = fw + vec3(0, offs[i], 0);
        }
        float pelvis = std::min(0.f, std::min(offs[0], offs[1]));
        v.pelvisAdj = damp(v.pelvisAdj, pelvis, 12.f, 1.f / 60.f);
        Pose p = v.anim.pose;
        p.rootOffset.y += v.pelvisAdj / v.scale;
        computeWorld(sk, p, v.pw);
        twoBoneIK(sk, p, v.pw, H_THIGHL, H_SHINL, H_FOOTL, inv.transformPoint(targets[0]), model.inverse().transformDir(vec3(std::sin(c.yaw), 0, std::cos(c.yaw))) + vec3(0, 0, 0.5f), 0.9f);
        twoBoneIK(sk, p, v.pw, H_THIGHR, H_SHINR, H_FOOTR, inv.transformPoint(targets[1]), vec3(0, 0, 1), 0.9f);
        // look at the interaction / lock target
        Entity look = (e == player) ? (lockTarget != NullEntity ? lockTarget : interactTarget) : NullEntity;
        if (c.npcDef >= 0 && player != NullEntity && dist2XZ(c.pos, pc().pos) < 6 * 6) look = player;
        if (look != NullEntity && reg.valid(look)) {
            Character* lc = reg.get<Character>(look);
            vec3 tp = lc ? lc->pos + vec3(0, lc->height * 0.9f, 0) : vec3(0);
            if (!lc) { NodeComp* n = reg.get<NodeComp>(look); if (n) tp = n->node.pos + vec3(0, 0.5f, 0); }
            lookAtIK(sk, p, v.pw, H_HEAD, inv.transformPoint(tp), {0, 0, 1}, 0.9f, 0.8f);
        }
    }
    DrawItem d;
    d.mesh = v.mesh.get(); d.model = model; d.flags = LIT_SKINNED | (v.dissolve > 0 ? LIT_DISSOLVE : 0) | (v.albedo ? LIT_TEXTURED : 0);
    d.albedo = v.albedo;
    d.bones = v.pw.skin[0].m; d.boneCount = sk.size();
    d.tint = vec4(v.tint, 0); d.fx = vec4(v.flash, v.dissolve, v.rim, v.emissiveBoost); d.dissolveColor = v.dissolveColor;
    d.outline = v.outline && v.dissolve <= 0; d.outlineColor = v.outlineColor; d.outlineWidth = 0.028f;
    if (e == interactTarget && c.npcDef >= 0) { d.outlineColor = vec4(1.f, 0.85f, 0.35f, 1); d.outlineWidth = 0.04f; }
    if (e == lockTarget) { d.outlineColor = vec4(0.9f, 0.2f, 0.15f, 1); d.outlineWidth = 0.04f; }
    renderer.submit(d);
    // weapons
    auto attach = [&](std::shared_ptr<Mesh>& m, int bone, bool shield) {
        if (!m) return;
        DrawItem w; w.mesh = m.get();
        mat4 bm = v.pw.world[bone];
        w.model = model * bm * (shield ? mat4::rotate(quat::axisAngle({0, 1, 0}, 0)) : mat4());
        w.fx = vec4(v.flash, v.dissolve, 0.2f, 0); w.flags = v.dissolve > 0 ? LIT_DISSOLVE : 0; w.dissolveColor = v.dissolveColor;
        w.outline = d.outline; w.outlineColor = d.outlineColor; w.outlineWidth = 0.02f;
        renderer.submit(w);
    };
    if (v.arch == Arch::Humanoid) {
        bool bow = v.wt == WeaponType::Bow || v.wt == WeaponType::Crossbow;
        attach(v.weapon, bow ? H_PROPL : H_PROPR, false);
        attach(v.offhand, H_PROPL, true);
    }
    // cape (verlet cloth pinned to the shoulders)
    if (v.cape && v.arch == Arch::Humanoid) {
        int cols = v.cloth.cols;
        if (cols == 0) {
            vec3 sl = model.transformPoint(v.pw.wpos[H_SHL] + vec3(0.04f, 0.02f, -0.12f)), sr = model.transformPoint(v.pw.wpos[H_SHR] + vec3(-0.04f, 0.02f, -0.12f));
            v.cloth.init(5, 7, length(sl - sr), 1.05f * v.scale, sr, normalize(sl - sr), {0, -1, 0});
            v.cloth.damping = 0.04f;
            cols = 5;
        }
        std::vector<vec3> pins(cols);
        vec3 sl = model.transformPoint(v.pw.wpos[H_SHL] + vec3(0.05f, 0.03f, -0.11f)), sr = model.transformPoint(v.pw.wpos[H_SHR] + vec3(-0.05f, 0.03f, -0.11f));
        for (int i = 0; i < cols; i++) pins[i] = lerp(sr, sl, (float)i / (cols - 1));
        vec3 back = model.transformDir({0, 0, -1});
        vec3 chest = model.transformPoint(v.pw.wpos[H_CHEST]);
        std::vector<vec4> spheres = {vec4(model.transformPoint(v.pw.wpos[H_SPINE]), 0.2f * v.scale), vec4(chest, 0.22f * v.scale),
                                     vec4(model.transformPoint(v.pw.wpos[H_THIGHL]), 0.13f * v.scale), vec4(model.transformPoint(v.pw.wpos[H_THIGHR]), 0.13f * v.scale)};
        vec3 pn = normalize(-back);
        float pd = -dot(pn, chest - back * 0.1f * v.scale);
        vec3 wind = vec3(env.windDir.x, 0, env.windDir.y) * env.windStrength * 6.f - c.vel * 1.5f;
        float dt = 1.f / 60.f;
        v.cloth.simulate(dt, pins, {0, -9.81f, 0}, wind, spheres, -pn, -pd, 3);
        buildCapeMesh(v);
        DrawItem cd; cd.mesh = v.capeMesh.get(); cd.flags = 0; cd.fx = vec4(v.flash, 0, 0.2f, 0);
        renderer.submit(cd);
    }
    // weapon trail ribbon
    if (v.trail.size() >= 2) {
        std::vector<Vertex> tv; std::vector<uint32_t> ti;
        int n = (int)v.trail.size();
        Color8 tc = toColor8(v.trailColor, 1);
        for (int i = 0; i < n; i++) {
            float t = (float)i / (n - 1);
            for (int s = 0; s < 2; s++) {
                vec3 p = s ? v.trail[i].second : v.trail[i].first;
                Vertex vv{}; vv.px = p.x; vv.py = p.y; vv.pz = p.z; vv.r = tc.r; vv.g = tc.g; vv.b = tc.b; vv.a = (uint8_t)(255 * t);
                vv.rough = (uint8_t)(255 * t); vv.metal = (uint8_t)(s * 255);
                tv.push_back(vv);
            }
            if (i + 1 < n) { uint32_t a = i * 2; ti.insert(ti.end(), {a, a + 2, a + 3, a, a + 3, a + 1}); }
        }
        if (!v.trailMesh) v.trailMesh = std::make_shared<Mesh>();
        v.trailMesh->upload(tv, ti, true);
        renderer.submitRibbon(v.trailMesh.get(), 2.2f);
    }
}

}  // namespace lk
