// AGR Engine — platform abstraction.
// Implemented by platform/AndroidMain.cpp (NativeActivity, EGL, AAudio, JNI haptics)
// and platform/DesktopMain.cpp (headless EGL/Mesa for automated tests & screenshots).
#pragma once
#include <cstdint>
#include <string>
#include <vector>
#include <cstdio>

namespace agr {

enum class LogLevel { Info, Warn, Error };
void logMessage(LogLevel lvl, const char* fmt, ...);
#define AGR_LOG(...) ::agr::logMessage(::agr::LogLevel::Info, __VA_ARGS__)
#define AGR_WARN(...) ::agr::logMessage(::agr::LogLevel::Warn, __VA_ARGS__)
#define AGR_ERR(...) ::agr::logMessage(::agr::LogLevel::Error, __VA_ARGS__)

struct TouchEvent {
    enum Type { Down, Move, Up, Cancel } type;
    int id;        // pointer id, stable while finger is down
    float x, y;    // pixels, origin top-left
};

namespace platform {
bool readAsset(const std::string& path, std::vector<uint8_t>& out);
std::string writableDir();           // persistent per-app directory (saves, settings)
bool writeFileAtomic(const std::string& path, const std::string& data);
bool readFile(const std::string& path, std::string& out);
bool fileExists(const std::string& path);
void removeFile(const std::string& path);
double timeSeconds();
void vibrate(int milliseconds, int amplitude /*1..255*/);
void requestQuit();
int cpuCount();
// Test/automation hooks (desktop harness only; no-ops on Android).
void requestScreenshot(const std::string& path);
const std::vector<std::string>& launchArgs();
bool isHeadlessTest();
// ---- mods (1.7.0): folders whose "assets/" subtree overrides the packaged assets (first root wins)
void setAssetOverrideRoots(const std::vector<std::string>& roots);
bool readAssetOverride(const std::string& path, std::vector<uint8_t>& out);   // used by readAsset implementations
std::vector<std::string> listDirectories(const std::string& dir);              // sub-directory names (sorted)
bool makeDirectory(const std::string& dir);
bool requestModImport();   // Android: opens the system file picker for a .zip mod (false where unsupported)
}  // namespace platform

}  // namespace agr
