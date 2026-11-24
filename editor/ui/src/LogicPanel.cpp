// Visual logic editor: the node canvas of a .agrlogic graph (events, flow, actions, values),
// links dragged between pins, add-node menu, variables, live compile errors, generated Lua,
// undo / redo, attach to the selected entity. The engine compiles the same graph at runtime.
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <filesystem>

#include <imgui.h>
#include <misc/cpp/imgui_stdlib.h>

#include "agr/editor/ui/Panels.hpp"
#include "agr/editor/ui/Theme.hpp"
#include "agr/script/ScriptComponent.hpp"

namespace agr::editor {

namespace {

using namespace agr::logic;

constexpr float kNodeWidth = 230.0f;
constexpr float kPinRadius = 6.0f;

ImU32 kindColor(PinKind k) {
    switch (k) {
        case PinKind::Exec: return IM_COL32(235, 235, 235, 255);
        case PinKind::Number: return IM_COL32(110, 200, 120, 255);
        case PinKind::Text: return IM_COL32(230, 120, 200, 255);
        case PinKind::Bool: return IM_COL32(230, 90, 90, 255);
        case PinKind::Entity: return IM_COL32(90, 170, 245, 255);
        case PinKind::Any: return IM_COL32(170, 170, 170, 255);
    }
    return IM_COL32_WHITE;
}

ImU32 categoryColor(const std::string& c) {
    if (c == "Événements") return IM_COL32(170, 60, 50, 255);
    if (c == "Flux") return IM_COL32(110, 110, 120, 255);
    if (c == "Actions") return IM_COL32(50, 100, 170, 255);
    if (c == "Effets") return IM_COL32(150, 90, 170, 255);
    if (c == "Valeurs") return IM_COL32(50, 130, 90, 255);
    return IM_COL32(160, 120, 40, 255);  // Maths
}

const char* kindLabel(PinKind k) {
    switch (k) {
        case PinKind::Number: return "Nombre";
        case PinKind::Text: return "Texte";
        case PinKind::Bool: return "Vrai / faux";
        case PinKind::Entity: return "Entité";
        default: return "Valeur";
    }
}

const char* kStarter = R"({"format": "agr.logic", "version": 1, "nextId": 3, "variables": [],
 "nodes": [{"id": 1, "type": "event.start", "x": 40, "y": 60, "values": {}},
           {"id": 2, "type": "act.log", "x": 320, "y": 60, "values": {"text": "Bonjour !"}}],
 "links": [{"from": 1, "fromPin": "next", "to": 2, "toPin": "in"}]})";

} // namespace

std::string LogicPanel::defaultGraph() { return kStarter; }

void LogicPanel::open(EditorShared& s, const std::string& uri) {
    auto text = s.engine.vfs().readText(uri);
    if (!text) {
        s.notify("Graphe introuvable : " + uri);
        return;
    }
    auto g = Graph::fromJson(text.value());
    if (!g) {
        s.notify(g.error().message);
        return;
    }
    m_uri = uri;
    m_graph = std::move(g.value());
    m_dirty = false;
    m_undo.clear();
    m_redo.clear();
    m_selected = 0;
    m_drag = {};
    recompile();
    s.showLogic = true;
}

bool LogicPanel::save(EditorShared& s) {
    if (m_uri.empty()) return false;
    if (Status st = s.engine.vfs().writeText(m_uri, m_graph.toJson()); !st) {
        s.notify("Enregistrement impossible : " + st.error().message);
        return false;
    }
    m_dirty = false;
    s.notify(m_result.ok() ? "Logique enregistrée : " + m_uri : "Logique enregistrée avec des erreurs : " + m_uri);
    return true;
}

void LogicPanel::snapshot() {
    m_undo.push_back(m_graph.toJson());
    if (m_undo.size() > 100) m_undo.erase(m_undo.begin());
    m_redo.clear();
}

void LogicPanel::changed() {
    m_dirty = true;
    recompile();
}

void LogicPanel::recompile() { m_result = compile(m_graph, m_uri); }

bool LogicPanel::undo() {
    if (m_undo.empty()) return false;
    m_redo.push_back(m_graph.toJson());
    auto g = Graph::fromJson(m_undo.back());
    m_undo.pop_back();
    if (g) m_graph = std::move(g.value());
    changed();
    return true;
}

