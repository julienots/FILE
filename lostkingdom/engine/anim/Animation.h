// AGR Engine — animation runtime.
//  * Skeleton: bone hierarchy defined by bind offsets (rest pose = identity rotations).
//  * Pose: local rotations + root translation.
//  * Clip: per-channel keyframes (quaternions, eased), events, hit windows, root motion curve.
//  * Animator: layered state machine — locomotion blend tree (speed + direction, phase-synced),
//    full-body states, masked action layer, crossfade transitions, procedural additive layers.
//  * IK: analytic two-bone IK (foot planting on uneven terrain, hand targets), look-at.
//  * Verlet: cloth sheets (capes, banners) and strand chains (hair, tails).
#pragma once
#include "../core/Math.h"
#include <vector>
#include <string>
#include <functional>
#include <map>

namespace agr {

constexpr int MAX_BONES = 32;

struct Bone {
    std::string name;
    int parent = -1;
    vec3 offset;      // bind translation relative to parent
    vec3 bindWorld;   // computed: bind position in model space
};

struct Skeleton {
    std::vector<Bone> bones;
    int add(const std::string& name, int parent, vec3 offset);
    void finalize();
    int find(const std::string& name) const;
    int size() const { return (int)bones.size(); }
};

struct Pose {
    quat rot[MAX_BONES];
    vec3 rootOffset;          // pelvis translation (bob, crouch, jump)
    vec3 scale[MAX_BONES];    // squash & stretch (rarely used)
    Pose() { reset(); }
    void reset() { for (int i = 0; i < MAX_BONES; i++) { rot[i] = quat(); scale[i] = vec3(1); } rootOffset = vec3(0); }
};
void blendPose(Pose& out, const Pose& a, const Pose& b, float t, int n);           // out = lerp(a,b,t)
void blendPoseMasked(Pose& out, const Pose& a, const Pose& b, float t, const float* mask, int n);
void addPose(Pose& out, const Pose& additive, float w, int n);                      // out = out * additive^w

// World-space evaluation of a pose.
struct PoseWorld {
    mat4 world[MAX_BONES];   // bone world transforms (model space)
    mat4 skin[MAX_BONES];    // world * inverse(bind)
    quat wrot[MAX_BONES];
    vec3 wpos[MAX_BONES];
};
void computeWorld(const Skeleton& sk, const Pose& p, PoseWorld& out);

// Euler helper in degrees (pitch X, yaw Y, roll Z).
inline quat E(float x, float y = 0, float z = 0) { return quat::euler(x * DEG, y * DEG, z * DEG); }

enum class Ease : uint8_t { Linear, Smooth, In, Out, OutBack };

struct AnimEvent { float time; int id; };

struct Clip {
    std::string name;
    float duration = 1.f;
    bool loop = false;
    struct Key { float t; quat q; Ease ease; };
    std::map<int, std::vector<Key>> channels;
    std::vector<std::pair<float, vec3>> rootKeys;  // pelvis offset keys
    std::vector<std::pair<float, float>> motion;    // root motion: forward distance (m) over time
    std::vector<AnimEvent> events;
    float hitStart = -1, hitEnd = -1;   // active hit window (normalized time)
    float comboOpen = 0.55f;            // from this normalized time the next combo input is accepted
    float cancelable = 0.75f;           // from here movement may cancel the clip
    // Authoring helpers
    Clip& key(float t, std::initializer_list<std::pair<int, quat>> bones, Ease e = Ease::Smooth);
    Clip& root(float t, vec3 off) { rootKeys.push_back({t, off}); return *this; }
    Clip& move(float t, float dist) { motion.push_back({t, dist}); return *this; }
    Clip& event(float t, int id) { events.push_back({t, id}); return *this; }
    Clip& hit(float a, float b) { hitStart = a; hitEnd = b; return *this; }
    void sample(float t, Pose& out, const Pose* base, int nBones, float* mask = nullptr) const;
    float motionAt(float t) const;  // cumulative root motion distance at normalized t
};

// Procedural locomotion parameters for a body archetype.
struct GaitParams {
    float strideWalk = 1.25f, strideRun = 2.1f;   // metres per full cycle
    float walkSpeed = 1.6f, runSpeed = 4.6f, sprintSpeed = 7.2f;
    float ampWalk = 1.f, ampRun = 1.f;            // leg swing amplitude (longer strides need wider steps)
    float sprintStride = 0.18f;                   // extra stride length at full sprint (bounds instead of faster steps)
};

enum class BodyState : uint8_t { Locomotion, Airborne, Swim, Climb, Mounted, Dead, Stagger, Downed, Victory, Sit, Sleep, Work };

class Animator;
using ProceduralFn = std::function<void(const Animator&, Pose&)>;

class Animator {
public:
    const Skeleton* skeleton = nullptr;
    GaitParams gait;
    // Inputs (set by gameplay each frame)
    float speed = 0;          // ground speed m/s
    float lateral = 0;        // -1..1 strafe component (lock-on)
    float turnRate = 0;       // rad/s (lean into turns)
    float verticalVel = 0;
    bool grounded = true;
    bool blocking = false;
    bool aiming = false;
    float breath = 1.f;       // breathing intensity (exertion)
    int stance = 0;           // archetype-specific (weapon stance for humanoids)
    float aux[4] = {0, 0, 0, 0};  // archetype-specific parameters (e.g. fly height, aggression)
    BodyState state = BodyState::Locomotion;
    // Outputs
    Pose pose;
    float phase = 0;          // locomotion cycle phase 0..1
    float time = 0;
    int footstepEvent = 0;    // incremented when a foot plants (0/1 alternate) — read & clear by game
    int lastFoot = -1;

