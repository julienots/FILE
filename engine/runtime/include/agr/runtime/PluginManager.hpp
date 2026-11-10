#pragma once
#include <memory>
#include <string>
#include <vector>

#include "agr/core/SharedLibrary.hpp"
#include "agr/runtime/Plugin.hpp"

namespace agr {

/// Discovers, orders (by dependencies), loads, ticks and unloads plugins.
class PluginManager {
public:
    PluginManager() = default;
    ~PluginManager();
    PluginManager(const PluginManager&) = delete;
    PluginManager& operator=(const PluginManager&) = delete;

    /// Registers a statically linked plugin (loaded by loadAll()).
    void addStatic(std::unique_ptr<Plugin> plugin);
    /// Opens a plugin shared library and registers it (loaded by loadAll()).
    Status addDynamic(const std::string& libraryPath);
    /// Adds every "*agr_plugin*" shared library found in `directory`.
    u32 scanDirectory(const std::string& directory);

    /// Sorts by dependencies and calls onLoad. Plugins whose dependencies are
    /// missing or fail are skipped (and reported); the others still load.
    Status loadAll(Engine& engine);
    /// Calls onUnload in reverse load order and closes libraries.
    void unloadAll(Engine& engine);

    void fixedUpdate(Engine& engine, const FrameTime& time);
    void update(Engine& engine, const FrameTime& time);
    void render(Engine& engine, const FrameTime& time);

    struct Entry {
        PluginInfo info;
        bool dynamic = false;
        bool loaded = false;
        std::string path;
        std::string error;
    };
    std::vector<Entry> entries() const;
    bool isLoaded(const std::string& name) const;

private:
    struct Record {
        // Members are destroyed in reverse order: the plugin object (whose code
        // lives in the library) must be destroyed before the library closes.
        SharedLibrary library;
        std::unique_ptr<Plugin, void (*)(Plugin*)> plugin{nullptr, nullptr};
        PluginInfo info;
        bool dynamic = false;
        bool loaded = false;
        std::string path;
        std::string error;
    };
    Status sortByDependencies(std::vector<Record*>& order);

    std::vector<std::unique_ptr<Record>> m_records;
    std::vector<Record*> m_loadOrder;
};

} // namespace agr
