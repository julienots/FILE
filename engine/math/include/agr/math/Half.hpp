#pragma once
// IEEE 754 half-precision conversion (RGBA16F texture data).
#include <bit>
#include <cstdint>

namespace agr::math {

/// Rounds to nearest even; overflow gives infinity, NaN stays NaN, tiny values become
/// subnormals or zero.
inline std::uint16_t floatToHalf(float value) {
    const std::uint32_t f = std::bit_cast<std::uint32_t>(value);
    const std::uint32_t sign = (f >> 16) & 0x8000u;
    const std::uint32_t absBits = f & 0x7fffffffu;
    if (absBits >= 0x7f800000u) return static_cast<std::uint16_t>(sign | (absBits > 0x7f800000u ? 0x7e00u : 0x7c00u));
    if (absBits >= 0x477ff000u) return static_cast<std::uint16_t>(sign | 0x7c00u);  // rounds above 65504
    if (absBits < 0x38800000u) {                                                   // subnormal half or zero
        if (absBits < 0x33000000u) return static_cast<std::uint16_t>(sign);
        const std::uint32_t mantissa = (absBits & 0x007fffffu) | 0x00800000u;
        const int shift = 126 - static_cast<int>(absBits >> 23);  // 14..24
        const std::uint32_t half = mantissa >> shift;
        const std::uint32_t rest = mantissa & ((1u << shift) - 1u);
        const std::uint32_t midpoint = 1u << (shift - 1);
        const std::uint32_t rounded = half + ((rest > midpoint || (rest == midpoint && (half & 1u))) ? 1u : 0u);
        return static_cast<std::uint16_t>(sign | rounded);
    }
    const std::uint32_t biased = absBits - 0x38000000u;  // rebias exponent 127 -> 15
    const std::uint32_t rounded = biased + 0x0fffu + ((biased >> 13) & 1u);
    return static_cast<std::uint16_t>(sign | (rounded >> 13));
}

inline float halfToFloat(std::uint16_t h) {
    const std::uint32_t sign = static_cast<std::uint32_t>(h & 0x8000u) << 16;
    const std::uint32_t exponent = (h >> 10) & 0x1fu;
    const std::uint32_t mantissa = h & 0x3ffu;
    if (exponent == 0) {
        const float v = static_cast<float>(mantissa) * (1.0f / 16777216.0f);  // 2^-24
        return sign ? -v : v;
    }
    if (exponent == 31) return std::bit_cast<float>(sign | 0x7f800000u | (mantissa << 13));
    return std::bit_cast<float>(sign | ((exponent + 112u) << 23) | (mantissa << 13));
}

} // namespace agr::math
