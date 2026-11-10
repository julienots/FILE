#include <algorithm>
#include <cctype>
#include <cmath>
#include <map>

#include <imgui.h>
#include <imgui_stdlib.h>

#include "agr/editor/ui/ImGuiRenderer.hpp"
#include "agr/editor/ui/Panels.hpp"
#include "agr/render/SpriteSheet.hpp"
#include "agr/twod/Particles2D.hpp"
#include "agr/twod/SpriteAnimator.hpp"
#include "agr/twod/TilemapEditing.hpp"

namespace agr::editor {

namespace {

/// Result of one property widget: value changed this frame / edit finished
/// (drag released, text validated) -> one undo entry per user gesture.
struct Edit {
    bool changed = false;
    bool committed = false;
    void track() {
        changed |= ImGui::IsItemEdited();
        committed |= ImGui::IsItemDeactivatedAfterEdit();
    }
    void merge(const Edit& o) {
        changed |= o.changed;
        committed |= o.committed;
    }
};

void label(const char* text, const char* tooltip) {
    ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted(text);
    if (tooltip && *tooltip && ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort)) ImGui::SetTooltip("%s", tooltip);
    ImGui::TableNextColumn();
    ImGui::SetNextItemWidth(-FLT_MIN);
}

/// X / Y / Z drag fields with the axis colors of the gizmo.
bool dragAxes(const char* id, float* v, int n, float speed, float mn, float mx, const char* fmt) {
    static const ImU32 axis[] = {palette::kAxisX, palette::kAxisY, palette::kAxisZ, palette::kTextMuted};
    bool changed = false;
    const float w = (ImGui::GetContentRegionAvail().x - ImGui::GetStyle().ItemInnerSpacing.x * static_cast<float>(n - 1)) / static_cast<float>(n);
    ImGui::PushID(id);
    for (int i = 0; i < n; ++i) {
        ImGui::PushID(i);
        if (i) ImGui::SameLine(0, ImGui::GetStyle().ItemInnerSpacing.x);
        const ImVec2 p = ImGui::GetCursorScreenPos();
        ImGui::SetNextItemWidth(w);
        changed |= ImGui::DragFloat("##v", &v[i], speed, mn, mx, fmt);
        ImGui::GetWindowDrawList()->AddRectFilled(p, {p.x + 3.0f, p.y + ImGui::GetFrameHeight()}, axis[i], 2.0f);
        ImGui::PopID();
    }
    ImGui::PopID();
    return changed;
}

Edit drawField(EditorShared& s, const FieldInfo& f, void* comp, std::unordered_map<u64, math::Vec3>& eulerCache, u64 cacheKey) {
    Edit e;
    if (f.flags & FieldFlag_Hidden) return e;
    ImGui::TableNextRow();
    ImGui::TableNextColumn();
    ImGui::PushID(f.name.c_str());
    label(f.displayName.c_str(), f.tooltip.c_str());
    ImGui::BeginDisabled((f.flags & FieldFlag_ReadOnly) != 0);
    const float mn = f.hasRange() ? f.rangeMin : 0.0f, mx = f.hasRange() ? f.rangeMax : 0.0f;
    switch (f.type) {
        case FieldType::Bool:
            if (ImGui::Checkbox("##v", &f.ref<bool>(comp))) e.changed = e.committed = true;
            break;
        case FieldType::Int32:
            ImGui::DragInt("##v", &f.ref<i32>(comp), std::max(f.speed, 0.2f), static_cast<int>(mn), static_cast<int>(mx));
            e.track();
            break;
        case FieldType::UInt32: {
            int v = static_cast<int>(f.ref<u32>(comp));
            if (ImGui::DragInt("##v", &v, std::max(f.speed, 0.2f), static_cast<int>(mn), f.hasRange() ? static_cast<int>(mx) : INT32_MAX))
                f.ref<u32>(comp) = static_cast<u32>(std::max(0, v));
            e.track();
            break;
        }
        case FieldType::Float:
            if (f.flags & FieldFlag_Angle) {
                float deg = math::degrees(f.ref<float>(comp));
                if (ImGui::DragFloat("##v", &deg, 0.5f, math::degrees(mn), math::degrees(mx), "%.1f°"))
                    f.ref<float>(comp) = math::radians(deg);
            } else {
                ImGui::DragFloat("##v", &f.ref<float>(comp), f.speed, mn, mx, "%.3f");
            }
            e.track();
            break;
        case FieldType::Vec2: dragAxes("v2", &f.ref<math::Vec2>(comp).x, 2, f.speed, mn, mx, "%.3f"); e.track(); break;
        case FieldType::Vec3:
            if (f.flags & FieldFlag_Color) ImGui::ColorEdit3("##v", &f.ref<math::Vec3>(comp).x, ImGuiColorEditFlags_Float);
            else dragAxes("v3", &f.ref<math::Vec3>(comp).x, 3, f.speed, mn, mx, "%.3f");
            e.track();
            break;
        case FieldType::Vec4:
            if (f.flags & FieldFlag_Color) ImGui::ColorEdit4("##v", &f.ref<math::Vec4>(comp).x, ImGuiColorEditFlags_AlphaPreviewHalf);
            else dragAxes("v4", &f.ref<math::Vec4>(comp).x, 4, f.speed, mn, mx, "%.3f");
            e.track();
            break;
        case FieldType::Quat: {
            // Edited as Euler angles in degrees; the cache keeps the user's angles stable
            // (a quaternion has several Euler decompositions).
            math::Quat& q = f.ref<math::Quat>(comp);
            auto it = eulerCache.find(cacheKey);
            math::Vec3 deg;
            if (it != eulerCache.end() && math::nearlyEqual(math::Quat::fromEuler(it->second * (math::kPi / 180.0f)), q, 1e-5f)) {
                deg = it->second;
            } else {
                const math::Vec3 r = q.toEuler();
                deg = {math::degrees(r.x), math::degrees(r.y), math::degrees(r.z)};
            }
            if (dragAxes("rot", &deg.x, 3, 0.5f, 0.0f, 0.0f, "%.1f°")) q = math::Quat::fromEuler(deg * (math::kPi / 180.0f));
            eulerCache[cacheKey] = deg;
            e.track();
            break;
        }
        case FieldType::String: {
            std::string& str = f.ref<std::string>(comp);
            if (f.flags & FieldFlag_AssetPath) {
                // Thumbnail + path; images can be dropped from the asset browser.
                const float thumb = ImGui::GetFrameHeight() * 1.6f;
                if (!str.empty() && isImageFile(str)) {
                    ImGui::Image(ImTextureRef(s.thumbnail(str)), {thumb, thumb});
                    ImGui::SameLine();
                }
                ImGui::SetNextItemWidth(-FLT_MIN);
                ImGui::InputTextWithHint("##v", "Glisser un asset ici", &str);
                e.track();
                if (ImGui::BeginDragDropTarget()) {
                    if (const ImGuiPayload* p = ImGui::AcceptDragDropPayload(kPayloadAsset)) {
                        str = static_cast<const char*>(p->Data);
                        e.changed = e.committed = true;
                    }
                    ImGui::EndDragDropTarget();
                }
            } else {
                ImGui::InputText("##v", &str);
                e.track();
            }
            break;
        }
        case FieldType::Entity: {
            ecs::Entity& target = f.ref<ecs::Entity>(comp);
            Scene& scene = s.ctx.scene();
            const std::string name = scene.valid(target) ? scene.nameOf(target) : std::string("(aucune)");
            ImGui::Button(name.c_str(), {-ImGui::GetFrameHeight() - 4.0f, 0});
            if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort)) ImGui::SetTooltip("Glisser une entité depuis la hiérarchie");
            if (ImGui::BeginDragDropTarget()) {
                if (const ImGuiPayload* p = ImGui::AcceptDragDropPayload(kPayloadEntity)) {
                    target = scene.findByUuid(Uuid{*static_cast<const u64*>(p->Data)});
                    e.changed = e.committed = true;
                }
                ImGui::EndDragDropTarget();
            }
            ImGui::SameLine(0, 4.0f);
            if (ImGui::Button("x", {ImGui::GetFrameHeight(), 0})) {
                target = {};
                e.changed = e.committed = true;
            }
            break;
        }
        case FieldType::IntArray:
            ImGui::TextDisabled("%zu valeurs", f.ref<std::vector<i32>>(comp).size());
            break;
        case FieldType::Enum: {
            i32& v = f.ref<i32>(comp);
            const char* current = "?";
            for (const auto& en : f.enumEntries)
                if (en.value == v) current = en.name.c_str();
            if (ImGui::BeginCombo("##v", current)) {
                for (const auto& en : f.enumEntries) {
                    if (ImGui::Selectable(en.name.c_str(), en.value == v)) {
                        v = en.value;
                        e.changed = e.committed = true;
                    }
                }
                ImGui::EndCombo();
            }
            break;
        }
    }
    ImGui::EndDisabled();
    ImGui::PopID();
    return e;
}

