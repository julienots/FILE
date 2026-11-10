#include "agr/twod/SpriteAnimator.hpp"

#include <cmath>

#include "agr/render/RenderComponents.hpp"
#include "agr/scene/Reflection.hpp"

namespace agr {

void registerSpriteAnimatorComponent() {
    auto& reg = ComponentRegistry::instance();
    if (reg.find<SpriteAnimatorComponent>()) return;
    reg.add<SpriteAnimatorComponent>("SpriteAnimator")
        .category("2D")
        .description("Frame-by-frame animation: plays a clip of the sprite sheet of the SpriteRenderer.")
        .field("clip", &SpriteAnimatorComponent::clip, {.tooltip = "Clip name defined in the sprite sheet"})
        .field("speed", &SpriteAnimatorComponent::speed, {.rangeMin = 0.0f, .rangeMax = 20.0f, .speed = 0.01f})
        .field("playing", &SpriteAnimatorComponent::playing)
        .field("time", &SpriteAnimatorComponent::time, {.flags = FieldFlag_Hidden | FieldFlag_Transient});
}

u32 clipFrameAt(const SpriteClip& clip, float time) {
    if (clip.frames.empty()) return 0;
    const auto n = static_cast<i64>(clip.frames.size());
    i64 i = static_cast<i64>(std::floor(std::max(time, 0.0f) * std::max(clip.fps, 0.0f)));
    i = clip.loop ? i % n : std::min(i, n - 1);
    return clip.frames[static_cast<usize>(i)];
}

u32 updateSpriteAnimators(Scene& scene, SpriteSheetLibrary& sheets, float dt) {
    u32 updated = 0;
    scene.registry().view<SpriteAnimatorComponent, SpriteRendererComponent>().each(
        [&](ecs::Entity, SpriteAnimatorComponent& a, SpriteRendererComponent& s) {
            if (a.currentClip != a.clip) {
                a.currentClip = a.clip;
                a.time = 0.0f;
            }
            const SpriteSheet* sheet = sheets.get(s.sheet);
            const SpriteClip* clip = sheet ? sheet->findClip(a.clip) : nullptr;
            if (!clip || clip->frames.empty()) return;
            if (a.playing) a.time += dt * a.speed;
            s.frame = static_cast<i32>(clipFrameAt(*clip, a.time));
            ++updated;
        });
    return updated;
}

} // namespace agr
