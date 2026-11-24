#include "agr/script/Rewind.hpp"

#include <algorithm>
#include <cmath>

#include <nlohmann/json.hpp>

#include "agr/physics2d/Physics2DWorld.hpp"
#include "agr/physics3d/Physics3DWorld.hpp"
#include "agr/scene/Reflection.hpp"

namespace agr {

void registerRewindComponent() {
    auto& reg = ComponentRegistry::instance();
    if (reg.find<RewindComponent>()) return;
    reg.add<RewindComponent>("Rewind")
        .category("Scripting")
        .description("Keeps the last seconds of the entity (transform, velocity): agr.rewind.start() plays time backward.")
        .field("seconds", &RewindComponent::seconds, {.tooltip = "History kept (seconds)", .rangeMin = 0.1f, .rangeMax = 120.0f, .speed = 0.1f})
        .field("record", &RewindComponent::record);
}

RewindRecorder::Frame RewindRecorder::read(Scene& scene, ecs::Entity e) const {
    Frame f;
    const auto& t = scene.registry().get<TransformComponent>(e);
    f.position = t.position, f.rotation = t.rotation, f.scale = t.scale;
    if (m_p2)
        if (auto v = m_p2->velocity(e)) f.velocity = {v->x, v->y, 0.0f};
    if (m_p3)
        if (auto v = m_p3->velocity(e)) f.velocity = *v;
    return f;
}

void RewindRecorder::apply(Scene& scene, ecs::Entity e, const Frame& f, bool velocity) const {
    auto& t = scene.registry().get<TransformComponent>(e);
    t.position = f.position, t.rotation = f.rotation, t.scale = f.scale;
    // While rewinding the bodies stay still between frames; at the end they get their velocity back.
    const math::Vec3 v = velocity ? f.velocity : math::Vec3{0.0f};
    if (m_p2) m_p2->setVelocity(e, {v.x, v.y});
    if (m_p3) m_p3->setVelocity(e, v);
}

void RewindRecorder::step(Scene& scene, float dt) {
    if (dt > 0.0f) m_dt = dt;
    auto& reg = scene.registry();
    // Ghosts: one recorded frame per fixed step.
    for (auto it = m_ghosts.begin(); it != m_ghosts.end();) {
        Ghost& g = it->second;
        if (!scene.valid(g.entity) || g.frames.empty()) {
            it = m_ghosts.erase(it);
            continue;
        }
        if (g.index >= g.frames.size()) {
            if (!g.loop) {
                it = m_ghosts.erase(it);
                continue;
            }
            g.index = 0;
        }
        apply(scene, g.entity, g.frames[g.index++], false);
        ++it;
    }
    if (m_rewinding) {
        m_debt += m_speed;
        bool any = false;
        while (m_debt >= 1.0f) {
            m_debt -= 1.0f;
            for (auto& [key, tr] : m_tracks) {
                if (!scene.valid(tr.entity) || tr.frames.empty()) continue;
                tr.last = tr.frames.back();
                tr.hasLast = true;
                tr.frames.pop_back();
                any = true;
            }
        }
        for (auto& [key, tr] : m_tracks)
            if (tr.hasLast && scene.valid(tr.entity)) apply(scene, tr.entity, tr.last, false);
        bool left = false;
        for (auto& [key, tr] : m_tracks) left |= !tr.frames.empty();
        if (!left && !any) stop(scene);  // history used up
        scene.updateTransforms();
        return;
    }
    std::vector<u64> seen;
    reg.view<RewindComponent>().each([&](ecs::Entity e, RewindComponent& rc) {
        if (!rc.record || !scene.isActiveInHierarchy(e)) return;
        Track& tr = m_tracks[e.packed()];
        tr.entity = e;
        tr.frames.push_back(read(scene, e));
        const usize keep = static_cast<usize>(std::max(1.0f, rc.seconds / m_dt));
        while (tr.frames.size() > keep) tr.frames.pop_front();
        seen.push_back(e.packed());
    });
    for (auto it = m_tracks.begin(); it != m_tracks.end();)
        it = std::find(seen.begin(), seen.end(), it->first) == seen.end() ? m_tracks.erase(it) : std::next(it);
}

void RewindRecorder::start() {
    m_rewinding = true;
    m_debt = 0.0f;
    for (auto& [key, tr] : m_tracks) tr.hasLast = false;
}

void RewindRecorder::stop(Scene& scene) {
    if (!m_rewinding) return;
    m_rewinding = false;
    for (auto& [key, tr] : m_tracks)
        if (tr.hasLast && scene.valid(tr.entity)) apply(scene, tr.entity, tr.last, true);  // moving again as it was
}

float RewindRecorder::available(ecs::Entity e) const {
    const auto it = m_tracks.find(e.packed());
    return it == m_tracks.end() ? 0.0f : static_cast<float>(it->second.frames.size()) * m_dt;
}

std::string RewindRecorder::capture(ecs::Entity e) const {
    nlohmann::json j = {{"format", "agr.recording"}, {"dt", m_dt}, {"frames", nlohmann::json::array()}};
    const auto it = m_tracks.find(e.packed());
    if (it != m_tracks.end())
        for (const Frame& f : it->second.frames)
            j["frames"].push_back({f.position.x, f.position.y, f.position.z, f.rotation.x, f.rotation.y, f.rotation.z, f.rotation.w, f.scale.x, f.scale.y,
                                   f.scale.z});
    return j.dump();
}

bool RewindRecorder::playGhost(ecs::Entity e, const std::string& recording, bool loop) {
    const auto j = nlohmann::json::parse(recording, nullptr, false);
    if (j.is_discarded() || j.value("format", "") != "agr.recording" || !j.contains("frames")) return false;
    Ghost g;
    g.entity = e;
    g.loop = loop;
    for (const auto& a : j["frames"]) {
        if (!a.is_array() || a.size() < 10) return false;
        Frame f;
        f.position = {a[0].get<float>(), a[1].get<float>(), a[2].get<float>()};
        f.rotation = {a[3].get<float>(), a[4].get<float>(), a[5].get<float>(), a[6].get<float>()};
        f.scale = {a[7].get<float>(), a[8].get<float>(), a[9].get<float>()};
        g.frames.push_back(f);
    }
    if (g.frames.empty()) return false;
    m_ghosts[e.packed()] = std::move(g);
    return true;
}

} // namespace agr