/// Sheet referenced by a component, or a message explaining why there is none.
const SpriteSheet* sheetOrMessage(EditorShared& s, const std::string& uri) {
    if (uri.empty()) return nullptr;
    SpriteSheetLibrary* lib = s.sheets();
    const SpriteSheet* sheet = lib ? lib->get(uri) : nullptr;
    if (!sheet) {
        ImGui::PushStyleColor(ImGuiCol_Text, palette::toVec4(palette::kWarning));
        ImGui::TextWrapped("Feuille de sprites introuvable ou invalide : %s (détails dans la console)", uri.c_str());
        ImGui::PopStyleColor();
    }
    return sheet;
}

/// One frame of a sheet as an ImGui image (UV rectangle of the sheet texture).
void frameImage(EditorShared& s, const SpriteSheet& sheet, u32 frame, float size) {
    const math::Vec4 uv = sheet.uv(frame);
    const SpriteFrame& f = sheet.frames[frame];
    const float aspect = f.h ? static_cast<float>(f.w) / static_cast<float>(f.h) : 1.0f;
    const ImVec2 sz = aspect >= 1.0f ? ImVec2{size, size / aspect} : ImVec2{size * aspect, size};
    ImGui::Image(ImTextureRef(s.thumbnail(sheet.texture)), sz, {uv.x, uv.y}, {uv.z, uv.w});
}

