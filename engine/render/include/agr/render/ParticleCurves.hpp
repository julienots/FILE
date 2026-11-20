#pragma once
#include <array>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "agr/core/FileSystem.hpp"
#include "agr/core/Result.hpp"
#include "agr/math/Math.hpp"

namespace agr {

/// Over-lifetime look of particles (.agrparticle), shared by any number of 2D / 3D emitters
/// (ParticleEmitter*.curves): a size curve and a color / alpha gradient over the particle's age
/// (0 = birth, 1 = death). They replace the emitter's start / end interpolation:
///   size  = startSize x size(t)        (endSize is ignored when the size curve has keys)
///   color = gradient(t)                (startColor / endColor are ignored when it has keys)
struct ParticleCurves {
    struct Key {
        float t = 0.0f;
        float value = 1.0f;
    };
    struct ColorKey {
        float t = 0.0f;
        math::Color color{1.0f, 1.0f, 1.0f, 1.0f};
    };
    std::vector<Key> size;          ///< Sorted by t; linear between keys, held outside.
    std::vector<ColorKey> gradient;  ///< Sorted by t; linear (in linear space as given).

    float sizeAt(float t) const;
    math::Color colorAt(float t) const;
    void sort();

    std::string toJson() const;
    static Result<ParticleCurves> fromJson(std::string_view json);
};

/// Baked lookup table of a ParticleCurves (64 samples), what the renderer reads per particle.
struct ParticleCurvesLut {
    static constexpr int kSamples = 64;
    bool hasSize = false, hasColor = false;
    std::array<float, kSamples> size{};
    std::array<math::Color, kSamples> color{};
    float sizeAt(float t) const;
    math::Color colorAt(float t) const;
};

/// .agrparticle files through the VFS, cached and baked, reloaded when they change.
class ParticleCurvesLibrary {
public:
    explicit ParticleCurvesLibrary(VirtualFileSystem& vfs) : m_vfs(vfs) {}
    /// nullptr when missing or invalid (logged once).
    const ParticleCurvesLut* get(const std::string& uri);
    const ParticleCurves* curves(const std::string& uri);
    Status save(const std::string& uri, const ParticleCurves& curves);
    /// Replaces the cached curves without writing (live editing).
    void preview(const std::string& uri, const ParticleCurves& curves);
    u32 reloadChanged();

private:
    struct Entry {
        std::unique_ptr<ParticleCurves> curves;
        std::unique_ptr<ParticleCurvesLut> lut;
        i64 modifiedTime = 0;
    };
    Entry* entry(const std::string& uri);
    VirtualFileSystem& m_vfs;
    std::unordered_map<std::string, Entry> m_entries;
};

ParticleCurvesLut bakeParticleCurves(const ParticleCurves& curves);

} // namespace agr
