#include "agr/assets/AssetManager.hpp"
#include "agr/assets/Model.hpp"

#include <algorithm>
#include <format>

#include "agr/core/Log.hpp"

namespace agr {

namespace {
/// File part of an asset URI ("a.glb#image0" -> "a.glb").
std::string_view fileOf(std::string_view uri) { return uri.substr(0, uri.find('#')); }
} // namespace

AssetManager::AssetManager(VirtualFileSystem& vfs, JobSystem& jobs, rhi::Device* device)
    : m_vfs(vfs), m_jobs(jobs), m_device(device) {
    if (m_device) {
        m_maxTextureSize = std::min(m_maxTextureSize, m_device->info().maxTextureSize);
        createFallbacks();
    }
}

AssetManager::~AssetManager() {
    m_jobs.wait(m_inFlight);  // workers capture `this`
    if (m_device) {
        m_entries.forEach([&](TextureAsset, Entry& e) {
            if (e.info.gpu) m_device->destroyTexture(e.info.gpu);
        });
        m_device->destroyTexture(m_white);
        m_device->destroyTexture(m_error);
    }
}

void AssetManager::createFallbacks() {
    const u8 white[4] = {255, 255, 255, 255};
    rhi::TextureDesc d;
    d.data = white;
    d.generateMips = false;
    d.format = rhi::TextureFormat::RGBA8;
    d.debugName = "agr.white";
    if (auto t = m_device->createTexture(d)) m_white = t.value();
    // 8x8 magenta / black checker: impossible to miss on screen.
    u8 checker[8 * 8 * 4];
    for (int y = 0; y < 8; ++y)
        for (int x = 0; x < 8; ++x) {
            const bool on = ((x / 2) + (y / 2)) % 2 == 0;
            u8* p = checker + (y * 8 + x) * 4;
            p[0] = on ? 255 : 0, p[1] = 0, p[2] = on ? 255 : 0, p[3] = 255;
        }
    d.width = d.height = 8;
    d.data = checker;
    d.sampler = {rhi::Filter::Nearest, rhi::Filter::Nearest, false, rhi::Wrap::Repeat, rhi::Wrap::Repeat};
    d.debugName = "agr.error";
    if (auto t = m_device->createTexture(d)) m_error = t.value();
}

std::string AssetManager::cacheKey(std::string_view uri, const TextureLoadOptions& o) const {
    return std::format("{}|{}{}{}{}", uri, o.srgb ? 's' : 'l', o.mipmaps ? 'm' : '-', o.pixelArt ? 'p' : '-',
                       static_cast<int>(o.wrap));
}

TextureAsset AssetManager::loadTexture(std::string_view uri, const TextureLoadOptions& options, LoadMode mode) {
    const std::string key = cacheKey(uri, options);
    if (auto it = m_cache.find(key); it != m_cache.end()) {
        if (Entry* e = m_entries.get(it->second)) {
            ++e->info.refCount;
            e->pendingDestroy = false;
            if (mode == LoadMode::Blocking && e->info.state == AssetState::Loading) waitAll();
            return it->second;
        }
    }
    Entry entry;
    entry.info.uri = std::string(uri);
    entry.info.options = options;
    entry.info.refCount = 1;
    const TextureAsset h = m_entries.create(std::move(entry));
    m_cache[key] = h;
    startLoad(h, mode);
    return h;
}

void AssetManager::startLoad(TextureAsset h, LoadMode mode) {
    Entry* e = m_entries.get(h);
    if (!e) return;
    e->info.state = AssetState::Loading;
    const u32 version = ++e->info.version;
    const std::string uri = e->info.uri;
    auto work = [this, h, version, uri] {
        // "model.glb#image3": image embedded in a model file (the file's date drives hot reload).
        const bool embedded = uri.find("#image") != std::string::npos;
        Decoded d{h, version, makeError(Error::Code::Unknown, "not loaded"), m_vfs.modifiedTime(fileOf(uri))};
        auto bytes = embedded ? readEmbeddedImage(m_vfs, uri) : m_vfs.readBytes(uri);
        if (!bytes) d.image = bytes.error();
        else d.image = decodeImage(bytes.value(), uri);
        std::lock_guard lock(m_doneMutex);
        m_done.push_back(std::move(d));
    };
    if (mode == LoadMode::Blocking) {
        work();
        // Finish this asset immediately, regardless of the frame upload budget.
        std::vector<Decoded> done;
        {
            std::lock_guard lock(m_doneMutex);
            done.swap(m_done);
        }
        for (auto& d : done) finish(d);
    } else {
        m_jobs.submit(std::move(work), &m_inFlight);
    }
}

void AssetManager::finish(Decoded& d) {
    Entry* e = m_entries.get(d.asset);
    if (!e || d.version != e->info.version) return;  // released or superseded by a reload
    TextureInfo& info = e->info;
    info.modifiedTime = d.modifiedTime;
    if (!d.image) {
        info.state = AssetState::Failed;
        info.error = d.image.error().message;
        AGR_LOG_ERROR("Assets", "{}", info.error);
        return;
    }
    Image img = std::move(d.image.value());
    info.sourceWidth = img.width;
    info.sourceHeight = img.height;
    if (const u32 halvings = fitToMaxSize(img, m_maxTextureSize))
        AGR_LOG_DEBUG("Assets", "'{}' downscaled {}x for max texture size {}", info.uri, 1u << halvings, m_maxTextureSize);
    info.width = img.width;
    info.height = img.height;
    info.error.clear();
    if (!m_device) {
        e->cpu = std::move(img);
        info.state = AssetState::Ready;
        return;
    }
    rhi::TextureDesc td;
    td.width = img.width;
    td.height = img.height;
    td.format = info.options.srgb ? rhi::TextureFormat::RGBA8_sRGB : rhi::TextureFormat::RGBA8;
    td.generateMips = info.options.mipmaps && !info.options.pixelArt;
    td.data = img.pixels.data();
    const rhi::Filter f = info.options.pixelArt ? rhi::Filter::Nearest : rhi::Filter::Linear;
    td.sampler = {f, f, td.generateMips, info.options.wrap, info.options.wrap};
    td.debugName = info.uri.c_str();
    auto tex = m_device->createTexture(td);
    if (!tex) {
        info.state = AssetState::Failed;
        info.error = tex.error().message;
        AGR_LOG_ERROR("Assets", "'{}': {}", info.uri, info.error);
        return;
    }
    if (info.gpu) m_device->destroyTexture(info.gpu);  // hot reload: replace
    info.gpu = tex.value();
    info.state = AssetState::Ready;
    m_uploadedThisFrame += img.byteSize();
}

void AssetManager::update() {
    m_uploadedThisFrame = 0;
    std::vector<Decoded> done;
    {
        std::lock_guard lock(m_doneMutex);
        done.swap(m_done);
    }
    usize i = 0;
    for (; i < done.size(); ++i) {
        if (m_device && m_uploadedThisFrame >= m_uploadBudget) break;  // spread uploads over frames
        finish(done[i]);
    }
    if (i < done.size()) {
        std::lock_guard lock(m_doneMutex);
        m_done.insert(m_done.begin(), std::make_move_iterator(done.begin() + static_cast<std::ptrdiff_t>(i)),
                      std::make_move_iterator(done.end()));
    }

    // Garbage collection of unreferenced assets (not while a load is in flight).
    std::vector<TextureAsset> dead;
    m_entries.forEach([&](TextureAsset h, Entry& e) {
        if (e.pendingDestroy && e.info.refCount == 0 && e.info.state != AssetState::Loading) dead.push_back(h);
    });
    for (TextureAsset h : dead) {
        Entry* e = m_entries.get(h);
        if (m_device && e->info.gpu) m_device->destroyTexture(e->info.gpu);
        std::erase_if(m_cache, [h](const auto& kv) { return kv.second == h; });
        m_entries.release(h);
    }
}

void AssetManager::waitAll() {
    m_jobs.wait(m_inFlight);
    const u64 budget = m_uploadBudget;
    m_uploadBudget = ~0ull;
    update();
    m_uploadBudget = budget;
}

void AssetManager::retain(TextureAsset h) {
    if (Entry* e = m_entries.get(h)) {
        ++e->info.refCount;
        e->pendingDestroy = false;
    }
}

void AssetManager::release(TextureAsset h) {
    Entry* e = m_entries.get(h);
    if (!e || e->info.refCount == 0) return;
    if (--e->info.refCount == 0) e->pendingDestroy = true;
}

const TextureInfo* AssetManager::texture(TextureAsset h) const {
    const Entry* e = m_entries.get(h);
    return e ? &e->info : nullptr;
}

rhi::TextureHandle AssetManager::gpuTexture(TextureAsset h) const {
    const Entry* e = m_entries.get(h);
    if (!e) return m_error;
    if (e->info.gpu) return e->info.gpu;  // during a hot reload the old texture stays visible
    return e->info.state == AssetState::Failed ? m_error : m_white;
}

const Image* AssetManager::cpuImage(TextureAsset h) const {
    const Entry* e = m_entries.get(h);
    return e && !e->cpu.empty() ? &e->cpu : nullptr;
}

void AssetManager::reload(TextureAsset h) {
    if (m_entries.valid(h)) startLoad(h, LoadMode::Async);
}

u32 AssetManager::reloadChanged() {
    std::vector<TextureAsset> changed;
    m_entries.forEach([&](TextureAsset h, Entry& e) {
        if (e.info.state == AssetState::Loading) return;
        const i64 t = m_vfs.modifiedTime(fileOf(e.info.uri));
        if (t != 0 && t != e.info.modifiedTime) changed.push_back(h);
    });
    for (TextureAsset h : changed) {
        AGR_LOG_INFO("Assets", "hot reload '{}'", m_entries.get(h)->info.uri);
        reload(h);
    }
    return static_cast<u32>(changed.size());
}

AssetStats AssetManager::stats() const {
    AssetStats s;
    auto& self = const_cast<AssetManager&>(*this);
    self.m_entries.forEach([&](TextureAsset, Entry& e) {
        ++s.textures;
        if (e.info.state == AssetState::Loading) ++s.loading;
        if (e.info.state == AssetState::Failed) ++s.failed;
        if (e.info.gpu) s.gpuBytes += static_cast<u64>(e.info.width) * e.info.height * 4 * (e.info.options.mipmaps ? 4 : 3) / 3;
    });
    s.uploadedThisFrame = m_uploadedThisFrame;
    return s;
}

} // namespace agr
