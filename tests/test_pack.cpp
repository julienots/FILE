#include <doctest/doctest.h>

#include <filesystem>
#include <fstream>

#include "agr/core/Crypto.hpp"
#include "agr/core/Pack.hpp"
#include "agr/core/Etc2.hpp"
#include <cmath>
#include "agr/core/Uuid.hpp"

using namespace agr;
namespace fs = std::filesystem;

TEST_SUITE("packs") {
TEST_CASE("crypto: SHA-256 and AES-256 match the standard test vectors") {
    CHECK(crypto::sha256Hex("") == "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855");
    CHECK(crypto::sha256Hex("abc") == "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");
    CHECK(crypto::sha256Hex("abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq") ==
          "248d6a61d20638b8e5c026930c3e6039a33ce45964ff2167f6ecedd419db06c1");
    CHECK(crypto::sha256Hex(std::string(1000000, 'a')) == "cdc76e5c9914fb9281a1c7e284d73e67f1809a48a497200e046d39ccc7112cd0");
    // FIPS-197 appendix C.3.
    crypto::AesKey key;
    for (int i = 0; i < 32; ++i) key[i] = static_cast<u8>(i);
    const u8 plain[16] = {0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88, 0x99, 0xaa, 0xbb, 0xcc, 0xdd, 0xee, 0xff};
    u8 out[16];
    crypto::Aes256(key).encryptBlock(plain, out);
    CHECK(crypto::toHex(out, 16) == "8ea2b7ca516745bfeafc49904b496089");
    // CTR: decrypting from the middle of the stream gives the same bytes as a full pass.
    const crypto::Aes256 aes(key);
    std::string msg = "The quick brown fox jumps over the lazy dog, again and again and again.";
    std::string enc = msg;
    crypto::aes256Ctr(aes, 42, 0, reinterpret_cast<u8*>(enc.data()), enc.size());
    CHECK(enc != msg);
    std::string part = enc.substr(21, 30);
    crypto::aes256Ctr(aes, 42, 21, reinterpret_cast<u8*>(part.data()), part.size());
    CHECK(part == msg.substr(21, 30));
    crypto::aes256Ctr(aes, 42, 0, reinterpret_cast<u8*>(enc.data()), enc.size());
    CHECK(enc == msg);
}

TEST_CASE("packs: plain and encrypted round trip, wrong key, listing, overlays for patches") {
    const fs::path dir = fs::temp_directory_path() / ("agr_pack_" + Uuid::generate().toString());
    fs::create_directories(dir);
    PackWriter w;
    w.add("game.json", std::string_view("{\"name\":\"Demo\"}"));
    w.add("assets/textures/hero.png", std::string_view("PNGDATA-hero"));
    w.add("assets/scripts/main.lua", std::string_view("print('v1')"));
    w.add("assets/empty.txt", ByteBuffer{});
    REQUIRE(w.write(dir / "plain.agrpak"));
    const crypto::AesKey key = packKeyFromText("secret");
    REQUIRE(w.write(dir / "enc.agrpak", &key));

    auto plain = PackFileProvider::openFile(dir / "plain.agrpak");
    REQUIRE(plain);
    CHECK_FALSE(plain.value()->encrypted());
    CHECK(plain.value()->entries().size() == 4);
    auto text = plain.value()->read("assets/scripts/main.lua");
    REQUIRE(text);
    CHECK(std::string(text.value().begin(), text.value().end()) == "print('v1')");
    CHECK(plain.value()->read("assets/empty.txt").value().empty());
    CHECK_FALSE(plain.value()->read("nope.txt"));
    CHECK(plain.value()->exists("assets/textures"));
    auto root = plain.value()->list("");
    REQUIRE(root);
    CHECK(root.value() == std::vector<std::string>{"assets/", "game.json"});
    CHECK(plain.value()->list("assets").value() == std::vector<std::string>{"empty.txt", "scripts/", "textures/"});
    CHECK_FALSE(plain.value()->write("x", "y", 1));

    // Encrypted: the bytes on disk do not contain the plain text; a key is required.
    std::ifstream raw(dir / "enc.agrpak", std::ios::binary);
    const std::string disk((std::istreambuf_iterator<char>(raw)), {});
    CHECK(disk.find("print('v1')") == std::string::npos);
    CHECK(disk.find("hero.png") == std::string::npos);  // the index is encrypted too
    CHECK_FALSE(PackFileProvider::openFile(dir / "enc.agrpak"));
    const crypto::AesKey wrong = packKeyFromText("guess");
    CHECK_FALSE(PackFileProvider::openFile(dir / "enc.agrpak", &wrong));
    auto enc = PackFileProvider::openFile(dir / "enc.agrpak", &key);
    REQUIRE(enc);
    CHECK(enc.value()->encrypted());
    auto hero = enc.value()->read("assets/textures/hero.png");
    REQUIRE(hero);
    CHECK(std::string(hero.value().begin(), hero.value().end()) == "PNGDATA-hero");
    CHECK(enc.value()->entries()[0].hash == plain.value()->entries()[0].hash);

    // A patch pack layered over the game replaces one file and adds one.
    PackWriter patch;
    patch.add("assets/scripts/main.lua", std::string_view("print('v2')"));
    patch.add("assets/levels/bonus.agrscene", std::string_view("{}"));
    REQUIRE(patch.write(dir / "patch1.agrpak", &key));
    OverlayFileProvider game;
    game.addLayer(std::shared_ptr<FileProvider>(std::move(enc.value())));
    game.addLayer(std::shared_ptr<FileProvider>(std::move(PackFileProvider::openFile(dir / "patch1.agrpak", &key).value())));
    SubdirFileProvider assets(std::shared_ptr<FileProvider>(&game, [](FileProvider*) {}), "assets");
    auto main = assets.read("scripts/main.lua");
    REQUIRE(main);
    CHECK(std::string(main.value().begin(), main.value().end()) == "print('v2')");
    CHECK(assets.exists("textures/hero.png"));
    CHECK(assets.list("").value() == std::vector<std::string>{"empty.txt", "levels/", "scripts/", "textures/"});

    // A damaged pack is refused.
    std::ofstream(dir / "bad.agrpak", std::ios::binary) << "AGRPAK01 garbage";
    CHECK_FALSE(PackFileProvider::openFile(dir / "bad.agrpak"));
    std::error_code ec;
    fs::remove_all(dir, ec);
}
}  // TEST_SUITE

