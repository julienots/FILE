#pragma once
#include <string>

namespace agr {

/// Attaches a Lua script (a .lua asset returning a table of callbacks) to the entity.
/// `properties` holds the values edited in the inspector for the script's declared
/// `properties` (JSON object); missing keys use the script's defaults.
struct ScriptComponent {
    std::string script;      ///< e.g. "assets://scripts/player.lua"
    std::string properties;  ///< JSON object, e.g. {"speed": 6}
    bool enabled = true;
};

void registerScriptComponent();

} // namespace agr
