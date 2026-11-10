#pragma once
#include "agr/core/FileSystem.hpp"

namespace agr {

/// Encodes interleaved float samples (-1..1, clamped) as a 16-bit PCM WAV file.
/// Used by tools that generate sounds and by tests.
ByteBuffer encodeWav16(const float* samples, u32 frames, u32 channels, u32 sampleRate);

} // namespace agr
