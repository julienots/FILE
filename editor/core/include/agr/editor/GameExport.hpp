#pragma once
#include <filesystem>
#include <string>
#include <vector>

#include "agr/editor/Project.hpp"
#include "agr/runtime/GameManifest.hpp"

namespace agr::editor {

/// Manifest of a project's exported game (name, version, package id, orientation...).
GameManifest gameManifest(const Project& project);
/// Default Android application id: com.<organization>.<name>, lower case.
std::string defaultPackageId(const ProjectSettings& settings);

struct ExportReport {
    std::filesystem::path output;   ///< Folder, APK or AAB produced.
    u32 files = 0;                  ///< Game data files.
    u64 bytes = 0;                  ///< Game data size.
    u32 binaryScenes = 0;           ///< Scenes converted to the binary format (faster loading).
    std::vector<std::string> warnings;
    std::string log;                ///< Output of external tools (Gradle).
};

/// Creates `dir`, or empties it when (and only when) a previous export wrote it; any other
/// non-empty folder is refused (nothing is deleted without being an export output).
Status prepareExportFolder(const std::filesystem::path& dir);

/// Writes the game data of a project into `gameDir` (created or emptied first if it holds
/// a previous export): game.json, assets/, every .agrscene of the project. Each scene is
/// converted to the binary scene format (same file name, loading detects the format) when
/// the conversion is verified lossless; otherwise it is copied as saved on disk.
Result<ExportReport> packageGameData(const Project& project, const std::filesystem::path& gameDir);

/// Desktop build: `outputDir`/<Name>/ with the player executable (renamed after the game)
/// and its game/ folder.
Result<ExportReport> exportDesktop(const Project& project, const std::filesystem::path& outputDir, const std::filesystem::path& playerExecutable);

/// Writes `zipPath`, a ZIP archive (stored, no compression) of every file under `dir`, paths
/// relative to dir's parent (the archive opens on the game's folder); Unix permissions kept.
Status zipFolder(const std::filesystem::path& dir, const std::filesystem::path& zipPath);

/// Windows x64 game player (agr_player.exe) for exports from any system: AGR_EXPORT_TEMPLATES or
/// export_templates/ next to the editor (windows-x64/agr_player.exe), the cross-compiled build
/// tree (build/windows-x64/bin), or the editor's own player on Windows. Empty when none exists.
std::filesystem::path findWindowsPlayer(const std::filesystem::path& editorDirectory);

} // namespace agr::editor
