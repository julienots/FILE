#pragma once
#include <functional>

#include "agr/core/Types.hpp"

namespace agr::ecs {

/// Lightweight entity identifier: slot index + version. A destroyed entity's
/// version is bumped so stale copies are detected by Registry::valid().
struct Entity {
    static constexpr u32 kNullIndex = 0xFFFFFFFFu;
    u32 index = kNullIndex;
    u32 version = 0;

    constexpr bool isNull() const { return index == kNullIndex; }
    constexpr explicit operator bool() const { return !isNull(); }
    constexpr bool operator==(const Entity&) const = default;
    constexpr u64 packed() const { return (static_cast<u64>(version) << 32) | index; }
};

inline constexpr Entity kNullEntity{};

} // namespace agr::ecs

template <>
struct std::hash<agr::ecs::Entity> {
    size_t operator()(const agr::ecs::Entity& e) const noexcept { return std::hash<agr::u64>{}(e.packed()); }
};
