// Sprite sheet editor: the texture with its frames, mouse editing (draw, move, resize, pan,
// zoom), grid and automatic slicing, trimming, pivots and clips with a live preview.
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <filesystem>

#include <imgui.h>
#include <misc/cpp/imgui_stdlib.h>

#include "agr/editor/ui/Panels.hpp"
#include "agr/editor/ui/Theme.hpp"
#include "agr/twod/Atlas.hpp"

namespace agr::editor {

namespace {

std::string stemOf(const std::string& uri) {
    const usize slash = uri.find_last_of('/');
    return std::filesystem::path(slash == std::string::npos ? uri : uri.substr(slash + 1)).stem().string();
}

std::string framesText(const SpriteClip& c) {
    std::string out;
    for (usize i = 0; i < c.frames.size(); ++i) out += (i ? "," : "") + std::to_string(c.frames[i]);
    return out;
}

std::vector<u32> parseFrames(const std::string& text, usize frameCount) {
    std::vector<u32> out;
    std::string cur;
    for (char ch : text + ",") {
        if (ch == ',' || ch == ' ' || ch == ';') {
            if (!cur.empty()) {
                const long v = std::strtol(cur.c_str(), nullptr, 10);
                if (v >= 0 && static_cast<usize>(v) < frameCount) out.push_back(static_cast<u32>(v));
            }
            cur.clear();
        } else if (std::isdigit(static_cast<unsigned char>(ch))) {
            cur += ch;
        }
    }
    return out;
}

} // namespace

bool SpriteSheetPanel::open(EditorShared& s, const std::string& uri) {
    auto text = s.engine.vfs().readText(uri);
    if (!text) {
        m_status = "Lecture impossible : " + text.error().message;
        return false;
    }
    auto sheet = SpriteSheet::parse(text.value());
    if (!sheet) {
        m_status = "Feuille invalide : " + sheet.error().message;
        return false;
    }
    m_uri = uri;
    m_sheet = std::move(sheet.value());
    m_dirty = false;
    m_selected = m_sheet.frames.empty() ? -1 : 0;
    m_clip = m_sheet.clips.empty() ? -1 : 0;
    m_clipFrames = m_clip >= 0 ? framesText(m_sheet.clips[0]) : "";
    m_zoom = 0.0f;
    m_panX = m_panY = 0.0f;
    m_pixelsUri.clear();
    m_status.clear();
    s.showSheetEditor = true;
    return true;
}

bool SpriteSheetPanel::save(EditorShared& s) {
    if (m_uri.empty()) return false;
    if (Status st = s.engine.vfs().writeText(m_uri, m_sheet.toJson()); !st) {
        m_status = "Enregistrement impossible : " + st.error().message;
        return false;
    }
    if (SpriteSheetLibrary* lib = s.sheets()) lib->invalidate(m_uri);  // sprites in the scene follow
    m_dirty = false;
    m_status = "Enregistré : " + m_uri;
    return true;
}

bool SpriteSheetPanel::loadPixels(EditorShared& s) {
    if (m_pixelsUri == m_sheet.texture && m_pixels.width > 0) return true;
    auto bytes = s.engine.vfs().readBytes(m_sheet.texture);
    if (!bytes) return false;
    auto img = decodeImage(bytes.value(), m_sheet.texture);
    if (!img) return false;
    m_pixels = std::move(img.value());
    m_pixelsUri = m_sheet.texture;
    if (m_sheet.textureWidth != m_pixels.width || m_sheet.textureHeight != m_pixels.height) {  // the image changed size
        m_sheet.textureWidth = m_pixels.width;
        m_sheet.textureHeight = m_pixels.height;
        m_dirty = true;
    }
    return true;
}

bool SpriteSheetPanel::autoSlice(EditorShared& s, int alphaThreshold, int minSize, int mergeDistance) {
    if (!loadPixels(s)) {
        m_status = "Image illisible : " + m_sheet.texture;
        return false;
    }
    m_sheet.frames = autoSliceFrames(m_pixels, static_cast<u8>(std::clamp(alphaThreshold, 0, 254)), static_cast<u32>(std::max(minSize, 1)),
                                     static_cast<u32>(std::max(mergeDistance, 0)), stemOf(m_sheet.texture));
    for (SpriteClip& c : m_sheet.clips) std::erase_if(c.frames, [&](u32 f) { return f >= m_sheet.frames.size(); });
    m_selected = m_sheet.frames.empty() ? -1 : 0;
    m_dirty = true;
    m_status = std::to_string(m_sheet.frames.size()) + " frames trouvées";
    return true;
}

void SpriteSheetPanel::trimFrame(SpriteFrame& f) const {
    if (m_pixels.width == 0) return;
    u32 x0 = f.x + f.w, y0 = f.y + f.h, x1 = f.x, y1 = f.y;
    bool any = false;
    for (u32 y = f.y; y < std::min(f.y + f.h, m_pixels.height); ++y)
        for (u32 x = f.x; x < std::min(f.x + f.w, m_pixels.width); ++x)
            if (m_pixels.pixels[(static_cast<usize>(y) * m_pixels.width + x) * 4 + 3] > 8) {
                any = true;
                x0 = std::min(x0, x), y0 = std::min(y0, y), x1 = std::max(x1, x), y1 = std::max(y1, y);
            }
    if (!any) return;
    f.x = x0, f.y = y0, f.w = x1 - x0 + 1, f.h = y1 - y0 + 1;
}

void SpriteSheetPanel::draw(EditorShared& s) {
    if (!s.openSheetRequest.empty()) {
        open(s, s.openSheetRequest);
        s.openSheetRequest.clear();
    }
    if (!s.showSheetEditor) return;
    ImGui::SetNextWindowSize({960, 620}, ImGuiCond_FirstUseEver);
    const std::string title = std::string("Feuille de sprites") + (m_dirty ? " *" : "") + "###sheeteditor";
    if (!ImGui::Begin(title.c_str(), &s.showSheetEditor)) {
        ImGui::End();
        return;
    }
    if (m_uri.empty()) {
        ImGui::TextDisabled("Aucune feuille ouverte.");
        ImGui::TextDisabled("Double-cliquez une feuille (.agrsheet) dans les ressources, ou faites un clic droit");
        ImGui::TextDisabled("sur une image › « Éditer comme feuille de sprites ».");
        if (!m_status.empty()) ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(palette::kError), "%s", m_status.c_str());
        ImGui::End();
        return;
    }
    if (ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows) && ImGui::GetIO().KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_S)) save(s);
    const float side = std::min(340.0f, ImGui::GetContentRegionAvail().x * 0.45f);
    ImGui::BeginChild("##canvas", {ImGui::GetContentRegionAvail().x - side - 8.0f, 0}, ImGuiChildFlags_Borders,
                      ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
    drawCanvas(s);
    ImGui::EndChild();
    ImGui::SameLine();
    ImGui::BeginChild("##props", {0, 0});
    drawProperties(s);
    ImGui::EndChild();
    ImGui::End();
}

