#pragma once
// Undo / redo for documents edited in a panel (animation clips, state machines): the panel shows
// its document's serialized state every frame; a change becomes one undo step, and a change
// made while the user is still interacting (dragging a key, typing a name) is merged into the
// step it started.
#include <optional>
#include <string>
#include <vector>

#include "agr/core/Types.hpp"

namespace agr::editor {

class EditHistory {
public:
    explicit EditHistory(usize limit = 100) : m_limit(limit) {}
    /// A new document (opened, created, reloaded): forgets the history.
    void reset(std::string snapshot) {
        m_undo.clear();
        m_redo.clear();
        m_current = std::move(snapshot);
        m_open = false;
    }
    /// The document as it is now. `interacting`: the user is still in the middle of an edit.
    void observe(const std::string& snapshot, bool interacting) {
        if (snapshot != m_current) {
            if (!m_open) {
                m_undo.push_back(m_current);
                if (m_undo.size() > m_limit) m_undo.erase(m_undo.begin());
                m_redo.clear();
            }
            m_current = snapshot;
            m_open = interacting;
        } else if (!interacting) {
            m_open = false;
        }
    }
    bool canUndo() const { return !m_undo.empty(); }
    bool canRedo() const { return !m_redo.empty(); }
    /// The state to restore, or nothing.
    std::optional<std::string> undo() {
        if (m_undo.empty()) return std::nullopt;
        m_redo.push_back(std::move(m_current));
        m_current = std::move(m_undo.back());
        m_undo.pop_back();
        m_open = false;
        return m_current;
    }
    std::optional<std::string> redo() {
        if (m_redo.empty()) return std::nullopt;
        m_undo.push_back(std::move(m_current));
        m_current = std::move(m_redo.back());
        m_redo.pop_back();
        m_open = false;
        return m_current;
    }
    usize undoCount() const { return m_undo.size(); }

private:
    std::vector<std::string> m_undo, m_redo;
    std::string m_current;
    bool m_open = false;
    usize m_limit;
};

} // namespace agr::editor
