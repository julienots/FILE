#include <doctest/doctest.h>

#include <cmath>

#include "agr/assets/Environment.hpp"

using namespace agr;
using math::Vec3;

namespace {
ImageF make(u32 w, u32 h, auto&& radiance) {
    ImageF img;
    img.width = w;
    img.height = h;
    img.rgb.resize(static_cast<usize>(w) * h * 3);
    for (u32 y = 0; y < h; ++y)
        for (u32 x = 0; x < w; ++x) {
            const Vec3 d = equirectDirection((static_cast<float>(x) + 0.5f) / static_cast<float>(w), (static_cast<float>(y) + 0.5f) / static_cast<float>(h));
            const Vec3 c = radiance(d);
            const usize i = (static_cast<usize>(y) * w + x) * 3;
            img.rgb[i] = c.x, img.rgb[i + 1] = c.y, img.rgb[i + 2] = c.z;
        }
    return img;
}
bool close(Vec3 a, Vec3 b, float eps) { return std::abs(a.x - b.x) < eps && std::abs(a.y - b.y) < eps && std::abs(a.z - b.z) < eps; }
}  // namespace

TEST_SUITE("environment") {

TEST_CASE("equirect mapping: directions and uvs round trip") {
    CHECK(close(equirectDirection(0.5f, 0.5f), {0, 0, -1}, 1e-5f));  // image center looks toward -Z
    CHECK(close(equirectDirection(0.75f, 0.5f), {1, 0, 0}, 1e-5f));
    CHECK(equirectDirection(0.3f, 0.0f).y > 0.999f);                  // top row looks up
    for (Vec3 d : {Vec3{0.3f, 0.5f, -0.8f}, Vec3{-1, 0.2f, 0.1f}, Vec3{0.1f, -0.9f, 0.3f}}) {
        const auto uv = equirectUv(d);
        CHECK(close(equirectDirection(uv.x, uv.y), math::normalize(d), 1e-4f));
    }
}

TEST_CASE("a uniform sky gives the same irradiance everywhere, equal to its radiance") {
    const auto img = make(128, 64, [](Vec3) { return Vec3{0.5f, 1.0f, 2.0f}; });
    auto env = buildEnvironmentMap(img, 256);
    REQUIRE(env);
    CHECK(env.value().width == 128);
    CHECK(env.value().height == 64);
    CHECK(env.value().levels.size() == 5);  // 128, 64, 32, 16, 8
    CHECK(close(env.value().average, {0.5f, 1.0f, 2.0f}, 1e-3f));
    for (Vec3 n : {Vec3{0, 1, 0}, Vec3{0, -1, 0}, Vec3{1, 0, 0}, math::normalize(Vec3{1, 1, -1})})
        CHECK(close(evaluateIrradiance(env.value().sh, n), {0.5f, 1.0f, 2.0f}, 0.02f));
    // Blurred levels of a uniform image stay uniform.
    const auto& last = env.value().levels.back();
    for (float v : {last[0], last[3], last[last.size() - 3]}) CHECK(std::abs(v - 0.5f) < 1e-3f);
}

TEST_CASE("a bright upper hemisphere lights upward normals: cosine-convolved irradiance") {
    const auto img = make(256, 128, [](Vec3 d) { return d.y > 0 ? Vec3{1, 1, 1} : Vec3{0, 0, 0}; });
    auto env = buildEnvironmentMap(img, 256);
    REQUIRE(env);
    // Analytic E/pi for a facing normal: 1 (fully up), 0.5 (horizontal), 0 (down). SH9 rings a little.
    CHECK(std::abs(evaluateIrradiance(env.value().sh, {0, 1, 0}).x - 1.0f) < 0.1f);
    CHECK(std::abs(evaluateIrradiance(env.value().sh, {1, 0, 0}).x - 0.5f) < 0.05f);
    CHECK(evaluateIrradiance(env.value().sh, {0, -1, 0}).x < 0.1f);
}

TEST_CASE("rough levels blur a small bright spot; the mirror level keeps it") {
    const Vec3 spot = math::normalize(Vec3{0.2f, 0.3f, -1.0f});
    const auto img = make(512, 256, [&](Vec3 d) { return math::dot(d, spot) > 0.995f ? Vec3{50, 50, 50} : Vec3{0.1f, 0.1f, 0.1f}; });
    auto env = buildEnvironmentMap(img, 512);
    REQUIRE(env);
    const auto uv = equirectUv(spot);
    auto levelSample = [&](usize k, Vec3 dir) {
        const auto t = equirectUv(dir);
        return sampleEquirect(env.value().levels[k], std::max(1u, env.value().width >> k), std::max(1u, env.value().height >> k), t).x;
    };
    (void)uv;
    CHECK(levelSample(0, spot) > 40.0f);
    const Vec3 aside = math::normalize(Vec3{0.5f, 0.3f, -1.0f});  // ~16 degrees away
    CHECK(levelSample(0, aside) < 0.2f);
    const usize rough = env.value().levels.size() - 3;
    CHECK(levelSample(rough, spot) < 20.0f);    // spread out...
    CHECK(levelSample(rough, aside) > 0.15f);   // ...into the neighborhood
    // Energy is roughly conserved through the chain (mean radiance).
    auto mean = [&](usize k) {
        const u32 w = std::max(1u, env.value().width >> k), h = std::max(1u, env.value().height >> k);
        double sum = 0, wsum = 0;
        for (u32 y = 0; y < h; ++y) {
            const double s = std::sin((y + 0.5) / h * 3.14159265);
            for (u32 x = 0; x < w; ++x) sum += env.value().levels[k][(static_cast<usize>(y) * w + x) * 3] * s, wsum += s;
        }
        return sum / wsum;
    };
    CHECK(std::abs(mean(rough) - mean(0)) < mean(0) * 0.15);
}

TEST_CASE("invalid images are rejected; large ones are reduced to the maximum width") {
    ImageF square;
    square.width = square.height = 64;
    square.rgb.assign(64 * 64 * 3, 1.0f);
    CHECK_FALSE(buildEnvironmentMap(square));
    CHECK_FALSE(buildEnvironmentMap(ImageF{}));
    const auto big = make(1000, 500, [](Vec3 d) { return Vec3{d.y * 0.5f + 0.5f, 0.2f, 0.1f}; });
    auto env = buildEnvironmentMap(big, 256);
    REQUIRE(env);
    CHECK(env.value().width == 256);
    CHECK(env.value().levels.front().size() == 256u * 128u * 3u);
    const auto mid = make(1000, 500, [](Vec3) { return Vec3{1, 1, 1}; });
    CHECK(buildEnvironmentMap(mid, 4096).value().width == 1024);  // nearest power of two
}

TEST_CASE("decodeImageLinear: 8-bit images are converted from sRGB") {
    // 1x1 white and mid-gray PNG would need an encoder: use the BMP-free path through encodePng.
    const std::vector<u8> rgba{255, 128, 0, 255};
    auto png = encodePng(1, 1, rgba);
    REQUIRE(png);
    auto img = decodeImageLinear(png.value(), "px.png");
    REQUIRE(img);
    CHECK(img.value().width == 1);
    CHECK(std::abs(img.value().rgb[0] - 1.0f) < 1e-4f);
    CHECK(std::abs(img.value().rgb[1] - 0.2158605f) < 1e-3f);  // sRGB 128 -> linear
    CHECK(img.value().rgb[2] == 0.0f);
    CHECK_FALSE(decodeImageLinear({}, "empty"));
}

TEST_CASE("Radiance .hdr round trip keeps values above 1") {
    const std::vector<float> rgb{4.0f, 0.5f, 0.0f, 100.0f, 1.0f, 0.25f};
    auto hdr = encodeHdr(2, 1, rgb);
    REQUIRE(hdr);
    auto img = decodeImageLinear(hdr.value(), "two.hdr");
    REQUIRE(img);
    REQUIRE(img.value().rgb.size() == 6);
    // RGBE shares one exponent per pixel: precision is relative to the brightest channel.
    for (usize i = 0; i < 6; ++i) CHECK(std::abs(img.value().rgb[i] - rgb[i]) <= (i < 3 ? 4.0f : 100.0f) * 0.01f);
}

}
