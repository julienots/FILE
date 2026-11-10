#pragma once
#include <string>

#include <nlohmann/json.hpp>

#include "agr/core/FileSystem.hpp"

namespace agr {

/// Local game save: a JSON object of named values kept in memory and written atomically to
/// a file of the user folder (user://save.json; Android internal storage). The engine writes
/// it when the application goes to the background, terminates or shuts down.
class SaveStore {
public:
    SaveStore(VirtualFileSystem& vfs, std::string uri) : m_vfs(vfs), m_uri(std::move(uri)) {}

    bool has(const std::string& key);
    /// Null when absent.
    const nlohmann::json& get(const std::string& key);
    void set(const std::string& key, nlohmann::json value);
    void remove(const std::string& key);
    void clear();
    /// Writes the file if something changed. Errors are returned (disk full...).
    Status flush();
    bool dirty() const { return m_dirty; }
    /// Forgets the in-memory values and reads the file again.
    void reload();
    const std::string& uri() const { return m_uri; }

private:
    void load();
    VirtualFileSystem& m_vfs;
    std::string m_uri;
    nlohmann::json m_data = nlohmann::json::object();
    bool m_loaded = false, m_dirty = false;
};

} // namespace agr
