#include "agr/animation/IK3D.hpp"

#include <algorithm>
#include <cmath>

#include "agr/scene/Components.hpp"
#include "agr/scene/Reflection.hpp"

namespace agr {

using math::Mat4;
using math::Quat;
using math::Vec3;

namespace {

struct WorldPose {
    Vec3 position;
    Quat rotation;
};

WorldPose worldPose(Scene& scene, ecs::Entity e) {
    Vec3 t, s;
    Quat r;
    math::decompose(scene.worldMatrix(e), t, r, s);
    return {t, math::normalize(r)};
}

Vec3 safeNormalize(Vec3 v, Vec3 fallback) {
    const float l = math::length(v);
    return l > 1e-6f ? v * (1.0f / l) : fallback;
}

float angleBetween(Vec3 a, Vec3 b) { return std::acos(std::clamp(math::dot(a, b), -1.0f, 1.0f)); }

/// Local rotation such that the bone's world rotation becomes R * (its current world rotation).
Quat rotateInWorld(const Quat& local, const Quat& world, const Quat& r) { return math::normalize(local * (world.conjugate() * r * world)); }

ecs::Entity firstChild(Scene& scene, ecs::Entity e) {
    ecs::Entity out{};
    scene.forEachChild(e, [&](ecs::Entity c) {
        if (out.isNull()) out = c;
    });
    return out;
}

} // namespace

void registerIK3DComponents() {
    auto& reg = ComponentRegistry::instance();
    if (reg.find<TwoBoneIK3DComponent>()) return;
    reg.add<TwoBoneIK3DComponent>("TwoBoneIK3D")
        .category("Animation")
        .description("On an upper bone (arm, thigh): bends it and the next bone so the tip (hand, foot) reaches the target.")
        .field("target", &TwoBoneIK3DComponent::target, {.tooltip = "Entity the tip reaches for"})
        .field("pole", &TwoBoneIK3DComponent::pole, {.tooltip = "Optional: the elbow / knee points toward it"})
        .field("mid", &TwoBoneIK3DComponent::mid, {.tooltip = "Middle bone (default: first child)"})
        .field("tip", &TwoBoneIK3DComponent::tip, {.tooltip = "End bone (default: first child of the middle bone)"})
        .field("weight", &TwoBoneIK3DComponent::weight, {.rangeMin = 0.0f, .rangeMax = 1.0f, .speed = 0.01f})
        .field("enabled", &TwoBoneIK3DComponent::enabled);
    reg.add<LookAtIK3DComponent>("LookAtIK3D")
        .category("Animation")
        .description("Turns the bone (head, eyes, turret) toward the target, within an angle of its animated direction.")
        .field("target", &LookAtIK3DComponent::target)
        .field("forwardAxis", &LookAtIK3DComponent::forwardAxis, {.tooltip = "Local axis that points at the target", .speed = 0.01f})
        .field("maxAngle", &LookAtIK3DComponent::maxAngle, {.flags = FieldFlag_Angle, .rangeMin = 0.0f, .rangeMax = 3.1416f, .speed = 0.01f})
        .field("weight", &LookAtIK3DComponent::weight, {.rangeMin = 0.0f, .rangeMax = 1.0f, .speed = 0.01f})
        .field("enabled", &LookAtIK3DComponent::enabled);
}

bool solveTwoBoneIK3D(Scene& scene, ecs::Entity upper, const TwoBoneIK3DComponent& ik) {
    if (!ik.enabled || ik.weight <= 0.0f || !scene.valid(ik.target)) return false;
    const ecs::Entity mid = scene.valid(ik.mid) ? ik.mid : firstChild(scene, upper);
    const ecs::Entity tip = scene.valid(ik.tip) ? ik.tip : (mid.isNull() ? ecs::Entity{} : firstChild(scene, mid));
    if (mid.isNull() || tip.isNull()) return false;
    auto& reg = scene.registry();
    auto& lu = reg.get<TransformComponent>(upper);
    auto& lm = reg.get<TransformComponent>(mid);
    const WorldPose A = worldPose(scene, upper), B = worldPose(scene, mid), C = worldPose(scene, tip);
    const Vec3 a = A.position, b = B.position, c = C.position, t = worldPose(scene, ik.target).position;
    const float lab = math::length(b - a), lcb = math::length(c - b);
    if (lab < 1e-5f || lcb < 1e-5f) return false;
    const float lat = std::clamp(math::length(t - a), std::abs(lab - lcb) + 1e-4f, lab + lcb - 1e-4f);
    // Current and wanted inner angles (law of cosines), in the plane of the chain.
    const Vec3 ac = safeNormalize(c - a, {0, 0, 1}), ab = safeNormalize(b - a, {0, 1, 0}), ba = ab * -1.0f, bc = safeNormalize(c - b, {0, 0, 1});
    const float acab0 = angleBetween(ac, ab), babc0 = angleBetween(ba, bc);
    const float acab1 = std::acos(std::clamp((lcb * lcb - lab * lab - lat * lat) / (-2.0f * lab * lat), -1.0f, 1.0f));
    const float babc1 = std::acos(std::clamp((lat * lat - lab * lab - lcb * lcb) / (-2.0f * lab * lcb), -1.0f, 1.0f));
    // Bending axis: normal of the chain's plane (toward the pole when there is one).
    const Vec3 bendRef = scene.valid(ik.pole) ? worldPose(scene, ik.pole).position - a : b - a;
    Vec3 axis0 = math::cross(c - a, bendRef);
    if (math::length(axis0) < 1e-6f) axis0 = math::cross(c - a, Vec3{0, 1, 0});
    if (math::length(axis0) < 1e-6f) axis0 = Vec3{1, 0, 0};
    axis0 = math::normalize(axis0);
    const Quat r0 = Quat::fromAxisAngle(axis0, acab1 - acab0);
    const Quat r1 = Quat::fromAxisAngle(axis0, babc1 - babc0);
    // Swing the whole chain so that a -> c points at the target.
    const Vec3 at = safeNormalize(t - a, ac);
    Vec3 axis2 = math::cross(ac, at);
    Quat r2 = Quat::identity();
    if (math::length(axis2) > 1e-6f) r2 = Quat::fromAxisAngle(math::normalize(axis2), angleBetween(ac, at));
    Quat R = math::normalize(r2 * r0);
    // Twist about a -> target so the elbow / knee faces the pole.
    if (scene.valid(ik.pole)) {
        const Vec3 p = worldPose(scene, ik.pole).position;
        const Vec3 elbow = R * (b - a);
        const Vec3 poleDir = p - a;
        const Vec3 e2 = elbow - at * math::dot(elbow, at), p2 = poleDir - at * math::dot(poleDir, at);
        if (math::length(e2) > 1e-5f && math::length(p2) > 1e-5f) {
            const Vec3 en = math::normalize(e2), pn = math::normalize(p2);
            const float sign = math::dot(math::cross(en, pn), at) < 0.0f ? -1.0f : 1.0f;
            R = math::normalize(Quat::fromAxisAngle(at, sign * angleBetween(en, pn)) * R);
        }
    }
    const Quat upperNew = rotateInWorld(lu.rotation, A.rotation, R);
    const Quat midNew = rotateInWorld(lm.rotation, B.rotation, r1);
    const float w = std::clamp(ik.weight, 0.0f, 1.0f);
    lu.rotation = w >= 1.0f ? upperNew : math::slerp(lu.rotation, upperNew, w);
    lm.rotation = w >= 1.0f ? midNew : math::slerp(lm.rotation, midNew, w);
    return true;
}

u32 solveIK3D(Scene& scene) {
    auto& reg = scene.registry();
    u32 solved = 0;
    reg.view<TwoBoneIK3DComponent>().each([&](ecs::Entity e, TwoBoneIK3DComponent& ik) {
        if (solveTwoBoneIK3D(scene, e, ik)) ++solved;
    });
    reg.view<LookAtIK3DComponent>().each([&](ecs::Entity e, LookAtIK3DComponent& la) {
        if (!la.enabled || la.weight <= 0.0f || !scene.valid(la.target)) return;
        const WorldPose P = worldPose(scene, e);
        const Vec3 forward = safeNormalize(P.rotation * safeNormalize(la.forwardAxis, {0, 0, 1}), {0, 0, 1});
        const Vec3 wanted = safeNormalize(worldPose(scene, la.target).position - P.position, forward);
        Vec3 axis = math::cross(forward, wanted);
        if (math::length(axis) < 1e-6f) return;  // already looking at it (or straight behind: ambiguous)
        const float angle = std::min(angleBetween(forward, wanted), std::max(la.maxAngle, 0.0f)) * std::clamp(la.weight, 0.0f, 1.0f);
        auto& tr = reg.get<TransformComponent>(e);
        tr.rotation = rotateInWorld(tr.rotation, P.rotation, Quat::fromAxisAngle(math::normalize(axis), angle));
        ++solved;
    });
    return solved;
}

} // namespace agr
