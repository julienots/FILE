// Particle effects panel: presets, over-lifetime size curve and color gradient (live preview
// through ParticleCurvesLibrary::preview), flipbook and the main emitter settings.
#include <algorithm>
#include <cmath>
#include <filesystem>

#include <imgui.h>
#include <misc/cpp/imgui_stdlib.h>

#include "agr/editor/ParticlePresets.hpp"
#include "agr/editor/ui/Panels.hpp"
#include "agr/editor/ui/Theme.hpp"

namespace agr::editor {

namespace fs = std::filesystem;

namespace {

ImU32 toU32(math::Color c) { return ImGui::ColorConvertFloat4ToU32({c.x, c.y, c.z, c.w}); }

/// Copies assets/particles of the editor's templates into an older project that lacks it.
void ensureParticleAssets(EditorShared& s, const std::string& templateAssetsDir) {
    if (!s.ctx.hasProject()) return;
    const fs::path dst = s.ctx.project().assetsDirectory() / "particles";
    std::error_code ec;
    if (fs::exists(dst / "explosion.png", ec)) return;
    fs::create_directories(dst, ec);
    fs::copy(fs::path(templateAssetsDir) / "particles", dst, fs::copy_options::recursive | fs::copy_options::skip_existing, ec);
    if (ec) s.notify("Textures d'effets non copiées : " + ec.message());
}

template <typename E>
bool emitterSettings(E& em) {
    bool changed = false;
    changed |= ImGui::DragFloat("Débit (/s)", &em.rate, 0.5f, 0.0f, 5000.0f);
    int burst = static_cast<int>(em.burst);
    if (ImGui::DragInt("Salve", &burst, 1.0f, 0, 10000)) {
        em.burst = static_cast<u32>(std::max(burst, 0));
        changed = true;
    }
    changed |= ImGui::DragFloat2("Durée de vie", &em.lifetimeMin, 0.01f, 0.01f, 60.0f);
    changed |= ImGui::DragFloat2("Vitesse", &em.speedMin, 0.01f, 0.0f, 500.0f);
    changed |= ImGui::DragFloat("Taille", &em.startSize, 0.01f, 0.0f, 100.0f);
    changed |= ImGui::DragFloat("Rotation (rad/s)", &em.angularVelocity, 0.01f, -50.0f, 50.0f);
    changed |= ImGui::Checkbox("Additif (lumineux)", &em.additive);
    ImGui::SameLine();
    changed |= ImGui::Checkbox("Boucle", &em.loop);
    int fb[2] = {em.flipbookColumns, em.flipbookRows};
    if (ImGui::DragInt2("Flipbook (col. x lignes)", fb, 0.05f, 1, 16)) {
        em.flipbookColumns = std::max(fb[0], 1);
        em.flipbookRows = std::max(fb[1], 1);
        changed = true;
    }
    if (em.flipbookColumns * em.flipbookRows > 1) changed |= ImGui::DragFloat("Cycles du flipbook", &em.flipbookCycles, 0.05f, 0.0f, 32.0f);
    return changed;
}

} // namespace

bool ParticlePanel::curveEditor(std::vector<ParticleCurves::Key>& keys) {
    bool changed = false;
    const ImVec2 size{ImGui::GetContentRegionAvail().x, 110.0f};
    const ImVec2 o = ImGui::GetCursorScreenPos();
    ImGui::InvisibleButton("##curve", size, ImGuiButtonFlags_MouseButtonLeft | ImGuiButtonFlags_MouseButtonRight);
    const bool hovered = ImGui::IsItemHovered();
    ImDrawList* dl = ImGui::GetWindowDrawList();
    float maxV = 2.0f;
    for (const auto& k : keys) maxV = std::max(maxV, k.value * 1.2f);
    auto toScreen = [&](float t, float v) { return ImVec2{o.x + t * size.x, o.y + size.y - v / maxV * size.y}; };
    auto fromScreen = [&](ImVec2 p) {
        return std::pair<float, float>{std::clamp((p.x - o.x) / size.x, 0.0f, 1.0f), std::clamp((o.y + size.y - p.y) / size.y * maxV, 0.0f, 1000.0f)};
    };
    dl->AddRectFilled(o, {o.x + size.x, o.y + size.y}, IM_COL32(20, 22, 26, 255), 4.0f);
    for (int i = 1; i < 4; ++i) dl->AddLine({o.x + size.x * i / 4.0f, o.y}, {o.x + size.x * i / 4.0f, o.y + size.y}, IM_COL32(255, 255, 255, 18));
    const ImVec2 one0 = toScreen(0, 1), one1 = toScreen(1, 1);
    dl->AddLine(one0, one1, IM_COL32(255, 255, 255, 50));  // value 1 = startSize
    ParticleCurves tmp;
    tmp.size = keys;
    ImVec2 prev = toScreen(0, tmp.sizeAt(0));
    for (int i = 1; i <= 64; ++i) {
        const float t = static_cast<float>(i) / 64.0f;
        const ImVec2 p = toScreen(t, tmp.sizeAt(t));
        dl->AddLine(prev, p, palette::kAmber, 2.0f);
        prev = p;
    }
    const ImVec2 mouse = ImGui::GetIO().MousePos;
    int hoveredKey = -1;
    for (int i = 0; i < static_cast<int>(keys.size()); ++i) {
        const ImVec2 p = toScreen(keys[static_cast<usize>(i)].t, keys[static_cast<usize>(i)].value);
        const bool near = std::abs(mouse.x - p.x) < 7 && std::abs(mouse.y - p.y) < 7;
        if (near && hovered) hoveredKey = i;
        dl->AddCircleFilled(p, near || i == m_dragKey ? 6.0f : 4.5f, i == m_dragKey ? palette::kSelect : IM_COL32(240, 240, 240, 255));
    }
    if (hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
        if (hoveredKey >= 0) {
            m_dragKey = hoveredKey;
        } else {  // a click on the curve area adds a key there
            const auto [t, v] = fromScreen(mouse);
            keys.push_back({t, v});
            std::stable_sort(keys.begin(), keys.end(), [](const auto& a, const auto& b) { return a.t < b.t; });
            changed = true;
        }
    }
    if (m_dragKey >= 0 && m_dragKey < static_cast<int>(keys.size())) {
        if (ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
            const auto [t, v] = fromScreen(mouse);
            keys[static_cast<usize>(m_dragKey)] = {t, v};
            changed = true;
        } else {
            std::stable_sort(keys.begin(), keys.end(), [](const auto& a, const auto& b) { return a.t < b.t; });
            m_dragKey = -1;
        }
    }
    if (hovered && hoveredKey >= 0 && ImGui::IsMouseClicked(ImGuiMouseButton_Right) && keys.size() > 1) {
        keys.erase(keys.begin() + hoveredKey);
        m_dragKey = -1;
        changed = true;
    }
    if (hovered) ImGui::SetTooltip("Taille au fil de la vie (1 = taille de départ). Clic : ajouter / déplacer une clé ; clic droit : supprimer");
    return changed;
}

bool ParticlePanel::gradientEditor(std::vector<ParticleCurves::ColorKey>& keys) {
    bool changed = false;
    const float w = ImGui::GetContentRegionAvail().x;
    const ImVec2 o = ImGui::GetCursorScreenPos();
    const float barH = 26.0f, stopH = 12.0f;
    ImGui::InvisibleButton("##gradient", {w, barH + stopH}, ImGuiButtonFlags_MouseButtonLeft | ImGuiButtonFlags_MouseButtonRight);
    const bool hovered = ImGui::IsItemHovered();
    ImDrawList* dl = ImGui::GetWindowDrawList();
    for (int cx = 0; cx < static_cast<int>(w / 8.0f) + 1; ++cx)  // checkerboard: transparency shows
        for (int cy = 0; cy < 3; ++cy)
            dl->AddRectFilled({o.x + cx * 8.0f, o.y + cy * 8.0f}, {std::min(o.x + w, o.x + cx * 8.0f + 8.0f), std::min(o.y + barH, o.y + cy * 8.0f + 8.0f)},
                              (cx + cy) % 2 ? IM_COL32(90, 90, 90, 255) : IM_COL32(150, 150, 150, 255));
    ParticleCurves tmp;
    tmp.gradient = keys;
    const int steps = 48;
    for (int i = 0; i < steps; ++i) {
        const float t0 = static_cast<float>(i) / steps, t1 = static_cast<float>(i + 1) / steps;
        dl->AddRectFilledMultiColor({o.x + t0 * w, o.y}, {o.x + t1 * w, o.y + barH}, toU32(tmp.colorAt(t0)), toU32(tmp.colorAt(t1)), toU32(tmp.colorAt(t1)),
                                    toU32(tmp.colorAt(t0)));
    }
    const ImVec2 mouse = ImGui::GetIO().MousePos;
    int hoveredStop = -1;
    for (int i = 0; i < static_cast<int>(keys.size()); ++i) {
        const float x = o.x + keys[static_cast<usize>(i)].t * w;
        const bool near = std::abs(mouse.x - x) < 7 && mouse.y > o.y + barH - 4;
        if (near && hovered) hoveredStop = i;
        const ImU32 edge = i == m_selectedStop ? palette::kSelect : IM_COL32(230, 230, 230, 255);
        dl->AddTriangleFilled({x, o.y + barH}, {x - 6, o.y + barH + stopH}, {x + 6, o.y + barH + stopH}, edge);
        math::Color c = keys[static_cast<usize>(i)].color;
        c.w = 1.0f;
        dl->AddRectFilled({x - 4, o.y + barH + 5}, {x + 4, o.y + barH + stopH - 1}, toU32(c));
    }
    if (hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
        if (hoveredStop >= 0) {
            m_selectedStop = m_dragStop = hoveredStop;
        } else {  // add a stop with the color already shown there
            const float t = std::clamp((mouse.x - o.x) / w, 0.0f, 1.0f);
            keys.push_back({t, tmp.colorAt(t)});
            m_selectedStop = static_cast<int>(keys.size()) - 1;
            changed = true;
        }
    }
    if (m_dragStop >= 0 && m_dragStop < static_cast<int>(keys.size())) {
        if (ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
            keys[static_cast<usize>(m_dragStop)].t = std::clamp((mouse.x - o.x) / w, 0.0f, 1.0f);
            changed = true;
        } else {
            m_dragStop = -1;
        }
    }
    if (hovered && hoveredStop >= 0 && ImGui::IsMouseClicked(ImGuiMouseButton_Right) && keys.size() > 1) {
        keys.erase(keys.begin() + hoveredStop);
        m_selectedStop = 0;
        changed = true;
    }
    if (hovered) ImGui::SetTooltip("Couleur et opacité au fil de la vie. Clic : ajouter / choisir / glisser un repère ; clic droit : supprimer");
    if (m_selectedStop >= 0 && m_selectedStop < static_cast<int>(keys.size())) {
        auto& k = keys[static_cast<usize>(m_selectedStop)];
        changed |= ImGui::ColorEdit4("Couleur du repère", &k.color.x, ImGuiColorEditFlags_AlphaBar | ImGuiColorEditFlags_AlphaPreviewHalf);
    }
    if (changed && m_dragStop < 0) {
        // Keep the selection on the same stop while sorting.
        const float selT = m_selectedStop >= 0 && m_selectedStop < static_cast<int>(keys.size()) ? keys[static_cast<usize>(m_selectedStop)].t : -1.0f;
        std::stable_sort(keys.begin(), keys.end(), [](const auto& a, const auto& b) { return a.t < b.t; });
        for (int i = 0; i < static_cast<int>(keys.size()); ++i)
            if (keys[static_cast<usize>(i)].t == selT) m_selectedStop = i;
    }
    return changed;
}

void ParticlePanel::draw(EditorShared& s, const std::string& templateAssetsDir) {
    if (!s.showParticles) return;
    if (!ImGui::Begin("Effets de particules", &s.showParticles)) {
        ImGui::End();
        return;
    }
    Scene& scene = s.ctx.scene();
    auto& reg = scene.registry();
    const ecs::Entity e = scene.findByUuid(s.ctx.selection().primary());
    auto* em2 = e.isNull() ? nullptr : reg.tryGet<ParticleEmitter2DComponent>(e);
    auto* em3 = e.isNull() ? nullptr : reg.tryGet<ParticleEmitter3DComponent>(e);
    if (!em2 && !em3) {
        ImGui::TextDisabled("Sélectionnez un émetteur de particules (2D ou 3D).");
        ImGui::TextDisabled("Créer › Particules 2D, ou Ajouter un composant › ParticleEmitter3D.");
        ImGui::End();
        return;
    }
    auto* curvesLib = s.engine.service<ParticleCurvesLibrary>();
    std::string& curvesUri = em2 ? em2->curves : em3->curves;
    auto restart = [&] {
        if (auto* st = reg.tryGet<ParticleState2DComponent>(e)) *st = {};
        if (auto* st = reg.tryGet<ParticleState3DComponent>(e)) *st = {};
    };

    // ---- presets
    ImGui::SeparatorText("Modèles");
    int column = 0;
    for (ParticlePreset p : kParticlePresets) {
        if (column++ % 4) ImGui::SameLine();
        if (ImGui::Button(particlePresetName(p), {ImGui::GetContentRegionAvail().x > 300 ? 80.0f : 0.0f, 0.0f})) {
            ensureParticleAssets(s, templateAssetsDir);
            if (em2) applyParticlePreset(p, *em2);
            if (em3) applyParticlePreset(p, *em3);
            restart();
            m_uri.clear();  // reload the preset's curves
            s.ctx.commit(std::string("Particle Preset ") + particlePresetName(p));
        }
    }
    ImGui::SameLine();
    if (ImGui::Button("Rejouer")) restart();
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Efface les particules et relance l'émission (aperçu)");

    // ---- emitter
    ImGui::SeparatorText("Émetteur");
    if (em2 ? emitterSettings(*em2) : emitterSettings(*em3)) m_pendingCommit = true;
    if (m_pendingCommit && !ImGui::IsAnyItemActive()) {  // one undo step per drag
        m_pendingCommit = false;
        s.ctx.commit("Edit Particles");
    }

    // ---- curves
    ImGui::SeparatorText("Au fil de la vie");
    if (curvesUri != m_uri) {
        if (m_dirty && curvesLib && !m_uri.empty()) (void)curvesLib->save(m_uri, m_curves);
        m_uri = curvesUri;
        m_dirty = false;
        m_curves = {};
        if (curvesLib && !m_uri.empty())
            if (const ParticleCurves* c = curvesLib->curves(m_uri)) m_curves = *c;
    }
    if (m_uri.empty()) {
        ImGui::TextDisabled("Pas de courbes : taille et couleur vont du départ à la fin.");
        if (ImGui::Button("Créer des courbes") && curvesLib && s.ctx.hasProject()) {
            std::string name = scene.nameOf(e);
            for (char& c : name)
                if (!std::isalnum(static_cast<unsigned char>(c))) c = '_';
            std::error_code ec;
            fs::create_directories(s.ctx.project().assetsDirectory() / "particles", ec);
            const std::string uri = "assets://particles/" + (name.empty() ? std::string("effect") : name) + ".agrparticle";
            ParticleCurves c;
            const math::Color c0 = em2 ? em2->startColor : em3->startColor, c1 = em2 ? em2->endColor : em3->endColor;
            const float s0 = em2 ? em2->startSize : em3->startSize, s1 = em2 ? em2->endSize : em3->endSize;
            c.size = {{0.0f, 1.0f}, {1.0f, s0 > 0.0f ? s1 / s0 : 1.0f}};
            c.gradient = {{0.0f, c0}, {1.0f, c1}};
            if (Status st = curvesLib->save(uri, c); !st) {
                s.notify(st.error().message);
            } else {
                curvesUri = uri;
                s.ctx.commit("Particle Curves");
            }
        }
    } else {
        ImGui::TextDisabled("%s", m_uri.c_str());
        ImGui::TextUnformatted("Taille");
        bool changed = curveEditor(m_curves.size);
        ImGui::TextUnformatted("Couleur et opacité");
        changed |= gradientEditor(m_curves.gradient);
        if (changed && curvesLib) {
            curvesLib->preview(m_uri, m_curves);  // the scene view shows it at once
            m_dirty = true;
        }
        if (ImGui::Button(m_dirty ? "Enregistrer les courbes*" : "Enregistrer les courbes") && curvesLib) {
            if (Status st = curvesLib->save(m_uri, m_curves); !st) s.notify(st.error().message);
            else m_dirty = false;
        }
        ImGui::SameLine();
        if (ImGui::Button("Détacher")) {
            curvesUri.clear();
            s.ctx.commit("Detach Particle Curves");
        }
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("L'émetteur revient aux valeurs de départ / fin (le fichier reste)");
    }
    ImGui::End();
}

} // namespace agr::editor
