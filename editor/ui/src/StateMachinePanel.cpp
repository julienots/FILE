#include <algorithm>
#include <cmath>
#include <filesystem>

#include <imgui.h>
#include <misc/cpp/imgui_stdlib.h>

#include "agr/editor/ui/Panels.hpp"

namespace agr::editor {

namespace {

constexpr float kNodeW = 150.0f, kNodeH = 44.0f;
const ImVec2 kAnyPos{-200.0f, 0.0f};  // the "any state" node, fixed

ecs::Entity animatedRoot(Scene& scene, ecs::Entity e) {
    for (; !e.isNull(); e = scene.parent(e))
        if (scene.registry().has<AnimatorComponent>(e)) return e;
    return {};
}

ImVec2 add(ImVec2 a, ImVec2 b) { return {a.x + b.x, a.y + b.y}; }

/// Point where the segment from the center of a node to `towards` leaves the node's box.
ImVec2 boxExit(ImVec2 center, ImVec2 towards) {
    const float dx = towards.x - center.x, dy = towards.y - center.y;
    if (std::fabs(dx) < 1e-3f && std::fabs(dy) < 1e-3f) return center;
    const float sx = std::fabs(dx) > 1e-3f ? (kNodeW * 0.5f) / std::fabs(dx) : 1e9f;
    const float sy = std::fabs(dy) > 1e-3f ? (kNodeH * 0.5f) / std::fabs(dy) : 1e9f;
    const float t = std::min(sx, sy);
    return {center.x + dx * t, center.y + dy * t};
}

float distanceToSegment(ImVec2 p, ImVec2 a, ImVec2 b) {
    const float vx = b.x - a.x, vy = b.y - a.y;
    const float len2 = vx * vx + vy * vy;
    const float t = len2 > 0 ? std::clamp(((p.x - a.x) * vx + (p.y - a.y) * vy) / len2, 0.0f, 1.0f) : 0.0f;
    const float dx = a.x + vx * t - p.x, dy = a.y + vy * t - p.y;
    return std::sqrt(dx * dx + dy * dy);
}

} // namespace

void StateMachinePanel::load(EditorShared& s, const std::string& uri) {
    m_uri = uri;
    m_state = m_transition = m_dragState = -1;
    m_layer = -1;
    m_dirty = false;
    m_ctrl = {};
    auto text = s.engine.vfs().readText(uri);
    if (!text) {
        s.notify("Machine à états introuvable : " + uri);
        return;
    }
    auto c = AnimatorController::fromJson(text.value());
    if (!c) {
        s.notify(c.error().message);
        return;
    }
    m_ctrl = std::move(c.value());
}

void StateMachinePanel::drawLayerBar(EditorShared& s) {
    (void)s;
    if (m_layer >= static_cast<int>(m_ctrl.layers.size())) m_layer = -1;
    auto layerName = [&](int i) { return i < 0 ? std::string("Base") : m_ctrl.layers[static_cast<usize>(i)].name; };
    ImGui::SetNextItemWidth(160);
    if (ImGui::BeginCombo("Couche", layerName(m_layer).c_str())) {
        for (int i = -1; i < static_cast<int>(m_ctrl.layers.size()); ++i)
            if (ImGui::Selectable(layerName(i).c_str(), i == m_layer)) {
                m_layer = i;
                m_state = m_transition = m_dragState = -1;
                m_maskText.clear();
                if (i >= 0)
                    for (const std::string& m : m_ctrl.layers[static_cast<usize>(i)].mask) m_maskText += (m_maskText.empty() ? "" : ", ") + m;
            }
        ImGui::EndCombo();
    }
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("Couches : d'autres machines à états jouées par-dessus la base (haut du corps qui tire pendant que les jambes courent)");
    ImGui::SameLine();
    if (ImGui::Button("+ Couche")) {
        AnimatorLayer l;
        l.name = "Couche " + std::to_string(m_ctrl.layers.size() + 1);
        l.states.push_back({"Vide", "", "", 1.0f, {0.0f, 0.0f}});
        l.entry = "Vide";
        m_ctrl.layers.push_back(std::move(l));
        m_layer = static_cast<int>(m_ctrl.layers.size()) - 1;
        m_state = m_transition = m_dragState = -1;
        m_maskText.clear();
        m_dirty = true;
    }
    if (m_layer < 0) return;
    AnimatorLayer& l = m_ctrl.layers[static_cast<usize>(m_layer)];
    ImGui::SameLine();
    ImGui::SetNextItemWidth(120);
    m_dirty |= ImGui::InputText("Nom##layer", &l.name);
    ImGui::SameLine();
    ImGui::SetNextItemWidth(100);
    m_dirty |= ImGui::SliderFloat("Poids", &l.weight, 0.0f, 1.0f);
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Part de la couche sur les valeurs du dessous ; les scripts le changent (setLayerWeight)");
    ImGui::SameLine();
    m_dirty |= ImGui::Checkbox("Additive", &l.additive);
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Ajoute le mouvement de ses clips (depuis leur première image) au lieu de remplacer : respiration, recul, sursaut");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(220);
    if (ImGui::InputTextWithHint("Masque", "Torse, Torse/Bras (vide = tout)", &m_maskText)) {
        l.mask.clear();
        std::string cur;
        for (char c : m_maskText + ",") {
            if (c == ',') {
                const auto b = cur.find_first_not_of(' '), e = cur.find_last_not_of(' ');
                if (b != std::string::npos) l.mask.push_back(cur.substr(b, e - b + 1));
                cur.clear();
            } else {
                cur += c;
            }
        }
        m_dirty = true;
    }
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Objets (chemins sous l'Animator, séparés par des virgules) que la couche anime, avec leurs enfants");
    ImGui::SameLine();
    if (ImGui::Button("Supprimer la couche")) {
        m_ctrl.layers.erase(m_ctrl.layers.begin() + m_layer);
        m_layer = -1;
        m_state = m_transition = m_dragState = -1;
        m_dirty = true;
    }
}

bool StateMachinePanel::save(EditorShared& s) {
    if (m_uri.empty()) return false;
    if (Status st = s.engine.vfs().writeText(m_uri, m_ctrl.toJson()); !st) {
        s.notify("Enregistrement impossible : " + st.error().message);
        return false;
    }
    m_dirty = false;
    if (s.animation) s.animation->animation().library().reloadChanged();
    s.notify("Machine à états enregistrée : " + m_uri);
    return true;
}

void StateMachinePanel::drawProperties(EditorShared& s) {
    (void)s;
    // ------------------------------------------------------------- parameters
    ImGui::SeparatorText("Paramètres");
    static const char* types[] = {"Nombre", "Entier", "Booléen", "Déclencheur"};
    for (usize i = 0; i < m_ctrl.parameters.size(); ++i) {
        AnimatorParameter& p = m_ctrl.parameters[i];
        ImGui::PushID(static_cast<int>(i));
        ImGui::SetNextItemWidth(100);
        m_dirty |= ImGui::InputText("##n", &p.name);
        ImGui::SameLine();
        ImGui::SetNextItemWidth(95);
        int t = static_cast<int>(p.type);
        if (ImGui::Combo("##t", &t, types, 4)) {
            p.type = static_cast<ParameterType>(t);
            m_dirty = true;
        }
        ImGui::SameLine();
        if (p.type == ParameterType::Bool) {
            bool b = p.defaultValue >= 0.5f;
            if (ImGui::Checkbox("##d", &b)) p.defaultValue = b ? 1.0f : 0.0f, m_dirty = true;
        } else if (p.type != ParameterType::Trigger) {
            ImGui::SetNextItemWidth(60);
            m_dirty |= ImGui::DragFloat("##d", &p.defaultValue, 0.05f);
        }
        ImGui::SameLine();
        if (ImGui::SmallButton("x")) {
            m_ctrl.parameters.erase(m_ctrl.parameters.begin() + static_cast<std::ptrdiff_t>(i));
            m_dirty = true;
            ImGui::PopID();
            break;
        }
        ImGui::PopID();
    }
    if (ImGui::SmallButton("+ Paramètre")) {
        m_ctrl.parameters.push_back({"param" + std::to_string(m_ctrl.parameters.size() + 1), ParameterType::Float, 0.0f});
        m_dirty = true;
    }

    // ------------------------------------------------------------- selection
    if (m_state >= 0 && m_state < static_cast<int>(states().size())) {
        AnimatorState& st = states()[static_cast<usize>(m_state)];
        ImGui::SeparatorText("État");
        const std::string oldName = st.name;
        if (ImGui::InputText("Nom", &st.name)) {
            // Keep transitions and the entry pointing at the renamed state.
            for (AnimatorTransition& t : transitions()) {
                if (t.from == oldName) t.from = st.name;
                if (t.to == oldName) t.to = st.name;
            }
            if (entry() == oldName) entry() = st.name;
            m_dirty = true;
        }
        m_dirty |= ImGui::InputText("Clip", &st.clip);
        if (ImGui::BeginDragDropTarget()) {
            if (const ImGuiPayload* p = ImGui::AcceptDragDropPayload(kPayloadAsset)) {
                st.clip = static_cast<const char*>(p->Data);
                m_dirty = true;
            }
            ImGui::EndDragDropTarget();
        }
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Clip de propriétés (.agranim) ; glissez-le depuis l'explorateur");
        m_dirty |= ImGui::InputText("Clip de sprite", &st.spriteClip);
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Nom d'un clip de la feuille de sprites (SpriteAnimator.clip)");
        m_dirty |= ImGui::DragFloat("Vitesse", &st.speed, 0.01f, 0.0f, 10.0f);
        // ---- blend state (1D blend tree): motions mixed by a parameter.
        bool blend = st.isBlend();
        if (ImGui::Checkbox("Mélange (blend tree)", &blend)) {
            if (blend) {
                st.motions = {{st.clip, 0.0f}, {"", 1.0f}};
                for (const AnimatorParameter& p : m_ctrl.parameters)
                    if (st.blendParameter.empty() && (p.type == ParameterType::Float || p.type == ParameterType::Int)) st.blendParameter = p.name;
            } else {
                st.motions.clear();
                st.blendParameterY.clear();
            }
            m_dirty = true;
        }
        if (st.isBlend()) {
            bool twoD = st.isBlend2D();
            ImGui::SameLine();
            if (ImGui::Checkbox("2D", &twoD)) {
                st.blendParameterY.clear();
                if (twoD)
                    for (const AnimatorParameter& p : m_ctrl.parameters)
                        if (st.blendParameterY.empty() && p.name != st.blendParameter && (p.type == ParameterType::Float || p.type == ParameterType::Int))
                            st.blendParameterY = p.name;
                if (twoD && st.blendParameterY.empty()) st.blendParameterY = "y";
                m_dirty = true;
            }
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("Espace de mélange 2D : chaque clip placé en (x, y) selon deux paramètres (déplacements latéraux, visée)");
        }
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Mélange des clips selon un paramètre (ex. Idle 0, Walk 2, Run 5 selon « speed »), pas synchronisés");
        if (st.isBlend()) {
            if (st.isBlend2D() && ImGui::BeginCombo("Paramètre Y", st.blendParameterY.c_str())) {
                for (const AnimatorParameter& p : m_ctrl.parameters) {
                    if (p.type != ParameterType::Float && p.type != ParameterType::Int) continue;
                    if (ImGui::Selectable(p.name.c_str(), p.name == st.blendParameterY)) {
                        st.blendParameterY = p.name;
                        m_dirty = true;
                    }
                }
                ImGui::EndCombo();
            }
            if (ImGui::BeginCombo(st.isBlend2D() ? "Paramètre X" : "Paramètre", st.blendParameter.c_str())) {
                for (const AnimatorParameter& p : m_ctrl.parameters) {
                    if (p.type != ParameterType::Float && p.type != ParameterType::Int) continue;
                    if (ImGui::Selectable(p.name.c_str(), p.name == st.blendParameter)) {
                        st.blendParameter = p.name;
                        m_dirty = true;
                    }
                }
                ImGui::EndCombo();
            }
            int removeAt = -1;
            for (usize i = 0; i < st.motions.size(); ++i) {
                ImGui::PushID(static_cast<int>(i));
                BlendMotion& m = st.motions[i];
                ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x * 0.6f);
                m_dirty |= ImGui::InputText("##clip", &m.clip);
                if (ImGui::BeginDragDropTarget()) {
                    if (const ImGuiPayload* p = ImGui::AcceptDragDropPayload(kPayloadAsset)) {
                        m.clip = static_cast<const char*>(p->Data);
                        m_dirty = true;
                    }
                    ImGui::EndDragDropTarget();
                }
                ImGui::SameLine();
                ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - ImGui::GetFrameHeight() - ImGui::GetStyle().ItemSpacing.x);
                if (st.isBlend2D()) {
                    m_dirty |= ImGui::DragFloat2("##position", &m.position.x, 0.01f);
                } else if (ImGui::DragFloat("##threshold", &m.threshold, 0.01f)) {
                    m_dirty = true;
                }
                if (!st.isBlend2D() && ImGui::IsItemDeactivatedAfterEdit())  // keep the motions ordered by threshold
                    std::stable_sort(st.motions.begin(), st.motions.end(), [](const BlendMotion& a, const BlendMotion& b) { return a.threshold < b.threshold; });
                ImGui::SameLine();
                if (ImGui::Button("x")) removeAt = static_cast<int>(i);
                ImGui::PopID();
            }
            if (removeAt >= 0 && st.motions.size() > 1) {
                st.motions.erase(st.motions.begin() + removeAt);
                m_dirty = true;
            }
            if (ImGui::Button("+ Mouvement")) {
                st.motions.push_back({"", st.motions.empty() ? 0.0f : st.motions.back().threshold + 1.0f});
                m_dirty = true;
            }
        }
        if (entry() != st.name && ImGui::Button("Définir comme état initial")) {
            entry() = st.name;
            m_dirty = true;
        }
    } else if (m_transition >= 0 && m_transition < static_cast<int>(transitions().size())) {
        AnimatorTransition& t = transitions()[static_cast<usize>(m_transition)];
        ImGui::SeparatorText("Transition");
        ImGui::Text("%s  ->  %s", t.from == "*" ? "Tout état" : t.from.c_str(), t.to.c_str());
        bool exit = t.exitTime >= 0.0f;
        if (ImGui::Checkbox("Attendre la fin du clip", &exit)) {
            t.exitTime = exit ? 1.0f : -1.0f;
            m_dirty = true;
        }
        if (exit) m_dirty |= ImGui::DragFloat("Instant de sortie", &t.exitTime, 0.01f, 0.0f, 10.0f, "%.2f (fraction du clip)");
        m_dirty |= ImGui::DragFloat("Fondu", &t.duration, 0.01f, 0.0f, 5.0f, "%.2f s");
        m_dirty |= ImGui::Checkbox("Interruptible pendant le fondu", &t.interruptible);
        ImGui::TextUnformatted("Conditions (toutes requises)");
        static const char* ops[] = {">", "<", "==", "!=", "vrai", "faux"};
        for (usize i = 0; i < t.conditions.size(); ++i) {
            TransitionCondition& c = t.conditions[i];
            ImGui::PushID(static_cast<int>(i));
            ImGui::SetNextItemWidth(110);
            if (ImGui::BeginCombo("##p", c.parameter.c_str())) {
                for (const AnimatorParameter& p : m_ctrl.parameters)
                    if (ImGui::Selectable(p.name.c_str(), p.name == c.parameter)) c.parameter = p.name, m_dirty = true;
                ImGui::EndCombo();
            }
            ImGui::SameLine();
            ImGui::SetNextItemWidth(60);
            int op = static_cast<int>(c.op);
            if (ImGui::Combo("##o", &op, ops, 6)) c.op = static_cast<ConditionOp>(op), m_dirty = true;
            if (c.op <= ConditionOp::NotEqual) {
                ImGui::SameLine();
                ImGui::SetNextItemWidth(60);
                m_dirty |= ImGui::DragFloat("##v", &c.value, 0.05f);
            }
            ImGui::SameLine();
            if (ImGui::SmallButton("x")) {
                t.conditions.erase(t.conditions.begin() + static_cast<std::ptrdiff_t>(i));
                m_dirty = true;
                ImGui::PopID();
                break;
            }
            ImGui::PopID();
        }
        if (ImGui::SmallButton("+ Condition")) {
            t.conditions.push_back({m_ctrl.parameters.empty() ? std::string() : m_ctrl.parameters.front().name, ConditionOp::IsTrue, 0.0f});
            m_dirty = true;
        }
        if (ImGui::Button("Supprimer la transition")) {
            transitions().erase(transitions().begin() + m_transition);
            m_transition = -1;
            m_dirty = true;
        }
    } else {
        ImGui::SeparatorText("Sélection");
        ImGui::TextDisabled("Cliquez sur un état ou une flèche.\nClic droit : ajouter un état ;\nclic droit sur un état : transitions.");
    }
    const std::vector<std::string> problems = m_ctrl.validate();
    if (!problems.empty()) {
        ImGui::SeparatorText("À corriger");
        for (const std::string& p : problems) ImGui::TextColored(palette::toVec4(palette::kWarning), "%s", p.c_str());
    }
}

