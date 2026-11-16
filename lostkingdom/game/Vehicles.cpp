// LOST KINGDOM — 1.11.0 drivable vehicles: a car and a motorbike (models from the user's Drive, wheels split by
// tools/import_vehicle.py). Arcade handling: the stick gives the direction to go (camera-relative, like the
// horse); the vehicle accelerates and turns towards it at a rate that depends on its speed. Body pitch and roll
// follow the ground under the wheels; the motorbike leans into turns; the wheels spin with the speed and the
// front wheels steer. The rider sits on the motorbike; in the car the hero is hidden (he is inside).
#include "Game.h"
#include "../engine/core/Platform.h"
#include <algorithm>

namespace lk {

const std::vector<VehicleDef>& vehicleDefs() {
    static const std::vector<VehicleDef> v = {
        // id, name, model, moto, maxSpeed (m/s), accel, turn (rad/s), radius, camDist
        {"car", "Voiture de sport", "car", false, 25.f, 7.5f, 1.5f, 1.4f, 10.f},
        {"moto", "Moto de course", "moto", true, 27.f, 9.f, 2.1f, 0.7f, 8.f},
    };
    return v;
}

Game::VehicleAsset* Game::vehicleAsset(const std::string& model) {
    VehicleAsset& a = vehicleAssets[model];
    if (a.tried) return a.ok ? &a : nullptr;
    a.tried = true;
    std::vector<uint8_t> mb, tb;
    a.ok = platform::readAsset("models/vehicles/" + model + ".lkm", mb) && a.data.loadLKM(mb) && platform::readAsset("models/vehicles/" + model + ".jpg", tb) && a.data.loadTexture(tb) && a.data.joints.size() >= 4;
    if (!a.ok) AGR_WARN("vehicle model '%s' missing or invalid", model.c_str());
    else AGR_LOG("vehicle '%s': %zu triangles, %zu wheels", model.c_str(), a.data.idx.size() / 3, a.data.joints.size() - 2);
    return a.ok ? &a : nullptr;
}

// parked on the road just outside Brindlemere, side by side
void Game::initVehicles() {
    vehicles.clear();
    const Poi* b = world.poiById("brindlemere");
    vec2 base = b ? b->pos + vec2(b->radius + 14.f, -6.f) : vec2(1500, 1600);
    for (int i = 0; i < (int)vehicleDefs().size(); i++) {
        VehicleState v; v.def = i;
        vec2 p = base + vec2(0, i * 5.f);
        v.pos = vec3(p.x, world.groundHeight(p.x, p.y), p.y);
        v.yaw = PI * 0.5f;
        vehicles.push_back(v);
    }
}

void Game::spawnVehicleEntities() {
    for (Entity e : vehicleEnts) if (reg.valid(e)) reg.destroy(e);
    vehicleEnts.clear();
    for (int i = 0; i < (int)vehicles.size(); i++) {
        Entity e = reg.create();
        reg.add<VehicleRef>(e, VehicleRef{i});
        Interact it; it.kind = InterKind::Vehicle; it.param = i; it.radius = 3.6f;
        it.text = vehicleDefs()[vehicles[i].def].moto ? "Monter sur la moto" : "Conduire la voiture";
        reg.add<Interact>(e, it);
        vehicleEnts.push_back(e);
    }
}

vec3 Game::vehicleLocal(const VehicleState& v, vec3 p) const {
    quat q = quat::axisAngle({0, 1, 0}, v.yaw) * quat::axisAngle({1, 0, 0}, -v.pitch) * quat::axisAngle({0, 0, 1}, v.roll);
    return v.pos + q.rotate(p);
}

void Game::enterVehicle(int i) {
    if (i < 0 || i >= (int)vehicles.size() || driving >= 0) return;
    if (riding) toggleMount();
    if (flying || dungeon.active) return;
    driving = i;
    Character& c = pc();
    const VehicleDef& d = vehicleDefs()[vehicles[i].def];
    c.vel = vec3(0);
    if (!d.moto) c.vis.visible = false;   // inside the car
    else { c.vis.anim.setState(BodyState::Mounted, 0.25f); c.vis.anim.aux[3] = 1.f; }
    camDistTarget = std::max(camDistTarget, d.camDist);
    play(d.moto ? "horse" : "door", 0.6f);
    if (!storyFlags.count("hint_vehicle")) { storyFlags.insert("hint_vehicle"); toastHint = "Joystick : la direction où aller. Bouton d'action : descendre."; }
}

void Game::exitVehicle() {
    if (driving < 0) return;
    VehicleState& v = vehicles[driving];
    const VehicleDef& d = vehicleDefs()[v.def];
    Character& c = pc();
    vec3 side = vehicleLocal(v, vec3(d.moto ? 1.1f : 1.9f, 0, 0)) - v.pos;
    c.pos = v.pos + vec3(side.x, 0, side.z);
    c.pos.y = groundAt(c.pos.x, c.pos.z);
    collideStatic(c.pos, c.radius, c.pos.y);
    c.vis.visible = true; c.vis.tiltPitch = c.vis.tiltRoll = 0;
    c.vis.anim.aux[3] = 0; c.vis.anim.setState(BodyState::Locomotion, 0.25f);
    v.speed = 0;
    driving = -1;
    if (sfx.count("engine")) audio.setEngine(sfx["engine"], 0.f, 0.8f);
    play("door", 0.5f, 1.2f);
}

void Game::updateDriving(float dt, vec3 want, float mag) {
    VehicleState& v = vehicles[driving];
    const VehicleDef& d = vehicleDefs()[v.def];
    VehicleAsset* A = vehicleAsset(d.model);
    Character& c = pc();
    if (input.mount || input.jump || c.dead) { exitVehicle(); return; }
    vec3 fwd(std::sin(v.yaw), 0, std::cos(v.yaw));
    // the stick gives where to go: drive towards it, reverse if it points behind while (nearly) stopped
    float target = 0, yawRate = 0;
    if (mag > 0.1f) {
        vec3 w = normalize(vec3(want.x, 0, want.z));
        float ang = wrapAngle(std::atan2(w.x, w.z) - v.yaw);
        bool back = std::fabs(ang) > 2.3f;
        bool reverse = back && v.speed < 3.f;
        if (reverse) { target = -mag * d.maxSpeed * 0.3f; ang = wrapAngle(ang + PI); }
        else if (back) { target = 0; ang = 0; }   // 1.11.1 stick pulled back while moving forward: brake (then reverse)
        else target = mag * d.maxSpeed * (std::fabs(ang) > 1.2f ? 0.45f : 1.f);   // slow down in a hairpin
        float grip = clampf(std::fabs(v.speed) / 5.f, 0.f, 1.f);
        yawRate = clampf(ang * 3.f, -d.turn, d.turn) * grip * (v.speed < 0 ? -1.f : 1.f);
        v.steer = damp(v.steer, clampf(ang, -0.5f, 0.5f) * (v.speed < 0 ? -1.f : 1.f), 8.f, dt);
    } else v.steer = damp(v.steer, 0.f, 6.f, dt);
    float accel = (std::fabs(target) > std::fabs(v.speed) && target * v.speed >= 0) ? d.accel : 9.f;   // engine / brakes
    v.speed = approach(v.speed, target, accel * dt);
    // uphill slows, downhill speeds up a little
    float hAhead = groundAt(v.pos.x + fwd.x * 2.f, v.pos.z + fwd.z * 2.f) - groundAt(v.pos.x, v.pos.z);
    v.speed -= clampf(hAhead / 2.f, -0.5f, 0.8f) * 3.f * dt * (v.speed >= 0 ? 1.f : -1.f);
    float prevYaw = v.yaw;
    v.yaw = wrapAngle(v.yaw + yawRate * dt);
    fwd = vec3(std::sin(v.yaw), 0, std::cos(v.yaw));
    vec3 np = v.pos + fwd * v.speed * dt;
    // water and cliffs stop the vehicle
    float wl = waterAt(np.x, np.z), gy = groundAt(np.x, np.z);
    bool blocked = (wl > -1e8f && gy < wl + 0.25f) || std::fabs(gy - v.pos.y) > 1.2f + std::fabs(v.speed) * dt * 1.5f;
    if (!blocked) {
        vec3 before = np;
        collideStatic(np, d.radius, gy);
        if (length(vec2(np.x - before.x, np.z - before.z)) > 0.02f && std::fabs(v.speed) > 6.f) { addShake(0.25f * std::fabs(v.speed) / d.maxSpeed); play("hit_wood", 0.5f, 0.7f); haptic(40, 160); v.speed *= 0.4f; }
        v.pos.x = np.x; v.pos.z = np.z;
    } else { if (std::fabs(v.speed) > 6.f) { addShake(0.2f); play("hit_wood", 0.4f, 0.7f); } v.speed = 0; }
    // ground under the wheels -> height, pitch, roll
    if (A) {
        const auto& J = A->data.joints;
        auto groundUnder = [&](vec3 local) { vec3 w = v.pos + quat::axisAngle({0, 1, 0}, v.yaw).rotate(vec3(local.x, 0, local.z)); return groundAt(w.x, w.z); };
        float tp = 0, tr = 0, ty;
        if (d.moto) {
            float hf = groundUnder(J[2]), hr = groundUnder(J[3]);
            tp = std::atan2(hf - hr, std::max(0.5f, J[2].z - J[3].z));
            ty = std::min(hf, hr) + std::fabs(hf - hr) * 0.5f;
            tr = clampf(-yawRate * std::fabs(v.speed) * 0.035f, -0.65f, 0.65f);   // leans into the turn
        } else {
            float fl = groundUnder(J[2]), fr = groundUnder(J[3]), rl = groundUnder(J[4]), rr = groundUnder(J[5]);
            float wb = std::max(0.5f, 0.5f * (J[2].z + J[3].z) - 0.5f * (J[4].z + J[5].z)), tw = std::max(0.5f, J[2].x - J[3].x);
            tp = std::atan2(0.5f * (fl + fr) - 0.5f * (rl + rr), wb);
            tr = std::atan2(0.5f * (fl + rl) - 0.5f * (fr + rr), tw) - clampf(yawRate * std::fabs(v.speed) * 0.006f, -0.06f, 0.06f);   // body roll
            ty = 0.25f * (fl + fr + rl + rr);
        }
        v.pitch = damp(v.pitch, tp, 10.f, dt); v.roll = damp(v.roll, tr, 8.f, dt);
        v.pos.y = damp(v.pos.y, ty, 14.f, dt);
        float r = std::max(0.15f, J[2].y);
        v.spin = std::fmod(v.spin + v.speed / r * dt, TAU * 100.f);
    } else v.pos.y = groundAt(v.pos.x, v.pos.z);
    v.yawRate = wrapAngle(v.yaw - prevYaw) / std::max(dt, 1e-4f);
    // the hero: on the seat (moto) or inside (car, hidden)
    vec3 seat = A ? vehicleLocal(v, A->data.joints[1]) : v.pos + vec3(0, 1, 0);
    if (d.moto) {
        float hip = c.vis.anim.skeleton ? c.vis.anim.skeleton->bones[0].offset.y * c.vis.scale : 0.95f;
        quat bq = quat::axisAngle({0, 1, 0}, v.yaw) * quat::axisAngle({1, 0, 0}, -v.pitch) * quat::axisAngle({0, 0, 1}, v.roll);
        c.pos = seat - bq.rotate(vec3(0, hip - 0.04f, 0));   // the pelvis on the seat, tilted with the bike
        c.vis.tiltPitch = v.pitch; c.vis.tiltRoll = v.roll;
        c.vis.anim.aux[1] = saturate(std::fabs(v.speed) / d.maxSpeed);
        c.vis.anim.aux[3] = 1.f;
    } else c.pos = seat;
    c.yaw = v.yaw; c.vel = fwd * v.speed; c.grounded = true;
    c.vis.anim.speed = 0;
    // engine: one looping voice, pitch follows the speed (the moto revs higher)
    float rev = saturate(std::fabs(v.speed) / d.maxSpeed);
    if (sfx.count("engine")) audio.setEngine(sfx["engine"], 0.22f + 0.3f * rev, (d.moto ? 1.25f : 0.9f) * (0.8f + 1.6f * rev));
    if (std::fabs(v.speed) > 8.f && particles.rng.chance(dt * 12.f)) {
        vec3 back = vehicleLocal(v, A ? vec3(A->data.joints.back().x, 0.1f, A->data.joints.back().z) : vec3(0, 0.1f, -1.5f));
        particles.dust(back, 0.5f + 0.5f * saturate(std::fabs(v.speed) / d.maxSpeed));
    }
}

void Game::renderVehicles() {
    for (int i = 0; i < (int)vehicles.size(); i++) {
        VehicleState& v = vehicles[i];
        const VehicleDef& d = vehicleDefs()[v.def];
        VehicleAsset* A = vehicleAsset(d.model);
        if (!A || distXZ(v.pos, camFocus) > renderer.settings.drawDistance) continue;
        if (driving != i) {   // parked: settle on the ground
            v.pos.y = groundAt(v.pos.x, v.pos.z);
        }
        if (!A->mesh || !A->mesh->valid()) { A->mesh = std::make_shared<Mesh>(); A->mesh->upload(A->data.verts, A->data.idx); }
        if (!A->tex.id) A->tex.create2D(A->data.texW, A->data.texH, GL_RGBA8, GL_RGBA, GL_UNSIGNED_BYTE, A->data.rgba.data(), true, false);
        const auto& J = A->data.joints;
        int nb = (int)J.size();
        v.bones.resize(nb);
        for (int b = 0; b < nb; b++) {
            if (b < 2) { v.bones[b] = mat4(); continue; }
            bool front = (d.moto && b == 2) || (!d.moto && b <= 3);
            float steer = front ? v.steer * (d.moto ? 0.6f : 1.f) : 0.f;
            mat4 toC = mat4::trs(J[b], quat(), vec3(1)), fromC = mat4::trs(-J[b], quat(), vec3(1));
            v.bones[b] = toC * mat4::trs(vec3(0), quat::axisAngle({0, 1, 0}, steer) * quat::axisAngle({1, 0, 0}, v.spin), vec3(1)) * fromC;
        }
        quat q = quat::axisAngle({0, 1, 0}, v.yaw) * quat::axisAngle({1, 0, 0}, -v.pitch) * quat::axisAngle({0, 0, 1}, v.roll);
        DrawItem it; it.mesh = A->mesh.get(); it.model = mat4::trs(v.pos, q, vec3(1));
        it.flags = LIT_SKINNED | LIT_TEXTURED; it.albedo = A->tex.id; it.bones = v.bones[0].m; it.boneCount = nb;
        it.castShadow = distXZ(v.pos, camFocus) < 44.f; it.fx = vec4(0, 0, 0.5f, 0);
        renderer.submit(it);
    }
}

}  // namespace lk
