// Visual shader editor: node canvas of a .agrshadergraph, compiled to the .agrshader next to it
// (live preview on the scene and on a sphere), undo / redo, generated code, errors per node.
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <filesystem>

#include <imgui.h>
#include <imgui_stdlib.h>

#include "agr/editor/ui/ImGuiRenderer.hpp"
#include "agr/editor/ui/Panels.hpp"
#include "agr/editor/ui/Theme.hpp"
#include "agr/render/ShaderLibrary.hpp"

namespace agr::editor {

namespace {
constexpr float kNodeWidth = 210.0f, kPinRadius = 5.0f;
using PT = ShaderPortType;

ImU32 typeColor(PT t) {
    switch (t) {
        case PT::Float: return IM_COL32(160, 160, 170, 255);
        case PT::Vec2: return IM_COL32(110, 200, 120, 255);
        case PT::Vec3: return IM_COL32(240, 200, 80, 255);
        case PT::Vec4: return IM_COL32(220, 120, 200, 255);
        case PT::Sampler: return IM_COL32(230, 110, 80, 255);
        default: return IM_COL32(120, 170, 240, 255);
    }
}
ImU32 categoryColor(const std::string& c) {
    if (c == "Sortie") return IM_COL32(150, 60, 60, 255);
    if (c == "Entrée") return IM_COL32(60, 110, 150, 255);
    if (c == "Paramètre") return IM_COL32(150, 110, 40, 255);
    if (c == "Texture" || c == "UV") return IM_COL32(150, 80, 40, 255);
    if (c == "Effet") return IM_COL32(110, 60, 150, 255);
    return IM_COL32(70, 76, 90, 255);
}
int components(PT t) { return t == PT::Vec2 ? 2 : t == PT::Vec3 ? 3 : t == PT::Vec4 ? 4 : 1; }
std::vector<float> parse(const std::string& s, int n) {
    std::vector<float> v;
    usize start = 0;
    while (start <= s.size() && static_cast<int>(v.size()) < n) {
        usize end = s.find(',', start);
        if (end == std::string::npos) end = s.size();
        v.push_back(static_cast<float>(std::atof(s.substr(start, end - start).c_str())));
        start = end + 1;
        if (end == s.size()) break;
    }
    while (static_cast<int>(v.size()) < n) v.push_back(v.empty() ? 0.0f : v.back());
    return v;
}
std::string format(const float* v, int n) {
    std::string s;
    for (int i = 0; i < n; ++i) {
        char buf[32];
        std::snprintf(buf, sizeof buf, "%g", static_cast<double>(v[i]));
        s += (i ? "," : "") + std::string(buf);
    }
    return s;
}
}  // namespace

void ShaderGraphPanel::open(EditorShared& s, const std::string& uri) {
    auto text = s.engine.vfs().readText(uri);
    if (!text) {
        s.notify("Graphe illisible : " + text.error().message);
        return;
    }
    auto g = ShaderGraph::fromJson(text.value());
    if (!g) {
        s.notify("Graphe invalide : " + g.error().message);
        return;
    }
    m_uri = uri;
    m_graph = std::move(g.value());
    m_undo.clear();
    m_redo.clear();
    m_dirty = false;
    m_selected = 0;
    m_pan = {40.0f, 40.0f};
    s.showShaderGraph = true;
    if (MaterialLibrary* materials = s.materials()) {
        MaterialData m;
        m.name = "Aperçu du graphe";
        m.shader = shaderUriForGraph(uri);
        materials->preview(kPreviewMaterial, m);
    }
    recompile(s);
}

void ShaderGraphPanel::recompile(EditorShared& s) {
    m_result = compileShaderGraph(m_graph);
    if (!m_result.ok() || m_uri.empty()) return;
    if (ShaderLibrary* lib = s.shaders()) lib->preview(shaderUriForGraph(m_uri), m_result.source);
    if (Renderer* r = s.renderer()) m_status = r->shaderStatus(shaderUriForGraph(m_uri));
}

void ShaderGraphPanel::changed(EditorShared& s) {
    m_dirty = true;
    recompile(s);
}

void ShaderGraphPanel::snapshot() {
    m_undo.push_back(m_graph.toJson());
    if (m_undo.size() > 100) m_undo.erase(m_undo.begin());
    m_redo.clear();
}

bool ShaderGraphPanel::undo(EditorShared& s) {
    if (m_undo.empty()) return false;
    m_redo.push_back(m_graph.toJson());
    m_graph = ShaderGraph::fromJson(m_undo.back()).value();
    m_undo.pop_back();
    changed(s);
    return true;
}

bool ShaderGraphPanel::redo(EditorShared& s) {
    if (m_redo.empty()) return false;
    m_undo.push_back(m_graph.toJson());
    m_graph = ShaderGraph::fromJson(m_redo.back()).value();
    m_redo.pop_back();
    changed(s);
    return true;
}

bool ShaderGraphPanel::save(EditorShared& s) {
    if (m_uri.empty()) return false;
    if (Status st = s.engine.vfs().writeText(m_uri, m_graph.toJson()); !st) {
        s.notify("Enregistrement impossible : " + st.error().message);
        return false;
    }
    m_dirty = false;
    m_result = compileShaderGraph(m_graph);
    if (!m_result.ok()) {
        s.notify("Graphe enregistré, mais il a des erreurs : le shader n'est pas mis à jour");
        return true;
    }
    const std::string shaderUri = shaderUriForGraph(m_uri);
    if (ShaderLibrary* lib = s.shaders()) {
        if (Status st = lib->save(shaderUri, m_result.source); !st) {
            s.notify("Shader non écrit : " + st.error().message);
            return false;
        }
    } else if (Status st = s.engine.vfs().writeText(shaderUri, m_result.source); !st) {
        s.notify("Shader non écrit : " + st.error().message);
        return false;
    }
    s.notify("Graphe enregistré ; shader généré : " + shaderUri + " (à choisir dans le champ Shader d'un matériau ou d'un sprite)");
    return true;
}

int ShaderGraphPanel::addNode(EditorShared& s, const std::string& type, ImVec2 canvasPos) {
    snapshot();
    const int id = m_graph.addNode(type, canvasPos.x, canvasPos.y);
    if (m_pendingFrom.node) {  // dropped link: connect the first compatible port
        const ShaderNodeDef* d = findShaderNodeDef(type);
        if (d && m_pendingFrom.output) {
            for (const ShaderPortDef& p : d->inputs)
                if (m_graph.connect(m_pendingFrom.node, m_pendingFrom.port, id, p.id)) break;
        } else if (d) {
            for (const ShaderPortDef& p : d->outputs)
                if (m_graph.connect(id, p.id, m_pendingFrom.node, m_pendingFrom.port)) break;
        }
        m_pendingFrom = {};
    }
    m_selected = id;
    changed(s);
    return id;
}

std::string ShaderGraphPanel::newGraph(ShaderGraph::Target target) {
    ShaderGraph g = ShaderGraph::create(target);
    const int out = g.nodes[0].id;
    // A starter: a color parameter into the base color (or the sprite color times it).
    const int tint = g.addNode("ParamColor", 60.0f, 80.0f);
    g.node(tint)->values["name"] = "tint";
    if (target == ShaderGraph::Target::Sprite) {
        const int base = g.addNode("BaseColor", 60.0f, 260.0f);
        const int mul = g.addNode("Multiply", 260.0f, 160.0f);
        (void)g.connect(base, "rgba", mul, "a");
        (void)g.connect(tint, "rgba", mul, "b");
        (void)g.connect(mul, "out", out, "color");
    } else {
        (void)g.connect(tint, "rgb", out, "baseColor");
    }
    return g.toJson();
}

void ShaderGraphPanel::draw(EditorShared& s, const MaterialPreview& preview) {
    if (!s.openShaderGraphRequest.empty()) {
        const std::string uri = std::move(s.openShaderGraphRequest);
        s.openShaderGraphRequest.clear();
        open(s, uri);
    }
    if (!s.showShaderGraph) return;
    ImGui::SetNextWindowSize({1100, 640}, ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("Graphe de shader", &s.showShaderGraph)) {
        ImGui::End();
        return;
    }
    if (m_uri.empty()) {
        ImGui::TextDisabled("Ouvrez un graphe (.agrshadergraph) depuis les assets, ou créez-en un avec « Nouveau graphe de shader ».");
        ImGui::End();
        return;
    }
    s.shaderPreviewMaterial = kPreviewMaterial;
    // ---- toolbar
    ImGui::TextUnformatted((m_uri.substr(m_uri.find_last_of('/') + 1) + (m_dirty ? " *" : "")).c_str());
    ImGui::SameLine();
    ImGui::TextDisabled(m_graph.target == ShaderGraph::Target::Sprite ? "(sprite)" : "(surface 3D)");
    ImGui::SameLine();
    if (ImGui::Button("Enregistrer")) save(s);
    ImGui::SameLine();
    ImGui::BeginDisabled(m_undo.empty());
    if (ImGui::Button("Annuler")) undo(s);
    ImGui::EndDisabled();
    ImGui::SameLine();
    ImGui::BeginDisabled(m_redo.empty());
    if (ImGui::Button("Rétablir")) redo(s);
    ImGui::EndDisabled();
    ImGui::SameLine();
    if (m_result.ok() && m_status.usable) ImGui::TextColored(palette::toVec4(palette::kSuccess), "Compilé");
    else ImGui::TextColored(palette::toVec4(palette::kError), "%zu erreur(s)", m_result.errors.size() + m_status.diagnostics.size());

    ImGui::BeginChild("##sgSide", {280, 0}, ImGuiChildFlags_ResizeX);
    if (const rhi::TextureHandle tex = preview.texture(s)) {
        const float w = std::min(ImGui::GetContentRegionAvail().x, 256.0f);
        const bool flip = s.device() && s.device()->info().renderTargetsFlipY;
        ImGui::Image(ImTextureRef(ImGuiRenderer::toImTexture(tex)), {w, w}, {0, flip ? 1.0f : 0.0f}, {1, flip ? 0.0f : 1.0f});
    }
    for (const std::string& e : m_result.errors) ImGui::TextColored(palette::toVec4(palette::kError), "%s", e.c_str());
    for (const auto& d : m_status.diagnostics) ImGui::TextColored(palette::toVec4(palette::kError), "GPU : %s", d.message.c_str());
    if (ImGui::CollapsingHeader("Code généré")) {
        ImGui::PushFont(s.fonts.mono, 0.0f);
        ImGui::TextUnformatted(m_result.source.c_str());
        ImGui::PopFont();
    }
    ImGui::TextDisabled("Clic droit : ajouter un nœud. Glisser d'une\nsortie vers une entrée : relier. Suppr : effacer\nle nœud choisi. Ctrl+Z / Ctrl+Y, Ctrl+S.");
    ImGui::EndChild();
    ImGui::SameLine();
    ImGui::BeginChild("##sgCanvas", {0, 0}, ImGuiChildFlags_Borders, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
    drawCanvas(s);
    ImGui::EndChild();

    if (ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows) && !ImGui::GetIO().WantTextInput) {
        const bool ctrl = ImGui::GetIO().KeyCtrl;
        if (ctrl && ImGui::IsKeyPressed(ImGuiKey_S, false)) save(s);
        if (ctrl && ImGui::IsKeyPressed(ImGuiKey_Z, false)) undo(s);
        if (ctrl && ImGui::IsKeyPressed(ImGuiKey_Y, false)) redo(s);
        if (m_selected && ImGui::IsKeyPressed(ImGuiKey_Delete, false)) {
            const ShaderGraph::Node* n = m_graph.node(m_selected);
            const bool output = n && (n->type == "SurfaceOutput" || n->type == "SpriteOutput");
            if (!output) {
                snapshot();
                m_graph.removeNode(m_selected);
                m_selected = 0;
                changed(s);
            }
        }
    }
    ImGui::End();
}

void ShaderGraphPanel::drawCanvas(EditorShared& s) {
    ImGuiIO& io = ImGui::GetIO();
    ImDrawList* dl = ImGui::GetWindowDrawList();
    const ImVec2 origin = ImGui::GetCursorScreenPos();
    const ImVec2 size = ImGui::GetContentRegionAvail();
    const float z = m_zoom;
    auto toScreen = [&](float x, float y) { return ImVec2{origin.x + (x + m_pan.x) * z, origin.y + (y + m_pan.y) * z}; };
    auto toCanvas = [&](ImVec2 p) { return ImVec2{(p.x - origin.x) / z - m_pan.x, (p.y - origin.y) / z - m_pan.y}; };
    dl->AddRectFilled(origin, {origin.x + size.x, origin.y + size.y}, palette::kGraphite950);
    const float step = 32.0f * z;
    for (float x = std::fmod(m_pan.x * z, step); x < size.x; x += step) dl->AddLine({origin.x + x, origin.y}, {origin.x + x, origin.y + size.y}, IM_COL32(255, 255, 255, 14));
    for (float y = std::fmod(m_pan.y * z, step); y < size.y; y += step) dl->AddLine({origin.x, origin.y + y}, {origin.x + size.x, origin.y + y}, IM_COL32(255, 255, 255, 14));

    ImGui::SetCursorScreenPos(origin);
    ImGui::SetNextItemAllowOverlap();
    ImGui::InvisibleButton("##canvas", size, ImGuiButtonFlags_MouseButtonLeft | ImGuiButtonFlags_MouseButtonRight | ImGuiButtonFlags_MouseButtonMiddle);
    const bool canvasHovered = ImGui::IsItemHovered();
    if (ImGui::IsItemActive() && (ImGui::IsMouseDragging(ImGuiMouseButton_Left, 2.0f) || ImGui::IsMouseDragging(ImGuiMouseButton_Middle, 0.0f) ||
                                  ImGui::IsMouseDragging(ImGuiMouseButton_Right, 4.0f))) {
        m_pan.x += io.MouseDelta.x / z;
        m_pan.y += io.MouseDelta.y / z;
        m_panned = true;
    }
    if (ImGui::IsItemClicked(ImGuiMouseButton_Left)) m_selected = 0;
    if (canvasHovered && io.MouseWheel != 0.0f) {
        const ImVec2 before = toCanvas(io.MousePos);
        m_zoom = std::clamp(m_zoom * (io.MouseWheel > 0 ? 1.1f : 1.0f / 1.1f), 0.5f, 1.6f);
        m_pan.x = (io.MousePos.x - origin.x) / m_zoom - before.x;
        m_pan.y = (io.MousePos.y - origin.y) / m_zoom - before.y;
    }
    if (canvasHovered && ImGui::IsMouseReleased(ImGuiMouseButton_Right) && !m_panned) {
        m_menuPos = toCanvas(io.MousePos);
        m_pendingFrom = {};
        m_filter.clear();
        ImGui::OpenPopup("##sgAdd");
    }
    if (!ImGui::IsMouseDown(ImGuiMouseButton_Right) && !ImGui::IsMouseDown(ImGuiMouseButton_Left)) m_panned = false;

    ImGui::PushFont(nullptr, ImGui::GetStyle().FontSizeBase * z);
    const float row = ImGui::GetFrameHeight() + 4.0f * z;
    const float width = kNodeWidth * z;
    m_pins.clear();
    dl->ChannelsSplit(2);
    std::vector<int> order;
    for (const ShaderGraph::Node& n : m_graph.nodes) order.push_back(n.id);
    for (int id : order) {
        ShaderGraph::Node* n = m_graph.node(id);
        if (!n) continue;
        const ShaderNodeDef* d = findShaderNodeDef(n->type);
        if (!d) continue;
        ImGui::PushID(id);
        const ImVec2 p0 = toScreen(n->x, n->y);
        dl->ChannelsSetCurrent(1);
        ImGui::SetCursorScreenPos(p0);
        ImGui::InvisibleButton("##title", {width, row});
        if (ImGui::IsItemActivated()) {
            m_selected = id;
            snapshot();
            m_moved = false;
        }
        if (ImGui::IsItemActive() && ImGui::IsMouseDragging(ImGuiMouseButton_Left, 1.0f)) {
            n->x += io.MouseDelta.x / z;
            n->y += io.MouseDelta.y / z;
            m_moved = true;
            m_dirty = true;
        }
        if (ImGui::IsItemDeactivated() && !m_moved && !m_undo.empty()) m_undo.pop_back();
        if (ImGui::IsItemHovered() && !d->tooltip.empty()) ImGui::SetTooltip("%s", d->tooltip.c_str());
        dl->AddText({p0.x + 8.0f * z, p0.y + (row - ImGui::GetFontSize()) * 0.5f}, IM_COL32_WHITE, d->title.c_str());
        float y = p0.y + row;
        const float labelW = 80.0f * z, widgetW = width - labelW - 14.0f * z;
        auto editValue = [&](const ShaderPortDef& p, const std::string& wid) {
            std::string& v = n->values[p.id];
            if (v.empty()) v = p.defaultValue;
            const bool isColor = (n->type == "Color" || n->type == "ParamColor") && p.id == "value";
            if (p.id == "name" || p.id == "path") {
                if (ImGui::InputText(wid.c_str(), &v)) changed(s);
                if (ImGui::IsItemActivated()) snapshot();
            } else if (p.id == "srgb") {
                bool b = v != "false";
                if (ImGui::Checkbox(wid.c_str(), &b)) {
                    snapshot();
                    v = b ? "true" : "false";
                    changed(s);
                }
            } else if (isColor) {
                std::vector<float> c = parse(v, 4);
                if (ImGui::ColorEdit4(wid.c_str(), c.data(), ImGuiColorEditFlags_NoInputs)) {
                    v = format(c.data(), 4);
                    changed(s);
                }
                if (ImGui::IsItemActivated()) snapshot();
            } else if (v.rfind("@", 0) == 0) {
                ImGui::TextDisabled("UV du modèle");
            } else {
                const int count = p.type == PT::Dynamic ? 1 : components(p.type);
                std::vector<float> c = parse(v, count);
                bool edited = false;
                switch (count) {
                    case 2: edited = ImGui::DragFloat2(wid.c_str(), c.data(), 0.01f, 0, 0, "%g"); break;
                    case 3: edited = ImGui::DragFloat3(wid.c_str(), c.data(), 0.01f, 0, 0, "%g"); break;
                    case 4: edited = ImGui::DragFloat4(wid.c_str(), c.data(), 0.01f, 0, 0, "%g"); break;
                    default: edited = ImGui::DragFloat(wid.c_str(), c.data(), 0.01f, 0, 0, "%g"); break;
                }
                if (ImGui::IsItemActivated()) snapshot();
                if (edited) {
                    v = format(c.data(), count);
                    changed(s);
                }
            }
        };
        for (const ShaderPortDef& f : d->fields) {
            ImGui::SetCursorScreenPos({p0.x + 8.0f * z, y + 2.0f * z});
            ImGui::TextUnformatted(f.label.c_str());
            ImGui::SetCursorScreenPos({p0.x + labelW, y + 2.0f * z});
            ImGui::SetNextItemWidth(widgetW);
            editValue(f, "##f" + f.id);
            y += row;
        }
        for (const ShaderPortDef& p : d->inputs) {
            m_pins.push_back({id, p.id, false, p.type, {p0.x, y + row * 0.5f}});
            ImGui::SetCursorScreenPos({p0.x + 12.0f * z, y + 2.0f * z});
            ImGui::TextUnformatted(p.label.c_str());
            if (!m_graph.linkTo(id, p.id) && p.type != PT::Sampler) {
                const bool output = n->type == "SurfaceOutput" || n->type == "SpriteOutput";
                ImGui::SetCursorScreenPos({p0.x + labelW, y + 2.0f * z});
                ImGui::SetNextItemWidth(widgetW);
                if (output) ImGui::TextDisabled("(matériau)");
                else editValue(p, "##i" + p.id);
            }
            y += row;
        }
        for (const ShaderPortDef& p : d->outputs) {
            m_pins.push_back({id, p.id, true, p.type, {p0.x + width, y + row * 0.5f}});
            const ImVec2 ts = ImGui::CalcTextSize(p.label.c_str());
            dl->AddText({p0.x + width - ts.x - 14.0f * z, y + (row - ts.y) * 0.5f}, palette::kText, p.label.c_str());
            y += row;
        }
        const ImVec2 p1{p0.x + width, std::max(y, p0.y + row * 1.5f) + 4.0f * z};
        dl->ChannelsSetCurrent(0);
        dl->AddRectFilled(p0, p1, IM_COL32(32, 36, 44, 240), 6.0f * z);
        dl->AddRectFilled(p0, {p1.x, p0.y + row}, categoryColor(d->category), 6.0f * z, ImDrawFlags_RoundCornersTop);
        const std::string tag = "(n° " + std::to_string(id) + ")";
        const bool hasError = std::any_of(m_result.errors.begin(), m_result.errors.end(), [&](const std::string& e) { return e.find(tag) != std::string::npos; });
        dl->AddRect(p0, p1, hasError ? palette::kError : (m_selected == id ? palette::kSelect : IM_COL32(0, 0, 0, 160)), 6.0f * z,
                    m_selected == id || hasError ? 2.5f : 1.0f);
        ImGui::PopID();
    }
    dl->ChannelsSetCurrent(1);
    auto pinPos = [&](int node, const std::string& port, bool output) -> const PinPos* {
        for (const PinPos& p : m_pins)
            if (p.node == node && p.port == port && p.output == output) return &p;
        return nullptr;
    };
    auto curve = [&](ImVec2 a, ImVec2 b, ImU32 col, float thick) {
        const float dx = std::max(40.0f * z, std::fabs(b.x - a.x) * 0.5f);
        dl->AddBezierCubic(a, {a.x + dx, a.y}, {b.x - dx, b.y}, b, col, thick * z);
    };
    for (const ShaderGraph::Link& l : m_graph.links) {
        const PinPos* a = pinPos(l.fromNode, l.fromPort, true);
        const PinPos* b = pinPos(l.toNode, l.toPort, false);
        if (a && b) curve(a->pos, b->pos, typeColor(a->type), 2.0f);
    }
    const PinPos* hoveredPin = nullptr;
    for (const PinPos& p : m_pins) {
        const float r = kPinRadius * z;
        const bool linked = p.output ? std::any_of(m_graph.links.begin(), m_graph.links.end(),
                                                   [&](const ShaderGraph::Link& l) { return l.fromNode == p.node && l.fromPort == p.port; })
                                     : m_graph.linkTo(p.node, p.port) != nullptr;
        const float dx = io.MousePos.x - p.pos.x, dy = io.MousePos.y - p.pos.y;
        const bool hover = dx * dx + dy * dy < (r + 5.0f * z) * (r + 5.0f * z) && ImGui::IsWindowHovered(ImGuiHoveredFlags_AllowWhenBlockedByActiveItem);
        if (hover) hoveredPin = &p;
        if (linked) dl->AddCircleFilled(p.pos, r, typeColor(p.type));
        else dl->AddCircle(p.pos, r, typeColor(p.type), 0, 2.0f * z);
        if (hover) dl->AddCircle(p.pos, r + 3.0f * z, IM_COL32_WHITE, 0, 1.5f * z);
    }
    if (hoveredPin && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
        snapshot();
        if (!hoveredPin->output) {
            if (const ShaderGraph::Link* l = m_graph.linkTo(hoveredPin->node, hoveredPin->port)) {
                m_drag = {l->fromNode, l->fromPort, true, true};
                m_graph.disconnect(hoveredPin->node, hoveredPin->port);
                changed(s);
            } else {
                m_drag = {hoveredPin->node, hoveredPin->port, false, true};
            }
        } else {
            m_drag = {hoveredPin->node, hoveredPin->port, true, true};
        }
    }
    if (m_drag.active) {
        if (const PinPos* from = pinPos(m_drag.node, m_drag.port, m_drag.output)) {
            if (m_drag.output) curve(from->pos, io.MousePos, typeColor(from->type), 2.5f);
            else curve(io.MousePos, from->pos, typeColor(from->type), 2.5f);
        }
        if (ImGui::IsMouseReleased(ImGuiMouseButton_Left)) {
            m_drag.active = false;
            if (hoveredPin && hoveredPin->output != m_drag.output) {
                const Status st = m_drag.output ? m_graph.connect(m_drag.node, m_drag.port, hoveredPin->node, hoveredPin->port)
                                                : m_graph.connect(hoveredPin->node, hoveredPin->port, m_drag.node, m_drag.port);
                if (!st) s.notify("Liaison impossible : " + st.error().message);
                changed(s);
            } else if (!hoveredPin) {
                m_pendingFrom = {m_drag.node, m_drag.port, m_drag.output};
                m_menuPos = toCanvas(io.MousePos);
                m_filter.clear();
                ImGui::OpenPopup("##sgAdd");
            }
        }
    }
    dl->ChannelsMerge();
    ImGui::PopFont();

    // ---- add-node menu (searchable, by category)
    ImGui::SetNextWindowSizeConstraints({240, 0}, {320, 460});
    if (ImGui::BeginPopup("##sgAdd")) {
        if (ImGui::IsWindowAppearing()) ImGui::SetKeyboardFocusHere();
        ImGui::InputTextWithHint("##q", "Rechercher un nœud", &m_filter);
        const u8 mask = m_graph.target == ShaderGraph::Target::Sprite ? 2 : 1;
        std::string category;
        auto lower = [](std::string t) {
            for (char& c : t) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
            return t;
        };
        const std::string q = lower(m_filter);
        for (const ShaderNodeDef& d : shaderNodeDefs()) {
            if (!(d.targets & mask) || d.type == "SurfaceOutput" || d.type == "SpriteOutput") continue;
            if (!q.empty() && lower(d.title + " " + d.type + " " + d.category).find(q) == std::string::npos) continue;
            if (d.category != category) {
                category = d.category;
                ImGui::SeparatorText(category.c_str());
            }
            if (ImGui::Selectable(d.title.c_str())) {
                addNode(s, d.type, m_menuPos);
                ImGui::CloseCurrentPopup();
            }
            if (ImGui::IsItemHovered() && !d.tooltip.empty()) ImGui::SetTooltip("%s", d.tooltip.c_str());
        }
        ImGui::EndPopup();
    } else if (!m_drag.active) {
        m_pendingFrom = {};
    }
}

} // namespace agr::editor
