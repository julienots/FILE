#include "agr/runtime/SaveStore.hpp"

#include "agr/core/Log.hpp"

namespace agr {

void SaveStore::load() {
    if (m_loaded) return;
    m_loaded = true;
    m_data = nlohmann::json::object();
    if (!m_vfs.exists(m_uri)) return;
    auto text = m_vfs.readText(m_uri);
    if (!text) {
        AGR_LOG_WARN("Save", "cannot read {}: {}", m_uri, text.error().message);
        return;
    }
    nlohmann::json j = nlohmann::json::parse(text.value(), nullptr, false);
    if (j.is_object() && j.contains("values") && j["values"].is_object()) m_data = std::move(j["values"]);
    else AGR_LOG_WARN("Save", "{} is damaged: starting from an empty save", m_uri);
}

bool SaveStore::has(const std::string& key) {
    load();
    return m_data.contains(key);
}

const nlohmann::json& SaveStore::get(const std::string& key) {
    static const nlohmann::json null;
    load();
    auto it = m_data.find(key);
    return it == m_data.end() ? null : *it;
}

void SaveStore::set(const std::string& key, nlohmann::json value) {
    load();
    if (value.is_null()) {
        remove(key);
        return;
    }
    auto it = m_data.find(key);
    if (it != m_data.end() && *it == value) return;
    m_data[key] = std::move(value);
    m_dirty = true;
}

void SaveStore::remove(const std::string& key) {
    load();
    if (m_data.erase(key) > 0) m_dirty = true;
}

void SaveStore::clear() {
    load();
    if (!m_data.empty()) m_dirty = true;
    m_data = nlohmann::json::object();
}

Status SaveStore::flush() {
    if (!m_dirty) return {};
    const nlohmann::json doc{{"format", "agr.save"}, {"version", 1}, {"values", m_data}};
    if (Status s = m_vfs.writeText(m_uri, doc.dump(1)); !s) return s;
    m_dirty = false;
    return {};
}

void SaveStore::reload() {
    m_loaded = false;
    m_dirty = false;
    load();
}

} // namespace agr
