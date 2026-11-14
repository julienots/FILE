#include "CameraRig.h"
#include <algorithm>
#include <cmath>

namespace lk {

float CameraRig::lowFactor(float pitch) { return smoothstepf(0.25f, PITCH_MIN, pitch); }
float CameraRig::mapFactor(float dist) { return smoothstepf(24.f, DIST_MAX, dist); }
float CameraRig::nearFactor(float dist) { return smoothstepf(9.f, DIST_MIN, dist); }

float CameraRig::minClearance(float pitch, float dist) {
    // 3 m above the ground in the usual over-the-shoulder views, down to 0.6 m when the camera is low and looks up
    float c = lerpf(0.6f, 3.f, smoothstepf(0.f, 0.45f, pitch));
    // far away, a camera skimming the ground only sees grass: rise a little with the distance
    c = std::max(c, std::min(3.f, dist * 0.05f));
    // close-ups: never force the eye higher than the framing needs
    return std::min(c, 0.6f + dist * 0.35f);
}

CameraRigOut CameraRig::compute(const CameraRigIn& in) {
    CameraRigOut o;
    float p = clampPitch(in.pitch);
    float d0 = clampDist(in.dist);
    o.lowK = in.playing ? lowFactor(p) : 0.f;
    o.mapK = in.playing ? mapFactor(d0) : 0.f;

    // Looking up: the camera comes closer (x0.6) so it stays behind the hero instead of sinking far below him.
    // Never closer than 4.5 m in that view: below that the whole hero cannot fit in the frame from 0.6 m high.
    // Capped at 18 m: from 0.6 m high, a hero further away is lost in the grass.
    float d = std::max(std::min(d0, 4.5f), std::min(d0 * lerpf(1.f, 0.6f, o.lowK), lerpf(DIST_MAX, 18.f, o.lowK)));
    // Zoomed out frames slightly lower (more horizon), as in 1.x; not in the low "sky" view
    float ep = clampf(p - (d0 - DIST_DEFAULT) * 0.004f * (1.f - o.lowK), PITCH_MIN, PITCH_MAX);
    vec3 dir{std::sin(in.yaw) * std::cos(ep), std::sin(ep), std::cos(in.yaw) * std::cos(ep)};

    // Scenery between the hero and the camera (houses, walls): pull the camera in front of it.
    // The camera snaps in at once (never inside a wall) but backs out slowly once the wall is passed.
    float free = d;
    if (in.blocked) {
        const float step = 0.4f, start = 0.8f;
        for (float s = start; s <= d; s += step) {
            if (in.blocked(in.focus + dir * s)) {
                free = std::max(1.6f, s - 0.5f);
                o.collided = true;
                break;
            }
        }
    }
    if (in.dt > 0 && in.prevCollisionDist > 0 && free > in.prevCollisionDist)
        free = std::min(free, in.prevCollisionDist + (free - in.prevCollisionDist) * (1.f - std::exp(-2.5f * in.smooth * in.dt)));
    o.collisionDist = free;
    d = std::min(d, free);
    o.eye = in.focus + dir * d;

    // Aim point: above the chest (more landscape), lower for close-ups (whole hero in frame, also when a wall
    // pulled the camera in), well above the head when looking up (hero in the lower third, sky above).
    o.nearK = in.playing ? nearFactor(d) : 0.f;
    float lookBase = in.playing ? lerpf(0.9f, -0.2f, o.nearK) : 0.f;
    o.look = in.focus + vec3(0, lookBase + 1.4f * o.lowK, 0) + in.lookAhead * (1.f - 0.7f * o.nearK);

    // Terrain: keep the eye above the ground (clearance depends on the pitch), and lift it when a hump of
    // terrain hides the hero.
    // The lift is smoothed (rises quickly, comes down slowly) so bumps under the camera do not shake it;
    // a hard floor of HARD_CLEARANCE above the ground is always enforced.
    if (in.ground) {
        o.minClear = minClearance(p, d);
        float raw = o.eye.y;
        float gy = in.ground(o.eye.x, o.eye.z);
        float want = std::max(raw, gy + o.minClear);
        for (int i = 2; i <= 5; i++) {
            float t = i / 6.f;
            vec3 q = lerp(in.focus, vec3(o.eye.x, want, o.eye.z), t);
            float g = in.ground(q.x, q.z) + 0.5f;
            if (q.y < g) want = std::max(want, in.focus.y + (g - in.focus.y) / t);
        }
        float lift = want - raw;
        if (in.dt > 0) {
            float rate = (lift > in.prevLift ? 9.f : 2.f) * in.smooth;
            lift = in.prevLift + (lift - in.prevLift) * (1.f - std::exp(-rate * in.dt));
            lift = std::max(lift, gy + HARD_CLEARANCE - raw);
            lift = std::max(0.f, lift);
        }
        o.lift = lift;
        o.eye.y = raw + lift;
    }
    // Looking up: keep the whole hero in frame. The aim point is raised only as long as the feet stay
    // inside 85 % of the lower half of the image (the hero ends up in the lower third, the sky above).
    if (o.lowK > 0) {
        vec3 feet = in.focus - vec3(0, in.feetDrop, 0);
        float hF = std::max(0.5f, length(vec2(feet.x - o.eye.x, feet.z - o.eye.z)));
        float hL = std::max(0.5f, length(vec2(o.look.x - o.eye.x, o.look.z - o.eye.z)));
        float aFeet = std::atan2(feet.y - o.eye.y, hF);
        float maxY = o.eye.y + hL * std::tan(std::min(1.2f, aFeet + in.fovY * 0.5f * 0.85f));
        o.look.y = std::max(feet.y + 0.6f, std::min(o.look.y, maxY));
    }
    o.pitch = ep;
    o.dist = d;
    return o;
}

}  // namespace lk
