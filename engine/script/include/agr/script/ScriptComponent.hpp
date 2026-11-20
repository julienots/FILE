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

/// Groups of the entity (comma-separated names, e.g. "enemies,flying"): agr.group("enemies")
/// lists the members; entity:addToGroup / removeFromGroup / inGroup from scripts.
struct GroupsComponent {
    std::string groups;
};

void registerScriptComponent();

} // namespace agr
