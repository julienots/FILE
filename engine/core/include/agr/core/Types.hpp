#pragma once
#include <cstddef>
#include <cstdint>

namespace agr {
using u8 = std::uint8_t;
using u16 = std::uint16_t;
using u32 = std::uint32_t;
using u64 = std::uint64_t;
using i8 = std::int8_t;
using i16 = std::int16_t;
using i32 = std::int32_t;
using i64 = std::int64_t;
using f32 = float;
using f64 = double;
using usize = std::size_t;

/// Base class for types that must never be copied (owners of OS / GPU resources).
class NonCopyable {
public:
    NonCopyable() = default;
    NonCopyable(const NonCopyable&) = delete;
    NonCopyable& operator=(const NonCopyable&) = delete;
    NonCopyable(NonCopyable&&) = default;
    NonCopyable& operator=(NonCopyable&&) = default;

protected:
    ~NonCopyable() = default;
};

constexpr usize KiB(usize v) { return v * 1024u; }
constexpr usize MiB(usize v) { return v * 1024u * 1024u; }

/// Rounds `value` up to the next multiple of `alignment` (power of two).
constexpr usize alignUp(usize value, usize alignment) {
    return (value + alignment - 1u) & ~(alignment - 1u);
}
constexpr bool isPowerOfTwo(usize v) { return v != 0u && (v & (v - 1u)) == 0u; }
} // namespace agr
