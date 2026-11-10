#pragma once
#include <cstdio>
#include <format>
#include <memory>
#include <mutex>
#include <string>
#include <string_view>
#include <vector>

#include "agr/core/Types.hpp"

namespace agr {

enum class LogLevel : u8 { Trace, Debug, Info, Warning, Error, Fatal, Off };

const char* enumName(LogLevel level);

struct LogMessage {
    LogLevel level = LogLevel::Info;
    std::string_view channel;  ///< Subsystem: "Core", "Render", "Physics"...
    std::string_view text;
    f64 timeSeconds = 0.0;     ///< Seconds since logger start.
    u64 threadId = 0;
};

/// Destination of log messages. Sinks are called under the logger mutex.
class LogSink {
public:
    virtual ~LogSink() = default;
    virtual void write(const LogMessage& message) = 0;
    virtual void flush() {}
};

/// Colored stdout / stderr sink (logcat on Android).
class ConsoleSink final : public LogSink {
public:
    void write(const LogMessage& message) override;
};

/// Appends to a text file.
class FileSink final : public LogSink {
public:
    explicit FileSink(const std::string& path);
    ~FileSink() override;
    bool isOpen() const { return m_file != nullptr; }
    void write(const LogMessage& message) override;
    void flush() override;

private:
    std::FILE* m_file = nullptr;
};

/// Keeps the last N messages in memory; feeds the editor console.
class RingBufferSink final : public LogSink {
public:
    struct Entry {
        LogLevel level;
        std::string channel;
        std::string text;
        f64 timeSeconds;
    };
    explicit RingBufferSink(usize capacity = 2048);
    void write(const LogMessage& message) override;
    /// Copies the entries, oldest first (thread-safe).
    std::vector<Entry> snapshot() const;
    void clear();
    /// Number of messages ever written (cheap change detection for UIs).
    u64 totalWritten() const;

private:
    mutable std::mutex m_mutex;
    std::vector<Entry> m_entries;
    usize m_capacity;
    usize m_head = 0;
    bool m_wrapped = false;
    u64 m_total = 0;
};

/// Global, thread-safe logger.
class Log {
public:
    static void addSink(std::shared_ptr<LogSink> sink);
    static void removeSink(const std::shared_ptr<LogSink>& sink);
    static void clearSinks();
    static void setLevel(LogLevel level);
    static LogLevel level();
    static void write(LogLevel level, std::string_view channel, std::string_view text);
    static void flush();

    /// When set, every message is forwarded to `fn` instead of local sinks.
    /// Plugins use it to route their logs into the host engine.
    using ForwardFn = void (*)(LogLevel, std::string_view channel, std::string_view text);
    static void setForwarder(ForwardFn fn);

    template <typename... Args>
    static void print(LogLevel lvl, std::string_view channel, std::format_string<Args...> fmt, Args&&... args) {
        if (lvl < level()) return;
        write(lvl, channel, std::format(fmt, std::forward<Args>(args)...));
    }
};

} // namespace agr

#define AGR_LOG_TRACE(channel, ...) ::agr::Log::print(::agr::LogLevel::Trace, channel, __VA_ARGS__)
#define AGR_LOG_DEBUG(channel, ...) ::agr::Log::print(::agr::LogLevel::Debug, channel, __VA_ARGS__)
#define AGR_LOG_INFO(channel, ...) ::agr::Log::print(::agr::LogLevel::Info, channel, __VA_ARGS__)
#define AGR_LOG_WARN(channel, ...) ::agr::Log::print(::agr::LogLevel::Warning, channel, __VA_ARGS__)
#define AGR_LOG_ERROR(channel, ...) ::agr::Log::print(::agr::LogLevel::Error, channel, __VA_ARGS__)
