#pragma once
// CPU profiler of the main thread: nested named scopes measured every frame (last frame, smoothed
// average, recent peak, calls). The engine instruments its frame phases, every plugin, every
// script class and the renderer's stages; games add their own scopes (AGR_PROFILE_SCOPE, or
// agr.profile.begin / finish in Lua). Shown by the editor's profiler panel.
#include <string>
#include <thread>
#include <vector>

#include "agr/core/Time.hpp"
#include "agr/core/Types.hpp"

namespace agr {

class Profiler {
public:
    struct ScopeStats {
        std::string name;
        std::string path;  ///< "Update/Script/assets://player.lua"
        u32 depth = 0;
        f64 lastMs = 0.0;  ///< Time inside the scope during the last finished frame.
        f64 avgMs = 0.0;   ///< Exponential moving average (~0.5 s).
        f64 maxMs = 0.0;   ///< Highest frame value of the last ~2 s.
        u32 calls = 0;     ///< Entries during the last frame.
    };

    static Profiler& instance();

    void setEnabled(bool enabled) { m_enabled = enabled; }
    bool enabled() const { return m_enabled; }

    /// Frame boundaries (Engine). Scopes still open at endFrame are closed.
    void beginFrame();
    void endFrame();
    /// Nested scopes (main thread only: calls from other threads are ignored).
    void push(std::string_view name);
    void pop();
    u32 depth() const { return static_cast<u32>(m_stack.size()); }

    /// Every scope seen so far, in tree order (parents before children, siblings by first use).
    std::vector<ScopeStats> snapshot() const;
    f64 lastFrameMs() const { return m_frameMs; }
    /// "path;last ms;average ms;max ms;calls" lines.
    std::string toCsv() const;
    void reset();

private:
    struct Node {
        std::string name;
        int parent = -1;
        u32 depth = 0;
        std::vector<int> children;
        f64 frameMs = 0.0, lastMs = 0.0, avgMs = 0.0, maxMs = 0.0, windowMax = 0.0;
        u32 frameCalls = 0, calls = 0;
    };
    struct Open {
        int node;
        Stopwatch clock;
    };
    bool onMainThread() const { return std::this_thread::get_id() == m_thread; }
    int child(int parent, std::string_view name);
    void collect(int node, const std::string& prefix, std::vector<ScopeStats>& out) const;

    bool m_enabled = true;
    std::thread::id m_thread = std::this_thread::get_id();
    std::vector<Node> m_nodes;
    std::vector<int> m_roots;
    std::vector<Open> m_stack;
    Stopwatch m_frameClock;
    f64 m_frameMs = 0.0, m_windowTime = 0.0;
};

/// RAII scope: AGR_PROFILE_SCOPE("Shadows");
class ProfileScope {
public:
    explicit ProfileScope(std::string_view name) { Profiler::instance().push(name); }
    ~ProfileScope() { Profiler::instance().pop(); }
    ProfileScope(const ProfileScope&) = delete;
    ProfileScope& operator=(const ProfileScope&) = delete;
};

#define AGR_PROFILE_CONCAT2(a, b) a##b
#define AGR_PROFILE_CONCAT(a, b) AGR_PROFILE_CONCAT2(a, b)
#define AGR_PROFILE_SCOPE(name) ::agr::ProfileScope AGR_PROFILE_CONCAT(agrProfileScope_, __LINE__)(name)

} // namespace agr
