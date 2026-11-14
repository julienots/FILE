#include "Game.h"
#include "../engine/core/Platform.h"
#include <algorithm>
#include <chrono>

namespace lk {

// ============================================================================================ camera
void Game::updateCamera(float dt) {
    vec3 target;
    bool playing = state == GameState::Playing && player != NullEntity;
    float distMin = CameraRig::DIST_MIN;
    if (playing) {
        Character& p = pc();
        // "Caméra souple" (option): leads the hero a little in his running direction. Default: locked on the hero.
        vec3 lead = settings.cameraSmooth ? vec3(p.vel.x, 0, p.vel.z) * 0.25f : vec3(0);
        target = p.pos + vec3(0, 1.1f, 0) + lead;
        // ---- player input: drag = yaw / pitch, pinch = zoom (multiplicative: same feel near and far)
        bool touched = input.camDrag.x != 0 || input.camDrag.y != 0 || input.pinch != 0;
        camIdleT = touched ? 0.f : camIdleT + dt;
        float sens = settings.camSensitivity;
        camYaw += input.camDrag.x * 0.006f * sens * (settings.invertCamera ? -1.f : 1.f);
        camPitch = CameraRig::clampPitch(camPitch + input.camDrag.y * 0.005f * sens * (settings.invertCameraY ? -1.f : 1.f));
        if (input.pinch != 0) camDistTarget = CameraRig::clampDist(camDistTarget * std::exp(-input.pinch * 0.0035f));
        if (input.camReset) { camResetT = 0.8f; play("ui_click", 0.3f); }
        if (touched && !input.camReset) camResetT = 0;
        if (camResetT > 0) {
            camResetT -= dt;
            camPitch = damp(camPitch, CameraRig::PITCH_DEFAULT, 7.f, dt);
            camDistTarget = damp(camDistTarget, CameraRig::DIST_DEFAULT, 7.f, dt);
        }
        float hs = length(vec2(p.vel.x, p.vel.z));
        bool locked = false;
        // lock-on: frame both the hero and the target (wider shot). The camera never turns by itself (1.6.1).
        if (lockTarget != NullEntity && reg.valid(lockTarget)) {
            Character* t = reg.get<Character>(lockTarget);
            if (t && !t->dead) {
                locked = true;
                target = lerp(target, t->pos + vec3(0, 1.f, 0), 0.3f);
                float sep = distXZ(p.pos, t->pos);
                distMin = std::max(distMin, std::min(CameraRig::DIST_MAX, sep * 0.9f + 7.f + t->height));
            }
        }
        if (bossEnt != NullEntity && reg.valid(bossEnt)) {
            Character* b = reg.get<Character>(bossEnt);
            if (b && !b->dead) { target = lerp(target, b->pos, 0.25f); distMin = std::max(distMin, std::min(CameraRig::DIST_MAX, 24.f + b->vis.scale * 1.5f)); }
        }
        // automatic return: only from extreme angles (sky / top view), after 6 s without touching the camera
        // while travelling; normal angles chosen by the player are kept
        if (camIdleT > 6.f && hs > 1.f && !locked && camResetT <= 0 && (camPitch < 0.3f || camPitch > 1.05f))
            camPitch = damp(camPitch, clampf(camPitch, 0.3f, 1.05f), 0.6f, dt);
        // look ahead: only when sprinting, slowly (zig-zags must not swing the camera)
        float runK = smoothstepf(5.5f, 7.f, hs) * (p.attackClip ? 0.f : 1.f);
        vec3 ahead = hs > 0.1f ? vec3(p.vel.x / hs, 0, p.vel.z / hs) * 1.5f * runK : vec3(0);
        camLookAhead = damp(camLookAhead, settings.cameraSmooth ? ahead : vec3(0), 1.2f, dt);
    } else {
        // title / credits: slow cinematic fly-over above Greenhaven
        titleCamT += dt;
        float a = titleCamT * 0.035f;
        vec2 c{1480.f + std::cos(a) * 60.f, 1600.f + std::sin(a) * 60.f};
        target = vec3(c.x, world.height(c.x, c.y) + 4.f, c.y);
        camYaw = a + 2.2f;
        camDistTarget = 36.f;
        camPitch = CameraRig::PITCH_DEFAULT;
        camLookAhead = vec3(0);
    }
    // 1.6.2: the camera follows the hero tightly by default (players found the floaty follow "bizarre")
    // story cinematics drive camFocus themselves (the world streams around what they film)
    if (!cine.active) {
        camFocus = damp(camFocus, target, state == GameState::Playing ? (settings.cameraSmooth ? 7.f : 22.f) : 1.5f, dt);
        if (length2(camFocus - target) > 60 * 60) camFocus = target;  // teleports
    }
    // dungeons are enclosed: beyond ~16 m the camera would see outside the level (the player's zoom is kept
    // for the overworld)
    float distCap = dungeon.active ? 16.f : CameraRig::DIST_MAX;
    camDist = damp(camDist, std::min(distCap, std::max(camDistTarget, distMin)), settings.cameraSmooth ? 4.f : 10.f, dt);

    CameraRigIn ri;
    ri.focus = camFocus; ri.lookAhead = camLookAhead;
    // dungeons (narrow 4 m corridors): the camera stays above the walls as in 1.5 and the see-through shader
    // fades them; no low "sky" angle there (walls would fill the view)
    ri.yaw = camYaw; ri.pitch = dungeon.active ? std::max(camPitch, 0.35f) : camPitch; ri.dist = camDist;
    ri.playing = state == GameState::Playing;
    ri.fovY = (dungeon.active ? 40.f : 38.f) * DEG;
    ri.ground = [this](float x, float z) {
        if (dungeon.active) { float y; return dungeonFloor(x, z, y) ? y : camFocus.y - 1.1f; }
        return world.height(x, z);
    };
    if (playing && !dungeon.active) ri.blocked = [this](vec3 p) { return cameraBlocked(p); };
    // smoothing state (collision distance, terrain lift); reset on teleports and outside gameplay
    bool jump = length2(camFocus - camRigFocusPrev) > 15.f * 15.f;
    camRigFocusPrev = camFocus;
    ri.dt = (playing && !jump) ? dt : 0.f;
    ri.prevCollisionDist = camRig.collisionDist; ri.prevLift = camRig.lift;
    ri.smooth = settings.cameraSmooth ? 1.f : 2.5f;   // tight mode: terrain lift / wall back-out settle 2.5x faster
    camRig = CameraRig::compute(ri);
    vec3 eye = camRig.eye, look = camRig.look;
    float cineFov = 0;
    if (cine.active) {
        vec3 ce, cl;
        if (cinematicCamera(ce, cl, cineFov)) { eye = ce; look = cl; cineLastEye = eye; cineLastLook = look; camCineBlend = 0; }
    } else if (cinematicT > 0) {
        float u = 1.f - cinematicT / 4.f;
        eye = lerp(cineFrom, cineTo, easeInOut(saturate(u)));
        look = cineLook;
        if (!dungeon.active) eye.y = std::max(eye.y, world.height(eye.x, eye.z) + 0.6f);
        cineLastEye = eye; cineLastLook = look; camCineBlend = 1.f;
    } else if (camCineBlend > 0) {
        // glide from the last cinematic shot back to the player's camera (no hard cut)
        float k = easeInOut(camCineBlend);
        eye = lerp(eye, cineLastEye, k); look = lerp(look, cineLastLook, k);
        camCineBlend = std::max(0.f, camCineBlend - dt / 0.7f);
    }
    if (shake > 0) {
        Rng r((uint64_t)(realTime * 977));
        eye += r.onSphere() * shake * 0.35f;
        shake = std::max(0.f, shake - dt * 2.5f);
    }
    cam.pos = eye; cam.target = look;
    cam.fovY = cineFov > 0 ? cineFov : (dungeon.active ? 40.f : 38.f) * DEG;
    cam.zNear = std::min(0.8f, std::max(0.3f, camRig.dist * 0.12f));
    cam.zFar = dungeon.active ? 420.f : 1500.f;   // the horizon relief reaches the whole island (fog hides the rest)
    cam.update((float)W / std::max(1, H));
    dofFocus = length(look - eye);
}

// Grass benders (1.6.1): the hero, the trail he leaves (grass stays down ~0.7 s then springs back with a small
// wobble), and the nearest characters (enemies, animals, villagers, the mount).
void Game::updateGrassPushers() {
    for (auto& pv : env.pushers) pv = vec4(0, 0, 0, 0);
    if (state != GameState::Playing || player == NullEntity || dungeon.active) return;
    int n = 0;
    auto add = [&](vec3 p, float st, float r) { float w = Environment::packPusher(st, r); if (n < 8 && w != 0.f) env.pushers[n++] = vec4(p, w); };
    Character& c = pc();
    float r = riding ? 1.7f : 1.05f;
    add(c.pos, 1.f, r);
    if (c.grounded && distXZ(c.pos, grassTrailLast) > 0.45f) {
        grassTrail.push_front({c.pos, realTime});
        grassTrailLast = c.pos;
        while (grassTrail.size() > 4) grassTrail.pop_back();
    }
    for (auto& t : grassTrail) {
        float age = realTime - t.t;
        float st = age < 0.7f ? 1.f : std::exp(-2.4f * (age - 0.7f)) * std::cos(5.f * (age - 0.7f));
        add(t.p, st * 0.9f, r * 0.9f);
    }
    std::vector<std::pair<float, Character*>> near;
    reg.each<Character>([&](Entity e, Character& o) {
        if (e == player || o.dead) return;
        float d2 = dist2XZ(o.pos, c.pos);
        if (d2 < 25.f * 25.f) near.push_back({d2, &o});
    });
    std::sort(near.begin(), near.end(), [](auto& a, auto& b) { return a.first < b.first; });
    for (auto& o : near) add(o.second->pos, 0.85f, clampf(o.second->radius * 1.7f, 0.6f, 3.f));
}

bool Game::cameraBlocked(vec3 p) const {
    if (dungeon.active && p.x > WORLD_SIZE) {
        // wall colliders are 8 m tall for gameplay, but the visible walls stop at ~4.6 m above the floor: the
        // camera may pass over them (top-down dungeon view, as in 1.5)
        float wallTop = dungeon.origin.y + 4.6f;
        if (p.y > wallTop) return false;
        for (const Collider& c : dungeon.colliders) {
            if (c.hx <= 0) continue;
            vec2 l = rotate(p.xz() - c.c, -c.rot);
            if (std::fabs(l.x) < c.hx && std::fabs(l.y) < c.hz) return true;
        }
        return false;
    }
    return world.cameraBlocked(p, 0.3f);
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
    // horizon relief: upload once the background build is done (and again after a context loss)
    if (!farTerrainData && farTerrainBuild.valid() && farTerrainBuild.wait_for(std::chrono::seconds(0)) == std::future_status::ready) {
        farTerrainData = farTerrainBuild.get();
        AGR_LOG("horizon relief: %zu vertices, %zu triangles", farTerrainData->v.size(), farTerrainData->idx.size() / 3);
    }
    if (farTerrainData && !farTerrain) { farTerrain = std::make_shared<Mesh>(); farTerrainData->upload(*farTerrain); }
    if (farTerrain && !dungeon.active) {
        DrawItem it; it.mesh = farTerrain.get(); it.flags = LIT_FAR; it.castShadow = false;
        it.farClip = vec4(focus.x, focus.z, std::max(40.f, gs.drawDistance - 14.f), 0);
        renderer.submit(it);
    }
    std::vector<std::pair<float, PointLight>> lights;
    if (!dungeon.active || focus.x < WORLD_SIZE) {
        world.forEachVisibleChunk([&](Chunk& ch) {
            if (!cam.frustum.visible(ch.bounds)) return;
            float d = distXZ(ch.bounds.center(), focus);
            if (d > gs.drawDistance + CHUNK) return;
            DrawItem it; it.tint = vec4(1, 1, 1, 0);
            if (ch.terrain.valid()) { it.mesh = &ch.terrain; it.flags = LIT_TERRAIN; it.castShadow = d < 70.f; renderer.submit(it); }
            // shadow casters limited to what the ~43 m shadow map around the hero can actually show
            if (ch.props.valid()) { it.mesh = &ch.props; it.flags = 0; it.tint.w = ch.snow * 0.55f; it.castShadow = d < 44.f; renderer.submit(it); }
            if (ch.foliage.valid()) { it.mesh = &ch.foliage; it.flags = LIT_WIND; it.tint.w = ch.snow * 0.5f; it.castShadow = d < 36.f; renderer.submit(it); }
            if (d < 48.f && gs.grassDensity > 0.01f) {
                it.castShadow = false; it.tint.w = 0;
                if (ch.grass.valid()) { it.mesh = &ch.grass; it.flags = LIT_INSTANCED | LIT_WIND | LIT_GRASS; renderer.submit(it); }
                if (ch.flowers.valid()) { it.mesh = &ch.flowers; it.flags = LIT_INSTANCED | LIT_WIND | LIT_GRASS; renderer.submit(it); }
                if (d < 30.f && ch.grassNear.valid()) { it.mesh = &ch.grassNear; it.flags = LIT_INSTANCED | LIT_WIND | LIT_GRASS; renderer.submit(it); }
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
    updateGrassPushers();
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
    Pose fpose = v.anim.pose;   // final pose (foot IK, look-at, weapon clearance)
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
        Pose& p = fpose;
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
    // ---- weapon clearance (1.6.2): a blade / spear tip never sinks into the ground. If the animated pose puts
    // the tip below the terrain (idle carry, slopes, steps, guards), the wrist lifts it just enough.
    if (v.arch == Arch::Humanoid && v.weapon && !c.dead && !c.swimming && v.anim.state != BodyState::Mounted && v.tip > 0.3f) {
        bool bow = v.wt == WeaponType::Bow || v.wt == WeaponType::Crossbow;
        int pb = bow ? H_PROPL : H_PROPR, hand = bow ? H_HANDL : H_HANDR;
        for (int it = 0; it < 4; it++) {
            vec3 tipM = v.pw.world[pb].transformPoint(vec3(0, 0, v.tip));
            vec3 tipW = model.transformPoint(tipM);
            float g = (groundAt(tipW.x, tipW.z) + 0.05f - c.pos.y) / std::max(0.1f, v.scale);   // model space
            float clear = (tipM.y - g) * v.scale + 0.05f;
            if (it == 0) v.tipClearRaw = clear;
            v.tipClear = clear;
            if (tipM.y >= g || it == 3) break;
            vec3 handM = v.pw.wpos[hand];
            vec3 dir = tipM - handM;
            float L = std::max(0.15f, length(dir));
            dir = dir / L;
            vec3 axis = cross(dir, vec3(0, 1, 0));
            axis = length2(axis) < 1e-4f ? vec3(-1, 0, 0) : normalize(axis);   // straight down: swing the tip forward
            float need = std::asin(clampf((g - tipM.y) / L, 0.f, 1.f)) + 0.04f;
            quat q = quat::axisAngle(axis, need);
            int par = sk.bones[hand].parent;
            quat parentRot = par >= 0 ? v.pw.wrot[par] : quat();
            fpose.rot[hand] = normalize(parentRot.conj() * (q * v.pw.wrot[hand]));
            computeWorld(sk, fpose, v.pw);
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
