#pragma once
#include <atomic>
#include <memory>
#include <mutex>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "agr/assets/Image.hpp"
#include "agr/core/FileSystem.hpp"
#include "agr/core/Handle.hpp"
#include "agr/core/JobSystem.hpp"
#include "agr/rhi/Rhi.hpp"

namespace agr {

struct TextureAssetTag;
using TextureAsset = Handle<TextureAssetTag>;

enum class AssetState : u8 { Loading, Ready, Failed };
enum class LoadMode : u8 { Async, Blocking };

struct TextureLoadOptions {
    bool srgb = true;           ///< Color data. False for normal maps, masks, data textures.
    bool mipmaps = true;
    bool pixelArt = false;      ///< Nearest filtering, no mipmaps.
    rhi::Wrap wrap = rhi::Wrap::Repeat;
};

struct TextureInfo {
    std::string uri;
    AssetState state = AssetState::Loading;
    u32 width = 0, height = 0;        ///< Size on the GPU (after profile downscaling).
    u32 sourceWidth = 0, sourceHeight = 0;
    rhi::TextureHandle gpu;
    std::string error;
    u32 refCount = 0;
    u32 version = 0;                  ///< Incremented on every (re)load.
    TextureLoadOptions options;
    i64 modifiedTime = 0;
};

struct AssetStats {
    u32 textures = 0;
    u32 loading = 0;
    u32 failed = 0;
    u64 gpuBytes = 0;
    u64 uploadedThisFrame = 0;
};

/// Loads, caches and owns textures (more asset types arrive with later phases).
///
///  * Same URI + options -> same asset (cache); every load*() call adds a reference.
///  * Async loads read and decode on JobSystem workers; GPU uploads happen in
///    update() on the main thread, limited by a per-frame byte budget to avoid hitches.
///  * While an asset loads, gpuTexture() returns a white texture; when it fails,
///    a magenta / black checkerboard so the problem is visible in the game.
///  * Without a device (headless servers, tests) images are decoded and kept on the CPU.
class AssetManager {
public:
    AssetManager(VirtualFileSystem& vfs, JobSystem& jobs, rhi::Device* device);
    ~AssetManager();
    AssetManager(const AssetManager&) = delete;
    AssetManager& operator=(const AssetManager&) = delete;

    TextureAsset loadTexture(std::string_view uri, const TextureLoadOptions& options = {}, LoadMode mode = LoadMode::Async);
    void retain(TextureAsset asset);
    /// Drops a reference; the texture is destroyed in update() when none remain.
    void release(TextureAsset asset);

    const TextureInfo* texture(TextureAsset asset) const;
    /// GPU texture to bind now (fallbacks while loading / on failure).
    rhi::TextureHandle gpuTexture(TextureAsset asset) const;
    /// CPU copy of the pixels (only kept when there is no device).
    const Image* cpuImage(TextureAsset asset) const;
    rhi::TextureHandle whiteTexture() const { return m_white; }
    rhi::TextureHandle errorTexture() const { return m_error; }

    /// Main thread, once per frame: uploads finished loads, destroys unreferenced assets.
    void update();
    /// Blocks until every pending load is decoded and uploaded (loading screens, tests).
    void waitAll();

    /// Reloads from disk (keeps the handle; the GPU texture is replaced).
    void reload(TextureAsset asset);
    /// Reloads every asset whose file changed on disk. Returns the number reloaded.
    u32 reloadChanged();

    void setUploadBudget(u64 bytesPerFrame) { m_uploadBudget = bytesPerFrame; }
    void setMaxTextureSize(u32 size) { m_maxTextureSize = size; }
    u32 maxTextureSize() const { return m_maxTextureSize; }
    AssetStats stats() const;

private:
    struct Entry {
        TextureInfo info;
        Image cpu;  // kept only when headless
        bool pendingDestroy = false;
    };
    struct Decoded {
        TextureAsset asset;
        u32 version;
        Result<Image> image;
        i64 modifiedTime;
    };

    std::string cacheKey(std::string_view uri, const TextureLoadOptions& o) const;
    void startLoad(TextureAsset asset, LoadMode mode);
    void finish(Decoded& d);
    void createFallbacks();

    VirtualFileSystem& m_vfs;
    JobSystem& m_jobs;
    rhi::Device* m_device;
    HandlePool<Entry, TextureAssetTag> m_entries;
    std::unordered_map<std::string, TextureAsset> m_cache;

    std::mutex m_doneMutex;
    std::vector<Decoded> m_done;
    JobCounter m_inFlight;

    rhi::TextureHandle m_white, m_error;
    u64 m_uploadBudget = 8ull * 1024 * 1024;
    u64 m_uploadedThisFrame = 0;
    u32 m_maxTextureSize = 4096;
};

} // namespace agr
