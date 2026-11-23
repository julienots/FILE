#pragma once
// Renaming and moving assets without breaking the project: every reference to the asset
// ("assets://old/path", also "old/path#Clip" model sub-assets and folder prefixes) is rewritten
// in the project's text files (scenes, prefabs, materials, clips, state machines, logic graphs,
// particle effects, sprite sheets, shader graphs, scripts, JSON data, the project file).
#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

#include "agr/core/Result.hpp"
#include "agr/core/Types.hpp"

namespace agr::editor {

struct AssetMoveReport {
    std::vector<std::filesystem::path> updatedFiles;  ///< Files whose references were rewritten.
    usize references = 0;                             ///< Number of references rewritten.
};

/// Replaces the references to `from` by `to` in `text` (URIs "assets://..."). A file reference
/// matches only whole paths ("a/b.png" does not match "a/b.png2"); `folder` = true moves every
/// path under "from/". Returns the number of replacements.
usize rewriteAssetReferences(std::string& text, std::string_view fromUri, std::string_view toUri, bool folder);

/// A file of the project that references an asset, with the lines where it does.
struct AssetUsage {
    std::filesystem::path file;
    std::vector<int> lines;  ///< 1-based
};
/// Every project text file that references the asset (or, for a folder, anything under it).
std::vector<AssetUsage> findAssetUsages(const std::filesystem::path& projectRoot, const std::filesystem::path& assetsDir, const std::string& assetRel);

/// Moves or renames an asset file or folder inside the project's assets directory (paths relative
/// to it, e.g. "textures/hero.png" -> "characters/hero.png") and rewrites the references to it in
/// every text file of the project. Refuses to overwrite an existing file or to leave the assets folder.
Result<AssetMoveReport> moveAsset(const std::filesystem::path& projectRoot, const std::filesystem::path& assetsDir, const std::string& fromRel,
                                  const std::string& toRel);

} // namespace agr::editor