/// Extra tools under the reflected fields of 2D components. Returns the edit made.
Edit drawComponentTools(EditorShared& s, const ComponentInfo& info, void* comp, ecs::Entity e) {
    Edit edit;
    Scene& scene = s.ctx.scene();
    auto& reg = scene.registry();
    if (info.name == "SpriteRenderer") {
        auto& sr = *static_cast<SpriteRendererComponent*>(comp);
        const SpriteSheet* sheet = sheetOrMessage(s, sr.sheet);
        if (!sheet) return edit;
        ImGui::PushID("spriteTools");
        if (sr.frame >= 0 && sheet->validFrame(sr.frame)) {
            frameImage(s, *sheet, static_cast<u32>(sr.frame), ImGui::GetFrameHeight() * 2.5f);
            ImGui::SameLine();
        }
        ImGui::BeginGroup();
        const std::string current = sheet->validFrame(sr.frame) ? sheet->frames[static_cast<u32>(sr.frame)].name : std::string("(aucune)");
        ImGui::SetNextItemWidth(-FLT_MIN);
        if (ImGui::BeginCombo("##frame", current.c_str(), ImGuiComboFlags_HeightLarge)) {
            for (u32 i = 0; i < sheet->frames.size(); ++i) {
                ImGui::PushID(static_cast<int>(i));
                const bool sel = static_cast<i32>(i) == sr.frame;
                if (ImGui::Selectable("##f", sel, 0, {0, ImGui::GetFrameHeight()})) {
                    sr.frame = static_cast<i32>(i);
                    edit.changed = edit.committed = true;
                }
                ImGui::SameLine();
                frameImage(s, *sheet, i, ImGui::GetFrameHeight());
                ImGui::SameLine();
                ImGui::TextUnformatted(sheet->frames[i].name.c_str());
                ImGui::PopID();
            }
            ImGui::EndCombo();
        }
        if (ImGui::Button("Taille native") && sheet->validFrame(sr.frame)) {
            const SpriteFrame& f = sheet->frames[static_cast<u32>(sr.frame)];
            sr.size = {static_cast<float>(f.w) / sheet->pixelsPerUnit, static_cast<float>(f.h) / sheet->pixelsPerUnit};
            edit.changed = edit.committed = true;
        }
        if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort))
            ImGui::SetTooltip("Taille = pixels de la frame / pixels par unité de la feuille (%.0f)", sheet->pixelsPerUnit);
        ImGui::SameLine();
        ImGui::TextDisabled("%zu frames, %zu clips", sheet->frames.size(), sheet->clips.size());
        ImGui::EndGroup();
        ImGui::PopID();
    } else if (info.name == "SpriteAnimator") {
        auto& anim = *static_cast<SpriteAnimatorComponent*>(comp);
        const auto* sr = reg.tryGet<SpriteRendererComponent>(e);
        if (!sr || sr->sheet.empty()) {
            ImGui::TextColored(palette::toVec4(palette::kWarning), "Nécessite un SpriteRenderer avec une feuille de sprites.");
            return edit;
        }
        const SpriteSheet* sheet = sheetOrMessage(s, sr->sheet);
        if (!sheet) return edit;
        ImGui::PushID("animTools");
        ImGui::SetNextItemWidth(-FLT_MIN);
        if (ImGui::BeginCombo("##clip", anim.clip.empty() ? "(choisir un clip)" : anim.clip.c_str())) {
            for (const SpriteClip& c : sheet->clips) {
                char label[160];
                std::snprintf(label, sizeof label, "%s  (%zu frames, %.0f i/s%s)", c.name.c_str(), c.frames.size(), c.fps, c.loop ? ", boucle" : "");
                if (ImGui::Selectable(label, c.name == anim.clip)) {
                    anim.clip = c.name;
                    edit.changed = edit.committed = true;
                }
            }
            ImGui::EndCombo();
        }
        if (!anim.clip.empty() && !sheet->findClip(anim.clip))
            ImGui::TextColored(palette::toVec4(palette::kWarning), "Clip « %s » absent de la feuille.", anim.clip.c_str());
        if (sheet->clips.empty()) ImGui::TextDisabled("Cette feuille ne définit aucun clip (champ \"clips\" du .agrsheet).");
        ImGui::TextDisabled("L'animation se joue en mode Jeu.");
        ImGui::PopID();
    } else if (info.name == "Tilemap") {
        auto& tm = *static_cast<TilemapComponent*>(comp);
        ImGui::PushID("tilemapTools");
        u32 painted = 0;
        for (i32 t : tm.tiles) painted += t >= 0 ? 1u : 0u;
        ImGui::TextDisabled("%u x %u cellules, %u peintes", tm.width, tm.height, painted);
        const SpriteSheet* sheet = sheetOrMessage(s, tm.tileset);
        if (!sheet) {
            if (tm.tileset.empty()) ImGui::TextDisabled("Déposez une feuille (.agrsheet) dans « Tileset » pour peindre.");
            ImGui::PopID();
            return edit;
        }
        TilemapBrush& brush = s.brush;
        const bool editMode = s.ctx.playState() == PlayState::Edit;
        ImGui::BeginDisabled(!editMode);
        const bool wasActive = brush.active;  // style pushes must match even if the button toggles it
        if (wasActive) {
            ImGui::PushStyleColor(ImGuiCol_Button, palette::toVec4(palette::kAmber));
            ImGui::PushStyleColor(ImGuiCol_Text, palette::toVec4(palette::kGraphite950));
        }
        if (ImGui::Button(brush.active ? "Arrêter de peindre (Échap)" : "Peindre dans la vue", {-FLT_MIN, 0})) {
            brush.active = !brush.active;
            s.focusViewport = brush.active;
        }
        if (wasActive) ImGui::PopStyleColor(2);
        ImGui::EndDisabled();
        struct ToolDef {
            TilemapBrush::Tool tool;
            const char* label;
            const char* tip;
        };
        static const ToolDef tools[] = {{TilemapBrush::Tool::Paint, "Pinceau", "Peindre (glisser pour tracer) - B"},
                                        {TilemapBrush::Tool::Erase, "Gomme", "Effacer - E"},
                                        {TilemapBrush::Tool::Rect, "Rectangle", "Remplir un rectangle (glisser) - U"},
                                        {TilemapBrush::Tool::Fill, "Remplissage", "Remplir la zone contiguë - G"},
                                        {TilemapBrush::Tool::Pick, "Pipette", "Prendre la tuile cliquée - I"}};
        for (const ToolDef& t : tools) {
            const bool sel = brush.tool == t.tool;
            if (sel) ImGui::PushStyleColor(ImGuiCol_Button, palette::toVec4(palette::kSelect));
            if (ImGui::SmallButton(t.label)) {
                brush.tool = t.tool;
                if (editMode) brush.active = true;
            }
            if (sel) ImGui::PopStyleColor();
            if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort)) ImGui::SetTooltip("%s", t.tip);
            ImGui::SameLine();
        }
        ImGui::NewLine();
        // Palette: every frame of the tileset; the selected one is outlined in amber.
        const float cell = 34.0f;
        const int columns = std::max(1, static_cast<int>(ImGui::GetContentRegionAvail().x / (cell + ImGui::GetStyle().ItemSpacing.x)));
        ImGui::BeginChild("##palette", {0, std::min(220.0f, (cell + 6.0f) * std::ceil(static_cast<float>(sheet->frames.size()) / static_cast<float>(columns)) + 8.0f)},
                          ImGuiChildFlags_Borders);
        for (u32 i = 0; i < sheet->frames.size(); ++i) {
            ImGui::PushID(static_cast<int>(i));
            if (i % static_cast<u32>(columns)) ImGui::SameLine();
            const math::Vec4 uv = sheet->uv(i);
            const bool sel = brush.tile == static_cast<i32>(i);
            const ImVec2 p = ImGui::GetCursorScreenPos();
            if (ImGui::ImageButton("##t", ImTextureRef(s.thumbnail(sheet->texture)), {cell, cell}, {uv.x, uv.y}, {uv.z, uv.w})) {
                brush.tile = static_cast<i32>(i);
                if (brush.tool == TilemapBrush::Tool::Erase || brush.tool == TilemapBrush::Tool::Pick) brush.tool = TilemapBrush::Tool::Paint;
                if (editMode) brush.active = true;
            }
            if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort)) ImGui::SetTooltip("%s (%u)", sheet->frames[i].name.c_str(), i);
            if (sel) {
                const ImVec2 q = ImGui::GetItemRectMax();
                ImGui::GetWindowDrawList()->AddRect(p, q, palette::kAmber, 3.0f, 2.0f);
            }
            ImGui::PopID();
        }
        ImGui::EndChild();
        if (ImGui::Button("Tout effacer") && painted > 0) {
            std::fill(tm.tiles.begin(), tm.tiles.end(), -1);
            edit.changed = edit.committed = true;
        }
        if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort)) ImGui::SetTooltip("Annulable avec Ctrl+Z");
        ImGui::PopID();
    } else if (info.name == "ParticleEmitter2D") {
        const auto* state = reg.tryGet<ParticleState2DComponent>(e);
        ImGui::TextDisabled("%zu particules actives", state ? state->particles.size() : usize{0});
        ImGui::SameLine();
        if (ImGui::SmallButton("Redémarrer")) restartParticles2D(scene, e);
    }
    return edit;
}

