#pragma once
// Shared GPU context for the GPU test suite: one hidden SDL window + GL / GLES context.
#include <doctest/doctest.h>

#include <memory>

#include "agr/platform/SdlPlatform.hpp"
#include "agr/rhi/Rhi.hpp"

namespace agr::test {

inline constexpr u32 kWidth = 64;
inline constexpr u32 kHeight = 64;

struct GpuContext {
    std::unique_ptr<SdlPlatform> platform;
    std::unique_ptr<rhi::Device> device;
    GraphicsApi api = GraphicsApi::OpenGL;  ///< AGR_TEST_API = gl | gles | vulkan
};

/// Created by main.cpp before the tests run.
GpuContext& gpu();

struct Rgba {
    u8 r, g, b, a;
};

/// Reads one backbuffer pixel (top-left origin).
inline Rgba pixel(rhi::Device& d, u32 x, u32 y) {
    std::vector<u8> px;
    REQUIRE(d.readBackbuffer(x, y, 1, 1, px));
    return {px[0], px[1], px[2], px[3]};
}

inline bool near(Rgba p, u8 r, u8 g, u8 b, int tolerance = 3) {
    auto close = [&](u8 a, u8 e) { return std::abs(int(a) - int(e)) <= tolerance; };
    return close(p.r, r) && close(p.g, g) && close(p.b, b);
}

} // namespace agr::test

inline std::ostream& operator<<(std::ostream& os, const agr::test::Rgba& p) {
    return os << "rgba(" << int(p.r) << "," << int(p.g) << "," << int(p.b) << "," << int(p.a) << ")";
}
