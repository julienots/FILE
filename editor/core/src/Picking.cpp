#include "agr/editor/Picking.hpp"

#include <array>
#include <cmath>

#include "agr/render/Primitives.hpp"
#include "agr/render/RenderComponents.hpp"

namespace agr::editor {

using math::Vec3;

namespace {

const math::Aabb& primitiveBounds(PrimitiveMesh type) {
    static const std::array<math::Aabb, 4> bounds = {makeCube().bounds, makeSphere().bounds, makePlane().bounds,
                                                     makeQuad().bounds};
    return bounds[static_cast<usize>(std::clamp(static_cast<int>(type), 0, 3))];
}

bool pickable(Scene& scene, ecs::Entity e) {
    const auto* w = scene.registry().tryGet<WorldTransformComponent>(e);
    const auto* s = scene.registry().tryGet<StateComponent>(e);
    if (w && !w->activeInHierarchy) return false;
    return !(s && (s->hiddenInEditor || s->lockedInEditor));
}

std::array<Vec3, 4> spriteCorners(const SpriteRendererComponent& s, const math::Mat4& m) {
    const float x0 = -s.pivot.x * s.size.x, x1 = (1.0f - s.pivot.x) * s.size.x;
    const float y0 = -s.pivot.y * s.size.y, y1 = (1.0f - s.pivot.y) * s.size.y;
    return {m.transformPoint({x0, y0, 0}), m.transformPoint({x1, y0, 0}), m.transformPoint({x1, y1, 0}),
            m.transformPoint({x0, y1, 0})};
}

} // namespace

float intersectPlane(const math::Ray& ray, Vec3 n, float d) {
    const float denom = math::dot(n, ray.direction);
    if (std::fabs(denom) < 1e-8f) return -1.0f;
    const float t = (d - math::dot(n, ray.origin)) / denom;
    return t >= 0.0f ? t : -1.0f;
}

std::optional<PickHit> pick(Scene& scene, const math::Ray& ray) {
    std::optional<PickHit> best;
    auto consider = [&](ecs::Entity e, float t) {
        if (t >= 0.0f && (!best || t < best->distance)) best = PickHit{e, t, ray.at(t)};
    };
    auto& reg = scene.registry();
    reg.view<MeshRendererComponent, WorldTransformComponent>().each(
        [&](ecs::Entity e, MeshRendererComponent& mr, WorldTransformComponent& w) {
            if (!mr.visible || !pickable(scene, e)) return;
            consider(e, math::intersect(ray, primitiveBounds(mr.mesh).transformed(w.matrix)));
        });
    reg.view<SpriteRendererComponent, WorldTransformComponent>().each(
        [&](ecs::Entity e, SpriteRendererComponent& s, WorldTransformComponent& w) {
            if (!s.visible || !pickable(scene, e)) return;
            const auto c = spriteCorners(s, w.matrix);
            const Vec3 e1 = c[1] - c[0], e3 = c[3] - c[0];
            const Vec3 n = math::cross(e1, e3);
            if (math::lengthSquared(n) < 1e-12f) return;
            const float t = intersectPlane(ray, n, math::dot(n, c[0]));
            if (t < 0.0f) return;
            const Vec3 p = ray.at(t) - c[0];
            const float u = math::dot(p, e1) / math::dot(e1, e1), v = math::dot(p, e3) / math::dot(e3, e3);
            if (u >= 0.0f && u <= 1.0f && v >= 0.0f && v <= 1.0f) consider(e, t);
        });
    return best;
}

math::Aabb worldBounds(Scene& scene, ecs::Entity e) {
    math::Aabb box;
    auto& reg = scene.registry();
    const auto* w = reg.tryGet<WorldTransformComponent>(e);
    if (!w) return box;
    if (const auto* mr = reg.tryGet<MeshRendererComponent>(e)) box = primitiveBounds(mr->mesh).transformed(w->matrix);
    if (const auto* s = reg.tryGet<SpriteRendererComponent>(e))
        for (const Vec3& c : spriteCorners(*s, w->matrix)) box.expand(c);
    if (!box.valid()) box = math::Aabb::fromCenterExtents(w->matrix.translation(), Vec3{0.5f});
    return box;
}

} // namespace agr::editor
