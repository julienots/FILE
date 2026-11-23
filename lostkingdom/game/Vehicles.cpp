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
        {"airship", "Vaisseau volant", "", false, 30.f, 6.f, 1.1f, 3.f, 26.f, true},   // 3.4.0 the sky-ship of the Order
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
        vec2 p = base + vec2(vehicleDefs()[i].fly ? 16.f : 0.f, vehicleDefs()[i].fly ? 2.f : i * 5.f);   // 3.4.0 the sky-ship moored a little apart
        v.pos = vec3(p.x, world.groundHeight(p.x, p.y), p.y);
        v.yaw = PI * 0.5f;
        vehicles.push_back(v);
    }
}

// 3.4.0 the vehicles are earned along the main quest: the moto when the Echo Stone has spoken (m2), the car
// when the letter reaches Highspire (m3), the Order's sky-ship once the Crown is reforged (m5). Older saves get
// what their progress has already earned.
bool Game::vehicleUnlocked(int i) const {
    if (i < 0 || i >= (int)vehicles.size()) return false;
    return vehiclesUnlocked.count(vehicleDefs()[vehicles[i].def].id) > 0;
}

void Game::refreshVehicleUnlocks(bool announce) {
    struct U { const char* quest; const char* id; const char* icon; const char* title; const char* desc; vec3 col; };
    static const U table[] = {
        {"m2", "moto", "i_mount", "Véhicule débloqué", "Brom, le mécanicien de Brindlemere, vous confie sa moto de course. Appelez-la depuis le garage (en haut de l'écran).", vec3(1.f, 0.6f, 0.3f)},
        {"m3", "car", "i_mount", "Véhicule débloqué", "La voiture de sport du bourgmestre est à vous : un remerciement de Greenhaven. Garage, en haut de l'écran.", vec3(0.5f, 0.8f, 1.f)},
        {"m5", "airship", "i_mount", "Vaisseau volant débloqué !", "L'Ordre de Highspire vous prête son vaisseau des nuées. Saut maintenu : monter. Bouclier maintenu : descendre. Action : se poser.", vec3(1.f, 0.85f, 0.4f)},
    };
    for (const U& u : table) {
        if (!questsDone.count(u.quest) || vehiclesUnlocked.count(u.id)) continue;
        vehiclesUnlocked.insert(u.id);
        if (!announce) continue;
        std::string name = u.id;
        for (const VehicleDef& d : vehicleDefs()) if (d.id == u.id) name = d.name;
        itemGet(u.icon, u.title, name, u.desc, u.col, true);
        play("discover", 0.9f, 1.f);
    }
}

void Game::spawnVehicleEntities() {
    for (Entity e : vehicleEnts) if (reg.valid(e)) reg.destroy(e);
    vehicleEnts.clear();
    for (int i = 0; i < (int)vehicles.size(); i++) {
        Entity e = reg.create();
        reg.add<VehicleRef>(e, VehicleRef{i});
        Interact it; it.kind = InterKind::Vehicle; it.param = i; it.radius = vehicleDefs()[vehicles[i].def].fly ? 6.5f : 3.6f;
        it.text = vehicleDefs()[vehicles[i].def].fly ? "Monter à bord du vaisseau" : (vehicleDefs()[vehicles[i].def].moto ? "Monter sur la moto" : "Conduire la voiture");
        reg.add<Interact>(e, it);
        vehicleEnts.push_back(e);
    }
}

vec3 Game::vehicleLocal(const VehicleState& v, vec3 p) const {
    quat q = quat::axisAngle({0, 1, 0}, v.yaw) * quat::axisAngle({1, 0, 0}, -v.pitch) * quat::axisAngle({0, 0, 1}, v.roll);
    return v.pos + q.rotate(p);
}

void Game::enterVehicle(int i) {
    if (i < 0 || i >= (int)vehicles.size() || driving >= 0 || !vehicleUnlocked(i)) return;
    if (riding) toggleMount();
    if (flying || dungeon.active) return;
    driving = i;
    { VehicleState& v = vehicles[i]; v.vy = 0; v.air = false; v.airT = 0; v.velYaw = v.yaw; v.groundPrev = v.pos.y; v.prevSpeed = v.speed; }
    Character& c = pc();
    const VehicleDef& d = vehicleDefs()[vehicles[i].def];
    c.vel = vec3(0);
    if (d.fly) { VehicleState& v = vehicles[i]; v.alt = 0; v.landing = false; c.vis.anim.setState(BodyState::Locomotion, 0.2f); }   // 3.4.0 at the helm, on deck
    else if (!d.moto) c.vis.visible = false;   // inside the car
    else { c.vis.anim.setState(BodyState::Mounted, 0.25f); c.vis.anim.aux[3] = 1.f; }
    camDistTarget = std::max(camDistTarget, d.camDist);
    if (d.fly) camPitch = std::max(camPitch, 0.42f);   // 3.4.0 above the envelope, looking down on the deck
    play(d.fly ? "magic_burst" : (d.moto ? "horse" : "door"), 0.6f);
    if (d.fly) { if (!storyFlags.count("hint_airship")) { storyFlags.insert("hint_airship"); toastHint = "Joystick : diriger. Saut maintenu : monter. Bouclier maintenu : descendre. Action : se poser."; } }
    else if (!storyFlags.count("hint_vehicle")) { storyFlags.insert("hint_vehicle"); toastHint = "Joystick : la direction où aller. Bouton d'action : descendre."; }
}

