#include <algorithm>
#include <cctype>
#include <cstring>

#include <imgui.h>
#include <imgui_stdlib.h>

#include "agr/scene/Prefab.hpp"
#include "agr/editor/ui/Panels.hpp"

namespace agr::editor {

namespace {

bool containsCaseless(const std::string& text, const std::string& needle) {
    if (needle.empty()) return true;
    auto lower = [](std::string s) {
        std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        return s;
    };
    return lower(text).find(lower(needle)) != std::string::npos;
}

/// True when `e` or one of its descendants matches the filter (keeps parents visible).
bool subtreeMatches(Scene& scene, ecs::Entity e, const std::string& filter) {
    if (containsCaseless(scene.nameOf(e), filter)) return true;
    for (ecs::Entity c : scene.children(e))
        if (subtreeMatches(scene, c, filter)) return true;
    return false;
}

icons::Kind iconFor(Scene& scene, ecs::Entity e) {
    auto& reg = scene.registry();
    if (reg.has<CameraComponent>(e)) return icons::Kind::Camera;
    if (reg.has<DirectionalLightComponent>(e) || reg.has<EnvironmentComponent>(e)) return icons::Kind::Light;
    if (reg.has<SpriteRendererComponent>(e)) return icons::Kind::Image;
    return icons::Kind::Entity;
}

} // namespace

void HierarchyPanel::createMenu(EditorShared& s, ecs::Entity parent) {
    struct Item {
        const char* label;
        CreateKind kind;
    };
    static const Item basic[] = {{"Entité vide", CreateKind::Empty}};
    static const Item shapes[] = {{"Cube", CreateKind::Cube}, {"Sphère", CreateKind::Sphere}, {"Plan", CreateKind::Plane}, {"Quad", CreateKind::Quad}};
    static const Item twoD[] = {{"Sprite", CreateKind::Sprite}, {"Tilemap", CreateKind::Tilemap},
                                {"Particules 2D", CreateKind::Particles2D}, {"Lumière 2D", CreateKind::Light2D},
                                {"Couche de parallaxe", CreateKind::ParallaxLayer}};
    static const Item scene[] = {{"Caméra", CreateKind::Camera}, {"Lumière directionnelle", CreateKind::DirectionalLight},
                                 {"Lumière ponctuelle", CreateKind::PointLight}, {"Projecteur (spot)", CreateKind::SpotLight},
                                 {"Environnement (ciel, ambiance)", CreateKind::Environment}};
    auto items = [&](const Item* list, usize n) {
        for (usize i = 0; i < n; ++i)
            if (ImGui::MenuItem(list[i].label)) s.ctx.create(list[i].kind, parent);
    };
    items(basic, 1);
    ImGui::Separator();
    if (ImGui::BeginMenu("Formes 3D")) {
        items(shapes, 4);
        ImGui::EndMenu();
    }
    if (ImGui::BeginMenu("2D")) {
        items(twoD, 5);
        ImGui::EndMenu();
    }
    if (ImGui::BeginMenu("Caméra et lumières")) {
        items(scene, 5);
        ImGui::EndMenu();
    }
    static const Item ui[] = {{"Canvas", CreateKind::UICanvas}, {"Panneau", CreateKind::UIPanel}, {"Texte", CreateKind::UIText},
                              {"Bouton", CreateKind::UIButton}, {"Curseur", CreateKind::UISlider}, {"Colonne (mise en page)", CreateKind::UIColumn}};
    if (ImGui::BeginMenu("Interface (UI)")) {
        items(ui, 6);
        ImGui::EndMenu();
    }
    if (ImGui::MenuItem("Source audio")) s.ctx.create(CreateKind::AudioSource, parent);
}

void HierarchyPanel::contextMenu(EditorShared& s, ecs::Entity e) {
    Scene& scene = s.ctx.scene();
    const Uuid id = scene.uuidOf(e);
    if (!s.ctx.selection().contains(id)) s.ctx.selection().set(id);
    if (ImGui::BeginMenu("Créer un enfant")) {
        createMenu(s, e);
        ImGui::EndMenu();
    }
    ImGui::Separator();
    if (ImGui::MenuItem("Renommer", "F2")) {
        m_renaming = id;
        m_renameBuffer = scene.nameOf(e);
        m_renameFocus = true;
    }
    if (ImGui::MenuItem("Dupliquer", "Ctrl+D")) s.ctx.duplicateSelection();
    if (ImGui::MenuItem("Grouper la sélection", "Ctrl+G")) s.ctx.groupSelection();
    if (ImGui::MenuItem("Cadrer dans la vue", "F")) s.ctx.focusSelection();
    if (!scene.parent(e).isNull() && ImGui::MenuItem("Détacher du parent")) s.ctx.reparent(e, {});
    ImGui::Separator();
    // Prefabs.
    const bool playing = s.ctx.playState() != PlayState::Edit;
    if (prefab::isInstanceRoot(scene, e)) {
        if (ImGui::MenuItem("Appliquer au prefab", nullptr, false, !playing)) {
            if (Status st = s.ctx.applyPrefab(e); st) s.notify("Prefab mis à jour");
            else s.notify("Échec : " + st.error().message);
        }
        if (ImGui::MenuItem("Rétablir le prefab")) {
            if (Status st = s.ctx.revertPrefab(e); !st) s.notify("Échec : " + st.error().message);
        }
        if (ImGui::MenuItem("Dissocier du prefab")) s.ctx.unpackPrefab(e);
    } else if (ImGui::MenuItem("Créer un prefab", nullptr, false, !playing)) {
        if (auto uri = s.ctx.createPrefab(e)) s.notify("Prefab créé : " + uri.value());
        else s.notify("Échec : " + uri.error().message);
    }
    ImGui::Separator();
    if (ImGui::MenuItem("Supprimer", "Suppr")) s.ctx.deleteSelection();
}

void HierarchyPanel::drawEntity(EditorShared& s, ecs::Entity e) {
    Scene& scene = s.ctx.scene();
    if (!m_filter.empty() && !subtreeMatches(scene, e, m_filter)) return;
    auto& reg = scene.registry();
    const Uuid id = scene.uuidOf(e);
    m_visibleOrder.push_back(id);
    auto& state = reg.get<StateComponent>(e);
    const bool selected = s.ctx.selection().contains(id);
    const bool hasChildren = reg.get<HierarchyComponent>(e).childCount > 0;

    ImGui::PushID(static_cast<int>(id.value() ^ (id.value() >> 32)));

    // Thin drop zone above the row: reorder before this entity (same parent).
    const ImVec2 rowStart = ImGui::GetCursorScreenPos();
    const float rowRight = ImGui::GetWindowPos().x + ImGui::GetWindowWidth() - ImGui::GetStyle().WindowPadding.x -
                           (ImGui::GetScrollMaxY() > 0.0f ? ImGui::GetStyle().ScrollbarSize : 0.0f);
    ImGui::InvisibleButton("##before", {ImGui::GetContentRegionAvail().x, 3.0f});
    if (ImGui::BeginDragDropTarget()) {
        ImGui::GetWindowDrawList()->AddLine({rowStart.x, rowStart.y + 1}, {rowStart.x + ImGui::GetContentRegionAvail().x, rowStart.y + 1},
                                            palette::kAmber, 2.0f);
        if (const ImGuiPayload* p = ImGui::AcceptDragDropPayload(kPayloadEntity)) {
            const Uuid dragged{*static_cast<const u64*>(p->Data)};
            const ecs::Entity d = scene.findByUuid(dragged);
            if (!d.isNull() && d != e) s.ctx.reparent(d, scene.parent(e), e);
        }
        ImGui::EndDragDropTarget();
    }
    ImGui::SetCursorScreenPos({rowStart.x, rowStart.y + 3.0f});

    ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_OpenOnDoubleClick |
                               ImGuiTreeNodeFlags_SpanAvailWidth | ImGuiTreeNodeFlags_FramePadding | ImGuiTreeNodeFlags_DefaultOpen;
    if (!hasChildren) flags |= ImGuiTreeNodeFlags_Leaf;
    if (selected) flags |= ImGuiTreeNodeFlags_Selected;
    if (!m_filter.empty()) ImGui::SetNextItemOpen(true);

    const bool inactive = !scene.isActiveInHierarchy(e);
    if (inactive) ImGui::PushStyleColor(ImGuiCol_Text, palette::toVec4(palette::kTextMuted));
    const bool renaming = m_renaming == id;
    const bool open = ImGui::TreeNodeEx("##node", flags, "%s", renaming ? "" : "");
    const ImVec2 labelPos = {ImGui::GetItemRectMin().x + ImGui::GetTreeNodeToLabelSpacing(), ImGui::GetItemRectMin().y};
    const float rowH = ImGui::GetItemRectSize().y;

    // Selection (click / ctrl / shift range).
    if (ImGui::IsItemClicked(ImGuiMouseButton_Left) && !ImGui::IsItemToggledOpen()) {
        Selection& sel = s.ctx.selection();
        if (ImGui::GetIO().KeyCtrl) {
            sel.toggle(id);
            m_rangeAnchor = id;
        } else if (ImGui::GetIO().KeyShift && !m_rangeAnchor.isNull()) {
            // Range resolved at the end of the frame when the full visible order is known.
            sel.add(id);
            m_pendingRangeEnd = id;
        } else {
            sel.set(id);
            m_rangeAnchor = id;
        }
    }
    if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left) && !hasChildren) s.ctx.focusSelection();
    if (ImGui::BeginPopupContextItem("##ctx")) {
        contextMenu(s, e);
        ImGui::EndPopup();
    }
    // Drag source / drop target: drop on a row = make it a child.
    if (ImGui::BeginDragDropSource()) {
        const u64 raw = id.value();
        ImGui::SetDragDropPayload(kPayloadEntity, &raw, sizeof raw);
        ImGui::TextUnformatted(scene.nameOf(e).c_str());
        ImGui::EndDragDropSource();
    }
    if (ImGui::BeginDragDropTarget()) {
        if (const ImGuiPayload* p = ImGui::AcceptDragDropPayload(kPayloadEntity)) {
            const ecs::Entity d = scene.findByUuid(Uuid{*static_cast<const u64*>(p->Data)});
            if (!d.isNull() && d != e && !s.ctx.reparent(d, e)) s.notify("Impossible : cela créerait une boucle dans la hiérarchie");
        }
        if (const ImGuiPayload* p = ImGui::AcceptDragDropPayload(kPayloadAsset)) {
            const std::string uri = static_cast<const char*>(p->Data);
            if (isImageFile(uri)) s.ctx.createSprite(uri, {}, e);
            else if (isPrefabFile(uri)) {
                if (auto r = s.ctx.instantiatePrefab(uri, {}, e); !r) s.notify("Échec : " + r.error().message);
            }
        }
        ImGui::EndDragDropTarget();
    }

    // Icon + label (or inline rename field).
    ImDrawList* dl = ImGui::GetWindowDrawList();
    const float icon = rowH * 0.62f;
    icons::draw(dl, iconFor(scene, e), {labelPos.x + icon * 0.5f, labelPos.y + rowH * 0.5f}, icon,
                selected ? palette::kSelect : palette::kTextMuted);
    const float textX = labelPos.x + icon + 6.0f;
    if (renaming) {
        ImGui::SetCursorScreenPos({textX, labelPos.y});
        ImGui::SetNextItemWidth(std::max(80.0f, ImGui::GetContentRegionAvail().x - 60.0f));
        if (m_renameFocus) {
            ImGui::SetKeyboardFocusHere();
            m_renameFocus = false;
        }
        const bool done = ImGui::InputText("##rename", &m_renameBuffer, ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_AutoSelectAll);
        if (done) s.ctx.rename(e, m_renameBuffer);
        if (done || ImGui::IsItemDeactivated()) m_renaming = {};
    } else {
        // Prefab instances: root in blue, its entities in a lighter blue.
        ImU32 color = ImGui::GetColorU32(ImGuiCol_Text);
        if (!inactive) {
            if (prefab::isInstanceRoot(scene, e)) color = IM_COL32(110, 170, 255, 255);
            else if (!prefab::instanceRootOf(scene, e).isNull()) color = IM_COL32(160, 200, 255, 255);
        }
        dl->AddText({textX, labelPos.y + ImGui::GetStyle().FramePadding.y}, color, scene.nameOf(e).c_str());
    }
    if (inactive) ImGui::PopStyleColor();

    // Visibility (editor only) and lock toggles, right aligned.
    const float btn = rowH;
    ImGui::SetCursorScreenPos({rowRight - btn * 2.0f - 2.0f, labelPos.y});
    if (icons::button("##vis", state.hiddenInEditor ? icons::Kind::EyeClosed : icons::Kind::Eye, btn,
                      state.hiddenInEditor ? palette::kTextMuted : palette::kText, "Masquer dans la vue de l'éditeur")) {
        state.hiddenInEditor = !state.hiddenInEditor;
        s.ctx.commit(state.hiddenInEditor ? "Hide" : "Show");
    }
    ImGui::SameLine(0, 2.0f);
    if (icons::button("##lock", state.lockedInEditor ? icons::Kind::Lock : icons::Kind::Unlock, btn,
                      state.lockedInEditor ? palette::kAmber : palette::kTextMuted, "Verrouiller (non sélectionnable dans la vue)")) {
        state.lockedInEditor = !state.lockedInEditor;
        s.ctx.commit(state.lockedInEditor ? "Lock" : "Unlock");
    }

    if (open) {
        for (ecs::Entity c : scene.children(e)) drawEntity(s, c);
        ImGui::TreePop();
    }
    ImGui::PopID();
}

