#include "agr/physics3d/Physics3DPlugin.hpp"

#include <mutex>
#include <unordered_map>

#include "agr/assets/Model.hpp"
#include "agr/runtime/Engine.hpp"

namespace agr {

bool lineBlocked(const Physics3DWorld& world, const Scene& scene, math::Vec3 from, math::Vec3 to, ecs::Entity ignore) {
    math::Vec3 start = from;
    for (int tries = 0; tries < 12; ++tries) {
        const math::Vec3 d = to - start;
        const float dist = math::length(d);
        if (dist < 0.05f) return false;
        const auto hit = world.raycast(start, d / dist, dist - 0.05f);
        if (!hit) return false;
        bool own = false;  // the source's own body (or a parent body carrying it)
        for (ecs::Entity e = ignore; !e.isNull() && !own; e = scene.parent(e)) own = e == hit->entity;
        if (!own && hit->distance > 0.02f) return true;
        // Started inside a collider (the listener's head or capsule), or met the source's own body:
        // continue past it (25 cm steps inside, just past the hit otherwise).
        start = (hit->distance <= 0.02f ? start : hit->point) + d / dist * (hit->distance <= 0.02f ? 0.25f : 0.05f);
    }
    return false;
}

PluginInfo Physics3DPlugin::info() const {
    return {"Physics3D", "1.0", "3D rigid bodies, colliders and triggers (Jolt Physics)", "Superessence Studio", {}};
}

Status Physics3DPlugin::onLoad(Engine& engine) {
    registerPhysics3DComponents();
    m_world = std::make_unique<Physics3DWorld>();
    m_world->setJobSystem(&engine.jobs());
    // Mesh / Convex colliders: geometry imported from the model files (glTF, OBJ, FBX), cached.
    struct MeshCache {
        std::mutex mutex;
        std::unordered_map<std::string, std::shared_ptr<const CollisionMesh>> meshes;
    };
    m_world->setCollisionMeshSource([&vfs = engine.vfs(), cache = std::make_shared<MeshCache>()](const std::string& model, i32 index)
                                        -> std::shared_ptr<const CollisionMesh> {
        std::lock_guard lock(cache->mutex);
        const std::string key = model + "#" + std::to_string(index);
        if (auto it = cache->meshes.find(key); it != cache->meshes.end()) return it->second;
        std::shared_ptr<const CollisionMesh> result;
        if (auto data = importModel(vfs, model); data && index >= 0 && static_cast<usize>(index) < data.value().meshes.size()) {
            const ModelMesh& src = data.value().meshes[static_cast<usize>(index)];
            auto mesh = std::make_shared<CollisionMesh>();
            mesh->positions.reserve(src.vertices.size());
            for (const ModelVertex& v : src.vertices) mesh->positions.push_back(v.position);
            mesh->indices = src.indices;
            result = std::move(mesh);
        }
        cache->meshes[key] = result;  // failures too: no retry every rebuild
        return result;
    });
    engine.registerService<Physics3DPlugin>(this);
    m_lineOfSight.blocked = [this, &engine](math::Vec3 from, math::Vec3 to, ecs::Entity ignore) {
        return lineBlocked(*m_world, engine.scene(), from, to, ignore);
    };
    engine.registerService<LineOfSightService>(&m_lineOfSight);
    return {};
}

void Physics3DPlugin::onUnload(Engine& engine) {
    engine.registerService<Physics3DPlugin>(nullptr);
    engine.registerService<LineOfSightService>(nullptr);
    m_world.reset();  // detaches from engine.jobs() before the engine shuts it down
}

void Physics3DPlugin::onFixedUpdate(Engine& engine, const FrameTime& time) {
    m_world->step(engine.scene(), static_cast<float>(time.fixedDeltaSeconds));
    for (const Collision3DEvent& e : m_world->events()) engine.events().publish(e);
    for (const JointBreak3DEvent& e : m_world->jointBreaks()) engine.events().publish(e);
}

} // namespace agr
