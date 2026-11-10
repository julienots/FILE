#pragma once
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

#include "agr/animation/AnimationClip.hpp"
#include "agr/animation/AnimatorController.hpp"
#include "agr/core/FileSystem.hpp"
#include "agr/scene/Scene.hpp"

namespace agr {

/// Plays a state machine (.agrctrl) or a single property clip (.agranim) on the entity
/// and its children.
struct AnimatorComponent {
    std::string controller;  ///< State machine; when empty, `clip` plays alone.
    std::string clip;
    float speed = 1.0f;
    bool playing = true;
};
void registerAnimationComponents();

/// Published by AnimationPlugin; scripts receive onAnimationEvent(name) / onStateEnter(name).
struct AnimationEvent {
    enum class Kind : u8 { ClipEvent, StateEnter };
    ecs::Entity entity;
    Kind kind = Kind::ClipEvent;
    std::string name;
};

struct AnimationStats {
    u32 animators = 0;
    u32 tracksApplied = 0;
    u32 transitions = 0;  ///< Since start.
    u32 clips = 0, controllers = 0;
    f64 updateMs = 0.0;
};

/// Clips and state machines loaded through the virtual file system, cached, reloaded
/// when their file changes.
class AnimationLibrary {
public:
    explicit AnimationLibrary(VirtualFileSystem& vfs) : m_vfs(vfs) {}
    /// .agranim file, or "<model>#<clip>" for an animation imported with a model (glTF).
    Result<std::shared_ptr<const AnimationClip>> clip(std::string_view uri);
    /// Names of the animations of a model file ("Idle", "Walk"...), empty if none or unreadable.
    std::vector<std::string> modelClipNames(std::string_view modelUri);
    Result<std::shared_ptr<const AnimatorController>> controller(std::string_view uri);
    /// Drops cached files modified on disk; returns how many.
    u32 reloadChanged();
    usize clipCount() const { return m_clips.size(); }
    usize controllerCount() const { return m_controllers.size(); }

private:
    template <typename T>
    struct Entry {
        std::shared_ptr<const T> value;
        i64 modified = 0;
    };
    /// "assets://hero.glb#Walk": the clip "Walk" of an imported model, as transform tracks.
    Result<std::shared_ptr<const AnimationClip>> modelClip(const std::string& uri);
    VirtualFileSystem& m_vfs;
    std::unordered_map<std::string, Entry<AnimationClip>> m_clips;
    std::unordered_map<std::string, Entry<AnimatorController>> m_controllers;
};

/// Runs the Animator components of a scene: state machines with parameters, conditions,
/// exit times and crossfades; property curves applied to reflected fields of the entity
/// and its children; clip events. Main thread.
class AnimationSystem {
public:
    explicit AnimationSystem(VirtualFileSystem& vfs);
    ~AnimationSystem();

    /// Advances every active Animator by dt (seconds) and applies the animated values.
    void update(Scene& scene, float dt);
    std::vector<AnimationEvent> takeEvents();
    /// Forgets every animator's state (game stopped, scene reloaded).
    void reset();

    // Parameters (false if the entity has no Animator with a state machine, or the parameter does not exist).
    bool setFloat(Scene& scene, ecs::Entity e, std::string_view name, float value);
    bool setBool(Scene& scene, ecs::Entity e, std::string_view name, bool value);
    bool setTrigger(Scene& scene, ecs::Entity e, std::string_view name);
    bool resetTrigger(Scene& scene, ecs::Entity e, std::string_view name);
    std::optional<float> parameter(Scene& scene, ecs::Entity e, std::string_view name);
    /// Jumps to a state (crossfade of `fade` seconds).
    bool play(Scene& scene, ecs::Entity e, std::string_view state, float fade = 0.0f);
    std::string currentState(ecs::Entity e) const;
    /// Seconds spent in the current state.
    float stateTime(ecs::Entity e) const;

    /// Editor preview: applies `clip` at `time` to `root` and its children (no events, no state).
    void preview(Scene& scene, ecs::Entity root, const AnimationClip& clip, float time);
    /// Reads the current value of a track's field channel on `root` (for keying in the editor).
    std::optional<float> readTrack(Scene& scene, ecs::Entity root, const AnimationTrack& track);
    /// Same for string fields.
    std::optional<std::string> readTrackText(Scene& scene, ecs::Entity root, const AnimationTrack& track);
    /// True when the track's field is a string (keys hold text).
    static bool isTextTrack(const AnimationTrack& track);

    AnimationLibrary& library() { return m_library; }
    const AnimationStats& stats() const { return m_stats; }

    /// Entity below `root` designated by a path of names ("" = root, "Arm/Hand").
    static ecs::Entity resolvePath(Scene& scene, ecs::Entity root, std::string_view path);
    /// Path of `e` relative to `root` ("" for root itself; empty optional if not below it).
    static std::optional<std::string> pathOf(Scene& scene, ecs::Entity root, ecs::Entity e);

private:
    struct Runtime;
    Runtime* runtime(Scene& scene, ecs::Entity e);
    void enterState(Scene& scene, ecs::Entity e, Runtime& rt, const AnimatorState* state, float fade);
    void apply(Scene& scene, ecs::Entity root, Runtime* rt, const AnimationClip* from, float fromTime, const AnimationClip& to, float toTime,
               float weight);

    VirtualFileSystem& m_vfs;
    AnimationLibrary m_library;
    std::unordered_map<u64, std::unique_ptr<Runtime>> m_runtimes;
    std::vector<AnimationEvent> m_events;
    u64 m_sceneGeneration = 0;
    Scene* m_scene = nullptr;
    AnimationStats m_stats;
};

} // namespace agr
