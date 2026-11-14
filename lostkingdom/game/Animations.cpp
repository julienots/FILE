// LOST KINGDOM — animation library (authored in code).
// Conventions (humanoid, bind pose = arms hanging): local X pitch < 0 swings a limb forward,
// Z roll swings sideways (+X is the character's left), Y yaw twists. Spine pitch > 0 leans forward.
#include "Characters.h"
#include <mutex>
#include <unordered_map>

namespace lk {

static Clip gClips[C_COUNT];
static std::once_flag gClipOnce;

static Clip& mk(ClipId id, const char* name, float dur, bool loop = false) {
    Clip& c = gClips[id];
    c.name = name; c.duration = dur; c.loop = loop;
    return c;
}

// --------------------------------------------------------------------------------------- humanoid procedural
// Periodic Catmull-Rom curve through (phase, value) keys; phase in [0,1).
struct GaitKey { float u, v; };
static float gaitCurve(const GaitKey* k, int n, float u) {
    u -= std::floor(u);
    int i = n - 1;
    for (int j = 0; j < n; j++) if (k[j].u <= u) i = j;
    auto key = [&](int j, float& ku, float& kv) {
        int w = ((j % n) + n) % n;
        ku = k[w].u + std::floor((float)j / (float)n);   // unwrap keys before/after the cycle
        kv = k[w].v;
    };
    float u0, v0, u1, v1, u2, v2, u3, v3;
    key(i, u1, v1);
    key(i + 1, u2, v2); if (u2 <= u1) u2 += 1.f;
    key(i - 1, u0, v0); if (u0 >= u1) u0 -= 1.f;
    key(i + 2, u3, v3); while (u3 <= u2) u3 += 1.f;
    float uu = u < u1 ? u + 1.f : u;
    float t = (uu - u1) / std::max(1e-4f, u2 - u1);
    float t2 = t * t, t3 = t2 * t;
    return 0.5f * ((2 * v1) + (-v0 + v2) * t + (2 * v0 - 5 * v1 + 4 * v2 - v3) * t2 + (-v0 + 3 * v1 - 3 * v2 + v3) * t3);
}

// Biomechanical gait curves per leg (degrees), phase 0 = this leg's heel / foot strike.
// Walk: loading-response knee flexion, terminal extension, push-off, swing knee flexion ~60 deg.
static const GaitKey WALK_HIP[] = {{0.f, 25}, {0.3f, 2}, {0.55f, -12}, {0.65f, -6}, {0.8f, 20}, {0.9f, 27}};
static const GaitKey WALK_KNEE[] = {{0.f, 4}, {0.15f, 17}, {0.4f, 5}, {0.62f, 38}, {0.73f, 62}, {0.88f, 20}, {0.97f, 3}};
static const GaitKey WALK_ANKLE[] = {{0.f, -4}, {0.08f, 5}, {0.3f, -6}, {0.5f, -9}, {0.62f, 18}, {0.7f, 10}, {0.85f, -3}};
// Run: short stance (toe-off ~0.38), flight phase, high swing knee.
static const GaitKey RUN_HIP[] = {{0.f, 28}, {0.2f, 6}, {0.38f, -16}, {0.5f, -8}, {0.75f, 40}, {0.9f, 36}};
// 1.6.2: stance knee straighter (it read as a crouched "squat run"), swing knee unchanged
static const GaitKey RUN_KNEE[] = {{0.f, 14}, {0.12f, 30}, {0.38f, 12}, {0.6f, 100}, {0.72f, 112}, {0.9f, 34}};
static const GaitKey RUN_ANKLE[] = {{0.f, -6}, {0.12f, -12}, {0.38f, 25}, {0.6f, 10}, {0.85f, -5}};
#define GK(a) a, (int)(sizeof(a) / sizeof(a[0]))

// Swing direction of an attack clip (+1 / -1): sign of the chest's yaw change from the start to the end of
// the hit window. Drives the hips' coil / uncoil so the body follows the blade. Cached per clip.
static float clipSwingDir(const Clip* c) {
    static std::unordered_map<const Clip*, float> cache;
    auto it = cache.find(c);
    if (it != cache.end()) return it->second;
    Pose base, a0, a1;
    c->sample(0.f, a0, &base, MAX_BONES);
    c->sample(c->hitEnd, a1, &base, MAX_BONES);
    auto yaw = [](const quat& q) { vec3 f = q.rotate(vec3(0, 0, 1)); return std::atan2(f.x, f.z); };
    float d = wrapAngle(yaw(a1.rot[H_CHEST]) + yaw(a1.rot[H_SPINE]) - yaw(a0.rot[H_CHEST]) - yaw(a0.rot[H_SPINE]));
    float dir = std::fabs(d) < 0.05f ? 1.f : (d > 0 ? 1.f : -1.f);
    cache[c] = dir;
    return dir;
}

static void humanLoco(const Animator& a, Pose& p) {
    const float t = a.time, ph = a.phase * TAU, sp = a.speed;
    const GaitParams& g = a.gait;
    float moving = smoothstepf(0.05f, 0.7f, sp);
    float runW = saturate((sp - g.walkSpeed) / (g.runSpeed - g.walkSpeed));
    runW = runW * runW * (3 - 2 * runW);
    float sprintW = saturate((sp - g.runSpeed) / std::max(0.1f, g.sprintSpeed - g.runSpeed));
    float slow = 0.55f + 0.45f * saturate(sp / std::max(0.1f, g.walkSpeed));   // slow walks: smaller steps
    float lean = lerpf(2, 7, runW) + 8 * sprintW;
    float crouch = a.blocking ? 0.06f : 0.f;
    float idle = 1.f - moving;
    float aggressive = a.aux[0];  // hunched combat idle for monsters (0..1)

    // ---- legs from the gait curves (left leg phase u, right leg u + 0.5)
    float hip[2], knee[2], ank[2];
    for (int s = 0; s < 2; s++) {
        float u = a.phase + (s ? 0.5f : 0.f);
        float amp = slow * (1.f + 0.12f * sprintW) * lerpf(g.ampWalk, g.ampRun, runW);
        hip[s] = lerpf(gaitCurve(GK(WALK_HIP), u), gaitCurve(GK(RUN_HIP), u), runW) * amp;
        knee[s] = lerpf(gaitCurve(GK(WALK_KNEE), u), gaitCurve(GK(RUN_KNEE), u), runW) * amp;
        ank[s] = lerpf(gaitCurve(GK(WALK_ANKLE), u), gaitCurve(GK(RUN_ANKLE), u), runW);
    }
    float uL = a.phase;

    // ---- 1.6.2 body language ------------------------------------------------------------------------
    // Idle: weight on one leg (contrapposto), switching sides every ~7 s. Ready (enemy near): bladed guard,
    // lead (left) foot forward, knees soft. Attacks: wind-up, lunge on the lead leg, hips into the swing.
    float ready = saturate(a.aux[3]);
    float stillW = idle * (1.f - aggressive);
    float weightSide = std::sin(t * 0.9f) > 0 ? 1.f : -1.f;   // which leg carries the weight (switches smoothly below)
    float contra = smoothstepf(-0.35f, 0.35f, std::sin(t * 0.45f)) * 2.f - 1.f;   // -1..1, slow
    (void)weightSide;
    float guardW = stillW * ready;
    float relaxW = stillW * (1.f - ready);
    float atkWind = 0, atkStrike = 0, atkLunge = 0, atkDir = 1.f;
    if (const Clip* ac = a.action()) {
        if (ac->hitStart >= 0 && ac->hitEnd > ac->hitStart) {
            float at = a.actionTime(), h0 = ac->hitStart, h1 = ac->hitEnd;
            atkWind = smoothstepf(0.f, h0 * 0.85f, at) * (1.f - smoothstepf(h0 * 0.85f, h0 + 0.04f, at));
            atkStrike = smoothstepf(h0 - 0.1f, (h0 + h1) * 0.5f, at) * (1.f - 0.9f * smoothstepf(h1 + 0.05f, 1.f, at));
            atkLunge = clampf(ac->motionAt(1.f) / 1.2f, 0.35f, 1.f) * (1.f - runW * 0.7f);
            atkDir = clipSwingDir(ac);
        }
    }
    float atkW = std::max(atkWind, atkStrike);

    // ---- pelvis: lowest at double support when walking, at mid-stance when running; sway over the stance leg
    float bobWalk = -0.028f * (0.5f + 0.5f * std::cos(4 * PI * uL));
    float bobRun = -0.035f * (0.5f + 0.5f * std::cos(4 * PI * (uL - 0.12f))) - 0.008f;   // pelvis kept high
    float sway = std::sin(TAU * uL) * lerpf(0.022f, 0.01f, runW);
    p.rootOffset = vec3((sway * moving) + std::sin(t * 0.45f) * 0.012f * idle + contra * 0.03f * relaxW,
                        lerpf(bobWalk, bobRun, runW) * moving - crouch - aggressive * 0.08f + std::sin(t * 1.7f) * 0.004f * idle
                            - 0.012f * relaxW - 0.045f * guardW - (0.03f * atkWind + 0.075f * atkStrike) * atkLunge,
                        0.05f * atkStrike * atkLunge);
    float pelvisYaw = (hip[0] - hip[1]) * 0.13f * moving;                 // pelvis turns with the swinging leg
    float roll = clampf(-a.turnRate * sp * 0.9f, -10, 10) + std::cos(ph) * lerpf(3.f, 1.5f, runW) * moving;
    // hips: drop on the free leg at rest, bladed (turned) in guard, coil then uncoil through an attack
    float hipYaw = 22.f * guardW + atkDir * (-14.f * atkWind + 20.f * atkStrike);
    float hipRoll = -contra * 4.f * relaxW;
    p.rot[H_ROOT] = E(lean * 0.25f * moving + 6.f * atkStrike * atkLunge, pelvisYaw + hipYaw, roll + hipRoll);
    // posture: chest open, back straight (heroic), less of it in the aggressive monster crouch
    float proud = 1.f - aggressive;
    p.rot[H_SPINE] = E(lean * moving * 0.45f + aggressive * 12 - 2.5f * proud - 4.f * atkWind + 8.f * atkStrike, -pelvisYaw * 0.6f - hipYaw * 0.35f, -roll * 0.4f - hipRoll * 0.6f);
    float breath = std::sin(t * (1.6f + a.breath * 1.2f)) * (1.2f + a.breath * 1.5f);
    p.rot[H_CHEST] = E(lean * moving * 0.3f + breath * idle * 0.6f + aggressive * 10 - 3.5f * proud, -pelvisYaw * 0.9f - hipYaw * 0.45f, -roll * 0.3f - hipRoll * 0.4f);   // shoulders counter-rotate
    p.rot[H_NECK] = E(-lean * moving * 0.45f - aggressive * 12 + 3.f * proud, pelvisYaw * 0.5f, roll * 0.3f);
    float look = std::sin(t * 0.31f) * 14.f * idle * (1 - aggressive);
    p.rot[H_HEAD] = E(-lean * moving * 0.3f + std::sin(t * 0.23f) * 3 * idle, look + pelvisYaw * 0.5f, roll * 0.2f);   // head stays stable

    for (int s = 0; s < 2; s++) {
        int th = s ? H_THIGHR : H_THIGHL, sh = s ? H_SHINR : H_SHINL, ft = s ? H_FOOTR : H_FOOTL;
        float u = a.phase + (s ? 0.5f : 0.f); u -= std::floor(u);
        float toeOff = lerpf(0.62f, 0.38f, runW);
        float stance = 1.f - smoothstepf(toeOff - 0.06f, toeOff + 0.04f, u) + smoothstepf(0.94f, 1.0f, u);
        float thigh = -hip[s] * moving - crouch * 120 - aggressive * 14;
        float kn = knee[s] * moving + crouch * 220 + aggressive * 24 + idle * 4;
        float lead = s ? 0.f : 1.f;   // left leg leads (weapon in the right hand)
        // rest: the weight leg straight, the free leg's knee relaxed and slightly forward
        float freeLeg = saturate((s ? -contra : contra) * 1.2f) * relaxW;
        thigh += -6.f * freeLeg; kn += 10.f * freeLeg - 3.f * relaxW;
        // guard: feet apart front/back, knees soft
        thigh += guardW * (lead ? -16.f : 10.f); kn += guardW * (lead ? 20.f : 14.f);
        // attacks: weight back on the wind-up, then a lunge onto the lead leg, rear leg driving
        thigh += atkLunge * (atkWind * (lead ? -6.f : 10.f) + atkStrike * (lead ? -34.f : 20.f));
        kn += atkLunge * (atkWind * (lead ? 8.f : 16.f) + atkStrike * (lead ? 40.f : 8.f));
        // foot: kept level on the ground during stance (IK refines it), relaxed in swing, + ankle roll
        float level = -(thigh + kn) * lerpf(0.55f, 1.0f, stance);
        float foot = level + ank[s] * moving - crouch * 100 - aggressive * 10;
        p.rot[th] = E(thigh, -(s ? -1.f : 1.f) * 6.f * guardW, (s ? -1.f : 1.f) * (2.f + idle * 1.5f + 4.f * guardW + freeLeg * 2.f));
        p.rot[sh] = E(kn, 0, 0);
        p.rot[ft] = E(foot, 0, 0);
        // arms: counter-swing driven by the opposite leg, elbows bend more when running
        int sl = s ? H_SHR : H_SHL, ua = s ? H_UARMR : H_UARML, la = s ? H_LARMR : H_LARML, hd = s ? H_HANDR : H_HANDL;
        float side = s ? -1.f : 1.f;
        float swing = hip[s] * lerpf(0.75f, 1.05f, runW) * moving;           // arm goes back when its leg goes forward
        float elbow = lerpf(16, 82, runW) + std::max(0.f, -swing) * 0.35f;
        p.rot[sl] = E(0, 0, side * breath * 0.6f * idle);
        p.rot[ua] = E(swing + breath * 0.5f * idle - aggressive * 20, side * 4 * runW, side * (7 + 5 * runW + aggressive * 15));
        p.rot[la] = E(-(elbow * moving + 10 * idle + aggressive * 30), 0, 0);
        p.rot[hd] = E(0, 0, side * 6 * runW);
    }
    // off-hand: reaches forward on the wind-up and pulls back as the blade comes through (counterbalance);
    // raised a little in guard
    if (atkW > 0.01f || guardW > 0.01f) {
        p.rot[H_UARML] = normalize(p.rot[H_UARML] * E(-35.f * atkWind + 25.f * atkStrike - 20.f * guardW, 0, 18.f * atkStrike + 10.f * guardW));
        p.rot[H_LARML] = normalize(p.rot[H_LARML] * E(-25.f * atkWind - 35.f * atkStrike - 30.f * guardW));
    }
    p.rot[H_HEAD] = normalize(p.rot[H_HEAD] * E(0, -hipYaw * 0.6f, 0));   // eyes stay on the target
    // relaxed (out of combat) one-handed carry: sword lowered, swinging with the arm
    // the wrist counter-rotates the arm swing and elbow bend so the blade keeps a steady angle to the ground
    float swingR = hip[1] * lerpf(0.75f, 1.05f, runW) * moving;
    float elbowR = lerpf(14, 70, runW) * moving + 12 * idle;
    quat relaxedR[3] = {p.rot[H_UARMR], E(-elbowR, 0, 0), E(lerpf(95, 70, runW) - swingR + elbowR * 0.6f, 10, 0)};
    // weapon stance overrides for the arms
    switch (a.stance) {
    case ST_ONEHAND: case ST_DAGGER: {
        bool dg = a.stance == ST_DAGGER;
        quat guard[3] = {dg ? E(-20, 0, -18) : E(-18 - 10 * runW + hip[1] * 0.3f * moving, 0, -14), dg ? E(-70, 0, 0) : E(-55 - 15 * runW, 0, 0), dg ? E(70, 0, 0) : E(20, 0, 0)};
        int bones[3] = {H_UARMR, H_LARMR, H_HANDR};
        for (int i = 0; i < 3; i++) p.rot[bones[i]] = nlerp(relaxedR[i], guard[i], ready);
        break;
    }
    case ST_TWOHAND:
        p.rot[H_UARMR] = E(-30, 20, -10); p.rot[H_LARMR] = E(-70, 0, 0); p.rot[H_HANDR] = E(35, 0, 30);
        p.rot[H_UARML] = E(-40, -30, 10); p.rot[H_LARML] = E(-75, 0, 0);
        p.rot[H_CHEST] = normalize(p.rot[H_CHEST] * E(0, -12, 0));
        break;
    case ST_SPEAR:
        p.rot[H_UARMR] = E(-25, 0, -18); p.rot[H_LARMR] = E(-80, 0, 0); p.rot[H_HANDR] = E(60, 0, 0);
        p.rot[H_UARML] = E(-45, -20, 8); p.rot[H_LARML] = E(-40, 0, 0);
        break;
    case ST_STAFF:
        p.rot[H_UARMR] = E(-15 + hip[1] * 0.35f * moving, 0, -12); p.rot[H_LARMR] = E(-80, 0, 0); p.rot[H_HANDR] = E(0, 0, 0);
        break;
    case ST_BOW:
        p.rot[H_UARML] = E(-10 + hip[0] * 0.4f * moving, 0, 10); p.rot[H_LARML] = E(-25, 0, 0); p.rot[H_HANDL] = E(0, 90, 0);
        if (a.aiming) {
            p.rot[H_CHEST] = normalize(p.rot[H_CHEST] * E(0, 60, 0));
            p.rot[H_UARML] = E(-90, -60, 0); p.rot[H_LARML] = E(0, 0, 0); p.rot[H_HANDL] = E(0, 0, -90);
            p.rot[H_UARMR] = E(-90, 30, -20); p.rot[H_LARMR] = E(-120, 0, 0);
            p.rot[H_HEAD] = E(0, -55, 0);
        }
        break;
    case ST_TOOL:
        p.rot[H_UARMR] = E(-12 + hip[1] * 0.6f * moving, 0, -12); p.rot[H_LARMR] = E(-35, 0, 0); p.rot[H_HANDR] = E(80, 0, 0);
        break;
    default: break;
    }
    if (a.blocking) {
        // shield / weapon raised to guard
        p.rot[H_UARML] = E(-65, -35, 25); p.rot[H_LARML] = E(-95, 0, 0); p.rot[H_HANDL] = E(0, 0, 0);
        if (a.stance == ST_TWOHAND) { p.rot[H_UARMR] = E(-70, 30, -20); p.rot[H_LARMR] = E(-90, 0, 0); p.rot[H_HANDR] = E(0, 0, -90); }
    }
}

static void humanAir(const Animator& a, Pose& p) {
    humanLoco(a, p);
    float up = clampf(a.verticalVel / 6.f, -1, 1);
    float tuck = up > 0 ? up : 0, fall = up < 0 ? -up : 0;
    p.rot[H_THIGHL] = E(-35 * tuck - 20 * fall, 0, 4); p.rot[H_SHINL] = E(60 * tuck + 20 * fall, 0, 0);
    p.rot[H_THIGHR] = E(-5 - 15 * fall + 10 * tuck, 0, -4); p.rot[H_SHINR] = E(30 + 25 * fall, 0, 0);
    if (a.stance == ST_UNARMED || a.stance == ST_TOOL) {
        p.rot[H_UARML] = E(-20 - 30 * fall, 0, 30 + 40 * fall); p.rot[H_UARMR] = E(-20 - 30 * fall, 0, -30 - 40 * fall);
        p.rot[H_LARML] = E(-30, 0, 0); p.rot[H_LARMR] = E(-30, 0, 0);
    }
    p.rot[H_SPINE] = E(8 * tuck - 6 * fall, 0, 0);
    p.rootOffset = vec3(0, 0.0f, 0);
}

static void humanSwim(const Animator& a, Pose& p) {
    const float t = a.time, ph = a.phase * TAU * 0.8f;
    float moving = smoothstepf(0.05f, 0.8f, a.speed);
    // breaststroke / treading water
    p.rootOffset = vec3(0, -0.85f + std::sin(t * 2.f) * 0.03f, 0);
    p.rot[H_ROOT] = E(lerpf(10, 70, moving), 0, std::sin(t * 1.3f) * 3);
    p.rot[H_SPINE] = E(-10 * moving, 0, 0);
    p.rot[H_NECK] = E(-lerpf(10, 45, moving), 0, 0);
    p.rot[H_HEAD] = E(-lerpf(0, 20, moving), 0, 0);
    float stroke = std::sin(ph * (moving > 0.1f ? 1.f : 0.6f) + t * (1 - moving) * 2.5f);
    for (int s = 0; s < 2; s++) {
        float side = s ? -1.f : 1.f;
        int ua = s ? H_UARMR : H_UARML, la = s ? H_LARMR : H_LARML, th = s ? H_THIGHR : H_THIGHL, sh = s ? H_SHINR : H_SHINL;
        p.rot[ua] = E(-100 + stroke * 30, side * stroke * 30, side * (40 + stroke * 30));
        p.rot[la] = E(-30 - std::max(0.f, -stroke) * 50, 0, 0);
        p.rot[th] = E(-10 + std::sin(t * 4.f + s * PI) * 20 * moving - (1 - moving) * 15, 0, side * (8 + stroke * 8));
        p.rot[sh] = E(30 + std::max(0.f, std::sin(t * 4.f + s * PI)) * 40, 0, 0);
    }
}

static void humanMounted(const Animator& a, Pose& p) {
    p.reset();
    float t = a.time;
    float trot = a.aux[1];  // mount gait intensity
    p.rootOffset = vec3(0, std::sin(a.phase * TAU * 2) * 0.04f * trot, 0);
    p.rot[H_SPINE] = E(6 + 10 * trot, 0, 0);
    p.rot[H_CHEST] = E(std::sin(t * 1.6f) * 1.2f, 0, 0);
    for (int s = 0; s < 2; s++) {
        float side = s ? -1.f : 1.f;
        p.rot[s ? H_THIGHR : H_THIGHL] = E(-75, side * 18, side * 28);
        p.rot[s ? H_SHINR : H_SHINL] = E(80, 0, 0);
        p.rot[s ? H_FOOTR : H_FOOTL] = E(-10, 0, 0);
        p.rot[s ? H_UARMR : H_UARML] = E(-35, 0, side * 8);
        p.rot[s ? H_LARMR : H_LARML] = E(-60, side * -15, 0);
    }
}

static void humanDead(const Animator&, Pose& p) {
    p.reset();
    p.rootOffset = vec3(0, -0.84f, -0.25f);
    p.rot[H_ROOT] = E(-86, 8, 3);
    p.rot[H_NECK] = E(10, 0, 0);
    p.rot[H_HEAD] = E(10, 35, 0);
    p.rot[H_UARML] = E(-10, 0, 60); p.rot[H_LARML] = E(-20, 0, 0);
    p.rot[H_UARMR] = E(-30, 0, -45); p.rot[H_LARMR] = E(-35, 0, 0);
    p.rot[H_THIGHL] = E(-10, 0, 8); p.rot[H_SHINL] = E(20, 0, 0);
    p.rot[H_THIGHR] = E(-25, 0, -6); p.rot[H_SHINR] = E(45, 0, 0);
}

static void humanStagger(const Animator& a, Pose& p) {
    humanLoco(a, p);
    float w = std::sin(a.time * 8.f) * 6;
    p.rot[H_SPINE] = E(-12, w, 4); p.rot[H_HEAD] = E(-15, -w, 0);
    p.rot[H_UARML] = E(10, 0, 35); p.rot[H_UARMR] = E(10, 0, -35);
}

static void humanVictory(const Animator& a, Pose& p) {
    humanLoco(a, p);
    float t = a.time;
    p.rot[H_UARMR] = E(-170, 0, -15); p.rot[H_LARMR] = E(-10, 0, 0); p.rot[H_HANDR] = E(0, 0, 0);
    p.rot[H_CHEST] = E(-10 + std::sin(t * 2) * 2, 0, 0); p.rot[H_HEAD] = E(-15, 0, 0);
    p.rot[H_UARML] = E(-20, 0, 25);
}

static void humanSit(const Animator& a, Pose& p) {
    p.reset();
    float b = std::sin(a.time * 1.4f);
    p.rootOffset = vec3(0, -0.5f, -0.05f);
    for (int s = 0; s < 2; s++) {
        float side = s ? -1.f : 1.f;
        p.rot[s ? H_THIGHR : H_THIGHL] = E(-88, 0, side * 6);
        p.rot[s ? H_SHINR : H_SHINL] = E(88, 0, 0);
        p.rot[s ? H_UARMR : H_UARML] = E(-25, 0, side * 6);
        p.rot[s ? H_LARMR : H_LARML] = E(-50, 0, 0);
    }
    p.rot[H_SPINE] = E(8 + b, 0, 0);
    p.rot[H_HEAD] = E(5, std::sin(a.time * 0.3f) * 25, 0);
}

static void humanSleep(const Animator& a, Pose& p) {
    humanDead(a, p);
    p.rot[H_CHEST] = E(std::sin(a.time * 0.9f) * 2.5f, 0, 0);
    p.rot[H_HEAD] = E(5, 50, 0);
}

static void humanWork(const Animator& a, Pose& p) {
    // generic labour loop (hammering / hoeing): driven by aux[2] style
    humanLoco(a, p);
    float t = a.time * 2.2f;
    float k = std::pow(std::max(0.f, std::sin(t)), 3.f);
    p.rot[H_SPINE] = E(18 + 10 * k, 0, 0);
    p.rot[H_UARMR] = E(-60 - 70 * (1 - k), 0, -10); p.rot[H_LARMR] = E(-40 - 30 * (1 - k), 0, 0); p.rot[H_HANDR] = E(80, 0, 0);
    p.rot[H_UARML] = E(-45, 0, 8); p.rot[H_LARML] = E(-50, 0, 0);
}

// --------------------------------------------------------------------------------------- quadruped procedural
static void quadLoco(const Animator& a, Pose& p) {
    const float t = a.time, sp = a.speed;
    const GaitParams& g = a.gait;
    float moving = smoothstepf(0.05f, 0.6f, sp);
    float trotW = saturate((sp - g.walkSpeed * 0.8f) / (g.walkSpeed));
    float gallopW = saturate((sp - g.runSpeed) / std::max(0.1f, g.sprintSpeed - g.runSpeed));
    float idle = 1 - moving;
    // leg phase offsets: FL, FR, BL, BR (walk -> trot -> gallop)
    float walkOff[4] = {0.25f, 0.75f, 0.0f, 0.5f}, trotOff[4] = {0.0f, 0.5f, 0.5f, 0.0f}, galOff[4] = {0.0f, 0.12f, 0.55f, 0.65f};
    int up[4] = {Q_FL1, Q_FR1, Q_BL1, Q_BR1}, mid[4] = {Q_FL2, Q_FR2, Q_BL2, Q_BR2}, low[4] = {Q_FL3, Q_FR3, Q_BL3, Q_BR3};
    float amp = lerpf(24, 36, trotW) + 12 * gallopW;
    for (int l = 0; l < 4; l++) {
        float off = lerpf(lerpf(walkOff[l], trotOff[l], trotW), galOff[l], gallopW);
        float ph = (a.phase + off) * TAU;
        float s = std::sin(ph), c = std::cos(ph);
        bool front = l < 2;
        p.rot[up[l]] = E(-s * amp * moving + (front ? 0.f : 0.f), 0, 0);
        float lift = std::max(0.f, c) * moving;
        if (front) { p.rot[mid[l]] = E(lift * 55, 0, 0); p.rot[low[l]] = E(-lift * 25, 0, 0); }
        else { p.rot[mid[l]] = E(-lift * 45 - 8, 0, 0); p.rot[low[l]] = E(lift * 50 + 8, 0, 0); }
    }
    float ph = a.phase * TAU;
    float bob = (0.02f + 0.04f * gallopW) * moving;
    p.rootOffset = vec3(0, -bob * (0.5f + 0.5f * std::cos(2 * ph)), 0);
    p.rot[Q_ROOT] = E(std::sin(ph) * 6 * gallopW, 0, clampf(-a.turnRate * sp * 0.8f, -8, 8));
    p.rot[Q_SPINE] = E(-std::sin(ph) * 10 * gallopW, std::sin(ph) * 3 * moving, 0);
    float breath = std::sin(t * 2.2f) * 1.5f * idle;
    p.rot[Q_CHEST] = E(breath + std::sin(ph) * 6 * gallopW, 0, 0);
    p.rot[Q_NECK] = E(-10 * moving + std::cos(2 * ph) * 4 * moving - a.aux[0] * 15, std::sin(t * 0.4f) * 20 * idle, 0);
    p.rot[Q_HEAD] = E(8 * moving + a.aux[0] * 20, std::sin(t * 0.27f) * 10 * idle, 0);
    p.rot[Q_JAW] = E(a.aux[0] * 8 + (gallopW > 0.5f ? 8.f : 0.f), 0, 0);
    float wag = std::sin(t * (2.f + 6 * moving)) * (15 + 10 * moving);
    p.rot[Q_TAIL1] = E(-10 - 20 * gallopW - a.aux[0] * 10, wag * 0.5f, 0);
    p.rot[Q_TAIL2] = E(5, wag * 0.6f, 0);
    p.rot[Q_TAIL3] = E(5, wag * 0.8f, 0);
}

static void quadDead(const Animator&, Pose& p) {
    p.reset();
    p.rootOffset = vec3(0, -0.5f, 0);
    p.rot[Q_ROOT] = E(0, 0, 85);
    p.rot[Q_NECK] = E(10, 0, -10);
    p.rot[Q_FL1] = E(-20, 0, 0); p.rot[Q_BL1] = E(25, 0, 0); p.rot[Q_FR1] = E(-35, 0, 0); p.rot[Q_BR1] = E(15, 0, 0);
    p.rot[Q_JAW] = E(15, 0, 0);
}

static void dragonLoco(const Animator& a, Pose& p) {
    quadLoco(a, p);
    float fly = a.aux[1];
    float t = a.time;
    float flap = std::sin(t * 5.f);
    // wings: folded on the ground, flapping when flying
    for (int s = 0; s < 2; s++) {
        float side = s ? -1.f : 1.f;
        int w1 = s ? Q_WINGR1 : Q_WINGL1, w2 = s ? Q_WINGR2 : Q_WINGL2;
        quat folded = E(10, side * -40, side * -25), wfold2 = E(0, side * -110, side * 15);
        quat fl1 = E(0, side * 5, side * (flap * 45 + 5)), fl2 = E(0, side * (5 - flap * 10), side * flap * 25);
        p.rot[w1] = nlerp(folded, fl1, fly);
        p.rot[w2] = nlerp(wfold2, fl2, fly);
    }
    if (fly > 0.01f) {
        int legsL[8] = {Q_FL1, Q_FR1, Q_BL1, Q_BR1, Q_FL2, Q_FR2, Q_BL2, Q_BR2};
        for (int i = 0; i < 4; i++) p.rot[legsL[i]] = nlerp(p.rot[legsL[i]], E(i < 2 ? -50.f : 40.f, 0, 0), fly);
        for (int i = 4; i < 8; i++) p.rot[legsL[i]] = nlerp(p.rot[legsL[i]], E(i < 6 ? 70.f : -60.f, 0, 0), fly);
        p.rootOffset.y += fly * (a.aux[2] + flap * 0.25f);
        p.rot[Q_TAIL1] = nlerp(p.rot[Q_TAIL1], E(-5, std::sin(t * 2) * 10, 0), fly);
    }
    p.rot[Q_NECK2] = E(-15 - a.aux[0] * 10, 0, 0);
}

// --------------------------------------------------------------------------------------- spider procedural
static void spiderLoco(const Animator& a, Pose& p) {
    float t = a.time, sp = a.speed;
    float moving = smoothstepf(0.05f, 0.6f, sp);
    float ph = a.phase * TAU * 2;
    p.rootOffset = vec3(0, std::sin(ph * 2) * 0.03f * moving + std::sin(t * 1.5f) * 0.01f, 0);
    p.rot[S_ABDOMEN] = E(std::sin(t * 1.7f) * 4 - a.aux[0] * 15, std::sin(ph) * 5 * moving, 0);
    p.rot[S_HEAD] = E(std::sin(t * 0.8f) * 4, std::sin(t * 0.5f) * 6 * (1 - moving), 0);
    for (int i = 0; i < 8; i++) {
        int k = i % 4, side = i < 4 ? 1 : -1;
        bool groupA = ((k % 2) == 0) == (side > 0);
        float lp = ph + (groupA ? 0.f : PI);
        float s = std::sin(lp), c = std::cos(lp);
        float baseYaw = (k - 1.5f) * -22.f * side;
        int up = S_LEG0 + i * 2, lo = up + 1;
        p.rot[up] = E(0, baseYaw + s * 18 * moving * side, side * (-std::max(0.f, c) * 22 * moving + std::sin(t * 2 + i) * 2));
        p.rot[lo] = E(0, 0, side * std::max(0.f, c) * 15 * moving);
    }
}

static void spiderDead(const Animator&, Pose& p) {
    p.reset();
    p.rootOffset = vec3(0, -0.35f, 0);
    p.rot[S_ROOT] = E(0, 0, 180);
    for (int i = 0; i < 8; i++) { int side = i < 4 ? 1 : -1; p.rot[S_LEG0 + i * 2] = E(0, 0, side * 50); p.rot[S_LEG0 + i * 2 + 1] = E(0, 0, side * 70); }
}

// --------------------------------------------------------------------------------------- clips
static void humanClips() {
    using C = Clip;
    // ---------- one-handed sword / axe: 3-hit combo + heavy spin + charged thrust
    {
        C& c = mk(C_1H_A1, "1h_slash_rl", 0.5f);
        c.key(0.0f, {{H_UARMR, E(-40, -60, -40)}, {H_LARMR, E(-70)}, {H_HANDR, E(60, 0, 0)}, {H_CHEST, E(0, -30)}, {H_SPINE, E(5, -10)}})
         .key(0.32f, {{H_UARMR, E(-95, -70, -10)}, {H_LARMR, E(-20)}, {H_HANDR, E(80, 0, 0)}, {H_CHEST, E(0, -35)}, {H_SPINE, E(8, -12)}}, Ease::In)
         .key(0.55f, {{H_UARMR, E(-85, 55, 0)}, {H_LARMR, E(-10)}, {H_HANDR, E(80, 0, 0)}, {H_CHEST, E(5, 35)}, {H_SPINE, E(10, 12)}}, Ease::Out)
         .key(1.0f, {{H_UARMR, E(-40, 30, -15)}, {H_LARMR, E(-55)}, {H_HANDR, E(40, 0, 0)}, {H_CHEST, E(0, 10)}, {H_SPINE, E(5, 0)}});
        c.hit(0.36f, 0.55f).event(0.33f, EV_SOUND_WHOOSH).move(0.3f, 0.2f).move(0.55f, 0.8f).move(1.f, 0.85f);
        c.comboOpen = 0.5f; c.cancelable = 0.7f;
    }
    {
        C& c = mk(C_1H_A2, "1h_slash_lr", 0.5f);
        c.key(0.0f, {{H_UARMR, E(-80, 60, 0)}, {H_LARMR, E(-30)}, {H_HANDR, E(80, 0, 0)}, {H_CHEST, E(5, 35)}})
         .key(0.3f, {{H_UARMR, E(-80, 75, 10)}, {H_LARMR, E(-60)}, {H_HANDR, E(100, 0, 0)}, {H_CHEST, E(5, 40)}}, Ease::In)
         .key(0.55f, {{H_UARMR, E(-90, -70, -20)}, {H_LARMR, E(-5)}, {H_HANDR, E(80, 0, 0)}, {H_CHEST, E(5, -35)}}, Ease::Out)
         .key(1.0f, {{H_UARMR, E(-35, -20, -15)}, {H_LARMR, E(-55)}, {H_HANDR, E(30, 0, 0)}, {H_CHEST, E(0, -5)}});
        c.hit(0.34f, 0.56f).event(0.32f, EV_SOUND_WHOOSH).move(0.55f, 0.8f).move(1.f, 0.9f);
        c.comboOpen = 0.5f;
    }
    {
        C& c = mk(C_1H_A3, "1h_overhead", 0.68f);
        c.key(0.0f, {{H_UARMR, E(-60, 0, -15)}, {H_LARMR, E(-50)}, {H_HANDR, E(30)}, {H_SPINE, E(0)}, {H_CHEST, E(0)}})
         .key(0.38f, {{H_UARMR, E(-175, 0, -10)}, {H_LARMR, E(-80)}, {H_HANDR, E(-30)}, {H_SPINE, E(-12)}, {H_CHEST, E(-10)}}, Ease::Out)
         .key(0.55f, {{H_UARMR, E(-35, 0, -10)}, {H_LARMR, E(-5)}, {H_HANDR, E(70)}, {H_SPINE, E(22)}, {H_CHEST, E(10)}}, Ease::In)
         .key(1.0f, {{H_UARMR, E(-30, 0, -15)}, {H_LARMR, E(-50)}, {H_HANDR, E(30)}, {H_SPINE, E(5)}, {H_CHEST, E(0)}});
        c.root(0.38f, {0, 0.02f, 0}).root(0.55f, {0, -0.12f, 0}).root(1.f, {0, -0.02f, 0});
        c.hit(0.45f, 0.6f).event(0.44f, EV_SOUND_WHOOSH).event(0.56f, EV_IMPACT).move(0.38f, 0.3f).move(0.55f, 1.3f).move(1.f, 1.35f);
        c.comboOpen = 0.62f; c.cancelable = 0.75f;
    }
    {
        C& c = mk(C_1H_HEAVY, "1h_spin", 0.9f);
        c.key(0.0f, {{H_ROOT, E(0, 0)}, {H_UARMR, E(-80, -60, -20)}, {H_LARMR, E(-30)}, {H_HANDR, E(85)}, {H_CHEST, E(0, -40)}})
         .key(0.2f, {{H_ROOT, E(0, 20)}, {H_UARMR, E(-90, -70, -10)}, {H_CHEST, E(0, -45)}}, Ease::In)
         .key(0.42f, {{H_ROOT, E(0, 140)}, {H_UARMR, E(-90, 40, 0)}, {H_LARMR, E(-5)}, {H_CHEST, E(0, 30)}}, Ease::Linear)
         .key(0.62f, {{H_ROOT, E(0, 260)}}, Ease::Linear)
         .key(0.8f, {{H_ROOT, E(0, 360)}, {H_UARMR, E(-70, 60, -10)}, {H_CHEST, E(0, 30)}}, Ease::Out)
         .key(1.0f, {{H_ROOT, E(0, 360)}, {H_UARMR, E(-35, 10, -15)}, {H_LARMR, E(-50)}, {H_HANDR, E(30)}, {H_CHEST, E(0, 0)}});
        c.root(0.f, {0, 0, 0}).root(0.3f, {0, -0.1f, 0}).root(0.8f, {0, -0.1f, 0}).root(1.f, {0, 0, 0});
        c.hit(0.3f, 0.8f).event(0.3f, EV_SOUND_WHOOSH).event(0.6f, EV_SOUND_WHOOSH).move(0.8f, 1.2f).move(1.f, 1.2f);
    }
    {
        C& c = mk(C_1H_CHARGED, "1h_thrust", 0.7f);
        c.key(0.0f, {{H_UARMR, E(-30, 10, -20)}, {H_LARMR, E(-100)}, {H_HANDR, E(90)}, {H_CHEST, E(0, -30)}, {H_SPINE, E(0, -15)}})
         .key(0.4f, {{H_UARMR, E(-20, 20, -25)}, {H_LARMR, E(-120)}, {H_CHEST, E(-5, -40)}}, Ease::Out)
         .key(0.55f, {{H_UARMR, E(-95, 5, -5)}, {H_LARMR, E(0)}, {H_HANDR, E(90)}, {H_CHEST, E(10, 20)}, {H_SPINE, E(15, 10)}}, Ease::In)
         .key(1.0f, {{H_UARMR, E(-40, 0, -15)}, {H_LARMR, E(-50)}, {H_HANDR, E(40)}, {H_CHEST, E(0)}, {H_SPINE, E(5)}});
        c.hit(0.5f, 0.68f).event(0.5f, EV_SOUND_WHOOSH).move(0.45f, -0.1f).move(0.62f, 3.2f).move(1.f, 3.3f);
    }
    // ---------- two-handed: cleave, rising, slam (+ leap)
    {
        C& c = mk(C_2H_A1, "2h_cleave", 0.75f);
        c.key(0.0f, {{H_UARMR, E(-120, -40, -40)}, {H_LARMR, E(-60)}, {H_UARML, E(-110, -60, 0)}, {H_LARML, E(-70)}, {H_HANDR, E(20, 0, 30)}, {H_CHEST, E(-10, -40)}, {H_SPINE, E(0, -15)}})
         .key(0.4f, {{H_UARMR, E(-150, -60, -30)}, {H_UARML, E(-140, -70, 10)}, {H_CHEST, E(-15, -50)}}, Ease::Out)
         .key(0.6f, {{H_UARMR, E(-40, 40, -10)}, {H_LARMR, E(-15)}, {H_UARML, E(-35, 10, 0)}, {H_LARML, E(-25)}, {H_HANDR, E(70, 0, 30)}, {H_CHEST, E(20, 35)}, {H_SPINE, E(15, 15)}}, Ease::In)
         .key(1.0f, {{H_UARMR, E(-30, 20, -10)}, {H_LARMR, E(-70)}, {H_UARML, E(-40, -30, 10)}, {H_LARML, E(-75)}, {H_HANDR, E(35, 0, 30)}, {H_CHEST, E(0, -10)}, {H_SPINE, E(0)}});
        c.root(0.6f, {0, -0.08f, 0}).root(1.f, {0, 0, 0});
        c.hit(0.47f, 0.66f).event(0.45f, EV_SOUND_WHOOSH).move(0.6f, 1.f).move(1.f, 1.1f);
        c.comboOpen = 0.6f;
    }
    {
        C& c = mk(C_2H_A2, "2h_rising", 0.7f);
        c.key(0.0f, {{H_UARMR, E(-20, 50, -10)}, {H_LARMR, E(-20)}, {H_UARML, E(-20, 20, 0)}, {H_LARML, E(-30)}, {H_HANDR, E(80, 0, 30)}, {H_CHEST, E(15, 35)}})
         .key(0.3f, {{H_UARMR, E(-5, 60, -10)}, {H_CHEST, E(20, 40)}}, Ease::Out)
         .key(0.55f, {{H_UARMR, E(-160, -30, -20)}, {H_LARMR, E(-20)}, {H_UARML, E(-150, -40, 10)}, {H_LARML, E(-30)}, {H_HANDR, E(0, 0, 30)}, {H_CHEST, E(-15, -30)}}, Ease::In)
         .key(1.0f, {{H_UARMR, E(-40, 0, -10)}, {H_LARMR, E(-70)}, {H_UARML, E(-40, -30, 10)}, {H_LARML, E(-75)}, {H_HANDR, E(35, 0, 30)}, {H_CHEST, E(0, -10)}});
        c.hit(0.35f, 0.58f).event(0.34f, EV_SOUND_WHOOSH).move(0.55f, 0.8f).move(1.f, 0.85f);
        c.comboOpen = 0.6f;
    }
    {
        C& c = mk(C_2H_A3, "2h_slam", 0.95f);
        c.key(0.0f, {{H_UARMR, E(-60, 0, -10)}, {H_UARML, E(-60, -20, 10)}, {H_LARMR, E(-60)}, {H_LARML, E(-60)}, {H_HANDR, E(20, 0, 30)}, {H_SPINE, E(0)}, {H_CHEST, E(0)}})
         .key(0.42f, {{H_UARMR, E(-175, 0, -10)}, {H_UARML, E(-175, 0, 10)}, {H_LARMR, E(-70)}, {H_LARML, E(-70)}, {H_HANDR, E(-40, 0, 30)}, {H_SPINE, E(-15)}, {H_CHEST, E(-15)}}, Ease::Out)
         .key(0.56f, {{H_UARMR, E(-25, 0, -5)}, {H_UARML, E(-25, -10, 5)}, {H_LARMR, E(0)}, {H_LARML, E(0)}, {H_HANDR, E(80, 0, 30)}, {H_SPINE, E(30)}, {H_CHEST, E(15)}}, Ease::In)
         .key(0.8f, {{H_UARMR, E(-25, 0, -5)}, {H_SPINE, E(28)}})
         .key(1.0f, {{H_UARMR, E(-40, 10, -10)}, {H_UARML, E(-40, -20, 10)}, {H_LARMR, E(-70)}, {H_LARML, E(-70)}, {H_HANDR, E(30, 0, 30)}, {H_SPINE, E(5)}, {H_CHEST, E(0)}});
        c.root(0.42f, {0, 0.05f, 0}).root(0.56f, {0, -0.22f, 0}).root(0.85f, {0, -0.2f, 0}).root(1.f, {0, 0, 0});
        c.key(0.56f, {{H_THIGHL, E(-50)}, {H_SHINL, E(70)}, {H_THIGHR, E(10)}, {H_SHINR, E(40)}});
        c.key(0.0f, {{H_THIGHL, E(0)}, {H_SHINL, E(10)}, {H_THIGHR, E(0)}, {H_SHINR, E(10)}});
        c.key(1.0f, {{H_THIGHL, E(0)}, {H_SHINL, E(10)}, {H_THIGHR, E(0)}, {H_SHINR, E(10)}});
        c.hit(0.5f, 0.62f).event(0.48f, EV_SOUND_WHOOSH).event(0.57f, EV_IMPACT).move(0.56f, 1.4f).move(1.f, 1.4f);
    }
    {
        C& c = mk(C_2H_HEAVY, "2h_leap", 1.1f);
        c.key(0.0f, {{H_UARMR, E(-40, 0, -10)}, {H_UARML, E(-40, -20, 10)}, {H_THIGHL, E(-10)}, {H_SHINL, E(40)}, {H_THIGHR, E(-10)}, {H_SHINR, E(40)}, {H_SPINE, E(20)}})
         .key(0.25f, {{H_THIGHL, E(-40)}, {H_SHINL, E(90)}, {H_THIGHR, E(-40)}, {H_SHINR, E(90)}, {H_SPINE, E(30)}})
         .key(0.5f, {{H_UARMR, E(-180, 0, -10)}, {H_UARML, E(-180, 0, 10)}, {H_LARMR, E(-60)}, {H_LARML, E(-60)}, {H_THIGHL, E(-60)}, {H_SHINL, E(100)}, {H_THIGHR, E(-20)}, {H_SHINR, E(80)}, {H_SPINE, E(-15)}}, Ease::Out)
         .key(0.66f, {{H_UARMR, E(-20, 0, -5)}, {H_UARML, E(-20, 0, 5)}, {H_LARMR, E(0)}, {H_LARML, E(0)}, {H_THIGHL, E(-60)}, {H_SHINL, E(90)}, {H_THIGHR, E(20)}, {H_SHINR, E(60)}, {H_SPINE, E(35)}}, Ease::In)
         .key(1.0f, {{H_UARMR, E(-40, 10, -10)}, {H_UARML, E(-40, -20, 10)}, {H_LARMR, E(-70)}, {H_LARML, E(-70)}, {H_THIGHL, E(0)}, {H_SHINL, E(10)}, {H_THIGHR, E(0)}, {H_SHINR, E(10)}, {H_SPINE, E(5)}});
        c.root(0.f, {0, 0, 0}).root(0.25f, {0, -0.25f, 0}).root(0.5f, {0, 1.4f, 0}).root(0.66f, {0, -0.3f, 0}).root(0.85f, {0, -0.3f, 0}).root(1.f, {0, 0, 0});
        c.hit(0.62f, 0.72f).event(0.66f, EV_IMPACT).event(0.3f, EV_SOUND_WHOOSH).move(0.25f, 0).move(0.66f, 4.f).move(1.f, 4.f);
    }
    // ---------- daggers
    {
        C& c = mk(C_DG_A1, "dagger_stab", 0.32f);
        c.key(0.0f, {{H_UARMR, E(-20, 0, -20)}, {H_LARMR, E(-110)}, {H_HANDR, E(80)}, {H_CHEST, E(0, -20)}})
         .key(0.45f, {{H_UARMR, E(-90, 10, -10)}, {H_LARMR, E(-5)}, {H_HANDR, E(90)}, {H_CHEST, E(5, 15)}}, Ease::Out)
         .key(1.0f, {{H_UARMR, E(-20, 0, -18)}, {H_LARMR, E(-70)}, {H_HANDR, E(70)}, {H_CHEST, E(0)}});
        c.hit(0.3f, 0.55f).event(0.25f, EV_SOUND_WHOOSH).move(0.45f, 0.5f).move(1.f, 0.5f);
        c.comboOpen = 0.4f;
    }
    {
        C& c = mk(C_DG_A2, "dagger_slash", 0.34f);
        c.key(0.0f, {{H_UARMR, E(-60, 60, -10)}, {H_LARMR, E(-60)}, {H_HANDR, E(90)}, {H_CHEST, E(0, 25)}})
         .key(0.5f, {{H_UARMR, E(-70, -60, -10)}, {H_LARMR, E(-10)}, {H_CHEST, E(0, -25)}}, Ease::Out)
         .key(1.0f, {{H_UARMR, E(-20, 0, -18)}, {H_LARMR, E(-70)}, {H_HANDR, E(70)}, {H_CHEST, E(0)}});
        c.hit(0.25f, 0.55f).event(0.2f, EV_SOUND_WHOOSH).move(0.5f, 0.4f).move(1.f, 0.4f);
        c.comboOpen = 0.4f;
    }
    {
        C& c = mk(C_DG_A3, "dagger_flurry", 0.6f);
        for (int i = 0; i < 3; i++) {
            float t0 = i / 3.f;
            c.key(t0 + 0.0f, {{H_UARMR, E(-30, 0, -20)}, {H_LARMR, E(-110)}, {H_HANDR, E(80)}});
            c.key(t0 + 0.18f, {{H_UARMR, E(-90, (i - 1) * 25.f, -10)}, {H_LARMR, E(-5)}}, Ease::Out);
        }
        c.key(1.0f, {{H_UARMR, E(-20, 0, -18)}, {H_LARMR, E(-70)}, {H_HANDR, E(70)}});
        c.hit(0.1f, 0.85f).event(0.1f, EV_SOUND_WHOOSH).event(0.43f, EV_SOUND_WHOOSH).event(0.76f, EV_SOUND_WHOOSH).move(1.f, 1.0f);
    }
    {
        C& c = mk(C_DG_HEAVY, "dagger_shadow_strike", 0.75f);
        c.key(0.0f, {{H_SPINE, E(30)}, {H_THIGHL, E(-50)}, {H_SHINL, E(90)}, {H_THIGHR, E(-10)}, {H_SHINR, E(80)}, {H_UARMR, E(10, 0, -30)}, {H_LARMR, E(-90)}})
         .key(0.45f, {{H_SPINE, E(35)}, {H_UARMR, E(-100, 0, -5)}, {H_LARMR, E(0)}, {H_HANDR, E(90)}}, Ease::In)
         .key(1.0f, {{H_SPINE, E(5)}, {H_THIGHL, E(0)}, {H_SHINL, E(10)}, {H_THIGHR, E(0)}, {H_SHINR, E(10)}, {H_UARMR, E(-20, 0, -18)}, {H_LARMR, E(-70)}});
        c.root(0.f, {0, -0.3f, 0}).root(0.45f, {0, -0.2f, 0}).root(1.f, {0, 0, 0});
        c.hit(0.4f, 0.6f).event(0.38f, EV_SOUND_WHOOSH).move(0.45f, 5.f).move(1.f, 5.2f);
    }
    // ---------- spear
    {
        C& c = mk(C_SP_A1, "spear_thrust", 0.5f);
        c.key(0.0f, {{H_UARMR, E(-10, 0, -20)}, {H_LARMR, E(-110)}, {H_HANDR, E(60)}, {H_UARML, E(-50, -20, 8)}, {H_LARML, E(-40)}})
         .key(0.45f, {{H_UARMR, E(-80, 0, -10)}, {H_LARMR, E(-10)}, {H_UARML, E(-80, -10, 5)}, {H_LARML, E(-5)}, {H_CHEST, E(10, 10)}}, Ease::Out)
         .key(1.0f, {{H_UARMR, E(-25, 0, -18)}, {H_LARMR, E(-80)}, {H_HANDR, E(60)}, {H_UARML, E(-45, -20, 8)}, {H_LARML, E(-40)}, {H_CHEST, E(0)}});
        c.hit(0.3f, 0.55f).event(0.28f, EV_SOUND_WHOOSH).move(0.45f, 0.6f).move(1.f, 0.6f);
        c.comboOpen = 0.55f;
    }
    {
        C& c = mk(C_SP_A2, "spear_sweep", 0.65f);
        c.key(0.0f, {{H_UARMR, E(-70, -50, -10)}, {H_LARMR, E(-40)}, {H_HANDR, E(80)}, {H_CHEST, E(0, -40)}})
         .key(0.55f, {{H_UARMR, E(-80, 70, -10)}, {H_LARMR, E(-10)}, {H_CHEST, E(0, 45)}}, Ease::Out)
         .key(1.0f, {{H_UARMR, E(-25, 0, -18)}, {H_LARMR, E(-80)}, {H_HANDR, E(60)}, {H_CHEST, E(0)}});
        c.hit(0.25f, 0.6f).event(0.22f, EV_SOUND_WHOOSH).move(1.f, 0.4f);
    }
    {
        C& c = mk(C_SP_HEAVY, "spear_vault", 0.9f);
        c.key(0.0f, {{H_UARMR, E(-150, 0, -10)}, {H_LARMR, E(-40)}, {H_HANDR, E(-60)}, {H_SPINE, E(-10)}})
         .key(0.5f, {{H_UARMR, E(-160, 0, -10)}, {H_SPINE, E(-15)}})
         .key(0.62f, {{H_UARMR, E(-40, 0, -10)}, {H_LARMR, E(0)}, {H_HANDR, E(90)}, {H_SPINE, E(30)}}, Ease::In)
         .key(1.0f, {{H_UARMR, E(-25, 0, -18)}, {H_LARMR, E(-80)}, {H_HANDR, E(60)}, {H_SPINE, E(5)}});
        c.root(0.f, {0, 0, 0}).root(0.45f, {0, 1.2f, 0}).root(0.62f, {0, -0.2f, 0}).root(1.f, {0, 0, 0});
        c.hit(0.58f, 0.72f).event(0.62f, EV_IMPACT).move(0.62f, 4.5f).move(1.f, 4.5f);
    }
    // ---------- bow
    {
        C& c = mk(C_BOW_SHOT, "bow_shot", 0.62f);
        c.key(0.0f, {{H_CHEST, E(0, 55)}, {H_HEAD, E(0, -50)}, {H_UARML, E(-88, -55, 0)}, {H_LARML, E(0)}, {H_HANDL, E(0, 0, -90)}, {H_UARMR, E(-88, 40, -10)}, {H_LARMR, E(-110)}})
         .key(0.45f, {{H_UARMR, E(-90, 30, -25)}, {H_LARMR, E(-145)}})
         .key(0.55f, {{H_UARMR, E(-80, 20, -40)}, {H_LARMR, E(-60)}}, Ease::Out)
         .key(1.0f, {{H_CHEST, E(0, 20)}, {H_HEAD, E(0, -15)}, {H_UARML, E(-30, -20, 10)}, {H_LARML, E(-20)}, {H_HANDL, E(0, 90, 0)}, {H_UARMR, E(-20, 0, -15)}, {H_LARMR, E(-40)}});
        c.event(0.5f, EV_RELEASE).hit(-1, -1);
        c.comboOpen = 0.6f;
    }
    {
        C& c = mk(C_BOW_HEAVY, "bow_power", 1.1f);
        c.key(0.0f, {{H_CHEST, E(0, 55)}, {H_HEAD, E(0, -50)}, {H_UARML, E(-88, -55, 0)}, {H_LARML, E(0)}, {H_HANDL, E(0, 0, -90)}, {H_UARMR, E(-88, 40, -10)}, {H_LARMR, E(-110)}, {H_THIGHL, E(-30)}, {H_SHINL, E(40)}})
         .key(0.75f, {{H_UARMR, E(-92, 25, -30)}, {H_LARMR, E(-155)}})
         .key(0.8f, {{H_UARMR, E(-75, 15, -45)}, {H_LARMR, E(-50)}}, Ease::Out)
         .key(1.0f, {{H_CHEST, E(0, 20)}, {H_HEAD, E(0, -15)}, {H_UARML, E(-30, -20, 10)}, {H_LARML, E(-20)}, {H_HANDL, E(0, 90, 0)}, {H_UARMR, E(-20, 0, -15)}, {H_LARMR, E(-40)}, {H_THIGHL, E(0)}, {H_SHINL, E(10)}});
        c.event(0.78f, EV_RELEASE).move(0.8f, -0.4f).move(1.f, -0.5f);
    }
    // ---------- staff & spells
    {
        C& c = mk(C_STAFF_CAST, "staff_cast", 0.6f);
        c.key(0.0f, {{H_UARMR, E(-40, 0, -15)}, {H_LARMR, E(-90)}, {H_UARML, E(-30, 0, 15)}, {H_LARML, E(-50)}, {H_CHEST, E(-5, -15)}})
         .key(0.4f, {{H_UARMR, E(-120, 0, -15)}, {H_LARMR, E(-40)}, {H_UARML, E(-90, -20, 10)}, {H_LARML, E(-10)}, {H_CHEST, E(-10, -10)}}, Ease::Out)
         .key(0.55f, {{H_UARMR, E(-80, 0, -10)}, {H_LARMR, E(-30)}, {H_UARML, E(-90, 10, 5)}, {H_LARML, E(0)}, {H_CHEST, E(8, 10)}}, Ease::In)
         .key(1.0f, {{H_UARMR, E(-15, 0, -12)}, {H_LARMR, E(-80)}, {H_UARML, E(-10, 0, 10)}, {H_LARML, E(-20)}, {H_CHEST, E(0)}});
        c.event(0.5f, EV_RELEASE);
        c.comboOpen = 0.6f;
    }
    {
        C& c = mk(C_STAFF_HEAVY, "staff_slam", 1.0f);
        c.key(0.0f, {{H_UARMR, E(-40, 0, -15)}, {H_LARMR, E(-90)}, {H_UARML, E(-40, 0, 15)}, {H_LARML, E(-60)}})
         .key(0.45f, {{H_UARMR, E(-170, 0, -10)}, {H_LARMR, E(-20)}, {H_UARML, E(-170, 0, 10)}, {H_LARML, E(-20)}, {H_SPINE, E(-12)}}, Ease::Out)
         .key(0.6f, {{H_UARMR, E(-60, 0, -10)}, {H_LARMR, E(-70)}, {H_UARML, E(-60, 0, 10)}, {H_LARML, E(-70)}, {H_SPINE, E(25)}}, Ease::In)
         .key(1.0f, {{H_UARMR, E(-15, 0, -12)}, {H_LARMR, E(-80)}, {H_UARML, E(-10, 0, 10)}, {H_LARML, E(-20)}, {H_SPINE, E(3)}});
        c.root(0.6f, {0, -0.15f, 0}).root(1.f, {0, 0, 0});
        c.event(0.6f, EV_IMPACT).hit(0.58f, 0.66f);
    }
    {
        C& c = mk(C_CAST_FORWARD, "cast_forward", 0.55f);
        c.key(0.0f, {{H_UARML, E(-20, 0, 20)}, {H_LARML, E(-90)}, {H_CHEST, E(0, 25)}})
         .key(0.4f, {{H_UARML, E(-60, 30, 30)}, {H_LARML, E(-110)}, {H_CHEST, E(-5, 35)}}, Ease::Out)
         .key(0.55f, {{H_UARML, E(-95, -10, 0)}, {H_LARML, E(0)}, {H_HANDL, E(-60)}, {H_CHEST, E(8, -10)}}, Ease::In)
         .key(1.0f, {{H_UARML, E(-15, 0, 10)}, {H_LARML, E(-30)}, {H_HANDL, E(0)}, {H_CHEST, E(0)}});
        c.event(0.52f, EV_RELEASE);
    }
    {
        C& c = mk(C_CAST_AREA, "cast_area", 0.9f);
        c.key(0.0f, {{H_UARML, E(-20, 0, 20)}, {H_UARMR, E(-20, 0, -20)}, {H_SPINE, E(0)}})
         .key(0.45f, {{H_UARML, E(-170, 0, 25)}, {H_UARMR, E(-170, 0, -25)}, {H_LARML, E(-10)}, {H_LARMR, E(-10)}, {H_SPINE, E(-15)}, {H_HEAD, E(-20)}}, Ease::Out)
         .key(0.62f, {{H_UARML, E(-40, 0, 50)}, {H_UARMR, E(-40, 0, -50)}, {H_LARML, E(-5)}, {H_LARMR, E(-5)}, {H_SPINE, E(30)}, {H_HEAD, E(10)}}, Ease::In)
         .key(1.0f, {{H_UARML, E(-10, 0, 10)}, {H_UARMR, E(-10, 0, -10)}, {H_LARML, E(-20)}, {H_LARMR, E(-20)}, {H_SPINE, E(3)}, {H_HEAD, E(0)}});
        c.root(0.62f, {0, -0.18f, 0}).root(1.f, {0, 0, 0});
        c.event(0.62f, EV_RELEASE);
    }
    {
        C& c = mk(C_CAST_SELF, "cast_self", 0.8f);
        c.key(0.0f, {{H_UARML, E(-20, 0, 20)}, {H_UARMR, E(-20, 0, -20)}, {H_CHEST, E(0)}})
         .key(0.4f, {{H_UARML, E(-60, -50, 10)}, {H_LARML, E(-100)}, {H_UARMR, E(-60, 50, -10)}, {H_LARMR, E(-100)}, {H_CHEST, E(5)}, {H_HEAD, E(15)}})
         .key(0.6f, {{H_UARML, E(-80, 0, 70)}, {H_LARML, E(-10)}, {H_UARMR, E(-80, 0, -70)}, {H_LARMR, E(-10)}, {H_CHEST, E(-12)}, {H_HEAD, E(-25)}}, Ease::Out)
         .key(1.0f, {{H_UARML, E(-10, 0, 10)}, {H_UARMR, E(-10, 0, -10)}, {H_LARML, E(-20)}, {H_LARMR, E(-20)}, {H_CHEST, E(0)}, {H_HEAD, E(0)}});
        c.event(0.55f, EV_RELEASE);
    }
    {
        C& c = mk(C_FIST_A1, "jab", 0.35f);
        c.key(0.0f, {{H_UARMR, E(-30, 20, -20)}, {H_LARMR, E(-120)}, {H_CHEST, E(0, -15)}})
         .key(0.4f, {{H_UARMR, E(-90, 10, -5)}, {H_LARMR, E(-5)}, {H_CHEST, E(5, 20)}}, Ease::Out)
         .key(1.0f, {{H_UARMR, E(-20, 0, -15)}, {H_LARMR, E(-60)}, {H_CHEST, E(0)}});
        c.hit(0.25f, 0.5f).move(0.4f, 0.3f).move(1.f, 0.3f);
        c.comboOpen = 0.4f;
    }
    {
        C& c = mk(C_FIST_A2, "hook", 0.4f);
        c.key(0.0f, {{H_UARML, E(-40, -20, 40)}, {H_LARML, E(-110)}, {H_CHEST, E(0, 25)}})
         .key(0.45f, {{H_UARML, E(-85, -60, 20)}, {H_LARML, E(-60)}, {H_CHEST, E(5, -30)}}, Ease::Out)
         .key(1.0f, {{H_UARML, E(-20, 0, 15)}, {H_LARML, E(-60)}, {H_CHEST, E(0)}});
        c.hit(0.3f, 0.55f).move(0.45f, 0.3f).move(1.f, 0.3f);
    }
    // ---------- defense
    {
        C& c = mk(C_PARRY, "parry", 0.35f);
        c.key(0.0f, {{H_UARMR, E(-40, 0, -15)}, {H_LARMR, E(-60)}, {H_HANDR, E(30)}})
         .key(0.3f, {{H_UARMR, E(-100, 40, -30)}, {H_LARMR, E(-90)}, {H_HANDR, E(0, 0, -80)}, {H_CHEST, E(-8, 20)}}, Ease::Out)
         .key(1.0f, {{H_UARMR, E(-40, 0, -15)}, {H_LARMR, E(-60)}, {H_HANDR, E(30)}, {H_CHEST, E(0)}});
        c.event(0.2f, EV_IMPACT);
    }
    {
        C& c = mk(C_RIPOSTE, "riposte", 0.55f);
        c.key(0.0f, {{H_UARMR, E(-100, 40, -30)}, {H_LARMR, E(-90)}, {H_HANDR, E(0, 0, -80)}, {H_CHEST, E(-8, 20)}})
         .key(0.35f, {{H_UARMR, E(-95, -20, -10)}, {H_LARMR, E(0)}, {H_HANDR, E(90)}, {H_CHEST, E(12, -25)}, {H_SPINE, E(12)}}, Ease::In)
         .key(1.0f, {{H_UARMR, E(-35, 0, -15)}, {H_LARMR, E(-55)}, {H_HANDR, E(30)}, {H_CHEST, E(0)}, {H_SPINE, E(4)}});
        c.hit(0.25f, 0.45f).event(0.24f, EV_SOUND_WHOOSH).move(0.35f, 1.0f).move(1.f, 1.0f);
    }
    {
        C& c = mk(C_SHIELD_BASH, "shield_bash", 0.5f);
        c.key(0.0f, {{H_UARML, E(-65, -35, 25)}, {H_LARML, E(-95)}, {H_CHEST, E(0, 25)}})
         .key(0.4f, {{H_UARML, E(-85, 10, 30)}, {H_LARML, E(-60)}, {H_CHEST, E(8, -25)}, {H_SPINE, E(10)}}, Ease::Out)
         .key(1.0f, {{H_UARML, E(-20, 0, 15)}, {H_LARML, E(-40)}, {H_CHEST, E(0)}, {H_SPINE, E(3)}});
        c.hit(0.25f, 0.5f).event(0.4f, EV_IMPACT).move(0.4f, 1.2f).move(1.f, 1.2f);
    }
    // ---------- movement actions
    {
        C& c = mk(C_ROLL, "roll", 0.62f);
        quat q0 = E(0), q1 = E(90), q2 = E(180), q3 = E(270), q4 = E(359.9f);
        c.key(0.0f, {{H_ROOT, q0}}, Ease::Linear).key(0.25f, {{H_ROOT, q1}}, Ease::Linear).key(0.45f, {{H_ROOT, q2}}, Ease::Linear).key(0.65f, {{H_ROOT, q3}}, Ease::Linear).key(0.82f, {{H_ROOT, q4}}, Ease::Out).key(1.0f, {{H_ROOT, q4}});
        c.key(0.0f, {{H_THIGHL, E(-40)}, {H_SHINL, E(60)}, {H_THIGHR, E(-20)}, {H_SHINR, E(50)}, {H_SPINE, E(30)}, {H_UARML, E(-60, 0, 10)}, {H_UARMR, E(-60, 0, -10)}, {H_LARML, E(-90)}, {H_LARMR, E(-90)}, {H_NECK, E(30)}})
         .key(0.2f, {{H_THIGHL, E(-120)}, {H_SHINL, E(140)}, {H_THIGHR, E(-120)}, {H_SHINR, E(140)}, {H_SPINE, E(45)}, {H_NECK, E(40)}})
         .key(0.75f, {{H_THIGHL, E(-110)}, {H_SHINL, E(130)}, {H_THIGHR, E(-100)}, {H_SHINR, E(130)}, {H_SPINE, E(40)}})
         .key(1.0f, {{H_THIGHL, E(-10)}, {H_SHINL, E(20)}, {H_THIGHR, E(-5)}, {H_SHINR, E(15)}, {H_SPINE, E(8)}, {H_UARML, E(-10, 0, 10)}, {H_UARMR, E(-10, 0, -10)}, {H_LARML, E(-30)}, {H_LARMR, E(-30)}, {H_NECK, E(0)}});
        c.root(0.f, {0, -0.2f, 0}).root(0.25f, {0, -0.45f, 0}).root(0.5f, {0, -0.35f, 0}).root(0.75f, {0, -0.45f, 0}).root(1.f, {0, 0, 0});
        c.move(0.1f, 0.4f).move(0.85f, 4.0f).move(1.f, 4.2f);
        c.cancelable = 0.85f;
    }
    {
        C& c = mk(C_DODGE_BACK, "backstep", 0.42f);
        c.key(0.0f, {{H_SPINE, E(10)}, {H_THIGHL, E(-10)}, {H_SHINL, E(30)}})
         .key(0.35f, {{H_SPINE, E(-12)}, {H_THIGHL, E(-35)}, {H_SHINL, E(50)}, {H_THIGHR, E(15)}, {H_SHINR, E(30)}, {H_UARML, E(-30, 0, 30)}, {H_UARMR, E(-30, 0, -30)}})
         .key(1.0f, {{H_SPINE, E(5)}, {H_THIGHL, E(0)}, {H_SHINL, E(10)}, {H_THIGHR, E(0)}, {H_SHINR, E(10)}, {H_UARML, E(-10, 0, 10)}, {H_UARMR, E(-10, 0, -10)}});
        c.root(0.35f, {0, 0.15f, 0}).root(1.f, {0, 0, 0});
        c.move(0.6f, -2.6f).move(1.f, -2.8f);
        c.cancelable = 0.8f;
    }
    {
        C& c = mk(C_LAND, "land", 0.35f);
        c.key(0.0f, {{H_THIGHL, E(-60)}, {H_SHINL, E(100)}, {H_THIGHR, E(-55)}, {H_SHINR, E(95)}, {H_SPINE, E(25)}, {H_FOOTL, E(-35)}, {H_FOOTR, E(-35)}})
         .key(1.0f, {{H_THIGHL, E(-5)}, {H_SHINL, E(10)}, {H_THIGHR, E(-5)}, {H_SHINR, E(10)}, {H_SPINE, E(4)}, {H_FOOTL, E(0)}, {H_FOOTR, E(0)}}, Ease::Out);
        c.root(0.f, {0, -0.32f, 0}).root(1.f, {0, 0, 0});
        c.cancelable = 0.4f;
    }
    {
        C& c = mk(C_VAULT, "vault", 0.6f);
        c.key(0.0f, {{H_UARML, E(-80, 0, 20)}, {H_UARMR, E(-80, 0, -20)}, {H_THIGHL, E(-30)}, {H_SHINL, E(40)}})
         .key(0.45f, {{H_UARML, E(-20, 0, 50)}, {H_UARMR, E(-20, 0, -50)}, {H_THIGHL, E(-90)}, {H_SHINL, E(110)}, {H_THIGHR, E(-70)}, {H_SHINR, E(90)}, {H_SPINE, E(25)}})
         .key(1.0f, {{H_UARML, E(-10, 0, 10)}, {H_UARMR, E(-10, 0, -10)}, {H_THIGHL, E(0)}, {H_SHINL, E(10)}, {H_THIGHR, E(0)}, {H_SHINR, E(10)}, {H_SPINE, E(5)}});
        c.move(1.f, 2.2f);
    }
    // ---------- reactions
    {
        C& c = mk(C_HIT_LIGHT, "hit_light", 0.32f);
        c.key(0.0f, {{H_SPINE, E(0)}, {H_CHEST, E(0)}, {H_HEAD, E(0)}})
         .key(0.25f, {{H_SPINE, E(-14, 8, 4)}, {H_CHEST, E(-10, 6)}, {H_HEAD, E(-18, -10)}}, Ease::Out)
         .key(1.0f, {{H_SPINE, E(3)}, {H_CHEST, E(0)}, {H_HEAD, E(0)}});
        c.move(0.3f, -0.25f).move(1.f, -0.3f);
        c.cancelable = 0.6f;
    }
    {
        C& c = mk(C_HIT_HEAVY, "hit_heavy", 0.75f);
        c.key(0.0f, {{H_SPINE, E(0)}, {H_CHEST, E(0)}, {H_HEAD, E(0)}, {H_UARML, E(-10, 0, 10)}, {H_UARMR, E(-10, 0, -10)}})
         .key(0.2f, {{H_SPINE, E(-25, 10, 8)}, {H_CHEST, E(-15)}, {H_HEAD, E(-25)}, {H_UARML, E(-50, 0, 60)}, {H_UARMR, E(-50, 0, -60)}, {H_THIGHL, E(-30)}, {H_SHINL, E(40)}}, Ease::Out)
         .key(0.55f, {{H_SPINE, E(20)}, {H_CHEST, E(10)}, {H_HEAD, E(10)}, {H_THIGHL, E(-20)}, {H_SHINL, E(60)}, {H_THIGHR, E(-10)}, {H_SHINR, E(50)}})
         .key(1.0f, {{H_SPINE, E(4)}, {H_CHEST, E(0)}, {H_HEAD, E(0)}, {H_UARML, E(-10, 0, 10)}, {H_UARMR, E(-10, 0, -10)}, {H_THIGHL, E(0)}, {H_SHINL, E(10)}, {H_THIGHR, E(0)}, {H_SHINR, E(10)}});
        c.root(0.55f, {0, -0.15f, 0}).root(1.f, {0, 0, 0});
        c.move(0.25f, -1.2f).move(1.f, -1.4f);
    }
    {
        C& c = mk(C_DEATH, "death", 1.3f);
        c.key(0.0f, {{H_ROOT, E(0)}, {H_SPINE, E(0)}, {H_HEAD, E(0)}, {H_THIGHL, E(0)}, {H_SHINL, E(10)}, {H_THIGHR, E(0)}, {H_SHINR, E(10)}})
         .key(0.3f, {{H_SPINE, E(25)}, {H_HEAD, E(20)}, {H_THIGHL, E(-40)}, {H_SHINL, E(90)}, {H_THIGHR, E(-30)}, {H_SHINR, E(80)}, {H_UARML, E(-20, 0, 30)}, {H_UARMR, E(-20, 0, -30)}}, Ease::Out)
         .key(0.42f, {{H_ROOT, E(-15)}})
         .key(0.75f, {{H_ROOT, E(-86, 8, 3)}, {H_SPINE, E(0)}, {H_NECK, E(10)}, {H_HEAD, E(10, 35)}, {H_UARML, E(-10, 0, 60)}, {H_LARML, E(-20)}, {H_UARMR, E(-30, 0, -45)}, {H_LARMR, E(-35)}, {H_THIGHL, E(-10, 0, 8)}, {H_SHINL, E(20)}, {H_THIGHR, E(-25, 0, -6)}, {H_SHINR, E(45)}}, Ease::In)
         .key(0.85f, {{H_ROOT, E(-80, 8, 3)}}, Ease::Out)
         .key(1.0f, {{H_ROOT, E(-86, 8, 3)}});
        c.root(0.f, {0, 0, 0}).root(0.3f, {0, -0.35f, 0}).root(0.75f, {0, -0.84f, -0.25f}).root(0.85f, {0, -0.8f, -0.25f}).root(1.f, {0, -0.84f, -0.25f});
        c.event(0.75f, EV_IMPACT);
        c.cancelable = 2.f;
    }
    // ---------- interactions & life
    {
        C& c = mk(C_INTERACT, "interact", 0.6f);
        c.key(0.0f, {{H_UARMR, E(-10, 0, -10)}, {H_LARMR, E(-30)}})
         .key(0.45f, {{H_UARMR, E(-75, 10, -5)}, {H_LARMR, E(-15)}, {H_HANDR, E(-30)}, {H_SPINE, E(15)}})
         .key(1.0f, {{H_UARMR, E(-10, 0, -10)}, {H_LARMR, E(-30)}, {H_HANDR, E(0)}, {H_SPINE, E(3)}});
        c.event(0.45f, EV_HIT);
    }
    {
        C& c = mk(C_PICKUP, "pickup", 0.8f);
        c.key(0.0f, {{H_SPINE, E(0)}, {H_THIGHL, E(0)}, {H_SHINL, E(10)}, {H_THIGHR, E(0)}, {H_SHINR, E(10)}})
         .key(0.45f, {{H_SPINE, E(35)}, {H_CHEST, E(15)}, {H_THIGHL, E(-70)}, {H_SHINL, E(110)}, {H_THIGHR, E(-20)}, {H_SHINR, E(90)}, {H_FOOTL, E(-30)}, {H_UARMR, E(-50, 0, -10)}, {H_LARMR, E(-10)}})
         .key(1.0f, {{H_SPINE, E(4)}, {H_CHEST, E(0)}, {H_THIGHL, E(0)}, {H_SHINL, E(10)}, {H_THIGHR, E(0)}, {H_SHINR, E(10)}, {H_FOOTL, E(0)}, {H_UARMR, E(-10, 0, -10)}, {H_LARMR, E(-30)}});
        c.root(0.45f, {0, -0.42f, 0}).root(1.f, {0, 0, 0});
        c.event(0.45f, EV_HIT);
    }
    {
        C& c = mk(C_OPEN_CHEST, "open_chest", 1.2f);
        c.key(0.0f, {{H_SPINE, E(0)}, {H_THIGHL, E(0)}, {H_SHINL, E(10)}, {H_THIGHR, E(0)}, {H_SHINR, E(10)}})
         .key(0.3f, {{H_SPINE, E(30)}, {H_THIGHL, E(-80)}, {H_SHINL, E(120)}, {H_THIGHR, E(10)}, {H_SHINR, E(110)}, {H_UARML, E(-60, 0, 10)}, {H_UARMR, E(-60, 0, -10)}, {H_LARML, E(-20)}, {H_LARMR, E(-20)}})
         .key(0.6f, {{H_SPINE, E(10)}, {H_UARML, E(-110, 0, 10)}, {H_UARMR, E(-110, 0, -10)}, {H_LARML, E(-40)}, {H_LARMR, E(-40)}, {H_HEAD, E(-10)}})
         .key(1.0f, {{H_SPINE, E(4)}, {H_THIGHL, E(0)}, {H_SHINL, E(10)}, {H_THIGHR, E(0)}, {H_SHINR, E(10)}, {H_UARML, E(-10, 0, 10)}, {H_UARMR, E(-10, 0, -10)}, {H_LARML, E(-30)}, {H_LARMR, E(-30)}, {H_HEAD, E(0)}});
        c.root(0.3f, {0, -0.45f, 0}).root(0.6f, {0, -0.4f, 0}).root(1.f, {0, 0, 0});
        c.event(0.55f, EV_HIT);
    }
    {
        C& c = mk(C_GATHER_SWING, "gather_swing", 1.3f);
        for (int i = 0; i < 2; i++) {
            float t0 = i * 0.5f;
            c.key(t0, {{H_UARMR, E(-150, 0, -15)}, {H_LARMR, E(-60)}, {H_HANDR, E(-20)}, {H_UARML, E(-120, 0, 15)}, {H_LARML, E(-60)}, {H_SPINE, E(-10)}}, Ease::Out);
            c.key(t0 + 0.18f, {{H_UARMR, E(-55, 0, -10)}, {H_LARMR, E(-10)}, {H_HANDR, E(70)}, {H_UARML, E(-55, 0, 10)}, {H_LARML, E(-10)}, {H_SPINE, E(30)}}, Ease::In);
            c.event(t0 + 0.18f, EV_HIT);
        }
        c.key(1.0f, {{H_UARMR, E(-12, 0, -12)}, {H_LARMR, E(-35)}, {H_HANDR, E(80)}, {H_UARML, E(-10, 0, 10)}, {H_LARML, E(-20)}, {H_SPINE, E(4)}});
    }
    {
        C& c = mk(C_HERB_PICK, "herb_pick", 1.0f);
        c.key(0.0f, {{H_SPINE, E(0)}, {H_THIGHL, E(0)}, {H_SHINL, E(10)}})
         .key(0.3f, {{H_SPINE, E(30)}, {H_THIGHL, E(-80)}, {H_SHINL, E(130)}, {H_THIGHR, E(20)}, {H_SHINR, E(120)}, {H_UARMR, E(-55, 0, -10)}, {H_LARMR, E(-10)}})
         .key(0.55f, {{H_UARMR, E(-40, 0, -10)}, {H_LARMR, E(-60)}})
         .key(0.75f, {{H_UARMR, E(-55, 0, -10)}, {H_LARMR, E(-10)}})
         .key(1.0f, {{H_SPINE, E(4)}, {H_THIGHL, E(0)}, {H_SHINL, E(10)}, {H_THIGHR, E(0)}, {H_SHINR, E(10)}, {H_UARMR, E(-10, 0, -10)}, {H_LARMR, E(-30)}});
        c.root(0.3f, {0, -0.5f, 0}).root(0.75f, {0, -0.5f, 0}).root(1.f, {0, 0, 0});
        c.event(0.55f, EV_HIT);
    }
    {
        C& c = mk(C_FISH_CAST, "fish_cast", 1.0f);
        c.key(0.0f, {{H_UARMR, E(-20, 0, -15)}, {H_LARMR, E(-40)}, {H_HANDR, E(80)}})
         .key(0.35f, {{H_UARMR, E(-170, 0, -10)}, {H_LARMR, E(-30)}, {H_HANDR, E(40)}, {H_SPINE, E(-10)}}, Ease::Out)
         .key(0.55f, {{H_UARMR, E(-70, 0, -10)}, {H_LARMR, E(-10)}, {H_HANDR, E(80)}, {H_SPINE, E(10)}}, Ease::In)
         .key(1.0f, {{H_UARMR, E(-55, 0, -10)}, {H_LARMR, E(-40)}, {H_HANDR, E(70)}, {H_SPINE, E(5)}});
        c.event(0.55f, EV_RELEASE);
    }
    {
        C& c = mk(C_DRINK, "drink", 0.9f);
        c.key(0.0f, {{H_UARML, E(-10, 0, 10)}, {H_LARML, E(-30)}})
         .key(0.35f, {{H_UARML, E(-40, -40, 5)}, {H_LARML, E(-140)}, {H_HEAD, E(-25)}})
         .key(0.7f, {{H_UARML, E(-45, -40, 5)}, {H_LARML, E(-145)}, {H_HEAD, E(-30)}})
         .key(1.0f, {{H_UARML, E(-10, 0, 10)}, {H_LARML, E(-30)}, {H_HEAD, E(0)}});
        c.event(0.45f, EV_DRINK);
    }
    {
        C& c = mk(C_MOUNT, "mount", 0.7f);
        c.key(0.0f, {{H_THIGHL, E(0)}, {H_SHINL, E(10)}, {H_UARML, E(-10, 0, 10)}})
         .key(0.4f, {{H_THIGHL, E(-100, 0, 30)}, {H_SHINL, E(100)}, {H_UARML, E(-120, 0, 20)}, {H_UARMR, E(-110, 0, -20)}, {H_SPINE, E(20)}})
         .key(1.0f, {{H_THIGHL, E(-75, 18, 28)}, {H_SHINL, E(80)}, {H_UARML, E(-35, 0, 8)}, {H_UARMR, E(-35, 0, -8)}, {H_SPINE, E(6)}});
        c.root(0.f, {0, 0, 0}).root(0.4f, {0, 0.5f, 0}).root(1.f, {0, 0, 0});
    }
    {
        C& c = mk(C_DISMOUNT, "dismount", 0.6f);
        c.key(0.0f, {{H_THIGHL, E(-75, 18, 28)}, {H_SHINL, E(80)}})
         .key(0.5f, {{H_THIGHL, E(-30, 0, 40)}, {H_SHINL, E(40)}, {H_UARML, E(-60, 0, 40)}, {H_UARMR, E(-60, 0, -40)}})
         .key(1.0f, {{H_THIGHL, E(0)}, {H_SHINL, E(10)}, {H_UARML, E(-10, 0, 10)}, {H_UARMR, E(-10, 0, -10)}});
    }
    {
        C& c = mk(C_WAVE, "wave", 1.6f);
        c.key(0.0f, {{H_UARMR, E(-10, 0, -10)}, {H_LARMR, E(-20)}})
         .key(0.2f, {{H_UARMR, E(-150, 0, -40)}, {H_LARMR, E(-30)}, {H_HANDR, E(0, 0, 20)}})
         .key(0.35f, {{H_HANDR, E(0, 0, -25)}}).key(0.5f, {{H_HANDR, E(0, 0, 25)}}).key(0.65f, {{H_HANDR, E(0, 0, -25)}})
         .key(0.8f, {{H_UARMR, E(-150, 0, -40)}, {H_HANDR, E(0, 0, 0)}})
         .key(1.0f, {{H_UARMR, E(-10, 0, -10)}, {H_LARMR, E(-20)}});
    }
    {
        C& c = mk(C_TALK, "talk", 2.4f, true);
        c.key(0.0f, {{H_UARMR, E(-20, 0, -15)}, {H_LARMR, E(-70)}, {H_HANDR, E(0, 0, 0)}, {H_HEAD, E(0, 0, 0)}})
         .key(0.25f, {{H_UARMR, E(-35, 25, -25)}, {H_LARMR, E(-90)}, {H_HANDR, E(0, 40, 0)}, {H_HEAD, E(-5, 8, 3)}})
         .key(0.55f, {{H_UARML, E(-30, -20, 25)}, {H_LARML, E(-80)}, {H_HEAD, E(3, -6, -2)}})
         .key(0.8f, {{H_UARMR, E(-25, 10, -18)}, {H_LARMR, E(-75)}, {H_UARML, E(-10, 0, 10)}, {H_LARML, E(-25)}, {H_HEAD, E(0, 4, 0)}});
    }
    {
        C& c = mk(C_HAMMER_WORK, "hammer_work", 0.9f, true);
        c.key(0.0f, {{H_UARMR, E(-130, 0, -15)}, {H_LARMR, E(-70)}, {H_HANDR, E(20)}, {H_SPINE, E(15)}})
         .key(0.4f, {{H_UARMR, E(-45, 0, -10)}, {H_LARMR, E(-30)}, {H_HANDR, E(70)}, {H_SPINE, E(25)}}, Ease::In)
         .key(0.5f, {{H_UARMR, E(-48, 0, -10)}}).key(1.0f, {{H_UARMR, E(-130, 0, -15)}, {H_LARMR, E(-70)}, {H_SPINE, E(15)}});
        c.event(0.42f, EV_IMPACT);
    }
    {
        C& c = mk(C_SWEEP_WORK, "sweep_work", 1.4f, true);
        c.key(0.0f, {{H_UARMR, E(-40, 40, -20)}, {H_UARML, E(-50, 40, 10)}, {H_LARMR, E(-40)}, {H_LARML, E(-60)}, {H_SPINE, E(20, 15)}})
         .key(0.5f, {{H_UARMR, E(-40, -40, -20)}, {H_UARML, E(-50, -30, 10)}, {H_SPINE, E(20, -15)}});
    }
    {
        C& c = mk(C_CHEER, "cheer", 1.2f);
        c.key(0.0f, {{H_UARML, E(-10, 0, 10)}, {H_UARMR, E(-10, 0, -10)}})
         .key(0.3f, {{H_UARML, E(-160, 0, 30)}, {H_UARMR, E(-160, 0, -30)}, {H_LARML, E(-10)}, {H_LARMR, E(-10)}, {H_HEAD, E(-20)}})
         .key(0.6f, {{H_UARML, E(-140, 0, 25)}, {H_UARMR, E(-140, 0, -25)}})
         .key(1.0f, {{H_UARML, E(-10, 0, 10)}, {H_UARMR, E(-10, 0, -10)}, {H_LARML, E(-20)}, {H_LARMR, E(-20)}, {H_HEAD, E(0)}});
        c.root(0.3f, {0, 0.12f, 0}).root(0.5f, {0, 0, 0});
    }
    {
        C& c = mk(C_TAUNT, "taunt", 1.0f);
        c.key(0.0f, {{H_CHEST, E(0)}, {H_UARMR, E(-20, 0, -15)}})
         .key(0.3f, {{H_CHEST, E(-12)}, {H_HEAD, E(-15)}, {H_UARMR, E(-90, 30, -60)}, {H_LARMR, E(-60)}, {H_UARML, E(-90, -30, 60)}, {H_LARML, E(-60)}})
         .key(0.6f, {{H_CHEST, E(-10)}, {H_UARMR, E(-80, 30, -55)}, {H_UARML, E(-80, -30, 55)}})
         .key(1.0f, {{H_CHEST, E(0)}, {H_HEAD, E(0)}, {H_UARMR, E(-20, 0, -15)}, {H_LARMR, E(-40)}, {H_UARML, E(-20, 0, 15)}, {H_LARML, E(-40)}});
    }
    {
        C& c = mk(C_ROAR_H, "roar_h", 1.2f);
        c.key(0.0f, {{H_SPINE, E(10)}, {H_CHEST, E(0)}})
         .key(0.3f, {{H_SPINE, E(-15)}, {H_CHEST, E(-15)}, {H_NECK, E(-20)}, {H_HEAD, E(-25)}, {H_UARML, E(-40, 0, 70)}, {H_UARMR, E(-40, 0, -70)}, {H_LARML, E(-60)}, {H_LARMR, E(-60)}})
         .key(0.8f, {{H_SPINE, E(-12)}, {H_CHEST, E(-12)}})
         .key(1.0f, {{H_SPINE, E(10)}, {H_CHEST, E(0)}, {H_NECK, E(0)}, {H_HEAD, E(0)}, {H_UARML, E(-20, 0, 15)}, {H_UARMR, E(-20, 0, -15)}, {H_LARML, E(-40)}, {H_LARMR, E(-40)}});
        c.event(0.3f, EV_SPAWN_FX);
    }
    // ---------- big humanoids (troll / golem / treant / boss knights)
    {
        C& c = mk(C_B_SLAM, "big_slam", 1.3f);
        c.key(0.0f, {{H_UARML, E(-20, 0, 15)}, {H_UARMR, E(-20, 0, -15)}, {H_SPINE, E(10)}})
         .key(0.45f, {{H_UARML, E(-175, 0, 10)}, {H_UARMR, E(-175, 0, -10)}, {H_LARML, E(-40)}, {H_LARMR, E(-40)}, {H_SPINE, E(-20)}, {H_CHEST, E(-10)}}, Ease::Out)
         .key(0.6f, {{H_UARML, E(-30, 0, 10)}, {H_UARMR, E(-30, 0, -10)}, {H_LARML, E(0)}, {H_LARMR, E(0)}, {H_SPINE, E(45)}, {H_CHEST, E(15)}}, Ease::In)
         .key(0.8f, {{H_SPINE, E(42)}})
         .key(1.0f, {{H_UARML, E(-20, 0, 15)}, {H_UARMR, E(-20, 0, -15)}, {H_LARML, E(-30)}, {H_LARMR, E(-30)}, {H_SPINE, E(10)}, {H_CHEST, E(0)}});
        c.root(0.6f, {0, -0.25f, 0}).root(1.f, {0, 0, 0});
        c.hit(0.56f, 0.66f).event(0.6f, EV_IMPACT).event(0.5f, EV_SOUND_WHOOSH).move(0.6f, 0.8f).move(1.f, 0.8f);
    }
    {
        C& c = mk(C_B_SWIPE, "big_swipe", 1.0f);
        c.key(0.0f, {{H_UARMR, E(-60, -60, -60)}, {H_LARMR, E(-40)}, {H_CHEST, E(0, -40)}, {H_SPINE, E(10, -15)}})
         .key(0.4f, {{H_UARMR, E(-80, -80, -50)}, {H_CHEST, E(0, -50)}}, Ease::Out)
         .key(0.6f, {{H_UARMR, E(-80, 60, -20)}, {H_LARMR, E(-10)}, {H_CHEST, E(5, 45)}, {H_SPINE, E(15, 15)}}, Ease::In)
         .key(1.0f, {{H_UARMR, E(-20, 0, -15)}, {H_LARMR, E(-30)}, {H_CHEST, E(0)}, {H_SPINE, E(10)}});
        c.hit(0.45f, 0.65f).event(0.44f, EV_SOUND_WHOOSH).move(0.6f, 0.7f).move(1.f, 0.7f);
    }
    {
        C& c = mk(C_B_STOMP, "big_stomp", 1.1f);
        c.key(0.0f, {{H_THIGHR, E(0)}, {H_SHINR, E(10)}, {H_SPINE, E(10)}})
         .key(0.45f, {{H_THIGHR, E(-90)}, {H_SHINR, E(90)}, {H_SPINE, E(-10)}, {H_UARML, E(-40, 0, 50)}, {H_UARMR, E(-40, 0, -50)}}, Ease::Out)
         .key(0.58f, {{H_THIGHR, E(-15)}, {H_SHINR, E(15)}, {H_SPINE, E(25)}}, Ease::In)
         .key(1.0f, {{H_THIGHR, E(0)}, {H_SHINR, E(10)}, {H_SPINE, E(10)}, {H_UARML, E(-20, 0, 15)}, {H_UARMR, E(-20, 0, -15)}});
        c.root(0.45f, {0, 0.1f, 0}).root(0.58f, {0, -0.15f, 0}).root(1.f, {0, 0, 0});
        c.hit(0.56f, 0.64f).event(0.58f, EV_IMPACT);
    }
    {
        C& c = mk(C_B_ROAR, "big_roar", 1.6f);
        c.key(0.0f, {{H_SPINE, E(15)}, {H_CHEST, E(0)}, {H_HEAD, E(0)}})
         .key(0.25f, {{H_SPINE, E(-20)}, {H_CHEST, E(-20)}, {H_NECK, E(-25)}, {H_HEAD, E(-30)}, {H_UARML, E(-60, 0, 80)}, {H_UARMR, E(-60, 0, -80)}, {H_LARML, E(-70)}, {H_LARMR, E(-70)}}, Ease::Out)
         .key(0.8f, {{H_SPINE, E(-18)}, {H_CHEST, E(-18)}})
         .key(1.0f, {{H_SPINE, E(15)}, {H_CHEST, E(0)}, {H_NECK, E(0)}, {H_HEAD, E(0)}, {H_UARML, E(-20, 0, 15)}, {H_UARMR, E(-20, 0, -15)}, {H_LARML, E(-30)}, {H_LARMR, E(-30)}});
        c.event(0.25f, EV_SPAWN_FX);
    }
    {
        C& c = mk(C_B_SPIN, "big_spin", 1.4f);
        c.key(0.0f, {{H_ROOT, E(0, 0)}, {H_UARML, E(-80, 0, 80)}, {H_UARMR, E(-80, 0, -80)}}, Ease::Linear)
         .key(0.25f, {{H_ROOT, E(0, 120)}}, Ease::Linear).key(0.5f, {{H_ROOT, E(0, 240)}}, Ease::Linear).key(0.75f, {{H_ROOT, E(0, 359)}}, Ease::Linear)
         .key(0.9f, {{H_ROOT, E(0, 359)}, {H_UARML, E(-80, 0, 80)}, {H_UARMR, E(-80, 0, -80)}})
         .key(1.0f, {{H_UARML, E(-20, 0, 15)}, {H_UARMR, E(-20, 0, -15)}});
        c.hit(0.05f, 0.75f).event(0.1f, EV_SOUND_WHOOSH).event(0.45f, EV_SOUND_WHOOSH);
    }
    {
        C& c = mk(C_B_THROW, "big_throw", 1.2f);
        c.key(0.0f, {{H_SPINE, E(30)}, {H_UARMR, E(-50, 0, -10)}, {H_LARMR, E(-20)}})
         .key(0.4f, {{H_SPINE, E(-10, -20)}, {H_UARMR, E(-170, -30, -20)}, {H_LARMR, E(-80)}}, Ease::Out)
         .key(0.55f, {{H_SPINE, E(20, 20)}, {H_UARMR, E(-70, 20, -10)}, {H_LARMR, E(-10)}}, Ease::In)
         .key(1.0f, {{H_SPINE, E(10)}, {H_UARMR, E(-20, 0, -15)}, {H_LARMR, E(-30)}});
        c.event(0.55f, EV_RELEASE);
    }
    {
        C& c = mk(C_VICTORY, "victory", 1.5f);
        c.key(0.0f, {{H_UARMR, E(-30, 0, -15)}, {H_LARMR, E(-50)}})
         .key(0.3f, {{H_UARMR, E(-175, 0, -10)}, {H_LARMR, E(-5)}, {H_HANDR, E(0)}, {H_CHEST, E(-10)}, {H_HEAD, E(-20)}}, Ease::OutBack)
         .key(0.85f, {{H_UARMR, E(-172, 0, -10)}})
         .key(1.0f, {{H_UARMR, E(-30, 0, -15)}, {H_LARMR, E(-50)}, {H_CHEST, E(0)}, {H_HEAD, E(0)}});
    }
}

static void quadClips() {
    {
        Clip& c = mk(C_Q_BITE, "q_bite", 0.55f);
        c.key(0.0f, {{Q_NECK, E(0)}, {Q_HEAD, E(0)}, {Q_JAW, E(0)}, {Q_FL1, E(0)}, {Q_FR1, E(0)}})
         .key(0.35f, {{Q_NECK, E(-25)}, {Q_HEAD, E(-15)}, {Q_JAW, E(40)}, {Q_FL1, E(15)}, {Q_FR1, E(15)}}, Ease::Out)
         .key(0.5f, {{Q_NECK, E(25)}, {Q_HEAD, E(10)}, {Q_JAW, E(0)}, {Q_FL1, E(-25)}, {Q_FR1, E(-20)}}, Ease::In)
         .key(1.0f, {{Q_NECK, E(0)}, {Q_HEAD, E(0)}, {Q_JAW, E(5)}, {Q_FL1, E(0)}, {Q_FR1, E(0)}});
        c.hit(0.42f, 0.58f).move(0.5f, 1.0f).move(1.f, 1.0f);
    }
    {
        Clip& c = mk(C_Q_POUNCE, "q_pounce", 0.9f);
        c.key(0.0f, {{Q_ROOT, E(0)}, {Q_BL1, E(30)}, {Q_BR1, E(30)}, {Q_FL1, E(-10)}, {Q_FR1, E(-10)}, {Q_JAW, E(10)}})
         .key(0.3f, {{Q_ROOT, E(-10)}, {Q_BL1, E(40)}, {Q_BR1, E(40)}, {Q_BL2, E(-60)}, {Q_BR2, E(-60)}})
         .key(0.55f, {{Q_ROOT, E(-20)}, {Q_FL1, E(-80)}, {Q_FR1, E(-80)}, {Q_FL2, E(20)}, {Q_FR2, E(20)}, {Q_BL1, E(60)}, {Q_BR1, E(60)}, {Q_BL2, E(0)}, {Q_BR2, E(0)}, {Q_JAW, E(40)}}, Ease::Out)
         .key(0.75f, {{Q_ROOT, E(10)}, {Q_FL1, E(-20)}, {Q_FR1, E(-20)}, {Q_JAW, E(0)}})
         .key(1.0f, {{Q_ROOT, E(0)}, {Q_FL1, E(0)}, {Q_FR1, E(0)}, {Q_FL2, E(0)}, {Q_FR2, E(0)}, {Q_BL1, E(0)}, {Q_BR1, E(0)}, {Q_JAW, E(5)}});
        c.root(0.f, {0, 0, 0}).root(0.3f, {0, -0.15f, 0}).root(0.55f, {0, 0.6f, 0}).root(0.75f, {0, 0, 0}).root(1.f, {0, 0, 0});
        c.hit(0.5f, 0.78f).move(0.3f, 0).move(0.75f, 5.f).move(1.f, 5.2f);
    }
    {
        Clip& c = mk(C_Q_HOWL, "q_howl", 1.6f);
        c.key(0.0f, {{Q_NECK, E(0)}, {Q_HEAD, E(0)}, {Q_JAW, E(0)}, {Q_ROOT, E(0)}})
         .key(0.3f, {{Q_NECK, E(-55)}, {Q_HEAD, E(-25)}, {Q_JAW, E(30)}, {Q_ROOT, E(8)}})
         .key(0.85f, {{Q_NECK, E(-55)}, {Q_HEAD, E(-25)}, {Q_JAW, E(25)}})
         .key(1.0f, {{Q_NECK, E(0)}, {Q_HEAD, E(0)}, {Q_JAW, E(0)}, {Q_ROOT, E(0)}});
        c.event(0.3f, EV_SPAWN_FX);
    }
    {
        Clip& c = mk(C_Q_HIT, "q_hit", 0.35f);
        c.key(0.0f, {{Q_SPINE, E(0)}, {Q_NECK, E(0)}}).key(0.3f, {{Q_SPINE, E(8, 10)}, {Q_NECK, E(15, -15)}}, Ease::Out).key(1.0f, {{Q_SPINE, E(0)}, {Q_NECK, E(0)}});
        c.move(0.3f, -0.4f).move(1.f, -0.4f);
    }
    {
        Clip& c = mk(C_Q_DEATH, "q_death", 1.0f);
        c.key(0.0f, {{Q_ROOT, E(0)}}).key(0.6f, {{Q_ROOT, E(0, 0, 85)}, {Q_NECK, E(10, 0, -10)}, {Q_JAW, E(15)}}, Ease::In).key(0.75f, {{Q_ROOT, E(0, 0, 80)}}).key(1.0f, {{Q_ROOT, E(0, 0, 85)}});
        c.root(0.f, {0, 0, 0}).root(0.6f, {0, -0.5f, 0}).root(1.f, {0, -0.5f, 0});
        c.event(0.6f, EV_IMPACT);
    }
    {
        Clip& c = mk(C_Q_CHARGE, "q_charge", 1.0f);
        c.key(0.0f, {{Q_NECK, E(0)}, {Q_HEAD, E(0)}}).key(0.25f, {{Q_NECK, E(20)}, {Q_HEAD, E(20)}}).key(1.0f, {{Q_NECK, E(20)}, {Q_HEAD, E(20)}});
        c.hit(0.25f, 1.f).move(0.25f, 0.f).move(1.f, 9.f);
    }
    {
        Clip& c = mk(C_Q_REAR, "q_rear", 1.0f);
        c.key(0.0f, {{Q_ROOT, E(0)}, {Q_FL1, E(0)}, {Q_FR1, E(0)}})
         .key(0.4f, {{Q_ROOT, E(-40)}, {Q_FL1, E(-60)}, {Q_FR1, E(-40)}, {Q_FL2, E(80)}, {Q_FR2, E(70)}, {Q_NECK, E(-20)}})
         .key(0.6f, {{Q_ROOT, E(-38)}, {Q_FL1, E(-40)}, {Q_FR1, E(-60)}})
         .key(1.0f, {{Q_ROOT, E(0)}, {Q_FL1, E(0)}, {Q_FR1, E(0)}, {Q_FL2, E(0)}, {Q_FR2, E(0)}, {Q_NECK, E(0)}});
        c.root(0.4f, {0, 0.25f, 0}).root(1.f, {0, 0, 0});
        c.hit(0.75f, 0.9f).event(0.85f, EV_IMPACT);
    }
    // spider
    {
        Clip& c = mk(C_S_BITE, "s_bite", 0.6f);
        c.key(0.0f, {{S_HEAD, E(0)}, {S_ROOT, E(0)}}).key(0.35f, {{S_HEAD, E(-25)}, {S_ROOT, E(-15)}}, Ease::Out).key(0.5f, {{S_HEAD, E(25)}, {S_ROOT, E(10)}}, Ease::In).key(1.0f, {{S_HEAD, E(0)}, {S_ROOT, E(0)}});
        c.root(0.35f, {0, 0.15f, 0}).root(0.5f, {0, -0.05f, 0}).root(1.f, {0, 0, 0});
        c.hit(0.42f, 0.6f).move(0.5f, 1.2f).move(1.f, 1.2f);
    }
    {
        Clip& c = mk(C_S_SPIT, "s_spit", 0.8f);
        c.key(0.0f, {{S_ABDOMEN, E(0)}, {S_ROOT, E(0)}}).key(0.4f, {{S_ABDOMEN, E(-50)}, {S_ROOT, E(-20)}}, Ease::Out).key(0.55f, {{S_ABDOMEN, E(20)}, {S_ROOT, E(5)}}, Ease::In).key(1.0f, {{S_ABDOMEN, E(0)}, {S_ROOT, E(0)}});
        c.event(0.55f, EV_RELEASE);
    }
    {
        Clip& c = mk(C_S_LEAP, "s_leap", 0.9f);
        c.key(0.0f, {{S_ROOT, E(0)}}).key(0.3f, {{S_ROOT, E(10)}}).key(0.55f, {{S_ROOT, E(-25)}}).key(1.0f, {{S_ROOT, E(0)}});
        c.root(0.f, {0, 0, 0}).root(0.3f, {0, -0.2f, 0}).root(0.55f, {0, 1.2f, 0}).root(0.8f, {0, 0, 0}).root(1.f, {0, 0, 0});
        c.hit(0.6f, 0.82f).event(0.8f, EV_IMPACT).move(0.3f, 0.f).move(0.8f, 7.f).move(1.f, 7.f);
    }
    {
        Clip& c = mk(C_S_HIT, "s_hit", 0.3f);
        c.key(0.0f, {{S_ROOT, E(0)}}).key(0.3f, {{S_ROOT, E(10, 0, 8)}}, Ease::Out).key(1.0f, {{S_ROOT, E(0)}});
        c.move(1.f, -0.4f);
    }
    {
        Clip& c = mk(C_S_DEATH, "s_death", 1.0f);
        c.key(0.0f, {{S_ROOT, E(0)}}).key(0.6f, {{S_ROOT, E(0, 0, 180)}}, Ease::In).key(1.0f, {{S_ROOT, E(0, 0, 180)}});
        c.root(0.f, {0, 0, 0}).root(0.6f, {0, -0.35f, 0}).root(1.f, {0, -0.35f, 0});
        c.event(0.6f, EV_IMPACT);
    }
    // dragon
    {
        Clip& c = mk(C_D_BITE, "d_bite", 0.8f);
        c.key(0.0f, {{Q_NECK, E(0)}, {Q_NECK2, E(-15)}, {Q_HEAD, E(0)}, {Q_JAW, E(0)}})
         .key(0.4f, {{Q_NECK, E(-35)}, {Q_NECK2, E(-30)}, {Q_HEAD, E(-10)}, {Q_JAW, E(45)}}, Ease::Out)
         .key(0.55f, {{Q_NECK, E(35)}, {Q_NECK2, E(20)}, {Q_HEAD, E(20)}, {Q_JAW, E(0)}}, Ease::In)
         .key(1.0f, {{Q_NECK, E(0)}, {Q_NECK2, E(-15)}, {Q_HEAD, E(0)}, {Q_JAW, E(5)}});
        c.hit(0.48f, 0.62f).event(0.55f, EV_IMPACT).move(0.55f, 1.5f).move(1.f, 1.5f);
    }
    {
        Clip& c = mk(C_D_TAIL, "d_tail", 1.1f);
        c.key(0.0f, {{Q_ROOT, E(0, 0)}, {Q_TAIL1, E(0, 0)}, {Q_TAIL2, E(0, 0)}, {Q_TAIL3, E(0, 0)}}, Ease::Linear)
         .key(0.35f, {{Q_ROOT, E(0, -40)}, {Q_TAIL1, E(0, 40)}, {Q_TAIL2, E(0, 30)}}, Ease::Out)
         .key(0.55f, {{Q_ROOT, E(0, 90)}, {Q_TAIL1, E(0, -50)}, {Q_TAIL2, E(0, -40)}, {Q_TAIL3, E(0, -40)}}, Ease::In)
         .key(0.75f, {{Q_ROOT, E(0, 160)}, {Q_TAIL1, E(0, -20)}}, Ease::Out)
         .key(1.0f, {{Q_ROOT, E(0, 0)}, {Q_TAIL1, E(0, 0)}, {Q_TAIL2, E(0, 0)}, {Q_TAIL3, E(0, 0)}});
        c.hit(0.45f, 0.75f).event(0.45f, EV_SOUND_WHOOSH);
    }
    {
        Clip& c = mk(C_D_BREATH, "d_breath", 2.2f);
        c.key(0.0f, {{Q_NECK, E(0)}, {Q_NECK2, E(-15)}, {Q_HEAD, E(0)}, {Q_JAW, E(0)}, {Q_CHEST, E(0)}})
         .key(0.25f, {{Q_NECK, E(-45)}, {Q_NECK2, E(-30)}, {Q_HEAD, E(-20)}, {Q_JAW, E(10)}, {Q_CHEST, E(-12)}}, Ease::Out)
         .key(0.35f, {{Q_NECK, E(15)}, {Q_NECK2, E(10)}, {Q_HEAD, E(15)}, {Q_JAW, E(40)}, {Q_CHEST, E(5)}}, Ease::In)
         .key(0.85f, {{Q_NECK, E(15)}, {Q_NECK2, E(10)}, {Q_HEAD, E(15)}, {Q_JAW, E(40)}})
         .key(1.0f, {{Q_NECK, E(0)}, {Q_NECK2, E(-15)}, {Q_HEAD, E(0)}, {Q_JAW, E(5)}, {Q_CHEST, E(0)}});
        c.event(0.35f, EV_RELEASE).hit(0.35f, 0.85f);
    }
    {
        Clip& c = mk(C_D_WING, "d_wing", 1.2f);
        c.key(0.0f, {{Q_WINGL1, E(10, -40, -25)}, {Q_WINGR1, E(10, 40, 25)}, {Q_WINGL2, E(0, -110, 15)}, {Q_WINGR2, E(0, 110, -15)}, {Q_ROOT, E(0)}})
         .key(0.35f, {{Q_WINGL1, E(0, 5, 70)}, {Q_WINGR1, E(0, -5, -70)}, {Q_WINGL2, E(0, 5, 30)}, {Q_WINGR2, E(0, -5, -30)}, {Q_ROOT, E(-25)}}, Ease::Out)
         .key(0.55f, {{Q_WINGL1, E(0, 5, -40)}, {Q_WINGR1, E(0, -5, 40)}, {Q_WINGL2, E(0, 5, -20)}, {Q_WINGR2, E(0, -5, 20)}, {Q_ROOT, E(-15)}}, Ease::In)
         .key(1.0f, {{Q_WINGL1, E(10, -40, -25)}, {Q_WINGR1, E(10, 40, 25)}, {Q_WINGL2, E(0, -110, 15)}, {Q_WINGR2, E(0, 110, -15)}, {Q_ROOT, E(0)}});
        c.root(0.35f, {0, 0.4f, 0}).root(1.f, {0, 0, 0});
        c.event(0.55f, EV_RELEASE);
    }
    {
        Clip& c = mk(C_D_ROAR, "d_roar", 1.8f);
        c.key(0.0f, {{Q_NECK, E(0)}, {Q_NECK2, E(-15)}, {Q_HEAD, E(0)}, {Q_JAW, E(0)}, {Q_ROOT, E(0)}})
         .key(0.3f, {{Q_NECK, E(-50)}, {Q_NECK2, E(-30)}, {Q_HEAD, E(-15)}, {Q_JAW, E(45)}, {Q_ROOT, E(-15)}}, Ease::Out)
         .key(0.8f, {{Q_NECK, E(-48)}, {Q_JAW, E(40)}})
         .key(1.0f, {{Q_NECK, E(0)}, {Q_NECK2, E(-15)}, {Q_HEAD, E(0)}, {Q_JAW, E(5)}, {Q_ROOT, E(0)}});
        c.event(0.3f, EV_SPAWN_FX);
    }
    {
        Clip& c = mk(C_D_HIT, "d_hit", 0.4f);
        c.key(0.0f, {{Q_NECK, E(0)}}).key(0.3f, {{Q_NECK, E(-20, 15)}}, Ease::Out).key(1.0f, {{Q_NECK, E(0)}});
    }
    {
        Clip& c = mk(C_D_DEATH, "d_death", 2.0f);
        c.key(0.0f, {{Q_ROOT, E(0)}, {Q_NECK, E(0)}}).key(0.4f, {{Q_ROOT, E(-20)}, {Q_NECK, E(-50)}, {Q_JAW, E(40)}}).key(0.8f, {{Q_ROOT, E(0, 0, 80)}, {Q_NECK, E(20, 0, -20)}, {Q_JAW, E(20)}}, Ease::In).key(1.0f, {{Q_ROOT, E(0, 0, 85)}});
        c.root(0.f, {0, 0, 0}).root(0.4f, {0, 0.3f, 0}).root(0.8f, {0, -0.5f, 0}).root(1.f, {0, -0.5f, 0});
        c.event(0.8f, EV_IMPACT).event(0.4f, EV_SPAWN_FX);
    }
    {
        Clip& c = mk(C_D_STOMP, "d_stomp", 1.0f);
        c.key(0.0f, {{Q_ROOT, E(0)}, {Q_FL1, E(0)}, {Q_FR1, E(0)}})
         .key(0.45f, {{Q_ROOT, E(-30)}, {Q_FL1, E(-70)}, {Q_FR1, E(-70)}, {Q_FL2, E(70)}, {Q_FR2, E(70)}}, Ease::Out)
         .key(0.6f, {{Q_ROOT, E(5)}, {Q_FL1, E(-10)}, {Q_FR1, E(-10)}, {Q_FL2, E(0)}, {Q_FR2, E(0)}}, Ease::In)
         .key(1.0f, {{Q_ROOT, E(0)}, {Q_FL1, E(0)}, {Q_FR1, E(0)}});
        c.root(0.45f, {0, 0.3f, 0}).root(0.6f, {0, -0.1f, 0}).root(1.f, {0, 0, 0});
        c.hit(0.58f, 0.66f).event(0.6f, EV_IMPACT);
    }
}

void initClips() {
    std::call_once(gClipOnce, [] { humanClips(); quadClips(); });
}

const Clip& clip(ClipId id) { initClips(); return gClips[id]; }

void setupAnimator(Animator& a, Arch arch) {
    initClips();
    a.skeleton = &skeleton(arch);
    for (int i = 0; i < MAX_BONES; i++) a.upperMask[i] = 0;
    switch (arch) {
    case Arch::Humanoid:
        a.locomotion = humanLoco; a.airborne = humanAir; a.swim = humanSwim; a.mounted = humanMounted;
        a.dead = humanDead; a.stagger = humanStagger; a.victory = humanVictory; a.sit = humanSit; a.sleep = humanSleep; a.work = humanWork;
        for (int b : {H_SPINE, H_CHEST, H_NECK, H_HEAD, H_SHL, H_UARML, H_LARML, H_HANDL, H_SHR, H_UARMR, H_LARMR, H_HANDR, H_PROPR, H_PROPL}) a.upperMask[b] = 1.f;
        a.upperMask[H_SPINE] = 0.6f;
        a.gait = GaitParams{1.4f, 2.9f, 1.6f, 4.6f, 7.2f};   // stride per full cycle: natural cadence (~1.1 Hz walk, ~1.6 Hz run)
        break;
    case Arch::Quadruped:
        a.locomotion = quadLoco; a.dead = quadDead; a.airborne = quadLoco; a.swim = quadLoco;
        a.gait = GaitParams{1.0f, 2.6f, 1.4f, 5.f, 9.f};
        for (int b : {Q_NECK, Q_HEAD, Q_JAW}) a.upperMask[b] = 1.f;
        break;
    case Arch::Spider:
        a.locomotion = spiderLoco; a.dead = spiderDead; a.airborne = spiderLoco;
        a.gait = GaitParams{0.9f, 1.6f, 1.5f, 4.5f, 7.f};
        break;
    case Arch::Dragon:
        a.locomotion = dragonLoco; a.dead = quadDead; a.airborne = dragonLoco;
        a.gait = GaitParams{1.0f, 2.6f, 1.4f, 4.f, 7.f};
        for (int b : {Q_NECK, Q_NECK2, Q_HEAD, Q_JAW}) a.upperMask[b] = 1.f;
        break;
    }
}

}  // namespace lk
