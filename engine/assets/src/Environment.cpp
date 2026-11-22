#include "agr/assets/Environment.hpp"
#include "agr/core/FileSystem.hpp"
#include <nlohmann/json.hpp>

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

// ------------------------------------------------------------------ cube map skies
CubeLayout detectCubeLayout(u32 w, u32 h) {
    if (w == 0 || h == 0) return CubeLayout::None;
    if (w * 3 == h * 4 && w % 4 == 0) return CubeLayout::HorizontalCross;
    if (w * 4 == h * 3 && w % 3 == 0) return CubeLayout::VerticalCross;
    if (w == h * 6) return CubeLayout::Strip6x1;
    if (w * 2 == h * 3 && w % 3 == 0) return CubeLayout::Grid3x2;
    return CubeLayout::None;
}

Result<CubeFaces> splitCubeImage(const ImageF& img, CubeLayout layout) {
    u32 s = 0;
    // Face cell (column, row) per face +X, -X, +Y, -Y, +Z, -Z; `flip` = rotated 180 degrees.
    std::array<std::array<u32, 2>, 6> cells{};
    std::array<bool, 6> flip{};
    switch (layout) {
        case CubeLayout::HorizontalCross:  // . +Y . . / -X +Z +X -Z / . -Y . .
            s = img.width / 4;
            cells = {{{2, 1}, {0, 1}, {1, 0}, {1, 2}, {1, 1}, {3, 1}}};
            break;
        case CubeLayout::VerticalCross:  // . +Y . / -X +Z +X / . -Y . / . -Z . (upside down)
            s = img.width / 3;
            cells = {{{2, 1}, {0, 1}, {1, 0}, {1, 2}, {1, 1}, {1, 3}}};
            flip[5] = true;
            break;
        case CubeLayout::Strip6x1:
            s = img.height;
            cells = {{{0, 0}, {1, 0}, {2, 0}, {3, 0}, {4, 0}, {5, 0}}};
            break;
        case CubeLayout::Grid3x2:  // +X -X +Y / -Y +Z -Z
            s = img.width / 3;
            cells = {{{0, 0}, {1, 0}, {2, 0}, {0, 1}, {1, 1}, {2, 1}}};
            break;
        case CubeLayout::None: return makeError(Error::Code::InvalidArgument, "not a cube map layout");
    }
    CubeFaces faces;
    for (int f = 0; f < 6; ++f) {
        ImageF& face = faces[static_cast<usize>(f)];
        face.width = face.height = s;
        face.rgb.resize(static_cast<usize>(s) * s * 3);
        for (u32 y = 0; y < s; ++y)
            for (u32 x = 0; x < s; ++x) {
                const u32 sx = flip[f] ? s - 1 - x : x, sy = flip[f] ? s - 1 - y : y;
                const usize src = (static_cast<usize>(cells[f][1] * s + sy) * img.width + cells[f][0] * s + sx) * 3;
                const usize dst = (static_cast<usize>(y) * s + x) * 3;
                for (int c = 0; c < 3; ++c) face.rgb[dst + c] = img.rgb[src + c];
            }
    }
    return faces;
}

Result<ImageF> cubeToEquirect(const CubeFaces& faces, u32 width) {
    const u32 s = faces[0].width;
    for (const ImageF& f : faces)
        if (f.width != s || f.height != s || f.rgb.size() != static_cast<usize>(s) * s * 3 || s == 0)
            return makeError(Error::Code::InvalidArgument, "cube map faces must be square and the same size");
    width = std::max(8u, width & ~1u);
    ImageF out;
    out.width = width;
    out.height = width / 2;
    out.rgb.resize(static_cast<usize>(out.width) * out.height * 3);
    auto sample = [&](const ImageF& f, float u, float v, float* rgb) {  // bilinear, clamped
        const float fx = std::clamp(u * s - 0.5f, 0.0f, s - 1.0f), fy = std::clamp(v * s - 0.5f, 0.0f, s - 1.0f);
        const u32 x0 = static_cast<u32>(fx), y0 = static_cast<u32>(fy), x1 = std::min(x0 + 1, s - 1), y1 = std::min(y0 + 1, s - 1);
        const float tx = fx - x0, ty = fy - y0;
        for (int c = 0; c < 3; ++c) {
            auto at = [&](u32 x, u32 y) { return f.rgb[(static_cast<usize>(y) * s + x) * 3 + c]; };
            rgb[c] = (at(x0, y0) * (1 - tx) + at(x1, y0) * tx) * (1 - ty) + (at(x0, y1) * (1 - tx) + at(x1, y1) * tx) * ty;
        }
    };
    for (u32 y = 0; y < out.height; ++y)
        for (u32 x = 0; x < out.width; ++x) {
            const math::Vec3 d = equirectDirection((x + 0.5f) / out.width, (y + 0.5f) / out.height);
            const float ax = std::fabs(d.x), ay = std::fabs(d.y), az = std::fabs(d.z);
            int face;
            float sc, tc, ma;
            if (ax >= ay && ax >= az) face = d.x > 0 ? 0 : 1, ma = ax, sc = d.x > 0 ? -d.z : d.z, tc = -d.y;
            else if (ay >= az) face = d.y > 0 ? 2 : 3, ma = ay, sc = d.x, tc = d.y > 0 ? d.z : -d.z;
            else face = d.z > 0 ? 4 : 5, ma = az, sc = d.z > 0 ? d.x : -d.x, tc = -d.y;
            sample(faces[static_cast<usize>(face)], (sc / ma + 1.0f) * 0.5f, (tc / ma + 1.0f) * 0.5f,
                   &out.rgb[(static_cast<usize>(y) * out.width + x) * 3]);
        }
    return out;
}

Result<ImageF> loadSkyImage(VirtualFileSystem& vfs, const std::string& uri) {
    if (uri.size() > 8 && uri.compare(uri.size() - 8, 8, ".agrcube") == 0) {
        auto text = vfs.readText(uri);
        if (!text) return text.error();
        const auto j = nlohmann::json::parse(text.value(), nullptr, false);
        if (j.is_discarded() || !j.is_object()) return makeError(Error::Code::ParseError, uri + ": invalid JSON");
        CubeFaces faces;
        const char* keys[6] = {"px", "nx", "py", "ny", "pz", "nz"};
        for (int i = 0; i < 6; ++i) {
            const std::string face = j.value(keys[i], "");
            if (face.empty()) return makeError(Error::Code::ParseError, uri + ": missing face \"" + keys[i] + "\"");
            auto bytes = vfs.readBytes(face);
            if (!bytes) return bytes.error();
            auto img = decodeImageLinear(bytes.value(), face);
            if (!img) return img.error();
            faces[static_cast<usize>(i)] = std::move(img.value());
        }
        return cubeToEquirect(faces, faces[0].width * 4);
    }
    auto bytes = vfs.readBytes(uri);
    if (!bytes) return bytes.error();
    auto img = decodeImageLinear(bytes.value(), uri);
    if (!img) return img.error();
    const CubeLayout layout = detectCubeLayout(img.value().width, img.value().height);
    if (layout == CubeLayout::None) return img;
    auto faces = splitCubeImage(img.value(), layout);
    if (!faces) return faces.error();
    return cubeToEquirect(faces.value(), faces.value()[0].width * 4);
}

}  // namespace agr
