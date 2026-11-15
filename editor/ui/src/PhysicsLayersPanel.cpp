// Collision layers panel: the names of the 32 layers of the project (assets://physics_layers.json),
// used by the inspector (Collider / Collider2D / CharacterController3D: layer and collidesWith)
// and by scripts (agr.physics3d.layer("Ennemis"), agr.physics3d.mask(...)).
#include <imgui.h>
#include <misc/cpp/imgui_stdlib.h>

#include "agr/editor/ui/Panels.hpp"

namespace agr::editor {

void PhysicsLayersPanel::draw(EditorShared& s) {
    if (!s.showPhysicsLayers) return;
    ImGui::SetNextWindowSize({420, 560}, ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("Couches de collision", &s.showPhysicsLayers)) {
        ImGui::End();
        return;
    }
    if (!s.ctx.hasProject()) {
        ImGui::TextDisabled("Ouvrez un projet pour nommer ses couches.");
        ImGui::End();
        return;
    }
    VirtualFileSystem& vfs = s.ctx.engine().vfs();
    s.physicsLayers.refresh(vfs);
    ImGui::TextWrapped("Chaque collisionneur a une couche et la liste des couches qu'il touche : deux objets se "
                       "touchent si chacun accepte la couche de l'autre. Les scripts utilisent ces noms : "
                       "agr.physics3d.mask(\"Joueur\", \"Ennemis\").");
    ImGui::Separator();
    if (ImGui::BeginTable("##layers", 2, ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY | ImGuiTableFlags_BordersInnerV)) {
        ImGui::TableSetupColumn("N°", ImGuiTableColumnFlags_WidthFixed, 32.0f);
        ImGui::TableSetupColumn("Nom", ImGuiTableColumnFlags_WidthStretch);
        for (u32 i = 0; i < PhysicsLayers::kCount; ++i) {
            ImGui::PushID(static_cast<int>(i));
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::Text("%u", i);
            ImGui::TableNextColumn();
            ImGui::SetNextItemWidth(-FLT_MIN);
            const std::string hint = i == 0 ? "Default" : "Couche " + std::to_string(i);
            ImGui::InputTextWithHint("##name", hint.c_str(), &s.physicsLayers.names[i]);
            if (ImGui::IsItemDeactivatedAfterEdit()) {
                if (Status st = s.physicsLayers.save(vfs); !st) s.notify("Enregistrement des couches impossible : " + st.error().message);
                else s.physicsLayers.refresh(vfs);  // takes the new file time
            }
            ImGui::PopID();
        }
        ImGui::EndTable();
    }
    ImGui::End();
}

} // namespace agr::editor
