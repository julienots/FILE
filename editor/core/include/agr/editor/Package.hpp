#pragma once
// Asset packages (.agrpkg): a manifest (name, version, author, description, dependencies, files
// with sizes and hashes) followed by the files' bytes. Created from project assets, installed
// into another project after a preview (new / overwritten / identical files, missing
// dependencies), recorded in assets/packages.json, uninstalled without ever deleting a file the
// user changed since the install. A "repository" is any folder of .agrpkg files. See docs/PACKAGES.md.
#include <filesystem>
#include <string>
#include <vector>

#include "agr/core/Result.hpp"
#include "agr/core/Types.hpp"

namespace agr::editor {

struct PackageFile {
    std::string path;  ///< Relative to the project's assets folder, '/' separators.
    u64 size = 0;
    std::string hash;  ///< FNV-1a 64-bit, hexadecimal.
};

struct PackageManifest {
    std::string name;  ///< Lowercase letters, digits, '-', '_' and '.'.
    std::string version = "1.0.0";
    std::string author;
    std::string description;
    std::vector<std::string> dependencies;  ///< Names of packages that must be installed first.
    std::vector<PackageFile> files;
    std::string engineVersion;  ///< Engine that created it (informative).
};

/// Content hash used by packages (FNV-1a 64, 16 hex digits).
std::string packageHash(std::string_view bytes);
/// Validates a package name and a relative asset path (no "..", no absolute path).
Status validatePackageName(const std::string& name);
Status validatePackagePath(const std::string& relative);

/// Writes a package from files and folders (relative to `assetsRoot`; folders are taken whole).
/// Fills manifest.files; returns the manifest written.
Result<PackageManifest> writePackage(const std::filesystem::path& output, PackageManifest manifest, const std::filesystem::path& assetsRoot,
                                     const std::vector<std::string>& relativePaths);
/// Reads only the manifest (fast: repository listings).
Result<PackageManifest> readPackageManifest(const std::filesystem::path& package);

struct InstalledPackage {
    std::string name, version, source;
    std::vector<PackageFile> files;  ///< As installed (hash at install time).
};
/// Packages recorded in <assets>/packages.json.
std::vector<InstalledPackage> installedPackages(const std::filesystem::path& assetsRoot);

struct InstallPlan {
    PackageManifest manifest;
    std::vector<std::string> newFiles, changedFiles, identicalFiles;  ///< changed = an existing different file would be replaced
    std::vector<std::string> missingDependencies;
    std::string installedVersion;  ///< Non-empty when another version is installed (update).
};
Result<InstallPlan> planInstall(const std::filesystem::path& package, const std::filesystem::path& assetsRoot);
/// Installs (verifies every file's size and hash first; nothing is written if one fails).
/// `replaceChanged` = overwrite existing different files (else the install is refused).
Status installPackage(const std::filesystem::path& package, const std::filesystem::path& assetsRoot, bool replaceChanged);

struct UninstallPlan {
    std::vector<std::string> remove;    ///< Unchanged since the install: removed.
    std::vector<std::string> modified;  ///< Changed by the user: kept unless asked.
    std::vector<std::string> missing;   ///< Already gone.
    std::vector<std::string> neededBy;  ///< Installed packages depending on this one.
};
Result<UninstallPlan> planUninstall(const std::string& name, const std::filesystem::path& assetsRoot);
/// Removes the package's files (and the folders it leaves empty), then its record. Refused while
/// other packages depend on it. Modified files are kept unless `removeModified`.
Status uninstallPackage(const std::string& name, const std::filesystem::path& assetsRoot, bool removeModified);

/// Manifests of the .agrpkg files of a folder (a local repository), with their paths.
struct RepositoryEntry {
    std::filesystem::path path;
    PackageManifest manifest;
};
std::vector<RepositoryEntry> listRepository(const std::filesystem::path& folder);

} // namespace agr::editor
