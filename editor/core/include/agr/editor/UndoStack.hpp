#pragma once
#include <string>
#include <vector>

#include "agr/core/Uuid.hpp"
#include "agr/scene/Scene.hpp"

namespace agr::editor {

/// Snapshot-based undo / redo: every committed action stores the serialized
/// scene and the selection. Simple and exact (every reflected component,
/// including plugin components, is covered).
/// Cost: one scene serialization per action — fine for editor-sized scenes
/// (~5 µs per entity); a delta-based history is planned for very large scenes.
class UndoStack {
public:
    explicit UndoStack(usize maxStates = 128) : m_max(maxStates < 2 ? 2 : maxStates) {}

    /// Starts a new history from the current scene (after load / new scene).
    void reset(const Scene& scene, const std::vector<Uuid>& selection);
    /// Records the scene after an action. `label` describes the action ("Move", "Delete"...).
    void commit(const std::string& label, const Scene& scene, const std::vector<Uuid>& selection);
    /// Restores the previous state. Returns false when there is nothing to undo.
    bool undo(Scene& scene, std::vector<Uuid>& selection);
    bool redo(Scene& scene, std::vector<Uuid>& selection);

    bool canUndo() const { return m_pos > 0; }
    bool canRedo() const { return m_pos + 1 < m_states.size(); }
    const std::string& undoLabel() const;
    const std::string& redoLabel() const;
    usize size() const { return m_states.size(); }
    /// Index of the current state (0 = the oldest kept).
    usize position() const { return m_pos; }
    /// Label of state `i` (the action that produced it; the first is the opening).
    const std::string& label(usize i) const { return m_states[i].label; }
    /// Restores state `i` directly (History panel): one step back or many at once.
    bool jumpTo(usize i, Scene& scene, std::vector<Uuid>& selection);

    void markSaved() { m_savedPos = static_cast<i64>(m_pos); }
    bool dirty() const { return m_savedPos != static_cast<i64>(m_pos); }

private:
    struct State {
        std::string label;
        std::vector<u8> scene;  ///< Binary snapshot (SceneSerializer::toBinary): fast to take and restore.
        std::vector<Uuid> selection;
    };
    bool restore(Scene& scene, std::vector<Uuid>& selection);

    std::vector<State> m_states;
    usize m_pos = 0;
    i64 m_savedPos = 0;
    usize m_max;
};

} // namespace agr::editor
