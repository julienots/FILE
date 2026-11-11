#include <algorithm>
#include <cstdio>

#include <imgui.h>
#include <misc/cpp/imgui_stdlib.h>

#include "agr/editor/ui/Panels.hpp"
#include "agr/runtime/InputActions.hpp"

namespace agr::editor {

namespace {

constexpr const char* kTypeLabels[] = {"Touche", "Bouton de souris", "Bouton de manette", "Axe de manette", "Zone tactile", "Joystick tactile"};
constexpr const char* kMouseLabels[] = {"Gauche", "Milieu", "Droit", "X1", "X2"};

void save(EditorShared& s, InputActions& actions) {
    if (Status st = actions.save(s.ctx.engine().vfs()); !st) s.notify("Enregistrement des entrées impossible : " + st.error().message);
}

/// First key or gamepad button pressed this frame (for capture).
bool captureInput(const Input& in, InputBinding& b) {
    if (b.type == InputBinding::Type::Key) {
        for (u16 k = 1; k < static_cast<u16>(Key::Count); ++k)
            if (in.keyPressed(static_cast<Key>(k)) && *keyName(static_cast<Key>(k))) {
                b.key = static_cast<Key>(k);
                return true;
            }
    } else if (b.type == InputBinding::Type::GamepadButton) {
        for (u8 i = 0; i < static_cast<u8>(GamepadButton::Count); ++i)
            if (in.anyGamepadPressed(static_cast<GamepadButton>(i))) {
                b.button = static_cast<GamepadButton>(i);
                return true;
            }
    }
    return false;
}

} // namespace

void InputPanel::draw(EditorShared& s) {
    if (!s.showInput) return;
    if (!ImGui::Begin("Entrées", &s.showInput)) {
        ImGui::End();
        return;
    }
    InputActions& actions = s.ctx.engine().actions();
    InputMap map = actions.map();  // edited copy; applied and saved on change
    bool changed = false;
    ImGui::TextDisabled("Actions du jeu (assets://input.agrinput) : les scripts lisent agr.input.down(\"jump\"), value(\"move_x\")...");

    // ---- action list
    ImGui::BeginChild("##actions", {200, 0}, ImGuiChildFlags_Borders | ImGuiChildFlags_ResizeX);
    for (int i = 0; i < static_cast<int>(map.actions.size()); ++i) {
        const InputAction& a = map.actions[static_cast<usize>(i)];
        const ActionState& st = actions.state(a.name);
        char label[160];
        std::snprintf(label, sizeof label, "%s%s##%d", a.name.c_str(), a.kind == InputAction::Kind::Axis ? "  (axe)" : "", i);
        if (ImGui::Selectable(label, m_selected == i)) {
            m_selected = i;
            m_capture = -1;
        }
        if (st.down || st.value != 0.0f) {  // live state while the game runs
            ImGui::SameLine(ImGui::GetContentRegionAvail().x - 34.0f + ImGui::GetCursorPosX());
            ImGui::TextColored(palette::toVec4(palette::kAmber), "%+.1f", static_cast<double>(st.value));
        }
    }
    ImGui::Separator();
    ImGui::SetNextItemWidth(-FLT_MIN);
    const bool enter = ImGui::InputTextWithHint("##new", "nouvelle action", &m_newName, ImGuiInputTextFlags_EnterReturnsTrue);
    if ((enter || ImGui::Button("Ajouter")) && !m_newName.empty()) {
        if (map.find(m_newName)) {
            s.notify("L'action « " + m_newName + " » existe déjà");
        } else {
            InputAction a;
            a.name = m_newName;
            map.actions.push_back(a);
            m_selected = static_cast<int>(map.actions.size()) - 1;
            m_newName.clear();
            changed = true;
        }
    }
    ImGui::SameLine();
    if (ImGui::Button("Par défaut")) ImGui::OpenPopup("##resetInput");
    if (ImGui::BeginPopup("##resetInput")) {
        ImGui::TextUnformatted("Remplacer toutes les actions par celles par défaut ?");
        if (ImGui::Button("Remplacer")) {
            map = InputMap::defaults();
            m_selected = 0;
            changed = true;
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
    ImGui::EndChild();
    ImGui::SameLine();

    // ---- selected action
    ImGui::BeginChild("##action", {0, 0});
    if (m_selected >= 0 && m_selected < static_cast<int>(map.actions.size())) {
        InputAction& a = map.actions[static_cast<usize>(m_selected)];
        ImGui::SetNextItemWidth(200);
        std::string name = a.name;
        if (ImGui::InputText("Nom", &name, ImGuiInputTextFlags_EnterReturnsTrue) && !name.empty() && !map.find(name)) {
            a.name = name;
            changed = true;
        }
        int kind = a.kind == InputAction::Kind::Axis ? 1 : 0;
        ImGui::SetNextItemWidth(200);
        if (ImGui::Combo("Type", &kind, "Bouton (appuyé / relâché)\0Axe (-1 à 1)\0")) {
            a.kind = kind ? InputAction::Kind::Axis : InputAction::Kind::Button;
            changed = true;
        }
        ImGui::SetNextItemWidth(200);
        if (ImGui::SliderFloat("Zone morte", &a.deadzone, 0.0f, 0.9f, "%.2f")) changed = true;
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Les valeurs analogiques (sticks, joystick tactile) en dessous comptent pour 0");
        ImGui::SameLine();
        if (ImGui::Button("Supprimer l'action")) {
            map.actions.erase(map.actions.begin() + m_selected);
            m_selected = std::max(0, m_selected - 1);
            changed = true;
            ImGui::EndChild();
            goto apply;
        }
        ImGui::SeparatorText("Liaisons");
        for (int bi = 0; bi < static_cast<int>(a.bindings.size()); ++bi) {
            InputBinding& b = a.bindings[static_cast<usize>(bi)];
            ImGui::PushID(bi);
            ImGui::AlignTextToFramePadding();
            ImGui::TextUnformatted(kTypeLabels[static_cast<usize>(b.type)]);
            ImGui::SameLine(150);
            ImGui::SetNextItemWidth(150);
            switch (b.type) {
                case InputBinding::Type::Key:
                case InputBinding::Type::GamepadButton: {
                    const bool listening = m_capture == bi;
                    const std::string current = b.type == InputBinding::Type::Key ? keyName(b.key) : enumName(b.button);
                    if (ImGui::Button(listening ? "Appuyez..." : current.c_str(), {150, 0})) m_capture = listening ? -1 : bi;
                    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Cliquer puis appuyer sur la touche ou le bouton voulu");
                    if (listening && captureInput(s.ctx.engine().input(), b)) {
                        m_capture = -1;
                        changed = true;
                    }
                    if (b.type == InputBinding::Type::GamepadButton) {
                        ImGui::SameLine();
                        int button = static_cast<int>(b.button);
                        ImGui::SetNextItemWidth(130);
                        if (ImGui::BeginCombo("##btn", enumName(b.button))) {
                            for (int i = 0; i < static_cast<int>(GamepadButton::Count); ++i)
                                if (ImGui::Selectable(enumName(static_cast<GamepadButton>(i)), i == button)) {
                                    b.button = static_cast<GamepadButton>(i);
                                    changed = true;
                                }
                            ImGui::EndCombo();
                        }
                    }
                    break;
                }
                case InputBinding::Type::MouseButton: {
                    int m = static_cast<int>(b.mouse);
                    if (ImGui::Combo("##mouse", &m, kMouseLabels, IM_ARRAYSIZE(kMouseLabels))) {
                        b.mouse = static_cast<MouseButton>(m);
                        changed = true;
                    }
                    break;
                }
                case InputBinding::Type::GamepadAxis: {
                    if (ImGui::BeginCombo("##axis", enumName(b.axis))) {
                        for (int i = 0; i < static_cast<int>(GamepadAxis::Count); ++i)
                            if (ImGui::Selectable(enumName(static_cast<GamepadAxis>(i)), static_cast<int>(b.axis) == i)) {
                                b.axis = static_cast<GamepadAxis>(i);
                                changed = true;
                            }
                        ImGui::EndCombo();
                    }
                    break;
                }
                case InputBinding::Type::TouchZone:
                case InputBinding::Type::TouchStick: {
                    float rect[4] = {b.zone.x, b.zone.y, b.zone.z, b.zone.w};
                    ImGui::SetNextItemWidth(260);
                    if (ImGui::SliderFloat4("##zone", rect, 0.0f, 1.0f, "%.2f")) {
                        b.zone = {rect[0], rect[1], rect[2], rect[3]};
                        changed = true;
                    }
                    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Zone de l'écran : x, y, largeur, hauteur (fractions, origine en haut à gauche)");
                    if (b.type == InputBinding::Type::TouchStick) {
                        ImGui::SameLine();
                        int axis = b.stickAxis;
                        ImGui::SetNextItemWidth(70);
                        if (ImGui::Combo("##sa", &axis, "x\0y\0")) {
                            b.stickAxis = static_cast<u8>(axis);
                            changed = true;
                        }
                        ImGui::SameLine();
                        ImGui::SetNextItemWidth(90);
                        if (ImGui::SliderFloat("##radius", &b.radius, 0.02f, 0.3f, "rayon %.2f")) changed = true;
                    }
                    break;
                }
            }
            ImGui::SameLine();
            ImGui::SetNextItemWidth(70);
            if (ImGui::DragFloat("##scale", &b.scale, 0.05f, -4.0f, 4.0f, "x %.2f")) changed = true;
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("Contribution : 1 = droite / haut, -1 = gauche / bas (axes)");
            if (b.type == InputBinding::Type::GamepadButton || b.type == InputBinding::Type::GamepadAxis) {
                ImGui::SameLine();
                ImGui::SetNextItemWidth(90);
                int pad = b.pad + 1;
                if (ImGui::Combo("##pad", &pad, "Toutes\0Manette 1\0Manette 2\0Manette 3\0Manette 4\0")) {
                    b.pad = pad - 1;
                    changed = true;
                }
            }
            ImGui::SameLine();
            if (ImGui::SmallButton("Retirer")) {
                a.bindings.erase(a.bindings.begin() + bi);
                m_capture = -1;
                changed = true;
                ImGui::PopID();
                break;
            }
            ImGui::PopID();
        }
        if (ImGui::Button("+ Liaison")) ImGui::OpenPopup("##addBinding");
        if (ImGui::BeginPopup("##addBinding")) {
            for (int t = 0; t < IM_ARRAYSIZE(kTypeLabels); ++t)
                if (ImGui::MenuItem(kTypeLabels[t])) {
                    InputBinding b;
                    b.type = static_cast<InputBinding::Type>(t);
                    if (b.type == InputBinding::Type::Key) b.key = Key::Space;
                    a.bindings.push_back(b);
                    if (b.type == InputBinding::Type::Key || b.type == InputBinding::Type::GamepadButton)
                        m_capture = static_cast<int>(a.bindings.size()) - 1;
                    changed = true;
                }
            ImGui::EndPopup();
        }
        const ActionState& st = actions.state(a.name);
        ImGui::SeparatorText("État (en jeu)");
        ImGui::ProgressBar(a.kind == InputAction::Kind::Axis ? st.value * 0.5f + 0.5f : st.value, {260, 0},
                           st.down ? "enfoncé" : "relâché");
    } else {
        ImGui::TextDisabled("Aucune action : ajoutez-en une à gauche.");
    }
    ImGui::EndChild();
apply:
    if (changed) {
        actions.setMap(std::move(map));
        save(s, actions);
    }
    ImGui::End();
}

} // namespace agr::editor