bool LogicPanel::redo() {
    if (m_redo.empty()) return false;
    m_undo.push_back(m_graph.toJson());
    auto g = Graph::fromJson(m_redo.back());
    m_redo.pop_back();
    if (g) m_graph = std::move(g.value());
    changed();
    return true;
}

Status LogicPanel::attachToSelection(EditorShared& s) {
    const ecs::Entity e = s.ctx.scene().findByUuid(s.ctx.selection().primary());
    if (e.isNull()) return makeError(Error::Code::NotFound, "sélectionnez d'abord une entité");
    if (m_uri.empty()) return makeError(Error::Code::InvalidArgument, "enregistrez d'abord le graphe");
    if (!s.ctx.scene().registry().has<ScriptComponent>(e) && !s.ctx.addComponent(e, "Script"))
        return makeError(Error::Code::Unknown, "impossible d'ajouter le composant Script");
    auto& sc = s.ctx.scene().registry().get<ScriptComponent>(s.ctx.scene().findByUuid(s.ctx.selection().primary()));
    sc.script = m_uri;
    s.ctx.commit("Attacher une logique visuelle");
    return {};
}

int LogicPanel::addNode(const std::string& type, ImVec2 canvasPos) {
    snapshot();
    const int id = m_graph.addNode(type, canvasPos.x, canvasPos.y);
    if (id && m_pendingFrom.node) {  // a link was dropped on empty space: connect the first compatible pin
        const NodeDef* d = findNodeDef(type);
        for (const PinDef& p : m_pendingFrom.output ? d->inputs : d->outputs) {
            const Status st = m_pendingFrom.output ? m_graph.connect(m_pendingFrom.node, m_pendingFrom.pin, id, p.id)
                                                   : m_graph.connect(id, p.id, m_pendingFrom.node, m_pendingFrom.pin);
            if (st) break;
        }
    }
    m_pendingFrom = {};
    m_selected = id;
    changed();
    return id;
}

