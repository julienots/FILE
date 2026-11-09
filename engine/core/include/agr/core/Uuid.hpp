#pragma once
#include <string>
#include <string_view>

#include "agr/core/Types.hpp"

namespace agr {

/// 64-bit random persistent identifier (entities, assets). 0 is the null id.
class Uuid {
public:
    constexpr Uuid() = default;
    constexpr explicit Uuid(u64 value) : m_value(value) {}
    static Uuid generate();

    constexpr u64 value() const { return m_value; }
    constexpr bool isNull() const { return m_value == 0; }
    constexpr auto operator<=>(const Uuid&) const = default;

    std::string toString() const;               ///< 16 hex characters.
    static Uuid fromString(std::string_view s);  ///< Null on parse error.

private:
    u64 m_value = 0;
};

} // namespace agr

template <>
struct std::hash<agr::Uuid> {
    size_t operator()(const agr::Uuid& id) const noexcept { return static_cast<size_t>(id.value() ^ (id.value() >> 32)); }
};
