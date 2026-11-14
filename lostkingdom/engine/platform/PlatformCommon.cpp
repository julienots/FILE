// Shared POSIX implementations of platform services (files, time, logging formatting).
#include "../core/Platform.h"
#include <cstdarg>
#include <cstdio>
#include <chrono>
#include <thread>
#include <sys/stat.h>
#include <unistd.h>
#include <dirent.h>
#include <algorithm>
#include <mutex>

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

namespace agr {
namespace platform {
static std::vector<std::string> gOverrideRoots;
static std::mutex gOverrideMutex;
void setAssetOverrideRoots(const std::vector<std::string>& roots) { std::lock_guard<std::mutex> l(gOverrideMutex); gOverrideRoots = roots; }
bool readAssetOverride(const std::string& path, std::vector<uint8_t>& out) {
    std::vector<std::string> roots;
    { std::lock_guard<std::mutex> l(gOverrideMutex); roots = gOverrideRoots; }
    if (roots.empty() || path.find("..") != std::string::npos) return false;
    for (const auto& r : roots) {
        std::string full = r + "/assets/" + path;
        FILE* f = fopen(full.c_str(), "rb");
        if (!f) continue;
        fseek(f, 0, SEEK_END); long n = ftell(f); fseek(f, 0, SEEK_SET);
        if (n < 0 || n > 64l * 1024 * 1024) { fclose(f); continue; }
        out.resize((size_t)n);
        size_t rd = fread(out.data(), 1, (size_t)n, f);
        fclose(f);
        if (rd == (size_t)n) return true;
    }
    return false;
}
std::vector<std::string> listDirectories(const std::string& dir) {
    std::vector<std::string> out;
    DIR* d = opendir(dir.c_str());
    if (!d) return out;
    while (dirent* e = readdir(d)) {
        std::string n = e->d_name;
        if (n.empty() || n[0] == '.') continue;
        struct stat st;
        if (stat((dir + "/" + n).c_str(), &st) == 0 && S_ISDIR(st.st_mode)) out.push_back(n);
    }
    closedir(d);
    std::sort(out.begin(), out.end());
    return out;
}
bool makeDirectory(const std::string& dir) { return mkdir(dir.c_str(), 0755) == 0 || fileExists(dir); }
}  // namespace platform
}  // namespace agr
