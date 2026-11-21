#pragma once
// Small self-contained primitives: SHA-256 and AES-256 (CTR mode). Used by data packs
// (encrypted game data) and agr.sha256. Not hardened against side channels.
#include <array>
#include <string>
#include <string_view>

#include "agr/core/Types.hpp"

namespace agr::crypto {

using Sha256Digest = std::array<u8, 32>;
Sha256Digest sha256(const void* data, usize size);
inline Sha256Digest sha256(std::string_view s) { return sha256(s.data(), s.size()); }
std::string toHex(const u8* data, usize size);
inline std::string sha256Hex(std::string_view s) {
    const Sha256Digest d = sha256(s);
    return toHex(d.data(), d.size());
}

using AesKey = std::array<u8, 32>;
/// AES-256 block cipher (encryption direction; CTR only needs this one).
class Aes256 {
public:
    explicit Aes256(const AesKey& key);
    void encryptBlock(const u8 in[16], u8 out[16]) const;

private:
    u8 m_round[15][16];
};

/// XORs `data` with the AES-256-CTR key stream: counter block = nonce (8 bytes) + big-endian
/// block index starting at `offset` / 16. The same call encrypts and decrypts; `offset` lets a
/// reader start in the middle of a stream.
void aes256Ctr(const Aes256& aes, u64 nonce, u64 offset, u8* data, usize size);

/// Random bytes from the system (std::random_device).
void randomBytes(u8* out, usize size);

} // namespace agr::crypto