void Game::summonVehicle(int i) {
    if (i < 0 || i >= (int)vehicles.size() || player == NullEntity) return;
    if (!vehicleUnlocked(i)) { notify("Ce véhicule n'est pas encore débloqué : avancez dans l'histoire.", vec3(1.f, 0.7f, 0.5f), "i_lock"); return; }
    if (driving >= 0) { notify("Descendez d'abord du véhicule.", vec3(1.f, 0.7f, 0.5f), "i_lock"); return; }
    if (dungeon.active) { notify("Pas de véhicule dans un donjon.", vec3(1.f, 0.7f, 0.5f), "i_lock"); return; }
    Character& c = pc();
    vec2 fwd{std::sin(c.yaw), std::cos(c.yaw)}, right{fwd.y, -fwd.x};
    // first free, dry, gentle spot around the hero (right side first, then the other places)
    static const vec2 tries[] = {{4.f, 1.f}, {-4.f, 1.f}, {0.f, 5.f}, {4.f, -3.f}, {-4.f, -3.f}, {0.f, -5.f}, {7.f, 0.f}, {-7.f, 0.f}};
    vec2 best = c.pos.xz() + right * 4.f; bool ok = false;
    for (vec2 t : tries) {
        float sc = vehicleDefs()[vehicles[i].def].fly ? 2.6f : 1.f;   // 3.4.0 the sky-ship needs room
        vec2 q = c.pos.xz() + (right * t.x + fwd * t.y) * sc;
        float gy = world.groundHeight(q.x, q.y), wl = world.waterLevel(q.x, q.y);
        if (wl > -1e8f && wl > gy - 0.2f) continue;
        if (world.normal(q.x, q.y).y < 0.85f || std::fabs(gy - c.pos.y) > 3.f) continue;
        vec3 test(q.x, gy, q.y); vec3 before = test;
        world.collide(test, 1.6f, gy);
        if (length(test.xz() - before.xz()) > 0.3f) continue;
        best = q; ok = true; break;
    }
    if (!ok) { notify("Pas assez de place ici pour le véhicule.", vec3(1.f, 0.7f, 0.5f), "i_lock"); return; }
    VehicleState& v = vehicles[i];
    v.pos = vec3(best.x, world.groundHeight(best.x, best.y), best.y);
    v.yaw = c.yaw; v.velYaw = v.yaw; v.speed = 0; v.vy = 0; v.air = false; v.airT = 0; v.pitch = v.roll = 0; v.groundPrev = v.pos.y;
    for (int k = 0; k < 3; k++) particles.dust(v.pos + vec3(particles.rng.range(-1.5f, 1.5f), 0, particles.rng.range(-1.5f, 1.5f)), 1.f);
    particles.shockwave(v.pos + vec3(0, 0.1f, 0), vec3(0.9f, 0.85f, 0.7f), 3.f);
    play("door", 0.7f);
    if (vehicleDefs()[v.def].fly) { v.pos.y += 25.f; v.alt = 25.f; v.landing = true; }   // 3.4.0 the sky-ship comes down from the clouds
    notify(std::string(vehicleDefs()[v.def].fly ? "Le vaisseau descend des nuages." : (vehicleDefs()[v.def].moto ? "La moto est là." : "La voiture est là.")), vec3(0.7f, 0.9f, 1.f), "i_mount");
}

// 2.3.2 whistle the horse straight to the hero (granted a stable horse if the hero has none), then hop on
void Game::summonHorse() {
    if (player == NullEntity) return;
    if (driving >= 0) { notify("Descendez d'abord du véhicule.", vec3(1.f, 0.7f, 0.5f), "i_lock"); return; }
    if (dungeon.active) { notify("Pas de cheval dans un donjon.", vec3(1.f, 0.7f, 0.5f), "i_lock"); return; }
    if (riding) { notify("Vous êtes déjà en selle.", vec3(0.7f, 0.9f, 1.f), "i_mount"); return; }
    if (ownedMounts.empty()) {
        ownedMounts.push_back("horse");
        notify("L'écurie royale vous confie un cheval.", vec3(0.8f, 1.f, 0.7f), "i_mount");
    }
    if (activeMount < 0 || activeMount >= (int)mounts().size()) {
        activeMount = 0;
        for (int i = 0; i < (int)mounts().size(); i++) if (ownedMounts.back() == mounts()[i].id) activeMount = i;
    }
    Character& c = pc();
    vec2 fwd{std::sin(c.yaw), std::cos(c.yaw)}, right{fwd.y, -fwd.x};
    static const vec2 tries[] = {{2.2f, 0.f}, {-2.2f, 0.f}, {0.f, 2.5f}, {0.f, -2.5f}, {2.f, 2.f}, {-2.f, 2.f}};
    vec2 best = c.pos.xz() + right * 2.2f;
    for (vec2 t : tries) {
        vec2 q = c.pos.xz() + right * t.x + fwd * t.y;
        float gy = world.groundHeight(q.x, q.y), wl = world.waterLevel(q.x, q.y);
        if (wl > -1e8f && wl > gy - 0.2f) continue;
        if (std::fabs(gy - c.pos.y) > 2.f) continue;
        vec3 test(q.x, gy, q.y); vec3 before = test;
        world.collide(test, 0.9f, gy);
        if (length(test.xz() - before.xz()) > 0.3f) continue;
        best = q; break;
    }
    vec3 sp(best.x, world.groundHeight(best.x, best.y), best.y);
    Character* m = reg.valid(mountEnt) ? reg.get<Character>(mountEnt) : nullptr;
    if (m) { m->pos = sp; m->yaw = c.yaw; }
    else { mountEnt = spawnMount(activeMount, sp); m = reg.get<Character>(mountEnt); if (m) m->yaw = c.yaw; }
    mountCalled = false;
    play("whistle", 0.8f); play3D("horse", sp, 0.8f);
    for (int k = 0; k < 3; k++) particles.dust(sp + vec3(particles.rng.range(-1.f, 1.f), 0, particles.rng.range(-1.f, 1.f)), 1.f);
    toggleMount();   // close enough: straight into the saddle
}

