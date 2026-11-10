#include "agr/editor/UndoStack.hpp"

#include "agr/core/Log.hpp"
#include "agr/scene/SceneSerializer.hpp"

namespace agr::editor {

void UndoStack::reset(const Scene& scene, const std::vector<Uuid>& selection) {
    m_states.clear();
    m_states.push_back({"", SceneSerializer::toString(scene, -1), selection});
    m_pos = 0;
    m_savedPos = 0;
}

void UndoStack::commit(const std::string& label, const Scene& scene, const std::vector<Uuid>& selection) {
    if (m_states.empty()) {
        reset(scene, selection);
        return;
    }
    std::string snapshot = SceneSerializer::toString(scene, -1);
    if (snapshot == m_states[m_pos].scene) {  // nothing changed: keep history clean
        m_states[m_pos].selection = selection;
        return;
    }
    m_states.resize(m_pos + 1);  // drop the redo branch
    m_states.push_back({label, std::move(snapshot), selection});
    m_pos = m_states.size() - 1;
    if (m_states.size() > m_max) {
        m_states.erase(m_states.begin());
        --m_pos;
        --m_savedPos;  // may become negative: the saved state left the history (stays dirty)
    }
}

bool UndoStack::restore(Scene& scene, std::vector<Uuid>& selection) {
    const State& s = m_states[m_pos];
    if (Status st = SceneSerializer::fromString(scene, s.scene); !st) {
        AGR_LOG_ERROR("Editor", "undo / redo failed: {}", st.error().message);
        return false;
    }
    selection = s.selection;
    return true;
}

bool UndoStack::undo(Scene& scene, std::vector<Uuid>& selection) {
    if (!canUndo()) return false;
    --m_pos;
    return restore(scene, selection);
}

bool UndoStack::redo(Scene& scene, std::vector<Uuid>& selection) {
    if (!canRedo()) return false;
    ++m_pos;
    return restore(scene, selection);
}

const std::string& UndoStack::undoLabel() const {
    static const std::string none;
    return canUndo() ? m_states[m_pos].label : none;
}

const std::string& UndoStack::redoLabel() const {
    static const std::string none;
    return canRedo() ? m_states[m_pos + 1].label : none;
}

} // namespace agr::editor
