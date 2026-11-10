#include "agr/editor/Picking.hpp"

#include <array>
#include <cmath>

#include "agr/render/ModelLibrary.hpp"
#include "agr/render/Primitives.hpp"
#include "agr/render/RenderComponents.hpp"
#include "agr/render/SpriteSheet.hpp"
#include "agr/twod/TilemapEditing.hpp"

namespace agr::editor {

using math::Vec3;

namespace {

const math::Aabb& primitiveBounds(PrimitiveMesh type) {
    static const std::array<math::Aabb, 4> bounds = {makeCube().bounds, makeSphere().bounds, makePlane().bounds,
                                                     makeQuad().bounds};
    return bounds[static_cast<usize>(std::clamp(static_cast<int>(type), 0, 3))];
}

/// Local bounds of a mesh renderer: built-in mesh or loaded model mesh.
math::Aabb meshBounds(const MeshRendererComponent& mr, ModelLibrary* models) {
    if (!mr.model.empty()) {
        const ModelAsset* m = models ? models->get(mr.model) : nullptr;
        if (m && m->state == AssetState::Ready && mr.meshIndex >= 0 && static_cast<usize>(mr.meshIndex) < m->meshes.size())
            return m->meshes[static_cast<usize>(mr.meshIndex)].bounds;
        return primitiveBounds(PrimitiveMesh::Cube);
    }
    return primitiveBounds(mr.mesh);
}

bool pickable(Scene& scene, ecs::Entity e) {
    const auto* w = scene.registry().tryGet<WorldTransformComponent>(e);
    const auto* s = scene.registry().tryGet<StateComponent>(e);
    if (w && !w->activeInHierarchy) return false;
    return !(s && (s->hiddenInEditor || s->lockedInEditor));
}

std::array<Vec3, 4> spriteCorners(const SpriteRendererComponent& s, const math::Mat4& m, SpriteSheetLibrary* sheets) {
    math::Vec2 pivot = s.pivot;
    if (sheets && !s.sheet.empty())
        if (const SpriteSheet* sheet = sheets->get(s.sheet); sheet && sheet->validFrame(s.frame))
            pivot = sheet->frames[static_cast<u32>(s.frame)].pivot;
    const float x0 = -pivot.x * s.size.x, x1 = (1.0f - pivot.x) * s.size.x;
    const float y0 = -pivot.y * s.size.y, y1 = (1.0f - pivot.y) * s.size.y;
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

std::optional<PickHit> pick(Scene& scene, const math::Ray& ray, PickAssets assets) {
    SpriteSheetLibrary* sheets = assets.sheets;
    std::optional<PickHit> best;
    auto consider = [&](ecs::Entity e, float t) {
        if (t >= 0.0f && (!best || t < best->distance)) best = PickHit{e, t, ray.at(t)};
    };
    auto& reg = scene.registry();
    reg.view<MeshRendererComponent, WorldTransformComponent>().each(
        [&](ecs::Entity e, MeshRendererComponent& mr, WorldTransformComponent& w) {
            if (!mr.visible || !pickable(scene, e)) return;
            consider(e, math::intersect(ray, meshBounds(mr, assets.models).transformed(w.matrix)));
        });
    reg.view<SpriteRendererComponent, WorldTransformComponent>().each(
        [&](ecs::Entity e, SpriteRendererComponent& s, WorldTransformComponent& w) {
            if (!s.visible || !pickable(scene, e)) return;
            const auto c = spriteCorners(s, w.matrix, sheets);
            const Vec3 e1 = c[1] - c[0], e3 = c[3] - c[0];
            const Vec3 n = math::cross(e1, e3);
            if (math::lengthSquared(n) < 1e-12f) return;
            const float t = intersectPlane(ray, n, math::dot(n, c[0]));
            if (t < 0.0f) return;
            const Vec3 p = ray.at(t) - c[0];
            const float u = math::dot(p, e1) / math::dot(e1, e1), v = math::dot(p, e3) / math::dot(e3, e3);
            if (u >= 0.0f && u <= 1.0f && v >= 0.0f && v <= 1.0f) consider(e, t);
        });
    // Tilemaps are picked on painted cells only, so empty areas select what lies behind.
    reg.view<TilemapComponent, WorldTransformComponent>().each(
        [&](ecs::Entity e, TilemapComponent& tm, WorldTransformComponent& w) {
            if (!pickable(scene, e)) return;
            const auto cell = tilemapCellAt(w.matrix, tm, ray);
            if (!cell || !tilemapContains(tm, cell->first, cell->second) ||
                tm.tileAt(static_cast<u32>(cell->first), static_cast<u32>(cell->second)) < 0)
                return;
            const Vec3 n = w.matrix.transformVector({0, 0, 1});
            consider(e, intersectPlane(ray, n, math::dot(n, w.matrix.translation())));
        });
    return best;
}

math::Aabb worldBounds(Scene& scene, ecs::Entity e, PickAssets assets) {
    SpriteSheetLibrary* sheets = assets.sheets;
    math::Aabb box;
    auto& reg = scene.registry();
    const auto* w = reg.tryGet<WorldTransformComponent>(e);
    if (!w) return box;
    if (const auto* mr = reg.tryGet<MeshRendererComponent>(e)) box = meshBounds(*mr, assets.models).transformed(w->matrix);
    if (const auto* s = reg.tryGet<SpriteRendererComponent>(e))
        for (const Vec3& c : spriteCorners(*s, w->matrix, sheets)) box.expand(c);
    if (const auto* tm = reg.tryGet<TilemapComponent>(e)) {
        const math::Aabb tb = tilemapBounds(w->matrix, *tm);
        if (tb.valid()) {
            box.expand(tb.min);
            box.expand(tb.max);
        }
    }
    if (!box.valid()) box = math::Aabb::fromCenterExtents(w->matrix.translation(), Vec3{0.5f});
    return box;
}

} // namespace agr::editor