TEST_CASE("ETC2: round trip quality, alpha, odd sizes, mips and the KTX container") {
    // A smooth gradient with a sharp edge and varying alpha.
    const u32 w = 37, h = 22;  // not multiples of 4
    std::vector<u8> src(w * h * 4);
    for (u32 y = 0; y < h; ++y)
        for (u32 x = 0; x < w; ++x) {
            u8* p = &src[(y * w + x) * 4];
            p[0] = static_cast<u8>(x * 255 / (w - 1));
            p[1] = static_cast<u8>(y * 255 / (h - 1));
            p[2] = x < w / 2 ? 30 : 220;
            p[3] = static_cast<u8>(x < 10 ? 255 : 128 + (x * 3) % 100);
        }
    const auto blocks = etc2::encodeEtc2Rgba(src.data(), w, h);
    CHECK(blocks.size() == etc2::levelBytes(w, h));
    CHECK(blocks.size() == 10 * 6 * 16);
    std::vector<u8> back(w * h * 4);
    etc2::decodeEtc2Rgba(blocks.data(), w, h, back.data());
    double se = 0, seA = 0;
    for (usize i = 0; i < src.size(); ++i) {
        const double d = static_cast<double>(src[i]) - back[i];
        (i % 4 == 3 ? seA : se) += d * d;
    }
    const double psnr = 10.0 * std::log10(255.0 * 255.0 / (se / (w * h * 3)));
    const double psnrA = 10.0 * std::log10(255.0 * 255.0 / std::max(seA / (w * h), 1e-9));
    CHECK(psnr > 30.0);   // typical ETC2 quality on such content
    CHECK(psnrA > 38.0);
    // Flat opaque alpha stays exactly 255.
    std::vector<u8> solid(16 * 16 * 4, 200);
    for (usize i = 3; i < solid.size(); i += 4) solid[i] = 255;
    const auto sb = etc2::encodeEtc2Rgba(solid.data(), 16, 16);
    std::vector<u8> sd(16 * 16 * 4);
    etc2::decodeEtc2Rgba(sb.data(), 16, 16, sd.data());
    for (usize i = 0; i < sd.size(); ++i) CHECK(std::abs(int(sd[i]) - int(i % 4 == 3 ? 255 : 200)) <= 2);
    // Mips down to 1x1 and the KTX file.
    const auto img = etc2::compressWithMips(src.data(), w, h);
    CHECK(img.levels.size() == 6);  // 37x22, 18x11, 9x5, 4x2, 2x1, 1x1
    const auto ktx = etc2::writeKtx(img);
    CHECK(etc2::isKtx(ktx));
    auto read = etc2::readKtx(ktx);
    REQUIRE(read);
    CHECK(read.value().width == w);
    CHECK(read.value().levels.size() == img.levels.size());
    CHECK(read.value().levels[2] == img.levels[2]);
    CHECK_FALSE(etc2::readKtx(std::vector<u8>(ktx.begin(), ktx.begin() + 70)));
}
