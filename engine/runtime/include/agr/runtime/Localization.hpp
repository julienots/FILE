#pragma once
// Translated texts: a table of keys and one column per language, stored as CSV
// (assets://localization.csv) so translators can use any spreadsheet.
//
//   key,fr,en
//   menu.play,Jouer,Play
//   hud.score,"Score : {1}","Score: {1}"
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "agr/core/FileSystem.hpp"
#include "agr/core/Result.hpp"

namespace agr {

class Localization {
public:
    static constexpr const char* kDefaultUri = "assets://localization.csv";

    /// Parses CSV text (UTF-8, optional BOM, quoted fields with "" escapes and line breaks).
    Status parse(std::string_view csv);
    std::string toCsv() const;
    /// Loads `uri`; a missing file gives an empty table (no error).
    Status load(VirtualFileSystem& vfs, const std::string& uri = kDefaultUri);
    Status save(VirtualFileSystem& vfs, const std::string& uri = kDefaultUri) const;

    const std::vector<std::string>& languages() const { return m_languages; }
    const std::string& language() const { return m_language; }
    /// Exact code or its base language ("fr-CA" -> "fr"). False if not in the table.
    bool setLanguage(std::string_view code);
    /// First of `preferred` (system order) available in the table, else the first column.
    void chooseLanguage(const std::vector<std::string>& preferred);

    /// Text in the current language; falls back to the first language, then to the key.
    const std::string& tr(std::string_view key) const;
    /// tr() with {1}, {2}... replaced by `args`.
    std::string format(std::string_view key, const std::vector<std::string>& args) const;
    bool has(std::string_view key) const { return m_index.contains(std::string(key)); }

    // Editing (editor table).
    const std::vector<std::string>& keys() const { return m_keys; }
    const std::string& cell(usize row, usize language) const;
    void setCell(usize row, usize language, std::string value);
    bool addKey(std::string key);  ///< False if empty or already present.
    void removeKey(usize row);
    bool renameKey(usize row, std::string key);
    bool addLanguage(std::string code);
    void removeLanguage(usize column);

    /// Incremented when the language or the table changes (texts to refresh).
    u64 revision() const { return m_revision; }

private:
    std::vector<std::string> m_languages;
    std::vector<std::string> m_keys;
    std::vector<std::vector<std::string>> m_cells;  // [row][language]
    std::unordered_map<std::string, usize> m_index;
    std::string m_language;
    usize m_current = 0;
    u64 m_revision = 1;
    void reindex();
};

} // namespace agr
