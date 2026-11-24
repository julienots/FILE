#pragma once
// Script editor help (like Godot's documentation comments and "find references"): the functions
// of a Lua script with their parameters and the `--` comment block written just above them, and
// every use of a name across scripts (whole words, outside comments and strings).
#include <string>
#include <utility>
#include <vector>

#include "agr/core/Types.hpp"

namespace agr::editor {

struct ScriptSymbol {
    std::string name;       ///< "Player:jump", "helper", "M.spawn"
    std::string signature;  ///< "Player:jump(height, double)"
    std::string doc;        ///< The comment lines above, without the dashes
    int line = 0;           ///< 1-based
};
std::vector<ScriptSymbol> scriptSymbols(const std::string& code);
/// The symbol named `word` ("jump" matches "Player:jump"), or nullptr.
const ScriptSymbol* findSymbol(const std::vector<ScriptSymbol>& symbols, const std::string& word);

struct ScriptReference {
    std::string file;  ///< As given in the input
    int line = 0, column = 0;  ///< 1-based
    std::string text;  ///< The whole line, trimmed
    bool definition = false;
};
/// Uses of `word` as a whole identifier in each (file, code), outside comments and strings.
std::vector<ScriptReference> findReferences(const std::vector<std::pair<std::string, std::string>>& files, const std::string& word);

} // namespace agr::editor