// speed in km/h, a needle dial and a bar; red past 80 % of the top speed
void Game::drawSpeedometer() {
    if (driving < 0 || driving >= (int)vehicles.size()) return;
    UI& u = ui;
    const VehicleState& v = vehicles[driving];
    const VehicleDef& d = vehicleDefs()[v.def];
    float kmh = std::fabs(v.speed) * 3.6f, top = d.maxSpeed * 3.6f;
    float k = saturate(kmh / std::max(1.f, top));
    // 3.1.0 a compact dial on the left, between the stats and the joystick: it used to sit in the middle of the
    // screen (over the road ahead) right under the quest tracker
    float R = 40.f;
    vec2 c{R + 46.f, u.H * 0.45f};
    u.circle(c, R + 10, vec4(0.03f, 0.025f, 0.02f, 0.78f), 40);
    const float a0 = PI * 0.75f, a1 = PI * 2.25f;
    u.ring(c, R - 6, R, a0, a1, vec4(1, 1, 1, 0.18f), 48);
    u.ring(c, R - 6, R, a0, a0 + (a1 - a0) * k, k > 0.8f ? vec4(1.f, 0.35f, 0.25f, 0.95f) : vec4(0.45f, 0.85f, 1.f, 0.95f), 48);
    for (int i = 0; i <= 8; i++) {
        float a = a0 + (a1 - a0) * i / 8.f;
        vec2 dir{std::cos(a), std::sin(a)};
        u.circle(c + dir * (R - 14), i % 2 ? 1.5f : 2.5f, vec4(1, 1, 1, 0.7f), 6);
    }
    float an = a0 + (a1 - a0) * k;
    vec2 nd{std::cos(an), std::sin(an)};
    for (int i = 0; i < 10; i++) u.circle(c + nd * (5.f + i * (R - 12.f) / 10.f), 2.4f - i * 0.15f, vec4(1.f, 0.55f, 0.3f, 1.f), 6);
    u.circle(c, 6, vec4(0.9f, 0.9f, 0.9f, 1), 10);
    char b[16]; snprintf(b, sizeof b, "%d", (int)std::round(kmh));
    u.text(b, c.x, c.y + 8, 26, rgba(0xf4f0e6), F_BOLD, A_CENTER, 0, true);
    u.text("km/h", c.x, c.y + 34, 12, rgba(0xb8b0a0), F_BODY, A_CENTER);
    if (v.speed < -0.3f) u.text("R", c.x + R * 0.55f, c.y - 16, 16, rgba(0xffb070), F_BOLD, A_CENTER);
}

