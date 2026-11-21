#include "agr/animation/IK2D.hpp"

#include <algorithm>
#include <cmath>
#include <vector>

#include "Fabrik.hpp"
#include "agr/render/RenderComponents.hpp"
#include "agr/scene/Components.hpp"
#include "agr/scene/Reflection.hpp"

namespace agr {

using math::Mat4;
using math::Vec3;

namespace {

float zAngle(const math::Quat& q) { return 2.0f * std::atan2(q.z, q.w); }  // rotation about +Z (2D)
float wrap(float a) {
    while (a > 3.14159265f) a -= 6.2831853f;
    while (a < -3.14159265f) a += 6.2831853f;
    return a;
}
float axisAngle(const Mat4& m) { return std::atan2(m.c[0].y, m.c[0].x); }  // world angle of the local +X axis

} // namespace

void registerIK2DComponents() {
    auto& reg = ComponentRegistry::instance();
    if (reg.find<TwoBoneIK2DComponent>()) return;
    reg.add<TwoBoneIK2DComponent>("TwoBoneIK2D")
        .category("2D")
        .description("On an upper bone (arm, thigh): turns it and its child bone so the child's tip reaches the target.")
        .field("target", &TwoBoneIK2DComponent::target, {.tooltip = "Entity the chain reaches for (hand or foot target)"})
        .field("flipBend", &TwoBoneIK2DComponent::flipBend, {.tooltip = "Bend the other way (elbow / knee)"})
        .field("weight", &TwoBoneIK2DComponent::weight, {.rangeMin = 0.0f, .rangeMax = 1.0f, .speed = 0.01f})
        .field("enabled", &TwoBoneIK2DComponent::enabled);
    reg.add<ChainIK2DComponent>("ChainIK2D")
        .category("2D")
        .description("On the root Bone2D of a chain (tail, rope, tentacle): FABRIK, the last bone's tip reaches the target.")
        .field("target", &ChainIK2DComponent::target)
        .field("boneCount", &ChainIK2DComponent::boneCount, {.rangeMin = 1.0f, .rangeMax = 64.0f})
        .field("iterations", &ChainIK2DComponent::iterations, {.rangeMin = 1.0f, .rangeMax = 64.0f})
        .field("maxAngle", &ChainIK2DComponent::maxAngle, {.tooltip = "Bend limit at each joint", .flags = FieldFlag_Angle, .rangeMin = 0.0f, .rangeMax = 3.1416f})
        .field("jointLimits", &ChainIK2DComponent::jointLimits, {.tooltip = "Per joint limits in degrees, from the first inner joint: \"30, 90, 45\""})
        .field("weight", &ChainIK2DComponent::weight, {.rangeMin = 0.0f, .rangeMax = 1.0f, .speed = 0.01f})
        .field("enabled", &ChainIK2DComponent::enabled);
}

u32 solveIK2D(Scene& scene) {
    auto& reg = scene.registry();
    u32 solved = 0;
    reg.view<ChainIK2DComponent, Bone2DComponent>().each([&](ecs::Entity root, ChainIK2DComponent& ik, Bone2DComponent&) {
        if (!ik.enabled || ik.weight <= 0.0f || !scene.valid(ik.target) || ik.boneCount < 1) return;
        std::vector<ecs::Entity> bones{root};
        while (static_cast<i32>(bones.size()) < ik.boneCount) {
            ecs::Entity next{};
            scene.forEachChild(bones.back(), [&](ecs::Entity c) {
                if (next.isNull() && reg.has<Bone2DComponent>(c)) next = c;
            });
            if (next.isNull()) break;
            bones.push_back(next);
        }
        // Joints: each bone's origin, then the tip of the last bone.
        std::vector<Vec3> p;
        for (ecs::Entity b : bones) p.push_back(scene.worldMatrix(b).transformPoint(Vec3(0.0f)));
        p.push_back(scene.worldMatrix(bones.back()).transformPoint(Vec3(reg.get<Bone2DComponent>(bones.back()).length, 0.0f, 0.0f)));
        const float z = p[0].z;
        for (Vec3& q : p) q.z = z;
        Vec3 t = scene.worldMatrix(ik.target).transformPoint(Vec3(0.0f));
        t.z = z;
        const float maxA = std::clamp(ik.maxAngle, 0.0f, 3.1416f);
        fabrik::solve(p, t, std::clamp(ik.iterations, 1, 64), maxA, nullptr, fabrik::parseJointLimits(ik.jointLimits, maxA));
        const float w = std::clamp(ik.weight, 0.0f, 1.0f);
        for (usize i = 0; i < bones.size(); ++i) {  // each bone turns about Z toward its solved next joint
            const Mat4 world = scene.worldMatrix(bones[i]);
            const Vec3 o = world.transformPoint(Vec3(0.0f));
            const Vec3 nextNow = i + 1 < bones.size() ? scene.worldMatrix(bones[i + 1]).transformPoint(Vec3(0.0f))
                                                      : world.transformPoint(Vec3(reg.get<Bone2DComponent>(bones[i]).length, 0.0f, 0.0f));
            const float now = std::atan2(nextNow.y - o.y, nextNow.x - o.x), want = std::atan2(p[i + 1].y - o.y, p[i + 1].x - o.x);
            auto& tr = reg.get<TransformComponent>(bones[i]);
            tr.rotation = math::Quat::fromAxisAngle({0, 0, 1}, zAngle(tr.rotation) + wrap(want - now) * w);
        }
        ++solved;
    });
    reg.view<TwoBoneIK2DComponent, Bone2DComponent>().each([&](ecs::Entity upper, TwoBoneIK2DComponent& ik, Bone2DComponent&) {
        if (!ik.enabled || ik.weight <= 0.0f || !scene.valid(ik.target)) return;
        ecs::Entity lower{};
        scene.forEachChild(upper, [&](ecs::Entity c) {
            if (lower.isNull() && reg.has<Bone2DComponent>(c)) lower = c;
        });
        if (lower.isNull()) return;
        auto& tu = reg.get<TransformComponent>(upper);
        auto& tl = reg.get<TransformComponent>(lower);
        // Fresh world matrices (animations may have moved the chain this frame).
        const ecs::Entity parent = scene.parent(upper);
        const Mat4 parentWorld = parent.isNull() ? Mat4::identity() : scene.worldMatrix(parent);
        const Mat4 upperWorld = parentWorld * tu.localMatrix();
        const Mat4 lowerWorld = upperWorld * tl.localMatrix();
        const Vec3 o = upperWorld.transformPoint(Vec3(0.0f));
        const Vec3 elbowNow = lowerWorld.transformPoint(Vec3(0.0f));
        const Vec3 tipNow = lowerWorld.transformPoint(Vec3(reg.get<Bone2DComponent>(lower).length, 0.0f, 0.0f));
        const Vec3 t = scene.worldMatrix(ik.target).transformPoint(Vec3(0.0f));
        const float l1 = math::length(elbowNow - o), l2 = math::length(tipNow - elbowNow);
        if (l1 < 1e-5f || l2 < 1e-5f) return;
        const float dx = t.x - o.x, dy = t.y - o.y;
        const float d = std::clamp(std::sqrt(dx * dx + dy * dy), std::abs(l1 - l2) + 1e-4f, l1 + l2 - 1e-4f);
        const float toTarget = std::atan2(dy, dx);
        const float cosA = std::clamp((l1 * l1 + d * d - l2 * l2) / (2.0f * l1 * d), -1.0f, 1.0f);
        const float bend = std::acos(cosA) * (ik.flipBend ? -1.0f : 1.0f);
        const float upperDir = toTarget + bend;  // world direction from the shoulder to the elbow
        const Vec3 elbow(o.x + l1 * std::cos(upperDir), o.y + l1 * std::sin(upperDir), 0.0f);
        const float lowerDir = std::atan2(t.y - elbow.y, t.x - elbow.x);  // elbow to hand
        // Bone frames: the elbow may sit off the upper bone's X axis, the tip is on the lower's X axis.
        const float elbowOffset = std::atan2(tl.position.y, tl.position.x);
        const float upperFrame = upperDir - elbowOffset;
        const float upperLocal = upperFrame - axisAngle(parentWorld);
        const float lowerLocal = lowerDir - upperFrame;
        const float w = std::clamp(ik.weight, 0.0f, 1.0f);
        const float a1 = zAngle(tu.rotation), a2 = zAngle(tl.rotation);
        tu.rotation = math::Quat::fromAxisAngle({0, 0, 1}, a1 + wrap(upperLocal - a1) * w);
        tl.rotation = math::Quat::fromAxisAngle({0, 0, 1}, a2 + wrap(lowerLocal - a2) * w);
        ++solved;
    });
    return solved;
}

} // namespace agr
