#pragma once
#include <filesystem>
#include <functional>
#include <span>
#include <string>
#include <vector>

#include "agr/core/FileSystem.hpp"
#include "agr/editor/GameExport.hpp"

namespace agr::editor {

/// External tools used by the Android export. Found from the environment
/// (ANDROID_HOME / ANDROID_SDK_ROOT, PATH, JAVA_HOME) and the editor installation.
struct AndroidToolchain {
    std::filesystem::path sdk;        ///< Android SDK (platforms/, build-tools/).
    std::filesystem::path gradle;     ///< Gradle executable (8.13 or newer).
    std::filesystem::path templates;  ///< Export templates: android/<abi>/libmain.so, libSDL3.so, android/java/.
    std::filesystem::path keytool;    ///< JDK keytool (release keystores).
    std::filesystem::path adb;        ///< Android Debug Bridge (AGR_ADB, SDK platform-tools, PATH): one-click deploy.
};

/// Locates the tools: environment variables, PATH, then the editor's folder
/// (export_templates/ next to the executable or in the build tree).
AndroidToolchain findAndroidToolchain(const std::filesystem::path& editorDirectory);
/// Problems that prevent an export (missing SDK platform, templates for an ABI, invalid id...).
std::vector<std::string> checkAndroidExport(const Project& project, const AndroidToolchain& tools);

enum class AndroidBuildKind : u8 {
    DebugApk,      ///< Signed with the debug key: install on your own devices.
    ReleaseApk,    ///< Signed with the project keystore.
    ReleaseBundle  ///< .aab for Google Play, signed with the project keystore.
};

struct AndroidBuildOptions {
    AndroidBuildKind kind = AndroidBuildKind::DebugApk;
    std::string storePassword;  ///< Release only; passed to Gradle through the environment, never written.
    std::string keyPassword;
};

/// Writes a complete Gradle project (Android Studio compatible) for the game into
/// `gradleDir`: manifest, activity, SDL Java sources, native libraries, game data, icons.
Result<ExportReport> generateAndroidProject(const Project& project, const AndroidToolchain& tools, const std::filesystem::path& gradleDir);

/// Generates the Gradle project in `outputDir`/gradle and runs Gradle; the APK / AAB is copied
/// to `outputDir`. `log` receives Gradle's output line by line (from the calling thread).
Result<ExportReport> buildAndroid(const Project& project, const AndroidToolchain& tools, const std::filesystem::path& outputDir,
                                  const AndroidBuildOptions& options, const std::function<void(const std::string&)>& log = {});

/// A phone, tablet or emulator seen by adb.
struct AndroidDevice {
    std::string serial;  ///< adb -s <serial>
    std::string state;   ///< "device" (ready), "unauthorized" (accept the prompt on the phone), "offline"...
    std::string model;   ///< e.g. "Pixel_7" (empty if adb does not say)
    bool ready() const { return state == "device"; }
};
/// Parses `adb devices -l`.
std::vector<AndroidDevice> parseAdbDevices(const std::string& output);
/// Devices connected now (USB or Wi-Fi debugging, emulators).
Result<std::vector<AndroidDevice>> listAndroidDevices(const AndroidToolchain& tools);
/// One-click deploy: installs the APK on the device (replacing an older install, keeping its data)
/// and starts the game. `serial` empty = the only ready device. `log` receives adb's output.
Status deployToAndroid(const AndroidToolchain& tools, const std::filesystem::path& apk, const std::string& packageId, const std::string& serial,
                       const std::function<void(const std::string&)>& log = {});
/// adb command that prints the game's log (engine messages and Lua errors, tag "AGR" and SDL),
/// e.g. for a terminal: `adb -s SERIAL logcat -s AGR:V SDL:V`.
std::string androidLogcatCommand(const AndroidToolchain& tools, const std::string& serial);

/// Creates a release keystore with the JDK's keytool (RSA 2048, valid 30 years).
Status createKeystore(const AndroidToolchain& tools, const std::filesystem::path& path, const std::string& alias,
                      const std::string& password, const std::string& owner);

/// Launcher icon of the given size from an image (centered, scaled to fit, transparent margins).
/// `fill`: fraction of the square the image fits in (0.84 legacy icons, ~0.6 adaptive foregrounds).
Result<ByteBuffer> makeLauncherIcon(std::span<const u8> sourceImage, u32 size, float fill = 0.84f);

} // namespace agr::editor
