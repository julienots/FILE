#include "Animation.h"
#include <algorithm>

namespace agr {

int Skeleton::add(const std::string& name, int parent, vec3 offset) {
    Bone b; b.name = name; b.parent = parent; b.offset = offset;
    bones.push_back(b);
    return (int)bones.size() - 1;
}

void Skeleton::finalize() {
    for (auto& b : bones) b.bindWorld = (b.parent >= 0 ? bones[b.parent].bindWorld : vec3(0)) + b.offset;
}

int Skeleton::find(const std::string& name) const {
    for (int i = 0; i < (int)bones.size(); i++) if (bones[i].name == name) return i;
    return -1;
}

void blendPose(Pose& out, const Pose& a, const Pose& b, float t, int n) {
    for (int i = 0; i < n; i++) { out.rot[i] = nlerp(a.rot[i], b.rot[i], t); out.scale[i] = lerp(a.scale[i], b.scale[i], t); }
    out.rootOffset = lerp(a.rootOffset, b.rootOffset, t);
}

void blendPoseMasked(Pose& out, const Pose& a, const Pose& b, float t, const float* mask, int n) {
    for (int i = 0; i < n; i++) {
        float w = t * mask[i];
        out.rot[i] = nlerp(a.rot[i], b.rot[i], w);
        out.scale[i] = lerp(a.scale[i], b.scale[i], w);
    }
    out.rootOffset = lerp(a.rootOffset, b.rootOffset, t * mask[0]);
}

void addPose(Pose& out, const Pose& add, float w, int n) {
    for (int i = 0; i < n; i++) out.rot[i] = normalize(out.rot[i] * nlerp(quat(), add.rot[i], w));
    out.rootOffset += add.rootOffset * w;
}

void computeWorld(const Skeleton& sk, const Pose& p, PoseWorld& o) {
    int n = sk.size();
    for (int i = 0; i < n; i++) {
        const Bone& b = sk.bones[i];
        vec3 off = b.offset + (i == 0 ? p.rootOffset : vec3(0));
        mat4 local = mat4::trs(off, p.rot[i], p.scale[i]);
        if (b.parent >= 0) {
            o.world[i] = o.world[b.parent] * local;
            o.wrot[i] = o.wrot[b.parent] * p.rot[i];
        } else {
            o.world[i] = local;
            o.wrot[i] = p.rot[i];
        }
        o.wpos[i] = o.world[i].translation();
        o.skin[i] = o.world[i] * mat4::translate(-b.bindWorld);
    }
}

static float applyEase(Ease e, float t) {
    switch (e) {
    case Ease::Linear: return t;
    case Ease::Smooth: return t * t * (3 - 2 * t);
    case Ease::In: return t * t;
    case Ease::Out: return 1 - (1 - t) * (1 - t);
    case Ease::OutBack: return easeOutBack(t);
    }
    return t;
}

Clip& Clip::key(float t, std::initializer_list<std::pair<int, quat>> bones, Ease e) {
    for (auto& b : bones) {
        auto& ch = channels[b.first];
        ch.push_back({t, b.second, e});
        std::sort(ch.begin(), ch.end(), [](const Key& x, const Key& y) { return x.t < y.t; });
    }
    return *this;
}

void Clip::sample(float t, Pose& out, const Pose* base, int nBones, float* mask) const {
    if (base) out = *base;
    if (loop) t = t - std::floor(t);
    t = saturate(t);
    for (auto& kv : channels) {
        int bone = kv.first;
        if (bone >= nBones) continue;
        const auto& ks = kv.second;
        if (ks.empty()) continue;
        quat q;
        if (t <= ks.front().t) q = ks.front().q;
        else if (t >= ks.back().t) {
            if (loop && ks.size() > 1) {  // wrap last -> first
                float span = 1.f - ks.back().t + ks.front().t;
                float f = span > 1e-4f ? (t - ks.back().t) / span : 0.f;
                q = nlerp(ks.back().q, ks.front().q, applyEase(ks.front().ease, f));
            } else q = ks.back().q;
        } else {
            size_t i = 1;
            while (i < ks.size() && ks[i].t < t) i++;
            const Key& a = ks[i - 1]; const Key& b = ks[i];
            float f = (t - a.t) / std::max(1e-5f, b.t - a.t);
            q = slerp(a.q, b.q, applyEase(b.ease, f));
        }
        out.rot[bone] = q;
        if (mask) mask[bone] = 1.f;
    }
    if (!rootKeys.empty()) {
        vec3 r;
        if (t <= rootKeys.front().first) r = rootKeys.front().second;
        else if (t >= rootKeys.back().first) r = rootKeys.back().second;
        else {
            size_t i = 1;
            while (i < rootKeys.size() && rootKeys[i].first < t) i++;
            float f = (t - rootKeys[i - 1].first) / std::max(1e-5f, rootKeys[i].first - rootKeys[i - 1].first);
            r = lerp(rootKeys[i - 1].second, rootKeys[i].second, f * f * (3 - 2 * f));
        }
        out.rootOffset = r;
    }
}

float Clip::motionAt(float t) const {
    if (motion.empty()) return 0;
    t = saturate(t);
    if (t <= motion.front().first) return motion.front().second * (motion.front().first > 0 ? t / motion.front().first : 1.f);
    for (size_t i = 1; i < motion.size(); i++)
        if (t <= motion[i].first) {
            float f = (t - motion[i - 1].first) / std::max(1e-5f, motion[i].first - motion[i - 1].first);
            f = 1 - (1 - f) * (1 - f);  // decelerate within each segment (natural lunges)
            return lerpf(motion[i - 1].second, motion[i].second, f);
        }
    return motion.back().second;
}

// ---------------------------------------------------------------------------------------------
void Animator::play(const Clip* c, float speedMul, bool upper, float fadeIn) {
    if (!c) return;
    // Crossfade from whatever is currently displayed.
    snapshot = pose;
    act = c; actT = 0; actSpeed = speedMul; actUpper = upper; actStopping = false;
    actFadeIn = fadeIn; actWeight = 0.f;
    firedEvents.clear();
}

void Animator::stopAction(float fadeOut) {
    if (!act) return;
    actStopping = true; actFadeOut = std::max(0.01f, fadeOut);
}

void Animator::setState(BodyState s, float fade) {
    if (s == state) return;
    snapshot = pose;
    state = s;
    stateBlend = 0; stateFade = std::max(0.01f, fade);
}

void Animator::evalState(BodyState s, Pose& out) {
    out.reset();
    const ProceduralFn* fn = &locomotion;
    switch (s) {
    case BodyState::Airborne: if (airborne) fn = &airborne; break;
    case BodyState::Swim: if (swim) fn = &swim; break;
    case BodyState::Climb: if (climb) fn = &climb; break;
    case BodyState::Mounted: if (mounted) fn = &mounted; break;
    case BodyState::Dead: case BodyState::Downed: if (dead) fn = &dead; break;
    case BodyState::Stagger: if (stagger) fn = &stagger; break;
    case BodyState::Victory: if (victory) fn = &victory; break;
    case BodyState::Sit: if (sit) fn = &sit; break;
    case BodyState::Sleep: if (sleep) fn = &sleep; break;
    case BodyState::Work: if (work) fn = &work; break;
    default: break;
    }
    if (*fn) (*fn)(*this, out);
}

void Animator::update(float dt) {
    if (!skeleton) return;
    int n = skeleton->size();
    time += dt;
    // Phase-synchronised locomotion: stride length interpolates with speed so feet don't slide.
    float stride = speed < gait.walkSpeed ? gait.strideWalk
                 : lerpf(gait.strideWalk, gait.strideRun, saturate((speed - gait.walkSpeed) / (gait.runSpeed - gait.walkSpeed)));
    if (speed > 1.6f) stride *= 1.f + saturate((speed - gait.runSpeed) / (gait.sprintSpeed - gait.runSpeed)) * gait.sprintStride;
    float prevPhase = phase;
    if (speed > 0.05f) phase += speed / std::max(0.2f, stride) * dt;
    else phase += dt * 0.05f;  // idle drift keeps cycles alive
    if (phase >= 1.f) phase -= std::floor(phase);
    if (speed > 0.3f && state == BodyState::Locomotion && grounded) {
        // contact points at phase 0 (left) and 0.5 (right)
        if ((prevPhase < 0.5f && phase >= 0.5f)) { footstepEvent++; lastFoot = 1; }
        if (phase < prevPhase) { footstepEvent++; lastFoot = 0; }
    }

    Pose base;
    evalState(state, base);
    if (stateBlend < 1.f) {
        stateBlend = std::min(1.f, stateBlend + dt / stateFade);
        Pose tmp; blendPose(tmp, snapshot, base, easeInOut(stateBlend), n); base = tmp;
    }

    firedEvents.clear();
    if (act) {
        float prevT = actT;
        actT += dt * actSpeed / std::max(0.05f, act->duration);
        for (auto& e : act->events) if (e.time > prevT && e.time <= actT) firedEvents.push_back(e.id);
        if (actStopping) actWeight -= dt / actFadeOut;
        else actWeight = std::min(1.f, actWeight + dt / std::max(0.01f, actFadeIn));
        if (actT >= 1.f && !act->loop && !actStopping) { actStopping = true; actFadeOut = 0.18f; }
        if (actStopping && actWeight <= 0.f) { act = nullptr; actWeight = 0; }
    }
    if (act) {
        Pose a;
        float mask[MAX_BONES];
        for (int i = 0; i < MAX_BONES; i++) mask[i] = 0;
        act->sample(act->loop ? actT : std::min(actT, 1.f), a, &base, n, mask);
        float w = easeInOut(saturate(actWeight));
        if (actUpper) {
            float m[MAX_BONES];
            for (int i = 0; i < n; i++) m[i] = upperMask[i] * mask[i];
            blendPoseMasked(pose, base, a, w, m, n);
            pose.rootOffset = base.rootOffset;
        } else {
            for (int i = 0; i < n; i++) mask[i] = 1.f;
            blendPose(pose, base, a, w, n);
        }
    } else {
        pose = base;
    }
}

// ---------------------------------------------------------------------------------------------
void twoBoneIK(const Skeleton& sk, Pose& pose, PoseWorld& w, int a, int b, int c, vec3 T, vec3 pole, float weight) {
    if (weight <= 0.001f) return;
    vec3 A = w.wpos[a], B = w.wpos[b], C = w.wpos[c];
    float lab = length(B - A), lcb = length(C - B);
    vec3 at = T - A;
    float lat = clampf(length(at), 0.01f, (lab + lcb) * 0.999f);
    vec3 dir = normalize(at);
    vec3 pd = pole - dir * dot(pole, dir);
    if (length2(pd) < 1e-6f) pd = normalize(cross(dir, vec3(1, 0, 0)));
    pd = normalize(pd);
    float cosA = clampf((lab * lab + lat * lat - lcb * lcb) / (2 * lab * lat), -1, 1);
    float sinA = std::sqrt(std::max(0.f, 1 - cosA * cosA));
    vec3 Bn = A + dir * (cosA * lab) + pd * (sinA * lab);
    // rotate upper bone
    quat qa = fromTo(B - A, Bn - A);
    qa = nlerp(quat(), qa, weight);
    int pa = sk.bones[a].parent;
    quat parentRot = pa >= 0 ? w.wrot[pa] : quat();
    pose.rot[a] = normalize(parentRot.conj() * (qa * w.wrot[a]));
    computeWorld(sk, pose, w);
    // rotate lower bone
    vec3 B2 = w.wpos[b], C2 = w.wpos[c];
    vec3 Tc = A + dir * lat;
    quat qb = fromTo(C2 - B2, Tc - B2);
    qb = nlerp(quat(), qb, weight);
    pose.rot[b] = normalize(w.wrot[a].conj() * (qb * w.wrot[b]));
    computeWorld(sk, pose, w);
}

void lookAtIK(const Skeleton& sk, Pose& pose, PoseWorld& w, int head, vec3 target, vec3 fwdLocal, float maxAngle, float weight) {
    if (weight <= 0.001f) return;
    vec3 fwd = w.wrot[head].rotate(fwdLocal);
    vec3 want = normalize(target - w.wpos[head]);
    float ang = std::acos(clampf(dot(fwd, want), -1, 1));
    if (ang < 1e-3f) return;
    float lim = std::min(1.f, maxAngle / ang);
    quat d = nlerp(quat(), fromTo(fwd, want), lim * weight);
    int p = sk.bones[head].parent;
    quat pr = p >= 0 ? w.wrot[p] : quat();
    pose.rot[head] = normalize(pr.conj() * (d * w.wrot[head]));
    computeWorld(sk, pose, w);
}

// ---------------------------------------------------------------------------------------------
void VerletCloth::init(int c, int r, float width, float height, vec3 o, vec3 right, vec3 down) {
    cols = c; rows = r;
    restX = width / (c - 1); restY = height / (r - 1);
    p.resize(c * r); prev.resize(c * r); pinW.assign(c * r, 0.f);
    for (int y = 0; y < r; y++)
        for (int x = 0; x < c; x++) {
            p[y * c + x] = o + right * (restX * x) + down * (restY * y);
            prev[y * c + x] = p[y * c + x];
            if (y == 0) pinW[y * c + x] = 1.f;
        }
}

static inline void satisfy(vec3& a, vec3& b, float rest, float wa, float wb, float stiff) {
    vec3 d = b - a;
    float l = length(d);
    if (l < 1e-6f) return;
    float diff = (l - rest) / l * stiff;
    float s = wa + wb;
    if (s <= 0) return;
    a += d * (diff * wa / s);
    b -= d * (diff * wb / s);
}

void VerletCloth::simulate(float dt, const std::vector<vec3>& pins, vec3 g, vec3 wind, const std::vector<vec4>& spheres, vec3 pn, float pd, int iters) {
    dt = std::min(dt, 1.f / 30.f);
    float dt2 = dt * dt;
    for (int i = 0; i < cols * rows; i++) {
        if (pinW[i] >= 1.f) continue;
        vec3 v = (p[i] - prev[i]) * (1.f - damping);
        prev[i] = p[i];
        int row = i / cols;
        float flutter = 0.6f + 0.4f * std::sin(i * 1.7f + p[i].x * 3.f + p[i].z * 2.f);
        p[i] += v + (g + wind * (flutter * (float)row / rows)) * dt2;
    }
    for (int x = 0; x < cols && x < (int)pins.size(); x++) { p[x] = pins[x]; prev[x] = pins[x]; }
    for (int it = 0; it < iters; it++) {
        for (int y = 0; y < rows; y++)
            for (int x = 0; x < cols; x++) {
                int i = y * cols + x;
                float wi = 1.f - pinW[i];
                if (x + 1 < cols) satisfy(p[i], p[i + 1], restX, wi, 1.f - pinW[i + 1], 1.f);
                if (y + 1 < rows) satisfy(p[i], p[i + cols], restY, wi, 1.f - pinW[i + cols], 1.f);
                if (x + 1 < cols && y + 1 < rows) satisfy(p[i], p[i + cols + 1], std::sqrt(restX * restX + restY * restY), wi, 1.f - pinW[i + cols + 1], 0.5f);
                if (y + 2 < rows) satisfy(p[i], p[i + 2 * cols], restY * 2, wi, 1.f - pinW[i + 2 * cols], 0.3f);  // bending
            }
        for (int i = cols; i < cols * rows; i++) {
            for (auto& s : spheres) {
                vec3 c{s.x, s.y, s.z};
                vec3 d = p[i] - c;
                float l = length(d);
                if (l < s.w && l > 1e-5f) p[i] = c + d * (s.w / l);
            }
            float side = dot(p[i], pn) + pd;
            if (side < 0) p[i] -= pn * side;
        }
    }
}

void VerletChain::init(int n, float seg, vec3 root, vec3 dir) {
    segLen = seg;
    p.resize(n); prev.resize(n);
    for (int i = 0; i < n; i++) p[i] = prev[i] = root + dir * (seg * i);
}

void VerletChain::simulate(float dt, vec3 root, vec3 restDir, vec3 g, vec3 wind, const std::vector<vec4>& spheres) {
    dt = std::min(dt, 1.f / 30.f);
    int n = (int)p.size();
    if (!n) return;
    p[0] = prev[0] = root;
    for (int i = 1; i < n; i++) {
        vec3 v = (p[i] - prev[i]) * (1.f - damping);
        prev[i] = p[i];
        // stiffness pulls each node toward its rest direction (keeps braids/tails shaped)
        vec3 rest = p[i - 1] + restDir * segLen;
        p[i] += v + (g + wind) * dt * dt + (rest - p[i]) * stiffness;
    }
    for (int it = 0; it < 3; it++) {
        for (int i = 1; i < n; i++) {
            vec3 d = p[i] - p[i - 1];
            float l = length(d);
            if (l > 1e-6f) p[i] = p[i - 1] + d * (segLen / l);
            for (auto& s : spheres) {
                vec3 c{s.x, s.y, s.z};
                vec3 e = p[i] - c;
                float le = length(e);
                if (le < s.w && le > 1e-5f) p[i] = c + e * (s.w / le);
            }
        }
    }
}

}  // namespace agr
