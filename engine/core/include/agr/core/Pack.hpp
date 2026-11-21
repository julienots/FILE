#pragma once
// Data packs (.agrpak): many files in one archive, optionally encrypted (AES-256-CTR), read
// in place (random access, nothing extracted). Exported games ship game.agrpak; patches and
// DLC are more packs layered on top (OverlayFileProvider: the last pack holding a file wins).
//
//  "AGRPAK01" | u32 flags (1 = encrypted) | 16-byte key check | u64 index nonce |
//  u32 index size | index | file data
//  index: u32 count, then per file: u16 path length, path, u64 offset, u64 size, u64 nonce,
//  32-byte SHA-256 of the plain bytes.
#include <filesystem>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <optional>

#include "agr/core/Crypto.hpp"
#include "agr/core/FileSystem.hpp"

namespace agr {

struct PackEntry {
    std::string path;  ///< '/' separators, relative.
    u64 offset = 0, size = 0, nonce = 0;
    crypto::Sha256Digest hash{};
};

/// Key from a passphrase or from raw bytes (hex): SHA-256 of "agr-pack:" + text.
crypto::AesKey packKeyFromText(std::string_view text);

class PackWriter {
public:
    void add(std::string path, ByteBuffer bytes);
    void add(std::string path, std::string_view text) { add(std::move(path), ByteBuffer(text.begin(), text.end())); }
    usize count() const { return m_files.size(); }
    /// Writes the pack; `key` = encrypt it.
    Status write(const std::filesystem::path& output, const crypto::AesKey* key = nullptr) const;

private:
    std::map<std::string, ByteBuffer> m_files;
};

/// Reads `size` bytes at `offset` of the pack (thread-safe), false on failure.
using PackReader = std::function<bool(u64 offset, void* out, u64 size)>;
/// Reader of a file on disk (kept open).
PackReader packFileReader(const std::filesystem::path& path);
/// Reader of a pack already in memory.
PackReader packMemoryReader(ByteBuffer bytes);
/// Key of the running game's packs (set by the player from its key slot), null = none.
void setGamePackKey(const crypto::AesKey* key);
const crypto::AesKey* gamePackKey();

class PackFileProvider final : public FileProvider {
public:
    /// Fails on a damaged pack or a wrong / missing key.
    static Result<std::unique_ptr<PackFileProvider>> open(PackReader reader, const crypto::AesKey* key = nullptr);
    static Result<std::unique_ptr<PackFileProvider>> openFile(const std::filesystem::path& path, const crypto::AesKey* key = nullptr);

    bool exists(std::string_view relativePath) const override;
    Result<ByteBuffer> read(std::string_view relativePath) const override;
    Status write(std::string_view, const void*, usize) override;
    Result<std::vector<std::string>> list(std::string_view relativeDir) const override;
    bool readOnly() const override { return true; }
    const std::vector<PackEntry>& entries() const { return m_entries; }
    bool encrypted() const { return m_aes.has_value(); }

private:
    PackReader m_reader;
    std::vector<PackEntry> m_entries;  // sorted by path
    std::optional<crypto::Aes256> m_aes;
    const PackEntry* find(std::string_view path) const;
};

/// A folder of another provider seen as its root ("assets/" of the game pack).
class SubdirFileProvider final : public FileProvider {
public:
    SubdirFileProvider(std::shared_ptr<FileProvider> inner, std::string prefix);
    bool exists(std::string_view p) const override { return m_inner->exists(full(p)); }
    Result<ByteBuffer> read(std::string_view p) const override { return m_inner->read(full(p)); }
    Status write(std::string_view p, const void* d, usize n) override { return m_inner->write(full(p), d, n); }
    Result<std::vector<std::string>> list(std::string_view d) const override { return m_inner->list(full(d)); }
    bool readOnly() const override { return m_inner->readOnly(); }
    i64 modifiedTime(std::string_view p) const override { return m_inner->modifiedTime(full(p)); }

private:
    std::string full(std::string_view p) const;
    std::shared_ptr<FileProvider> m_inner;
    std::string m_prefix;
};

/// Layers of providers: reads come from the last layer that has the file (patches, DLC,
/// mods over the base game); lists merge every layer. Read-only.
class OverlayFileProvider final : public FileProvider {
public:
    void addLayer(std::shared_ptr<FileProvider> layer);
    usize layerCount() const;
    bool exists(std::string_view p) const override;
    Result<ByteBuffer> read(std::string_view p) const override;
    Status write(std::string_view, const void*, usize) override;
    Result<std::vector<std::string>> list(std::string_view d) const override;
    bool readOnly() const override { return true; }
    i64 modifiedTime(std::string_view p) const override;

private:
    mutable std::mutex m_mutex;  // layers may be added while workers read
    std::vector<std::shared_ptr<FileProvider>> m_layers;
    std::vector<std::shared_ptr<FileProvider>> snapshot() const;
};

} // namespace agr
