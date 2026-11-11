#include "agr/render/EnvironmentLibrary.hpp"

#include "agr/core/Log.hpp"
#include "agr/math/Half.hpp"

namespace agr {

EnvironmentLibrary::EnvironmentLibrary(VirtualFileSystem& vfs, JobSystem& jobs, rhi::Device* device)
    : m_vfs(vfs), m_jobs(jobs), m_device(device) {}

EnvironmentLibrary::~EnvironmentLibrary() { clear(); }

const EnvironmentAsset* EnvironmentLibrary::get(const std::string& uri) {
    if (uri.empty()) return nullptr;
    if (auto it = m_assets.find(uri); it != m_assets.end()) return it->second.get();
    auto asset = std::make_unique<EnvironmentAsset>();
    asset->uri = uri;
    EnvironmentAsset* raw = asset.get();
    m_assets.emplace(uri, std::move(asset));
    start(*raw);
    return raw;
}

void EnvironmentLibrary::start(EnvironmentAsset& asset) {
    asset.state = AssetState::Loading;
    const u32 version = ++asset.version;
    const std::string uri = asset.uri;
    const u32 maxWidth = m_maxWidth;
    m_jobs.submit(
        [this, uri, version, maxWidth] {
            Pending p;
            p.uri = uri;
            p.version = version;
            p.modifiedTime = m_vfs.modifiedTime(uri);
            auto bytes = m_vfs.readBytes(uri);
            if (!bytes) {
                p.error = bytes.error().message;
            } else if (auto img = decodeImageLinear(bytes.value(), uri); !img) {
                p.error = img.error().message;
            } else if (auto env = buildEnvironmentMap(img.value(), maxWidth); !env) {
                p.error = env.error().message;
            } else {
                p.map = std::move(env.value());
                // RGBA16F for the GPU (half floats keep the HDR range).
                for (const auto& level : p.map.levels) {
                    std::vector<u16> half(level.size() / 3 * 4);
                    for (usize i = 0, j = 0; i < level.size(); i += 3, j += 4) {
                        half[j] = math::floatToHalf(std::min(level[i], 65000.0f));
                        half[j + 1] = math::floatToHalf(std::min(level[i + 1], 65000.0f));
                        half[j + 2] = math::floatToHalf(std::min(level[i + 2], 65000.0f));
                        half[j + 3] = 0x3c00;  // 1.0
                    }
                    p.halfLevels.push_back(std::move(half));
                }
                for (auto& level : p.map.levels) std::vector<float>().swap(level);  // CPU copy no longer needed
            }
            std::lock_guard lock(m_mutex);
            m_done.push_back(std::move(p));
        },
        &m_inFlight);
}

void EnvironmentLibrary::finish(Pending& p) {
    auto it = m_assets.find(p.uri);
    if (it == m_assets.end() || it->second->version != p.version) return;  // superseded
    EnvironmentAsset& a = *it->second;
    a.modifiedTime = p.modifiedTime;
    if (!p.error.empty()) {
        a.state = AssetState::Failed;
        a.error = p.error;
        AGR_LOG_ERROR("Assets", "environment '{}': {}", p.uri, p.error);
        return;
    }
    rhi::TextureHandle texture;
    if (m_device) {
        rhi::TextureDesc d;
        d.width = p.map.width;
        d.height = p.map.height;
        d.format = rhi::TextureFormat::RGBA16F;
        d.sampler = {rhi::Filter::Linear, rhi::Filter::Linear, true, rhi::Wrap::Repeat, rhi::Wrap::Clamp};
        d.data = p.halfLevels.front().data();
        for (usize i = 1; i < p.halfLevels.size(); ++i) d.mipData.push_back(p.halfLevels[i].data());
        d.debugName = "agr.environment";
        auto t = m_device->createTexture(d);
        if (!t) {
            a.state = AssetState::Failed;
            a.error = t.error().message;
            AGR_LOG_ERROR("Assets", "environment '{}': {}", p.uri, a.error);
            return;
        }
        texture = t.value();
    }
    if (m_device && a.texture) m_device->destroyTexture(a.texture);
    a.texture = texture;
    a.width = p.map.width;
    a.height = p.map.height;
    a.levels = static_cast<u32>(p.halfLevels.size());
    a.sh = p.map.sh;
    a.average = p.map.average;
    a.error.clear();
    a.state = AssetState::Ready;
}

void EnvironmentLibrary::update() {
    std::vector<Pending> done;
    {
        std::lock_guard lock(m_mutex);
        done.swap(m_done);
    }
    for (Pending& p : done) finish(p);
}

void EnvironmentLibrary::waitAll() { m_jobs.wait(m_inFlight); }

u32 EnvironmentLibrary::reloadChanged() {
    u32 n = 0;
    for (auto& [uri, a] : m_assets) {
        if (a->state == AssetState::Loading) continue;
        const i64 t = m_vfs.modifiedTime(uri);
        if (t != 0 && t != a->modifiedTime) {
            AGR_LOG_INFO("Assets", "hot reload '{}'", uri);
            start(*a);
            ++n;
        }
    }
    return n;
}

void EnvironmentLibrary::clear() {
    waitAll();
    update();
    if (m_device)
        for (auto& [uri, a] : m_assets)
            if (a->texture) m_device->destroyTexture(a->texture);
    m_assets.clear();
}

} // namespace agr
