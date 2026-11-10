#include "agr/render/ModelLibrary.hpp"

#include <filesystem>

#include "agr/core/Log.hpp"
#include "agr/render/RenderComponents.hpp"

namespace agr {

ModelLibrary::ModelLibrary(VirtualFileSystem& vfs, JobSystem& jobs, rhi::Device* device) : m_vfs(vfs), m_jobs(jobs), m_device(device) {}

ModelLibrary::~ModelLibrary() {
    waitAll();
    for (auto& [uri, m] : m_models) releaseGpu(*m);
}

void ModelLibrary::releaseGpu(ModelAsset& asset) {
    if (!m_device) return;
    for (auto& mesh : asset.meshes) {
        m_device->destroyBuffer(mesh.vertices);
        m_device->destroyBuffer(mesh.indices);
    }
}

const ModelAsset* ModelLibrary::get(const std::string& uri) {
    if (uri.empty()) return nullptr;
    auto it = m_models.find(uri);
    if (it != m_models.end()) return it->second.get();
    auto asset = std::make_unique<ModelAsset>();
    asset->uri = uri;
    ModelAsset* raw = asset.get();
    m_models.emplace(uri, std::move(asset));
    start(*raw, false);
    return raw;
}

const ModelAsset* ModelLibrary::load(const std::string& uri) {
    if (uri.empty()) return nullptr;
    auto it = m_models.find(uri);
    if (it == m_models.end()) {
        auto asset = std::make_unique<ModelAsset>();
        asset->uri = uri;
        ModelAsset* raw = asset.get();
        m_models.emplace(uri, std::move(asset));
        start(*raw, true);
        return raw;
    }
    if (it->second->state == AssetState::Loading) {
        waitAll();
        update();
    }
    return it->second.get();
}

void ModelLibrary::start(ModelAsset& asset, bool blocking) {
    asset.state = AssetState::Loading;
    const u32 version = ++asset.version;
    const std::string uri = asset.uri;
    auto work = [this, uri, version] {
        Pending p{uri, version, makeError(Error::Code::Unknown, "not loaded"), m_vfs.modifiedTime(uri)};
        p.data = importModel(m_vfs, uri);
        std::lock_guard lock(m_mutex);
        m_done.push_back(std::move(p));
    };
    if (blocking) {
        work();
        update();
    } else {
        m_jobs.submit(work, &m_inFlight);
    }
}

void ModelLibrary::finish(Pending& p) {
    auto it = m_models.find(p.uri);
    if (it == m_models.end() || it->second->version != p.version) return;  // superseded by a reload
    ModelAsset& a = *it->second;
    a.modifiedTime = p.modifiedTime;
    if (!p.data) {
        a.state = AssetState::Failed;
        a.error = p.data.error().message;
        AGR_LOG_ERROR("Assets", "model '{}': {}", a.uri, a.error);
        return;
    }
    ModelData& data = p.data.value();
    std::vector<ModelAsset::Mesh> meshes;
    for (ModelMesh& src : data.meshes) {
        ModelAsset::Mesh m;
        m.name = src.name;
        m.indexCount = static_cast<u32>(src.indices.size());
        m.vertexCount = static_cast<u32>(src.vertices.size());
        m.submeshes = src.submeshes;
        m.bounds = src.bounds;
        m.skinned = src.skinWeights.size() == src.vertices.size() && !src.vertices.empty();
        if (m_device && !src.vertices.empty()) {
            // Skinned meshes: interleaved vertex + joints / weights (80 bytes).
            std::vector<SkinnedModelVertex> skinned;
            if (m.skinned) {
                skinned.resize(src.vertices.size());
                for (usize i = 0; i < skinned.size(); ++i) skinned[i] = {src.vertices[i], src.skinWeights[i]};
            }
            auto vb = m.skinned ? m_device->createBuffer({rhi::BufferUsage::Vertex, rhi::BufferMemory::Static, skinned.size() * sizeof(SkinnedModelVertex),
                                                          skinned.data(), "agr.model.skinned.vb"})
                                : m_device->createBuffer({rhi::BufferUsage::Vertex, rhi::BufferMemory::Static, src.vertices.size() * sizeof(ModelVertex),
                                                          src.vertices.data(), "agr.model.vb"});
            auto ib = m_device->createBuffer({rhi::BufferUsage::Index, rhi::BufferMemory::Static, src.indices.size() * sizeof(u32),
                                              src.indices.data(), "agr.model.ib"});
            if (!vb || !ib) {
                if (vb) m_device->destroyBuffer(vb.value());
                if (ib) m_device->destroyBuffer(ib.value());
                for (auto& done : meshes) {
                    m_device->destroyBuffer(done.vertices);
                    m_device->destroyBuffer(done.indices);
                }
                a.state = AssetState::Failed;
                a.error = "GPU upload failed";
                AGR_LOG_ERROR("Assets", "model '{}': GPU upload failed", a.uri);
                return;
            }
            m.vertices = vb.value();
            m.indices = ib.value();
        }
        meshes.push_back(std::move(m));
    }
    releaseGpu(a);  // previous version (reload)
    a.meshes = std::move(meshes);
    a.nodes = std::move(data.nodes);
    a.materials = std::move(data.materials);
    a.skins = std::move(data.skins);
    a.animationNames.clear();
    for (const ModelAnimation& an : data.animations) a.animationNames.push_back(an.name);
    a.warnings = std::move(data.warnings);
    a.error.clear();
    a.state = AssetState::Ready;
    for (const std::string& w : a.warnings) AGR_LOG_WARN("Assets", "model '{}': {}", a.uri, w);
    AGR_LOG_INFO("Assets", "model '{}' loaded: {} mesh(es), {} vertices, {} triangles", a.uri, a.meshes.size(), data.vertexCount(),
                 data.triangleCount());
}

void ModelLibrary::update() {
    std::vector<Pending> done;
    {
        std::lock_guard lock(m_mutex);
        done.swap(m_done);
    }
    for (Pending& p : done) finish(p);
}

void ModelLibrary::waitAll() {
    m_jobs.wait(m_inFlight);
}

u32 ModelLibrary::reloadChanged() {
    u32 n = 0;
    for (auto& [uri, m] : m_models) {
        if (m->state == AssetState::Loading) continue;
        const i64 t = m_vfs.modifiedTime(uri);
        if (t != 0 && t != m->modifiedTime) {
            AGR_LOG_INFO("Assets", "hot reload '{}'", uri);
            start(*m, false);
            ++n;
        }
    }
    return n;
}

void ModelLibrary::clear() {
    waitAll();
    update();
    for (auto& [uri, m] : m_models) releaseGpu(*m);
    m_models.clear();
}

// ------------------------------------------------------------------ materials
const MaterialData* MaterialLibrary::get(const std::string& uri) {
    if (uri.empty()) return nullptr;
    if (auto it = m_entries.find(uri); it != m_entries.end()) return it->second.data.get();
    Entry e;
    e.modifiedTime = m_vfs.modifiedTime(uri);
    auto text = m_vfs.readText(uri);
    if (!text) {
        AGR_LOG_ERROR("Assets", "material '{}': {}", uri, text.error().message);
    } else if (auto m = parseMaterial(text.value()); !m) {
        AGR_LOG_ERROR("Assets", "material '{}': {}", uri, m.error().message);
    } else {
        e.data = std::make_unique<MaterialData>(std::move(m.value()));
    }
    return m_entries.emplace(uri, std::move(e)).first->second.data.get();
}

Status MaterialLibrary::save(const std::string& uri, const MaterialData& material) {
    if (Status s = m_vfs.writeText(uri, materialToJson(material)); !s) return s;
    Entry e;
    e.data = std::make_unique<MaterialData>(material);
    e.modifiedTime = m_vfs.modifiedTime(uri);
    if (auto it = m_entries.find(uri); it != m_entries.end() && it->second.data) {
        *it->second.data = material;  // keep the address: renderers may hold it this frame
        it->second.modifiedTime = e.modifiedTime;
    } else {
        m_entries[uri] = std::move(e);
    }
    return {};
}

void MaterialLibrary::preview(const std::string& uri, const MaterialData& material) {
    Entry& e = m_entries[uri];
    if (e.data) *e.data = material;
    else e.data = std::make_unique<MaterialData>(material);
}

u32 MaterialLibrary::reloadChanged() {
    std::vector<std::string> changed;
    for (const auto& [uri, e] : m_entries) {
        const i64 t = m_vfs.modifiedTime(uri);
        if (t != 0 && t != e.modifiedTime) changed.push_back(uri);
    }
    for (const std::string& uri : changed) {
        auto text = m_vfs.readText(uri);
        auto m = text ? parseMaterial(text.value()) : Result<MaterialData>(text.error());
        Entry& e = m_entries[uri];
        e.modifiedTime = m_vfs.modifiedTime(uri);
        if (!m) {
            AGR_LOG_ERROR("Assets", "material '{}': {}", uri, m.error().message);
            continue;  // keep the last valid version
        }
        if (e.data) *e.data = std::move(m.value());
        else e.data = std::make_unique<MaterialData>(std::move(m.value()));
        AGR_LOG_INFO("Assets", "hot reload '{}'", uri);
    }
    return static_cast<u32>(changed.size());
}

// --------------------------------------------------------------- instantiate
ecs::Entity instantiateModel(Scene& scene, const ModelAsset& model, ecs::Entity parent, std::string name) {
    if (model.state != AssetState::Ready) return {};
    registerRenderComponents();
    if (name.empty()) {
        const std::string_view uri = model.uri;
        const auto slash = uri.find_last_of('/');
        name = std::filesystem::path(std::string(uri.substr(slash == std::string_view::npos ? 0 : slash + 1))).stem().string();
    }
    const ecs::Entity root = scene.createEntity(name.empty() ? "Model" : name, parent);
    std::vector<ecs::Entity> created(model.nodes.size());
    std::vector<std::string> meshNames;
    for (const auto& m : model.meshes) meshNames.push_back(m.name);
    for (usize i = 0; i < model.nodes.size(); ++i) {
        const ModelNode& n = model.nodes[i];
        const ecs::Entity p = n.parent >= 0 ? created[static_cast<usize>(n.parent)] : root;
        // Same naming as animation tracks and skin joints (modelNodeName).
        const ecs::Entity e = scene.createEntity(modelNodeName(model.nodes, meshNames, i), p);
        auto& t = scene.registry().get<TransformComponent>(e);
        t.position = n.position;
        t.rotation = n.rotation;
        t.scale = n.scale;
        if (n.mesh >= 0) {
            auto& mr = scene.registry().emplace<MeshRendererComponent>(e);
            mr.model = model.uri;
            mr.meshIndex = n.mesh;
            mr.skin = n.skin;
        }
        created[i] = e;
    }
    scene.updateTransforms();
    return root;
}

} // namespace agr
