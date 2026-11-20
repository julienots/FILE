// Built-in Lua code editor: syntax colors, live syntax check, completion from the engine's live
// API (agr.*, entity methods after ':', Lua libraries, words of the file), automatic indentation,
// Ctrl+S (scripts are hot reloaded when saved).
#include <algorithm>
#include <cctype>
#include <cstdio>
#include <filesystem>

#include <imgui.h>
#include <imgui_stdlib.h>

#include "agr/editor/ui/Panels.hpp"
#include "agr/script/ScriptPlugin.hpp"

namespace agr::editor {

namespace {

constexpr f64 kCheckDelay = 0.3;
constexpr const char* kKeywords[] = {"and",   "break", "do",  "else", "elseif", "end",    "false", "for",  "function", "goto", "if",
                                     "in",    "local", "nil", "not",  "or",     "repeat", "return", "then", "true",     "until", "while"};

bool isWord(char c) { return std::isalnum(static_cast<unsigned char>(c)) || c == '_'; }
bool isKeyword(std::string_view w) {
    return std::any_of(std::begin(kKeywords), std::end(kKeywords), [&](const char* k) { return w == k; });
}

int lineOf(const char* text, int offset) {
    int line = 1;
    for (int i = 0; i < offset && text[i]; ++i)
        if (text[i] == '\n') ++line;
    return line;
}

std::string expandTabs(const std::string& s) {
    std::string out;
    out.reserve(s.size());
    for (char c : s) {
        if (c == '\t') out += "    ";
        else if (c != '\r') out += c;
    }
    return out;
}

// Colors (dark theme, readable on the editor background).
constexpr ImU32 kColKeyword = IM_COL32(198, 120, 221, 255);
constexpr ImU32 kColString = IM_COL32(152, 195, 121, 255);
constexpr ImU32 kColNumber = IM_COL32(209, 154, 102, 255);
constexpr ImU32 kColComment = IM_COL32(110, 118, 130, 255);
constexpr ImU32 kColApi = IM_COL32(97, 175, 239, 255);
constexpr ImU32 kColFunction = IM_COL32(229, 192, 123, 255);

} // namespace

std::vector<ScriptEditorPanel::Token> ScriptEditorPanel::tokenize(const std::string& text) {
    std::vector<Token> out;
    usize i = 0;
    const usize n = text.size();
    auto longBracket = [&](usize at) -> int {  // [[ or [==[ : level, -1 if none
        if (at >= n || text[at] != '[') return -1;
        usize j = at + 1;
        int level = 0;
        while (j < n && text[j] == '=') ++level, ++j;
        return j < n && text[j] == '[' ? level : -1;
    };
    auto closeLong = [&](usize from, int level) {
        const std::string close = "]" + std::string(static_cast<usize>(level), '=') + "]";
        const usize e = text.find(close, from);
        return e == std::string::npos ? n : e + close.size();
    };
    std::string_view prevWord;
    while (i < n) {
        const char c = text[i];
        if (c == '-' && i + 1 < n && text[i + 1] == '-') {  // comment
            const int level = longBracket(i + 2);
            usize e;
            if (level >= 0) e = closeLong(i + 2, level);
            else {
                e = text.find('\n', i);
                if (e == std::string::npos) e = n;
            }
            out.push_back({i, e, kColComment});
            i = e;
            continue;
        }
        if (c == '"' || c == '\'') {
            usize j = i + 1;
            while (j < n && text[j] != c && text[j] != '\n') j += text[j] == '\\' ? 2 : 1;
            j = std::min(j + 1, n);
            out.push_back({i, j, kColString});
            i = j;
            continue;
        }
        if (c == '[') {
            const int level = longBracket(i);
            if (level >= 0) {
                const usize e = closeLong(i, level);
                out.push_back({i, e, kColString});
                i = e;
                continue;
            }
        }
        if (std::isdigit(static_cast<unsigned char>(c))) {
            usize j = i;
            while (j < n && (isWord(text[j]) || text[j] == '.')) ++j;
            out.push_back({i, j, kColNumber});
            i = j;
            continue;
        }
        if (isWord(c)) {
            usize j = i;
            while (j < n && isWord(text[j])) ++j;
            const std::string_view w(text.data() + i, j - i);
            ImU32 col = 0;
            if (isKeyword(w)) col = kColKeyword;
            else if (w == "agr" || w == "self") col = kColApi;
            else if (prevWord == "function" || (i > 0 && (text[i - 1] == ':' || text[i - 1] == '.') && j < n && text[j] == '(')) col = kColFunction;
            if (col) out.push_back({i, j, col});
            prevWord = w;
            i = j;
            continue;
        }
        if (!std::isspace(static_cast<unsigned char>(c))) prevWord = {};
        ++i;
    }
    return out;
}

std::vector<std::string> ScriptEditorPanel::suggestions(EditorShared& s, const std::string& text, int cursor, int& wordStart) {
    std::vector<std::string> out;
    int b = cursor;
    while (b > 0 && isWord(text[static_cast<usize>(b - 1)])) --b;
    wordStart = b;
    const std::string prefix = text.substr(static_cast<usize>(b), static_cast<usize>(cursor - b));
    // Context: "a.b.<prefix>" or "x:<prefix>".
    std::string path;
    bool member = false;
    if (b > 0 && (text[static_cast<usize>(b - 1)] == '.' || text[static_cast<usize>(b - 1)] == ':')) {
        member = true;
        if (text[static_cast<usize>(b - 1)] == ':') {
            path = ":";
        } else {
            int p = b - 1;
            while (p > 0 && (isWord(text[static_cast<usize>(p - 1)]) || text[static_cast<usize>(p - 1)] == '.')) --p;
            path = text.substr(static_cast<usize>(p), static_cast<usize>(b - 1 - p));
            if (path == "self.entity" || path == "self") path = path == "self" ? "" : ":";
        }
    }
    if (!member && prefix.empty()) return out;
    // A number being typed, a comment or a string: no suggestions.
    if (!prefix.empty() && std::isdigit(static_cast<unsigned char>(prefix[0]))) return out;
    {
        const usize lineStart = text.rfind('\n', cursor > 0 ? static_cast<usize>(cursor - 1) : 0);
        const std::string line = text.substr(lineStart == std::string::npos ? 0 : lineStart + 1,
                                             static_cast<usize>(cursor) - (lineStart == std::string::npos ? 0 : lineStart + 1));
        if (line.find("--") != std::string::npos) return out;
        if (std::count(line.begin(), line.end(), '"') % 2 == 1 || std::count(line.begin(), line.end(), '\'') % 2 == 1) return out;
    }
    std::vector<std::string> pool;
    auto* plugin = s.engine.service<ScriptPlugin>();
    if (member) {
        if (path != "self" && plugin && plugin->scripts()) pool = plugin->scripts()->completions(path);
        if (path == ":") {  // entity methods + script callbacks names useful on self
            for (const char* extra : {"onStart", "onUpdate", "onFixedUpdate", "onCollision", "onTrigger", "onDestroy"}) pool.push_back(extra);
        }
        if (path.empty()) {  // self.<...>: fields of the script = words of the file used after "self."
            for (usize p = text.find("self."); p != std::string::npos; p = text.find("self.", p + 5)) {
                usize e = p + 5;
                while (e < text.size() && isWord(text[e])) ++e;
                if (e > p + 5) pool.push_back(text.substr(p + 5, e - p - 5));
            }
            pool.push_back("entity");
        }
    } else {
        for (const char* k : kKeywords) pool.emplace_back(k);
        if (plugin && plugin->scripts())
            for (std::string& g : plugin->scripts()->completions("")) pool.push_back(std::move(g));
        pool.push_back("self");
        for (usize i = 0; i < text.size();) {  // words of the file (locals, functions)
            if (isWord(text[i]) && (i == 0 || !isWord(text[i - 1])) && !std::isdigit(static_cast<unsigned char>(text[i]))) {
                usize e = i;
                while (e < text.size() && isWord(text[e])) ++e;
                if (static_cast<int>(i) != b && e - i >= 3) pool.push_back(text.substr(i, e - i));
                i = e;
            } else {
                ++i;
            }
        }
    }
    std::sort(pool.begin(), pool.end());
    pool.erase(std::unique(pool.begin(), pool.end()), pool.end());
    auto lower = [](std::string v) {
        for (char& c : v) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        return v;
    };
    const std::string lp = lower(prefix);
    std::vector<std::string> starts, contains;
    for (const std::string& p : pool) {
        if (p == prefix) continue;
        const std::string l = lower(p);
        if (l.rfind(lp, 0) == 0) starts.push_back(p);
        else if (lp.size() >= 2 && l.find(lp) != std::string::npos) contains.push_back(p);
    }
    out = std::move(starts);
    out.insert(out.end(), contains.begin(), contains.end());
    if (out.size() > 40) out.resize(40);
    return out;
}

void ScriptEditorPanel::findNext(EditorShared&, bool backwards) {
    if (m_find.empty()) return;
    usize from = static_cast<usize>(std::max(m_cursorPos, 0));
    usize at = std::string::npos;
    if (backwards) {
        const usize start = from >= m_find.size() + 1 ? from - m_find.size() - 1 : std::string::npos;
        at = start == std::string::npos ? std::string::npos : m_text.rfind(m_find, start);
        if (at == std::string::npos) at = m_text.rfind(m_find);  // wrap
    } else {
        at = m_text.find(m_find, from);
        if (at == std::string::npos) at = m_text.find(m_find);  // wrap
    }
    if (at == std::string::npos) return;
    m_selectStart = static_cast<int>(at);
    m_selectEnd = static_cast<int>(at + m_find.size());
    m_refocus = true;
}

int ScriptEditorPanel::replaceAll(EditorShared& s, const std::string& what, const std::string& with) {
    if (what.empty()) return 0;
    int n = 0;
    std::string out;
    usize i = 0;
    for (usize at; (at = m_text.find(what, i)) != std::string::npos; i = at + what.size(), ++n) out += m_text.substr(i, at - i) + with;
    if (n == 0) return 0;
    out += m_text.substr(i);
    m_text = out;
    s.scriptDirty = m_text != m_saved;
    m_pendingCheck = true;
    m_editTime = 0.0;
    return n;
}

void ScriptEditorPanel::searchFiles(EditorShared& s) {
    m_fileHits.clear();
    if (m_find.empty()) return;
    std::error_code ec;
    const auto assets = s.ctx.project().assetsDirectory();
    for (auto it = std::filesystem::recursive_directory_iterator(assets, ec); !ec && it != std::filesystem::recursive_directory_iterator(); it.increment(ec)) {
        if (!it->is_regular_file(ec) || it->path().extension() != ".lua") continue;
        const std::string uri = "assets://" + std::filesystem::relative(it->path(), assets, ec).generic_string();
        auto text = s.engine.vfs().readText(uri);
        if (!text) continue;
        int line = 1;
        usize start = 0;
        const std::string& t = text.value();
        while (start <= t.size()) {
            usize end = t.find('\n', start);
            if (end == std::string::npos) end = t.size();
            const std::string l = t.substr(start, end - start);
            if (l.find(m_find) != std::string::npos && m_fileHits.size() < 500) {
                const usize b = l.find_first_not_of(" \t");
                m_fileHits.push_back({uri, line, b == std::string::npos ? l : l.substr(b)});
            }
            start = end + 1;
            ++line;
        }
    }
}

void ScriptEditorPanel::open(EditorShared& s, const std::string& uri) {
    auto text = s.engine.vfs().readText(uri);
    if (!text) {
        s.notify("Script illisible : " + uri);
        return;
    }
    m_uri = uri;
    m_text = expandTabs(text.value());
    m_saved = m_text;
    m_cursorLine = 1;
    m_gotoLine = m_gotoAfterOpen;
    m_gotoAfterOpen = 0;
    m_refocus = m_gotoLine > 0;
    m_suggestions.clear();
    s.scriptDirty = false;
    check(s);
}

void ScriptEditorPanel::check(EditorShared& s) {
    m_pendingCheck = false;
    m_error.reset();
    auto* plugin = s.engine.service<ScriptPlugin>();
    if (!plugin || !plugin->scripts() || m_uri.empty()) return;
    const std::string name = m_uri.substr(m_uri.find_last_of('/') + 1);
    m_error = plugin->scripts()->checkSyntax(m_text, name);
}

void ScriptEditorPanel::edit(EditorShared& s, std::string text) {
    m_text = expandTabs(text);
    s.scriptDirty = m_text != m_saved;
    check(s);
}

bool ScriptEditorPanel::save(EditorShared& s) {
    if (m_uri.empty()) return false;
    if (Status st = s.engine.vfs().writeText(m_uri, m_text); !st) {
        s.notify("Enregistrement impossible : " + st.error().message);
        return false;
    }
    m_saved = m_text;
    s.scriptDirty = false;
    check(s);
    s.notify(m_error ? "Script enregistré avec une erreur de syntaxe (ligne " + std::to_string(m_error->line) + ")"
                     : "Script enregistré : rechargé à chaud dans le jeu");
    return true;
}

void ScriptEditorPanel::drawCode(EditorShared& s, float height) {
    const float lineH = ImGui::GetTextLineHeight();
    const float adv = ImGui::CalcTextSize("M").x;
    const ImGuiStyle& style = ImGui::GetStyle();
    ImGui::BeginChild("##scriptScroll", {0, height}, ImGuiChildFlags_Borders, ImGuiWindowFlags_HorizontalScrollbar);
    const int lines = static_cast<int>(std::count(m_text.begin(), m_text.end(), '\n')) + 1;
    char buf[16];
    std::snprintf(buf, sizeof buf, "%d", std::max(lines, 999));
    const float gutter = ImGui::CalcTextSize(buf).x + style.ItemSpacing.x * 2.0f;
    const ImVec2 origin = ImGui::GetCursorScreenPos();
    ImDrawList* dl = ImGui::GetWindowDrawList();
    const float scroll = ImGui::GetScrollY();
    const int first = std::max(0, static_cast<int>(scroll / lineH) - 1);
    const int last = std::min(lines, first + static_cast<int>(ImGui::GetWindowHeight() / lineH) + 3);
    for (int i = first; i < last; ++i) {
        const bool error = m_error && m_error->line == i + 1;
        std::snprintf(buf, sizeof buf, "%d", i + 1);
        const float y = origin.y + style.FramePadding.y + static_cast<float>(i) * lineH;
        if (error) dl->AddRectFilled({origin.x, y}, {origin.x + gutter - style.ItemSpacing.x * 0.5f, y + lineH}, IM_COL32(200, 60, 60, 90));
        dl->AddText({origin.x + gutter - style.ItemSpacing.x - ImGui::CalcTextSize(buf).x, y},
                    error ? palette::kError : (i + 1 == m_cursorLine ? palette::kText : palette::kTextMuted), buf);
    }
    ImGui::SetCursorScreenPos({origin.x + gutter, origin.y});
    float width = ImGui::GetContentRegionAvail().x;
    usize longest = 0;
    for (usize start = 0; start <= m_text.size();) {
        usize end = m_text.find('\n', start);
        if (end == std::string::npos) end = m_text.size();
        longest = std::max(longest, end - start);
        start = end + 1;
    }
    width = std::max(width, static_cast<float>(longest) * adv + 60.0f);
    if (m_gotoLine > 0 || m_refocus) ImGui::SetKeyboardFocusHere();
    auto callback = [](ImGuiInputTextCallbackData* data) -> int {
        auto* self = static_cast<ScriptEditorPanel*>(data->UserData);
        if (data->EventFlag == ImGuiInputTextFlags_CallbackCompletion) {  // Tab
            if (!self->m_suggestions.empty()) {
                const std::string& pick = self->m_suggestions[static_cast<usize>(std::clamp(self->m_selected, 0, static_cast<int>(self->m_suggestions.size()) - 1))];
                data->DeleteChars(self->m_wordStart, data->CursorPos - self->m_wordStart);
                data->InsertChars(data->CursorPos, pick.c_str());
                self->m_suggestions.clear();
                self->m_hideUntilEdit = true;
            } else {
                data->InsertChars(data->CursorPos, "    ");
            }
            return 0;
        }
        // Pending insertion of a suggestion clicked with the mouse.
        if (!self->m_clickedSuggestion.empty()) {
            const int cursor = std::clamp(self->m_cursorPos, 0, data->BufTextLen);
            const int start = std::clamp(self->m_wordStart, 0, cursor);
            data->CursorPos = cursor;
            data->DeleteChars(start, cursor - start);
            data->InsertChars(start, self->m_clickedSuggestion.c_str());
            data->SelectionStart = data->SelectionEnd = data->CursorPos;
            self->m_clickedSuggestion.clear();
            self->m_hideUntilEdit = true;
        }
        if (self->m_selectStart >= 0) {  // a find result: select it
            data->SelectionStart = std::clamp(self->m_selectStart, 0, data->BufTextLen);
            data->SelectionEnd = data->CursorPos = std::clamp(self->m_selectEnd, 0, data->BufTextLen);
            self->m_selectStart = self->m_selectEnd = -1;
            self->m_followCursor = true;
        }
        if (self->m_gotoLine > 0) {
            int line = 1, at = 0;
            while (at < data->BufTextLen && line < self->m_gotoLine) {
                if (data->Buf[at] == '\n') ++line;
                ++at;
            }
            data->CursorPos = data->SelectionStart = data->SelectionEnd = at;
            self->m_gotoLine = 0;
            self->m_followCursor = true;
        }
        // Automatic indentation after Enter: the previous line's, one level more after an opener.
        if (data->BufTextLen == self->m_lastLength + 1 && data->CursorPos > 0 && data->Buf[data->CursorPos - 1] == '\n') {
            int ls = data->CursorPos - 1;
            while (ls > 0 && data->Buf[ls - 1] != '\n') --ls;
            std::string prev(data->Buf + ls, static_cast<usize>(data->CursorPos - 1 - ls));
            std::string indent;
            for (char c : prev) {
                if (c != ' ') break;
                indent += ' ';
            }
            while (!prev.empty() && prev.back() == ' ') prev.pop_back();
            auto endsWith = [&](const char* w) {
                const usize l = std::char_traits<char>::length(w);
                return prev.size() >= l && prev.compare(prev.size() - l, l, w) == 0 && (prev.size() == l || !isWord(prev[prev.size() - l - 1]));
            };
            const bool opener = endsWith("then") || endsWith("do") || endsWith("else") || endsWith("repeat") || (!prev.empty() && prev.back() == '{') ||
                                (prev.find("function") != std::string::npos && !prev.empty() && prev.back() == ')');
            if (opener) indent += "    ";
            if (!indent.empty()) data->InsertChars(data->CursorPos, indent.c_str());
        }
        self->m_lastLength = data->BufTextLen;
        const int line = lineOf(data->Buf, data->CursorPos);
        if (line != self->m_cursorLine) {
            self->m_cursorLine = line;
            self->m_followCursor = true;
        }
        self->m_cursorPos = data->CursorPos;
        int col = 0;
        for (int i = data->CursorPos - 1; i >= 0 && data->Buf[i] != '\n'; --i) ++col;
        self->m_cursorCol = col;
        return 0;
    };
    const float textH = static_cast<float>(lines + 1) * lineH + style.FramePadding.y * 2.0f;
    ImGui::PushStyleColor(ImGuiCol_FrameBg, IM_COL32(0, 0, 0, 0));
    const std::string before = m_text;
    const bool edited = ImGui::InputTextMultiline("##lua", &m_text, {width, std::max(textH, ImGui::GetContentRegionAvail().y)},
                                                  ImGuiInputTextFlags_CallbackCompletion | ImGuiInputTextFlags_CallbackAlways, callback, this);
    const bool active = ImGui::IsItemActive();
    const ImVec2 textMin = ImGui::GetItemRectMin();
    ImGui::PopStyleColor();
    m_refocus = false;
    if (edited) {
        m_pendingCheck = true;
        m_editTime = ImGui::GetTime();
        s.scriptDirty = m_text != m_saved;
        m_hideUntilEdit = false;
    }
    // Syntax colors over the visible lines (monospace font: column * advance).
    {
        const float x0 = textMin.x + style.FramePadding.x, y0 = textMin.y + style.FramePadding.y;
        const float visTop = scroll - lineH, visBottom = scroll + ImGui::GetWindowHeight();
        int line = 0;
        usize lineStart = 0, scanned = 0;
        for (const Token& t : tokenize(m_text)) {
            // Split multi-line tokens (block comments, long strings) per line.
            usize a = t.begin;
            while (a < t.end) {
                while (scanned < a) {
                    if (m_text[scanned] == '\n') {
                        ++line;
                        lineStart = scanned + 1;
                    }
                    ++scanned;
                }
                usize e = m_text.find('\n', a);
                if (e == std::string::npos || e > t.end) e = t.end;
                const float y = y0 + static_cast<float>(line) * lineH;
                if (y - textMin.y >= visTop && y - textMin.y <= visBottom && e > a)
                    dl->AddText({x0 + static_cast<float>(a - lineStart) * adv, y}, t.color, m_text.data() + a, m_text.data() + e);
                a = e < t.end ? e + 1 : e;
            }
        }
    }
    // Completion list under the cursor.
    if (active && (edited || m_text != before || m_suggestionsDirty)) {
        m_suggestionsDirty = false;
        m_suggestions = m_hideUntilEdit ? std::vector<std::string>{} : suggestions(s, m_text, m_cursorPos, m_wordStart);
        m_selected = 0;
    }
    if (ImGui::IsKeyPressed(ImGuiKey_Escape)) {
        m_suggestions.clear();
        m_hideUntilEdit = true;
    }
    if (!m_suggestions.empty() && (active || ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows))) {
        const ImVec2 at{textMin.x + style.FramePadding.x + static_cast<float>(m_cursorCol) * adv,
                        textMin.y + style.FramePadding.y + static_cast<float>(m_cursorLine) * lineH};
        ImGui::SetNextWindowPos(at);
        ImGui::SetNextWindowSizeConstraints({160, 0}, {420, lineH * 10.5f + style.WindowPadding.y * 2});
        ImGui::BeginTooltip();
        for (usize i = 0; i < m_suggestions.size(); ++i) {
            const bool sel = static_cast<int>(i) == m_selected;
            ImGui::TextColored(sel ? ImVec4(1, 1, 1, 1) : ImVec4(0.7f, 0.75f, 0.8f, 1), "%s%s", sel ? "> " : "  ", m_suggestions[i].c_str());
        }
        ImGui::TextDisabled("Tab : compléter · Échap : fermer");
        ImGui::EndTooltip();
    }
    if (m_followCursor) {
        m_followCursor = false;
        const float y = style.FramePadding.y + static_cast<float>(m_cursorLine - 1) * lineH;
        const float view = ImGui::GetWindowHeight() - style.ScrollbarSize;
        if (y < ImGui::GetScrollY()) ImGui::SetScrollY(y);
        else if (y + lineH * 2.0f > ImGui::GetScrollY() + view) ImGui::SetScrollY(y + lineH * 2.0f - view);
    }
    ImGui::EndChild();
}

