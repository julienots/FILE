#include <doctest/doctest.h>

#include <chrono>
#include <filesystem>
#include <thread>

#include "agr/assets/AssetManager.hpp"
#include "agr/assets/Image.hpp"
#include "agr/core/Uuid.hpp"

using namespace agr;
namespace fs = std::filesystem;

namespace {

Image makeImage(u32 w, u32 h, u8 r, u8 g, u8 b) {
    Image img;
    img.width = w;
    img.height = h;
    img.pixels.resize(static_cast<usize>(w) * h * 4);
    for (usize i = 0; i < img.pixels.size(); i += 4) {
        img.pixels[i] = r, img.pixels[i + 1] = g, img.pixels[i + 2] = b, img.pixels[i + 3] = 255;
    }
    return img;
}

struct TempAssets {
    fs::path root = fs::temp_directory_path() / ("agr_assets_" + Uuid::generate().toString());
    VirtualFileSystem vfs;
    TempAssets() {
        fs::create_directories(root);
        vfs.mount("assets", std::make_unique<NativeFileProvider>(root.string(), false));
    }
    ~TempAssets() { fs::remove_all(root); }
    void writePng(const std::string& uri, const Image& img) {
        auto png = encodePng(img.width, img.height, img.pixels);
        REQUIRE(png);
        REQUIRE(vfs.writeBytes(uri, png.value().data(), png.value().size()));
    }
};

}  // namespace

TEST_SUITE("assets") {

TEST_CASE("PNG encode / decode round trip keeps pixels and orientation") {
    Image img = makeImage(3, 2, 10, 20, 30);
    img.pixels[0] = 255;  // top-left red channel marker
    auto png = encodePng(img.width, img.height, img.pixels);
    REQUIRE(png);
    auto decoded = decodeImage(png.value(), "roundtrip");
    REQUIRE(decoded);
    CHECK(decoded.value().width == 3);
    CHECK(decoded.value().height == 2);
    CHECK(decoded.value().pixels == img.pixels);
    CHECK_FALSE(decodeImage(std::vector<u8>{1, 2, 3, 4}, "garbage"));
    CHECK_FALSE(decodeImage({}, "empty"));
    CHECK_FALSE(encodePng(0, 4, img.pixels));
}

TEST_CASE("downscaling averages 2x2 blocks and respects the limit") {
    Image img = makeImage(4, 2, 0, 0, 0);
    // Left 2x2 block: white / black checker -> mid grey.
    img.pixels[0] = 255;
    img.pixels[(1 * 4 + 1) * 4] = 255;
    const Image half = downscaleHalf(img);
    CHECK(half.width == 2);
    CHECK(half.height == 1);
    CHECK(half.pixels[0] == 128);
    Image big = makeImage(1000, 300, 1, 2, 3);
    CHECK(fitToMaxSize(big, 256) == 2);
    CHECK(big.width == 250);
    CHECK(big.height == 75);
    Image odd = makeImage(1, 1, 9, 9, 9);
    CHECK(downscaleHalf(odd).width == 1);
}

TEST_CASE("AssetManager without GPU: async load, cache, failure, release") {
    TempAssets t;
    t.writePng("assets://hero.png", makeImage(64, 32, 200, 100, 50));
    JobSystem jobs(2);
    AssetManager am(t.vfs, jobs, nullptr);

    const TextureAsset a = am.loadTexture("assets://hero.png");
    const TextureAsset b = am.loadTexture("assets://hero.png");
    CHECK(a == b);  // cached
    const TextureAsset other = am.loadTexture("assets://hero.png", {.srgb = false});
    CHECK(other != a);  // different import options -> different asset
    const TextureAsset missing = am.loadTexture("assets://nope.png");
    am.waitAll();

    REQUIRE(am.texture(a));
    CHECK(am.texture(a)->state == AssetState::Ready);
    CHECK(am.texture(a)->width == 64);
    CHECK(am.texture(a)->refCount == 2);
    REQUIRE(am.cpuImage(a));
    CHECK(am.cpuImage(a)->pixels[0] == 200);
    CHECK(am.texture(missing)->state == AssetState::Failed);
    CHECK_FALSE(am.texture(missing)->error.empty());
    CHECK(am.stats().failed == 1);

    am.release(a);
    am.update();
    CHECK(am.texture(a) != nullptr);  // one reference left
    am.release(a);
    am.update();
    CHECK(am.texture(a) == nullptr);  // collected
    const TextureAsset again = am.loadTexture("assets://hero.png", {}, LoadMode::Blocking);
    CHECK(am.texture(again)->state == AssetState::Ready);  // blocking load is complete on return
}

TEST_CASE("AssetManager applies the max texture size") {
    TempAssets t;
    t.writePng("assets://large.png", makeImage(512, 256, 1, 2, 3));
    JobSystem jobs(1);
    AssetManager am(t.vfs, jobs, nullptr);
    am.setMaxTextureSize(128);
    const TextureAsset a = am.loadTexture("assets://large.png", {}, LoadMode::Blocking);
    CHECK(am.texture(a)->width == 128);
    CHECK(am.texture(a)->height == 64);
    CHECK(am.texture(a)->sourceWidth == 512);
}

TEST_CASE("hot reload picks up modified files") {
    TempAssets t;
    t.writePng("assets://live.png", makeImage(8, 8, 10, 10, 10));
    JobSystem jobs(1);
    AssetManager am(t.vfs, jobs, nullptr);
    const TextureAsset a = am.loadTexture("assets://live.png", {}, LoadMode::Blocking);
    CHECK(am.reloadChanged() == 0);
    // Make sure the timestamp differs even on coarse file systems.
    const auto path = t.root / "live.png";
    t.writePng("assets://live.png", makeImage(16, 8, 99, 10, 10));
    fs::last_write_time(path, fs::last_write_time(path) + std::chrono::seconds(2));
    CHECK(am.reloadChanged() == 1);
    am.waitAll();
    CHECK(am.texture(a)->width == 16);
    CHECK(am.texture(a)->version == 2);
    CHECK(am.cpuImage(a)->pixels[0] == 99);
}

}  // TEST_SUITE
