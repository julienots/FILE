#pragma once
#include <memory>
#include <string>
#include <unordered_map>

#include "agr/assets/CustomShader.hpp"
#include "agr/core/FileSystem.hpp"
#include "agr/core/Result.hpp"

namespace agr {

/// A loaded `.agrshader` file. `version` changes whenever the source changes (file edit,
/// save or live preview): renderers rebuild their pipelines when it differs.
struct ShaderAsset {
    std::string source;
    CustomShader parsed;
    u64 version = 0;
    i64 modifiedTime = 0;
};

/// `.agrshader` files, cached by URI and reloaded when they change.
class ShaderLibrary {
public:
    explicit ShaderLibrary(VirtualFileSystem& vfs) : m_vfs(vfs) {}
    /// nullptr when the file cannot be read (logged once). A file with errors is returned
    /// (see `parsed.errors`): renderers then draw its materials with the standard shader.
    const ShaderAsset* get(const std::string& uri);
    /// Writes the file and updates the cache (editor).
    Status save(const std::string& uri, const std::string& source);
    /// Replaces the cached source without writing the file (live editing).
    void preview(const std::string& uri, const std::string& source);
    /// Re-reads files modified on disk; returns how many changed.
    u32 reloadChanged();
    void invalidate(const std::string& uri) { m_entries.erase(uri); }
    void clear() { m_entries.clear(); }

private:
    void set(ShaderAsset& asset, const std::string& source);
    VirtualFileSystem& m_vfs;
    std::unordered_map<std::string, std::unique_ptr<ShaderAsset>> m_entries;  // null: unreadable
    u64 m_nextVersion = 1;
};

} // namespace agr