    // Locomotion & states are procedural functions provided by the archetype.
    ProceduralFn locomotion;  // writes base pose from phase/speed/lateral
    ProceduralFn airborne, swim, climb, mounted, dead, stagger, victory, sit, sleep, work;
    float upperMask[MAX_BONES] = {0};  // 1 for bones driven by upper-body actions

    // Action layer (attacks, casts, interactions)
    void play(const Clip* c, float speedMul = 1.f, bool upperBodyOnly = false, float fadeIn = 0.08f);
    void stopAction(float fadeOut = 0.15f);
    const Clip* action() const { return act; }
    float actionTime() const { return actT; }      // normalized 0..1
    bool actionDone() const { return !act || actT >= 1.f; }
    bool actionInHitWindow() const { return act && act->hitStart >= 0 && actT >= act->hitStart && actT <= act->hitEnd; }
    std::vector<int> firedEvents;                   // events crossed this frame

    void setState(BodyState s, float fade = 0.2f);
    void update(float dt);

private:
    const Clip* act = nullptr;
    float actT = 0, actSpeed = 1, actWeight = 0, actFadeIn = 0.08f, actFadeOut = 0.15f;
    bool actUpper = false, actStopping = false;
    Pose snapshot;            // pose at the moment of a state change (crossfade source)
    float stateBlend = 1.f, stateFade = 0.2f;
    void evalState(BodyState s, Pose& out);
};

// ---- Inverse kinematics ---------------------------------------------------------------------
// Adjusts local rotations of a, b (upper/lower limb) so that end-effector c reaches target.
// pole: world-space direction the middle joint should bend towards.
void twoBoneIK(const Skeleton& sk, Pose& pose, PoseWorld& w, int a, int b, int c, vec3 target, vec3 pole, float weight = 1.f);
void lookAtIK(const Skeleton& sk, Pose& pose, PoseWorld& w, int head, vec3 target, vec3 forwardLocal, float maxAngle, float weight);

// ---- Verlet secondary motion ---------------------------------------------------------------
struct VerletCloth {
    int cols = 0, rows = 0;
    std::vector<vec3> p, prev;
    std::vector<float> pinW;    // 1 = pinned
    float restX = 0.1f, restY = 0.1f;
    float damping = 0.03f;
    void init(int c, int r, float width, float height, vec3 origin, vec3 right, vec3 down);
    // pins: positions for the first row, colliders: spheres (xyz, radius)
    void simulate(float dt, const std::vector<vec3>& pins, vec3 gravity, vec3 wind, const std::vector<vec4>& spheres, vec3 planeN, float planeD, int iterations = 3);
    vec3 at(int c, int r) const { return p[r * cols + c]; }
};

struct VerletChain {
    std::vector<vec3> p, prev;
    float segLen = 0.1f, damping = 0.06f, stiffness = 0.2f;
    void init(int n, float seg, vec3 root, vec3 dir);
    void simulate(float dt, vec3 root, vec3 restDir, vec3 gravity, vec3 wind, const std::vector<vec4>& spheres);
};

}  // namespace agr
