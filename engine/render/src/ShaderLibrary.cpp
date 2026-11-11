#include "agr/render/ShaderLibrary.hpp"

#include <vector>

#include "agr/core/Log.hpp"

namespace agr {

void ShaderLibrary::set(ShaderAsset& asset, const std::string& source) {
    if (asset.version != 0 && asset.source == source) return;
    asset.source = source;
    asset.parsed = parseCustomShader(source);
    asset.version = m_nextVersion++;
}

const ShaderAsset* ShaderLibrary::get(const std::string& uri) {
    if (uri.empty()) return nullptr;
    if (auto it = m_entries.find(uri); it != m_entries.end()) return it->second.get();
    std::unique_ptr<ShaderAsset> asset;
    if (auto text = m_vfs.readText(uri); !text) {
        AGR_LOG_ERROR("Assets", "shader '{}': {}", uri, text.error().message);
    } else {
        asset = std::make_unique<ShaderAsset>();
        asset->modifiedTime = m_vfs.modifiedTime(uri);
        set(*asset, text.value());
        for (const ShaderDiagnostic& d : asset->parsed.errors) AGR_LOG_ERROR("Assets", "shader '{}' line {}: {}", uri, d.line, d.message);
    }
    return m_entries.emplace(uri, std::move(asset)).first->second.get();
}

Status ShaderLibrary::save(const std::string& uri, const std::string& source) {
    if (Status s = m_vfs.writeText(uri, source); !s) return s;
    auto& slot = m_entries[uri];
    if (!slot) slot = std::make_unique<ShaderAsset>();
    set(*slot, source);
    slot->modifiedTime = m_vfs.modifiedTime(uri);
    return {};
}

void ShaderLibrary::preview(const std::string& uri, const std::string& source) {
    auto& slot = m_entries[uri];
    if (!slot) {
        slot = std::make_unique<ShaderAsset>();
        slot->modifiedTime = m_vfs.modifiedTime(uri);
    }
    set(*slot, source);
}

u32 ShaderLibrary::reloadChanged() {
    std::vector<std::string> changed;
    for (const auto& [uri, asset] : m_entries) {
        const i64 t = m_vfs.modifiedTime(uri);
        if (t != 0 && (!asset || t != asset->modifiedTime)) changed.push_back(uri);
    }
    for (const std::string& uri : changed) {
        auto text = m_vfs.readText(uri);
        auto& slot = m_entries[uri];
        if (!text) {
            if (slot) slot->modifiedTime = m_vfs.modifiedTime(uri);
            AGR_LOG_ERROR("Assets", "shader '{}': {}", uri, text.error().message);
            continue;  // keep the last version
        }
        if (!slot) slot = std::make_unique<ShaderAsset>();
        slot->modifiedTime = m_vfs.modifiedTime(uri);
        set(*slot, text.value());
        AGR_LOG_INFO("Assets", "hot reload '{}'", uri);
    }
    return static_cast<u32>(changed.size());
}

} // namespace agr
