#pragma once
// Small, fast LZ77 compression (LZ4-style sequences) for network packets and save data. The
// decoder checks every length and offset: it is safe on untrusted input (network).
#include <optional>
#include <span>
#include <vector>

#include "agr/core/Types.hpp"

namespace agr::lz {

/// Compressed bytes (may be larger than the input for random data: compare before using).
std::vector<u8> compress(std::span<const u8> input);
/// The original bytes, or nothing if the data is corrupt or would exceed `maxSize`.
std::optional<std::vector<u8>> decompress(std::span<const u8> input, usize maxSize);

} // namespace agr::lz