void SpriteSheetPanel::drawCanvas(EditorShared& s) {
    const float tw = static_cast<float>(std::max(1u, m_sheet.textureWidth)), th = static_cast<float>(std::max(1u, m_sheet.textureHeight));
    const ImVec2 origin = ImGui::GetCursorScreenPos(), avail = ImGui::GetContentRegionAvail();
    if (avail.x < 8.0f || avail.y < 8.0f) return;
    if (m_zoom <= 0.0f) {  // fit, centered
        m_zoom = std::max(0.05f, std::min((avail.x - 16.0f) / tw, (avail.y - 16.0f) / th));
        m_panX = (avail.x - tw * m_zoom) * 0.5f;
        m_panY = (avail.y - th * m_zoom) * 0.5f;
    }
    ImGui::InvisibleButton("##area", avail, ImGuiButtonFlags_MouseButtonLeft | ImGuiButtonFlags_MouseButtonRight | ImGuiButtonFlags_MouseButtonMiddle);
    const bool hovered = ImGui::IsItemHovered();
    ImDrawList* dl = ImGui::GetWindowDrawList();
    const ImVec2 o{origin.x + m_panX, origin.y + m_panY};
    auto toScreen = [&](float x, float y) { return ImVec2{o.x + x * m_zoom, o.y + y * m_zoom}; };
    const ImVec2 mouse = ImGui::GetIO().MousePos;
    const float mx = (mouse.x - o.x) / m_zoom, my = (mouse.y - o.y) / m_zoom;  // texels
    // Zoom around the cursor, pan with the right / middle button.
    if (hovered && ImGui::GetIO().MouseWheel != 0.0f) {
        const float z = std::clamp(m_zoom * std::pow(1.2f, ImGui::GetIO().MouseWheel), 0.05f, 64.0f);
        m_panX = mouse.x - origin.x - mx * z;
        m_panY = mouse.y - origin.y - my * z;
        m_zoom = z;
    }
    if (ImGui::IsItemActive() && (ImGui::IsMouseDragging(ImGuiMouseButton_Right) || ImGui::IsMouseDragging(ImGuiMouseButton_Middle))) {
        m_panX += ImGui::GetIO().MouseDelta.x;
        m_panY += ImGui::GetIO().MouseDelta.y;
    }
    // Checkerboard under the texture (transparency), then the texture.
    dl->PushClipRect(origin, {origin.x + avail.x, origin.y + avail.y}, true);
    const ImVec2 a = toScreen(0, 0), b = toScreen(tw, th);
    dl->AddRectFilled(a, b, palette::kGraphite700);
    const float cell = 12.0f;
    for (float y = std::max(a.y, origin.y); y < std::min(b.y, origin.y + avail.y); y += cell)
        for (float x = std::max(a.x, origin.x); x < std::min(b.x, origin.x + avail.x); x += cell)
            if ((static_cast<int>((x - a.x) / cell) + static_cast<int>((y - a.y) / cell)) % 2 == 0)
                dl->AddRectFilled({x, y}, {std::min(x + cell, b.x), std::min(y + cell, b.y)}, palette::kGraphite800);
    dl->AddImage(ImTextureRef(s.thumbnail(m_sheet.texture)), a, b);
    // Frames.
    int hoveredFrame = -1;
    for (int i = static_cast<int>(m_sheet.frames.size()) - 1; i >= 0; --i) {
        const SpriteFrame& f = m_sheet.frames[static_cast<usize>(i)];
        if (mx >= static_cast<float>(f.x) && my >= static_cast<float>(f.y) && mx < static_cast<float>(f.x + f.w) && my < static_cast<float>(f.y + f.h)) {
            hoveredFrame = i;
            break;
        }
    }
    for (usize i = 0; i < m_sheet.frames.size(); ++i) {
        const SpriteFrame& f = m_sheet.frames[i];
        const ImVec2 p0 = toScreen(static_cast<float>(f.x), static_cast<float>(f.y));
        const ImVec2 p1 = toScreen(static_cast<float>(f.x + f.w), static_cast<float>(f.y + f.h));
        const bool sel = static_cast<int>(i) == m_selected;
        dl->AddRect(p0, p1, sel ? palette::kAmber : (static_cast<int>(i) == hoveredFrame ? palette::kText : IM_COL32(61, 139, 253, 200)), 0.0f,
                    sel ? 2.0f : 1.0f);
        if (m_zoom * static_cast<float>(f.w) > 18.0f) dl->AddText({p0.x + 2, p0.y + 1}, sel ? palette::kAmber : palette::kTextMuted, std::to_string(i).c_str());
        if (sel) {
            const ImVec2 pv = toScreen(static_cast<float>(f.x) + f.pivot.x * static_cast<float>(f.w), static_cast<float>(f.y) + f.pivot.y * static_cast<float>(f.h));
            dl->AddCircleFilled(pv, 4.0f, palette::kAmber);
            dl->AddRectFilled({p1.x - 5, p1.y - 5}, {p1.x + 5, p1.y + 5}, palette::kAmber);  // resize handle
        }
    }
    // Mouse editing: draw a new frame on empty space, move a frame, resize from its corner.
    auto texel = [&](float v, float max) { return std::clamp(std::round(v), 0.0f, max); };
    if (hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
        const SpriteFrame* sel = m_selected >= 0 && m_selected < static_cast<int>(m_sheet.frames.size()) ? &m_sheet.frames[static_cast<usize>(m_selected)] : nullptr;
        const ImVec2 corner = sel ? toScreen(static_cast<float>(sel->x + sel->w), static_cast<float>(sel->y + sel->h)) : ImVec2{-1e9f, -1e9f};
        if (sel && std::abs(mouse.x - corner.x) <= 6.0f && std::abs(mouse.y - corner.y) <= 6.0f) {
            m_drag = Drag::Resize;
        } else if (hoveredFrame >= 0) {
            m_selected = hoveredFrame;
            m_drag = Drag::Move;
        } else if (mx >= 0 && my >= 0 && mx <= tw && my <= th) {
            m_drag = Drag::Create;
            m_selected = -1;
        }
        m_dragX = texel(mx, tw), m_dragY = texel(my, th);
        if (m_selected >= 0) m_dragOrigin = m_sheet.frames[static_cast<usize>(m_selected)];
    }
    if (m_drag != Drag::None && ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
        const float cx = texel(mx, tw), cy = texel(my, th);
        if (m_drag == Drag::Create) {
            dl->AddRect(toScreen(std::min(m_dragX, cx), std::min(m_dragY, cy)), toScreen(std::max(m_dragX, cx), std::max(m_dragY, cy)), palette::kSuccess, 0.0f,
                        2.0f);
        } else if (m_selected >= 0 && m_selected < static_cast<int>(m_sheet.frames.size())) {
            SpriteFrame& f = m_sheet.frames[static_cast<usize>(m_selected)];
            const i64 dx = static_cast<i64>(cx - m_dragX), dy = static_cast<i64>(cy - m_dragY);
            if (m_drag == Drag::Move) {
                f.x = static_cast<u32>(std::clamp<i64>(static_cast<i64>(m_dragOrigin.x) + dx, 0, static_cast<i64>(tw) - f.w));
                f.y = static_cast<u32>(std::clamp<i64>(static_cast<i64>(m_dragOrigin.y) + dy, 0, static_cast<i64>(th) - f.h));
            } else if (m_drag == Drag::Resize) {
                f.w = static_cast<u32>(std::clamp<i64>(static_cast<i64>(m_dragOrigin.w) + dx, 1, static_cast<i64>(tw) - f.x));
                f.h = static_cast<u32>(std::clamp<i64>(static_cast<i64>(m_dragOrigin.h) + dy, 1, static_cast<i64>(th) - f.y));
            }
            if (dx != 0 || dy != 0) m_dirty = true;
        }
    }
    if (m_drag != Drag::None && ImGui::IsMouseReleased(ImGuiMouseButton_Left)) {
        if (m_drag == Drag::Create) {
            const float cx = texel(mx, tw), cy = texel(my, th);
            const u32 x0 = static_cast<u32>(std::min(m_dragX, cx)), y0 = static_cast<u32>(std::min(m_dragY, cy));
            const u32 x1 = static_cast<u32>(std::max(m_dragX, cx)), y1 = static_cast<u32>(std::max(m_dragY, cy));
            if (x1 > x0 && y1 > y0) {
                SpriteFrame f;
                f.name = stemOf(m_sheet.texture) + "_" + std::to_string(m_sheet.frames.size());
                f.x = x0, f.y = y0, f.w = x1 - x0, f.h = y1 - y0;
                m_sheet.frames.push_back(f);
                m_selected = static_cast<int>(m_sheet.frames.size()) - 1;
                m_dirty = true;
            }
        }
        m_drag = Drag::None;
    }
    if (ImGui::IsWindowFocused() && m_selected >= 0 && (ImGui::IsKeyPressed(ImGuiKey_Delete) || ImGui::IsKeyPressed(ImGuiKey_Backspace))) {
        m_sheet.removeFrame(static_cast<u32>(m_selected));
        m_selected = std::min(m_selected, static_cast<int>(m_sheet.frames.size()) - 1);
        if (m_clip >= 0 && m_clip < static_cast<int>(m_sheet.clips.size())) m_clipFrames = framesText(m_sheet.clips[static_cast<usize>(m_clip)]);
        m_dirty = true;
    }
    dl->PopClipRect();
    char info[96];
    std::snprintf(info, sizeof info, "%u x %u  ·  zoom %.0f %%  ·  %d, %d", m_sheet.textureWidth, m_sheet.textureHeight, m_zoom * 100.0f,
                  static_cast<int>(std::floor(mx)), static_cast<int>(std::floor(my)));
    dl->AddText({origin.x + 6, origin.y + avail.y - ImGui::GetFontSize() - 4}, palette::kTextMuted, info);
    if (hovered && m_drag == Drag::None)
        ImGui::SetItemTooltip("Glisser dans le vide : nouvelle frame · glisser une frame : la déplacer · coin : la redimensionner\n"
                              "Molette : zoom · clic droit / milieu glissé : déplacer la vue · Suppr : supprimer la frame");
}

