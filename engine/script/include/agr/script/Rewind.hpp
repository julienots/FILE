#pragma once
// Time rewind and recordings (not built into Godot): entities with a Rewind component keep their
// last seconds (transform and physics velocity, every fixed step). Rewinding plays them backward
// (Braid, Prince of Persia); stopping gives them back the velocity they had at that moment.
// A recording can also be captured as text and replayed on another entity (racing ghosts,
// kill-cams, replays).
#include <deque>
#include <string>
#include <unordered_map>
#include <vector>

#include "agr/math/Math.hpp"
#include "agr/scene/Scene.hpp"

namespace agr {

class Physics2DWorld;
class Physics3DWorld;

struct RewindComponent {
    float seconds = 5.0f;  ///< History kept.
    bool record = true;    ///< Off: neither recorded nor rewound.
};
void registerRewindComponent();

class RewindRecorder {
public:
    struct Frame {
        math::Vec3 position, scale{1.0f};
        math::Quat rotation;
        math::Vec3 velocity{0.0f};
        float spin = 0.0f;  ///< 2D angular velocity (radians / s).
    };

    void setPhysics(Physics2DWorld* p2, Physics3DWorld* p3) { m_p2 = p2, m_p3 = p3; }
    /// Once per fixed step: records (or rewinds, or advances ghosts).
    void step(Scene& scene, float dt);

    void start();
    void stop(Scene& scene);
    bool rewinding() const { return m_rewinding; }
    /// Steps of history consumed per fixed step while rewinding (2 = twice as fast).
    void setSpeed(float speed) { m_speed = speed > 0.0f ? speed : 1.0f; }
    /// Seconds of history an entity has.
    float available(ecs::Entity e) const;
    /// Is this entity under the rewind's control right now (its scripts pause)?
    bool controls(ecs::Entity e) const { return m_rewinding && m_tracks.contains(e.packed()); }

    /// The entity's history as text (oldest first) for ghosts and replays.
    std::string capture(ecs::Entity e) const;
    /// Replays a captured history on `e` (world transform, from the start); `loop` repeats.
    bool playGhost(ecs::Entity e, const std::string& recording, bool loop = false);
    bool ghostPlaying(ecs::Entity e) const { return m_ghosts.contains(e.packed()); }
    void clear() { m_tracks.clear(), m_ghosts.clear(), m_rewinding = false; }

private:
    Frame read(Scene& scene, ecs::Entity e) const;
    void apply(Scene& scene, ecs::Entity e, const Frame& f, bool velocity) const;
    struct Track {
        ecs::Entity entity;
        std::deque<Frame> frames;
        Frame last;
        bool hasLast = false;
    };
    struct Ghost {
        ecs::Entity entity;
        std::vector<Frame> frames;
        usize index = 0;
        bool loop = false;
    };
    std::unordered_map<u64, Track> m_tracks;
    std::unordered_map<u64, Ghost> m_ghosts;
    Physics2DWorld* m_p2 = nullptr;
    Physics3DWorld* m_p3 = nullptr;
    bool m_rewinding = false;
    float m_speed = 1.0f, m_debt = 0.0f, m_dt = 1.0f / 60.0f;
};

} // namespace agr
