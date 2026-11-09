#include "agr/runtime/PluginManager.hpp"

#include <algorithm>
#include <filesystem>
#include <functional>
#include <unordered_map>

#include "agr/core/Log.hpp"

namespace agr {

PluginManager::~PluginManager() {
    if (!m_loadOrder.empty())
        AGR_LOG_WARN("Plugins", "PluginManager destroyed while plugins are still loaded (call unloadAll)");
    // Destroy plugin objects before their libraries are closed (member order).
    for (auto& r : m_records) {
        r->plugin.reset();
        r->library.close();
    }
}

void PluginManager::addStatic(std::unique_ptr<Plugin> plugin) {
    auto rec = std::make_unique<Record>();
    rec->info = plugin->info();
    rec->plugin = std::unique_ptr<Plugin, void (*)(Plugin*)>(plugin.release(), [](Plugin* p) { delete p; });
    m_records.push_back(std::move(rec));
}

Status PluginManager::addDynamic(const std::string& libraryPath) {
    auto lib = SharedLibrary::open(libraryPath);
    if (!lib) return lib.error();
    auto apiFn = reinterpret_cast<PluginApiVersionFn>(lib.value().symbol("agrPluginApiVersion"));
    auto createFn = reinterpret_cast<PluginCreateFn>(lib.value().symbol("agrCreatePlugin"));
    auto destroyFn = reinterpret_cast<PluginDestroyFn>(lib.value().symbol("agrDestroyPlugin"));
    if (!apiFn || !createFn || !destroyFn)
        return makeError(Error::Code::InvalidArgument, "'" + libraryPath + "' is not an AGR plugin (missing entry points)");
    if (apiFn() != kPluginApiVersion)
        return makeError(Error::Code::VersionMismatch, "'" + libraryPath + "' was built for plugin API " +
                                                           std::to_string(apiFn()) + ", host is " +
                                                           std::to_string(kPluginApiVersion));
    HostApi host;
    host.logWrite = &Log::write;
    host.components = &ComponentRegistry::instance();
    Plugin* raw = createFn(&host);
    if (!raw) return makeError(Error::Code::Unknown, "'" + libraryPath + "' failed to create its plugin instance");

    auto rec = std::make_unique<Record>();
    rec->plugin = std::unique_ptr<Plugin, void (*)(Plugin*)>(raw, destroyFn);
    rec->library = std::move(lib.value());
    rec->info = raw->info();
    rec->dynamic = true;
    rec->path = libraryPath;
    for (const auto& r : m_records) {
        if (r->info.name == rec->info.name)
            return makeError(Error::Code::AlreadyExists, "plugin '" + rec->info.name + "' is already registered");
    }
    AGR_LOG_INFO("Plugins", "found plugin '{}' {} ({})", rec->info.name, rec->info.version, libraryPath);
    m_records.push_back(std::move(rec));
    return {};
}

u32 PluginManager::scanDirectory(const std::string& directory) {
    namespace fs = std::filesystem;
    std::error_code ec;
    if (!fs::is_directory(directory, ec)) return 0;
    std::vector<fs::path> candidates;
    for (const auto& entry : fs::directory_iterator(directory, ec)) {
        const std::string file = entry.path().filename().string();
        const std::string ext = entry.path().extension().string();
        if (file.find("agr_plugin") != std::string::npos && (ext == ".so" || ext == ".dll" || ext == ".dylib"))
            candidates.push_back(entry.path());
    }
    std::sort(candidates.begin(), candidates.end());
    u32 added = 0;
    for (const auto& path : candidates) {
        Status s = addDynamic(path.string());
        if (s) ++added;
        else AGR_LOG_ERROR("Plugins", "{}", s.error().message);
    }
    return added;
}

Status PluginManager::sortByDependencies(std::vector<Record*>& order) {
    std::unordered_map<std::string, Record*> byName;
    for (auto& r : m_records) byName[r->info.name] = r.get();

    enum class Mark { None, Visiting, Done };
    std::unordered_map<Record*, Mark> marks;
    std::string cycleError;
    std::function<bool(Record*)> visit = [&](Record* r) -> bool {
        Mark& m = marks[r];
        if (m == Mark::Done) return true;
        if (m == Mark::Visiting) {
            cycleError = "dependency cycle involving '" + r->info.name + "'";
            return false;
        }
        m = Mark::Visiting;
        for (const auto& dep : r->info.dependencies) {
            auto it = byName.find(dep);
            if (it == byName.end()) {
                r->error = "missing dependency '" + dep + "'";
                continue;
            }
            if (!visit(it->second)) return false;
        }
        marks[r] = Mark::Done;
        order.push_back(r);
        return true;
    };
    for (auto& r : m_records)
        if (!visit(r.get())) return makeError(Error::Code::InvalidArgument, cycleError);
    return {};
}

Status PluginManager::loadAll(Engine& engine) {
    std::vector<Record*> order;
    if (Status s = sortByDependencies(order); !s) return s;
    for (Record* r : order) {
        if (r->loaded) continue;
        if (r->error.empty()) {
            for (const auto& dep : r->info.dependencies) {
                auto it = std::find_if(m_records.begin(), m_records.end(), [&](auto& o) { return o->info.name == dep; });
                if (it != m_records.end() && !(*it)->loaded) r->error = "dependency '" + dep + "' failed to load";
            }
        }
        if (!r->error.empty()) {
            AGR_LOG_ERROR("Plugins", "plugin '{}' not loaded: {}", r->info.name, r->error);
            continue;
        }
        Status s = r->plugin->onLoad(engine);
        if (!s) {
            r->error = s.error().message;
            AGR_LOG_ERROR("Plugins", "plugin '{}' failed to load: {}", r->info.name, r->error);
            continue;
        }
        r->loaded = true;
        m_loadOrder.push_back(r);
        AGR_LOG_INFO("Plugins", "loaded plugin '{}' {}", r->info.name, r->info.version);
    }
    return {};
}

void PluginManager::unloadAll(Engine& engine) {
    for (auto it = m_loadOrder.rbegin(); it != m_loadOrder.rend(); ++it) {
        (*it)->plugin->onUnload(engine);
        (*it)->loaded = false;
        AGR_LOG_INFO("Plugins", "unloaded plugin '{}'", (*it)->info.name);
    }
    m_loadOrder.clear();
    for (auto& r : m_records) {
        r->plugin.reset();
        r->library.close();
    }
    m_records.clear();
}

void PluginManager::fixedUpdate(Engine& engine, const FrameTime& time) {
    for (Record* r : m_loadOrder) r->plugin->onFixedUpdate(engine, time);
}

void PluginManager::update(Engine& engine, const FrameTime& time) {
    for (Record* r : m_loadOrder) r->plugin->onUpdate(engine, time);
}

std::vector<PluginManager::Entry> PluginManager::entries() const {
    std::vector<Entry> out;
    for (const auto& r : m_records) out.push_back(Entry{r->info, r->dynamic, r->loaded, r->path, r->error});
    return out;
}

bool PluginManager::isLoaded(const std::string& name) const {
    for (const auto& r : m_records)
        if (r->info.name == name) return r->loaded;
    return false;
}

} // namespace agr
