#pragma once
#include <array>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

#include "agr/assets/AssetManager.hpp"
#include "agr/assets/Environment.hpp"
#include "agr/core/JobSystem.hpp"
#include "agr/rhi/Rhi.hpp"

namespace agr {

/// An equirectangular sky image ready for rendering: prefiltered RGBA16F mip chain on the
/// GPU (sky background and specular reflections) and SH9 irradiance (diffuse light).
struct EnvironmentAsset {
    std::string uri;
    AssetState state = AssetState::Loading;
    std::string error;
    rhi::TextureHandle texture;  ///< Null without a device (headless).
    u32 width = 0, height = 0;
    u32 levels = 0;
    std::array<math::Vec3, 9> sh{};
    math::Vec3 average{0.0f};
    u32 version = 0;
    i64 modifiedTime = 0;
};

/// Loads sky images (.hdr, .png, .jpg...) on worker threads: decoding, resampling,
/// prefiltering and SH projection happen off the main thread; update() uploads.
class EnvironmentLibrary {
public:
    EnvironmentLibrary(VirtualFileSystem& vfs, JobSystem& jobs, rhi::Device* device);
    ~EnvironmentLibrary();
    EnvironmentLibrary(const EnvironmentLibrary&) = delete;
    EnvironmentLibrary& operator=(const EnvironmentLibrary&) = delete;

    /// Starts loading on first use; check `state`. Null for an empty URI.
    const EnvironmentAsset* get(const std::string& uri);
    /// Width cap of level 0 for images loaded from now on (power of two, 2048 by default).
    void setMaxWidth(u32 width) { m_maxWidth = width; }
    /// Main thread: uploads finished loads.
    void update();
    void waitAll();
    /// Reloads images modified on disk; returns how many.
    u32 reloadChanged();
    void clear();
    usize size() const { return m_assets.size(); }

private:
    struct Pending {
        std::string uri;
        u32 version = 0;
        i64 modifiedTime = 0;
        std::string error;
        EnvironmentMap map;
        std::vector<std::vector<u16>> halfLevels;
    };
    void start(EnvironmentAsset& asset);
    void finish(Pending& p);

    VirtualFileSystem& m_vfs;
    JobSystem& m_jobs;
    rhi::Device* m_device;
    u32 m_maxWidth = 2048;
    std::unordered_map<std::string, std::unique_ptr<EnvironmentAsset>> m_assets;
    std::mutex m_mutex;
    std::vector<Pending> m_done;
    JobCounter m_inFlight;
};

} // namespace agr
