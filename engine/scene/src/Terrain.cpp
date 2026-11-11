#include "agr/scene/Terrain.hpp"

#include <algorithm>
#include <bit>
#include <cmath>

#include "agr/scene/Reflection.hpp"

namespace agr {

using math::Vec2;
using math::Vec3;

void registerTerrainComponents() {
    auto& reg = ComponentRegistry::instance();
    if (reg.find<TerrainComponent>()) return;
    reg.add<TerrainComponent>("Terrain")
        .category("Rendering")
        .description("Heightmap terrain painted with up to 4 texture layers; sculpt and paint it with the Terrain tools.")
        .field("size", &TerrainComponent::size, {.tooltip = "Width and depth (meters)", .rangeMin = 1.0f, .rangeMax = 10000.0f, .speed = 0.5f})
        .field("resolution", &TerrainComponent::resolution,
               {.tooltip = "Vertices per side (2 to 1025); changing it keeps the existing heights where they fit", .rangeMin = 2, .rangeMax = 1025, .speed = 1})
        .field("heights", &TerrainComponent::heights, {.flags = FieldFlag_Hidden})
        .field("splat", &TerrainComponent::splat, {.flags = FieldFlag_Hidden})
        .field("layer0", &TerrainComponent::layer0, {.tooltip = "Base layer texture", .flags = FieldFlag_AssetPath})
        .field("layer1", &TerrainComponent::layer1, {.flags = FieldFlag_AssetPath})
        .field("layer2", &TerrainComponent::layer2, {.flags = FieldFlag_AssetPath})
        .field("layer3", &TerrainComponent::layer3, {.flags = FieldFlag_AssetPath})
        .field("tiling", &TerrainComponent::tiling, {.tooltip = "Texture repeats across the terrain, per layer", .speed = 0.1f})
        .field("color", &TerrainComponent::color, {.flags = FieldFlag_Color})
        .field("roughness", &TerrainComponent::roughness, {.rangeMin = 0.0f, .rangeMax = 1.0f, .speed = 0.01f})
        .field("castShadows", &TerrainComponent::castShadows)
        .field("collision", &TerrainComponent::collision, {.tooltip = "Static collision (3D physics) and navigation mesh source"});
}

namespace terrain {

u32 resolution(const TerrainComponent& t) { return std::clamp<u32>(t.resolution, 2, 1025); }
float spacing(const TerrainComponent& t) { return std::max(t.size, 0.01f) / static_cast<float>(resolution(t) - 1); }

void ensureData(TerrainComponent& t) {
    const u32 n = resolution(t);
    t.resolution = n;
    const usize count = static_cast<usize>(n) * n;
    auto resize = [&](std::vector<i32>& v, i32 fill) {
        if (v.size() == count) return;
        if (v.empty()) {
            v.assign(count, fill);
            return;
        }
        // Resample the old grid (nearest) so a resolution change keeps the shape.
        const u32 old = static_cast<u32>(std::lround(std::sqrt(static_cast<double>(v.size()))));
        std::vector<i32> out(count, fill);
        if (old >= 2 && static_cast<usize>(old) * old == v.size())
            for (u32 z = 0; z < n; ++z)
                for (u32 x = 0; x < n; ++x) {
                    const u32 ox = static_cast<u32>(std::lround(static_cast<double>(x) * (old - 1) / (n - 1)));
                    const u32 oz = static_cast<u32>(std::lround(static_cast<double>(z) * (old - 1) / (n - 1)));
                    out[static_cast<usize>(z) * n + x] = v[static_cast<usize>(oz) * old + ox];
                }
        v = std::move(out);
    };
    resize(t.heights, 0);
    resize(t.splat, 0xFF);  // r = 255: layer 0
}

float sample(const TerrainComponent& t, i32 x, i32 z) {
    const i32 n = static_cast<i32>(resolution(t));
    if (t.heights.size() != static_cast<usize>(n) * static_cast<usize>(n)) return 0.0f;
    x = std::clamp(x, 0, n - 1);
    z = std::clamp(z, 0, n - 1);
    return static_cast<float>(t.heights[static_cast<usize>(z) * static_cast<usize>(n) + static_cast<usize>(x)]) * 0.001f;
}

Vec3 vertex(const TerrainComponent& t, i32 x, i32 z) {
    const float half = std::max(t.size, 0.01f) * 0.5f, s = spacing(t);
    return {-half + static_cast<float>(x) * s, sample(t, x, z), -half + static_cast<float>(z) * s};
}

float heightAt(const TerrainComponent& t, float x, float z) {
    const float half = std::max(t.size, 0.01f) * 0.5f, s = spacing(t);
    const float gx = std::clamp((x + half) / s, 0.0f, static_cast<float>(resolution(t) - 1));
    const float gz = std::clamp((z + half) / s, 0.0f, static_cast<float>(resolution(t) - 1));
    const i32 x0 = static_cast<i32>(std::floor(gx)), z0 = static_cast<i32>(std::floor(gz));
    const float fx = gx - static_cast<float>(x0), fz = gz - static_cast<float>(z0);
    const float a = sample(t, x0, z0), b = sample(t, x0 + 1, z0), c = sample(t, x0, z0 + 1), d = sample(t, x0 + 1, z0 + 1);
    return (a * (1.0f - fx) + b * fx) * (1.0f - fz) + (c * (1.0f - fx) + d * fx) * fz;
}

Vec3 normalAt(const TerrainComponent& t, float x, float z) {
    const float e = spacing(t);
    const float dx = heightAt(t, x + e, z) - heightAt(t, x - e, z), dz = heightAt(t, x, z + e) - heightAt(t, x, z - e);
    return math::normalize(Vec3{-dx, 2.0f * e, -dz});
}

void buildMesh(const TerrainComponent& t, std::vector<Vec3>& positions, std::vector<u32>& indices, std::vector<Vec3>* normals,
               std::vector<Vec2>* uvs) {
    const u32 n = resolution(t);
    positions.resize(static_cast<usize>(n) * n);
    if (normals) normals->resize(positions.size());
    if (uvs) uvs->resize(positions.size());
    const float e = spacing(t);
    for (u32 z = 0; z < n; ++z)
        for (u32 x = 0; x < n; ++x) {
            const usize i = static_cast<usize>(z) * n + x;
            positions[i] = vertex(t, static_cast<i32>(x), static_cast<i32>(z));
            if (normals) {
                const float dx = sample(t, static_cast<i32>(x) + 1, static_cast<i32>(z)) - sample(t, static_cast<i32>(x) - 1, static_cast<i32>(z));
                const float dz = sample(t, static_cast<i32>(x), static_cast<i32>(z) + 1) - sample(t, static_cast<i32>(x), static_cast<i32>(z) - 1);
                (*normals)[i] = math::normalize(Vec3{-dx, 2.0f * e, -dz});
            }
            if (uvs) (*uvs)[i] = {static_cast<float>(x) / static_cast<float>(n - 1), static_cast<float>(z) / static_cast<float>(n - 1)};
        }
    indices.clear();
    indices.reserve(static_cast<usize>(n - 1) * (n - 1) * 6);
    for (u32 z = 0; z + 1 < n; ++z)
        for (u32 x = 0; x + 1 < n; ++x) {
            const u32 a = z * n + x, b = a + 1, c = a + n, d = c + 1;
            // Counter-clockwise seen from +Y: a(x,z) -> c(x,z+1) -> b(x+1,z) ...
            indices.insert(indices.end(), {a, c, b, b, c, d});
        }
}

math::Aabb localBounds(const TerrainComponent& t) {
    const float half = std::max(t.size, 0.01f) * 0.5f;
    float lo = 0.0f, hi = 0.0f;
    if (!t.heights.empty()) {
        const auto [mn, mx] = std::minmax_element(t.heights.begin(), t.heights.end());
        lo = static_cast<float>(*mn) * 0.001f;
        hi = static_cast<float>(*mx) * 0.001f;
    }
    return {{-half, lo, -half}, {half, std::max(hi, lo + 0.01f), half}};
}

namespace {
u64 hashInts(u64 h, const std::vector<i32>& v) {
    for (i32 x : v) h = (h ^ static_cast<u32>(x)) * 0x100000001b3ull;
    return h;
}
} // namespace

u64 heightsHash(const TerrainComponent& t) {
    u64 h = 0xcbf29ce484222325ull ^ resolution(t);
    h = (h ^ std::bit_cast<u32>(t.size)) * 0x100000001b3ull;
    return hashInts(h, t.heights);
}

u64 splatHash(const TerrainComponent& t) { return hashInts(0xcbf29ce484222325ull ^ resolution(t), t.splat); }

bool raycast(const TerrainComponent& t, const math::Ray& ray, float maxDistance, float& distance) {
    const math::Aabb b = localBounds(t);
    // Clip the ray to the bounds, then march at half the grid spacing and refine by bisection.
    float t0 = 0.0f, t1 = maxDistance;
    for (int a = 0; a < 3; ++a) {
        const float o = (&ray.origin.x)[a], d = (&ray.direction.x)[a], lo = (&b.min.x)[a], hi = (&b.max.x)[a];
        if (std::fabs(d) < 1e-8f) {
            if (o < lo || o > hi) return false;
            continue;
        }
        float ta = (lo - o) / d, tb = (hi - o) / d;
        if (ta > tb) std::swap(ta, tb);
        t0 = std::max(t0, ta);
        t1 = std::min(t1, tb);
        if (t0 > t1) return false;
    }
    const float step = spacing(t) * 0.5f;
    auto above = [&](float s) {
        const Vec3 p = ray.at(s);
        return p.y - heightAt(t, p.x, p.z);
    };
    float prev = t0, prevH = above(t0);
    if (prevH < -1e-3f) return false;  // starts below the surface (tolerance: the bounds touch the highest point)
    for (float s = t0 + step; s <= t1 + step; s += step) {
        const float cur = std::min(s, t1);
        const float h = above(cur);
        if (h <= 0.0f) {
            float lo = prev, hi = cur;
            for (int i = 0; i < 20; ++i) {
                const float mid = 0.5f * (lo + hi);
                (above(mid) > 0.0f ? lo : hi) = mid;
            }
            distance = hi;
            return true;
        }
        prev = cur;
        prevH = h;
        if (cur >= t1) break;
    }
    return false;
}

bool applyBrush(TerrainComponent& t, Vec2 center, const BrushSettings& b) {
    ensureData(t);
    const i32 n = static_cast<i32>(resolution(t));
    const float s = spacing(t), half = t.size * 0.5f, radius = std::max(b.radius, s * 0.5f);
    const i32 x0 = std::max(0, static_cast<i32>(std::floor((center.x - radius + half) / s)));
    const i32 x1 = std::min(n - 1, static_cast<i32>(std::ceil((center.x + radius + half) / s)));
    const i32 z0 = std::max(0, static_cast<i32>(std::floor((center.y - radius + half) / s)));
    const i32 z1 = std::min(n - 1, static_cast<i32>(std::ceil((center.y + radius + half) / s)));
    if (x0 > x1 || z0 > z1) return false;
    const std::vector<i32> before = b.brush == Brush::Smooth ? t.heights : std::vector<i32>{};
    bool changed = false;
    for (i32 z = z0; z <= z1; ++z)
        for (i32 x = x0; x <= x1; ++x) {
            const Vec3 v = vertex(t, x, z);
            const float d = math::length(Vec2{v.x - center.x, v.z - center.y}) / radius;
            if (d >= 1.0f) continue;
            const float w = 0.5f + 0.5f * std::cos(d * 3.14159265f);  // smooth falloff, 1 at the center
            const usize i = static_cast<usize>(z) * static_cast<usize>(n) + static_cast<usize>(x);
            if (b.brush == Brush::Paint) {
                const u32 layer = std::min<u32>(b.layer, 3);
                float wts[4];
                for (u32 c = 0; c < 4; ++c) wts[c] = static_cast<float>((static_cast<u32>(t.splat[i]) >> (c * 8)) & 0xFF) / 255.0f;
                const float amount = std::clamp(b.strength, 0.0f, 1.0f) * w;
                for (u32 c = 0; c < 4; ++c) wts[c] = c == layer ? wts[c] + (1.0f - wts[c]) * amount : wts[c] * (1.0f - amount);
                const float sum = std::max(wts[0] + wts[1] + wts[2] + wts[3], 1e-4f);
                u32 packed = 0;
                for (u32 c = 0; c < 4; ++c) packed |= static_cast<u32>(std::lround(std::clamp(wts[c] / sum, 0.0f, 1.0f) * 255.0f)) << (c * 8);
                if (static_cast<i32>(packed) != t.splat[i]) {
                    t.splat[i] = static_cast<i32>(packed);
                    changed = true;
                }
                continue;
            }
            const float h = static_cast<float>(t.heights[i]) * 0.001f;
            float target = h;
            switch (b.brush) {
                case Brush::Raise: target = h + b.strength * w; break;
                case Brush::Lower: target = h - b.strength * w; break;
                case Brush::Flatten: target = h + (b.targetHeight - h) * std::clamp(b.strength, 0.0f, 1.0f) * w; break;
                case Brush::Smooth: {
                    float sum = 0.0f;
                    int count = 0;
                    for (i32 dz = -1; dz <= 1; ++dz)
                        for (i32 dx = -1; dx <= 1; ++dx) {
                            const i32 xx = std::clamp(x + dx, 0, n - 1), zz = std::clamp(z + dz, 0, n - 1);
                            sum += static_cast<float>(before[static_cast<usize>(zz) * static_cast<usize>(n) + static_cast<usize>(xx)]) * 0.001f;
                            ++count;
                        }
                    target = h + (sum / static_cast<float>(count) - h) * std::clamp(b.strength, 0.0f, 1.0f) * w;
                    break;
                }
                case Brush::Paint: break;
            }
            const i32 mm = static_cast<i32>(std::lround(std::clamp(target, -10000.0f, 10000.0f) * 1000.0f));
            if (mm != t.heights[i]) {
                t.heights[i] = mm;
                changed = true;
            }
        }
    return changed;
}

} // namespace terrain
} // namespace agr
