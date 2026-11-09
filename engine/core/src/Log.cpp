#include "agr/core/Log.hpp"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdio>
#include <functional>
#include <thread>

#include "agr/core/Platform.hpp"

#if defined(AGR_PLATFORM_ANDROID)
    #include <android/log.h>
#endif

namespace agr {
namespace {

struct LoggerState {
    std::mutex mutex;
    std::vector<std::shared_ptr<LogSink>> sinks;
    std::atomic<LogLevel> level{LogLevel::Info};
    std::chrono::steady_clock::time_point start = std::chrono::steady_clock::now();
    bool defaultSinkInstalled = false;
    std::atomic<Log::ForwardFn> forwarder{nullptr};
};

LoggerState& state() {
    static LoggerState s;
    return s;
}

// Installs a console sink the first time something is logged and no sink was
// configured explicitly, so early boot messages are never lost.
void ensureDefaultSink(LoggerState& s) {
    if (!s.defaultSinkInstalled && s.sinks.empty()) {
        s.sinks.push_back(std::make_shared<ConsoleSink>());
    }
    s.defaultSinkInstalled = true;
}

} // namespace

const char* enumName(LogLevel level) {
    switch (level) {
        case LogLevel::Trace: return "TRACE";
        case LogLevel::Debug: return "DEBUG";
        case LogLevel::Info: return "INFO";
        case LogLevel::Warning: return "WARN";
        case LogLevel::Error: return "ERROR";
        case LogLevel::Fatal: return "FATAL";
        case LogLevel::Off: return "OFF";
    }
    return "?";
}

// ---------------------------------------------------------------- ConsoleSink
void ConsoleSink::write(const LogMessage& m) {
#if defined(AGR_PLATFORM_ANDROID)
    static constexpr int prio[] = {ANDROID_LOG_VERBOSE, ANDROID_LOG_DEBUG, ANDROID_LOG_INFO,
                                   ANDROID_LOG_WARN, ANDROID_LOG_ERROR, ANDROID_LOG_FATAL, ANDROID_LOG_SILENT};
    __android_log_print(prio[static_cast<int>(m.level)], "AGR", "[%.*s] %.*s", static_cast<int>(m.channel.size()),
                        m.channel.data(), static_cast<int>(m.text.size()), m.text.data());
#else
    static constexpr const char* colors[] = {"\x1b[90m", "\x1b[36m", "\x1b[32m", "\x1b[33m", "\x1b[31m", "\x1b[41;97m", ""};
    std::FILE* out = m.level >= LogLevel::Error ? stderr : stdout;
    std::fprintf(out, "%s[%9.3f] %-5s [%.*s] %.*s\x1b[0m\n", colors[static_cast<int>(m.level)], m.timeSeconds,
                 enumName(m.level), static_cast<int>(m.channel.size()), m.channel.data(), static_cast<int>(m.text.size()),
                 m.text.data());
#endif
}

// ------------------------------------------------------------------- FileSink
FileSink::FileSink(const std::string& path) { m_file = std::fopen(path.c_str(), "w"); }
FileSink::~FileSink() {
    if (m_file) std::fclose(m_file);
}
void FileSink::write(const LogMessage& m) {
    if (!m_file) return;
    std::fprintf(m_file, "[%9.3f] %-5s [%.*s] %.*s\n", m.timeSeconds, enumName(m.level), static_cast<int>(m.channel.size()),
                 m.channel.data(), static_cast<int>(m.text.size()), m.text.data());
}
void FileSink::flush() {
    if (m_file) std::fflush(m_file);
}

// ------------------------------------------------------------- RingBufferSink
RingBufferSink::RingBufferSink(usize capacity) : m_capacity(std::max<usize>(capacity, 1)) { m_entries.reserve(m_capacity); }

void RingBufferSink::write(const LogMessage& m) {
    std::lock_guard lock(m_mutex);
    Entry e{m.level, std::string(m.channel), std::string(m.text), m.timeSeconds};
    if (m_entries.size() < m_capacity) {
        m_entries.push_back(std::move(e));
    } else {
        m_entries[m_head] = std::move(e);
        m_wrapped = true;
    }
    m_head = (m_head + 1) % m_capacity;
}

std::vector<RingBufferSink::Entry> RingBufferSink::snapshot() const {
    std::lock_guard lock(m_mutex);
    if (!m_wrapped) return m_entries;
    std::vector<Entry> out;
    out.reserve(m_entries.size());
    for (usize i = 0; i < m_entries.size(); ++i) out.push_back(m_entries[(m_head + i) % m_capacity]);
    return out;
}

void RingBufferSink::clear() {
    std::lock_guard lock(m_mutex);
    m_entries.clear();
    m_head = 0;
    m_wrapped = false;
}

// ------------------------------------------------------------------------ Log
void Log::addSink(std::shared_ptr<LogSink> sink) {
    auto& s = state();
    std::lock_guard lock(s.mutex);
    s.defaultSinkInstalled = true;
    s.sinks.push_back(std::move(sink));
}

void Log::removeSink(const std::shared_ptr<LogSink>& sink) {
    auto& s = state();
    std::lock_guard lock(s.mutex);
    std::erase(s.sinks, sink);
}

void Log::clearSinks() {
    auto& s = state();
    std::lock_guard lock(s.mutex);
    s.sinks.clear();
    s.defaultSinkInstalled = true;
}

void Log::setLevel(LogLevel level) { state().level.store(level, std::memory_order_relaxed); }
LogLevel Log::level() { return state().level.load(std::memory_order_relaxed); }

void Log::write(LogLevel level, std::string_view channel, std::string_view text) {
    auto& s = state();
    if (level < s.level.load(std::memory_order_relaxed)) return;
    if (auto fwd = s.forwarder.load(std::memory_order_acquire)) {
        fwd(level, channel, text);
        return;
    }
    LogMessage msg;
    msg.level = level;
    msg.channel = channel;
    msg.text = text;
    msg.timeSeconds = std::chrono::duration<f64>(std::chrono::steady_clock::now() - s.start).count();
    msg.threadId = static_cast<u64>(std::hash<std::thread::id>{}(std::this_thread::get_id()));
    std::lock_guard lock(s.mutex);
    ensureDefaultSink(s);
    for (auto& sink : s.sinks) sink->write(msg);
    if (level >= LogLevel::Error) {
        for (auto& sink : s.sinks) sink->flush();
    }
}

void Log::setForwarder(ForwardFn fn) { state().forwarder.store(fn, std::memory_order_release); }

void Log::flush() {
    auto& s = state();
    std::lock_guard lock(s.mutex);
    for (auto& sink : s.sinks) sink->flush();
}

} // namespace agr
