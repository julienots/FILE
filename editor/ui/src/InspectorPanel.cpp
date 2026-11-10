#include <algorithm>
#include <cctype>
#include <cmath>
#include <filesystem>
#include <functional>
#include <map>

#include <SDL3/SDL.h>
#include <imgui.h>
#include <imgui_stdlib.h>

#include "agr/editor/ui/ImGuiRenderer.hpp"
#include "agr/editor/ui/Panels.hpp"
#include "agr/render/ModelLibrary.hpp"
#include "agr/script/ScriptComponent.hpp"
#include "agr/animation/AnimationSystem.hpp"
#include "agr/audio/AudioComponents.hpp"
#include "agr/ui/UIComponents.hpp"
#include "agr/script/ScriptPlugin.hpp"
#include <nlohmann/json.hpp>
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

bool beginPropertyTable(const char* id);

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
    } else if (info.name == "MeshRenderer") {
        auto& mr = *static_cast<MeshRendererComponent*>(comp);
        ImGui::PushID("meshTools");
        if (!mr.model.empty()) {
            const ModelAsset* model = s.models() ? s.models()->get(mr.model) : nullptr;
            if (!model || model->state == AssetState::Loading) {
                ImGui::TextDisabled("Modèle en cours de chargement...");
            } else if (model->state == AssetState::Failed) {
                ImGui::PushStyleColor(ImGuiCol_Text, palette::toVec4(palette::kError));
                ImGui::TextWrapped("Modèle non chargé : %s", model->error.c_str());
                ImGui::PopStyleColor();
            } else {
                const bool valid = mr.meshIndex >= 0 && static_cast<usize>(mr.meshIndex) < model->meshes.size();
                if (valid) {
                    const ModelAsset::Mesh& mesh = model->meshes[static_cast<usize>(mr.meshIndex)];
                    ImGui::TextDisabled("Maillage « %s » : %u sommets, %u triangles, %zu matériau(x)", mesh.name.c_str(), mesh.vertexCount,
                                        mesh.indexCount / 3, mesh.submeshes.size());
                } else {
                    ImGui::TextColored(palette::toVec4(palette::kWarning), "meshIndex hors limites (le modèle a %zu maillage(s))",
                                       model->meshes.size());
                }
                if (!model->warnings.empty()) {
                    ImGui::TextColored(palette::toVec4(palette::kWarning), "%zu avertissement(s) d'import", model->warnings.size());
                    if (ImGui::IsItemHovered()) {
                        ImGui::BeginTooltip();
                        for (const std::string& w : model->warnings) ImGui::BulletText("%s", w.c_str());
                        ImGui::EndTooltip();
                    }
                }
            }
        }
        if (!mr.material.empty() && ImGui::SmallButton("Modifier le matériau")) {
            s.inspectedAsset = mr.material;
            s.ctx.selection().clear();
        }
        ImGui::PopID();
    } else if (info.name == "Script") {
        auto& sc = *static_cast<ScriptComponent*>(comp);
        auto* plugin = s.engine.service<ScriptPlugin>();
        if (sc.script.empty() || !plugin) {
            ImGui::TextDisabled("Glissez un fichier .lua dans « Script ».");
            return edit;
        }
        ImGui::PushID("scriptTools");
        const ScriptInfo si = plugin->scripts()->describe(sc.script);
        if (!si.valid) {
            ImGui::PushStyleColor(ImGuiCol_Text, palette::toVec4(palette::kError));
            ImGui::TextWrapped("%s", si.error.empty() ? "Script introuvable" : si.error.c_str());
            ImGui::PopStyleColor();
        } else {
            nlohmann::json values = nlohmann::json::parse(sc.properties.empty() ? "{}" : sc.properties, nullptr, false);
            if (!values.is_object()) values = nlohmann::json::object();
            bool changed = false, commit = false;
            if (!si.properties.empty() && beginPropertyTable("##scriptProps")) {
                for (const ScriptProperty& p : si.properties) {
                    ImGui::TableNextRow();
                    ImGui::TableNextColumn();
                    label(p.name.c_str(), "Propriété déclarée par le script (properties = { ... })");
                    ImGui::PushID(p.name.c_str());
                    const auto it = values.find(p.name);
                    switch (p.type) {
                        case ScriptProperty::Type::Number: {
                            float v = static_cast<float>(it != values.end() && it->is_number() ? it->get<double>() : p.number);
                            if (ImGui::DragFloat("##v", &v, 0.05f)) {
                                values[p.name] = v;
                                changed = true;
                            }
                            commit |= ImGui::IsItemDeactivatedAfterEdit();
                            break;
                        }
                        case ScriptProperty::Type::Boolean: {
                            bool v = it != values.end() && it->is_boolean() ? it->get<bool>() : p.boolean;
                            if (ImGui::Checkbox("##v", &v)) {
                                values[p.name] = v;
                                changed = commit = true;
                            }
                            break;
                        }
                        case ScriptProperty::Type::String: {
                            std::string v = it != values.end() && it->is_string() ? it->get<std::string>() : p.string;
                            if (ImGui::InputText("##v", &v)) {
                                values[p.name] = v;
                                changed = true;
                            }
                            commit |= ImGui::IsItemDeactivatedAfterEdit();
                            break;
                        }
                    }
                    ImGui::PopID();
                }
                ImGui::EndTable();
            }
            if (changed) sc.properties = values.dump();
            edit.changed |= changed;
            edit.committed |= commit;
            std::string callbacks;
            for (const std::string& c : si.callbacks) callbacks += (callbacks.empty() ? "" : ", ") + c;
            ImGui::TextDisabled("Fonctions : %s", callbacks.empty() ? "(aucune)" : callbacks.c_str());
        }
        if (ImGui::SmallButton("Ouvrir le fichier") && s.ctx.hasProject()) {
            const std::string rel = sc.script.rfind("assets://", 0) == 0 ? sc.script.substr(9) : sc.script;
            const std::string url = "file://" + std::filesystem::absolute(s.ctx.project().assetsDirectory() / rel).generic_string();
            if (!SDL_OpenURL(url.c_str())) s.notify(std::string("Impossible d'ouvrir : ") + SDL_GetError());
        }
        if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort))
            ImGui::SetTooltip("Ouvre le script dans l'éditeur de texte du système ; il est rechargé à chaud quand vous l'enregistrez");
        ImGui::PopID();
    } else if (info.name == "AudioSource") {
        auto& as = *static_cast<AudioSourceComponent*>(comp);
        ImGui::PushID("audioTools");
        if (!s.audio || !s.audio->audio().initialized()) {
            ImGui::TextDisabled("Audio indisponible.");
        } else if (as.clip.empty()) {
            ImGui::TextDisabled("Glissez un fichier audio (.wav, .ogg, .mp3, .flac) dans « Clip ».");
        } else {
            AudioSystem& audio = s.audio->audio();
            static VoiceId preview = 0;  // one preview at a time, on the Interface bus
            auto duration = audio.clipDuration(as.clip);
            if (!duration) {
                ImGui::PushStyleColor(ImGuiCol_Text, palette::toVec4(palette::kError));
                ImGui::TextWrapped("%s", duration.error().message.c_str());
                ImGui::PopStyleColor();
            } else {
                const bool playing = preview != 0 && audio.isPlaying(preview);
                if (ImGui::SmallButton(playing ? "Arrêter" : "Écouter")) {
                    if (playing) {
                        audio.stop(preview);
                        preview = 0;
                    } else {
                        PlayParams p;
                        p.bus = AudioBus::Interface;
                        p.volume = as.volume;
                        p.pitch = as.pitch;
                        p.stream = as.stream;
                        auto v = audio.play(as.clip, p);
                        preview = v ? v.value() : 0;
                        if (!v) s.notify(v.error().message);
                    }
                }
                ImGui::SameLine();
                ImGui::TextDisabled("%.2f s%s", duration.value(), as.loop ? ", en boucle" : "");
            }
        }
        ImGui::PopID();
    } else if (info.name == "UIRect") {
        auto& r = *static_cast<UIRectComponent*>(comp);
        ImGui::PushID("uiRectTools");
        ImGui::TextDisabled("Ancres (colonne : gauche, centre, droite, étirer ; ligne : haut, milieu, bas, étirer)");
        static const float stops[4] = {0.0f, 0.5f, 1.0f, -1.0f};  // -1 = stretch
        const float cell = ImGui::GetFrameHeight();
        auto isPreset = [&](float sx, float sy) {
            auto axis = [](float stop, float mn, float mx) { return stop < 0 ? (mn == 0.0f && mx == 1.0f) : (mn == stop && mx == stop); };
            return axis(sx, r.anchorMin.x, r.anchorMax.x) && axis(sy, r.anchorMin.y, r.anchorMax.y);
        };
        float scale = 1.0f;
        if (s.ui) scale = std::max(1e-3f, s.ui->ui().canvasScale(s.ui->ui().canvasOf(e)));
        for (int row = 0; row < 4; ++row) {
            for (int col = 0; col < 4; ++col) {
                if (col) ImGui::SameLine(0, 2);
                ImGui::PushID(row * 4 + col);
                const bool active = isPreset(stops[col], stops[row]);
                if (active) ImGui::PushStyleColor(ImGuiCol_Button, palette::toVec4(palette::kAmber));
                const char* glyph = stops[col] < 0 && stops[row] < 0 ? "+" : stops[col] < 0 ? "-" : stops[row] < 0 ? "|" : ".";
                if (ImGui::Button(glyph, {cell, cell})) {
                    // Keeps the element's on-screen size where possible.
                    const math::Vec2 pixelSize{r.screenRect.z / scale, r.screenRect.w / scale};
                    for (int axis = 0; axis < 2; ++axis) {
                        const float stop = stops[axis == 0 ? col : row];
                        float& mn = axis == 0 ? r.anchorMin.x : r.anchorMin.y;
                        float& mx = axis == 0 ? r.anchorMax.x : r.anchorMax.y;
                        float& pv = axis == 0 ? r.pivot.x : r.pivot.y;
                        float& pos = axis == 0 ? r.position.x : r.position.y;
                        float& sz = axis == 0 ? r.size.x : r.size.y;
                        const bool wasStretch = mn != mx;
                        if (stop < 0) {
                            mn = 0.0f, mx = 1.0f, pv = 0.5f, pos = 0.0f, sz = 0.0f;
                        } else {
                            if (wasStretch) sz = axis == 0 ? pixelSize.x : pixelSize.y;
                            mn = mx = pv = stop;
                            pos = 0.0f;
                        }
                    }
                    edit.changed = edit.committed = true;
                }
                if (active) ImGui::PopStyleColor();
                ImGui::PopID();
            }
        }
        if (r.screenRect.z > 0.0f)
            ImGui::TextDisabled("À l'écran : %.0f x %.0f px (vue Jeu)", r.screenRect.z, r.screenRect.w);
        else
            ImGui::TextDisabled("Placez cet élément sous un Canvas ; il s'affiche dans la vue Jeu.");
        ImGui::PopID();
    } else if (info.name == "Animator") {
        auto& an = *static_cast<AnimatorComponent*>(comp);
        ImGui::PushID("animatorTools");
        if (s.animation && !an.controller.empty()) {
            auto ctrl = s.animation->animation().library().controller(an.controller);
            if (!ctrl) {
                ImGui::PushStyleColor(ImGuiCol_Text, palette::toVec4(palette::kError));
                ImGui::TextWrapped("%s", ctrl.error().message.c_str());
                ImGui::PopStyleColor();
            } else {
                for (const std::string& problem : ctrl.value()->validate())
                    ImGui::TextColored(palette::toVec4(palette::kWarning), "%s", problem.c_str());
                if (s.ctx.playState() != PlayState::Edit) {
                    ImGui::Text("État : %s (%.1f s)", s.animation->animation().currentState(e).c_str(), s.animation->animation().stateTime(e));
                    for (const AnimatorParameter& p : ctrl.value()->parameters) {
                        const auto v = s.animation->animation().parameter(scene, e, p.name);
                        ImGui::BulletText("%s = %.2f", p.name.c_str(), v.value_or(0.0f));
                    }
                }
            }
        }
        // Clips of an imported model below this entity ("<model>#<clip>").
        std::string modelUri;
        std::function<void(ecs::Entity)> findModel = [&](ecs::Entity x) {
            if (!modelUri.empty()) return;
            if (const auto* mr = reg.tryGet<MeshRendererComponent>(x); mr && !mr->model.empty()) modelUri = mr->model;
            scene.forEachChild(x, [&](ecs::Entity c) { findModel(c); });
        };
        findModel(e);
        if (!modelUri.empty() && s.models()) {
            const ModelAsset* model = s.models()->get(modelUri);
            if (model && !model->animationNames.empty()) {
                const std::string current = an.clip.rfind(modelUri + "#", 0) == 0 ? an.clip.substr(modelUri.size() + 1) : std::string("(autre)");
                ImGui::SetNextItemWidth(-FLT_MIN);
                if (ImGui::BeginCombo("##modelClips", ("Clip du modèle : " + current).c_str())) {
                    for (const std::string& name : model->animationNames)
                        if (ImGui::Selectable(name.c_str(), name == current)) {
                            an.clip = modelUri + "#" + name;
                            edit.changed = edit.committed = true;
                        }
                    ImGui::EndCombo();
                }
                if (ImGui::IsItemHovered())
                    ImGui::SetTooltip("Animations importées avec le modèle ; utilisables aussi dans une machine à états (%s#Nom)", modelUri.c_str());
            }
        }
        if (ImGui::SmallButton("Timeline")) s.showAnimation = true;
        ImGui::SameLine();
        if (ImGui::SmallButton("Machine à états")) s.showStateMachine = true;
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

namespace {

/// Texture slot of a material: thumbnail, URI, drop target, clear button. Returns true when changed.
bool textureSlot(EditorShared& s, const char* label, std::string& uri, const char* tooltip) {
    bool changed = false;
    ImGui::TableNextRow();
    ImGui::TableNextColumn();
    ImGui::PushID(label);
    ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted(label);
    if (tooltip && ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort)) ImGui::SetTooltip("%s", tooltip);
    ImGui::TableNextColumn();
    const float thumb = ImGui::GetFrameHeight();
    if (!uri.empty()) {
        ImGui::Image(ImTextureRef(s.thumbnail(uri)), {thumb, thumb});
        ImGui::SameLine();
    }
    ImGui::SetNextItemWidth(-thumb - ImGui::GetStyle().ItemSpacing.x);
    ImGui::InputTextWithHint("##uri", "Glisser une image ici", &uri);
    changed |= ImGui::IsItemDeactivatedAfterEdit();
    if (ImGui::BeginDragDropTarget()) {
        if (const ImGuiPayload* p = ImGui::AcceptDragDropPayload(kPayloadAsset)) {
            uri = static_cast<const char*>(p->Data);
            changed = true;
        }
        ImGui::EndDragDropTarget();
    }
    ImGui::SameLine();
    if (ImGui::Button("x", {thumb, 0}) && !uri.empty()) {
        uri.clear();
        changed = true;
    }
    ImGui::PopID();
    return changed;
}

} // namespace

