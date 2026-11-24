#include "agr/editor/OnionSkin.hpp"

#include <algorithm>
#include <cmath>
#include <unordered_set>

#include "agr/render/RenderComponents.hpp"

namespace agr::editor {

std::vector<OnionGhost> onionSkin(Scene& scene, AnimationSystem& anim, ecs::Entity root, const AnimationClip& clip, float time, float step,
                                  u32 before, u32 after, PickAssets assets) {
    std::vector<OnionGhost> out;
    if (!scene.valid(root) || step <= 0.0f) return out;
    // Entities the clip animates (and their subtrees: a moved parent moves its children).
    std::unordered_set<u64> animated;
    std::vector<ecs::Entity> targets;
    auto visit = [&](auto&& self, ecs::Entity e) -> void {
        if (!animated.insert(e.packed()).second) return;
        auto& reg = scene.registry();
        if (reg.has<SpriteRendererComponent>(e) || reg.has<MeshRendererComponent>(e) || reg.has<TilemapComponent>(e)) targets.push_back(e);
        for (ecs::Entity c : scene.children(e)) self(self, c);
    };
    for (const AnimationTrack& t : clip.tracks) {
        const ecs::Entity e = t.target.empty() ? root : AnimationSystem::resolvePath(scene, root, t.target);
        if (!e.isNull()) visit(visit, e);
    }
    for (const TransformTrack& t : clip.transformTracks) {
        const ecs::Entity e = t.target.empty() ? root : AnimationSystem::resolvePath(scene, root, t.target);
        if (!e.isNull()) visit(visit, e);
    }
    if (targets.empty()) return out;
    const float end = clip.loop ? 1e9f : std::max(clip.duration, 0.0f);
    std::vector<i32> offsets;
    for (i32 k = -static_cast<i32>(before); k <= static_cast<i32>(after); ++k)
        if (k != 0) offsets.push_back(k);
    for (i32 k : offsets) {
        float t = time + static_cast<float>(k) * step;
        if (clip.loop && clip.duration > 0.0f) t = std::fmod(std::fmod(t, clip.duration) + clip.duration, clip.duration);
        else if (t < 0.0f || t > end) continue;
        anim.preview(scene, root, clip, t);
        scene.updateTransforms();
        for (ecs::Entity e : targets) out.push_back({e, k, worldBounds(scene, e, assets), scene.worldMatrix(e).translation()});
    }
    anim.preview(scene, root, clip, time);
    scene.updateTransforms();
    return out;
}

} // namespace agr::editor
