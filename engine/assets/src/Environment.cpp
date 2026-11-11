#include "agr/assets/Environment.hpp"

#include <algorithm>
#include <bit>
#include <cmath>
#include <string>

namespace agr {

using math::Vec2;
using math::Vec3;

namespace {
constexpr float kPi = 3.14159265358979f;

Vec3 at(const std::vector<float>& rgb, u32 width, u32 x, u32 y) {
    const usize i = (static_cast<usize>(y) * width + x) * 3;
    return {rgb[i], rgb[i + 1], rgb[i + 2]};
}

/// Box 2x2 downsample (sizes are powers of two).
std::vector<float> downsample(const std::vector<float>& src, u32 w, u32 h) {
    const u32 nw = std::max(1u, w / 2), nh = std::max(1u, h / 2);
    std::vector<float> dst(static_cast<usize>(nw) * nh * 3);
    for (u32 y = 0; y < nh; ++y)
        for (u32 x = 0; x < nw; ++x) {
            const u32 x0 = std::min(x * 2, w - 1), x1 = std::min(x * 2 + 1, w - 1);
            const u32 y0 = std::min(y * 2, h - 1), y1 = std::min(y * 2 + 1, h - 1);
            const Vec3 c = (at(src, w, x0, y0) + at(src, w, x1, y0) + at(src, w, x0, y1) + at(src, w, x1, y1)) * 0.25f;
            const usize i = (static_cast<usize>(y) * nw + x) * 3;
            dst[i] = c.x, dst[i + 1] = c.y, dst[i + 2] = c.z;
        }
    return dst;
}

std::vector<float> gaussianWeights(float sigma) {
    const int radius = std::clamp(static_cast<int>(std::ceil(sigma * 3.0f)), 1, 64);
    std::vector<float> wts(static_cast<usize>(radius) * 2 + 1);
    float sum = 0.0f;
    for (int i = -radius; i <= radius; ++i) sum += wts[static_cast<usize>(i + radius)] = std::exp(-0.5f * static_cast<float>(i * i) / (sigma * sigma));
    for (float& v : wts) v /= sum;
    return wts;
}

/// Blur of angular standard deviation `sigma` (radians): vertical pass (clamped at the
/// poles) then horizontal pass widened by 1/sin(theta) (wraps around).
void blur(std::vector<float>& img, u32 w, u32 h, float sigma) {
    const float texelAngle = kPi / static_cast<float>(h);
    const float sigmaV = sigma / texelAngle;
    if (sigmaV < 0.25f) return;
    std::vector<float> tmp(img.size());
    const auto wv = gaussianWeights(sigmaV);
    const int rv = static_cast<int>(wv.size() / 2);
    for (u32 y = 0; y < h; ++y)
        for (u32 x = 0; x < w; ++x) {
            Vec3 c{0.0f};
            for (int k = -rv; k <= rv; ++k) {
                const int yy = std::clamp(static_cast<int>(y) + k, 0, static_cast<int>(h) - 1);
                c = c + at(img, w, x, static_cast<u32>(yy)) * wv[static_cast<usize>(k + rv)];
            }
            const usize i = (static_cast<usize>(y) * w + x) * 3;
            tmp[i] = c.x, tmp[i + 1] = c.y, tmp[i + 2] = c.z;
        }
    for (u32 y = 0; y < h; ++y) {
        const float sinTheta = std::max(std::sin((static_cast<float>(y) + 0.5f) / static_cast<float>(h) * kPi), 1e-3f);
        const float sigmaH = std::min(sigmaV / sinTheta, static_cast<float>(w) * 0.5f);
        if (sigmaH < 0.25f) {
            std::copy_n(tmp.begin() + static_cast<std::ptrdiff_t>(static_cast<usize>(y) * w * 3), w * 3,
                        img.begin() + static_cast<std::ptrdiff_t>(static_cast<usize>(y) * w * 3));
            continue;
        }
        const auto wh = gaussianWeights(sigmaH);
        const int rh = static_cast<int>(wh.size() / 2);
        for (u32 x = 0; x < w; ++x) {
            Vec3 c{0.0f};
            for (int k = -rh; k <= rh; ++k) {
                const int xx = ((static_cast<int>(x) + k) % static_cast<int>(w) + static_cast<int>(w)) % static_cast<int>(w);
                c = c + at(tmp, w, static_cast<u32>(xx), y) * wh[static_cast<usize>(k + rh)];
            }
            const usize i = (static_cast<usize>(y) * w + x) * 3;
            img[i] = c.x, img[i + 1] = c.y, img[i + 2] = c.z;
        }
    }
}
} // namespace

Vec3 equirectDirection(float u, float v) {
    const float phi = (u - 0.5f) * 2.0f * kPi;
    const float theta = v * kPi;
    return {std::sin(theta) * std::sin(phi), std::cos(theta), -std::sin(theta) * std::cos(phi)};
}

Vec2 equirectUv(Vec3 d) {
    d = math::normalize(d);
    return {0.5f + std::atan2(d.x, -d.z) / (2.0f * kPi), std::acos(std::clamp(d.y, -1.0f, 1.0f)) / kPi};
}

Vec3 sampleEquirect(const std::vector<float>& rgb, u32 width, u32 height, Vec2 uv) {
    const float fx = uv.x * static_cast<float>(width) - 0.5f;
    const float fy = std::clamp(uv.y * static_cast<float>(height) - 0.5f, 0.0f, static_cast<float>(height - 1));
    const float x0f = std::floor(fx), y0f = std::floor(fy);
    const float tx = fx - x0f, ty = fy - y0f;
    const int wi = static_cast<int>(width);
    auto wrapX = [&](int x) { return static_cast<u32>(((x % wi) + wi) % wi); };
    const u32 x0 = wrapX(static_cast<int>(x0f)), x1 = wrapX(static_cast<int>(x0f) + 1);
    const u32 y0 = static_cast<u32>(y0f), y1 = std::min(y0 + 1, height - 1);
    const Vec3 top = at(rgb, width, x0, y0) * (1.0f - tx) + at(rgb, width, x1, y0) * tx;
    const Vec3 bottom = at(rgb, width, x0, y1) * (1.0f - tx) + at(rgb, width, x1, y1) * tx;
    return top * (1.0f - ty) + bottom * ty;
}

std::array<float, 9> shBasis(Vec3 n) {
    return {0.282095f,
            0.488603f * n.y,
            0.488603f * n.z,
            0.488603f * n.x,
            1.092548f * n.x * n.y,
            1.092548f * n.y * n.z,
            0.315392f * (3.0f * n.z * n.z - 1.0f),
            1.092548f * n.x * n.z,
            0.546274f * (n.x * n.x - n.y * n.y)};
}

Vec3 evaluateIrradiance(const std::array<Vec3, 9>& sh, Vec3 n) {
    const auto b = shBasis(n);
    Vec3 e{0.0f};
    for (usize i = 0; i < 9; ++i) e = e + sh[i] * b[i];
    return {std::max(e.x, 0.0f), std::max(e.y, 0.0f), std::max(e.z, 0.0f)};
}

Result<EnvironmentMap> buildEnvironmentMap(const ImageF& src, u32 maxWidth) {
    if (src.width < 2 || src.height < 1 || src.rgb.size() < static_cast<usize>(src.width) * src.height * 3)
        return makeError(Error::Code::InvalidArgument, "environment image is empty");
    const float aspect = static_cast<float>(src.width) / static_cast<float>(src.height);
    if (aspect < 1.5f || aspect > 2.5f)
        return makeError(Error::Code::InvalidArgument, "environment image must be equirectangular (2:1), got " + std::to_string(src.width) + " x " +
                                                           std::to_string(src.height));
    EnvironmentMap env;
    // Resample (bilinear) to a power-of-two 2:1 size.
    u32 width = std::bit_ceil(src.width);  // nearest power of two
    if (static_cast<float>(width) > static_cast<float>(src.width) * 1.5f) width /= 2;
    width = std::clamp(width, 16u, std::max(16u, std::bit_floor(maxWidth)));
    const u32 height = width / 2;
    env.width = width;
    env.height = height;
    std::vector<float> level(static_cast<usize>(width) * height * 3);
    // Large reductions: box-filter the source first so every source texel counts.
    ImageF reduced = src;
    while (reduced.width >= width * 2 && reduced.height >= 2) {
        const u32 nw = reduced.width / 2, nh = reduced.height / 2;
        std::vector<float> d(static_cast<usize>(nw) * nh * 3);
        for (u32 y = 0; y < nh; ++y)
            for (u32 x = 0; x < nw; ++x) {
                const Vec3 c = (at(reduced.rgb, reduced.width, x * 2, y * 2) + at(reduced.rgb, reduced.width, x * 2 + 1, y * 2) +
                                at(reduced.rgb, reduced.width, x * 2, y * 2 + 1) + at(reduced.rgb, reduced.width, x * 2 + 1, y * 2 + 1)) *
                               0.25f;
                const usize i = (static_cast<usize>(y) * nw + x) * 3;
                d[i] = c.x, d[i + 1] = c.y, d[i + 2] = c.z;
            }
        reduced.width = nw;
        reduced.height = nh;
        reduced.rgb = std::move(d);
    }
    for (u32 y = 0; y < height; ++y)
        for (u32 x = 0; x < width; ++x) {
            const Vec2 uv{(static_cast<float>(x) + 0.5f) / static_cast<float>(width), (static_cast<float>(y) + 0.5f) / static_cast<float>(height)};
            const Vec3 c = sampleEquirect(reduced.rgb, reduced.width, reduced.height, uv);
            const usize i = (static_cast<usize>(y) * width + x) * 3;
            level[i] = c.x, level[i + 1] = c.y, level[i + 2] = c.z;
        }
    env.levels.push_back(level);

    // Mip chain down to 8 x 4, each blurred for its roughness (GGX alpha = roughness^2;
    // the blur's angular deviation approximates the lobe). An approximation of the split-sum
    // prefilter, not an exact GGX convolution.
    u32 count = 1;
    for (u32 w = width; w > 8; w /= 2) ++count;
    u32 w = width, h = height;
    for (u32 k = 1; k < count; ++k) {
        std::vector<float> next = downsample(env.levels.back(), w, h);
        w = std::max(1u, w / 2);
        h = std::max(1u, h / 2);
        const float roughness = static_cast<float>(k) / static_cast<float>(count - 1);
        const float sigmaTarget = 0.6f * roughness * roughness;
        const float prevRoughness = static_cast<float>(k - 1) / static_cast<float>(count - 1);
        const float sigmaPrev = 0.6f * prevRoughness * prevRoughness;
        // Blur only what the previous level does not already have (Gaussians add in variance).
        const float sigma = std::sqrt(std::max(sigmaTarget * sigmaTarget - sigmaPrev * sigmaPrev, 0.0f));
        blur(next, w, h, sigma);
        env.levels.push_back(std::move(next));
    }

    // SH9 projection of the radiance from a small level, weighted by solid angle, then the
    // cosine-lobe convolution (Ramamoorthi & Hanrahan): A0 = pi, A1 = 2pi/3, A2 = pi/4; / pi.
    usize shLevel = 0;
    while (shLevel + 1 < env.levels.size() && (width >> shLevel) > 64) ++shLevel;
    const u32 sw = std::max(1u, width >> shLevel), sh = std::max(1u, height >> shLevel);
    const std::vector<float>& small = env.levels[shLevel];
    std::array<Vec3, 9> coeffs{};
    float weightSum = 0.0f;
    Vec3 average{0.0f};
    for (u32 y = 0; y < sh; ++y) {
        const float v = (static_cast<float>(y) + 0.5f) / static_cast<float>(sh);
        const float dOmega = (2.0f * kPi / static_cast<float>(sw)) * (kPi / static_cast<float>(sh)) * std::sin(v * kPi);
        for (u32 x = 0; x < sw; ++x) {
            const Vec3 dir = equirectDirection((static_cast<float>(x) + 0.5f) / static_cast<float>(sw), v);
            const Vec3 radiance = at(small, sw, x, y);
            const auto b = shBasis(dir);
            for (usize i = 0; i < 9; ++i) coeffs[i] = coeffs[i] + radiance * (b[i] * dOmega);
            weightSum += dOmega;
            average = average + radiance * dOmega;
        }
    }
    const float norm = 4.0f * kPi / std::max(weightSum, 1e-6f);  // exact total solid angle
    constexpr float band[9] = {1.0f, 2.0f / 3.0f, 2.0f / 3.0f, 2.0f / 3.0f, 0.25f, 0.25f, 0.25f, 0.25f, 0.25f};
    for (usize i = 0; i < 9; ++i) env.sh[i] = coeffs[i] * (norm * band[i]);
    env.average = average * (1.0f / std::max(weightSum, 1e-6f));
    return env;
}

} // namespace agr
