#pragma once
// Cryptography for games and tools (like Godot's Crypto / HMACContext / AESContext): message
// authentication, authenticated encryption, secure random bytes and password-based keys.
#include <optional>
#include <string>
#include <string_view>

#include "agr/core/Types.hpp"

namespace agr::net::cryptobox {

/// HMAC-SHA-256 (32 raw bytes): sign a score, a save, a request so it cannot be forged.
std::string hmacSha256(std::string_view key, std::string_view message);
/// `count` bytes from a cryptographic random generator.
std::string randomBytes(usize count);
/// PBKDF2-HMAC-SHA-256 key of `length` bytes from a password and a salt.
std::string deriveKey(std::string_view password, std::string_view salt, u32 iterations = 10000, usize length = 32);
/// AES-256-GCM with a random nonce: nonce (12) + ciphertext + tag (16). The key is hashed to 32
/// bytes when it has another length.
std::string encrypt(std::string_view key, std::string_view plaintext);
/// The plaintext, or nothing if the data was changed or the key is wrong.
std::optional<std::string> decrypt(std::string_view key, std::string_view box);

} // namespace agr::net::cryptobox
