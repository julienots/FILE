// Trail components: points recorded as the entity moves, read by the renderer.
#include <algorithm>

#include "agr/scene/Components.hpp"
#include "agr/twod/Particles2D.hpp"

namespace agr {

u32 updateTrails(Scene& scene, float dt) {
    auto& reg = scene.registry();
    std::vector<ecs::Entity> missing;
    reg.view<TrailComponent>().each([&](ecs::Entity e, TrailComponent&) {
        if (!reg.has<TrailStateComponent>(e)) missing.push_back(e);
    });
    for (ecs::Entity e : missing) reg.emplace<TrailStateComponent>(e);
    u32 points = 0;
    reg.view<TrailComponent, TrailStateComponent, WorldTransformComponent>().each([&](ecs::Entity, TrailComponent& t, TrailStateComponent& st, WorldTransformComponent& w) {
        st.time += std::max(dt, 0.0f);
        auto& pts = st.points;
        // Expired points leave from the tail (oldest first).
        const float life = std::max(t.lifetime, 1e-3f);
        usize expired = 0;
        while (expired < pts.size() && st.time - pts[expired].time > life) ++expired;
        if (expired) pts.erase(pts.begin(), pts.begin() + static_cast<std::ptrdiff_t>(expired));
        if (t.emitting && w.activeInHierarchy) {
            const math::Vec3 pos = w.matrix.translation();
            if (pts.size() < 2) {
                pts.push_back({pos, st.time});
            } else {
                // The head follows the entity; a new point is fixed every minDistance.
                const TrailPoint& anchor = pts[pts.size() - 2];
                const bool far = math::length(pos - anchor.position) >= std::max(t.minDistance, 1e-4f);
                pts.back() = {pos, st.time};
                if (far) pts.push_back({pos, st.time});  // the head becomes a fixed point; a new head starts there
            }
            const usize cap = std::max<usize>(2, t.maxPoints);
            if (pts.size() > cap) pts.erase(pts.begin(), pts.begin() + static_cast<std::ptrdiff_t>(pts.size() - cap));
        }
        points += static_cast<u32>(pts.size());
    });
    return points;
}

} // namespace agr