bool beginPropertyTable(const char* id) {
    if (!ImGui::BeginTable(id, 2, ImGuiTableFlags_SizingStretchProp)) return false;
    ImGui::TableSetupColumn("label", ImGuiTableColumnFlags_WidthFixed, ImGui::GetFontSize() * 7.5f);
    ImGui::TableSetupColumn("value", ImGuiTableColumnFlags_WidthStretch);
    return true;
}

bool containsCaseless(std::string text, std::string needle) {
    auto lower = [](std::string& v) {
        std::transform(v.begin(), v.end(), v.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    };
    lower(text);
    lower(needle);
    return text.find(needle) != std::string::npos;
}

} // namespace

void InspectorPanel::draw(EditorShared& s) {
    if (!ImGui::Begin("Inspecteur")) {
        ImGui::End();
        return;
    }
    Scene& scene = s.ctx.scene();
    const auto selected = s.ctx.selection().entities(scene);
    if (selected.empty()) {
        ImGui::TextDisabled("Sélectionnez une entité dans la hiérarchie ou la vue.");
        ImGui::End();
        return;
    }
    const ecs::Entity e = scene.findByUuid(s.ctx.selection().primary());
    if (e.isNull()) {
        ImGui::End();
        return;
    }
    auto& reg = scene.registry();
    const Uuid id = scene.uuidOf(e);
    Edit total;

    if (selected.size() > 1) {
        ImGui::PushStyleColor(ImGuiCol_Text, palette::toVec4(palette::kAmber));
        ImGui::TextWrapped("%zu entités sélectionnées : l'inspecteur montre la dernière, le gizmo déplace toute la sélection.",
                           selected.size());
        ImGui::PopStyleColor();
    }

    // ---- header: active, name, layer, id
    auto& state = reg.get<StateComponent>(e);
    if (ImGui::Checkbox("##active", &state.active)) total.changed = total.committed = true;
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Active (les entités inactives et leurs enfants sont ignorées)");
    ImGui::SameLine();
    ImGui::PushFont(s.fonts.semibold, 0.0f);
    ImGui::SetNextItemWidth(-FLT_MIN);
    ImGui::InputText("##name", &reg.get<NameComponent>(e).name);
    total.track();
    ImGui::PopFont();
    ImGui::TextDisabled("ID %s", id.toString().c_str());
    ImGui::SameLine();
    ImGui::TextDisabled("  Calque");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(70);
    int layer = static_cast<int>(state.layer);
    if (ImGui::DragInt("##layer", &layer, 0.2f, 0, 31)) state.layer = static_cast<u32>(std::clamp(layer, 0, 31));
    total.track();
    ImGui::Spacing();

    const auto& catalogue = ComponentRegistry::instance();
    // ---- Transform first, then the other components in catalogue order
    auto drawComponent = [&](const ComponentInfo& info, void* comp, bool removable) {
        ImGui::PushID(info.name.c_str());
        ImGui::PushFont(s.fonts.semibold, 0.0f);
        const bool open = ImGui::CollapsingHeader(info.name.c_str(), ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_AllowOverlap);
        ImGui::PopFont();
        if (!info.description.empty() && ImGui::IsItemHovered(ImGuiHoveredFlags_DelayNormal)) ImGui::SetTooltip("%s", info.description.c_str());
        bool remove = false;
        if (removable) {
            if (ImGui::BeginPopupContextItem("##compCtx")) {
                if (ImGui::MenuItem("Retirer le composant")) remove = true;
                ImGui::EndPopup();
            }
            ImGui::SameLine(ImGui::GetContentRegionAvail().x - ImGui::GetFrameHeight() + ImGui::GetCursorPosX() - 4.0f);
            if (ImGui::SmallButton("...")) ImGui::OpenPopup("##compCtx");
        }
        if (open && !remove && beginPropertyTable("##props")) {
            for (const FieldInfo& f : info.fields) total.merge(drawField(s, f, comp, m_eulerCache, id.value() ^ info.id));
            ImGui::EndTable();
        }
        if (open && !remove) total.merge(drawComponentTools(s, info, comp, e));
        ImGui::PopID();
        if (remove) s.ctx.removeComponent(e, info.name);
        return remove;
    };

    if (const ComponentInfo* t = catalogue.find<TransformComponent>())
        drawComponent(*t, &reg.get<TransformComponent>(e), false);
    for (const ComponentInfo* info : catalogue.all()) {
        if (info->name == "Name" || info->name == "State" || info->name == "Transform") continue;
        ecs::PoolBase* pool = reg.findPool(info->id);
        if (!pool || !pool->contains(e)) continue;
        if (drawComponent(*info, pool->getRaw(e), true)) break;  // pools changed: redraw next frame
    }

    // ---- Add Component
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();
    const float w = std::min(260.0f, ImGui::GetContentRegionAvail().x);
    ImGui::SetCursorPosX(ImGui::GetCursorPosX() + (ImGui::GetContentRegionAvail().x - w) * 0.5f);
    if (ImGui::Button("Ajouter un composant", {w, 0})) {
        m_componentFilter.clear();
        ImGui::OpenPopup("##addComponent");
    }
    if (ImGui::BeginPopup("##addComponent")) {
        ImGui::SetNextItemWidth(220);
        if (ImGui::IsWindowAppearing()) ImGui::SetKeyboardFocusHere();
        ImGui::InputTextWithHint("##filter", "Rechercher...", &m_componentFilter);
        std::map<std::string, std::vector<const ComponentInfo*>> byCategory;
        for (const ComponentInfo* info : catalogue.all()) {
            if (!info->userAddable) continue;
            ecs::PoolBase* pool = reg.findPool(info->id);
            if (pool && pool->contains(e)) continue;
            if (!m_componentFilter.empty() && !containsCaseless(info->name, m_componentFilter)) continue;
            byCategory[info->category].push_back(info);
        }
        if (byCategory.empty()) ImGui::TextDisabled("Aucun composant disponible");
        for (const auto& [category, list] : byCategory) {
            ImGui::SeparatorText(category.c_str());
            for (const ComponentInfo* info : list) {
                if (ImGui::Selectable(info->name.c_str())) {
                    s.ctx.addComponent(e, info->name);
                    ImGui::CloseCurrentPopup();
                }
                if (!info->description.empty() && ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort))
                    ImGui::SetTooltip("%s", info->description.c_str());
            }
        }
        ImGui::EndPopup();
    }

    if (total.changed) scene.updateTransforms();
    if (total.committed) s.ctx.commit("Edit " + scene.nameOf(e));
    ImGui::End();
}

} // namespace agr::editor
