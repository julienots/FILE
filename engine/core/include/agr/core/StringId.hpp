#pragma once
#include <string_view>

#include "agr/core/Types.hpp"

namespace agr {

/// 64-bit FNV-1a hash, usable at compile time.
constexpr u64 fnv1a64(std::string_view text) {
    u64 hash = 0xcbf29ce484222325ull;
    for (char c : text) {
        hash ^= static_cast<u8>(c);
        hash *= 0x100000001b3ull;
    }
    return hash;
}

/// Hashed identifier used for fast lookups of names (events, assets, types).
class StringId {
public:
    constexpr StringId() = default;
    constexpr explicit StringId(std::string_view text) : m_hash(fnv1a64(text)) {}
    constexpr u64 value() const { return m_hash; }
    constexpr bool valid() const { return m_hash != 0; }
    constexpr auto operator<=>(const StringId&) const = default;

private:
    u64 m_hash = 0;
};

namespace literals {
constexpr StringId operator""_sid(const char* s, usize n) { return StringId(std::string_view(s, n)); }
} // namespace literals

/// Stable compile-time name of a type, derived from the compiler signature.
template <typename T>
constexpr std::string_view typeName() {
#if defined(_MSC_VER)
    constexpr std::string_view sig = __FUNCSIG__;
    constexpr std::string_view prefix = "typeName<";
    constexpr std::string_view suffix = ">(void)";
#else
    constexpr std::string_view sig = __PRETTY_FUNCTION__;
    constexpr std::string_view prefix = "T = ";
    constexpr std::string_view suffix = ";]";  // GCC appends "; std::string_view = ..."
#endif
    constexpr auto start = sig.find(prefix) + prefix.size();
#if defined(_MSC_VER)
    constexpr auto end = sig.rfind(suffix);
#else
    constexpr auto end = sig.find_first_of(suffix, start);
#endif
    return sig.substr(start, end - start);
}

template <typename T>
constexpr u64 typeHash() { return fnv1a64(typeName<T>()); }

} // namespace agr

template <>
struct std::hash<agr::StringId> {
    size_t operator()(const agr::StringId& id) const noexcept { return static_cast<size_t>(id.value()); }
};
