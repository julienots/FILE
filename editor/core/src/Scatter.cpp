#include "agr/editor/Scatter.hpp"

#include <algorithm>
#include <cmath>

#include "agr/physics3d/Physics3DComponents.hpp"
#include "agr/render/RenderComponents.hpp"

namespace agr::editor::templates {

namespace {

/// SplitMix64: tiny, fast and identical everywhere (std distributions are not portable).
struct Rng {
    u64 state;
    u64 next() {
        u64 z = (state += 0x9E3779B97F4A7C15ull);
        z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ull;
        z = (z ^ (z >> 27)) * 0x94D049BB133111EBull;
        return z ^ (z >> 31);
    }
    float uniform() { return static_cast<float>(next() >> 40) / static_cast<float>(1ull << 24); }  // [0, 1)
    float range(float a, float b) { return a + (b - a) * uniform(); }
};

} // namespace

std::vector<ecs::Entity> scatter(Scene& scene, ecs::Entity parent, const ScatterSettings& s, const std::vector<ScatterItem>& items) {
    std::vector<ecs::Entity> out;
    float total = 0.0f;
    for (const ScatterItem& it : items) total += std::max(it.weight, 0.0f);
    if (items.empty() || total <= 0.0f) return out;
    struct Placed {
        math::Vec2 p;
        float r;
    };
    std::vector<Placed> placed;
    std::vector<u32> counters(items.size(), 0);
    Rng rng{static_cast<u64>(s.seed) * 0x2545F4914F6CDD1Dull + 1};
    auto& reg = scene.registry();
    for (u32 attempt = 0; attempt < s.attempts && placed.size() < s.maxCount; ++attempt) {
        // Weighted choice, then size and position.
        float pick = rng.uniform() * total;
        usize k = 0;
        while (k + 1 < items.size() && pick >= std::max(items[k].weight, 0.0f)) pick -= std::max(items[k++].weight, 0.0f);
        const ScatterItem& it = items[k];
        const float scale = rng.range(it.minScale, it.maxScale);
        const float r = it.radius * scale;
        const math::Vec2 p{rng.range(s.min.x, s.max.x), rng.range(s.min.y, s.max.y)};
        const float yaw = rng.range(0.0f, 6.2831853f);
        const float tintA = rng.range(-1.0f, 1.0f), tintB = rng.range(-1.0f, 1.0f);
        bool free = true;
        for (const auto& [mn, mx] : s.exclude)
            if (p.x > mn.x - r && p.x < mx.x + r && p.y > mn.y - r && p.y < mx.y + r) free = false;
        for (usize i = 0; free && i < placed.size(); ++i) {
            const float d = r + placed[i].r + s.spacing;
            const math::Vec2 dp = p - placed[i].p;
            if (dp.x * dp.x + dp.y * dp.y < d * d) free = false;
        }
        if (!free) continue;
        placed.push_back({p, r});
        const auto root = scene.createEntity(it.name + " " + std::to_string(++counters[k]), parent);
        auto& t = reg.get<TransformComponent>(root);
        t.position = {p.x, s.heightAt ? s.heightAt(p.x, p.y) : s.height, p.y};
        t.rotation = math::Quat::fromAxisAngle({0.0f, 1.0f, 0.0f}, yaw);
        t.scale = math::Vec3{scale};
        const auto model = scene.createEntity("Model", root);
        const auto skin = scene.createEntity("Skin", model);
        auto& mr = reg.emplace<MeshRendererComponent>(skin);
        mr.model = it.model;
        mr.meshIndex = 0;
        mr.toon = s.toon;
        mr.outlineWidth = it.outlineWidth;
        mr.castShadows = it.castShadows;
        // Brightness and a slight warm / cool shift: per-instance color, still drawn instanced.
        const float v = 1.0f - it.tint * (0.5f + 0.5f * tintA), w = it.tint * 0.5f * tintB;  // color multiplies: darker only
        mr.color = {std::clamp(v + w, 0.0f, 1.0f), std::clamp(v, 0.0f, 1.0f), std::clamp(v - w, 0.0f, 1.0f), 1.0f};
        if (it.colliderRadius > 0.0f) {  // a trunk the characters bump into (and navigation avoids)
            reg.emplace<RigidBodyComponent>(root).type = BodyType3D::Static;
            auto& col = reg.emplace<ColliderComponent>(root);
            col.shape = ColliderShape3D::Capsule;
            col.radius = it.colliderRadius;
            col.height = 2.0f;
            col.offset = {0.0f, 1.0f, 0.0f};
        }
        out.push_back(root);
    }
    return out;
}

} // namespace agr::editor::templates
