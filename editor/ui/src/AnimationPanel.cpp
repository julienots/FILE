#include <algorithm>
#include <cmath>
#include <filesystem>

#include <imgui.h>
#include <misc/cpp/imgui_stdlib.h>

#include "agr/editor/ui/Panels.hpp"
#include "agr/scene/Reflection.hpp"

namespace agr::editor {

namespace {

/// Channels a field can be animated on (-1 = the whole scalar / string).
std::vector<i32> channelsOf(const FieldInfo& f) {
    switch (f.type) {
        case FieldType::Float:
        case FieldType::Int32:
        case FieldType::UInt32:
        case FieldType::Enum:
        case FieldType::Bool:
        case FieldType::String: return {-1};
        case FieldType::Vec2: return {0, 1};
        case FieldType::Vec3:
        case FieldType::Quat: return {0, 1, 2};
        case FieldType::Vec4: return {0, 1, 2, 3};
        default: return {};
    }
}

bool animatableField(const FieldInfo& f) {
    return !(f.flags & (FieldFlag_Hidden | FieldFlag_Transient | FieldFlag_ReadOnly)) && !channelsOf(f).empty();
}

/// The selected entity or its nearest ancestor with an Animator.
ecs::Entity animatedRoot(Scene& scene, ecs::Entity e) {
    for (; !e.isNull(); e = scene.parent(e))
        if (scene.registry().has<AnimatorComponent>(e)) return e;
    return {};
}

void collectDescendants(Scene& scene, ecs::Entity e, std::vector<ecs::Entity>& out) {
    out.push_back(e);
    scene.forEachChild(e, [&](ecs::Entity c) { collectDescendants(scene, c, out); });
}

std::string relativeAssetPath(const std::string& uri) { return uri.rfind("assets://", 0) == 0 ? uri.substr(9) : uri; }

} // namespace

void AnimationPanel::loadClip(EditorShared& s, const std::string& uri) {
    endPreview(s);
    m_uri = uri;
    m_track = m_key = m_event = -1;
    m_time = 0.0f;
    m_playing = false;
    m_dirty = false;
    m_clip = {};
    auto text = s.engine.vfs().readText(uri);
    if (!text) {
        s.notify("Clip introuvable : " + uri);
        return;
    }
    auto clip = AnimationClip::fromJson(text.value());
    if (!clip) {
        s.notify(clip.error().message);
        return;
    }
    m_clip = std::move(clip.value());
}

bool AnimationPanel::saveClip(EditorShared& s) {
    if (m_uri.empty()) return false;
    if (Status st = s.engine.vfs().writeText(m_uri, m_clip.toJson()); !st) {
        s.notify("Enregistrement impossible : " + st.error().message);
        return false;
    }
    m_dirty = false;
    if (s.animation) s.animation->animation().library().reloadChanged();
    s.notify("Clip enregistré : " + m_uri);
    return true;
}

void AnimationPanel::endPreview(EditorShared& s) {
    if (!m_previewing) return;
    m_previewing = false;
    m_playing = false;
    Scene& scene = s.ctx.scene();
    const ecs::Entity root = scene.findByUuid(m_entity);
    if (root.isNull() || !s.animation || m_restore.empty()) {
        m_restore.clear();
        return;
    }
    AnimationClip back;
    for (auto& [track, value] : m_restore) {
        AnimationTrack t = track;
        t.curve.keys.clear();
        Keyframe k;
        k.value = value;
        k.text = track.curve.keys.empty() ? std::string() : track.curve.keys.front().text;
        t.curve.keys.push_back(k);
        back.tracks.push_back(std::move(t));
    }
    s.animation->animation().preview(scene, root, back, 0.0f);
    scene.updateTransforms();
    m_restore.clear();
}

void AnimationPanel::applyPreview(EditorShared& s) {
    if (!s.animation || s.ctx.playState() != PlayState::Edit) return;
    Scene& scene = s.ctx.scene();
    const ecs::Entity root = scene.findByUuid(m_entity);
    if (root.isNull()) return;
    AnimationSystem& anim = s.animation->animation();
    // Remember the original value of every track before it is first previewed.
    for (const AnimationTrack& t : m_clip.tracks) {
        const bool known = std::any_of(m_restore.begin(), m_restore.end(), [&](const auto& r) {
            return r.first.target == t.target && r.first.component == t.component && r.first.field == t.field && r.first.channel == t.channel;
        });
        if (known) continue;
        AnimationTrack saved = t;
        saved.curve.keys.clear();
        float value = 0.0f;
        if (AnimationSystem::isTextTrack(t)) {
            Keyframe k;
            k.text = anim.readTrackText(scene, root, t).value_or("");
            saved.curve.keys.push_back(k);
        } else if (auto v = anim.readTrack(scene, root, t)) {
            value = *v;
        } else {
            continue;
        }
        m_restore.emplace_back(std::move(saved), value);
    }
    m_previewing = true;
    anim.preview(scene, root, m_clip, m_time);
    scene.updateTransforms();
}

void AnimationPanel::keyTracks(EditorShared& s, bool selectedOnly) {
    if (!s.animation) return;
    Scene& scene = s.ctx.scene();
    const ecs::Entity root = scene.findByUuid(m_entity);
    AnimationSystem& anim = s.animation->animation();
    for (int i = 0; i < static_cast<int>(m_clip.tracks.size()); ++i) {
        if (selectedOnly && i != m_track) continue;
        AnimationTrack& t = m_clip.tracks[static_cast<usize>(i)];
        if (AnimationSystem::isTextTrack(t)) {
            if (auto text = anim.readTrackText(scene, root, t)) t.curve.setKey(m_time, 0.0f, Interpolation::Constant).text = *text;
        } else if (auto v = anim.readTrack(scene, root, t)) {
            t.curve.setKey(m_time, *v);
        }
    }
    if (m_time > m_clip.duration) m_clip.duration = m_time;
    m_dirty = true;
}

void AnimationPanel::addTrackPopup(EditorShared& s) {
    if (!ImGui::BeginPopup("addTrack")) return;
    Scene& scene = s.ctx.scene();
    const ecs::Entity root = scene.findByUuid(m_entity);
    std::vector<ecs::Entity> entities;
    if (!root.isNull()) collectDescendants(scene, root, entities);
    m_addTarget = std::clamp(m_addTarget, 0, std::max(0, static_cast<int>(entities.size()) - 1));
    std::vector<std::string> paths;
    for (ecs::Entity e : entities) {
        const std::string p = AnimationSystem::pathOf(scene, root, e).value_or("?");
        paths.push_back(p.empty() ? scene.nameOf(e) + " (racine)" : p);
    }
    ImGui::SetNextItemWidth(260);
    if (ImGui::BeginCombo("Objet", paths.empty() ? "" : paths[static_cast<usize>(m_addTarget)].c_str())) {
        for (int i = 0; i < static_cast<int>(paths.size()); ++i)
            if (ImGui::Selectable(paths[static_cast<usize>(i)].c_str(), i == m_addTarget)) m_addTarget = i, m_addComponent = m_addField = 0;
        ImGui::EndCombo();
    }
    if (entities.empty()) {
        ImGui::EndPopup();
        return;
    }
    const ecs::Entity target = entities[static_cast<usize>(m_addTarget)];
    std::vector<const ComponentInfo*> comps;
    for (const ComponentInfo* info : ComponentRegistry::instance().all()) {
        if (info->name == "Name") continue;
        ecs::PoolBase* pool = scene.registry().findPool(info->id);
        if (!pool || !pool->getRaw(target)) continue;
        if (std::any_of(info->fields.begin(), info->fields.end(), [](const FieldInfo& f) { return animatableField(f); })) comps.push_back(info);
    }
    m_addComponent = std::clamp(m_addComponent, 0, std::max(0, static_cast<int>(comps.size()) - 1));
    ImGui::SetNextItemWidth(260);
    if (ImGui::BeginCombo("Composant", comps.empty() ? "" : comps[static_cast<usize>(m_addComponent)]->name.c_str())) {
        for (int i = 0; i < static_cast<int>(comps.size()); ++i)
            if (ImGui::Selectable(comps[static_cast<usize>(i)]->name.c_str(), i == m_addComponent)) m_addComponent = i, m_addField = 0;
        ImGui::EndCombo();
    }
    if (comps.empty()) {
        ImGui::EndPopup();
        return;
    }
    const ComponentInfo* comp = comps[static_cast<usize>(m_addComponent)];
    std::vector<const FieldInfo*> fields;
    for (const FieldInfo& f : comp->fields)
        if (animatableField(f)) fields.push_back(&f);
    m_addField = std::clamp(m_addField, 0, std::max(0, static_cast<int>(fields.size()) - 1));
    ImGui::SetNextItemWidth(260);
    if (ImGui::BeginCombo("Champ", fields.empty() ? "" : fields[static_cast<usize>(m_addField)]->displayName.c_str())) {
        for (int i = 0; i < static_cast<int>(fields.size()); ++i)
            if (ImGui::Selectable(fields[static_cast<usize>(i)]->displayName.c_str(), i == m_addField)) m_addField = i;
        ImGui::EndCombo();
    }
    if (!fields.empty() && ImGui::Button("Ajouter la piste (clé à l'instant courant)")) {
        const FieldInfo* f = fields[static_cast<usize>(m_addField)];
        const std::string path = AnimationSystem::pathOf(scene, root, target).value_or("");
        const int before = static_cast<int>(m_clip.tracks.size());
        for (i32 ch : channelsOf(*f)) {
            if (m_clip.findTrack(path, comp->name, f->name, ch)) continue;
            m_clip.track(path, comp->name, f->name, ch);
        }
        // Key the new tracks with their current values.
        const int prevSelected = m_track;
        for (int i = before; i < static_cast<int>(m_clip.tracks.size()); ++i) {
            m_track = i;
            keyTracks(s, true);
        }
        m_track = static_cast<int>(m_clip.tracks.size()) > before ? before : prevSelected;
        m_key = 0;
        m_dirty = true;
        ImGui::CloseCurrentPopup();
    }
    ImGui::EndPopup();
}

void AnimationPanel::draw(EditorShared& s, const FrameTime& time) {
    if (!s.showAnimation) {
        endPreview(s);
        return;
    }
    if (!ImGui::Begin("Animation", &s.showAnimation)) {
        ImGui::End();
        return;
    }
    Scene& scene = s.ctx.scene();
    const bool editMode = s.ctx.playState() == PlayState::Edit;
    if (!editMode) endPreview(s);
    const ecs::Entity root = animatedRoot(scene, scene.findByUuid(s.ctx.selection().primary()));
    if (root.isNull()) {
        endPreview(s);
        ImGui::TextDisabled("Sélectionnez une entité avec un composant Animator (ou l'un de ses enfants).");
        const ecs::Entity sel = scene.findByUuid(s.ctx.selection().primary());
        if (!sel.isNull() && editMode && ImGui::Button("Ajouter un Animator à la sélection")) s.ctx.addComponent(sel, "Animator");
        ImGui::End();
        return;
    }
    if (scene.uuidOf(root) != m_entity) {
        endPreview(s);
        m_entity = scene.uuidOf(root);
        m_uri.clear();
    }
    auto& animator = scene.registry().get<AnimatorComponent>(root);

    // Clips of this animator: its own clip and the clips of its state machine's states.
    std::vector<std::string> clips;
    if (!animator.clip.empty()) clips.push_back(animator.clip);
    if (!animator.controller.empty() && s.animation)
        if (auto ctrl = s.animation->animation().library().controller(animator.controller))
            for (const AnimatorState& st : ctrl.value()->states)
                if (!st.clip.empty() && std::find(clips.begin(), clips.end(), st.clip) == clips.end()) clips.push_back(st.clip);
    if (m_uri.empty() && !clips.empty()) loadClip(s, clips.front());

    ImGui::TextUnformatted(scene.nameOf(root).c_str());
    ImGui::SameLine();
    ImGui::SetNextItemWidth(280);
    if (ImGui::BeginCombo("##clip", m_uri.empty() ? "(aucun clip)" : relativeAssetPath(m_uri).c_str())) {
        for (const std::string& c : clips)
            if (ImGui::Selectable(relativeAssetPath(c).c_str(), c == m_uri)) {
                if (m_dirty) saveClip(s);
                loadClip(s, c);
            }
        ImGui::EndCombo();
    }
    ImGui::SameLine();
    ImGui::SetNextItemWidth(120);
    ImGui::InputText("##newclip", &m_newClipName);
    ImGui::SameLine();
    if (ImGui::Button("Nouveau clip") && s.ctx.hasProject() && editMode) {
        std::string name = m_newClipName;
        for (char& c : name)
            if (!std::isalnum(static_cast<unsigned char>(c)) && c != '_' && c != '-') c = '_';
        const std::string uri = "assets://animations/" + (name.empty() ? std::string("clip") : name) + ".agranim";
        if (s.engine.vfs().exists(uri)) {
            s.notify("Ce clip existe déjà : " + uri);
        } else {
            AnimationClip fresh;
            std::filesystem::create_directories(s.ctx.project().assetsDirectory() / "animations");
            if (Status st = s.engine.vfs().writeText(uri, fresh.toJson()); !st) {
                s.notify(st.error().message);
            } else {
                if (animator.clip.empty() && animator.controller.empty()) {
                    animator.clip = uri;
                    s.ctx.commit("Assign Animation Clip");
                }
                loadClip(s, uri);
            }
        }
    }
    if (m_uri.empty()) {
        ImGui::TextDisabled("Créez un clip, ou indiquez-en un dans l'Animator (champ Clip) ou dans un état de sa machine à états.");
        ImGui::End();
        return;
    }

    // ---------------------------------------------------------------- toolbar
    if (ImGui::Button(m_dirty ? "Enregistrer*" : "Enregistrer")) saveClip(s);
    ImGui::SameLine();
    if (ImGui::Button("|<")) {
        m_time = 0.0f;
        if (m_previewing) applyPreview(s);
    }
    ImGui::SameLine();
    ImGui::BeginDisabled(!editMode);
    if (ImGui::Button(m_playing ? "Pause" : "Lire")) {
        m_playing = !m_playing;
        if (m_playing) applyPreview(s);
    }
    ImGui::SameLine();
    bool preview = m_previewing;
    if (ImGui::Checkbox("Aperçu", &preview)) {
        if (preview) applyPreview(s);
        else endPreview(s);
    }
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("Applique le clip à la scène à l'instant choisi.\nÀ la fin de l'aperçu, les valeurs animées reprennent leur état d'origine.");
    ImGui::EndDisabled();
    ImGui::SameLine();
    ImGui::SetNextItemWidth(80);
    if (ImGui::DragFloat("##time", &m_time, 0.01f, 0.0f, 600.0f, "%.2f s") && m_previewing) applyPreview(s);
    ImGui::SameLine();
    ImGui::SetNextItemWidth(90);
    if (ImGui::DragFloat("Durée", &m_clip.duration, 0.01f, 0.01f, 600.0f, "%.2f s")) m_dirty = true;
    ImGui::SameLine();
    if (ImGui::Checkbox("Boucle", &m_clip.loop)) m_dirty = true;
    ImGui::SameLine();
    if (ImGui::Button("+ Piste")) ImGui::OpenPopup("addTrack");
    addTrackPopup(s);
    ImGui::SameLine();
    ImGui::BeginDisabled(m_clip.tracks.empty());
    if (ImGui::Button("Clé")) keyTracks(s, false);
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
        ImGui::SetTooltip("Enregistre la valeur actuelle de chaque piste à l'instant courant.\nModifiez l'objet (inspecteur, gizmo), puis cliquez ici.");
    ImGui::EndDisabled();
    ImGui::SameLine();
    if (ImGui::Button("+ Événement")) {
        m_clip.events.push_back({m_time, "event"});
        std::sort(m_clip.events.begin(), m_clip.events.end(), [](const auto& a, const auto& b) { return a.time < b.time; });
        m_dirty = true;
    }

    if (m_playing && editMode) {
        m_time += static_cast<float>(time.unscaledDeltaSeconds);
        if (m_time > m_clip.duration) m_time = m_clip.loop ? std::fmod(m_time, std::max(0.01f, m_clip.duration)) : m_clip.duration;
        applyPreview(s);
    }

    // --------------------------------------------------------- tracks + keys
    const float rowH = ImGui::GetFrameHeight();
    const float leftW = 240.0f;
    const float detailsH = rowH * 6.5f;
    ImGui::BeginChild("tracks", {0, std::max(rowH * 4.0f, ImGui::GetContentRegionAvail().y - detailsH)}, ImGuiChildFlags_Borders);
    ImDrawList* dl = ImGui::GetWindowDrawList();
    const ImVec2 o = ImGui::GetCursorScreenPos();
    const float width = ImGui::GetContentRegionAvail().x;
    const float x0 = o.x + leftW;  // time 0
    auto xAt = [&](float t) { return x0 + 8.0f + t * m_zoom; };
    auto timeAt = [&](float x) { return std::max(0.0f, (x - x0 - 8.0f) / m_zoom); };
    const int rows = static_cast<int>(m_clip.tracks.size()) + 1;  // + events
    const float totalH = rowH * static_cast<float>(rows + 1);

    // Ruler.
    dl->AddRectFilled({x0, o.y}, {o.x + width, o.y + rowH}, palette::kGraphite800);
    const float step = m_zoom >= 400 ? 0.1f : m_zoom >= 120 ? 0.25f : m_zoom >= 50 ? 0.5f : 1.0f;
    for (float t = 0.0f; xAt(t) < o.x + width; t += step) {
        const float x = xAt(t);
        const bool major = std::fabs(std::fmod(t + 1e-4f, 1.0f)) < 2e-3f;
        dl->AddLine({x, o.y + (major ? 2.0f : rowH * 0.6f)}, {x, o.y + totalH}, major ? palette::kGraphite700 : IM_COL32(42, 47, 55, 90));
        if (major || step >= 0.25f) {
            char buf[16];
            std::snprintf(buf, sizeof buf, "%g", std::round(static_cast<double>(t) * 100.0) / 100.0);
            dl->AddText({x + 2, o.y + 2}, palette::kTextMuted, buf);
        }
    }
    // Clip end.
    dl->AddRectFilled({xAt(m_clip.duration), o.y + rowH}, {o.x + width, o.y + totalH}, IM_COL32(0, 0, 0, 70));

    // Track labels.
    for (int i = 0; i < rows; ++i) {
        ImGui::SetCursorScreenPos({o.x, o.y + rowH * static_cast<float>(i + 1)});
        ImGui::PushID(i);
        if (i < rows - 1) {
            const AnimationTrack& t = m_clip.tracks[static_cast<usize>(i)];
            if (ImGui::Selectable(t.label().c_str(), m_track == i, 0, {leftW - 34, rowH})) m_track = i, m_key = -1;
            ImGui::SameLine(leftW - 26);
            if (ImGui::SmallButton("x")) {
                m_clip.tracks.erase(m_clip.tracks.begin() + i);
                m_track = m_key = -1;
                m_dirty = true;
                ImGui::PopID();
                break;
            }
        } else {
            ImGui::TextDisabled("Événements");
        }
        ImGui::PopID();
    }

    // Keys and events (diamonds), playhead.
    struct Hit {
        int track = -1, key = -1, event = -1;
    } hover;
    const ImVec2 mouse = ImGui::GetIO().MousePos;
    for (int i = 0; i < rows; ++i) {
        const float cy = o.y + rowH * (static_cast<float>(i) + 1.5f);
        if (i == m_track) dl->AddRectFilled({x0, cy - rowH * 0.5f}, {o.x + width, cy + rowH * 0.5f}, IM_COL32(61, 139, 253, 30));
        if (i < rows - 1) {
            const auto& keys = m_clip.tracks[static_cast<usize>(i)].curve.keys;
            for (int k = 0; k < static_cast<int>(keys.size()); ++k) {
                const float x = xAt(keys[static_cast<usize>(k)].time);
                const bool sel = i == m_track && k == m_key;
                const float r = 5.0f;
                dl->AddQuadFilled({x, cy - r}, {x + r, cy}, {x, cy + r}, {x - r, cy}, sel ? palette::kAmber : palette::kText);
                if (std::fabs(mouse.x - x) <= r + 1 && std::fabs(mouse.y - cy) <= r + 1) hover = {i, k, -1};
            }
        } else {
            for (int k = 0; k < static_cast<int>(m_clip.events.size()); ++k) {
                const float x = xAt(m_clip.events[static_cast<usize>(k)].time);
                dl->AddTriangleFilled({x - 5, cy - 6}, {x + 5, cy - 6}, {x, cy + 6}, k == m_event ? palette::kAmber : palette::kSuccess);
                if (std::fabs(mouse.x - x) <= 6 && std::fabs(mouse.y - cy) <= 7) hover = {-1, -1, k};
            }
        }
    }
    const float px = xAt(m_time);
    dl->AddLine({px, o.y}, {px, o.y + totalH}, palette::kError, 2.0f);

    // Interaction on the time area.
    ImGui::SetCursorScreenPos({x0, o.y});
    ImGui::InvisibleButton("##timeline", {std::max(1.0f, width - leftW), totalH});
    const bool active = ImGui::IsItemActive(), clicked = ImGui::IsItemClicked(ImGuiMouseButton_Left);
    if (ImGui::IsItemHovered() && ImGui::GetIO().KeyCtrl && ImGui::GetIO().MouseWheel != 0.0f)
        m_zoom = std::clamp(m_zoom * std::pow(1.2f, ImGui::GetIO().MouseWheel), 20.0f, 2000.0f);
    if (clicked) {
        m_draggingKey = m_scrubbing = false;
        if (hover.key >= 0) {
            m_track = hover.track, m_key = hover.key, m_event = -1;
            m_draggingKey = true;
        } else if (hover.event >= 0) {
            m_event = hover.event, m_key = -1;
            m_draggingKey = true;
        } else {
            m_scrubbing = true;
        }
    }
    if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left) && hover.key < 0) {
        const int row = static_cast<int>((mouse.y - o.y) / rowH) - 1;
        if (row >= 0 && row < rows - 1) {
            m_track = row;
            const float t = std::round(timeAt(mouse.x) * 100.0f) / 100.0f;
            const float saved = m_time;
            m_time = t;
            keyTracks(s, true);
            m_time = saved;
            const auto& keys = m_clip.tracks[static_cast<usize>(row)].curve.keys;
            for (int k = 0; k < static_cast<int>(keys.size()); ++k)
                if (std::fabs(keys[static_cast<usize>(k)].time - t) < 1e-4f) m_key = k;
        }
    }
    if (active && m_scrubbing) {
        m_time = timeAt(mouse.x);
        if (m_previewing || editMode) applyPreview(s);
    }
    if (active && m_draggingKey && ImGui::IsMouseDragging(ImGuiMouseButton_Left, 2.0f)) {
        const float t = std::round(timeAt(mouse.x) * 100.0f) / 100.0f;  // 10 ms steps
        if (m_event >= 0 && m_event < static_cast<int>(m_clip.events.size())) {
            m_clip.events[static_cast<usize>(m_event)].time = t;
            m_dirty = true;
        } else if (m_track >= 0 && m_key >= 0) {
            Curve& c = m_clip.tracks[static_cast<usize>(m_track)].curve;
            if (m_key < static_cast<int>(c.keys.size())) {
                Keyframe moved = c.keys[static_cast<usize>(m_key)];
                c.removeKey(static_cast<usize>(m_key));
                moved.time = t;
                Keyframe& placed = c.setKey(t, moved.value, moved.interpolation);
                placed = moved;
                for (int k = 0; k < static_cast<int>(c.keys.size()); ++k)
                    if (std::fabs(c.keys[static_cast<usize>(k)].time - t) < 1e-4f) m_key = k;
                m_dirty = true;
            }
        }
    }
    if (!active) m_draggingKey = m_scrubbing = false;
    if (ImGui::IsWindowFocused() && ImGui::IsKeyPressed(ImGuiKey_Delete, false)) {
        if (m_event >= 0 && m_event < static_cast<int>(m_clip.events.size())) {
            m_clip.events.erase(m_clip.events.begin() + m_event);
            m_event = -1;
            m_dirty = true;
        } else if (m_track >= 0 && m_key >= 0) {
            m_clip.tracks[static_cast<usize>(m_track)].curve.removeKey(static_cast<usize>(m_key));
            m_key = -1;
            m_dirty = true;
        }
    }
    ImGui::SetCursorScreenPos({o.x, o.y + totalH + 4});
    ImGui::TextDisabled("Double-clic : clé   Glisser : déplacer   Suppr : supprimer   Ctrl + molette : zoom");
    ImGui::EndChild();

    // ------------------------------------------------- key / event + curve
    ImGui::BeginChild("details", {0, 0});
    if (ImGui::BeginTable("##details", 2, ImGuiTableFlags_SizingStretchProp)) {
        ImGui::TableNextColumn();
        if (m_event >= 0 && m_event < static_cast<int>(m_clip.events.size())) {
            AnimationEventKey& ev = m_clip.events[static_cast<usize>(m_event)];
            ImGui::TextUnformatted("Événement (onAnimationEvent(nom) dans le script)");
            m_dirty |= ImGui::InputText("Nom", &ev.name);
            m_dirty |= ImGui::DragFloat("Instant", &ev.time, 0.01f, 0.0f, 600.0f, "%.2f s");
        } else if (m_track >= 0 && m_track < static_cast<int>(m_clip.tracks.size())) {
            AnimationTrack& t = m_clip.tracks[static_cast<usize>(m_track)];
            ImGui::TextUnformatted(t.label().c_str());
            if (m_key >= 0 && m_key < static_cast<int>(t.curve.keys.size())) {
                Keyframe& k = t.curve.keys[static_cast<usize>(m_key)];
                if (ImGui::DragFloat("Instant", &k.time, 0.01f, 0.0f, 600.0f, "%.2f s")) {
                    t.curve.sort();
                    m_dirty = true;
                }
                if (AnimationSystem::isTextTrack(t)) {
                    m_dirty |= ImGui::InputText("Valeur", &k.text);
                } else {
                    m_dirty |= ImGui::DragFloat("Valeur", &k.value, 0.01f);
                    static const char* modes[] = {"Constante", "Linéaire", "Lissée", "Tangentes"};
                    int mode = static_cast<int>(k.interpolation);
                    if (ImGui::Combo("Interpolation", &mode, modes, 4)) {
                        k.interpolation = static_cast<Interpolation>(mode);
                        m_dirty = true;
                    }
                    if (k.interpolation == Interpolation::Custom) {
                        m_dirty |= ImGui::DragFloat("Pente entrée", &k.inTangent, 0.05f);
                        m_dirty |= ImGui::DragFloat("Pente sortie", &k.outTangent, 0.05f);
                    }
                }
            } else {
                ImGui::TextDisabled("Sélectionnez une clé.");
            }
        } else {
            ImGui::TextDisabled("Sélectionnez une piste, une clé ou un événement.");
        }
        ImGui::TableNextColumn();
        // Curve of the selected track.
        if (m_track >= 0 && m_track < static_cast<int>(m_clip.tracks.size()) && !AnimationSystem::isTextTrack(m_clip.tracks[static_cast<usize>(m_track)])) {
            const Curve& c = m_clip.tracks[static_cast<usize>(m_track)].curve;
            const ImVec2 p = ImGui::GetCursorScreenPos();
            const ImVec2 sz{std::max(100.0f, ImGui::GetContentRegionAvail().x), std::max(60.0f, ImGui::GetContentRegionAvail().y - 4)};
            ImDrawList* cdl = ImGui::GetWindowDrawList();
            cdl->AddRectFilled(p, {p.x + sz.x, p.y + sz.y}, palette::kGraphite950);
            const float end = std::max(m_clip.duration, c.duration());
            float lo = 1e9f, hi = -1e9f;
            for (int i = 0; i <= 120; ++i) {
                const float v = c.evaluate(end * static_cast<float>(i) / 120.0f);
                lo = std::min(lo, v), hi = std::max(hi, v);
            }
            if (hi - lo < 1e-4f) lo -= 0.5f, hi += 0.5f;
            const float pad = (hi - lo) * 0.1f;
            lo -= pad, hi += pad;
            auto pt = [&](float t, float v) { return ImVec2{p.x + t / std::max(end, 1e-3f) * sz.x, p.y + sz.y - (v - lo) / (hi - lo) * sz.y}; };
            ImVec2 prev = pt(0.0f, c.evaluate(0.0f));
            for (int i = 1; i <= 160; ++i) {
                const float t = end * static_cast<float>(i) / 160.0f;
                const ImVec2 q = pt(t, c.evaluate(t));
                cdl->AddLine(prev, q, palette::kAmber, 1.5f);
                prev = q;
            }
            for (const Keyframe& k : c.keys) cdl->AddCircleFilled(pt(k.time, k.value), 3.5f, palette::kText);
            cdl->AddLine(pt(m_time, lo), pt(m_time, hi), palette::kError);
            char range[64];
            std::snprintf(range, sizeof range, "%.3g .. %.3g", static_cast<double>(lo + pad), static_cast<double>(hi - pad));
            cdl->AddText({p.x + 4, p.y + 2}, palette::kTextMuted, range);
            ImGui::Dummy(sz);
        }
        ImGui::EndTable();
    }
    ImGui::EndChild();
    ImGui::End();
}

} // namespace agr::editor
