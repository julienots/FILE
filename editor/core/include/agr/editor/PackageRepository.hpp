#pragma once
// Online package repositories: an index.json next to .agrpkg files on any static web host
// (GitHub Pages, a CDN, an intranet server) or a folder (file://). The editor lists the index,
// downloads a package (size and SHA-256 checked), resolves its dependencies and installs them
// in order with the regular package installer. See docs/PACKAGES.md.
#include <filesystem>
#include <string>
#include <vector>

#include "agr/core/Result.hpp"
#include "agr/editor/Package.hpp"

namespace agr::editor {

struct OnlinePackage {
    std::string name, version, author, description;
    std::vector<std::string> dependencies;
    std::string url;     ///< Absolute (relative URLs of the index are resolved against it).
    std::string sha256;  ///< Of the .agrpkg file.
    u64 size = 0;
};

struct OnlineIndex {
    std::string title;
    std::vector<OnlinePackage> packages;  ///< Latest version of each name first when several.
    const OnlinePackage* find(const std::string& name) const;
};

/// index.json: {"format": "agr.repository", "title": ..., "packages": [{name, version, author,
/// description, dependencies, file, size, sha256}]}; `file` is relative to the index URL.
Result<OnlineIndex> parseRepositoryIndex(std::string_view json, const std::string& indexUrl);
/// Writes <folder>/index.json for the .agrpkg files of a folder (to publish it as is).
Result<OnlineIndex> writeRepositoryIndex(const std::filesystem::path& folder, const std::string& title);
/// Reads an index from http(s):// or file:// (or a plain path).
Result<OnlineIndex> fetchRepositoryIndex(const std::string& url);
/// Downloads a package into `folder` (verified; nothing is left on a mismatch). Returns the file.
Result<std::filesystem::path> downloadPackage(const OnlinePackage& package, const std::filesystem::path& folder);
/// Packages to install, dependencies first, for `name` (already installed ones are skipped).
/// Fails on a missing dependency or a dependency cycle.
Result<std::vector<std::string>> resolveInstallOrder(const std::string& name, const OnlineIndex& index,
                                                     const std::vector<std::string>& installed);
/// Downloads and installs `name` and its missing dependencies into the project's assets.
Result<std::vector<std::string>> installFromRepository(const std::string& name, const OnlineIndex& index,
                                                       const std::filesystem::path& downloadFolder, const std::filesystem::path& assetsRoot,
                                                       bool replaceChanged);

} // namespace agr::editor