void Game::exitVehicle() {
    if (driving < 0) return;
    VehicleState& v = vehicles[driving];
    const VehicleDef& d = vehicleDefs()[v.def];
    if (d.fly && v.alt > 1.2f && !pc().dead) { if (!v.landing) { v.landing = true; notify("Le vaisseau se pose…", vec3(0.8f, 0.9f, 1.f), "i_mount"); } return; }   // 3.4.0 land first
    if (d.fly) { v.landing = false; v.speed = 0; }
    Character& c = pc();
    vec3 side = vehicleLocal(v, vec3(d.fly ? 4.2f : (d.moto ? 1.1f : 1.9f), 0, 0)) - v.pos;
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
    if (d.fly) { updateAirship(dt, want, mag); return; }
    VehicleAsset* A = vehicleAsset(d.model);
    Character& c = pc();
    if (input.mount || input.jump || c.dead) { exitVehicle(); return; }
    vec3 fwd(std::sin(v.yaw), 0, std::cos(v.yaw));
    if (std::fabs(v.speed) < 0.5f || std::fabs(wrapAngle(v.velYaw - v.yaw)) > 1.2f) v.velYaw = v.yaw;
    bool airborne = v.air && v.airT > 0.06f;   // tiny bumps keep the grip
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
        float grip = clampf(std::fabs(v.speed) / 5.f, 0.f, 1.f) * (airborne ? 0.15f : 1.f);
        yawRate = clampf(ang * 3.f, -d.turn, d.turn) * grip * (v.speed < 0 ? -1.f : 1.f);
        v.steer = damp(v.steer, clampf(ang, -0.5f, 0.5f) * (v.speed < 0 ? -1.f : 1.f), 8.f, dt);
    } else v.steer = damp(v.steer, 0.f, 6.f, dt);
    float accel = (std::fabs(target) > std::fabs(v.speed) && target * v.speed >= 0) ? d.accel : 9.f;   // engine / brakes
    if (!airborne) v.speed = approach(v.speed, target, accel * dt);
    // uphill slows, downhill speeds up a little
    float hAhead = groundAt(v.pos.x + fwd.x * 2.f, v.pos.z + fwd.z * 2.f) - groundAt(v.pos.x, v.pos.z);
    v.speed -= clampf(hAhead / 2.f, -0.5f, 0.8f) * 3.f * dt * (v.speed >= 0 ? 1.f : -1.f);
    float prevYaw = v.yaw;
    v.yaw = wrapAngle(v.yaw + yawRate * dt);
    fwd = vec3(std::sin(v.yaw), 0, std::cos(v.yaw));
    // 1.11.2 grip: the travel direction follows the heading; in a hard fast turn the car slides (drift), the
    // moto barely does, nothing steers in the air
    float latAcc = std::fabs(yawRate * v.speed);
    float gripRate = airborne ? 0.f : (d.moto ? 14.f : (latAcc > 17.f ? 3.2f : 10.f));
    v.velYaw = wrapAngle(v.velYaw + wrapAngle(v.yaw - v.velYaw) * std::min(1.f, gripRate * dt));
    v.slip = std::fabs(wrapAngle(v.velYaw - v.yaw));
    if (!airborne) v.speed *= 1.f - std::min(0.5f, v.slip) * 0.5f * dt;   // sliding scrubs speed
    vec3 vdir(std::sin(v.velYaw), 0, std::cos(v.velYaw));
    vec3 np = v.pos + vdir * v.speed * dt;
    // water, walls of rock and high drops stop the vehicle (going down a slope or off a crest is fine)
    float wl = waterAt(np.x, np.z), gy = groundAt(np.x, np.z);
    bool blocked = (wl > -1e8f && gy < wl + 0.25f) || gy - v.pos.y > 1.2f + std::fabs(v.speed) * dt * 1.5f || (!v.air && v.pos.y - gy > 3.f);
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
        // 1.11.2 vertical motion: gravity, the ground pushes up (and launches the vehicle over a crest)
        float gvy = clampf((ty - v.groundPrev) / std::max(dt, 1e-4f), -30.f, 0.3f * std::fabs(v.speed) + 1.f);
        if (std::fabs(ty - v.groundPrev) > 2.f) gvy = 0;   // teleport, first frame
        v.groundPrev = ty;
        v.vy -= 20.f * dt;
        float y = v.pos.y + v.vy * dt;
        if (y <= ty) {
            float impact = v.vy - gvy;
            if (v.air && v.airT > 0.12f && impact < -3.5f) {   // landing: the springs compress, dust, a thud
                v.heaveV += impact * 0.12f; v.suspPV += impact * 0.05f;
                addShake(clampf(-impact * 0.025f, 0.05f, 0.35f)); haptic(30, (int)clampf(-impact * 25.f, 60.f, 220.f));
                play("land", 0.5f, 0.6f); particles.dust(v.pos, 1.2f);
            }
            y = ty; v.vy = gvy; v.air = false; v.airT = 0;
        } else if (y > ty + 0.05f) { v.air = true; v.airT += dt; }
        v.pos.y = y;
        if (!v.air || v.airT < 0.06f) { v.pitch = damp(v.pitch, tp, 10.f, dt); v.roll = damp(v.roll, tr, 8.f, dt); }
        else { v.pitch = damp(v.pitch, -0.12f, 1.2f, dt); v.roll = damp(v.roll, 0.f, 2.f, dt); }   // nose drops slowly in the air
        // body springs (car): squat when accelerating, dive when braking, lean out of the turn
        if (!d.moto) {
            float acc = clampf((v.speed - v.prevSpeed) / std::max(dt, 1e-4f), -25.f, 25.f);
            float lat = clampf(-yawRate * v.speed * 0.004f, -0.07f, 0.07f);
            v.suspPV += (-70.f * v.suspP - 9.f * v.suspPV + acc * 0.0035f * 70.f) * dt; v.suspP = clampf(v.suspP + v.suspPV * dt, -0.08f, 0.08f);
            v.suspRV += (-70.f * (v.suspR - lat) - 9.f * v.suspRV) * dt; v.suspR = clampf(v.suspR + v.suspRV * dt, -0.09f, 0.09f);
            v.heaveV += (-90.f * v.heave - 10.f * v.heaveV) * dt; v.heave = clampf(v.heave + v.heaveV * dt, -0.12f, 0.08f);
        }
        v.prevSpeed = v.speed;
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
    // tyres sliding: smoke-dust from the rear wheels
    if (A && v.slip > 0.12f && std::fabs(v.speed) > 9.f && !v.air && particles.rng.chance(dt * 30.f)) {
        const auto& J = A->data.joints;
        for (size_t j = J.size() - (d.moto ? 1 : 2); j < J.size(); j++) particles.dust(vehicleLocal(v, vec3(J[j].x, 0.1f, J[j].z)), 0.9f);
    }
    if (std::fabs(v.speed) > 8.f && particles.rng.chance(dt * 12.f)) {
        vec3 back = vehicleLocal(v, A ? vec3(A->data.joints.back().x, 0.1f, A->data.joints.back().z) : vec3(0, 0.1f, -1.5f));
        particles.dust(back, 0.5f + 0.5f * saturate(std::fabs(v.speed) / d.maxSpeed));
    }
}