void LogicPanel::drawSidebar(EditorShared& s) {
    ImGui::TextUnformatted(m_uri.empty() ? "(aucun graphe)" : std::filesystem::path(m_uri).filename().string().c_str());
    if (m_dirty) {
        ImGui::SameLine();
        ImGui::TextColored(palette::toVec4(palette::kAmber), "*");
    }
    if (ImGui::Button("Nouveau")) {
        std::string folder = s.assetFolder.empty() ? "logic" : s.assetFolder;
        std::string uri;
        for (int i = 1; i < 1000; ++i) {
            uri = "assets://" + folder + "/logique" + (i > 1 ? "_" + std::to_string(i) : "") + ".agrlogic";
            if (!s.engine.vfs().exists(uri)) break;
        }
        if (Status st = s.engine.vfs().writeText(uri, kStarter); st) {
            open(s, uri);
            s.notify("Graphe créé : " + uri);
        } else {
            s.notify("Création impossible : " + st.error().message);
        }
    }
    ImGui::SameLine();
    ImGui::BeginDisabled(m_uri.empty());
    if (ImGui::Button("Enregistrer")) save(s);
    ImGui::SameLine();
    ImGui::BeginDisabled(m_undo.empty());
    if (ImGui::Button("<")) undo();
    ImGui::EndDisabled();
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) ImGui::SetTooltip("Annuler (Ctrl+Z)");
    ImGui::SameLine();
    ImGui::BeginDisabled(m_redo.empty());
    if (ImGui::Button(">")) redo();
    ImGui::EndDisabled();
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) ImGui::SetTooltip("Rétablir (Ctrl+Y)");
    if (ImGui::Button("Attacher à la sélection", {-1, 0})) {
        if (m_dirty) save(s);
        if (Status st = attachToSelection(s); st) s.notify("Logique attachée : " + m_uri);
        else s.notify("Attacher : " + st.error().message);
    }
    ImGui::EndDisabled();
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort))
        ImGui::SetTooltip("Ajoute un composant Script à l'entité sélectionnée et y branche ce graphe");

    // ---- variables
    ImGui::SeparatorText("Variables");
    static const PinKind kinds[] = {PinKind::Number, PinKind::Text, PinKind::Bool};
    for (usize i = 0; i < m_graph.variables.size(); ++i) {
        Variable& v = m_graph.variables[i];
        ImGui::PushID(static_cast<int>(i));
        ImGui::SetNextItemWidth(90);
        if (ImGui::InputText("##n", &v.name)) changed();
        if (ImGui::IsItemActivated()) snapshot();
        ImGui::SameLine();
        ImGui::SetNextItemWidth(80);
        if (ImGui::BeginCombo("##t", kindLabel(v.type))) {
            for (PinKind k : kinds)
                if (ImGui::Selectable(kindLabel(k), v.type == k)) {
                    snapshot();
                    v.type = k;
                    changed();
                }
            ImGui::EndCombo();
        }
        ImGui::SameLine();
        ImGui::SetNextItemWidth(60);
        if (ImGui::InputText("##v", &v.value)) changed();
        if (ImGui::IsItemActivated()) snapshot();
        ImGui::SameLine();
        if (ImGui::SmallButton("x")) {
            snapshot();
            m_graph.variables.erase(m_graph.variables.begin() + static_cast<std::ptrdiff_t>(i));
            changed();
            ImGui::PopID();
            break;
        }
        ImGui::PopID();
    }
    if (ImGui::SmallButton("+ Variable")) {
        snapshot();
        m_graph.variables.push_back({"variable" + std::to_string(m_graph.variables.size() + 1), PinKind::Number, "0"});
        changed();
    }
    ImGui::TextDisabled("Modifiables par entité dans l'inspecteur.");

    // ---- compile result
    ImGui::SeparatorText("Vérification");
    if (m_result.ok()) ImGui::TextColored(palette::toVec4(palette::kSuccess), "Aucune erreur");
    for (const std::string& e : m_result.errors) {
        ImGui::PushStyleColor(ImGuiCol_Text, palette::toVec4(palette::kError));
        ImGui::TextWrapped("%s", e.c_str());
        ImGui::PopStyleColor();
    }
    for (const std::string& w : m_result.warnings) {
        ImGui::PushStyleColor(ImGuiCol_Text, palette::toVec4(palette::kWarning));
        ImGui::TextWrapped("%s", w.c_str());
        ImGui::PopStyleColor();
    }
    if (ImGui::CollapsingHeader("Code Lua généré")) {
        ImGui::PushFont(s.fonts.mono, 0.0f);
        std::string code = m_result.lua;
        ImGui::InputTextMultiline("##lua", &code, {-1, 260}, ImGuiInputTextFlags_ReadOnly);
        ImGui::PopFont();
    }
    ImGui::SeparatorText("Aide");
    if (ImGui::SmallButton("Guide complet de la logique visuelle")) s.openDocRequest = "LOGIC.md";
    ImGui::TextWrapped("Clic droit (ou appui long) sur la toile : ajouter un nœud. Tirez d'une broche à une autre pour les relier ; "
                       "tirez une liaison dans le vide pour créer le nœud suivant. Glisser la toile : déplacer la vue ; molette : zoom. "
                       "Suppr : effacer le nœud choisi ; Ctrl+D : le dupliquer.");
}

void LogicPanel::draw(EditorShared& s) {
    if (!s.openLogicRequest.empty()) {
        const std::string uri = std::move(s.openLogicRequest);
        s.openLogicRequest.clear();
        if (uri != m_uri) {
            if (m_dirty) save(s);  // never lose edits silently: the previous graph is saved
            open(s, uri);
        }
        s.showLogic = true;
        ImGui::SetWindowFocus("Logique visuelle");
    }
    if (!s.showLogic) return;
    if (!ImGui::Begin("Logique visuelle", &s.showLogic)) {
        ImGui::End();
        return;
    }
    ImGui::BeginChild("##logicSide", {270, 0}, ImGuiChildFlags_ResizeX);
    drawSidebar(s);
    ImGui::EndChild();
    ImGui::SameLine();
    ImGui::BeginChild("##logicCanvas", {0, 0}, ImGuiChildFlags_Borders, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
    if (m_uri.empty()) {
        ImGui::TextDisabled("Ouvrez un graphe (.agrlogic) depuis les assets, ou « Nouveau ».");
        ImGui::TextDisabled("Un graphe se branche sur une entité comme un script : les événements (démarrage, touches,");
        ImGui::TextDisabled("collisions, minuteurs...) déclenchent des actions, sans écrire de code.");
    } else {
        drawCanvas(s);
    }
    ImGui::EndChild();
    // Shortcuts while the panel has focus.
    if (ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows) && !ImGui::GetIO().WantTextInput) {
        const bool ctrl = ImGui::GetIO().KeyCtrl;
        if (ctrl && ImGui::IsKeyPressed(ImGuiKey_S, false)) save(s);
        if (ctrl && ImGui::IsKeyPressed(ImGuiKey_Z, false)) undo();
        if (ctrl && ImGui::IsKeyPressed(ImGuiKey_Y, false)) redo();
        if (m_selected && ImGui::IsKeyPressed(ImGuiKey_Delete, false)) {
            snapshot();
            m_graph.removeNode(m_selected);
            m_selected = 0;
            changed();
        }
        if (m_selected && ctrl && ImGui::IsKeyPressed(ImGuiKey_D, false)) {
            if (const Node* n = m_graph.node(m_selected)) {
                snapshot();
                const Node copy = *n;
                const int id = m_graph.addNode(copy.type, copy.x + 30, copy.y + 30);
                if (Node* c = m_graph.node(id)) c->values = copy.values;
                m_selected = id;
                changed();
            }
        }
    }
    ImGui::End();
}

