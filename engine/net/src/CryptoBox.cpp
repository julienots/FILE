#include "agr/net/CryptoBox.hpp"

#include <mbedtls/ctr_drbg.h>
#include <mbedtls/entropy.h>
#include <mbedtls/gcm.h>
#include <mbedtls/md.h>
#include <mbedtls/pkcs5.h>
#include <mbedtls/sha256.h>

#include <mutex>

namespace agr::net::cryptobox {

namespace {
const u8* bytes(std::string_view s) { return reinterpret_cast<const u8*>(s.data()); }

std::string key32(std::string_view key) {
    if (key.size() == 32) return std::string(key);
    std::string out(32, '\0');
    mbedtls_sha256(bytes(key), key.size(), reinterpret_cast<u8*>(out.data()), 0);
    return out;
}

struct Rng {
    mbedtls_entropy_context entropy;
    mbedtls_ctr_drbg_context drbg;
    bool ok = false;
    std::mutex mutex;
    Rng() {
        mbedtls_entropy_init(&entropy);
        mbedtls_ctr_drbg_init(&drbg);
        static const char personal[] = "agr.cryptobox";
        ok = mbedtls_ctr_drbg_seed(&drbg, mbedtls_entropy_func, &entropy, reinterpret_cast<const u8*>(personal), sizeof personal - 1) == 0;
    }
    ~Rng() {
        mbedtls_ctr_drbg_free(&drbg);
        mbedtls_entropy_free(&entropy);
    }
};
Rng& rng() {
    static Rng r;
    return r;
}
} // namespace

std::string hmacSha256(std::string_view key, std::string_view message) {
    std::string out(32, '\0');
    mbedtls_md_hmac(mbedtls_md_info_from_type(MBEDTLS_MD_SHA256), bytes(key), key.size(), bytes(message), message.size(), reinterpret_cast<u8*>(out.data()));
    return out;
}

std::string randomBytes(usize count) {
    std::string out(count, '\0');
    Rng& r = rng();
    std::lock_guard lock(r.mutex);
    for (usize at = 0; at < count;) {  // the generator gives at most 1024 bytes per call
        const usize n = std::min<usize>(count - at, 1024);
        if (!r.ok || mbedtls_ctr_drbg_random(&r.drbg, reinterpret_cast<u8*>(out.data() + at), n) != 0) return {};
        at += n;
    }
    return out;
}

std::string deriveKey(std::string_view password, std::string_view salt, u32 iterations, usize length) {
    std::string out(length, '\0');
    mbedtls_pkcs5_pbkdf2_hmac_ext(MBEDTLS_MD_SHA256, bytes(password), password.size(), bytes(salt), salt.size(), std::max(iterations, 1u),
                                  static_cast<u32>(length), reinterpret_cast<u8*>(out.data()));
    return out;
}

std::string encrypt(std::string_view key, std::string_view plaintext) {
    const std::string k = key32(key), nonce = randomBytes(12);
    if (nonce.size() != 12) return {};
    std::string out = nonce;
    out.resize(12 + plaintext.size() + 16);
    mbedtls_gcm_context gcm;
    mbedtls_gcm_init(&gcm);
    const bool ok = mbedtls_gcm_setkey(&gcm, MBEDTLS_CIPHER_ID_AES, bytes(k), 256) == 0 &&
                    mbedtls_gcm_crypt_and_tag(&gcm, MBEDTLS_GCM_ENCRYPT, plaintext.size(), bytes(nonce), 12, nullptr, 0, bytes(plaintext),
                                              reinterpret_cast<u8*>(out.data() + 12), 16, reinterpret_cast<u8*>(out.data() + 12 + plaintext.size())) == 0;
    mbedtls_gcm_free(&gcm);
    return ok ? out : std::string();
}

std::optional<std::string> decrypt(std::string_view key, std::string_view box) {
    if (box.size() < 28) return std::nullopt;
    const std::string k = key32(key);
    const usize n = box.size() - 28;
    std::string out(n, '\0');
    mbedtls_gcm_context gcm;
    mbedtls_gcm_init(&gcm);
    const bool ok = mbedtls_gcm_setkey(&gcm, MBEDTLS_CIPHER_ID_AES, bytes(k), 256) == 0 &&
                    mbedtls_gcm_auth_decrypt(&gcm, n, bytes(box), 12, nullptr, 0, bytes(box) + 12 + n, 16, bytes(box) + 12,
                                             reinterpret_cast<u8*>(out.data())) == 0;
    mbedtls_gcm_free(&gcm);
    if (!ok) return std::nullopt;
    return out;
}

} // namespace agr::net::cryptobox