void SpriteSheetPanel::drawProperties(EditorShared& s) {
    ImGui::TextUnformatted(m_uri.c_str());
    if (ImGui::Button(m_dirty ? "Enregistrer *" : "Enregistrer")) save(s);
    ImGui::SameLine();
    if (ImGui::Button("Recharger")) open(s, m_uri);
    ImGui::SameLine();
    if (ImGui::Button("Ajuster la vue")) m_zoom = 0.0f;
    if (!m_status.empty()) ImGui::TextDisabled("%s", m_status.c_str());

    ImGui::SeparatorText("Feuille");
    ImGui::SetNextItemWidth(-90);
    if (ImGui::InputText("Texture", &m_sheet.texture, ImGuiInputTextFlags_EnterReturnsTrue)) {
        m_pixelsUri.clear();
        loadPixels(s);
        m_zoom = 0.0f;
        m_dirty = true;
    }
    ImGui::SetNextItemWidth(-90);
    if (ImGui::DragFloat("Pixels/unité", &m_sheet.pixelsPerUnit, 0.5f, 1.0f, 10000.0f, "%.0f")) m_dirty = true;
    if (ImGui::Checkbox("Pixel art (filtrage net)", &m_sheet.pixelArt)) m_dirty = true;

    ImGui::SeparatorText("Découpage");
    ImGui::SetNextItemWidth(120);
    ImGui::InputInt2("Cellule (px)", &m_gridW);
    ImGui::SetNextItemWidth(120);
    ImGui::InputInt2("Marge, espace", &m_margin);
    if (ImGui::Button("Découper en grille")) {
        SpriteSheet g = SpriteSheet::grid(m_sheet.texture, m_sheet.textureWidth, m_sheet.textureHeight, static_cast<u32>(std::max(m_gridW, 1)),
                                          static_cast<u32>(std::max(m_gridH, 1)), static_cast<u32>(std::max(m_margin, 0)), static_cast<u32>(std::max(m_spacing, 0)),
                                          stemOf(m_sheet.texture));
        m_sheet.frames = std::move(g.frames);
        for (SpriteClip& c : m_sheet.clips) std::erase_if(c.frames, [&](u32 f) { return f >= m_sheet.frames.size(); });
        m_selected = m_sheet.frames.empty() ? -1 : 0;
        m_status = std::to_string(m_sheet.frames.size()) + " frames";
        m_dirty = true;
    }
    ImGui::SetNextItemWidth(120);
    ImGui::InputInt("Seuil alpha", &m_alpha);
    ImGui::SetNextItemWidth(120);
    ImGui::InputInt("Taille min (px)", &m_minSize);
    ImGui::SetNextItemWidth(120);
    ImGui::InputInt("Fusion (px)", &m_merge);
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Les îlots plus proches que cette distance forment une seule frame (une épée à côté du héros)");
    if (ImGui::Button("Découpage automatique")) autoSlice(s, m_alpha, m_minSize, m_merge);
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Une frame par îlot de pixels opaques, rangées par lignes");

    ImGui::SeparatorText("Frames");
    ImGui::BeginChild("##frames", {0, 120}, ImGuiChildFlags_Borders);
    for (usize i = 0; i < m_sheet.frames.size(); ++i) {
        const std::string label = std::to_string(i) + "  " + m_sheet.frames[i].name;
        if (ImGui::Selectable(label.c_str(), static_cast<int>(i) == m_selected)) m_selected = static_cast<int>(i);
    }
    ImGui::EndChild();
    if (m_selected >= 0 && m_selected < static_cast<int>(m_sheet.frames.size())) {
        SpriteFrame& f = m_sheet.frames[static_cast<usize>(m_selected)];
        ImGui::SetNextItemWidth(-90);
        if (ImGui::InputText("Nom", &f.name)) m_dirty = true;
        int r[4] = {static_cast<int>(f.x), static_cast<int>(f.y), static_cast<int>(f.w), static_cast<int>(f.h)};
        ImGui::SetNextItemWidth(-90);
        if (ImGui::DragInt4("x y l h", r, 0.25f, 0, static_cast<int>(std::max(m_sheet.textureWidth, m_sheet.textureHeight)))) {
            f.x = static_cast<u32>(std::max(r[0], 0)), f.y = static_cast<u32>(std::max(r[1], 0));
            f.w = static_cast<u32>(std::max(r[2], 1)), f.h = static_cast<u32>(std::max(r[3], 1));
            m_dirty = true;
        }
        ImGui::SetNextItemWidth(-90);
        if (ImGui::SliderFloat2("Pivot", &f.pivot.x, 0.0f, 1.0f, "%.2f")) m_dirty = true;
        if (ImGui::SmallButton("Centre")) f.pivot = {0.5f, 0.5f}, m_dirty = true;
        ImGui::SameLine();
        if (ImGui::SmallButton("Pieds")) f.pivot = {0.5f, 1.0f}, m_dirty = true;
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Pivot en bas au centre (personnages posés au sol)");
        ImGui::SameLine();
        if (ImGui::SmallButton("Rogner")) {
            if (loadPixels(s)) {
                trimFrame(f);
                m_dirty = true;
            }
        }
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Réduit la frame aux pixels opaques qu'elle contient");
        if (ImGui::SmallButton("Monter") && m_selected > 0) {
            m_sheet.moveFrame(static_cast<u32>(m_selected), static_cast<u32>(m_selected - 1));
            --m_selected;
            m_dirty = true;
        }
        ImGui::SameLine();
        if (ImGui::SmallButton("Descendre") && m_selected + 1 < static_cast<int>(m_sheet.frames.size())) {
            m_sheet.moveFrame(static_cast<u32>(m_selected), static_cast<u32>(m_selected + 1));
            ++m_selected;
            m_dirty = true;
        }
        ImGui::SameLine();
        if (ImGui::SmallButton("Supprimer")) {
            m_sheet.removeFrame(static_cast<u32>(m_selected));
            m_selected = std::min(m_selected, static_cast<int>(m_sheet.frames.size()) - 1);
            m_dirty = true;
        }
    }

    ImGui::SeparatorText("Clips");
    for (usize i = 0; i < m_sheet.clips.size(); ++i) {
        if (i) ImGui::SameLine();
        if (ImGui::RadioButton((m_sheet.clips[i].name + "##clip" + std::to_string(i)).c_str(), m_clip == static_cast<int>(i))) {
            m_clip = static_cast<int>(i);
            m_clipFrames = framesText(m_sheet.clips[i]);
        }
    }
    if (ImGui::SmallButton("+ Clip")) {
        SpriteClip c;
        c.name = "clip" + std::to_string(m_sheet.clips.size());
        if (m_selected >= 0) c.frames.push_back(static_cast<u32>(m_selected));
        m_sheet.clips.push_back(c);
        m_clip = static_cast<int>(m_sheet.clips.size()) - 1;
        m_clipFrames = framesText(c);
        m_dirty = true;
    }
    if (m_clip >= 0 && m_clip < static_cast<int>(m_sheet.clips.size())) {
        SpriteClip& c = m_sheet.clips[static_cast<usize>(m_clip)];
        ImGui::SameLine();
        if (ImGui::SmallButton("Supprimer le clip")) {
            m_sheet.clips.erase(m_sheet.clips.begin() + m_clip);
            m_clip = m_sheet.clips.empty() ? -1 : 0;
            m_clipFrames = m_clip >= 0 ? framesText(m_sheet.clips[0]) : "";
            m_dirty = true;
            return;
        }
        ImGui::SetNextItemWidth(-90);
        if (ImGui::InputText("Nom du clip", &c.name)) m_dirty = true;
        ImGui::SetNextItemWidth(-90);
        if (ImGui::InputText("Frames", &m_clipFrames)) {
            c.frames = parseFrames(m_clipFrames, m_sheet.frames.size());
            m_dirty = true;
        }
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Indices des frames dans l'ordre, ex. 0,1,2,1");
        if (m_selected >= 0 && ImGui::SmallButton("+ frame sélectionnée")) {
            c.frames.push_back(static_cast<u32>(m_selected));
            m_clipFrames = framesText(c);
            m_dirty = true;
        }
        ImGui::SetNextItemWidth(120);
        if (ImGui::DragFloat("Images/s", &c.fps, 0.1f, 0.1f, 120.0f, "%.1f")) m_dirty = true;
        ImGui::SameLine();
        if (ImGui::Checkbox("Boucle", &c.loop)) m_dirty = true;
        // Live preview of the clip.
        if (!c.frames.empty() && m_sheet.textureWidth > 0) {
            const double t = std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count();
            const usize n = c.frames.size();
            usize k = static_cast<usize>(t * static_cast<double>(std::max(c.fps, 0.1f)));
            k = c.loop ? k % n : std::min(k % (n + 4), n - 1);
            const u32 fi = c.frames[k];
            if (fi < m_sheet.frames.size()) {
                const SpriteFrame& f = m_sheet.frames[fi];
                const float box = 96.0f;
                const float scale = box / static_cast<float>(std::max(f.w, f.h));
                const math::Vec4 uv = m_sheet.uv(fi);
                ImGui::Image(ImTextureRef(s.thumbnail(m_sheet.texture)), {static_cast<float>(f.w) * scale, static_cast<float>(f.h) * scale}, {uv.x, uv.y},
                             {uv.z, uv.w});
                ImGui::SameLine();
                ImGui::TextDisabled("frame %u (%zu / %zu)", fi, k + 1, n);
            }
        }
    }
}

} // namespace agr::editor
