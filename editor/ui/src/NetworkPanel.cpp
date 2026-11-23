// Network profiler panel (like Godot's debugger network profiler): the session (role, players,
// ping, encryption, compression), bandwidth over the last minute, and the entities and RPCs that
// use the network most.
#include <algorithm>
#include <cstdio>

#include <imgui.h>

#include "agr/net/NetPlugin.hpp"
#include "agr/net/Replication.hpp"
#include "agr/editor/ui/Panels.hpp"
#include "agr/editor/ui/Theme.hpp"
#include "agr/runtime/Engine.hpp"

namespace agr::editor {

namespace {
std::string kb(u64 bytes) {
    char buf[32];
    std::snprintf(buf, sizeof buf, bytes < 10240 ? "%.0f o" : "%.1f Ko", bytes < 10240 ? static_cast<double>(bytes) : bytes / 1024.0);
    return buf;
}
} // namespace

void NetworkPanel::draw(EditorShared& s) {
    if (!s.showNetwork) return;
    ImGui::SetNextWindowSize({640, 520}, ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("Réseau (profileur)", &s.showNetwork)) {
        ImGui::End();
        return;
    }
    auto* plugin = s.engine.service<NetPlugin>();
    if (!plugin) {
        ImGui::TextDisabled("Module réseau non chargé.");
        ImGui::End();
        return;
    }
    net::NetSession& session = plugin->session();
    net::Replicator& rep = plugin->replicator();
    const char* role = session.role() == net::NetRole::Host ? "hôte" : session.role() == net::NetRole::Client ? "client" : "aucune session";
    ImGui::Text("Rôle : %s   joueur n° %u   port %u", role, session.localId(), session.port());
    ImGui::SameLine();
    ImGui::TextDisabled("  (actif en mode Jeu, quand un script héberge ou rejoint)");
    const auto& st = session.stats();
    ImGui::TextWrapped("Envoyé %s (%llu paquets)   reçu %s (%llu)   renvoyés %llu   rejetés %llu   périmés %llu", kb(st.bytesSent).c_str(),
                static_cast<unsigned long long>(st.packetsSent), kb(st.bytesReceived).c_str(), static_cast<unsigned long long>(st.packetsReceived),
                static_cast<unsigned long long>(st.resent), static_cast<unsigned long long>(st.rejected), static_cast<unsigned long long>(st.staleDropped));
    if (st.bytesBeforeCompression > 0)
        ImGui::Text("Compression : %s → %s (%.0f %% gagnés)", kb(st.bytesBeforeCompression).c_str(), kb(st.bytesAfterCompression).c_str(),
                    100.0 * (1.0 - static_cast<double>(st.bytesAfterCompression) / static_cast<double>(st.bytesBeforeCompression)));

    // Bandwidth over the last minute.
    std::vector<float> out, in;
    float top = 1.0f;
    for (const auto& b : session.bandwidthHistory()) {
        out.push_back(static_cast<float>(b.sent) / 1024.0f);
        in.push_back(static_cast<float>(b.received) / 1024.0f);
        top = std::max({top, out.back(), in.back()});
    }
    ImGui::SeparatorText("Débit (Ko/s, dernière minute)");
    if (out.empty()) {
        ImGui::TextDisabled("Pas encore de mesure.");
    } else {
        char label[64];
        std::snprintf(label, sizeof label, "envoi : %.1f Ko/s", out.back());
        ImGui::PlotLines("##out", out.data(), static_cast<int>(out.size()), 0, label, 0.0f, top * 1.1f, {-1, 60});
        std::snprintf(label, sizeof label, "réception : %.1f Ko/s", in.back());
        ImGui::PlotLines("##in", in.data(), static_cast<int>(in.size()), 0, label, 0.0f, top * 1.1f, {-1, 60});
    }

    ImGui::SeparatorText("Joueurs");
    if (ImGui::BeginTable("##peers", 3, ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerV)) {
        ImGui::TableSetupColumn("Joueur");
        ImGui::TableSetupColumn("Ping");
        ImGui::TableSetupColumn("Lien");
        ImGui::TableHeadersRow();
        for (u32 peer : session.peers()) {
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::Text("%u%s", peer, peer == net::NetSession::kHostId ? " (hôte)" : "");
            ImGui::TableNextColumn();
            ImGui::Text("%.0f ms", session.rtt(peer) * 1000.0);
            ImGui::TableNextColumn();
            ImGui::TextUnformatted(session.encrypted(peer) ? "chiffré" : "en clair");
        }
        ImGui::EndTable();
    }

    const auto& rs = rep.stats();
    ImGui::SeparatorText("Synchronisation");
    ImGui::TextWrapped("Entités NetSync %u   états envoyés / reçus (image) %u / %u   ignorés (inchangés) %u   cachés %u", rs.synced, rs.statesSent, rs.statesReceived,
                rs.statesSkipped, rs.statesHidden);
    ImGui::TextWrapped("RPC envoyées %u, reçues %u   prédiction : entrées %u envoyées, %u appliquées, %u rejouées, dernière correction %.3f m", rs.rpcsSent,
                rs.rpcsReceived, rs.inputsSent, rs.inputsApplied, rs.replayed, rs.lastCorrection);

    // Entities by traffic.
    struct Row {
        std::string name;
        net::Replicator::EntityTraffic t;
    };
    std::vector<Row> rows;
    Scene& scene = s.engine.scene();
    for (const auto& [uuid, t] : rep.profile()) {
        const ecs::Entity e = scene.findByUuid(Uuid(uuid));
        rows.push_back({e.isNull() ? "(supprimée)" : scene.nameOf(e), t});
    }
    std::sort(rows.begin(), rows.end(), [](const Row& a, const Row& b) {
        return a.t.stateBytesOut + a.t.stateBytesIn + a.t.rpcsOut + a.t.rpcsIn > b.t.stateBytesOut + b.t.stateBytesIn + b.t.rpcsOut + b.t.rpcsIn;
    });
    ImGui::SeparatorText("Entités (depuis le début)");
    if (ImGui::SmallButton("Remettre à zéro")) rep.clearProfile();
    if (ImGui::BeginTable("##entities", 5, ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY | ImGuiTableFlags_Resizable, {0, 0})) {
        ImGui::TableSetupScrollFreeze(0, 1);
        ImGui::TableSetupColumn("Entité");
        ImGui::TableSetupColumn("États envoyés");
        ImGui::TableSetupColumn("États reçus");
        ImGui::TableSetupColumn("RPC (env. / reç.)");
        ImGui::TableSetupColumn("Fonctions");
        ImGui::TableHeadersRow();
        for (const Row& r : rows) {
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::TextUnformatted(r.name.c_str());
            ImGui::TableNextColumn();
            ImGui::Text("%u  (%s)", r.t.statesOut, kb(r.t.stateBytesOut).c_str());
            ImGui::TableNextColumn();
            ImGui::Text("%u  (%s)", r.t.statesIn, kb(r.t.stateBytesIn).c_str());
            ImGui::TableNextColumn();
            ImGui::Text("%u / %u", r.t.rpcsOut, r.t.rpcsIn);
            ImGui::TableNextColumn();
            std::string fns;
            for (const auto& [fn, n] : r.t.rpcCalls) fns += (fns.empty() ? "" : ", ") + fn + " ×" + std::to_string(n);
            ImGui::TextUnformatted(fns.c_str());
        }
        ImGui::EndTable();
    }
    ImGui::End();
}

} // namespace agr::editor
