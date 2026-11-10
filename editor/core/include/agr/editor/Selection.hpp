#pragma once
#include <algorithm>
#include <vector>

#include "agr/core/Uuid.hpp"
#include "agr/scene/Scene.hpp"

namespace agr::editor {

/// Selected entities, stored by persistent id so selection survives undo / redo
/// (which recreates entities). The last selected entity is the "primary" one
/// (inspector, gizmo pivot).
class Selection {
public:
    void clear() { m_ids.clear(); }
    void set(Uuid id) {
        m_ids.clear();
        if (!id.isNull()) m_ids.push_back(id);
    }
    void add(Uuid id) {
        if (id.isNull()) return;
        remove(id);
        m_ids.push_back(id);
    }
    void remove(Uuid id) { std::erase(m_ids, id); }
    void toggle(Uuid id) { contains(id) ? remove(id) : add(id); }
    bool contains(Uuid id) const { return std::find(m_ids.begin(), m_ids.end(), id) != m_ids.end(); }
    bool empty() const { return m_ids.empty(); }
    usize size() const { return m_ids.size(); }
    Uuid primary() const { return m_ids.empty() ? Uuid{} : m_ids.back(); }
    const std::vector<Uuid>& ids() const { return m_ids; }
    void assign(std::vector<Uuid> ids) { m_ids = std::move(ids); }

    /// Valid entities of the selection (ids that no longer exist are skipped).
    std::vector<ecs::Entity> entities(const Scene& scene) const {
        std::vector<ecs::Entity> out;
        for (Uuid id : m_ids)
            if (auto e = scene.findByUuid(id); !e.isNull()) out.push_back(e);
        return out;
    }
    /// Drops ids that no longer exist.
    void prune(const Scene& scene) {
        std::erase_if(m_ids, [&](Uuid id) { return scene.findByUuid(id).isNull(); });
    }

private:
    std::vector<Uuid> m_ids;
};

} // namespace agr::editor
