// AGR Engine — procedural texture authoring (generated at start-up, no external files).
#pragma once
#include <vector>
#include <cstdint>

namespace agr {

struct ImageRGBA {
    int w = 0, h = 0;
    std::vector<uint8_t> px;
    void alloc(int W, int H) { w = W; h = H; px.assign((size_t)W * H * 4, 0); }
    uint8_t* at(int x, int y) { return &px[((size_t)y * w + x) * 4]; }
};

namespace texgen {
// Tileable gradient noise with integer period.
float tileNoise(float x, float y, int period, uint32_t seed);
float tileFbm(float x, float y, int period, int octaves, uint32_t seed);
float tileWorley(float x, float y, int period, uint32_t seed, float* secondDist = nullptr, uint32_t* cellId = nullptr);

ImageRGBA noiseTexture(int size);
// 8 terrain layers: albedo (rgb) + height (a), and tangent space normals.
void terrainLayers(int size, std::vector<std::vector<uint8_t>>& albedo, std::vector<std::vector<uint8_t>>& normals);
ImageRGBA particleAtlas(int cell);  // 4x4 grid of particle shapes
}  // namespace texgen

}  // namespace agr
