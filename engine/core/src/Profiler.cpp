#include "agr/core/Profiler.hpp"

#include <algorithm>
#include <cstdio>

namespace agr {

Profiler& Profiler::instance() {
    static Profiler p;
    return p;
}

int Profiler::child(int parent, std::string_view name) {
    const std::vector<int>& list = parent < 0 ? m_roots : m_nodes[static_cast<usize>(parent)].children;
    for (int c : list)
        if (m_nodes[static_cast<usize>(c)].name == name) return c;
    Node n;
    n.name = std::string(name);
    n.parent = parent;
    n.depth = parent < 0 ? 0 : m_nodes[static_cast<usize>(parent)].depth + 1;
    m_nodes.push_back(std::move(n));
    const int id = static_cast<int>(m_nodes.size()) - 1;
    if (parent < 0) m_roots.push_back(id);
    else m_nodes[static_cast<usize>(parent)].children.push_back(id);
    return id;
}

void Profiler::push(std::string_view name) {
    if (!m_enabled || !onMainThread()) return;
    if (m_stack.size() >= 64) return;  // runaway recursion guard (pop() checks the same)
    const int parent = m_stack.empty() ? -1 : m_stack.back().node;
    m_stack.push_back({child(parent, name), Stopwatch{}, m_captureLeft > 0 ? m_captureClock.elapsedSeconds() * 1e6 : 0.0});
}

void Profiler::pop() {
    if (!onMainThread() || m_stack.empty()) return;
    const Open o = m_stack.back();
    m_stack.pop_back();
    Node& n = m_nodes[static_cast<usize>(o.node)];
    n.frameMs += o.clock.elapsedMilliseconds();
    ++n.frameCalls;
    if (m_captureLeft > 0 && m_captureFrameOpen && m_trace.size() < 2000000)
        m_trace.push_back({n.name, o.traceStart, m_captureClock.elapsedSeconds() * 1e6 - o.traceStart, static_cast<u32>(m_stack.size()) + 1});
}

void Profiler::beginFrame() {
    if (!onMainThread()) return;
    m_frameClock.restart();
    if (m_captureLeft > 0) {
        m_captureFrameOpen = true;
        m_captureFrameStart = m_captureClock.elapsedSeconds() * 1e6;
    }
}

void Profiler::endFrame() {
    if (!onMainThread()) return;
    while (!m_stack.empty()) pop();
    if (m_captureLeft > 0 && m_captureFrameOpen) {
        m_trace.push_back({"Frame", m_captureFrameStart, m_captureClock.elapsedSeconds() * 1e6 - m_captureFrameStart, 0});
        m_captureFrameOpen = false;
        --m_captureLeft;
    }
    m_frameMs = m_frameClock.elapsedMilliseconds();
    m_windowTime += m_frameMs;
    const bool newWindow = m_windowTime > 2000.0;
    for (Node& n : m_nodes) {
        n.lastMs = n.frameMs;
        n.calls = n.frameCalls;
        n.avgMs = n.avgMs == 0.0 ? n.frameMs : n.avgMs * 0.9 + n.frameMs * 0.1;
        n.windowMax = std::max(n.windowMax, n.frameMs);
        n.maxMs = std::max(n.maxMs, n.frameMs);
        if (newWindow) {
            n.maxMs = n.windowMax;
            n.windowMax = 0.0;
        }
        n.frameMs = 0.0;
        n.frameCalls = 0;
    }
    if (newWindow) m_windowTime = 0.0;
}

void Profiler::collect(int node, const std::string& prefix, std::vector<ScopeStats>& out) const {
    const Node& n = m_nodes[static_cast<usize>(node)];
    ScopeStats s;
    s.name = n.name;
    s.path = prefix.empty() ? n.name : prefix + "/" + n.name;
    s.depth = n.depth;
    s.lastMs = n.lastMs;
    s.avgMs = n.avgMs;
    s.maxMs = n.maxMs;
    s.calls = n.calls;
    out.push_back(s);
    const std::string path = s.path;
    for (int c : n.children) collect(c, path, out);
}

std::vector<Profiler::ScopeStats> Profiler::snapshot() const {
    std::vector<ScopeStats> out;
    for (int r : m_roots) collect(r, "", out);
    return out;
}

std::string Profiler::toCsv() const {
    std::string out = "scope;last_ms;average_ms;max_ms;calls\n";
    char buf[96];
    for (const ScopeStats& s : snapshot()) {
        std::snprintf(buf, sizeof buf, ";%.4f;%.4f;%.4f;%u\n", s.lastMs, s.avgMs, s.maxMs, s.calls);
        out += s.path + buf;
    }
    return out;
}

void Profiler::reset() {
    m_nodes.clear();
    m_roots.clear();
    m_stack.clear();
    m_windowTime = 0.0;
}

void Profiler::startCapture(u32 frames) {
    m_trace.clear();
    m_captureLeft = frames;
    m_captureFrameOpen = false;
    m_captureClock.restart();
}

std::string Profiler::captureToChromeTrace() const {
    std::string out = "{\"displayTimeUnit\":\"ms\",\"traceEvents\":[";
    char buf[96];
    bool first = true;
    for (const TraceEvent& e : m_trace) {
        out += first ? "\n" : ",\n";
        first = false;
        out += "{\"name\":\"";
        for (char c : e.name) {  // JSON string escaping (names are paths and identifiers)
            if (c == '"' || c == '\\') out += '\\', out += c;
            else if (static_cast<unsigned char>(c) < 0x20) out += ' ';
            else out += c;
        }
        std::snprintf(buf, sizeof buf, "\",\"cat\":\"agr\",\"ph\":\"X\",\"ts\":%.3f,\"dur\":%.3f,\"pid\":1,\"tid\":1}", e.startUs, std::max(e.durationUs, 0.0));
        out += buf;
    }
    out += "\n]}\n";
    return out;
}

} // namespace agr
