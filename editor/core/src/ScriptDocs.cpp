#include "agr/editor/ScriptDocs.hpp"

#include <algorithm>
#include <cctype>

namespace agr::editor {

namespace {
bool identChar(char c) { return std::isalnum(static_cast<unsigned char>(c)) || c == '_'; }
std::string trim(std::string s) {
    while (!s.empty() && std::isspace(static_cast<unsigned char>(s.front()))) s.erase(0, 1);
    while (!s.empty() && std::isspace(static_cast<unsigned char>(s.back()))) s.pop_back();
    return s;
}
std::vector<std::string> lines(const std::string& code) {
    std::vector<std::string> out;
    std::string cur;
    for (char c : code) {
        if (c == '\n') {
            out.push_back(cur);
            cur.clear();
        } else if (c != '\r') {
            cur += c;
        }
    }
    out.push_back(cur);
    return out;
}
} // namespace

std::vector<ScriptSymbol> scriptSymbols(const std::string& code) {
    std::vector<ScriptSymbol> out;
    const auto ls = lines(code);
    for (usize i = 0; i < ls.size(); ++i) {
        std::string l = trim(ls[i]);
        if (l.rfind("local ", 0) == 0) l = trim(l.substr(6));
        std::string name, params;
        if (l.rfind("function ", 0) == 0) {  // function a.b:c(x, y)
            const usize open = l.find('(');
            if (open == std::string::npos) continue;
            name = trim(l.substr(9, open - 9));
            const usize close = l.find(')', open);
            params = l.substr(open + 1, (close == std::string::npos ? l.size() : close) - open - 1);
        } else {  // name = function(x, y)
            const usize eq = l.find('=');
            if (eq == std::string::npos || eq + 1 >= l.size() || l[eq + 1] == '=') continue;
            const std::string rhs = trim(l.substr(eq + 1));
            if (rhs.rfind("function", 0) != 0) continue;
            name = trim(l.substr(0, eq));
            const usize open = rhs.find('('), close = rhs.find(')');
            if (open == std::string::npos) continue;
            params = rhs.substr(open + 1, (close == std::string::npos ? rhs.size() : close) - open - 1);
        }
        if (name.empty() || !std::all_of(name.begin(), name.end(), [](char c) { return identChar(c) || c == '.' || c == ':'; })) continue;
        // Documentation: the comment lines just above (blank-free block).
        std::vector<std::string> doc;
        for (usize k = i; k-- > 0;) {
            const std::string c = trim(ls[k]);
            if (c.rfind("--", 0) != 0) break;
            std::string text = c.substr(2);
            while (!text.empty() && text.front() == '-') text.erase(0, 1);  // ---
            doc.insert(doc.begin(), trim(text));
        }
        std::string docText;
        for (const auto& d : doc) docText += (docText.empty() ? "" : "\n") + d;
        out.push_back({name, name + "(" + trim(params) + ")", docText, static_cast<int>(i + 1)});
    }
    return out;
}

const ScriptSymbol* findSymbol(const std::vector<ScriptSymbol>& symbols, const std::string& word) {
    for (const ScriptSymbol& s : symbols) {
        if (s.name == word) return &s;
        const usize cut = s.name.find_last_of(".:");
        if (cut != std::string::npos && s.name.substr(cut + 1) == word) return &s;
    }
    return nullptr;
}

std::vector<ScriptReference> findReferences(const std::vector<std::pair<std::string, std::string>>& files, const std::string& word) {
    std::vector<ScriptReference> out;
    if (word.empty()) return out;
    for (const auto& [file, code] : files) {
        const auto symbols = scriptSymbols(code);
        std::vector<int> defLines;
        for (const ScriptSymbol& s : symbols) {
            const usize cut = s.name.find_last_of(".:");
            if (s.name == word || (cut != std::string::npos && s.name.substr(cut + 1) == word)) defLines.push_back(s.line);
        }
        // Lua lexing light: strings ("", '', [[ ]]) and comments (--, --[[ ]]) are skipped.
        int line = 1, column = 1;
        usize lineStart = 0;
        bool longComment = false, longString = false;
        auto addRef = [&](usize at) {
            const usize end = code.find('\n', lineStart);
            ScriptReference r{file, line, static_cast<int>(at - lineStart) + 1, code.substr(lineStart, (end == std::string::npos ? code.size() : end) - lineStart), false};
            r.text = trim(r.text);
            r.definition = std::find(defLines.begin(), defLines.end(), line) != defLines.end();
            out.push_back(std::move(r));
        };
        for (usize i = 0; i < code.size();) {
            const char c = code[i];
            if (c == '\n') {
                ++line;
                lineStart = i + 1;
                ++i;
                continue;
            }
            if (longComment || longString) {
                if (code.compare(i, 2, "]]") == 0) {
                    longComment = longString = false;
                    i += 2;
                } else {
                    ++i;
                }
                continue;
            }
            if (code.compare(i, 4, "--[[") == 0) {
                longComment = true;
                i += 4;
                continue;
            }
            if (code.compare(i, 2, "--") == 0) {
                while (i < code.size() && code[i] != '\n') ++i;
                continue;
            }
            if (code.compare(i, 2, "[[") == 0) {
                longString = true;
                i += 2;
                continue;
            }
            if (c == '"' || c == '\'') {
                ++i;
                while (i < code.size() && code[i] != c && code[i] != '\n') i += code[i] == '\\' ? 2 : 1;
                ++i;
                continue;
            }
            if (identChar(c)) {
                usize j = i;
                while (j < code.size() && identChar(code[j])) ++j;
                if (code.compare(i, j - i, word) == 0 && j - i == word.size() && !(i > 0 && std::isdigit(static_cast<unsigned char>(code[i])))) addRef(i);
                i = j;
                continue;
            }
            ++i;
        }
        (void)column;
    }
    return out;
}

} // namespace agr::editor
