#include "agr/render/Skin2D.hpp"

#include <algorithm>
#include <cmath>

#include "agr/render/RenderComponents.hpp"
#include "agr/scene/Components.hpp"

namespace agr {

using math::Mat4;
using math::Vec3;

namespace {

Mat4 restLocal(Scene& scene, ecs::Entity e) {
    auto& reg = scene.registry();
    if (const auto* b = reg.tryGet<Bone2DComponent>(e); b && b->restSet) return Mat4::trs(b->restPosition, b->restRotation, b->restScale);
    return reg.get<TransformComponent>(e).localMatrix();
}

/// Rest transform of `e` relative to `root` (bones at rest, other entities as they are).
Mat4 restRelative(Scene& scene, ecs::Entity e, ecs::Entity root) {
    Mat4 m = Mat4::identity();
    for (ecs::Entity cur = e; !cur.isNull() && cur != root; cur = scene.parent(cur)) m = restLocal(scene, cur) * m;
    return m;
}

void collectBones(Scene& scene, ecs::Entity e, std::vector<ecs::Entity>& out) {
    scene.forEachChild(e, [&](ecs::Entity c) {
        if (scene.registry().has<Bone2DComponent>(c)) out.push_back(c);
        collectBones(scene, c, out);
    });
}

float segmentDistanceSq(Vec3 p, Vec3 a, Vec3 b) {
    const Vec3 ab = b - a;
    const float len2 = math::dot(ab, ab);
    const float t = len2 > 1e-12f ? std::clamp(math::dot(p - a, ab) / len2, 0.0f, 1.0f) : 0.0f;
    const Vec3 d = p - (a + ab * t);
    return math::dot(d, d);
}

} // namespace

void captureBone2DRest(Scene& scene, ecs::Entity root) {
    auto& reg = scene.registry();
    auto capture = [&](ecs::Entity e, Bone2DComponent& b) {
        const auto& t = reg.get<TransformComponent>(e);
        b.restPosition = t.position;
        b.restRotation = t.rotation;
        b.restScale = t.scale;
        b.restSet = true;
    };
    if (root.isNull()) {
        reg.view<Bone2DComponent>().each(capture);
        return;
    }
    std::vector<ecs::Entity> bones;
    if (reg.has<Bone2DComponent>(root)) bones.push_back(root);
    collectBones(scene, root, bones);
    for (ecs::Entity b : bones) capture(b, reg.get<Bone2DComponent>(b));
}

ecs::Entity skeleton2DOf(Scene& scene, ecs::Entity e) {
    for (ecs::Entity cur = e; !cur.isNull(); cur = scene.parent(cur))
        if (scene.registry().has<Skeleton2DComponent>(cur)) return cur;
    return {};
}

bool skin2DPoints(Scene& scene, ecs::Entity skin, std::vector<Vec3>& out) {
    auto& reg = scene.registry();
    const auto* s = reg.tryGet<Skin2DComponent>(skin);
    if (!s) return false;
    const i32 cols = std::clamp(s->columns, 1, 64), rows = std::clamp(s->rows, 1, 64);
    const usize count = static_cast<usize>(cols + 1) * static_cast<usize>(rows + 1);
    out.resize(count);
    const float x0 = -s->pivot.x * s->size.x, y0 = -s->pivot.y * s->size.y;
    auto local = [&](i32 i, i32 j) {
        return Vec3(x0 + s->size.x * static_cast<float>(i) / static_cast<float>(cols), y0 + s->size.y * static_cast<float>(j) / static_cast<float>(rows), 0.0f);
    };
    const Mat4& skinWorld = reg.get<WorldTransformComponent>(skin).matrix;
    const ecs::Entity root = skeleton2DOf(scene, skin);
    std::vector<ecs::Entity> bones;
    if (!root.isNull()) collectBones(scene, root, bones);
    if (bones.empty()) {  // no skeleton: a flat grid
        for (i32 j = 0; j <= rows; ++j)
            for (i32 i = 0; i <= cols; ++i) out[static_cast<usize>(j * (cols + 1) + i)] = skinWorld.transformPoint(local(i, j));
        return true;
    }
    // Bones relative to the skeleton root: at rest (segment for the weights) and now (deformation).
    const Mat4& rootWorld = reg.get<WorldTransformComponent>(root).matrix;
    const Mat4 invRoot = math::inverse(rootWorld);
    struct BoneData {
        Mat4 skinning;  // rest (root space) -> current (root space)
        Vec3 a, b;      // rest segment, root space
    };
    std::vector<BoneData> data(bones.size());
    for (usize k = 0; k < bones.size(); ++k) {
        const Mat4 rest = restRelative(scene, bones[k], root);
        const Mat4 current = invRoot * reg.get<WorldTransformComponent>(bones[k]).matrix;
        data[k].skinning = current * math::inverse(rest);
        data[k].a = rest.transformPoint(Vec3(0.0f));
        data[k].b = rest.transformPoint(Vec3(std::max(reg.get<Bone2DComponent>(bones[k]).length, 0.0f), 0.0f, 0.0f));
    }
    const Mat4 skinRest = restRelative(scene, skin, root);
    const float power = std::max(s->stiffness, 0.5f) * 0.5f;  // applied to squared distances
    for (i32 j = 0; j <= rows; ++j) {
        for (i32 i = 0; i <= cols; ++i) {
            const Vec3 q = skinRest.transformPoint(local(i, j));
            // The 4 nearest bones, inverse distance weights.
            usize best[4] = {0, 0, 0, 0};
            float bestD[4] = {1e30f, 1e30f, 1e30f, 1e30f};
            for (usize k = 0; k < data.size(); ++k) {
                const float d = segmentDistanceSq(q, data[k].a, data[k].b);
                for (int slot = 0; slot < 4; ++slot) {
                    if (d < bestD[slot]) {
                        for (int m = 3; m > slot; --m) {
                            bestD[m] = bestD[m - 1];
                            best[m] = best[m - 1];
                        }
                        bestD[slot] = d;
                        best[slot] = k;
                        break;
                    }
                }
            }
            Vec3 p(0.0f);
            float total = 0.0f;
            for (int slot = 0; slot < 4 && bestD[slot] < 1e29f; ++slot) {
                const float w = 1.0f / std::pow(bestD[slot] + 1e-6f, power);
                p = p + data[best[slot]].skinning.transformPoint(q) * w;
                total += w;
            }
            out[static_cast<usize>(j * (cols + 1) + i)] = rootWorld.transformPoint(p * (1.0f / total));
        }
    }
    return true;
}

} // namespace agr