void HierarchyPanel::draw(EditorShared& s) {
    if (!ImGui::Begin("Hiérarchie")) {
        ImGui::End();
        return;
    }
    Scene& scene = s.ctx.scene();
    if (icons::button("##create", icons::Kind::Plus, ImGui::GetFrameHeight(), palette::kAmber, "Créer une entité"))
        ImGui::OpenPopup("##createRoot");
    if (ImGui::BeginPopup("##createRoot")) {
        createMenu(s, {});
        ImGui::EndPopup();
    }
    ImGui::SameLine();
    ImGui::SetNextItemWidth(-1);
    ImGui::InputTextWithHint("##filter", "Rechercher...", &m_filter);
    ImGui::Separator();

    if (s.renameSelection) {
        s.renameSelection = false;
        const ecs::Entity e = scene.findByUuid(s.ctx.selection().primary());
        if (!e.isNull()) {
            m_renaming = scene.uuidOf(e);
            m_renameBuffer = scene.nameOf(e);
            m_renameFocus = true;
        }
    }

    ImGui::BeginChild("##tree", {0, 0}, ImGuiChildFlags_None);
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, {4, 0});
    m_visibleOrder.clear();
    m_pendingRangeEnd = {};
    for (ecs::Entity r : scene.roots()) drawEntity(s, r);
    ImGui::PopStyleVar();

    // Shift-click range selection over the visible rows.
    if (!m_pendingRangeEnd.isNull()) {
        auto a = std::find(m_visibleOrder.begin(), m_visibleOrder.end(), m_rangeAnchor);
        auto b = std::find(m_visibleOrder.begin(), m_visibleOrder.end(), m_pendingRangeEnd);
        if (a != m_visibleOrder.end() && b != m_visibleOrder.end()) {
            if (a > b) std::swap(a, b);
            std::vector<Uuid> range(a, b + 1);
            s.ctx.selection().assign(range);
        }
    }

    // Empty area: click clears the selection, drop makes an entity a root, right click creates.
    ImGui::InvisibleButton("##empty", {std::max(1.0f, ImGui::GetContentRegionAvail().x), std::max(40.0f, ImGui::GetContentRegionAvail().y)});
    if (ImGui::IsItemClicked()) s.ctx.selection().clear();
    if (ImGui::BeginDragDropTarget()) {
        if (const ImGuiPayload* p = ImGui::AcceptDragDropPayload(kPayloadEntity)) {
            const ecs::Entity d = scene.findByUuid(Uuid{*static_cast<const u64*>(p->Data)});
            if (!d.isNull()) s.ctx.reparent(d, {});
        }
        if (const ImGuiPayload* p = ImGui::AcceptDragDropPayload(kPayloadAsset)) {
            const std::string uri = static_cast<const char*>(p->Data);
            if (isImageFile(uri)) s.ctx.createSprite(uri, {});
            else if (isPrefabFile(uri)) {
                if (auto r = s.ctx.instantiatePrefab(uri, {}); !r) s.notify("Échec : " + r.error().message);
            }
        }
        ImGui::EndDragDropTarget();
    }
    if (ImGui::BeginPopupContextItem("##emptyCtx")) {
        createMenu(s, {});
        ImGui::EndPopup();
    }
    ImGui::EndChild();
    ImGui::End();
}

} // namespace agr::editor
