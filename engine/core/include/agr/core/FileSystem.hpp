#pragma once
#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "agr/core/Result.hpp"
#include "agr/core/Types.hpp"

namespace agr {

using ByteBuffer = std::vector<u8>;

/// Backend serving files for one mount point. Implementations: native folders
/// (desktop, Android internal storage), APK assets (platform module), archives.
class FileProvider {
public:
    virtual ~FileProvider() = default;
    virtual bool exists(std::string_view relativePath) const = 0;
    virtual Result<ByteBuffer> read(std::string_view relativePath) const = 0;
    virtual Status write(std::string_view relativePath, const void* data, usize size) = 0;
    virtual Result<std::vector<std::string>> list(std::string_view relativeDir) const = 0;
    virtual bool readOnly() const = 0;
    /// Last modification time (opaque, monotonic per file) or 0 if unknown. Used for hot reload.
    virtual i64 modifiedTime(std::string_view relativePath) const { (void)relativePath; return 0; }
};

/// Maps a directory of the host file system.
class NativeFileProvider final : public FileProvider {
public:
    NativeFileProvider(std::string rootDirectory, bool readOnly);
    bool exists(std::string_view relativePath) const override;
    Result<ByteBuffer> read(std::string_view relativePath) const override;
    Status write(std::string_view relativePath, const void* data, usize size) override;
    Result<std::vector<std::string>> list(std::string_view relativeDir) const override;
    bool readOnly() const override { return m_readOnly; }
    i64 modifiedTime(std::string_view relativePath) const override;
    const std::string& root() const { return m_root; }

private:
    std::string m_root;
    bool m_readOnly;
};

/// Virtual file system addressed with URIs: "<mount>://relative/path".
/// Thread safety: mount()/unmount() are main-thread only and must not race with
/// reads; read operations may run concurrently from worker threads (asset loading).
/// Typical mounts: assets:// (read-only game data), user:// (saves, settings),
/// project:// (editor only).
class VirtualFileSystem {
public:
    void mount(const std::string& scheme, std::unique_ptr<FileProvider> provider);
    void unmount(const std::string& scheme);
    bool isMounted(const std::string& scheme) const;

    bool exists(std::string_view uri) const;
    Result<ByteBuffer> readBytes(std::string_view uri) const;
    Result<std::string> readText(std::string_view uri) const;
    Status writeBytes(std::string_view uri, const void* data, usize size);
    Status writeText(std::string_view uri, std::string_view text);
    Result<std::vector<std::string>> list(std::string_view uriDir) const;
    i64 modifiedTime(std::string_view uri) const;

    struct ParsedUri {
        std::string scheme;
        std::string path;
    };
    /// Splits and validates a URI. Rejects absolute paths and ".." segments that
    /// would escape the mount root.
    static Result<ParsedUri> parse(std::string_view uri);

private:
    Result<FileProvider*> resolve(std::string_view uri, std::string& outPath) const;
    std::unordered_map<std::string, std::unique_ptr<FileProvider>> m_mounts;
};

} // namespace agr