void LogicPanel::drawCanvas(EditorShared& s) {
    (void)s;
    ImGuiIO& io = ImGui::GetIO();
    ImDrawList* dl = ImGui::GetWindowDrawList();
    const ImVec2 origin = ImGui::GetCursorScreenPos();
    const ImVec2 size = ImGui::GetContentRegionAvail();
    const float z = m_zoom;
    auto toScreen = [&](float x, float y) { return ImVec2{origin.x + (x + m_pan.x) * z, origin.y + (y + m_pan.y) * z}; };
    auto toCanvas = [&](ImVec2 p) { return ImVec2{(p.x - origin.x) / z - m_pan.x, (p.y - origin.y) / z - m_pan.y}; };

    // Background grid.
    dl->AddRectFilled(origin, {origin.x + size.x, origin.y + size.y}, palette::kGraphite950);
    const float step = 32.0f * z;
    for (float x = std::fmod(m_pan.x * z, step); x < size.x; x += step) dl->AddLine({origin.x + x, origin.y}, {origin.x + x, origin.y + size.y}, IM_COL32(255, 255, 255, 14));
    for (float y = std::fmod(m_pan.y * z, step); y < size.y; y += step) dl->AddLine({origin.x, origin.y + y}, {origin.x + size.x, origin.y + y}, IM_COL32(255, 255, 255, 14));

    // Canvas background item: panning, zoom, context menu (drawn first: nodes on top take the clicks).
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
        const float nz = m_zoom;
        m_pan.x = (io.MousePos.x - origin.x) / nz - before.x;
        m_pan.y = (io.MousePos.y - origin.y) / nz - before.y;
    }
    if (ImGui::IsItemHovered() && ImGui::IsMouseReleased(ImGuiMouseButton_Right) && !m_panned) {
        m_menuPos = toCanvas(io.MousePos);
        m_pendingFrom = {};
        ImGui::OpenPopup("##addNode");
    }
    if (!ImGui::IsMouseDown(ImGuiMouseButton_Right) && !ImGui::IsMouseDown(ImGuiMouseButton_Left)) m_panned = false;

    // ---- comment frames (behind the nodes): drag the title to move it with its nodes, the corner to resize.
    for (int ci = 0; ci < static_cast<int>(m_graph.comments.size()); ++ci) {
        logic::Comment& c = m_graph.comments[static_cast<usize>(ci)];
        const ImVec2 p0 = toScreen(c.x, c.y), p1 = toScreen(c.x + c.w, c.y + c.h);
        const float titleH = 24.0f * z;
        const ImU32 rgb = IM_COL32((c.color >> 16) & 0xFF, (c.color >> 8) & 0xFF, c.color & 0xFF, 0);
        dl->AddRectFilled(p0, p1, rgb | IM_COL32(0, 0, 0, 40), 6.0f * z);
        dl->AddRectFilled(p0, {p1.x, p0.y + titleH}, rgb | IM_COL32(0, 0, 0, 150), 6.0f * z, ImDrawFlags_RoundCornersTop);
        dl->AddRect(p0, p1, rgb | IM_COL32(0, 0, 0, 200), 6.0f * z);
        dl->AddText(ImGui::GetFont(), ImGui::GetFontSize() * z, {p0.x + 8.0f * z, p0.y + 4.0f * z}, palette::kText, c.text.c_str());
        ImGui::PushID(100000 + ci);
        ImGui::SetCursorScreenPos(p0);
        ImGui::InvisibleButton("##title", {std::max(p1.x - p0.x - 18.0f * z, 1.0f), titleH});
        if (ImGui::IsItemActivated()) {
            snapshot();
            m_commentDrag = ci;
            m_commentNodes.clear();
            for (const logic::Node& n : m_graph.nodes)
                if (n.x >= c.x && n.y >= c.y && n.x < c.x + c.w && n.y < c.y + c.h) m_commentNodes.push_back(n.id);
        }
        if (ImGui::IsItemActive() && m_commentDrag == ci && (io.MouseDelta.x != 0.0f || io.MouseDelta.y != 0.0f)) {
            const float dx = io.MouseDelta.x / z, dy = io.MouseDelta.y / z;
            c.x += dx, c.y += dy;
            for (int id : m_commentNodes)
                if (logic::Node* n = m_graph.node(id)) n->x += dx, n->y += dy;
            m_dirty = true;
        }
        if (ImGui::IsItemDeactivated() && m_commentDrag == ci) m_commentDrag = -1;
        if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) m_commentEdit = ci;
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Glisser : déplacer avec les nœuds dedans ; double-clic : renommer ; clic droit : couleur, supprimer");
        if (ImGui::BeginPopupContextItem("##commentMenu")) {
            static const std::pair<const char*, u32> colors[] = {{"Bleu", 0x3C6E9Bu}, {"Vert", 0x3C8B5Au}, {"Ambre", 0xB0802Cu}, {"Rouge", 0xA0453Cu}, {"Gris", 0x6A6F78u}};
            for (const auto& [name, value] : colors)
                if (ImGui::MenuItem(name, nullptr, c.color == value)) {
                    snapshot();
                    c.color = value;
                    m_dirty = true;
                }
            ImGui::Separator();
            if (ImGui::MenuItem("Supprimer le commentaire")) {
                snapshot();
                m_graph.comments.erase(m_graph.comments.begin() + ci);
                m_dirty = true;
                ImGui::EndPopup();
                ImGui::PopID();
                break;
            }
            ImGui::EndPopup();
        }
        const ImVec2 grip{p1.x - 14.0f * z, p1.y - 14.0f * z};
        ImGui::SetCursorScreenPos(grip);
        ImGui::InvisibleButton("##resize", {14.0f * z, 14.0f * z});
        dl->AddTriangleFilled({p1.x, grip.y}, p1, {grip.x, p1.y}, rgb | IM_COL32(0, 0, 0, 200));
        if (ImGui::IsItemActivated()) snapshot(), m_commentResize = ci;
        if (ImGui::IsItemActive() && m_commentResize == ci) {
            c.w = std::max(c.w + io.MouseDelta.x / z, 80.0f);
            c.h = std::max(c.h + io.MouseDelta.y / z, 50.0f);
            m_dirty = true;
        }
        if (ImGui::IsItemDeactivated()) m_commentResize = -1;
        ImGui::PopID();
    }
    if (m_commentEdit >= 0 && m_commentEdit < static_cast<int>(m_graph.comments.size())) {
        ImGui::OpenPopup("##commentText");
        if (ImGui::BeginPopup("##commentText")) {
            if (ImGui::IsWindowAppearing()) {
                snapshot();
                ImGui::SetKeyboardFocusHere();
            }
            if (ImGui::InputText("##text", &m_graph.comments[static_cast<usize>(m_commentEdit)].text, ImGuiInputTextFlags_EnterReturnsTrue)) {
                m_commentEdit = -1;
                ImGui::CloseCurrentPopup();
            }
            m_dirty = true;
            ImGui::EndPopup();
        } else {
            m_commentEdit = -1;
        }
    }

    // ---- nodes
    ImGui::PushFont(nullptr, ImGui::GetStyle().FontSizeBase * z);
    const float row = ImGui::GetFrameHeight() + 4.0f * z;
    const float width = kNodeWidth * z;
    m_pins.clear();
    dl->ChannelsSplit(2);
    std::vector<int> order;
    for (const Node& n : m_graph.nodes) order.push_back(n.id);
    bool structural = false;
    for (int id : order) {
        Node* n = m_graph.node(id);
        if (!n) continue;
        const NodeDef* d = findNodeDef(n->type);
        if (!d) continue;
        ImGui::PushID(id);
        const ImVec2 p0 = toScreen(n->x, n->y);
        dl->ChannelsSetCurrent(1);
        // Title (drag to move, click to select).
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
        if (ImGui::IsItemDeactivated() && !m_moved && !m_undo.empty()) m_undo.pop_back();  // a click, not a move
        if (ImGui::IsItemHovered() && !d->tooltip.empty()) ImGui::SetTooltip("%s", d->tooltip.c_str());
        dl->AddText({p0.x + 8.0f * z, p0.y + (row - ImGui::GetFontSize()) * 0.5f}, IM_COL32_WHITE, d->title.c_str());
        float y = p0.y + row;
        auto pinAt = [&](const PinDef& pin, bool output, float py) {
            const ImVec2 c{output ? p0.x + width : p0.x, py + row * 0.5f};
            m_pins.push_back({id, pin.id, output, pin.kind, c});
        };
        // Exec input and the "next" output on the title row.
        for (const PinDef& p : d->inputs)
            if (p.kind == PinKind::Exec) pinAt(p, false, p0.y);
        for (const PinDef& p : d->outputs)
            if (p.kind == PinKind::Exec && p.id == "next") pinAt(p, true, p0.y);
        auto widgetWidth = [&] { return width - 100.0f * z; };
        // Fields.
        for (const PinDef& f : d->fields) {
            ImGui::SetCursorScreenPos({p0.x + 8.0f * z, y + 2.0f * z});
            ImGui::TextUnformatted(f.label.c_str());
            ImGui::SameLine(0, 0);
            ImGui::SetCursorScreenPos({p0.x + 90.0f * z, y + 2.0f * z});
            ImGui::SetNextItemWidth(widgetWidth());
            std::string& v = n->values[f.id];
            if (!f.choices.empty()) {
                if (ImGui::BeginCombo(("##f" + f.id).c_str(), v.c_str())) {
                    for (const std::string& c : f.choices)
                        if (ImGui::Selectable(c.c_str(), c == v)) {
                            snapshot();
                            v = c;
                            changed();
                        }
                    ImGui::EndCombo();
                }
            } else {
                if (ImGui::InputText(("##f" + f.id).c_str(), &v)) changed();
                if (ImGui::IsItemActivated()) snapshot();
            }
            y += row;
        }
        // Data inputs (pin + inline value when unconnected).
        for (const PinDef& p : d->inputs) {
            if (p.kind == PinKind::Exec) continue;
            pinAt(p, false, y);
            ImGui::SetCursorScreenPos({p0.x + 12.0f * z, y + 2.0f * z});
            ImGui::TextUnformatted(p.label.c_str());
            if (!m_graph.linkTo(id, p.id)) {
                ImGui::SetCursorScreenPos({p0.x + 90.0f * z, y + 2.0f * z});
                ImGui::SetNextItemWidth(widgetWidth());
                std::string& v = n->values[p.id];
                const std::string wid = "##i" + p.id;
                if (p.kind == PinKind::Bool) {
                    bool b = v == "true";
                    if (ImGui::Checkbox(wid.c_str(), &b)) {
                        snapshot();
                        v = b ? "true" : "false";
                        changed();
                    }
                } else if (p.kind == PinKind::Number) {
                    float f = static_cast<float>(std::atof(v.c_str()));
                    if (ImGui::DragFloat(wid.c_str(), &f, 0.05f, 0.0f, 0.0f, "%g")) {
                        char buf[32];
                        std::snprintf(buf, sizeof buf, "%g", f);
                        v = buf;
                        changed();
                    }
                    if (ImGui::IsItemActivated()) snapshot();
                } else {
                    if (ImGui::InputTextWithHint(wid.c_str(), p.kind == PinKind::Entity ? "moi" : "", &v)) changed();
                    if (ImGui::IsItemActivated()) snapshot();
                }
            }
            y += row;
        }
        // Outputs (data, and exec outputs other than "next").
        for (const PinDef& p : d->outputs) {
            if (p.kind == PinKind::Exec && p.id == "next") continue;
            pinAt(p, true, y);
            const ImVec2 ts = ImGui::CalcTextSize(p.label.c_str());
            dl->AddText({p0.x + width - ts.x - 14.0f * z, y + (row - ts.y) * 0.5f}, palette::kText, p.label.c_str());
            y += row;
        }
        const ImVec2 p1{p0.x + width, std::max(y, p0.y + row * 1.5f) + 4.0f * z};
        dl->ChannelsSetCurrent(0);
        dl->AddRectFilled(p0, p1, IM_COL32(32, 36, 44, 240), 6.0f * z);
        dl->AddRectFilled(p0, {p1.x, p0.y + row}, categoryColor(d->category), 6.0f * z, ImDrawFlags_RoundCornersTop);
        const bool hasError = std::any_of(m_result.errors.begin(), m_result.errors.end(),
                                          [&](const std::string& e) { return e.find("(n° " + std::to_string(id) + ")") != std::string::npos; });
        dl->AddRect(p0, p1, hasError ? palette::kError : (m_selected == id ? palette::kSelect : IM_COL32(0, 0, 0, 160)), 6.0f * z,
                    m_selected == id || hasError ? 2.5f : 1.0f);
        ImGui::PopID();
        if (structural) break;
    }
    // ---- pins and links
    dl->ChannelsSetCurrent(1);
    auto pinPos = [&](int node, const std::string& pin, bool output) -> const PinPos* {
        for (const PinPos& p : m_pins)
            if (p.node == node && p.pin == pin && p.output == output) return &p;
        return nullptr;
    };
    auto curve = [&](ImVec2 a, ImVec2 b, ImU32 col, float thick) {
        const float dx = std::max(40.0f * z, std::fabs(b.x - a.x) * 0.5f);
        dl->AddBezierCubic(a, {a.x + dx, a.y}, {b.x - dx, b.y}, b, col, thick * z);
    };
    for (const Link& l : m_graph.links) {
        const PinPos* a = pinPos(l.fromNode, l.fromPin, true);
        const PinPos* b = pinPos(l.toNode, l.toPin, false);
        if (a && b) curve(a->pos, b->pos, kindColor(a->kind), a->kind == PinKind::Exec ? 3.0f : 2.0f);
    }
    const PinPos* hoveredPin = nullptr;
    for (const PinPos& p : m_pins) {
        const float r = kPinRadius * z;
        const bool linked = p.output ? m_graph.linkFrom(p.node, p.pin) != nullptr : m_graph.linkTo(p.node, p.pin) != nullptr;
        const float dx = io.MousePos.x - p.pos.x, dy = io.MousePos.y - p.pos.y;
        const bool hover = dx * dx + dy * dy < (r + 5.0f * z) * (r + 5.0f * z) && ImGui::IsWindowHovered(ImGuiHoveredFlags_AllowWhenBlockedByActiveItem);
        if (hover) hoveredPin = &p;
        if (p.kind == PinKind::Exec) {
            const ImVec2 c = p.pos;
            const ImVec2 t[3] = {{c.x - r, c.y - r}, {c.x + r, c.y}, {c.x - r, c.y + r}};
            if (linked) dl->AddTriangleFilled(t[0], t[1], t[2], kindColor(p.kind));
            else dl->AddTriangle(t[0], t[1], t[2], kindColor(p.kind), 1.5f * z);
        } else {
            if (linked) dl->AddCircleFilled(p.pos, r, kindColor(p.kind));
            else dl->AddCircle(p.pos, r, kindColor(p.kind), 0, 2.0f * z);
        }
        if (hover) dl->AddCircle(p.pos, r + 3.0f * z, IM_COL32_WHITE, 0, 1.5f * z);
    }
    // Start a link from a pin (an input already linked is detached and re-routed from its source).
    if (hoveredPin && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
        snapshot();
        if (!hoveredPin->output) {
            if (const Link* l = m_graph.linkTo(hoveredPin->node, hoveredPin->pin)) {
                m_drag = {l->fromNode, l->fromPin, true, hoveredPin->kind, true};
                m_graph.disconnect(hoveredPin->node, hoveredPin->pin);
                changed();
            } else {
                m_drag = {hoveredPin->node, hoveredPin->pin, false, hoveredPin->kind, true};
            }
        } else {
            m_drag = {hoveredPin->node, hoveredPin->pin, true, hoveredPin->kind, true};
        }
    }
    if (m_drag.active) {
        if (const PinPos* from = pinPos(m_drag.node, m_drag.pin, m_drag.output)) {
            if (m_drag.output) curve(from->pos, io.MousePos, kindColor(m_drag.kind), 2.5f);
            else curve(io.MousePos, from->pos, kindColor(m_drag.kind), 2.5f);
        }
        if (ImGui::IsMouseReleased(ImGuiMouseButton_Left)) {
            m_drag.active = false;
            if (hoveredPin && hoveredPin->output != m_drag.output) {
                const Status st = m_drag.output ? m_graph.connect(m_drag.node, m_drag.pin, hoveredPin->node, hoveredPin->pin)
                                                : m_graph.connect(hoveredPin->node, hoveredPin->pin, m_drag.node, m_drag.pin);
                if (!st) s.notify("Liaison impossible : " + st.error().message);
                changed();
            } else if (!hoveredPin) {  // dropped on empty space: create the next node
                m_pendingFrom = {m_drag.node, m_drag.pin, m_drag.output};
                m_menuPos = toCanvas(io.MousePos);
                ImGui::OpenPopup("##addNode");
            }
        }
    }
    dl->ChannelsMerge();
    ImGui::PopFont();

    // ---- add-node menu
    ImGui::SetNextWindowSizeConstraints({260, 0}, {320, 480});
    if (ImGui::BeginPopup("##addNode")) {
        if (ImGui::IsWindowAppearing()) {
            m_search.clear();
            ImGui::SetKeyboardFocusHere();
        }
        ImGui::InputTextWithHint("##search", "Rechercher un nœud…", &m_search);
        if (!m_pendingFrom.node && ImGui::Selectable("+ Commentaire (cadre)")) {
            snapshot();
            m_graph.comments.push_back({m_menuPos.x, m_menuPos.y, 320.0f, 200.0f, "Commentaire", 0x3C6E9Bu});
            m_dirty = true;
            ImGui::CloseCurrentPopup();
        }
        std::string lower = m_search;
        std::transform(lower.begin(), lower.end(), lower.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        std::string pick;
        const char* cats[] = {"Événements", "Flux", "Actions", "Effets", "Valeurs", "Maths"};
        for (const char* cat : cats) {
            bool header = false, open = true;
            for (const NodeDef& d : nodeCatalog()) {
                if (d.category != cat) continue;
                if (m_pendingFrom.node) {  // only nodes that can take the dragged link
                    const PinDef* pin = nullptr;
                    if (const Node* src = m_graph.node(m_pendingFrom.node))
                        for (const PinDef& p : m_pendingFrom.output ? findNodeDef(src->type)->outputs : findNodeDef(src->type)->inputs)
                            if (p.id == m_pendingFrom.pin) pin = &p;
                    bool fits = false;
                    for (const PinDef& p : m_pendingFrom.output ? d.inputs : d.outputs)
                        if (pin && (m_pendingFrom.output ? compatible(pin->kind, p.kind) : compatible(p.kind, pin->kind))) fits = true;
                    if (!fits) continue;
                }
                std::string t = d.title;
                std::transform(t.begin(), t.end(), t.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
                if (!lower.empty() && t.find(lower) == std::string::npos) continue;
                if (!header) {
                    header = true;
                    open = lower.empty() ? ImGui::TreeNodeEx(cat, ImGuiTreeNodeFlags_DefaultOpen * (m_pendingFrom.node != 0)) : true;
                    if (!lower.empty()) ImGui::TextDisabled("%s", cat);
                }
                if (open && ImGui::Selectable(d.title.c_str())) pick = d.type;
                if (open && ImGui::IsItemHovered() && !d.tooltip.empty()) ImGui::SetTooltip("%s", d.tooltip.c_str());
            }
            if (header && open && lower.empty()) ImGui::TreePop();
        }
        if (!pick.empty()) {
            addNode(pick, m_menuPos);
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    } else if (m_pendingFrom.node && !ImGui::IsPopupOpen("##addNode")) {
        m_pendingFrom = {};
    }
}

} // namespace agr::editor
