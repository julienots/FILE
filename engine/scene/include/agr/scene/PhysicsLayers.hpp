#pragma once
#include <array>
#include <optional>
#include <string>
#include <string_view>

#include "agr/core/FileSystem.hpp"
#include "agr/core/Result.hpp"

namespace agr {

/// Names of the 32 collision layers of a project (assets://physics_layers.json), shared by the
/// editor (inspector, layers panel) and the game (agr.physics.layer / mask). Layer 0 is
/// "Default" unless renamed; unnamed layers display as "Couche n".
class PhysicsLayers {
public:
    static constexpr const char* kUri = "assets://physics_layers.json";
    static constexpr u32 kCount = 32;

    std::array<std::string, kCount> names{};

    /// Missing file = defaults (not an error).
    Status load(const VirtualFileSystem& vfs, const std::string& uri = kUri);
    Status save(VirtualFileSystem& vfs, const std::string& uri = kUri) const;
    /// Reloads when the file changed since the last call; true if the names changed.
    bool refresh(const VirtualFileSystem& vfs, const std::string& uri = kUri);

    std::string displayName(u32 layer) const;
    /// Layer of a name (case sensitive), or nullopt.
    std::optional<u32> find(std::string_view name) const;

    std::string toJson() const;
    Status fromJson(std::string_view text);

private:
    i64 m_loadedTime = -2;
};

} // namespace agr
