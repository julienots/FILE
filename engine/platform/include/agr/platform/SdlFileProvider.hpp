#pragma once
#include "agr/core/FileSystem.hpp"

namespace agr {

/// Read-only files opened through SDL. On Android, relative paths are read from the
/// APK's assets folder (AAssetManager); elsewhere from the file system. Used to mount
/// the game data of an exported game.
class SdlFileProvider final : public FileProvider {
public:
    explicit SdlFileProvider(std::string root) : m_root(std::move(root)) {}
    bool exists(std::string_view relativePath) const override;
    Result<ByteBuffer> read(std::string_view relativePath) const override;
    Status write(std::string_view, const void*, usize) override;
    /// Not supported for APK assets (empty list); games do not need to enumerate their files.
    Result<std::vector<std::string>> list(std::string_view relativeDir) const override;
    bool readOnly() const override { return true; }

private:
    std::string path(std::string_view relative) const;
    std::string m_root;
};

} // namespace agr
