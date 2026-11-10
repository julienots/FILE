#include "agr/editor/GameExport.hpp"

#include <algorithm>
#include <cctype>

#include "agr/core/Log.hpp"

namespace agr::editor {

namespace fs = std::filesystem;

namespace {

constexpr const char* kMarker = ".agr-export";  // marks folders written by an export (safe to replace)

std::string lower(std::string s) {
    for (char& c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return s;
}

/// Prepares an output folder: creates it, or empties it if (and only if) a previous export wrote it.
Status prepareOutput(const fs::path& dir) {
    std::error_code ec;
    if (fs::exists(dir, ec)) {
        const bool empty = fs::is_directory(dir, ec) && fs::directory_iterator(dir, ec) == fs::directory_iterator();
        if (!empty) {
            if (!fs::exists(dir / kMarker, ec))
                return makeError(Error::Code::AlreadyExists,
                                 "'" + dir.string() + "' is not empty and was not created by an export: choose another folder");
            fs::remove_all(dir, ec);
            if (ec) return makeError(Error::Code::IOError, "cannot replace '" + dir.string() + "': " + ec.message());
        }
    }
    fs::create_directories(dir, ec);
    if (ec) return makeError(Error::Code::IOError, "cannot create '" + dir.string() + "': " + ec.message());
    if (FILE* f = std::fopen((dir / kMarker).string().c_str(), "wb")) {
        std::fputs("Written by the AGR ENGINE exporter; the next export replaces this folder.\n", f);
        std::fclose(f);
    }
    return {};
}

Status copyFile(const fs::path& from, const fs::path& to, ExportReport& report) {
    std::error_code ec;
    fs::create_directories(to.parent_path(), ec);
    fs::copy_file(from, to, fs::copy_options::overwrite_existing, ec);
    if (ec) return makeError(Error::Code::IOError, "cannot copy '" + from.string() + "': " + ec.message());
    ++report.files;
    report.bytes += fs::file_size(to, ec);
    return {};
}

bool hidden(const fs::path& p) {
    for (const auto& part : p)
        if (!part.empty() && part.string()[0] == '.') return true;
    return false;
}

} // namespace

std::string defaultPackageId(const ProjectSettings& settings) {
    auto segment = [](const std::string& s) {
        std::string out;
        for (char c : lower(s))
            if (std::isalnum(static_cast<unsigned char>(c))) out += c;
        if (out.empty() || !std::isalpha(static_cast<unsigned char>(out[0]))) out = "g" + out;
        return out;
    };
    return "com." + segment(settings.exportSettings.organization) + "." + segment(settings.name);
}

GameManifest gameManifest(const Project& project) {
    const ProjectSettings& s = project.settings();
    const ExportSettings& x = s.exportSettings;
    GameManifest m;
    m.name = s.name;
    m.organization = x.organization;
    m.version = x.version;
    m.versionCode = x.versionCode;
    m.packageId = x.packageId.empty() ? defaultPackageId(s) : x.packageId;
    m.startupScene = s.startupScene;
    m.graphicsProfile = s.graphicsProfile;
    m.orientation = x.orientation;
    m.targetFps = x.targetFps;
    m.backQuits = x.backQuits;
    m.vibration = x.vibration;
    m.keepScreenOn = x.keepScreenOn;
    m.engineVersion = std::to_string(AGR_VERSION_MAJOR) + "." + std::to_string(AGR_VERSION_MINOR) + "." + std::to_string(AGR_VERSION_PATCH);
    return m;
}

Result<ExportReport> packageGameData(const Project& project, const fs::path& gameDir) {
    ExportReport report;
    report.output = gameDir;
    const fs::path startup = project.root() / project.settings().startupScene;
    std::error_code ec;
    if (!fs::is_regular_file(startup, ec))
        return makeError(Error::Code::NotFound, "the startup scene '" + project.settings().startupScene + "' does not exist (save it first)");
    if (Status s = prepareOutput(gameDir); !s) return s.error();

    // Assets (hidden files and folders skipped).
    const fs::path assets = project.assetsDirectory();
    if (fs::is_directory(assets, ec)) {
        for (auto it = fs::recursive_directory_iterator(assets, ec); it != fs::recursive_directory_iterator(); it.increment(ec)) {
            if (ec) break;
            const fs::path rel = fs::relative(it->path(), assets, ec);
            if (hidden(rel)) {
                if (it->is_directory()) it.disable_recursion_pending();
                continue;
            }
            if (!it->is_regular_file()) continue;
            if (Status s = copyFile(it->path(), gameDir / "assets" / rel, report); !s) return s.error();
        }
    }
    // Scenes (wherever they are in the project, outside assets/ and hidden folders).
    for (const std::string& scene : project.listScenes()) {
        if (hidden(fs::path(scene))) continue;
        if (Status s = copyFile(project.root() / scene, gameDir / scene, report); !s) return s.error();
    }

    const GameManifest manifest = gameManifest(project);
    if (!GameManifest::validPackageId(manifest.packageId))
        report.warnings.push_back("invalid Android application id '" + manifest.packageId + "'");
    {
        const std::string text = manifest.toJson();
        FILE* f = std::fopen((gameDir / "game.json").string().c_str(), "wb");
        if (!f) return makeError(Error::Code::IOError, "cannot write game.json");
        std::fwrite(text.data(), 1, text.size(), f);
        std::fclose(f);
        ++report.files;
        report.bytes += text.size();
    }
    AGR_LOG_INFO("Export", "game data: {} files, {:.1f} MB -> {}", report.files, static_cast<double>(report.bytes) / (1024.0 * 1024.0),
                 gameDir.string());
    return report;
}

Result<ExportReport> exportDesktop(const Project& project, const fs::path& outputDir, const fs::path& playerExecutable) {
    std::error_code ec;
    if (!fs::is_regular_file(playerExecutable, ec))
        return makeError(Error::Code::NotFound, "game player not found: " + playerExecutable.string());
    const fs::path dir = outputDir / GameManifest::safeName(project.settings().name);
    if (Status s = prepareOutput(dir); !s) return s.error();
    auto data = packageGameData(project, dir / "game");
    if (!data) return data.error();
    ExportReport report = std::move(data.value());
    report.output = dir;
    const fs::path exe = dir / (GameManifest::safeName(project.settings().name) + playerExecutable.extension().string());
    fs::copy_file(playerExecutable, exe, fs::copy_options::overwrite_existing, ec);
    if (ec) return makeError(Error::Code::IOError, "cannot copy the player: " + ec.message());
    fs::permissions(exe, fs::perms::owner_exec | fs::perms::group_exec | fs::perms::others_exec, fs::perm_options::add, ec);
    AGR_LOG_INFO("Export", "desktop build: {}", exe.string());
    return report;
}

} // namespace agr::editor
