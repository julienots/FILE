// Shared POSIX implementations of platform services (files, time, logging formatting).
#include "../core/Platform.h"
#include <cstdarg>
#include <cstdio>
#include <chrono>
#include <thread>
#include <sys/stat.h>
#include <unistd.h>

#ifdef __ANDROID__
#include <android/log.h>
#endif

namespace agr {

void logMessage(LogLevel lvl, const char* fmt, ...) {
    char buf[2048];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
#ifdef __ANDROID__
    __android_log_write(lvl == LogLevel::Error ? ANDROID_LOG_ERROR : (lvl == LogLevel::Warn ? ANDROID_LOG_WARN : ANDROID_LOG_INFO), "LostKingdom", buf);
#else
    fprintf(lvl == LogLevel::Info ? stdout : stderr, "[%s] %s\n", lvl == LogLevel::Error ? "ERR" : (lvl == LogLevel::Warn ? "WRN" : "inf"), buf);
    fflush(stdout);
#endif
}

namespace platform {

bool writeFileAtomic(const std::string& path, const std::string& data) {
    std::string tmp = path + ".tmp";
    FILE* f = fopen(tmp.c_str(), "wb");
    if (!f) { AGR_ERR("cannot write %s", tmp.c_str()); return false; }
    size_t n = fwrite(data.data(), 1, data.size(), f);
    fflush(f);
    fsync(fileno(f));
    fclose(f);
    if (n != data.size()) { remove(tmp.c_str()); return false; }
    if (rename(tmp.c_str(), path.c_str()) != 0) { remove(tmp.c_str()); return false; }
    return true;
}

bool readFile(const std::string& path, std::string& out) {
    FILE* f = fopen(path.c_str(), "rb");
    if (!f) return false;
    fseek(f, 0, SEEK_END);
    long n = ftell(f);
    fseek(f, 0, SEEK_SET);
    out.resize(n > 0 ? (size_t)n : 0);
    size_t r = n > 0 ? fread(&out[0], 1, (size_t)n, f) : 0;
    fclose(f);
    return r == out.size();
}

bool fileExists(const std::string& path) { struct stat st; return stat(path.c_str(), &st) == 0; }
void removeFile(const std::string& path) { remove(path.c_str()); }

double timeSeconds() {
    using namespace std::chrono;
    static const auto t0 = steady_clock::now();
    return duration<double>(steady_clock::now() - t0).count();
}

int cpuCount() { unsigned n = std::thread::hardware_concurrency(); return n ? (int)n : 2; }

}  // namespace platform
}  // namespace agr
