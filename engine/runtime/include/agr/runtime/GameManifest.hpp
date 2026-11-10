#pragma once
#include <string>

#include "agr/core/Result.hpp"
#include "agr/core/Types.hpp"

namespace agr {

enum class ScreenOrientation : u8 { Landscape = 0, Portrait = 1, Sensor = 2 };
const char* enumName(ScreenOrientation o);

/// Description of an exported game (game.json at the root of the game data).
/// Layout of the game data, read-only:
///   game.json            this manifest (mounted as game://game.json)
///   assets/...           mounted as assets://
///   scenes/*.agrscene    loaded from game://scenes/...
struct GameManifest {
    std::string name = "AGR Game";
    std::string organization = "Superessence Studio";
    std::string version = "1.0.0";
    u32 versionCode = 1;                       ///< Android: must increase with every store upload.
    std::string packageId = "com.example.game";  ///< Android application id.
    std::string startupScene = "scenes/main.agrscene";  ///< Relative to the game root.
    std::string graphicsProfile = "auto";
    ScreenOrientation orientation = ScreenOrientation::Landscape;
    u32 targetFps = 0;                         ///< 0 = display refresh rate.
    bool backQuits = true;                     ///< Android back button / Escape-less desktop: quits the game.
    bool vibration = true;                     ///< Android: VIBRATE permission.
    bool keepScreenOn = true;
    std::string engineVersion;

    std::string toJson() const;
    static Result<GameManifest> fromJson(std::string_view text);
    /// "MyGame" -> letters, digits and '_' only (file names, identifiers).
    static std::string safeName(std::string_view name);
    /// Valid Android application id: at least two segments of [a-zA-Z][a-zA-Z0-9_]*.
    static bool validPackageId(std::string_view id);
};

} // namespace agr
