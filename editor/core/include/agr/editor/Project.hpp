#pragma once
#include <vector>

#include "agr/runtime/GameManifest.hpp"
#include <filesystem>
#include <string>
#include <vector>

#include "agr/core/Result.hpp"
#include "agr/core/Types.hpp"

namespace agr::editor {

enum class ProjectMode : u8 { Mode2D, Mode25D, Mode3D };
const char* enumName(ProjectMode mode);  ///< "2d", "2.5d", "3d"
Result<ProjectMode> parseProjectMode(std::string_view text);

/// Scene templates that exist today (game templates arrive in phase 9).
/// Scene templates: empty bases, showcases, and complete playable games (Phase 9, docs/TEMPLATES.md).
enum class SceneTemplate : u8 {
    Empty2D, Empty25D, Empty3D, Showcase2D, Showcase3D,
    GamePlatformer, GameRpg2D, GameTowerDefense, GameCards, GameFarm, GameIdle, GameTycoon, GameSurvival, GameRpg3D, GameStrategy,
};

/// Export settings (game identity, mobile behavior, Android build). Signing passwords are
/// never stored: they are asked for at export time.
struct ExportSettings {
    std::string organization = "Superessence Studio";
    std::string version = "1.0.0";
    u32 versionCode = 1;
    std::string packageId;  ///< Android application id; empty = derived from the organization and the name.
    ScreenOrientation orientation = ScreenOrientation::Landscape;
    u32 targetFps = 0;
    bool vibration = true;
    bool network = false;  ///< Android INTERNET permission (multiplayer: agr.net).
    bool keepScreenOn = true;
    bool backQuits = true;
    u32 minSdk = 26;     ///< Android 8.0
    u32 targetSdk = 35;
    std::vector<std::string> abis{"arm64-v8a"};
    std::string keystorePath;  ///< Release keystore (empty = debug signing only).
    std::string keyAlias;
};

struct ProjectSettings {
    std::string name = "New Project";
    ProjectMode mode = ProjectMode::Mode3D;
    bool targetDesktop = true;
    bool targetAndroid = false;
    std::string startupScene = "scenes/main.agrscene";
    std::string graphicsProfile = "auto";
    std::string engineVersion;  ///< Written on save.
    ExportSettings exportSettings;
};

/// A project folder:
///   project.agrproject   settings (JSON)
///   assets/              game data, mounted as assets://
///   scenes/              *.agrscene
class Project {
public:
    static constexpr const char* kFileName = "project.agrproject";

    /// Creates the folder structure, copies the template textures from
    /// `templateAssetsDir` and writes the startup scene. Fails if the folder exists and is not empty.
    static Result<Project> create(const std::filesystem::path& folder, const ProjectSettings& settings,
                                  SceneTemplate sceneTemplate, const std::filesystem::path& templateAssetsDir);
    static Result<Project> open(const std::filesystem::path& folder);

    Status save() const;

    const std::filesystem::path& root() const { return m_root; }
    std::filesystem::path assetsDirectory() const { return m_root / "assets"; }
    std::filesystem::path scenesDirectory() const { return m_root / "scenes"; }
    ProjectSettings& settings() { return m_settings; }
    const ProjectSettings& settings() const { return m_settings; }

    /// Project-relative paths ("scenes/main.agrscene"), sorted.
    std::vector<std::string> listScenes() const;

private:
    std::filesystem::path m_root;
    ProjectSettings m_settings;
};

/// Recently opened projects, most recent first (persisted by the editor).
struct RecentProjects {
    std::vector<std::string> paths;
    void touch(const std::string& path, usize max = 12);
    void remove(const std::string& path);
    std::string toJson() const;
    static RecentProjects fromJson(std::string_view text);
};

} // namespace agr::editor
