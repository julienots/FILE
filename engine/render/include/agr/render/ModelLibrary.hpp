#pragma once
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

#include "agr/assets/AssetManager.hpp"
#include "agr/assets/Material.hpp"
#include "agr/assets/Model.hpp"
#include "agr/core/JobSystem.hpp"
#include "agr/rhi/Rhi.hpp"
#include "agr/scene/Scene.hpp"

namespace agr {

/// A model ready to draw: GPU buffers per mesh, plus the CPU data editors and
/// gameplay need (hierarchy, materials, bounds).
struct ModelAsset {
    struct Mesh {
        std::string name;
        rhi::BufferHandle vertices, indices;  ///< Null without a device (headless).
        u32 indexCount = 0;
        u32 vertexCount = 0;
        std::vector<ModelSubmesh> submeshes;
        math::Aabb bounds;
    };
    std::string uri;
    AssetState state = AssetState::Loading;
    std::string error;
    std::vector<Mesh> meshes;
    std::vector<ModelNode> nodes;
    std::vector<MaterialData> materials;
    std::vector<std::string> warnings;
    u32 version = 0;  ///< Incremented on every (re)load.
    i64 modifiedTime = 0;
};

/// Loads models asynchronously (import on a worker thread, GPU upload on the main
/// thread in update()), caches them by URI and reloads them when the file changes.
class ModelLibrary {
public:
    ModelLibrary(VirtualFileSystem& vfs, JobSystem& jobs, rhi::Device* device);
    ~ModelLibrary();
    ModelLibrary(const ModelLibrary&) = delete;
    ModelLibrary& operator=(const ModelLibrary&) = delete;

    /// Entry for a URI, starting an asynchronous load on the first request.
    /// The pointer stays valid until clear() (reloads update it in place).
    const ModelAsset* get(const std::string& uri);
    /// Same, but imports and uploads now (editor instantiation, loading screens).
    const ModelAsset* load(const std::string& uri);
    /// Main thread, once per frame: uploads finished imports.
    void update();
    void waitAll();
    /// Reloads models whose file changed. Returns the number reloaded.
    u32 reloadChanged();
    void clear();
    usize size() const { return m_models.size(); }

private:
    struct Pending {
        std::string uri;
        u32 version;
        Result<ModelData> data;
        i64 modifiedTime;
    };
    void start(ModelAsset& asset, bool blocking);
    void finish(Pending& p);
    void releaseGpu(ModelAsset& asset);

    VirtualFileSystem& m_vfs;
    JobSystem& m_jobs;
    rhi::Device* m_device;
    std::unordered_map<std::string, std::unique_ptr<ModelAsset>> m_models;
    std::mutex m_mutex;
    std::vector<Pending> m_done;
    JobCounter m_inFlight;
};

/// `.agrmat` files, cached by URI and reloaded when they change.
class MaterialLibrary {
public:
    explicit MaterialLibrary(VirtualFileSystem& vfs) : m_vfs(vfs) {}
    /// nullptr when the file is missing or invalid (the error is logged once).
    const MaterialData* get(const std::string& uri);
    /// Writes a material file and refreshes the cache (editor).
    Status save(const std::string& uri, const MaterialData& material);
    /// Replaces the cached material without writing the file (live editing preview).
    void preview(const std::string& uri, const MaterialData& material);
    void invalidate(const std::string& uri) { m_entries.erase(uri); }
    u32 reloadChanged();
    void clear() { m_entries.clear(); }

private:
    struct Entry {
        std::unique_ptr<MaterialData> data;  // stable address while cached
        i64 modifiedTime = 0;
    };
    VirtualFileSystem& m_vfs;
    std::unordered_map<std::string, Entry> m_entries;
};

/// Creates the model's node hierarchy under `parent`: a root entity named `name`
/// (or after the file) with one child entity per node; nodes with a mesh get a
/// MeshRenderer (model = uri, meshIndex). Returns the root (null if not loaded).
ecs::Entity instantiateModel(Scene& scene, const ModelAsset& model, ecs::Entity parent = {}, std::string name = {});

} // namespace agr