void InspectorPanel::drawMaterialAsset(EditorShared& s, const MaterialPreview& preview) {
    const std::string uri = s.inspectedAsset;
    MaterialLibrary* lib = s.materials();
    if (!lib) {
        ImGui::TextDisabled("Pas de rendu : matériaux indisponibles.");
        return;
    }
    if (m_materialUri != uri) {
        const MaterialData* m = lib->get(uri);
        if (!m) {
            ImGui::TextColored(palette::toVec4(palette::kWarning), "Matériau illisible : %s (voir la console)", uri.c_str());
            return;
        }
        m_material = *m;
        m_materialUri = uri;
    }
    s.previewMaterial = uri;
    ImGui::PushFont(s.fonts.semibold, 0.0f);
    ImGui::TextUnformatted(uri.substr(uri.find_last_of('/') + 1).c_str());
    ImGui::PopFont();
    ImGui::TextDisabled("Matériau PBR (métal / rugosité)");
    // Preview sphere (rendered by MaterialPreview before the UI pass).
    if (const rhi::TextureHandle tex = preview.texture(s)) {
        const float w = std::min(ImGui::GetContentRegionAvail().x, 220.0f);
        const bool flip = s.device() && s.device()->info().renderTargetsFlipY;
        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + (ImGui::GetContentRegionAvail().x - w) * 0.5f);
        ImGui::Image(ImTextureRef(ImGuiRenderer::toImTexture(tex)), {w, w}, {0, flip ? 1.0f : 0.0f}, {1, flip ? 0.0f : 1.0f});
    }
    MaterialData& m = m_material;
    bool changed = false, commit = false;
    auto track = [&] {
        changed |= ImGui::IsItemEdited();
        commit |= ImGui::IsItemDeactivatedAfterEdit();
    };
    auto row = [](const char* label, const char* tip = nullptr) {
        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        ImGui::AlignTextToFramePadding();
        ImGui::TextUnformatted(label);
        if (tip && ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort)) ImGui::SetTooltip("%s", tip);
        ImGui::TableNextColumn();
        ImGui::SetNextItemWidth(-FLT_MIN);
    };
    if (beginPropertyTable("##material")) {
        row("Nom");
        ImGui::InputText("##name", &m.name);
        track();
        row("Couleur de base");
        ImGui::ColorEdit4("##base", &m.baseColor.x, ImGuiColorEditFlags_AlphaPreviewHalf);
        track();
        commit |= textureSlot(s, "Texture de base", m.baseColorTexture, "Couleur (sRGB), alpha = opacité");
        row("Métallique");
        ImGui::SliderFloat("##metal", &m.metallic, 0.0f, 1.0f, "%.2f");
        track();
        row("Rugosité");
        ImGui::SliderFloat("##rough", &m.roughness, 0.0f, 1.0f, "%.2f");
        track();
        commit |= textureSlot(s, "Métal / rugosité", m.metallicRoughnessTexture, "Texture linéaire : vert = rugosité, bleu = métal (glTF)");
        commit |= textureSlot(s, "Normal map", m.normalTexture, "Espace tangent, vert vers le haut (convention OpenGL / glTF)");
        row("Intensité normale");
        ImGui::SliderFloat("##nscale", &m.normalScale, 0.0f, 2.0f, "%.2f");
        track();
        commit |= textureSlot(s, "Occlusion", m.occlusionTexture, "Texture linéaire : rouge = occlusion ambiante");
        row("Force occlusion");
        ImGui::SliderFloat("##ao", &m.occlusionStrength, 0.0f, 1.0f, "%.2f");
        track();
        row("Émission");
        ImGui::ColorEdit3("##emissive", &m.emissive.x);
        track();
        row("Intensité émission");
        ImGui::DragFloat("##eint", &m.emissiveIntensity, 0.05f, 0.0f, 100.0f, "%.2f");
        track();
        commit |= textureSlot(s, "Texture émission", m.emissiveTexture, nullptr);
        row("Transparence");
        static const char* modes[] = {"Opaque", "Découpe (alpha test)", "Fondu (transparent)"};
        int mode = static_cast<int>(m.alphaMode);
        if (ImGui::Combo("##alpha", &mode, modes, 3)) {
            m.alphaMode = static_cast<AlphaMode>(mode);
            commit = true;
        }
        if (m.alphaMode == AlphaMode::Mask) {
            row("Seuil alpha");
            ImGui::SliderFloat("##cutoff", &m.alphaCutoff, 0.0f, 1.0f, "%.2f");
            track();
        }
        row("Double face", "Affiche aussi l'arrière des faces (feuillages, tissus)");
        commit |= ImGui::Checkbox("##double", &m.doubleSided);
        row("Sans éclairage", "Affiche la couleur telle quelle (interfaces 3D, écrans)");
        commit |= ImGui::Checkbox("##unlit", &m.unlit);
        row("Répétition UV");
        dragAxes("uvs", &m.uvScale.x, 2, 0.01f, 0.0f, 0.0f, "%.2f");
        track();
        row("Décalage UV");
        dragAxes("uvo", &m.uvOffset.x, 2, 0.01f, 0.0f, 0.0f, "%.2f");
        track();
        ImGui::EndTable();
    }
    if (changed) lib->preview(uri, m);  // live preview while dragging
    if (commit) {
        if (Status st = lib->save(uri, m); !st) s.notify("Enregistrement impossible : " + st.error().message);
    }
    ImGui::Spacing();
    ImGui::TextDisabled("Enregistré automatiquement dans le fichier.");
    ImGui::TextDisabled("Non annulable avec Ctrl+Z (Ctrl+Z annule les modifications de scène).");
    ImGui::TextDisabled("Glissez le matériau sur un objet de la scène pour l'appliquer.");
}

void InspectorPanel::draw(EditorShared& s, const MaterialPreview& preview) {
    if (!ImGui::Begin("Inspecteur")) {
        ImGui::End();
        return;
    }
    Scene& scene = s.ctx.scene();
    const auto selected = s.ctx.selection().entities(scene);
    if (!selected.empty()) {
        s.inspectedAsset.clear();
        m_materialUri.clear();
    }
    if (selected.empty()) {
        if (!s.inspectedAsset.empty() && s.inspectedAsset.ends_with(kMaterialExtension)) drawMaterialAsset(s, preview);
        else ImGui::TextDisabled("Sélectionnez une entité dans la hiérarchie ou la vue.");
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