void ScriptEditorPanel::draw(EditorShared& s) {
    if (!s.openScriptRequest.empty()) {
        const std::string uri = std::move(s.openScriptRequest);
        s.openScriptRequest.clear();
        s.showScriptEditor = true;
        if (uri != m_uri) {
            if (s.scriptDirty) {
                m_afterUri = uri;
                m_askSave = true;
            } else {
                open(s, uri);
            }
        }
    }
    if (m_askSave) {
        ImGui::OpenPopup("Script non enregistré");
        m_askSave = false;
    }
    if (ImGui::BeginPopupModal("Script non enregistré", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("%s a des modifications non enregistrées.", m_uri.c_str());
        int choice = 0;
        if (ImGui::Button("Enregistrer")) choice = 1;
        ImGui::SameLine();
        if (ImGui::Button("Ignorer les modifications")) choice = 2;
        ImGui::SameLine();
        if (ImGui::Button("Annuler")) choice = 3;
        if (choice) {
            const bool ok = choice == 2 || (choice == 1 && save(s));
            if (ok) {
                s.scriptDirty = false;
                if (m_afterUri.empty()) s.showScriptEditor = false;
                else open(s, m_afterUri);
            }
            m_afterUri.clear();
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
    if (!s.showScriptEditor) return;
    if (!s.scriptDirty && !m_uri.empty()) {  // changed outside (external editor): follow it
        if (auto disk = s.engine.vfs().readText(m_uri); disk && expandTabs(disk.value()) != m_saved) open(s, m_uri);
    }
    if (m_pendingCheck && ImGui::GetTime() - m_editTime >= kCheckDelay) check(s);
    bool visible = true;
    const bool shown = ImGui::Begin("Script", &visible);
    if (!visible) {
        if (s.scriptDirty) {
            m_afterUri.clear();
            m_askSave = true;
        } else {
            s.showScriptEditor = false;
        }
    }
    if (!shown) {
        ImGui::End();
        return;
    }
    if (m_uri.empty()) {
        ImGui::TextDisabled("Aucun script ouvert.");
        ImGui::TextDisabled("Cliquez sur un fichier .lua dans les assets, ou « Éditer » sur le composant Script.");
        ImGui::End();
        return;
    }
    ImGui::PushFont(s.fonts.semibold, 0.0f);
    ImGui::TextUnformatted((m_uri.substr(m_uri.find_last_of('/') + 1) + (s.scriptDirty ? " *" : "")).c_str());
    ImGui::PopFont();
    ImGui::SameLine();
    ImGui::BeginDisabled(!s.scriptDirty);
    if (ImGui::Button("Enregistrer")) save(s);
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort | ImGuiHoveredFlags_AllowWhenDisabled)) ImGui::SetTooltip("Ctrl+S quand l'éditeur a le focus");
    ImGui::SameLine();
    if (ImGui::Button("Annuler les modifications")) open(s, m_uri);
    ImGui::EndDisabled();
    ImGui::SameLine();
    if (m_error) ImGui::TextColored(palette::toVec4(palette::kError), "Ligne %d : %s", m_error->line, m_error->message.c_str());
    else ImGui::TextColored(palette::toVec4(palette::kSuccess), "Syntaxe correcte");
    const bool focused = ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows);
    if (focused && ImGui::GetIO().KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_S, false) && s.scriptDirty) save(s);
    if (focused && ImGui::GetIO().KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_F, false)) {
        m_findOpen = m_findFocus = true;
        m_findInFiles = ImGui::GetIO().KeyShift;
    }
    if (focused && ImGui::IsKeyPressed(ImGuiKey_F3, false)) findNext(s, ImGui::GetIO().KeyShift);
    if (m_findOpen) {
        ImGui::SetNextItemWidth(200);
        if (m_findFocus) {
            ImGui::SetKeyboardFocusHere();
            m_findFocus = false;
        }
        const bool enter = ImGui::InputTextWithHint("##find", "Rechercher", &m_find, ImGuiInputTextFlags_EnterReturnsTrue);
        if (enter) {
            if (m_findInFiles) searchFiles(s);
            else findNext(s, false);
        }
        ImGui::SameLine();
        if (ImGui::Checkbox("Tous les scripts", &m_findInFiles) && m_findInFiles) searchFiles(s);
        if (!m_findInFiles) {
            ImGui::SameLine();
            if (ImGui::SmallButton("<")) findNext(s, true);
            ImGui::SameLine();
            if (ImGui::SmallButton(">")) findNext(s, false);
            ImGui::SameLine();
            ImGui::SetNextItemWidth(160);
            ImGui::InputTextWithHint("##replace", "Remplacer par", &m_replace);
            ImGui::SameLine();
            if (ImGui::SmallButton("Remplacer") && m_selectStart < 0 && !m_find.empty()) {
                // The current match (at the cursor's selection end) is replaced, then the next one is selected.
                const usize end = static_cast<usize>(std::max(m_cursorPos, 0));
                if (end >= m_find.size() && m_text.compare(end - m_find.size(), m_find.size(), m_find) == 0) {
                    m_text.replace(end - m_find.size(), m_find.size(), m_replace);
                    m_cursorPos = static_cast<int>(end - m_find.size() + m_replace.size());
                    s.scriptDirty = m_text != m_saved;
                    m_pendingCheck = true;
                }
                findNext(s, false);
            }
            ImGui::SameLine();
            if (ImGui::SmallButton("Tout remplacer")) s.notify(std::to_string(replaceAll(s, m_find, m_replace)) + " remplacement(s)");
        }
        ImGui::SameLine();
        if (ImGui::SmallButton("x")) m_findOpen = false;
        if (m_findInFiles) {
            ImGui::BeginChild("##hits", {0, std::min(160.0f, ImGui::GetTextLineHeightWithSpacing() * static_cast<float>(m_fileHits.size() + 1))},
                              ImGuiChildFlags_Borders);
            if (m_fileHits.empty()) ImGui::TextDisabled(m_find.empty() ? "Tapez puis Entrée" : "Aucun résultat");
            for (usize i = 0; i < m_fileHits.size(); ++i) {
                const FileHit& h = m_fileHits[i];
                ImGui::PushID(static_cast<int>(i));
                const std::string label = h.uri.substr(h.uri.find_last_of('/') + 1) + ":" + std::to_string(h.line) + "  " + h.text;
                if (ImGui::Selectable(label.c_str())) {
                    if (h.uri != m_uri) {
                        s.openScriptRequest = h.uri;
                        m_gotoAfterOpen = h.line;
                    } else {
                        m_gotoLine = h.line;
                    }
                    m_refocus = true;
                }
                ImGui::PopID();
            }
            ImGui::EndChild();
        }
    }
    if (ImGui::BeginTable("##scriptLayout", 2, ImGuiTableFlags_Resizable | ImGuiTableFlags_BordersInnerV)) {
        ImGui::TableSetupColumn("code", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupColumn("api", ImGuiTableColumnFlags_WidthFixed, 220.0f);
        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        if (s.fonts.mono) ImGui::PushFont(s.fonts.mono, 0.0f);
        drawCode(s, std::max(120.0f, ImGui::GetContentRegionAvail().y));
        if (s.fonts.mono) ImGui::PopFont();
        ImGui::TableNextColumn();
        // API browser: the live agr library; a click inserts the name at the cursor.
        ImGui::TextDisabled("API (clic : insérer)");
        ImGui::BeginChild("##api");
        auto* plugin = s.engine.service<ScriptPlugin>();
        if (plugin && plugin->scripts()) {
            auto insert = [&](const std::string& name) {
                m_wordStart = m_cursorPos;
                m_clickedSuggestion = name;
                m_refocus = true;
            };
            for (const std::string& top : plugin->scripts()->completions("agr")) {
                const auto children = plugin->scripts()->completions("agr." + top);
                if (children.empty()) {
                    if (ImGui::Selectable(("agr." + top).c_str())) insert("agr." + top);
                } else if (ImGui::TreeNode(("agr." + top).c_str())) {
                    for (const std::string& c : children)
                        if (ImGui::Selectable(c.c_str())) insert("agr." + top + "." + c);
                    ImGui::TreePop();
                }
            }
            if (ImGui::TreeNode("entité : méthodes")) {
                for (const std::string& c : plugin->scripts()->completions(":"))
                    if (ImGui::Selectable(c.c_str())) insert(":" + c);
                ImGui::TreePop();
            }
        }
        ImGui::EndChild();
        ImGui::EndTable();
    }
    ImGui::End();
}

} // namespace agr::editor
