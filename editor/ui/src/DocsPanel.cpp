// Integrated documentation: the engine's guides (docs/*.md, shipped in editor_resources/docs)
// rendered with a small Markdown renderer (headings, paragraphs, lists, quotes, code blocks,
// tables, bold, inline code, links between guides), full-text search, contextual opening.
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <sstream>

#include <imgui.h>
#include <misc/cpp/imgui_stdlib.h>

#include "agr/editor/ui/Panels.hpp"
#include "agr/editor/ui/Theme.hpp"

namespace agr::editor {

namespace fs = std::filesystem;

namespace {

std::string lowerAscii(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return s;
}

std::string trim(const std::string& s) {
    const auto a = s.find_first_not_of(" \t\r");
    if (a == std::string::npos) return {};
    const auto b = s.find_last_not_of(" \t\r");
    return s.substr(a, b - a + 1);
}

} // namespace

void DocsPanel::scan(const std::string& docsDir) {
    m_dir = docsDir;
    m_docs.clear();
    std::error_code ec;
    for (const auto& entry : fs::directory_iterator(docsDir, ec)) {
        if (!entry.is_regular_file() || entry.path().extension() != ".md") continue;
        std::ifstream in(entry.path(), std::ios::binary);
        Doc d;
        d.file = entry.path().filename().string();
        d.text.assign((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
        d.lower = lowerAscii(d.text);
        std::istringstream lines(d.text);
        for (std::string line; std::getline(lines, line);)
            if (line.rfind("# ", 0) == 0) {
                d.title = trim(line.substr(2));
                break;
            }
        if (d.title.empty()) d.title = d.file;
        m_docs.push_back(std::move(d));
    }
    std::sort(m_docs.begin(), m_docs.end(), [](const Doc& a, const Doc& b) { return a.title < b.title; });
    m_scanned = true;
}

bool DocsPanel::openDoc(const std::string& file) {
    for (usize i = 0; i < m_docs.size(); ++i)
        if (m_docs[i].file == file) {
            m_current = static_cast<int>(i);
            m_scrollTop = true;
            return true;
        }
    return false;
}

// ---------------------------------------------------------------------------- inline text
/// Renders a paragraph with **bold**, `code` and [links](target), wrapping word by word.
void DocsPanel::inlineText(EditorShared& s, const std::string& text, float indent) {
    struct Span {
        std::string text;
        int style = 0;  // 0 normal, 1 bold, 2 code, 3 link
        std::string link;
    };
    std::vector<Span> spans;
    std::string cur;
    int style = 0;
    auto flush = [&] {
        if (!cur.empty()) spans.push_back({cur, style, {}});
        cur.clear();
    };
    for (usize i = 0; i < text.size(); ++i) {
        const char c = text[i];
        if (c == '*' && i + 1 < text.size() && text[i + 1] == '*' && style != 2) {
            flush();
            style = style == 1 ? 0 : 1;
            ++i;
        } else if (c == '`') {
            flush();
            style = style == 2 ? 0 : 2;
        } else if (c == '[' && style != 2) {
            const auto close = text.find("](", i);
            const auto end = close == std::string::npos ? std::string::npos : text.find(')', close);
            if (close != std::string::npos && end != std::string::npos) {
                flush();
                spans.push_back({text.substr(i + 1, close - i - 1), 3, text.substr(close + 2, end - close - 2)});
                i = end;
            } else {
                cur += c;
            }
        } else {
            cur += c;
        }
    }
    flush();
    const float wrap = ImGui::GetContentRegionAvail().x;
    const float startX = ImGui::GetCursorPosX() + indent;
    ImGui::SetCursorPosX(startX);
    float x = 0.0f;
    const float space = ImGui::CalcTextSize(" ").x;
    bool firstWord = true, spaceBefore = false;
    for (const Span& sp : spans) {
        ImFont* font = sp.style == 1 ? s.fonts.semibold : sp.style == 2 ? s.fonts.mono : nullptr;
        if (font) ImGui::PushFont(font, 0.0f);
        if (!sp.text.empty() && sp.text.front() == ' ') spaceBefore = true;
        std::istringstream words(sp.text);
        std::string w;
        while (words >> w) {
            const float ww = ImGui::CalcTextSize(w.c_str()).x;
            const float gap = spaceBefore ? space : 0.0f;
            if (!firstWord && x + gap + ww > wrap - indent && x > 0.0f) {
                x = 0.0f;  // the previous item already moved the cursor to the next line
                ImGui::SetCursorPosX(startX);
            } else if (!firstWord) {
                ImGui::SameLine(0.0f, gap);
                x += gap;
            }
            if (sp.style == 2) {
                const ImVec2 p = ImGui::GetCursorScreenPos();
                ImGui::GetWindowDrawList()->AddRectFilled({p.x - 2, p.y}, {p.x + ww + 2, p.y + ImGui::GetTextLineHeight()}, IM_COL32(255, 255, 255, 22), 3.0f);
                ImGui::TextUnformatted(w.c_str());
            } else if (sp.style == 3) {
                ImGui::TextColored(palette::toVec4(palette::kSelect), "%s", w.c_str());
                if (ImGui::IsItemHovered()) {
                    ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
                    ImGui::SetTooltip("%s", sp.link.c_str());
                }
                if (ImGui::IsItemClicked()) followLink(s, sp.link);
            } else {
                ImGui::TextUnformatted(w.c_str());
            }
            x += ww;
            firstWord = false;
            spaceBefore = true;  // between the words of a span
        }
        spaceBefore = !sp.text.empty() && sp.text.back() == ' ';
        if (font) ImGui::PopFont();
    }
    if (firstWord) ImGui::NewLine();
}

void DocsPanel::followLink(EditorShared& s, const std::string& link) {
    if (link.rfind("http", 0) == 0) {
        ImGui::SetClipboardText(link.c_str());
        s.notify("Lien copié : " + link);
        return;
    }
    std::string file = fs::path(link.substr(0, link.find('#'))).filename().string();
    if (!openDoc(file)) s.notify("Document introuvable : " + link);
}

// ---------------------------------------------------------------------------- blocks
void DocsPanel::render(EditorShared& s, const std::string& text) {
    std::istringstream in(text);
    std::vector<std::string> lines;
    for (std::string l; std::getline(in, l);) {
        if (!l.empty() && l.back() == '\r') l.pop_back();
        lines.push_back(l);
    }
    std::string paragraph;
    auto endParagraph = [&] {
        if (!paragraph.empty()) {
            inlineText(s, paragraph, 0.0f);
            ImGui::Spacing();
        }
        paragraph.clear();
    };
    for (usize i = 0; i < lines.size(); ++i) {
        const std::string& line = lines[i];
        const std::string t = trim(line);
        if (t.empty()) {
            endParagraph();
            continue;
        }
        if (t.rfind("```", 0) == 0) {  // code block
            endParagraph();
            std::string code;
            for (++i; i < lines.size() && trim(lines[i]).rfind("```", 0) != 0; ++i) code += lines[i] + "\n";
            ImGui::PushFont(s.fonts.mono, 0.0f);
            const float h = ImGui::CalcTextSize(code.c_str()).y + ImGui::GetStyle().WindowPadding.y * 2.0f;
            ImGui::PushStyleColor(ImGuiCol_ChildBg, IM_COL32(255, 255, 255, 12));
            ImGui::BeginChild(ImGui::GetID(static_cast<int>(i)), {0, h}, ImGuiChildFlags_Borders, ImGuiWindowFlags_HorizontalScrollbar);
            ImGui::TextUnformatted(code.c_str());
            ImGui::EndChild();
            ImGui::PopStyleColor();
            ImGui::PopFont();
            ImGui::Spacing();
            continue;
        }
        if (t[0] == '#') {  // heading
            endParagraph();
            usize level = 0;
            while (level < t.size() && t[level] == '#') ++level;
            const float sizes[] = {30.0f, 24.0f, 20.0f, 17.0f};
            ImGui::Spacing();
            ImGui::PushFont(s.fonts.semibold, sizes[std::min<usize>(level, 4) - 1]);
            ImGui::PushStyleColor(ImGuiCol_Text, level == 1 ? palette::toVec4(palette::kAmber) : palette::toVec4(palette::kText));
            ImGui::TextWrapped("%s", trim(t.substr(level)).c_str());
            ImGui::PopStyleColor();
            ImGui::PopFont();
            if (level <= 2) ImGui::Separator();
            ImGui::Spacing();
            continue;
        }
        if (t[0] == '|') {  // table
            endParagraph();
            std::vector<std::vector<std::string>> rows;
            for (; i < lines.size() && !trim(lines[i]).empty() && trim(lines[i])[0] == '|'; ++i) {
                const std::string r = trim(lines[i]);
                if (r.find("---") != std::string::npos && r.find_first_not_of("|-: ") == std::string::npos) continue;
                std::vector<std::string> cells;
                std::string cell;
                bool code = false;
                for (usize k = 1; k < r.size(); ++k) {
                    if (r[k] == '`') code = !code;
                    if (r[k] == '|' && !code && (k == 0 || r[k - 1] != '\\')) {
                        cells.push_back(trim(cell));
                        cell.clear();
                    } else {
                        cell += r[k];
                    }
                }
                rows.push_back(cells);
            }
            --i;
            if (rows.empty()) continue;
            const int cols = static_cast<int>(rows.front().size());
            if (cols > 0 && ImGui::BeginTable(("##t" + std::to_string(i)).c_str(), cols, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingStretchProp)) {
                for (usize r = 0; r < rows.size(); ++r) {
                    ImGui::TableNextRow();
                    for (int c = 0; c < cols; ++c) {
                        ImGui::TableSetColumnIndex(c);
                        const std::string cell = c < static_cast<int>(rows[r].size()) ? rows[r][static_cast<usize>(c)] : "";
                        if (r == 0) ImGui::PushFont(s.fonts.semibold, 0.0f);
                        ImGui::PushID(static_cast<int>(r * 64 + static_cast<usize>(c)));
                        inlineText(s, cell, 0.0f);
                        ImGui::PopID();
                        if (r == 0) ImGui::PopFont();
                    }
                }
                ImGui::EndTable();
            }
            ImGui::Spacing();
            continue;
        }
        const usize lead = line.find_first_not_of(' ');
        const bool bullet = t.rfind("- ", 0) == 0 || t.rfind("* ", 0) == 0;
        const usize dot = t.find(". ");
        const bool numbered = dot != std::string::npos && dot > 0 && dot < 4 && std::all_of(t.begin(), t.begin() + static_cast<std::ptrdiff_t>(dot), ::isdigit);
        if (bullet || numbered) {
            endParagraph();
            std::string item = bullet ? t.substr(2) : t.substr(dot + 2);
            // Continuation lines (indented, not a new item).
            while (i + 1 < lines.size()) {
                const std::string n = trim(lines[i + 1]);
                const usize nl = lines[i + 1].find_first_not_of(' ');
                if (n.empty() || nl == std::string::npos || nl <= lead || n.rfind("- ", 0) == 0 || n.rfind("* ", 0) == 0 || n[0] == '|' || n.rfind("```", 0) == 0)
                    break;
                item += " " + n;
                ++i;
            }
            const float indent = 18.0f + static_cast<float>(lead) * 6.0f;
            ImGui::SetCursorPosX(ImGui::GetCursorPosX() + indent - 12.0f);
            ImGui::TextColored(palette::toVec4(palette::kAmber), "%s", bullet ? "•" : t.substr(0, dot + 1).c_str());
            ImGui::SameLine();
            inlineText(s, item, 0.0f);
            continue;
        }
        if (t[0] == '>') {  // quote
            endParagraph();
            const ImVec2 p = ImGui::GetCursorScreenPos();
            inlineText(s, trim(t.substr(1)), 12.0f);
            ImGui::GetWindowDrawList()->AddLine({p.x + 3, p.y}, {p.x + 3, ImGui::GetCursorScreenPos().y - 2}, palette::kAmber, 3.0f);
            continue;
        }
        if (t.rfind("<", 0) == 0 || t.rfind("![", 0) == 0) continue;  // HTML / images: not rendered
        paragraph += (paragraph.empty() ? "" : " ") + t;
    }
    endParagraph();
}

void DocsPanel::draw(EditorShared& s) {
    if (!s.openDocRequest.empty()) {
        if (!m_scanned) scan(m_dir);
        if (!openDoc(s.openDocRequest)) s.notify("Document introuvable : " + s.openDocRequest);
        s.openDocRequest.clear();
        s.showDocs = true;
        ImGui::SetWindowFocus("Documentation");
    }
    if (!s.showDocs) return;
    if (!m_scanned) scan(m_dir);
    if (!ImGui::Begin("Documentation", &s.showDocs)) {
        ImGui::End();
        return;
    }
    ImGui::BeginChild("##docList", {240, 0}, ImGuiChildFlags_ResizeX);
    ImGui::SetNextItemWidth(-1);
    ImGui::InputTextWithHint("##docSearch", "Rechercher dans la documentation", &m_search);
    const std::string q = lowerAscii(trim(m_search));
    if (m_docs.empty()) ImGui::TextWrapped("Aucun guide trouvé dans %s", m_dir.c_str());
    for (usize i = 0; i < m_docs.size(); ++i) {
        const Doc& d = m_docs[i];
        if (!q.empty() && d.lower.find(q) == std::string::npos) continue;
        if (ImGui::Selectable(d.title.c_str(), m_current == static_cast<int>(i))) {
            m_current = static_cast<int>(i);
            m_scrollTop = true;
        }
        if (!q.empty()) {  // where it matches
            const usize at = d.lower.find(q);
            const usize from = d.text.rfind('\n', at);
            const usize to = d.text.find('\n', at);
            std::string ctx = trim(d.text.substr(from == std::string::npos ? 0 : from + 1, (to == std::string::npos ? d.text.size() : to) - (from == std::string::npos ? 0 : from + 1)));
            if (ctx.size() > 90) ctx = ctx.substr(0, 90) + "…";
            ImGui::Indent();
            ImGui::TextDisabled("%s", ctx.c_str());
            ImGui::Unindent();
        }
    }
    ImGui::EndChild();
    ImGui::SameLine();
    ImGui::BeginChild("##docView", {0, 0}, ImGuiChildFlags_Borders);
    if (m_scrollTop) {
        ImGui::SetScrollY(0.0f);
        m_scrollTop = false;
    }
    if (m_current >= 0 && m_current < static_cast<int>(m_docs.size())) {
        ImGui::TextDisabled("docs/%s", m_docs[static_cast<usize>(m_current)].file.c_str());
        render(s, m_docs[static_cast<usize>(m_current)].text);
    } else {
        ImGui::TextWrapped("Choisissez un guide à gauche, ou cherchez un mot (« tween », « Android », « collision »...).");
    }
    ImGui::EndChild();
    ImGui::End();
}

} // namespace agr::editor