void StateMachinePanel::draw(EditorShared& s) {
    if (!s.showStateMachine) return;
    if (!ImGui::Begin("Machine à états", &s.showStateMachine)) {
        ImGui::End();
        return;
    }
    Scene& scene = s.ctx.scene();
    const ecs::Entity root = animatedRoot(scene, scene.findByUuid(s.ctx.selection().primary()));
    if (root.isNull()) {
        ImGui::TextDisabled("Sélectionnez une entité avec un composant Animator.");
        ImGui::End();
        return;
    }
    auto& animator = scene.registry().get<AnimatorComponent>(root);
    const bool editMode = s.ctx.playState() == PlayState::Edit;
    if (animator.controller.empty()) {
        ImGui::TextDisabled("Cet Animator n'a pas de machine à états.");
        ImGui::SetNextItemWidth(160);
        ImGui::InputText("##name", &m_newName);
        ImGui::SameLine();
        if (ImGui::Button("Créer") && editMode && s.ctx.hasProject()) {
            std::string name = m_newName;
            for (char& c : name)
                if (!std::isalnum(static_cast<unsigned char>(c)) && c != '_' && c != '-') c = '_';
            const std::string uri = "assets://animations/" + (name.empty() ? std::string("controller") : name) + ".agrctrl";
            AnimatorController fresh;
            fresh.states.push_back({"Idle", animator.clip, "", 1.0f, {0.0f, 0.0f}});
            fresh.entry = "Idle";
            std::filesystem::create_directories(s.ctx.project().assetsDirectory() / "animations");
            if (s.engine.vfs().exists(uri)) {
                s.notify("Ce fichier existe déjà : " + uri);
            } else if (Status st = s.engine.vfs().writeText(uri, fresh.toJson()); !st) {
                s.notify(st.error().message);
            } else {
                animator.controller = uri;
                s.ctx.commit("Assign State Machine");
            }
        }
        ImGui::End();
        return;
    }
    if (animator.controller != m_uri) {
        if (m_dirty) save(s);
        load(s, animator.controller);
    }

    if (ImGui::Button(m_dirty ? "Enregistrer*" : "Enregistrer")) save(s);
    ImGui::SameLine();
    ImGui::TextDisabled("%s", m_uri.c_str());
    drawLayerBar(s);
    const std::string current = s.animation && !editMode
                                    ? s.animation->animation().currentState(root, m_layer < 0 ? std::string_view{} : std::string_view(m_ctrl.layers[static_cast<usize>(m_layer)].name))
                                    : std::string();

    const float propsW = 300.0f;
    ImGui::BeginChild("graph", {std::max(100.0f, ImGui::GetContentRegionAvail().x - propsW), 0}, ImGuiChildFlags_Borders,
                      ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
    ImDrawList* dl = ImGui::GetWindowDrawList();
    const ImVec2 o = ImGui::GetCursorScreenPos();
    const ImVec2 size = ImGui::GetContentRegionAvail();
    const ImVec2 origin = add(o, m_pan);
    // Grid.
    for (float x = std::fmod(m_pan.x, 32.0f); x < size.x; x += 32.0f) dl->AddLine({o.x + x, o.y}, {o.x + x, o.y + size.y}, IM_COL32(255, 255, 255, 12));
    for (float y = std::fmod(m_pan.y, 32.0f); y < size.y; y += 32.0f) dl->AddLine({o.x, o.y + y}, {o.x + size.x, o.y + y}, IM_COL32(255, 255, 255, 12));

    ImGui::InvisibleButton("##canvas", {std::max(1.0f, size.x), std::max(1.0f, size.y)},
                           ImGuiButtonFlags_MouseButtonLeft | ImGuiButtonFlags_MouseButtonRight | ImGuiButtonFlags_MouseButtonMiddle);
    const bool hovered = ImGui::IsItemHovered();
    const ImVec2 mouse = ImGui::GetIO().MousePos;
    if (ImGui::IsItemActive() && ImGui::IsMouseDragging(ImGuiMouseButton_Middle)) {
        m_pan.x += ImGui::GetIO().MouseDelta.x;
        m_pan.y += ImGui::GetIO().MouseDelta.y;
    }

    auto center = [&](int i) {
        const ImVec2 p = i < 0 ? kAnyPos : ImVec2{states()[static_cast<usize>(i)].editorPosition.x, states()[static_cast<usize>(i)].editorPosition.y};
        return ImVec2{origin.x + p.x + kNodeW * 0.5f, origin.y + p.y + kNodeH * 0.5f};
    };
    auto indexOf = [&](const std::string& name) {
        if (name == "*") return -1;
        for (int i = 0; i < static_cast<int>(states().size()); ++i)
            if (states()[static_cast<usize>(i)].name == name) return i;
        return -2;
    };

    // Transitions (arrows); two-way pairs are offset so both stay visible.
    int hoverTransition = -1;
    for (int i = 0; i < static_cast<int>(transitions().size()); ++i) {
        const AnimatorTransition& t = transitions()[static_cast<usize>(i)];
        const int a = indexOf(t.from), b = indexOf(t.to);
        if (a == -2 || b < 0) continue;
        ImVec2 ca = center(a), cb = center(b);
        const float dx = cb.x - ca.x, dy = cb.y - ca.y, len = std::max(1.0f, std::sqrt(dx * dx + dy * dy));
        const ImVec2 n{-dy / len * 6.0f, dx / len * 6.0f};
        ca = add(ca, n), cb = add(cb, n);
        const ImVec2 p0 = boxExit(ca, cb), p1 = boxExit(cb, ca);
        const bool sel = i == m_transition;
        const ImU32 col = sel ? palette::kAmber : palette::kTextMuted;
        dl->AddLine(p0, p1, col, sel ? 2.5f : 1.5f);
        const ImVec2 mid{(p0.x + p1.x) * 0.5f, (p0.y + p1.y) * 0.5f};
        const ImVec2 dir{dx / len, dy / len};
        dl->AddTriangleFilled({mid.x + dir.x * 8, mid.y + dir.y * 8}, {mid.x - dir.x * 6 + n.x, mid.y - dir.y * 6 + n.y},
                              {mid.x - dir.x * 6 - n.x, mid.y - dir.y * 6 - n.y}, col);
        if (hovered && distanceToSegment(mouse, p0, p1) < 5.0f) hoverTransition = i;
    }

    // Nodes.
    int hoverState = -2;
    auto node = [&](int i, const char* label, ImU32 border, ImU32 fill) {
        const ImVec2 c = center(i);
        const ImVec2 a{c.x - kNodeW * 0.5f, c.y - kNodeH * 0.5f}, b{c.x + kNodeW * 0.5f, c.y + kNodeH * 0.5f};
        dl->AddRectFilled(a, b, fill, 6.0f);
        dl->AddRect(a, b, border, 6.0f, 2.0f);
        const ImVec2 ts = ImGui::CalcTextSize(label);
        dl->AddText({c.x - ts.x * 0.5f, c.y - ts.y * 0.5f}, palette::kText, label);
        if (hovered && mouse.x >= a.x && mouse.x <= b.x && mouse.y >= a.y && mouse.y <= b.y) hoverState = i;
    };
    node(-1, "Tout état", palette::kGraphite700, palette::kGraphite900);
    for (int i = 0; i < static_cast<int>(states().size()); ++i) {
        const AnimatorState& st = states()[static_cast<usize>(i)];
        ImU32 border = palette::kGraphite700;
        if (st.name == entry()) border = palette::kSuccess;
        if (!current.empty() && st.name == current) border = palette::kError;
        if (i == m_state) border = palette::kAmber;
        node(i, st.name.c_str(), border, palette::kGraphite800);
    }
    dl->AddText({o.x + 8, o.y + 6}, palette::kTextMuted,
                "Vert : état initial   Rouge : état courant (jeu)   Clic molette : déplacer la vue");

    // Selection and dragging.
    if (ImGui::IsItemClicked(ImGuiMouseButton_Left)) {
        m_state = hoverState >= 0 ? hoverState : -1;
        m_transition = hoverState >= 0 ? -1 : hoverTransition;
        m_dragState = hoverState >= 0 ? hoverState : -1;
    }
    if (m_dragState >= 0 && ImGui::IsItemActive() && ImGui::IsMouseDragging(ImGuiMouseButton_Left)) {
        math::Vec2& p = states()[static_cast<usize>(m_dragState)].editorPosition;
        p.x += ImGui::GetIO().MouseDelta.x;
        p.y += ImGui::GetIO().MouseDelta.y;
        m_dirty = true;
    }
    if (!ImGui::IsItemActive()) m_dragState = -1;

    // Context menus.
    static int menuState = -2;
    static ImVec2 menuPos;
    if (ImGui::IsItemClicked(ImGuiMouseButton_Right)) {
        menuState = hoverState;
        menuPos = {mouse.x - origin.x - kNodeW * 0.5f, mouse.y - origin.y - kNodeH * 0.5f};
        ImGui::OpenPopup("graphMenu");
    }
    if (ImGui::BeginPopup("graphMenu")) {
        if (menuState == -2) {
            if (ImGui::MenuItem("Ajouter un état")) {
                std::string name = "State";
                for (int n = 1; indexOf(name) != -2; ++n) name = "State" + std::to_string(n);
                states().push_back({name, "", "", 1.0f, {menuPos.x, menuPos.y}});
                if (entry().empty()) entry() = name;
                m_state = static_cast<int>(states().size()) - 1;
                m_transition = -1;
                m_dirty = true;
            }
        } else {
            const std::string from = menuState < 0 ? std::string("*") : states()[static_cast<usize>(menuState)].name;
            if (ImGui::BeginMenu("Transition vers")) {
                for (const AnimatorState& st : states()) {
                    if (st.name == from) continue;
                    if (ImGui::MenuItem(st.name.c_str())) {
                        transitions().push_back({from, st.name, {}, from == "*" ? -1.0f : 1.0f, 0.1f});
                        m_transition = static_cast<int>(transitions().size()) - 1;
                        m_state = -1;
                        m_dirty = true;
                    }
                }
                ImGui::EndMenu();
            }
            if (menuState >= 0) {
                if (ImGui::MenuItem("Définir comme état initial")) {
                    entry() = from;
                    m_dirty = true;
                }
                if (ImGui::MenuItem("Supprimer l'état")) {
                    std::erase_if(transitions(), [&](const AnimatorTransition& t) { return t.from == from || t.to == from; });
                    states().erase(states().begin() + menuState);
                    if (entry() == from) entry() = states().empty() ? std::string() : states().front().name;
                    m_state = m_transition = -1;
                    m_dirty = true;
                }
            }
        }
        ImGui::EndPopup();
    }
    ImGui::EndChild();
    ImGui::SameLine();
    ImGui::BeginChild("props", {0, 0}, ImGuiChildFlags_Borders);
    drawProperties(s);
    ImGui::EndChild();
    ImGui::End();
}

} // namespace agr::editor
