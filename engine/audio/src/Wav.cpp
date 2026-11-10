#include "agr/audio/Wav.hpp"

#include <algorithm>
#include <cmath>

namespace agr {

namespace {
void put16(ByteBuffer& b, u32 v) {
    b.push_back(static_cast<u8>(v & 0xFF));
    b.push_back(static_cast<u8>((v >> 8) & 0xFF));
}
void put32(ByteBuffer& b, u32 v) {
    put16(b, v & 0xFFFF);
    put16(b, v >> 16);
}
void tag(ByteBuffer& b, const char* t) { b.insert(b.end(), t, t + 4); }
} // namespace

ByteBuffer encodeWav16(const float* samples, u32 frames, u32 channels, u32 sampleRate) {
    const u32 dataBytes = frames * channels * 2;
    ByteBuffer b;
    b.reserve(44 + dataBytes);
    tag(b, "RIFF");
    put32(b, 36 + dataBytes);
    tag(b, "WAVE");
    tag(b, "fmt ");
    put32(b, 16);
    put16(b, 1);  // PCM
    put16(b, channels);
    put32(b, sampleRate);
    put32(b, sampleRate * channels * 2);
    put16(b, channels * 2);
    put16(b, 16);
    tag(b, "data");
    put32(b, dataBytes);
    for (u32 i = 0; i < frames * channels; ++i) {
        const float s = std::clamp(samples[i], -1.0f, 1.0f);
        const i32 v = static_cast<i32>(std::lround(s * 32767.0f));
        put16(b, static_cast<u32>(static_cast<u16>(static_cast<i16>(v))));
    }
    return b;
}

} // namespace agr
