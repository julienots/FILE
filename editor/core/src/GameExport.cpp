#include "agr/editor/GameExport.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdlib>
#include <fstream>

#include <cstdio>
#include <optional>

#include <nlohmann/json.hpp>

#include "agr/core/Log.hpp"
#include "agr/scene/SceneSerializer.hpp"

namespace agr::editor {

namespace fs = std::filesystem;

namespace {

constexpr const char* kMarker = ".agr-export";  // marks folders written by an export (safe to replace)

std::string lower(std::string s) {
    for (char& c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return s;
}

} // namespace

Status prepareExportFolder(const fs::path& dir) {
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

namespace {

Status copyFile(const fs::path& from, const fs::path& to, ExportReport& report) {
    std::error_code ec;
    fs::create_directories(to.parent_path(), ec);
    fs::copy_file(from, to, fs::copy_options::overwrite_existing, ec);
    if (ec) return makeError(Error::Code::IOError, "cannot copy '" + from.string() + "': " + ec.message());
    ++report.files;
    report.bytes += fs::file_size(to, ec);
    return {};
}

Status writeFile(const fs::path& to, const std::vector<u8>& bytes, ExportReport& report) {
    std::error_code ec;
    fs::create_directories(to.parent_path(), ec);
    FILE* f = std::fopen(to.string().c_str(), "wb");
    if (!f) return makeError(Error::Code::IOError, "cannot write '" + to.string() + "'");
    const bool ok = std::fwrite(bytes.data(), 1, bytes.size(), f) == bytes.size();
    std::fclose(f);
    if (!ok) return makeError(Error::Code::IOError, "cannot write '" + to.string() + "'");
    ++report.files;
    report.bytes += bytes.size();
    return {};
}

/// The scene in the binary format, or nullopt when the conversion would not be exact
/// (component unknown to the editor, prefab not found, hand-edited file...): the
/// binary file, read back, must give the very same document as the JSON on disk.
std::optional<std::vector<u8>> binaryScene(const fs::path& file) {
    std::string text;
    if (FILE* f = std::fopen(file.string().c_str(), "rb")) {
        char buf[65536];
        for (usize n; (n = std::fread(buf, 1, sizeof buf, f)) > 0;) text.append(buf, n);
        std::fclose(f);
    } else {
        return std::nullopt;
    }
    if (SceneSerializer::isBinary(text)) return std::vector<u8>(text.begin(), text.end());
    const auto original = nlohmann::ordered_json::parse(text, nullptr, false);
    if (original.is_discarded()) return std::nullopt;
    Scene source;
    if (!SceneSerializer::fromJson(source, original)) return std::nullopt;
    std::vector<u8> bin = SceneSerializer::toBinary(source);
    Scene check;
    if (!SceneSerializer::fromBinary(check, bin) || SceneSerializer::toJson(check) != original) {
        AGR_LOG_INFO("Export", "{}: kept as text (not converted to binary losslessly)", file.filename().string());
        return std::nullopt;
    }
    return bin;
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
    m.splashImage = x.splashImage;
    m.splashColor = {x.splashColor.x, x.splashColor.y, x.splashColor.z, x.splashColor.w};
    m.splashSeconds = x.splashSeconds;
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
    if (Status s = prepareExportFolder(gameDir); !s) return s.error();

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
        if (std::optional<std::vector<u8>> bin = binaryScene(project.root() / scene)) {
            if (Status s = writeFile(gameDir / scene, *bin, report); !s) return s.error();
            ++report.binaryScenes;
            continue;
        }
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
    AGR_LOG_INFO("Export", "game data: {} files, {:.1f} MB, {} binary scene(s) -> {}", report.files,
                 static_cast<double>(report.bytes) / (1024.0 * 1024.0), report.binaryScenes, gameDir.string());
    return report;
}

Result<ExportReport> exportDesktop(const Project& project, const fs::path& outputDir, const fs::path& playerExecutable) {
    std::error_code ec;
    if (!fs::is_regular_file(playerExecutable, ec))
        return makeError(Error::Code::NotFound, "game player not found: " + playerExecutable.string());
    const fs::path dir = outputDir / GameManifest::safeName(project.settings().name);
    if (Status s = prepareExportFolder(dir); !s) return s.error();
    auto data = packageGameData(project, dir / "game");
    if (!data) return data.error();
    ExportReport report = std::move(data.value());
    report.output = dir;
    const fs::path exe = dir / (GameManifest::safeName(project.settings().name) + playerExecutable.extension().string());
    fs::copy_file(playerExecutable, exe, fs::copy_options::overwrite_existing, ec);
    if (ec) return makeError(Error::Code::IOError, "cannot copy the player: " + ec.message());
    fs::permissions(exe, fs::perms::owner_exec | fs::perms::group_exec | fs::perms::others_exec, fs::perm_options::add, ec);
    // Icon next to the executable (assets/icon.png, else the AGR mark), and on Linux a desktop entry.
    fs::path icon = project.assetsDirectory() / "icon.png";
    if (!fs::is_regular_file(icon, ec)) icon = project.assetsDirectory() / "textures" / "agr_mark.png";
    if (fs::is_regular_file(icon, ec)) fs::copy_file(icon, dir / "icon.png", fs::copy_options::overwrite_existing, ec);
    if (playerExecutable.extension() != ".exe") {
        const std::string name = project.settings().name;
        std::string entry = "[Desktop Entry]\nType=Application\nName=" + name + "\nExec=sh -c 'cd \"$(dirname \"%k\")\" && ./" + exe.filename().string() +
                            "'\nIcon=icon\nCategories=Game;\nTerminal=false\n";
        if (fs::exists(dir / "icon.png", ec)) entry += "# Icon: icon.png next to this file (copy it to ~/.local/share/icons to show it in menus)\n";
        if (std::ofstream out(dir / (exe.stem().string() + ".desktop")); out) out << entry;
        fs::permissions(dir / (exe.stem().string() + ".desktop"), fs::perms::owner_exec, fs::perm_options::add, ec);
        report.files += 1;
    }
    AGR_LOG_INFO("Export", "desktop build: {}", exe.string());
    if (project.settings().exportSettings.zipPackage) {
        const fs::path zip = outputDir / (dir.filename().string() + (playerExecutable.extension() == ".exe" ? "-windows" : "") + ".zip");
        if (Status s = zipFolder(dir, zip); !s) report.warnings.push_back("zip: " + s.error().message);
        else AGR_LOG_INFO("Export", "archive: {}", zip.string());
    }
    return report;
}

Status zipFolder(const fs::path& dir, const fs::path& zipPath) {
    static const auto crcTable = [] {
        std::array<u32, 256> t{};
        for (u32 i = 0; i < 256; ++i) {
            u32 c = i;
            for (int k = 0; k < 8; ++k) c = (c & 1) ? 0xEDB88320u ^ (c >> 1) : c >> 1;
            t[i] = c;
        }
        return t;
    }();
    auto crc32 = [&](const std::string& data) {
        u32 c = 0xFFFFFFFFu;
        for (unsigned char b : data) c = crcTable[(c ^ b) & 0xFF] ^ (c >> 8);
        return c ^ 0xFFFFFFFFu;
    };
    std::string out, central;
    auto u16le = [](std::string& s, u32 v) { s += static_cast<char>(v & 0xFF), s += static_cast<char>((v >> 8) & 0xFF); };
    auto u32le = [&](std::string& s, u32 v) { u16le(s, v & 0xFFFF), u16le(s, v >> 16); };
    std::error_code ec;
    std::vector<fs::path> files;
    for (auto it = fs::recursive_directory_iterator(dir, ec); !ec && it != fs::recursive_directory_iterator(); it.increment(ec))
        if (it->is_regular_file(ec) && it->path().filename() != ".agr-export") files.push_back(it->path());  // not the export marker
    std::sort(files.begin(), files.end());
    u32 count = 0;
    for (const fs::path& f : files) {
        std::ifstream in(f, std::ios::binary);
        const std::string data((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
        const std::string name = fs::relative(f, dir.parent_path(), ec).generic_string();
        if (data.size() > 0xFFFFFFFEu) return makeError(Error::Code::Unsupported, "zip: file larger than 4 GB: " + name);
        const u32 crc = crc32(data), offset = static_cast<u32>(out.size());
        const fs::perms perms = fs::status(f, ec).permissions();
        const u32 mode = 0100000u | (static_cast<u32>(perms) & 0777u);  // regular file + permissions (executables stay executable)
        // Local header.
        u32le(out, 0x04034b50u), u16le(out, 20), u16le(out, 0x0800), u16le(out, 0), u16le(out, 0), u16le(out, 0x21);
        u32le(out, crc), u32le(out, static_cast<u32>(data.size())), u32le(out, static_cast<u32>(data.size()));
        u16le(out, static_cast<u32>(name.size())), u16le(out, 0);
        out += name;
        out += data;
        // Central directory entry.
        u32le(central, 0x02014b50u), u16le(central, 0x0314), u16le(central, 20), u16le(central, 0x0800), u16le(central, 0), u16le(central, 0),
            u16le(central, 0x21);
        u32le(central, crc), u32le(central, static_cast<u32>(data.size())), u32le(central, static_cast<u32>(data.size()));
        u16le(central, static_cast<u32>(name.size())), u16le(central, 0), u16le(central, 0), u16le(central, 0), u16le(central, 0);
        u32le(central, mode << 16), u32le(central, offset);
        central += name;
        ++count;
    }
    if (out.size() > 0xFFFFFFFEu) return makeError(Error::Code::Unsupported, "zip: archive larger than 4 GB");
    const u32 centralOffset = static_cast<u32>(out.size());
    out += central;
    u32le(out, 0x06054b50u), u16le(out, 0), u16le(out, 0), u16le(out, count), u16le(out, count);
    u32le(out, static_cast<u32>(central.size())), u32le(out, centralOffset), u16le(out, 0);
    std::ofstream zip(zipPath, std::ios::binary);
    if (!zip) return makeError(Error::Code::IOError, "cannot write " + zipPath.string());
    zip.write(out.data(), static_cast<std::streamsize>(out.size()));
    return {};
}

fs::path findWindowsPlayer(const fs::path& editorDirectory) {
    std::error_code ec;
    std::vector<fs::path> candidates;
    if (const char* t = std::getenv("AGR_EXPORT_TEMPLATES"); t && *t) candidates.push_back(fs::path(t) / "windows-x64" / "agr_player.exe");
    candidates.push_back(editorDirectory / "export_templates" / "windows-x64" / "agr_player.exe");
    candidates.push_back(editorDirectory / ".." / ".." / "export_templates" / "windows-x64" / "agr_player.exe");
    candidates.push_back(editorDirectory / ".." / ".." / "windows-x64" / "bin" / "agr_player.exe");  // build/<preset>/bin -> build/windows-x64/bin
    candidates.push_back(editorDirectory / "agr_player.exe");
    for (const fs::path& p : candidates)
        if (fs::is_regular_file(p, ec)) return fs::weakly_canonical(p, ec);
    return {};
}

} // namespace agr::editor
