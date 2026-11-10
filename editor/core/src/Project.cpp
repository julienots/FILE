#include "agr/editor/Project.hpp"

#include <algorithm>
#include <fstream>
#include <sstream>

#include <nlohmann/json.hpp>

#include "agr/core/Log.hpp"
#include "agr/editor/SceneTemplates.hpp"
#include "agr/scene/SceneSerializer.hpp"

namespace agr::editor {

namespace fs = std::filesystem;
using json = nlohmann::ordered_json;

const char* enumName(ProjectMode mode) {
    switch (mode) {
        case ProjectMode::Mode2D: return "2d";
        case ProjectMode::Mode25D: return "2.5d";
        case ProjectMode::Mode3D: return "3d";
    }
    return "3d";
}

Result<ProjectMode> parseProjectMode(std::string_view text) {
    if (text == "2d") return ProjectMode::Mode2D;
    if (text == "2.5d") return ProjectMode::Mode25D;
    if (text == "3d") return ProjectMode::Mode3D;
    return makeError(Error::Code::ParseError, "unknown project mode '" + std::string(text) + "'");
}

namespace {

Status writeFile(const fs::path& path, const std::string& text) {
    std::error_code ec;
    fs::create_directories(path.parent_path(), ec);
    const fs::path tmp = path.string() + ".tmp";
    {
        std::ofstream out(tmp, std::ios::binary | std::ios::trunc);
        if (!out) return makeError(Error::Code::IOError, "cannot write '" + tmp.string() + "'");
        out << text;
        if (!out) return makeError(Error::Code::IOError, "write failed for '" + tmp.string() + "'");
    }
    fs::rename(tmp, path, ec);
    if (ec) return makeError(Error::Code::IOError, "rename failed: " + ec.message());
    return {};
}

Result<std::string> readFile(const fs::path& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) return makeError(Error::Code::NotFound, "cannot open '" + path.string() + "'");
    std::ostringstream ss;
    ss << in.rdbuf();
    return ss.str();
}

} // namespace

Result<Project> Project::create(const fs::path& folder, const ProjectSettings& settings, SceneTemplate tmpl,
                                const fs::path& templateAssetsDir) {
    std::error_code ec;
    if (settings.name.empty()) return makeError(Error::Code::InvalidArgument, "project name is empty");
    if (fs::exists(folder, ec) && !fs::is_empty(folder, ec))
        return makeError(Error::Code::AlreadyExists, "folder '" + folder.string() + "' already exists and is not empty");
    fs::create_directories(folder / "assets" / "textures", ec);
    fs::create_directories(folder / "scenes", ec);
    if (ec) return makeError(Error::Code::IOError, "cannot create project folders: " + ec.message());

    // Template textures (original AGR assets) so the startup scene is not empty.
    if (fs::is_directory(templateAssetsDir, ec)) {
        for (const auto& entry : fs::directory_iterator(templateAssetsDir, ec)) {
            if (!entry.is_regular_file()) continue;
            fs::copy_file(entry.path(), folder / "assets" / "textures" / entry.path().filename(),
                          fs::copy_options::overwrite_existing, ec);
        }
    } else {
        AGR_LOG_WARN("Editor", "template assets not found in '{}'", templateAssetsDir.string());
    }

    Project p;
    p.m_root = folder;
    p.m_settings = settings;
    if (Status s = p.save(); !s) return s.error();

    Scene scene(settings.name);
    buildSceneTemplate(scene, tmpl);
    if (Status s = writeFile(folder / settings.startupScene, SceneSerializer::toString(scene)); !s) return s.error();
    AGR_LOG_INFO("Editor", "project '{}' created in {}", settings.name, folder.string());
    return p;
}

Result<Project> Project::open(const fs::path& folder) {
    auto text = readFile(folder / kFileName);
    if (!text) return makeError(Error::Code::NotFound, "'" + folder.string() + "' is not an AGR project (no " + kFileName + ")");
    json j = json::parse(text.value(), nullptr, false);
    if (j.is_discarded() || !j.is_object() || j.value("format", "") != "agr.project")
        return makeError(Error::Code::ParseError, "invalid project file in '" + folder.string() + "'");
    Project p;
    p.m_root = folder;
    ProjectSettings& s = p.m_settings;
    s.name = j.value("name", s.name);
    if (auto mode = parseProjectMode(j.value("mode", std::string("3d")))) s.mode = mode.value();
    s.startupScene = j.value("startupScene", s.startupScene);
    s.graphicsProfile = j.value("graphicsProfile", s.graphicsProfile);
    s.engineVersion = j.value("engineVersion", std::string());
    s.targetDesktop = s.targetAndroid = false;
    if (j.contains("targets") && j["targets"].is_array()) {
        for (const auto& t : j["targets"]) {
            if (t == "desktop") s.targetDesktop = true;
            if (t == "android") s.targetAndroid = true;
        }
    }
    return p;
}

Status Project::save() const {
    json j;
    j["format"] = "agr.project";
    j["version"] = 1;
    j["name"] = m_settings.name;
    j["engineVersion"] = std::to_string(AGR_VERSION_MAJOR) + "." + std::to_string(AGR_VERSION_MINOR) + "." +
                         std::to_string(AGR_VERSION_PATCH);
    j["mode"] = enumName(m_settings.mode);
    json targets = json::array();
    if (m_settings.targetDesktop) targets.push_back("desktop");
    if (m_settings.targetAndroid) targets.push_back("android");
    j["targets"] = targets;
    j["startupScene"] = m_settings.startupScene;
    j["graphicsProfile"] = m_settings.graphicsProfile;
    return writeFile(m_root / kFileName, j.dump(2));
}

std::vector<std::string> Project::listScenes() const {
    std::vector<std::string> out;
    std::error_code ec;
    if (!fs::is_directory(m_root, ec)) return out;
    for (auto it = fs::recursive_directory_iterator(m_root, ec); it != fs::recursive_directory_iterator(); it.increment(ec)) {
        if (ec) break;
        if (it->is_regular_file() && it->path().extension() == ".agrscene")
            out.push_back(fs::relative(it->path(), m_root, ec).generic_string());
    }
    std::sort(out.begin(), out.end());
    return out;
}

void RecentProjects::touch(const std::string& path, usize max) {
    remove(path);
    paths.insert(paths.begin(), path);
    if (paths.size() > max) paths.resize(max);
}

void RecentProjects::remove(const std::string& path) { std::erase(paths, path); }

std::string RecentProjects::toJson() const {
    json j;
    j["recentProjects"] = paths;
    return j.dump(2);
}

RecentProjects RecentProjects::fromJson(std::string_view text) {
    RecentProjects r;
    json j = json::parse(text, nullptr, false);
    if (j.is_object() && j.contains("recentProjects") && j["recentProjects"].is_array())
        for (const auto& p : j["recentProjects"])
            if (p.is_string()) r.paths.push_back(p.get<std::string>());
    return r;
}

} // namespace agr::editor
