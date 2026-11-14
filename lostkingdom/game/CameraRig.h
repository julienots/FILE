// LOST KINGDOM — third-person camera rig as a pure function (unit-tested, no Game dependency).
// The gameplay camera orbits the hero: yaw (drag left/right), pitch (drag up/down) and distance (pinch).
//   pitch -0.35 : low camera looking up at the sky (hero framed in the lower third)
//   pitch  0.66 : default framing
//   pitch  1.35 : almost top-down ("map" view)
#pragma once
#include "../engine/core/Math.h"
#include <functional>

namespace lk {
using namespace agr;

struct CameraRigIn {
    vec3 focus;               // smoothed point followed by the camera (hero chest, + lock-on / boss bias)
    vec3 lookAhead;           // horizontal offset of the aim point (run direction), already smoothed
    float yaw = 0, pitch = 0.66f, dist = 15.5f;
    bool playing = true;      // gameplay framing (aim above the chest) vs title fly-over (aim at the focus)
    float fovY = 38.f * DEG;  // vertical field of view (keeps the hero's feet in frame when looking up)
    float feetDrop = 1.1f;    // focus height above the hero's feet
    // terrain height under a point (nullptr: no terrain, e.g. dungeons use `blocked` only)
    std::function<float(float, float)> ground;
    // solid scenery (buildings, walls) at a point; the camera is pulled in front of it (nullptr: none)
    std::function<bool(vec3)> blocked;
    // Smoothing (1.6.1: "the camera wanders"). dt = 0 disables it (unit tests, first frame). The caller feeds
    // back the previous frame's collisionDist / lift from CameraRigOut.
    float dt = 0;
    float prevCollisionDist = -1;   // < 0: none yet
    float prevLift = 0;
    float smooth = 1.f;             // > 1: faster settling (tight camera)
};

struct CameraRigOut {
    vec3 eye, look;
    float pitch = 0;          // effective elevation angle used (radians)
    float dist = 0;           // effective distance after the low-angle pull-in and collisions
    float lowK = 0;           // 0..1: how much the "look up at the sky" framing is engaged
    float mapK = 0;           // 0..1: how much the zoomed-out "map" view is engaged
    float nearK = 0;          // 0..1: how much the close-up framing is engaged
    float minClear = 3.f;     // minimum eye height above the terrain that was enforced
    bool collided = false;    // scenery shortened the distance
    float collisionDist = -1; // smoothed distance allowed by the scenery (feed back as prevCollisionDist)
    float lift = 0;           // smoothed terrain lift of the eye (feed back as prevLift)
};

struct CameraRig {
    static constexpr float PITCH_MIN = -0.35f, PITCH_MAX = 1.35f, PITCH_DEFAULT = 0.66f;
    static constexpr float DIST_MIN = 4.f, DIST_MAX = 40.f, DIST_DEFAULT = 15.5f;
    static float clampPitch(float p) { return clampf(p, PITCH_MIN, PITCH_MAX); }
    static float clampDist(float d) { return clampf(d, DIST_MIN, DIST_MAX); }
    // 0 above pitch 0.25, 1 at PITCH_MIN (smooth)
    static float lowFactor(float pitch);
    // 0 below distance 24, 1 at DIST_MAX (smooth)
    static float mapFactor(float dist);
    // 0 above distance 9, 1 at DIST_MIN (smooth)
    static float nearFactor(float dist);
    // minimum height of the eye above the terrain: 3 m at normal pitch, 0.6 m when looking up
    static float minClearance(float pitch, float dist);
    static constexpr float HARD_CLEARANCE = 0.3f;   // never closer to the ground than this, even while smoothing
    static CameraRigOut compute(const CameraRigIn& in);
};

}  // namespace lk