void Game::renderVehicles() {
    for (int i = 0; i < (int)vehicles.size(); i++) {
        VehicleState& v = vehicles[i];
        const VehicleDef& d = vehicleDefs()[v.def];
        if (!vehicleUnlocked(i) || distXZ(v.pos, camFocus) > renderer.settings.drawDistance) continue;   // 3.4.0 not earned yet: not there
        if (d.fly) { renderAirship(v); continue; }
        VehicleAsset* A = vehicleAsset(d.model);
        if (!A) continue;
        if (driving != i) {   // parked: settle on the ground
            v.pos.y = groundAt(v.pos.x, v.pos.z);
        }
        if (!A->mesh || !A->mesh->valid()) { A->mesh = std::make_shared<Mesh>(); A->mesh->upload(A->data.verts, A->data.idx); }
        if (!A->tex.id) A->tex.create2D(A->data.texW, A->data.texH, GL_RGBA8, GL_RGBA, GL_UNSIGNED_BYTE, A->data.rgba.data(), true, false);
        const auto& J = A->data.joints;
        int nb = (int)J.size();
        v.bones.resize(nb);
        for (int b = 0; b < nb; b++) {
            if (b == 0 && !d.moto) {   // 1.11.2 car body on its springs (the wheels stay on the ground)
                vec3 pv(0, J.size() > 2 ? J[2].y : 0.4f, 0);
                v.bones[0] = mat4::trs(pv + vec3(0, v.heave, 0), quat::axisAngle({1, 0, 0}, -v.suspP) * quat::axisAngle({0, 0, 1}, v.suspR), vec3(1)) * mat4::trs(-pv, quat(), vec3(1));
                continue;
            }
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


// ============================================================================================ 3.4.0 the sky-ship
// The Order's airship: a wooden hull with a gilded gunwale, a planked deck, railings, a stern cabin with lit
// windows, a helm wheel, an eagle figurehead and a bow lantern, hung under a striped envelope (gold bands,
// tail fins) by eight ropes, pushed by two engine pods with propellers. Origin: under the keel; +Z forward.
static void buildAirship(MeshBuilder& M) {
    Mat hull(hexColor(0x5a3a22), 0.75f), hull2(hexColor(0x4a2e1a), 0.8f), deck(hexColor(0x9a7448), 0.7f), deck2(hexColor(0x8a6a40), 0.75f);
    Mat gold(hexColor(0xd8b058), 0.3f, 0.9f), iron(hexColor(0x3a3a40), 0.45f, 0.8f), rope(hexColor(0xc8b48a), 0.9f);
    Mat cloth(hexColor(0xece2c8), 0.85f), red(hexColor(0xa8342a), 0.8f), glass(hexColor(0xffc870), 0.2f, 0.f, 0.f);
    glass = glass.nightGlow(3.5f);
    // hull: a deep rounded keel, a darker band of planks at the waterline, the gilded gunwale
    M.ellipsoid({0, 1.35f, 0}, vec3(2.1f, 1.25f, 6.2f), hull, 18, 10);
    M.ellipsoid({0, 1.0f, -0.2f}, vec3(1.9f, 0.9f, 5.7f), hull2, 16, 8);
    M.ellipsoid({0, 2.05f, 0}, vec3(2.2f, 0.16f, 6.3f), gold, 22, 4);
    M.box({0, 0.25f, 0}, {0.12f, 0.2f, 5.2f}, iron);   // the keel
    // deck planks
    for (int i = -8; i <= 8; i++) M.box({i * 0.21f, 2.17f, -0.1f}, {0.1f, 0.05f, 5.f - std::fabs((float)i) * 0.12f}, i % 2 ? deck : deck2);
    // railings: posts and a rail each side, round the bow
    for (int sd = -1; sd <= 1; sd += 2) {
        for (int k = 0; k < 10; k++) { float z = -4.6f + k * 1.0f; float x = sd * 1.95f * std::sqrt(std::max(0.05f, 1.f - (z / 6.3f) * (z / 6.3f))); M.tube({x, 2.2f, z}, {x, 2.85f, z}, 0.05f, 0.05f, deck2, 6, false); }
        for (int k = 0; k < 9; k++) { float z0 = -4.6f + k, z1 = z0 + 1.f; auto X = [&](float z) { return sd * 1.95f * std::sqrt(std::max(0.05f, 1.f - (z / 6.3f) * (z / 6.3f))); }; M.tube({X(z0), 2.88f, z0}, {X(z1), 2.88f, z1}, 0.06f, 0.06f, gold, 6, false); }
    }
    // stern cabin with lit windows and a roof
    M.bevelBox({0, 2.95f, -3.9f}, {1.45f, 0.75f, 1.1f}, 0.08f, hull);
    for (int sd = -1; sd <= 1; sd += 2) for (int k = 0; k < 2; k++) M.box({sd * 1.47f, 3.05f, -4.4f + k * 0.9f}, {0.03f, 0.25f, 0.22f}, glass);
    M.box({0, 3.05f, -2.78f}, {0.5f, 0.3f, 0.03f}, glass);
    M.bevelBox({0, 3.78f, -3.9f}, {1.65f, 0.1f, 1.3f}, 0.05f, red);
    // helm: post, wheel with spokes
    M.tube({0, 2.2f, -2.2f}, {0, 3.f, -2.2f}, 0.09f, 0.07f, hull2, 8, true);
    M.lathe({{0.42f, -0.04f}, {0.42f, 0.04f}, {0.36f, 0.04f}, {0.36f, -0.04f}}, deck2, 16, {0, 3.15f, -2.1f}, quat::axisAngle({1, 0, 0}, PI / 2));
    for (int k = 0; k < 8; k++) { float a = k * TAU / 8; vec3 d{std::cos(a), std::sin(a), 0}; M.tube(vec3(0, 3.15f, -2.1f), vec3(0, 3.15f, -2.1f) + d * 0.52f, 0.025f, 0.02f, gold, 5, true); }
    // bow: figurehead (a gilded eagle), bowsprit, lantern
    M.tube({0, 2.1f, 5.8f}, {0, 2.9f, 8.2f}, 0.12f, 0.05f, hull2, 8, true);
    M.ellipsoid({0, 2.35f, 6.25f}, vec3(0.3f, 0.35f, 0.45f), gold, 10, 7);
    M.cone({0, 2.45f, 6.6f}, {0, 2.35f, 7.05f}, 0.12f, gold, 6);
    for (int sd = -1; sd <= 1; sd += 2) M.box({sd * 0.42f, 2.4f, 6.05f}, {0.32f, 0.04f, 0.3f}, gold, quat::axisAngle({0, 0, 1}, sd * 0.5f));
    M.box({0, 3.2f, 7.9f}, {0.16f, 0.2f, 0.16f}, glass); M.box({0, 3.43f, 7.9f}, {0.2f, 0.04f, 0.2f}, iron);
    // mast and furled sail
    M.tube({0, 2.2f, 1.2f}, {0, 6.6f, 1.2f}, 0.12f, 0.08f, hull2, 8, true);
    M.tube({-1.3f, 5.6f, 1.2f}, {1.3f, 5.6f, 1.2f}, 0.06f, 0.06f, hull2, 6, true);
    M.ellipsoid({0, 5.45f, 1.25f}, vec3(1.25f, 0.2f, 0.18f), cloth, 10, 5);
    // side wings (fins of canvas on a frame)
    for (int sd = -1; sd <= 1; sd += 2) {
        M.box({sd * 3.1f, 2.3f, 0.6f}, {1.1f, 0.03f, 1.6f}, cloth, quat::axisAngle({0, 0, 1}, sd * 0.22f));
        M.tube({sd * 2.f, 2.1f, 2.2f}, {sd * 4.2f, 2.6f, 1.4f}, 0.05f, 0.04f, gold, 5, true);
        M.tube({sd * 2.f, 2.1f, -1.0f}, {sd * 4.2f, 2.6f, -0.2f}, 0.05f, 0.04f, gold, 5, true);
    }
    // engine pods (the propellers are drawn apart, they turn)
    for (int sd = -1; sd <= 1; sd += 2) {
        vec3 pc{sd * 2.7f, 3.0f, -3.6f};
        M.tube(pc + vec3(0, 0, 1.1f), pc + vec3(0, 0, -0.9f), 0.32f, 0.38f, iron, 12, true);
        M.tube(pc + vec3(-sd * 0.3f, -0.1f, 0.2f), {sd * 1.9f, 2.4f, -3.4f}, 0.08f, 0.08f, iron, 6, false);
        M.ellipsoid(pc + vec3(0, 0, 1.15f), vec3(0.3f, 0.3f, 0.25f), gold, 10, 6);
    }
    // envelope: striped cloth, gold bands, tail fins
    const vec3 E{0, 9.6f, -0.3f}; const vec3 ER{3.4f, 2.8f, 8.2f};
    {   // the envelope as rings along its length: crisp cream and red stripes with thin gold bands between them
        quat along = quat::axisAngle({1, 0, 0}, PI / 2);
        auto rad = [&](float z) { float t = clampf(z / ER.z, -1.f, 1.f); return std::max(0.02f, ER.x * std::sqrt(1.f - t * t)); };
        std::vector<float> cut = {-ER.z};
        for (int k = -7; k <= 7; k++) { float z = k * 1.05f; cut.push_back(z - 0.06f); cut.push_back(z + 0.06f); }
        cut.push_back(ER.z);
        for (size_t i = 0; i + 1 < cut.size(); i++) {
            float z0 = cut[i], z1 = cut[i + 1];
            bool band = i % 2 == 1;   // the thin pieces between two cuts of the same seam
            int stripe = (int)std::floor((z0 + z1) * 0.5f / 1.05f + 0.5f);
            const Mat& m = band ? gold : ((stripe & 1) ? red : cloth);
            std::vector<vec2> prof;
            int n = band ? 1 : 3;
            for (int k = 0; k <= n; k++) { float z = lerpf(z0, z1, (float)k / n); prof.push_back({rad(z) + (band ? 0.03f : 0.f), z}); }
            M.lathe(prof, m, 28, E, along, ER.y / ER.x);
        }
    }
    M.ellipsoid(E + vec3(0, -ER.y + 0.05f, 0), vec3(0.6f, 0.15f, 6.f), gold, 10, 4);   // the keel band under it
    M.box(E + vec3(0, 1.9f, -7.4f), {0.06f, 1.4f, 1.1f}, red, quat::axisAngle({1, 0, 0}, -0.25f));
    for (int sd = -1; sd <= 1; sd += 2) M.box(E + vec3(sd * 1.7f, 0, -7.3f), {1.2f, 0.06f, 1.0f}, red);
    M.box(E + vec3(0, -1.9f, -7.4f), {0.06f, 1.f, 0.9f}, red, quat::axisAngle({1, 0, 0}, 0.25f));
    // ropes from the gunwale to the envelope
    for (int sd = -1; sd <= 1; sd += 2) for (int k = 0; k < 4; k++) {
        float z = -3.6f + k * 2.6f;
        float x = sd * 1.9f * std::sqrt(std::max(0.05f, 1.f - (z / 6.3f) * (z / 6.3f)));
        float f = std::sqrt(std::max(0.f, 1.f - ((z + 0.3f) / ER.z) * ((z + 0.3f) / ER.z)));
        M.tube({x, 2.9f, z}, E + vec3(sd * ER.x * f * 0.55f, -ER.y * f * 0.8f, z + 0.3f), 0.025f, 0.025f, rope, 4, false);
    }
}

static void buildPropeller(MeshBuilder& M) {
    Mat wood(hexColor(0x8a6a40), 0.6f), gold(hexColor(0xd8b058), 0.3f, 0.9f);
    for (int k = 0; k < 3; k++) {
        float a = k * TAU / 3;
        vec3 d{std::cos(a), std::sin(a), 0};
        M.box(d * 0.55f, {0.5f, 0.09f, 0.025f}, wood, quat::axisAngle({0, 0, 1}, a) * quat::axisAngle({1, 0, 0}, 0.35f));
    }
    M.ellipsoid({0, 0, 0}, vec3(0.14f), gold, 8, 5);
}

void Game::updateAirship(float dt, vec3 want, float mag) {
    VehicleState& v = vehicles[driving];
    const VehicleDef& d = vehicleDefs()[v.def];
    Character& c = pc();
    if (c.dead) { v.alt = 0; exitVehicle(); return; }
    vec3 fwd(std::sin(v.yaw), 0, std::cos(v.yaw));
    float gy = groundAt(v.pos.x, v.pos.z), wl = waterAt(v.pos.x, v.pos.z);
    float base = std::max(gy, wl > -1e8f ? wl : gy);
    float yawRate = 0;
    if (v.landing) {   // coming down to let the hero off
        v.alt = approach(v.alt, 0.f, (2.5f + v.alt * 0.5f) * dt);
        v.speed = approach(v.speed, 0.f, 8.f * dt);
        if (v.alt <= 0.02f) { v.alt = 0; v.landing = false; exitVehicle(); return; }
    } else {
        if (input.jumpHeld || input.jump) v.alt += 8.f * dt;   // climb
        if (input.block) v.alt -= 8.f * dt;                     // descend
        if (mag > 0.1f && v.alt < 3.f) v.alt = approach(v.alt, 3.f, 5.f * dt);   // pushing the stick lifts it off
        v.alt = clampf(v.alt, 0.f, 150.f);
        float target = 0;
        if (mag > 0.1f && v.alt > 1.f) {
            vec3 w = normalize(vec3(want.x, 0, want.z));
            float ang = wrapAngle(std::atan2(w.x, w.z) - v.yaw);
            yawRate = clampf(ang * 1.6f, -d.turn, d.turn);
            target = mag * d.maxSpeed * (std::fabs(ang) > 1.6f ? 0.3f : 1.f);
        }
        v.speed = approach(v.speed, target, (target > v.speed ? d.accel : 7.f) * dt);
    }
    float prevYaw = v.yaw;
    v.yaw = wrapAngle(v.yaw + yawRate * dt);
    fwd = vec3(std::sin(v.yaw), 0, std::cos(v.yaw));
    vec3 np = v.pos + fwd * v.speed * dt;
    // mountains ahead: the ship rises over them on its own (it never flies into a slope)
    float ahead = 0;
    for (int k = 1; k <= 3; k++) { vec3 q = np + fwd * (k * 5.f); ahead = std::max(ahead, groundAt(q.x, q.z)); }
    float ny = groundAt(np.x, np.z), nwl = waterAt(np.x, np.z);
    float nbase = std::max(ny, nwl > -1e8f ? nwl : ny);
    float ty = nbase + v.alt;
    if (v.alt > 0.5f) ty = std::max(ty, ahead + 2.5f);
    if (v.alt > 0.3f) {   // buildings and rocks: slide along them like the other vehicles
        vec3 before = np; collideStatic(np, d.radius, v.pos.y);
        if (length(vec2(np.x - before.x, np.z - before.z)) > 0.05f && std::fabs(v.speed) > 6.f) { v.speed *= 0.5f; addShake(0.2f); play("hit_wood", 0.5f, 0.6f); }
    }
    float prevY = v.pos.y;
    v.pos.x = np.x; v.pos.z = np.z;
    v.pos.y = std::max(nbase, damp(v.pos.y, ty, 2.2f, dt));
    v.alt = std::max(v.alt, std::min(150.f, v.pos.y - nbase - 0.01f) * (v.landing ? 0.f : 1.f));   // the altitude follows when it lifted itself over a crest
    float vy = (v.pos.y - prevY) / std::max(dt, 1e-4f);
    v.yawRate = wrapAngle(v.yaw - prevYaw) / std::max(dt, 1e-4f);
    v.pitch = damp(v.pitch, clampf(vy * 0.035f, -0.22f, 0.22f), 3.f, dt);
    v.roll = damp(v.roll, clampf(-v.yawRate * v.speed * 0.012f, -0.3f, 0.3f), 3.f, dt);
    v.prop = std::fmod(v.prop + (4.f + std::fabs(v.speed) * 1.2f + (v.alt > 0.5f ? 6.f : 0.f)) * dt * 3.f, TAU * 100.f);
    // the hero at the helm
    c.pos = vehicleLocal(v, vec3(0, 2.2f, -2.85f));
    c.yaw = v.yaw; c.vel = fwd * v.speed; c.grounded = true;
    c.vis.anim.speed = 0;
    float rev = saturate(std::fabs(v.speed) / d.maxSpeed);
    if (sfx.count("engine")) audio.setEngine(sfx["engine"], 0.12f + 0.18f * rev, 0.55f + 0.5f * rev);
    // exhaust from the engine pods, wisps when it flies fast
    if (particles.rng.chance(dt * (4.f + 10.f * rev))) for (int sd = -1; sd <= 1; sd += 2) particles.smoke(vehicleLocal(v, vec3(sd * 2.7f, 3.f, -4.6f)), 0.4f, 0.1f, vec3(0.75f, 0.75f, 0.78f));
}

void Game::renderAirship(VehicleState& v) {
    if (!airshipMesh) { MeshBuilder b; buildAirship(b); airshipMesh = std::make_shared<Mesh>(); b.upload(*airshipMesh); MeshBuilder pb; buildPropeller(pb); airshipProp = std::make_shared<Mesh>(); pb.upload(*airshipProp); }
    int idx = (int)(&v - vehicles.data());
    if (driving != idx) {   // parked, or coming down from the clouds when called
        float gy = groundAt(v.pos.x, v.pos.z), wl = waterAt(v.pos.x, v.pos.z);
        float base = std::max(gy, wl > -1e8f ? wl : gy);
        float dt = 1.f / 60.f;
        if (v.landing) { v.alt = approach(v.alt, 0.f, (3.f + v.alt * 0.5f) * dt); if (v.alt <= 0.02f) { v.alt = 0; v.landing = false; } }
        else v.alt = 0;
        v.pos.y = base + v.alt; v.pitch = damp(v.pitch, 0.f, 3.f, dt); v.roll = damp(v.roll, 0.f, 3.f, dt);
        v.prop = std::fmod(v.prop + (v.landing ? 8.f : 0.f) * dt * 3.f, TAU * 100.f);
    }
    float bob = v.alt > 0.5f ? std::sin(realTime * 1.3f) * 0.18f : 0.f;
    quat q = quat::axisAngle({0, 1, 0}, v.yaw) * quat::axisAngle({1, 0, 0}, -v.pitch) * quat::axisAngle({0, 0, 1}, v.roll + (v.alt > 0.5f ? std::sin(realTime * 0.9f) * 0.02f : 0.f));
    vec3 p = v.pos + vec3(0, bob, 0);
    bool near = distXZ(v.pos, camFocus) < 70.f;
    DrawItem it; it.mesh = airshipMesh.get(); it.model = mat4::trs(p, q, vec3(1)); it.castShadow = near;
    renderer.submit(it);
    for (int sd = -1; sd <= 1; sd += 2) {
        DrawItem pi; pi.mesh = airshipProp.get(); pi.castShadow = false;
        pi.model = mat4::trs(p + q.rotate(vec3(sd * 2.7f, 3.0f, -4.6f)), q * quat::axisAngle({0, 0, 1}, v.prop * (float)sd), vec3(1));
        renderer.submit(pi);
    }
}

}  // namespace lk
