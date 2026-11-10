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

/// Creates a release keystore with the JDK's keytool (RSA 2048, valid 30 years).
Status createKeystore(const AndroidToolchain& tools, const std::filesystem::path& path, const std::string& alias,
                      const std::string& password, const std::string& owner);

/// Launcher icon of the given size from an image (centered, scaled to fit, transparent margins).
Result<ByteBuffer> makeLauncherIcon(std::span<const u8> sourceImage, u32 size);

} // namespace agr::editor
