#include "agr/editor/AndroidExport.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <sstream>

#include "agr/assets/Image.hpp"
#include "agr/core/Log.hpp"

namespace agr::editor {

namespace fs = std::filesystem;

namespace {

constexpr const char* kAgpVersion = "8.13.2";  // Android Gradle Plugin (needs Gradle 8.13+)

std::string env(const char* name) {
    const char* v = std::getenv(name);
    return v ? std::string(v) : std::string();
}

#if defined(_WIN32)
constexpr char kPathSeparator = ';';
const std::vector<std::string> kExeSuffixes{".exe", ".bat", ".cmd", ""};
#else
constexpr char kPathSeparator = ':';
const std::vector<std::string> kExeSuffixes{""};
#endif

fs::path findInPath(const std::string& name) {
    std::stringstream ss(env("PATH"));
    std::string dir;
    std::error_code ec;
    while (std::getline(ss, dir, kPathSeparator)) {
        if (dir.empty()) continue;
        for (const std::string& suffix : kExeSuffixes) {
            const fs::path p = fs::path(dir) / (name + suffix);
            if (fs::is_regular_file(p, ec)) return p;
        }
    }
    return {};
}

Status writeText(const fs::path& path, const std::string& text) {
    std::error_code ec;
    fs::create_directories(path.parent_path(), ec);
    std::ofstream out(path, std::ios::binary);
    if (!out) return makeError(Error::Code::IOError, "cannot write " + path.string());
    out << text;
    return out ? Status{} : makeError(Error::Code::IOError, "cannot write " + path.string());
}

Status writeBytes(const fs::path& path, const ByteBuffer& bytes) {
    std::error_code ec;
    fs::create_directories(path.parent_path(), ec);
    std::ofstream out(path, std::ios::binary);
    out.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
    return out ? Status{} : makeError(Error::Code::IOError, "cannot write " + path.string());
}

/// XML text / attribute escaping.
std::string xml(const std::string& s) {
    std::string out;
    for (char c : s) {
        switch (c) {
            case '&': out += "&amp;"; break;
            case '<': out += "&lt;"; break;
            case '>': out += "&gt;"; break;
            case '"': out += "&quot;"; break;
            case '\'': out += "\\'"; break;  // Android string resources
            default: out += c;
        }
    }
    return out;
}

/// Groovy single-quoted string.
std::string groovy(const std::string& s) {
    std::string out = "'";
    for (char c : s) {
        if (c == '\\' || c == '\'') out += '\\';
        out += c;
    }
    return out + "'";
}

std::string quoteArg(const std::string& s) {
#if defined(_WIN32)
    return "\"" + s + "\"";
#else
    std::string out = "'";
    for (char c : s) {
        if (c == '\'') out += "'\\''";
        else out += c;
    }
    return out + "'";
#endif
}

void setEnv(const char* name, const std::string& value) {
#if defined(_WIN32)
    _putenv_s(name, value.c_str());
#else
    if (value.empty()) unsetenv(name);
    else setenv(name, value.c_str(), 1);
#endif
}

/// Runs a command, forwarding its output (stdout + stderr) line by line. Returns the exit code.
int runProcess(const std::string& command, const std::function<void(const std::string&)>& log, std::string& transcript) {
#if defined(_WIN32)
    FILE* pipe = _popen((command + " 2>&1").c_str(), "r");
#else
    FILE* pipe = popen((command + " 2>&1").c_str(), "r");
#endif
    if (!pipe) return -1;
    char buffer[4096];
    std::string line;
    while (std::fgets(buffer, sizeof buffer, pipe)) {
        line += buffer;
        if (!line.empty() && line.back() == '\n') {
            line.pop_back();
            if (log) log(line);
            transcript += line + "\n";
            line.clear();
        }
    }
    if (!line.empty()) {
        if (log) log(line);
        transcript += line + "\n";
    }
#if defined(_WIN32)
    return _pclose(pipe);
#else
    const int status = pclose(pipe);
    return WIFEXITED(status) ? WEXITSTATUS(status) : -1;
#endif
}

const char* activityOrientation(ScreenOrientation o) {
    switch (o) {
        case ScreenOrientation::Landscape: return "sensorLandscape";
        case ScreenOrientation::Portrait: return "sensorPortrait";
        case ScreenOrientation::Sensor: return "fullSensor";
    }
    return "sensorLandscape";
}

fs::path keystorePath(const Project& project) {
    const fs::path p = project.settings().exportSettings.keystorePath;
    if (p.empty() || p.is_absolute()) return p;
    return project.root() / p;
}

} // namespace

// ------------------------------------------------------------------ tools
AndroidToolchain findAndroidToolchain(const fs::path& editorDirectory) {
    AndroidToolchain t;
    std::error_code ec;
    for (const std::string& candidate : {env("ANDROID_HOME"), env("ANDROID_SDK_ROOT")})
        if (!candidate.empty() && fs::is_directory(candidate, ec)) {
            t.sdk = candidate;
            break;
        }
    if (t.sdk.empty()) {
        const std::string home = env("HOME");
        for (const fs::path& p : {fs::path(home) / "Android" / "Sdk", fs::path(home) / "Library" / "Android" / "sdk",
                                  fs::path(env("LOCALAPPDATA")) / "Android" / "Sdk"})
            if (!p.empty() && fs::is_directory(p / "platforms", ec)) {
                t.sdk = p;
                break;
            }
    }
    t.gradle = env("AGR_GRADLE").empty() ? findInPath("gradle") : fs::path(env("AGR_GRADLE"));
    for (const fs::path& p : {fs::path(env("AGR_EXPORT_TEMPLATES")), editorDirectory / "export_templates",
                              editorDirectory / ".." / ".." / "export_templates"})
        if (!p.empty() && fs::is_directory(p / "android", ec)) {
            t.templates = fs::weakly_canonical(p, ec);
            break;
        }
    const std::string javaHome = env("JAVA_HOME");
    for (const std::string& suffix : kExeSuffixes)
        if (!javaHome.empty() && fs::is_regular_file(fs::path(javaHome) / "bin" / ("keytool" + suffix), ec))
            t.keytool = fs::path(javaHome) / "bin" / ("keytool" + suffix);
    if (t.keytool.empty()) t.keytool = findInPath("keytool");
    return t;
}

std::vector<std::string> checkAndroidExport(const Project& project, const AndroidToolchain& tools) {
    std::vector<std::string> problems;
    std::error_code ec;
    const ExportSettings& x = project.settings().exportSettings;
    if (tools.sdk.empty()) problems.push_back("Android SDK not found: set ANDROID_HOME");
    else if (!fs::is_regular_file(tools.sdk / "platforms" / ("android-" + std::to_string(x.targetSdk)) / "android.jar", ec))
        problems.push_back("Android SDK platform " + std::to_string(x.targetSdk) + " is not installed (sdkmanager \"platforms;android-" +
                           std::to_string(x.targetSdk) + "\")");
    if (tools.gradle.empty()) problems.push_back("Gradle not found: put gradle in PATH or set AGR_GRADLE");
    if (tools.templates.empty()) problems.push_back("Android export templates not found (tools/build_android_templates.sh)");
    else {
        if (!fs::is_regular_file(tools.templates / "android" / "java" / "org" / "libsdl" / "app" / "SDLActivity.java", ec))
            problems.push_back("export templates: SDL Java sources missing");
        for (const std::string& abi : x.abis)
            if (!fs::is_regular_file(tools.templates / "android" / abi / "libmain.so", ec))
                problems.push_back("export templates: no player for ABI " + abi);
    }
    if (x.abis.empty()) problems.push_back("no Android ABI selected");
    const GameManifest m = gameManifest(project);
    if (!GameManifest::validPackageId(m.packageId)) problems.push_back("invalid application id '" + m.packageId + "' (e.g. com.studio.game)");
    if (x.versionCode == 0) problems.push_back("the version code must be at least 1");
    if (x.minSdk < 26) problems.push_back("minimum Android version below 8.0 (API 26) is not supported");
    if (x.targetSdk < x.minSdk) problems.push_back("target SDK lower than the minimum SDK");
    return problems;
}

Result<ByteBuffer> makeLauncherIcon(std::span<const u8> sourceImage, u32 size) {
    auto img = decodeImage(sourceImage, "icon");
    if (!img) return img.error();
    const Image& src = img.value();
    std::vector<u8> out(static_cast<usize>(size) * size * 4, 0);
    // Fit inside 84 % of the square (launcher masks), centered, bilinear sampling.
    const float box = static_cast<float>(size) * 0.84f;
    const float k = std::min(box / static_cast<float>(src.width), box / static_cast<float>(src.height));
    const float w = static_cast<float>(src.width) * k, h = static_cast<float>(src.height) * k;
    const float x0 = (static_cast<float>(size) - w) * 0.5f, y0 = (static_cast<float>(size) - h) * 0.5f;
    auto texel = [&](int x, int y, int c) {
        x = std::clamp(x, 0, static_cast<int>(src.width) - 1);
        y = std::clamp(y, 0, static_cast<int>(src.height) - 1);
        return static_cast<float>(src.pixels[(static_cast<usize>(y) * src.width + static_cast<usize>(x)) * 4 + static_cast<usize>(c)]);
    };
    for (u32 y = 0; y < size; ++y)
        for (u32 x = 0; x < size; ++x) {
            const float u = (static_cast<float>(x) + 0.5f - x0) / k - 0.5f, v = (static_cast<float>(y) + 0.5f - y0) / k - 0.5f;
            if (u < -0.5f || v < -0.5f || u > static_cast<float>(src.width) - 0.5f || v > static_cast<float>(src.height) - 0.5f) continue;
            const int ix = static_cast<int>(std::floor(u)), iy = static_cast<int>(std::floor(v));
            const float fx = u - static_cast<float>(ix), fy = v - static_cast<float>(iy);
            for (int c = 0; c < 4; ++c) {
                const float a = texel(ix, iy, c) * (1 - fx) + texel(ix + 1, iy, c) * fx;
                const float b = texel(ix, iy + 1, c) * (1 - fx) + texel(ix + 1, iy + 1, c) * fx;
                out[(static_cast<usize>(y) * size + x) * 4 + static_cast<usize>(c)] = static_cast<u8>(std::lround(std::clamp(a * (1 - fy) + b * fy, 0.0f, 255.0f)));
            }
        }
    return encodePng(size, size, out);
}

// --------------------------------------------------------- Gradle project
Result<ExportReport> generateAndroidProject(const Project& project, const AndroidToolchain& tools, const fs::path& dir) {
    if (const auto problems = checkAndroidExport(project, tools); !problems.empty()) {
        std::string all;
        for (const std::string& p : problems) all += (all.empty() ? "" : "; ") + p;
        return makeError(Error::Code::Unsupported, all);
    }
    if (Status s = prepareExportFolder(dir); !s) return s.error();
    const ExportSettings& x = project.settings().exportSettings;
    const GameManifest m = gameManifest(project);
    const fs::path app = dir / "app";
    const fs::path main = app / "src" / "main";

    // Game data in the APK assets (read by the player through SDL).
    auto data = packageGameData(project, main / "assets" / "game");
    if (!data) return data.error();
    ExportReport report = std::move(data.value());
    report.output = dir;
    std::error_code ec;
    fs::remove(main / "assets" / "game" / ".agr-export", ec);

    // Native libraries and SDL's Java sources from the export templates.
    for (const std::string& abi : x.abis)
        for (const char* lib : {"libmain.so", "libSDL3.so"}) {
            fs::create_directories(main / "jniLibs" / abi, ec);
            fs::copy_file(tools.templates / "android" / abi / lib, main / "jniLibs" / abi / lib, fs::copy_options::overwrite_existing, ec);
            if (ec) return makeError(Error::Code::IOError, std::string("cannot copy ") + lib + " (" + abi + "): " + ec.message());
        }
    fs::create_directories(main / "java", ec);
    fs::copy(tools.templates / "android" / "java" / "org", main / "java" / "org", fs::copy_options::recursive | fs::copy_options::overwrite_existing, ec);
    if (ec) return makeError(Error::Code::IOError, "cannot copy the SDL Java sources: " + ec.message());

    // Activity.
    std::string pkgPath = m.packageId;
    std::replace(pkgPath.begin(), pkgPath.end(), '.', '/');
    if (Status s = writeText(main / "java" / pkgPath / "AgrActivity.java",
                             "// Generated by AGR ENGINE. The game runs in libmain.so (AGR player), started by SDL.\n"
                             "package " + m.packageId + ";\n\n"
                             "public class AgrActivity extends org.libsdl.app.SDLActivity {\n"
                             "    @Override\n"
                             "    protected String[] getLibraries() {\n"
                             "        return new String[] {\"SDL3\", \"main\"};\n"
                             "    }\n"
                             "}\n");
        !s)
        return s.error();

    // Manifest.
    std::string manifest =
        "<?xml version=\"1.0\" encoding=\"utf-8\"?>\n"
        "<!-- Generated by AGR ENGINE: changes are overwritten by the next export. -->\n"
        "<manifest xmlns:android=\"http://schemas.android.com/apk/res/android\" android:installLocation=\"auto\">\n"
        "    <uses-feature android:glEsVersion=\"0x00030000\" android:required=\"true\" />\n"
        "    <uses-feature android:name=\"android.hardware.touchscreen\" android:required=\"false\" />\n"
        "    <uses-feature android:name=\"android.hardware.gamepad\" android:required=\"false\" />\n"
        "    <uses-feature android:name=\"android.hardware.type.pc\" android:required=\"false\" />\n";
    if (x.vibration) manifest += "    <uses-permission android:name=\"android.permission.VIBRATE\" />\n";
    if (x.network) manifest += "    <uses-permission android:name=\"android.permission.INTERNET\" />\n";
    manifest +=
        "    <application android:label=\"@string/app_name\" android:icon=\"@mipmap/ic_launcher\" android:allowBackup=\"true\"\n"
        "        android:isGame=\"true\" android:appCategory=\"game\" android:hardwareAccelerated=\"true\"\n"
        "        android:theme=\"@android:style/Theme.NoTitleBar.Fullscreen\">\n"
        "        <meta-data android:name=\"SDL_ENV.SDL_ANDROID_TRAP_BACK_BUTTON\" android:value=\"1\" />\n"
        "        <activity android:name=\".AgrActivity\" android:label=\"@string/app_name\" android:exported=\"true\"\n"
        "            android:alwaysRetainTaskState=\"true\" android:launchMode=\"singleInstance\"\n"
        "            android:screenOrientation=\"" + std::string(activityOrientation(x.orientation)) + "\"\n"
        "            android:configChanges=\"layoutDirection|locale|grammaticalGender|fontScale|fontWeightAdjustment|orientation|uiMode|screenLayout|screenSize|smallestScreenSize|keyboard|keyboardHidden|navigation\"\n"
        "            android:preferMinimalPostProcessing=\"true\">\n"
        "            <intent-filter>\n"
        "                <action android:name=\"android.intent.action.MAIN\" />\n"
        "                <category android:name=\"android.intent.category.LAUNCHER\" />\n"
        "            </intent-filter>\n"
        "        </activity>\n"
        "    </application>\n"
        "</manifest>\n";
    if (Status s = writeText(main / "AndroidManifest.xml", manifest); !s) return s.error();
    if (Status s = writeText(main / "res" / "values" / "strings.xml",
                             "<?xml version=\"1.0\" encoding=\"utf-8\"?>\n<resources>\n    <string name=\"app_name\">" + xml(m.name) +
                                 "</string>\n</resources>\n");
        !s)
        return s.error();

    // Launcher icons from the project's icon (default: the AGR mark of the templates).
    {
        fs::path iconSource = project.assetsDirectory() / "icon.png";
        if (!fs::is_regular_file(iconSource, ec)) iconSource = project.assetsDirectory() / "textures" / "agr_mark.png";
        ByteBuffer bytes;
        if (std::ifstream in(iconSource, std::ios::binary | std::ios::ate); in) {
            bytes.resize(static_cast<usize>(in.tellg()));
            in.seekg(0);
            in.read(reinterpret_cast<char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
        }
        static const std::pair<const char*, u32> densities[] = {{"mdpi", 48}, {"hdpi", 72}, {"xhdpi", 96}, {"xxhdpi", 144}, {"xxxhdpi", 192}};
        for (const auto& [density, size] : densities) {
            auto png = bytes.empty() ? Result<ByteBuffer>(makeError(Error::Code::NotFound, "no icon image")) : makeLauncherIcon(bytes, size);
            if (!png) {
                report.warnings.push_back("launcher icon: " + png.error().message + " (assets/icon.png)");
                break;
            }
            if (Status s = writeBytes(main / "res" / (std::string("mipmap-") + density) / "ic_launcher.png", png.value()); !s) return s.error();
        }
    }

    // Gradle files.
    std::string abis;
    for (const std::string& a : x.abis) abis += (abis.empty() ? "" : ", ") + groovy(a);
    const fs::path keystore = keystorePath(project);
    std::string gradle =
        "// Generated by AGR ENGINE: changes are overwritten by the next export.\n"
        "plugins {\n    id 'com.android.application'\n}\n\n"
        "android {\n"
        "    namespace " + groovy(m.packageId) + "\n"
        "    compileSdk " + std::to_string(x.targetSdk) + "\n"
        "    defaultConfig {\n"
        "        applicationId " + groovy(m.packageId) + "\n"
        "        minSdk " + std::to_string(x.minSdk) + "\n"
        "        targetSdk " + std::to_string(x.targetSdk) + "\n"
        "        versionCode " + std::to_string(x.versionCode) + "\n"
        "        versionName " + groovy(x.version) + "\n"
        "        ndk { abiFilters " + abis + " }\n"
        "    }\n";
    if (!keystore.empty()) {
        gradle += "    signingConfigs {\n"
                  "        release {\n"
                  "            storeFile file(" + groovy(keystore.generic_string()) + ")\n"
                  "            // Passwords come from the environment (asked by the editor, never stored).\n"
                  "            storePassword System.getenv('AGR_STORE_PASSWORD')\n"
                  "            keyAlias " + groovy(x.keyAlias) + "\n"
                  "            keyPassword System.getenv('AGR_KEY_PASSWORD')\n"
                  "        }\n"
                  "    }\n";
    }
    gradle += "    buildTypes {\n"
              "        release {\n"
              "            minifyEnabled false\n";
    if (!keystore.empty()) gradle += "            signingConfig signingConfigs.release\n";
    gradle += "        }\n"
              "    }\n"
              "    compileOptions {\n"
              "        sourceCompatibility JavaVersion.VERSION_1_8\n"
              "        targetCompatibility JavaVersion.VERSION_1_8\n"
              "    }\n"
              "    lint { abortOnError false; checkReleaseBuilds false }\n"
              "}\n";
    if (Status s = writeText(app / "build.gradle", gradle); !s) return s.error();
    if (Status s = writeText(dir / "build.gradle",
                             "// Generated by AGR ENGINE.\nplugins {\n    id 'com.android.application' version '" + std::string(kAgpVersion) +
                                 "' apply false\n}\n");
        !s)
        return s.error();
    if (Status s = writeText(dir / "settings.gradle",
                             "// Generated by AGR ENGINE.\n"
                             "pluginManagement {\n    repositories {\n        google()\n        mavenCentral()\n        gradlePluginPortal()\n    }\n}\n"
                             "dependencyResolutionManagement {\n    repositories {\n        google()\n        mavenCentral()\n    }\n}\n"
                             "rootProject.name = " + groovy(GameManifest::safeName(m.name)) + "\ninclude ':app'\n");
        !s)
        return s.error();
    if (Status s = writeText(dir / "gradle.properties", "org.gradle.jvmargs=-Xmx2048m -Dfile.encoding=UTF-8\nandroid.useAndroidX=false\n"); !s)
        return s.error();
    std::string sdk = tools.sdk.generic_string();
    if (Status s = writeText(dir / "local.properties", "sdk.dir=" + sdk + "\n"); !s) return s.error();
    AGR_LOG_INFO("Export", "Android project generated in {}", dir.string());
    return report;
}

// ------------------------------------------------------------------ build
Result<ExportReport> buildAndroid(const Project& project, const AndroidToolchain& tools, const fs::path& outputDir, const AndroidBuildOptions& options,
                                  const std::function<void(const std::string&)>& log) {
    const bool release = options.kind != AndroidBuildKind::DebugApk;
    const ExportSettings& x = project.settings().exportSettings;
    std::error_code ec;
    if (release) {
        const fs::path ks = keystorePath(project);
        if (ks.empty() || x.keyAlias.empty()) return makeError(Error::Code::InvalidArgument, "release builds need a keystore and a key alias");
        if (!fs::is_regular_file(ks, ec)) return makeError(Error::Code::NotFound, "keystore not found: " + ks.string());
        if (options.storePassword.empty()) return makeError(Error::Code::InvalidArgument, "the keystore password is required");
    }
    fs::create_directories(outputDir, ec);
    auto generated = generateAndroidProject(project, tools, outputDir / "gradle");
    if (!generated) return generated.error();
    ExportReport report = std::move(generated.value());

    const char* task = options.kind == AndroidBuildKind::DebugApk ? "assembleDebug" : options.kind == AndroidBuildKind::ReleaseApk ? "assembleRelease" : "bundleRelease";
    const std::string command = quoteArg(tools.gradle.string()) + " --no-daemon --console=plain -p " + quoteArg((outputDir / "gradle").string()) + " " + task;
    if (log) log("> " + command);
    setEnv("AGR_STORE_PASSWORD", options.storePassword);
    setEnv("AGR_KEY_PASSWORD", options.keyPassword.empty() ? options.storePassword : options.keyPassword);
    const int code = runProcess(command, log, report.log);
    setEnv("AGR_STORE_PASSWORD", "");
    setEnv("AGR_KEY_PASSWORD", "");
    if (code != 0) return makeError(Error::Code::Unknown, "Gradle failed (exit code " + std::to_string(code) + "): see the export log");

    const fs::path outputs = outputDir / "gradle" / "app" / "build" / "outputs";
    const fs::path built = options.kind == AndroidBuildKind::DebugApk     ? outputs / "apk" / "debug" / "app-debug.apk"
                           : options.kind == AndroidBuildKind::ReleaseApk ? outputs / "apk" / "release" / "app-release.apk"
                                                                          : outputs / "bundle" / "release" / "app-release.aab";
    if (!fs::is_regular_file(built, ec)) return makeError(Error::Code::NotFound, "Gradle succeeded but produced no " + built.filename().string());
    const std::string base = GameManifest::safeName(project.settings().name) + "-" + x.version + (release ? "" : "-debug");
    const fs::path final = outputDir / (base + built.extension().string());
    fs::copy_file(built, final, fs::copy_options::overwrite_existing, ec);
    if (ec) return makeError(Error::Code::IOError, "cannot copy " + built.string() + ": " + ec.message());
    report.output = final;
    AGR_LOG_INFO("Export", "Android build: {} ({:.1f} MB)", final.string(), static_cast<double>(fs::file_size(final, ec)) / (1024.0 * 1024.0));
    return report;
}

Status createKeystore(const AndroidToolchain& tools, const fs::path& path, const std::string& alias, const std::string& password,
                      const std::string& owner) {
    std::error_code ec;
    if (tools.keytool.empty()) return makeError(Error::Code::NotFound, "keytool not found (install a JDK or set JAVA_HOME)");
    if (fs::exists(path, ec)) return makeError(Error::Code::AlreadyExists, "a keystore already exists there: " + path.string());
    if (password.size() < 6) return makeError(Error::Code::InvalidArgument, "the password needs at least 6 characters");
    if (alias.empty()) return makeError(Error::Code::InvalidArgument, "the key alias is empty");
    fs::create_directories(path.parent_path(), ec);
    std::string dname = "CN=" + (owner.empty() ? std::string("AGR Game") : owner);
    dname.erase(std::remove_if(dname.begin(), dname.end(), [](char c) { return c == ',' || c == '"' || c == '\''; }), dname.end());
    // Passwords through the environment, not the command line (visible to other processes).
    setEnv("AGR_KS_PASSWORD", password);
    std::string transcript;
    const int code = runProcess(quoteArg(tools.keytool.string()) + " -genkeypair -noprompt -keystore " + quoteArg(path.string()) + " -alias " +
                                    quoteArg(alias) + " -keyalg RSA -keysize 2048 -validity 10950 -storetype PKCS12 -storepass:env AGR_KS_PASSWORD"
                                    " -keypass:env AGR_KS_PASSWORD -dname " + quoteArg(dname),
                                {}, transcript);
    setEnv("AGR_KS_PASSWORD", "");
    if (code != 0 || !fs::is_regular_file(path, ec)) return makeError(Error::Code::Unknown, "keytool failed: " + transcript);
    return {};
}

} // namespace agr::editor
