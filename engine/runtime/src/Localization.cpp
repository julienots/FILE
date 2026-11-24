#include "agr/runtime/Localization.hpp"

#include <cmath>

#include <algorithm>
#include <cctype>

namespace agr {

namespace {

std::vector<std::vector<std::string>> parseCsv(std::string_view text, std::string& error) {
    std::vector<std::vector<std::string>> rows;
    if (text.size() >= 3 && static_cast<u8>(text[0]) == 0xEF && static_cast<u8>(text[1]) == 0xBB && static_cast<u8>(text[2]) == 0xBF)
        text.remove_prefix(3);
    std::vector<std::string> row;
    std::string field;
    bool quoted = false, fieldStarted = false;
    usize line = 1;
    for (usize i = 0; i < text.size(); ++i) {
        const char c = text[i];
        if (quoted) {
            if (c == '"') {
                if (i + 1 < text.size() && text[i + 1] == '"') {
                    field += '"';
                    ++i;
                } else {
                    quoted = false;
                }
            } else {
                if (c == '\n') ++line;
                field += c;
            }
            continue;
        }
        if (c == '"' && !fieldStarted) {
            quoted = fieldStarted = true;
        } else if (c == ',' || c == ';') {  // ';' : spreadsheets in French locales export with it
            row.push_back(std::move(field));
            field.clear();
            fieldStarted = false;
        } else if (c == '\n' || c == '\r') {
            if (c == '\r' && i + 1 < text.size() && text[i + 1] == '\n') ++i;
            row.push_back(std::move(field));
            field.clear();
            fieldStarted = false;
            if (!(row.size() == 1 && row[0].empty())) rows.push_back(std::move(row));
            row.clear();
            ++line;
        } else {
            field += c;
            fieldStarted = true;
        }
    }
    if (quoted) {
        error = "unterminated quoted field (line " + std::to_string(line) + ")";
        return {};
    }
    if (fieldStarted || !row.empty()) {
        row.push_back(std::move(field));
        rows.push_back(std::move(row));
    }
    return rows;
}

std::string csvField(const std::string& s) {
    if (s.find_first_of(",;\"\n\r") == std::string::npos && (s.empty() || (s.front() != ' ' && s.back() != ' '))) return s;
    std::string out = "\"";
    for (char c : s) {
        if (c == '"') out += '"';
        out += c;
    }
    return out + "\"";
}

std::string lower(std::string_view s) {
    std::string out(s);
    for (char& c : out) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return out;
}

} // namespace

Status Localization::parse(std::string_view csv) {
    std::string error;
    auto rows = parseCsv(csv, error);
    if (!error.empty()) return makeError(Error::Code::ParseError, "localization CSV: " + error);
    std::vector<std::string> languages, keys;
    std::vector<std::vector<std::string>> cells;
    if (!rows.empty()) {
        if (rows[0].empty() || rows[0].size() < 2) return makeError(Error::Code::ParseError, "localization CSV: header must be key,<language>,...");
        languages.assign(rows[0].begin() + 1, rows[0].end());
        for (const auto& l : languages)
            if (l.empty()) return makeError(Error::Code::ParseError, "localization CSV: empty language code in the header");
        std::unordered_map<std::string, usize> seen;
        for (usize r = 1; r < rows.size(); ++r) {
            if (rows[r].empty() || rows[r][0].empty()) continue;
            if (seen.contains(rows[r][0]))
                return makeError(Error::Code::ParseError, "localization CSV: key '" + rows[r][0] + "' appears twice");
            seen[rows[r][0]] = keys.size();
            keys.push_back(rows[r][0]);
            std::vector<std::string> values(languages.size());
            for (usize c = 0; c < languages.size() && c + 1 < rows[r].size(); ++c) values[c] = rows[r][c + 1];
            cells.push_back(std::move(values));
        }
    }
    m_languages = std::move(languages);
    m_keys = std::move(keys);
    m_cells = std::move(cells);
    reindex();
    if (!setLanguage(m_language)) {
        m_current = 0;
        m_language = m_languages.empty() ? std::string() : m_languages.front();
    }
    ++m_revision;
    return {};
}

std::string Localization::toCsv() const {
    std::string out = "key";
    for (const auto& l : m_languages) out += "," + csvField(l);
    out += "\n";
    for (usize r = 0; r < m_keys.size(); ++r) {
        out += csvField(m_keys[r]);
        for (usize c = 0; c < m_languages.size(); ++c) out += "," + csvField(m_cells[r][c]);
        out += "\n";
    }
    return out;
}

Status Localization::load(VirtualFileSystem& vfs, const std::string& uri) {
    if (!vfs.exists(uri)) {
        m_languages.clear();
        m_keys.clear();
        m_cells.clear();
        reindex();
        m_language.clear();
        ++m_revision;
        return {};
    }
    auto text = vfs.readText(uri);
    if (!text) return text.error();
    return parse(text.value());
}

Status Localization::save(VirtualFileSystem& vfs, const std::string& uri) const { return vfs.writeText(uri, toCsv()); }

bool Localization::setLanguage(std::string_view code) {
    if (code.empty()) return false;
    const std::string want = lower(code);
    auto pick = [&](usize i) {
        if (m_current != i || m_language != m_languages[i]) ++m_revision;
        m_current = i;
        m_language = m_languages[i];
        return true;
    };
    for (usize i = 0; i < m_languages.size(); ++i)
        if (lower(m_languages[i]) == want) return pick(i);
    const std::string base = want.substr(0, want.find_first_of("-_"));
    for (usize i = 0; i < m_languages.size(); ++i)
        if (lower(m_languages[i]) == base) return pick(i);
    return false;
}

void Localization::chooseLanguage(const std::vector<std::string>& preferred) {
    for (const auto& p : preferred)
        if (setLanguage(p)) return;
    if (!m_languages.empty()) setLanguage(m_languages.front());
}

const std::string& Localization::tr(std::string_view key) const {
    thread_local std::string fallback;
    auto it = m_index.find(std::string(key));
    if (it != m_index.end()) {
        const auto& row = m_cells[it->second];
        if (m_current < row.size() && !row[m_current].empty()) return row[m_current];
        if (!row.empty() && !row[0].empty()) return row[0];
    }
    fallback.assign(key);
    return fallback;
}

const char* pluralCategory(std::string_view language, f64 n) {
    const std::string_view lang = language.substr(0, language.find_first_of("-_"));
    const bool whole = n == std::floor(n);
    const f64 a = std::abs(n);
    const long long i = static_cast<long long>(a), m10 = i % 10, m100 = i % 100;
    auto is = [&](std::initializer_list<std::string_view> list) { return std::find(list.begin(), list.end(), lang) != list.end(); };
    if (is({"ja", "zh", "ko", "vi", "th", "id", "ms", "lo", "my", "km"})) return "other";
    if (is({"fr", "ff", "kab", "hy"})) return a < 2.0 ? "one" : "other";
    if (language == "pt-BR" || language == "pt_BR") return a < 2.0 ? "one" : "other";
    if (is({"ru", "uk", "be"})) {
        if (!whole) return "other";
        if (m10 == 1 && m100 != 11) return "one";
        if (m10 >= 2 && m10 <= 4 && (m100 < 12 || m100 > 14)) return "few";
        return "many";
    }
    if (is({"pl"})) {
        if (!whole) return "other";
        if (i == 1) return "one";
        if (m10 >= 2 && m10 <= 4 && (m100 < 12 || m100 > 14)) return "few";
        return "many";
    }
    if (is({"cs", "sk"})) {
        if (!whole) return "many";
        return i == 1 ? "one" : (i >= 2 && i <= 4 ? "few" : "other");
    }
    if (is({"ar"})) {
        if (!whole) return "other";
        if (i == 0) return "zero";
        if (i == 1) return "one";
        if (i == 2) return "two";
        if (m100 >= 3 && m100 <= 10) return "few";
        if (m100 >= 11) return "many";
        return "other";
    }
    if (is({"he", "iw"})) return whole && i == 1 ? "one" : (whole && i == 2 ? "two" : "other");
    if (is({"lt"})) {
        if (!whole) return "many";
        if (m10 == 1 && (m100 < 11 || m100 > 19)) return "one";
        if (m10 >= 2 && (m100 < 11 || m100 > 19)) return "few";
        return "other";
    }
    if (is({"ro"})) return whole && i == 1 ? "one" : ((!whole || i == 0 || (m100 >= 2 && m100 <= 19)) ? "few" : "other");
    // English, German, Dutch, Scandinavian, Spanish, Italian, Portuguese, Greek, Turkish, Hungarian...
    return whole && i == 1 ? "one" : "other";
}

std::string Localization::plural(std::string_view key, f64 n, const std::vector<std::string>& args) const {
    auto available = [&](const std::string& k) {
        const auto it = m_index.find(k);
        return it != m_index.end() && !cell(it->second, m_current).empty();
    };
    const std::string base(key);
    std::string chosen = base;
    if (n == 0.0 && available(base + ".zero")) chosen = base + ".zero";
    else if (const std::string c = base + "." + pluralCategory(m_language, n); available(c)) chosen = c;
    else if (available(base + ".other")) chosen = base + ".other";
    std::vector<std::string> all;
    std::string count = n == std::floor(n) && std::abs(n) < 1e15 ? std::to_string(static_cast<long long>(n)) : std::to_string(n);
    if (count.find('.') != std::string::npos) {
        while (count.back() == '0') count.pop_back();
        if (count.back() == '.') count.pop_back();
    }
    all.push_back(std::move(count));
    all.insert(all.end(), args.begin(), args.end());
    return format(chosen, all);
}

std::string Localization::format(std::string_view key, const std::vector<std::string>& args) const {
    std::string out = tr(key);
    for (usize i = args.size(); i-- > 0;) {  // {10} before {1}
        const std::string token = "{" + std::to_string(i + 1) + "}";
        for (usize pos = out.find(token); pos != std::string::npos; pos = out.find(token, pos + args[i].size()))
            out.replace(pos, token.size(), args[i]);
    }
    return out;
}

const std::string& Localization::cell(usize row, usize language) const {
    static const std::string none;
    return row < m_cells.size() && language < m_cells[row].size() ? m_cells[row][language] : none;
}

void Localization::setCell(usize row, usize language, std::string value) {
    if (row >= m_cells.size() || language >= m_languages.size()) return;
    m_cells[row][language] = std::move(value);
    ++m_revision;
}

bool Localization::addKey(std::string key) {
    if (key.empty() || m_index.contains(key)) return false;
    m_keys.push_back(std::move(key));
    m_cells.emplace_back(m_languages.size());
    reindex();
    ++m_revision;
    return true;
}

void Localization::removeKey(usize row) {
    if (row >= m_keys.size()) return;
    m_keys.erase(m_keys.begin() + static_cast<std::ptrdiff_t>(row));
    m_cells.erase(m_cells.begin() + static_cast<std::ptrdiff_t>(row));
    reindex();
    ++m_revision;
}

bool Localization::renameKey(usize row, std::string key) {
    if (row >= m_keys.size() || key.empty() || m_index.contains(key)) return false;
    m_keys[row] = std::move(key);
    reindex();
    ++m_revision;
    return true;
}

bool Localization::addLanguage(std::string code) {
    if (code.empty() || std::find(m_languages.begin(), m_languages.end(), code) != m_languages.end()) return false;
    m_languages.push_back(std::move(code));
    for (auto& row : m_cells) row.emplace_back();
    if (m_languages.size() == 1) setLanguage(m_languages.front());
    ++m_revision;
    return true;
}

void Localization::removeLanguage(usize column) {
    if (column >= m_languages.size()) return;
    m_languages.erase(m_languages.begin() + static_cast<std::ptrdiff_t>(column));
    for (auto& row : m_cells) row.erase(row.begin() + static_cast<std::ptrdiff_t>(column));
    if (!setLanguage(m_language)) {
        m_current = 0;
        m_language = m_languages.empty() ? std::string() : m_languages.front();
    }
    ++m_revision;
}

void Localization::reindex() {
    m_index.clear();
    for (usize i = 0; i < m_keys.size(); ++i) m_index[m_keys[i]] = i;
}

} // namespace agr
