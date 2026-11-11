#include <algorithm>
#include <cctype>
#include <filesystem>

#include <SDL3/SDL.h>
#include <imgui.h>
#include <imgui_stdlib.h>

#include "agr/editor/ui/Panels.hpp"
#include "agr/render/SpriteSheet.hpp"
#include "agr/twod/Atlas.hpp"

namespace agr::editor {

namespace fs = std::filesystem;

namespace {

std::string join(const std::string& folder, const std::string& name) { return folder.empty() ? name : folder + "/" + name; }

std::string lower(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return s;
}

fs::path absoluteAsset(EditorShared& s, const std::string& rel) { return s.ctx.project().assetsDirectory() / fs::path(rel); }

icons::Kind iconFor(const std::string& name, bool folder) {
    if (folder) return icons::Kind::Folder;
    if (isImageFile(name)) return icons::Kind::Image;
    if (fs::path(name).extension() == ".agrscene") return icons::Kind::Scene;
    if (isModelFile(name)) return icons::Kind::Model;
    if (isPrefabFile(name)) return icons::Kind::Prefab;
    if (fs::path(name).extension() == kMaterialExtension) return icons::Kind::Material;
    return icons::Kind::File;
}

std::string uniqueName(const fs::path& dir, const std::string& base) {
    std::error_code ec;
    std::string name = base;
    for (int i = 2; fs::exists(dir / name, ec); ++i) name = base + " " + std::to_string(i);
    return name;
}

/// "stem" + ext, or "stem_2" + ext... so generated files never overwrite existing assets.
std::string uniqueFile(const fs::path& dir, const std::string& stem, const std::vector<std::string>& extensions) {
    std::error_code ec;
    for (int i = 1;; ++i) {
        const std::string candidate = i == 1 ? stem : stem + "_" + std::to_string(i);
        bool free = true;
        for (const std::string& ext : extensions) free &= !fs::exists(dir / (candidate + ext), ec);
        if (free) return candidate;
    }
}

bool isSheetFile(const std::string& name) { return fs::path(name).extension() == ".agrsheet"; }

} // namespace

void AssetBrowserPanel::importFiles(EditorShared& s, const std::vector<std::string>& paths) {
    if (!s.ctx.hasProject()) return;
    const fs::path dest = absoluteAsset(s, s.assetFolder);
    std::error_code ec;
    fs::create_directories(dest, ec);
    int imported = 0;
    for (const std::string& p : paths) {
        const fs::path src(p);
        if (!fs::is_regular_file(src, ec)) continue;
        fs::path target = dest / src.filename();
        if (fs::exists(target, ec)) target = dest / (src.stem().string() + " (import)" + src.extension().string());
        fs::copy_file(src, target, fs::copy_options::skip_existing, ec);
        if (ec) {
            s.notify("Import impossible : " + src.filename().string() + " (" + ec.message() + ")");
            continue;
        }
        ++imported;
        const std::string fname = src.filename().string();
        if (!isImageFile(fname) && !isModelFile(fname) && src.extension() != kMaterialExtension && src.extension() != ".agrsheet" &&
            src.extension() != ".lua")
            AGR_LOG_WARN("Editor", "'{}' copied, but this file type has no importer yet", fname);
        if (src.extension() == ".gltf")
            AGR_LOG_INFO("Editor", "'{}': a .gltf file references its .bin and textures; import them too (same folder layout)", fname);
    }
    if (imported) s.notify(std::to_string(imported) + " fichier(s) importé(s) dans assets/" + s.assetFolder);
}

void AssetBrowserPanel::drawFolderTree(EditorShared& s, const std::string& folder) {
    auto list = s.engine.vfs().list("assets://" + folder);
    if (!list) return;
    for (const std::string& entry : list.value()) {
        if (entry.empty() || entry.back() != '/') continue;
        const std::string name = entry.substr(0, entry.size() - 1);
        const std::string path = join(folder, name);
        ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_SpanAvailWidth;
        if (s.assetFolder == path) flags |= ImGuiTreeNodeFlags_Selected;
        const bool open = ImGui::TreeNodeEx(path.c_str(), flags, "%s", name.c_str());
        if (ImGui::IsItemClicked() && !ImGui::IsItemToggledOpen()) s.assetFolder = path;
        if (open) {
            drawFolderTree(s, path);
            ImGui::TreePop();
        }
    }
}

void AssetBrowserPanel::draw(EditorShared& s) {
    if (!ImGui::Begin("Assets")) {
        ImGui::End();
        return;
    }
    if (!s.ctx.hasProject()) {
        ImGui::TextDisabled("Aucun projet ouvert.");
        ImGui::End();
        return;
    }
    std::error_code ec;
    if (!fs::is_directory(absoluteAsset(s, s.assetFolder), ec)) s.assetFolder.clear();

    // ---- toolbar
    if (ImGui::Button("Importer...")) {
        struct Ctx {
            EditorShared* shared;
            AssetBrowserPanel* panel;
        };
        static Ctx ctx;
        ctx = {&s, this};
        static const SDL_DialogFileFilter filters[] = {{"Images et modèles 3D", "png;jpg;jpeg;tga;bmp;psd;gif;hdr;gltf;glb;bin;obj;mtl;fbx"},
                                                       {"Tous les fichiers", "*"}};
        // SDL may call back from another thread: queue the paths for the main thread.
        SDL_ShowOpenFileDialog(
            [](void* user, const char* const* files, int) {
                auto* c = static_cast<Ctx*>(user);
                if (!files) return;
                std::lock_guard lock(c->panel->m_importMutex);
                for (const char* const* f = files; *f; ++f) c->panel->m_importQueue.emplace_back(*f);
            },
            &ctx, s.platform ? s.platform->window() : nullptr, filters, 2, nullptr, true);
    }
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort)) ImGui::SetTooltip("Copier des fichiers dans le dossier courant (ou glisser-déposer des fichiers sur la fenêtre)");
    ImGui::SameLine();
    if (ImGui::Button("Nouveau dossier")) {
        const fs::path dir = absoluteAsset(s, s.assetFolder);
        fs::create_directories(dir / uniqueName(dir, "Nouveau dossier"), ec);
    }
    ImGui::SameLine();
    if (ImGui::Button("Nouveau matériau")) {
        const std::string stem = uniqueFile(absoluteAsset(s, s.assetFolder), "Nouveau matériau", {kMaterialExtension});
        const std::string uri = "assets://" + join(s.assetFolder, stem + kMaterialExtension);
        MaterialData m;
        m.name = stem;
        const Status st = s.materials() ? s.materials()->save(uri, m) : s.engine.vfs().writeText(uri, materialToJson(m));
        if (st) {
            s.inspectedAsset = uri;
            s.ctx.selection().clear();
            s.notify("Matériau créé : " + uri);
        } else {
            s.notify("Création impossible : " + st.error().message);
        }
    }
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort)) ImGui::SetTooltip("Matériau PBR (.agrmat) modifiable dans l'inspecteur");
    ImGui::SameLine();
    if (ImGui::Button("Nouveau script")) {
        const std::string stem = uniqueFile(absoluteAsset(s, s.assetFolder), "nouveau_script", {".lua"});
        const std::string uri = "assets://" + join(s.assetFolder, stem + ".lua");
        static const char* kTemplate =
            "-- Script AGR ENGINE (Lua 5.4). Glisser ce fichier sur une entité de la scène pour l'y attacher.\n"
            "-- Guide : docs/SCRIPTING.md\n"
            "local Script = {\n"
            "    -- Valeurs modifiables dans l'inspecteur.\n"
            "    properties = { speed = 2 },\n"
            "}\n\n"
            "function Script:onStart()\n"
            "end\n\n"
            "function Script:onUpdate(dt)\n"
            "    local h = agr.input.axis(\"horizontal\")\n"
            "    self.entity:translate(h * self.speed * dt, 0, 0)\n"
            "end\n\n"
            "return Script\n";
        if (Status st = s.engine.vfs().writeText(uri, kTemplate); st) s.notify("Script créé : " + uri);
        else s.notify("Création impossible : " + st.error().message);
    }
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort)) ImGui::SetTooltip("Script Lua : glisser sur une entité de la vue pour l'attacher");
    ImGui::SameLine();
    if (ImGui::Button("Créer un atlas...")) m_atlasOpen = true;
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort))
        ImGui::SetTooltip("Regrouper les images du dossier courant dans une seule texture + une feuille de sprites (.agrsheet)");
    ImGui::SameLine();
    if (ImGui::Button("Ouvrir dans l'explorateur")) {
        const std::string url = "file://" + fs::absolute(absoluteAsset(s, s.assetFolder), ec).generic_string();
        if (!SDL_OpenURL(url.c_str())) s.notify(std::string("Impossible d'ouvrir le dossier : ") + SDL_GetError());
    }
    ImGui::SameLine();
    ImGui::SetNextItemWidth(std::max(120.0f, ImGui::GetContentRegionAvail().x - 120.0f));
    ImGui::InputTextWithHint("##filter", "Rechercher dans le dossier...", &m_filter);
    ImGui::SameLine();
    ImGui::SetNextItemWidth(-FLT_MIN);
    ImGui::SliderFloat("##tile", &m_tileSize, 56.0f, 160.0f, "Taille");

    {  // queued imports (file dialog)
        std::vector<std::string> pending;
        {
            std::lock_guard lock(m_importMutex);
            pending.swap(m_importQueue);
        }
        if (!pending.empty()) importFiles(s, pending);
    }

    // ---- folder tree | content grid
    if (ImGui::BeginTable("##layout", 2, ImGuiTableFlags_Resizable | ImGuiTableFlags_BordersInnerV)) {
        ImGui::TableSetupColumn("tree", ImGuiTableColumnFlags_WidthFixed, 190.0f);
        ImGui::TableSetupColumn("grid", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableNextColumn();
        ImGui::BeginChild("##tree");
        ImGuiTreeNodeFlags rootFlags = ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_SpanAvailWidth;
        if (s.assetFolder.empty()) rootFlags |= ImGuiTreeNodeFlags_Selected;
        if (ImGui::TreeNodeEx("assets", rootFlags)) {
            if (ImGui::IsItemClicked() && !ImGui::IsItemToggledOpen()) s.assetFolder.clear();
            drawFolderTree(s, "");
            ImGui::TreePop();
        }
        ImGui::Spacing();
        ImGui::SeparatorText("Scènes");
        for (const std::string& scene : s.ctx.project().listScenes()) {
            const bool current = scene == s.ctx.scenePath();
            if (ImGui::Selectable(fs::path(scene).stem().string().c_str(), current, ImGuiSelectableFlags_AllowDoubleClick) &&
                ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left) && !current) {
                s.runGuarded([&s, scene] {
                    if (Status st = s.ctx.loadScene(scene); !st) s.notify("Ouverture impossible : " + st.error().message);
                });
            }
            if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort)) ImGui::SetTooltip("%s (double-clic pour ouvrir)", scene.c_str());
        }
        ImGui::EndChild();

        ImGui::TableNextColumn();
        ImGui::BeginChild("##grid");
        ImGui::TextDisabled("assets/%s", s.assetFolder.c_str());
        if (!s.assetFolder.empty()) {
            ImGui::SameLine();
            if (ImGui::SmallButton("Dossier parent")) {
                const auto slash = s.assetFolder.find_last_of('/');
                s.assetFolder = slash == std::string::npos ? "" : s.assetFolder.substr(0, slash);
            }
        }
        auto list = s.engine.vfs().list("assets://" + s.assetFolder);
        const float tile = m_tileSize;
        const int columns = std::max(1, static_cast<int>(ImGui::GetContentRegionAvail().x / (tile + 12.0f)));
        if (list && ImGui::BeginTable("##items", columns)) {
            for (const std::string& entry : list.value()) {
                const bool folder = !entry.empty() && entry.back() == '/';
                const std::string name = folder ? entry.substr(0, entry.size() - 1) : entry;
                if (!m_filter.empty() && lower(name).find(lower(m_filter)) == std::string::npos) continue;
                const std::string rel = join(s.assetFolder, name);
                const std::string uri = "assets://" + rel;
                ImGui::TableNextColumn();
                ImGui::PushID(rel.c_str());
                const ImVec2 p = ImGui::GetCursorScreenPos();
                ImGui::InvisibleButton("##tile", {tile, tile});
                const bool hovered = ImGui::IsItemHovered();
                ImDrawList* dl = ImGui::GetWindowDrawList();
                dl->AddRectFilled(p, {p.x + tile, p.y + tile}, hovered ? palette::kGraphite700 : palette::kGraphite800, 4.0f);
                const SpriteSheet* sheet = !folder && isSheetFile(name) && s.sheets() ? s.sheets()->get(uri) : nullptr;
                if (!folder && isImageFile(name)) {
                    const float pad = 6.0f;
                    dl->AddImage(ImTextureRef(s.thumbnail(uri)), {p.x + pad, p.y + pad}, {p.x + tile - pad, p.y + tile - pad});
                } else if (sheet && !sheet->frames.empty()) {
                    // Sprite sheet: its first frame, with a frame counter.
                    const float pad = 10.0f;
                    const math::Vec4 uv = sheet->uv(0);
                    dl->AddImage(ImTextureRef(s.thumbnail(sheet->texture)), {p.x + pad, p.y + pad}, {p.x + tile - pad, p.y + tile - pad},
                                 {uv.x, uv.y}, {uv.z, uv.w});
                    char count[32];
                    std::snprintf(count, sizeof count, "%zu", sheet->frames.size());
                    dl->AddText({p.x + 5, p.y + tile - ImGui::GetFontSize() - 3}, palette::kAmber, count);
                } else {
                    icons::draw(dl, iconFor(name, folder), {p.x + tile * 0.5f, p.y + tile * 0.5f}, tile * 0.45f,
                                folder ? palette::kAmber : palette::kTextMuted);
                }
                if (folder && hovered && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) s.assetFolder = rel;
                const bool isMaterial = !folder && fs::path(name).extension() == kMaterialExtension;
                if (isMaterial && ImGui::IsItemClicked(ImGuiMouseButton_Left)) {  // edit it in the inspector
                    s.inspectedAsset = uri;
                    s.ctx.selection().clear();
                }
                if (isMaterial && s.inspectedAsset == uri) dl->AddRect(p, {p.x + tile, p.y + tile}, palette::kSelect, 4.0f, 2.0f);
                if (!folder && ImGui::BeginDragDropSource()) {
                    ImGui::SetDragDropPayload(kPayloadAsset, uri.c_str(), uri.size() + 1);
                    ImGui::Text("%s", name.c_str());
                    ImGui::EndDragDropSource();
                }
                if (hovered) {
                    if (sheet) ImGui::SetTooltip("%s\nFeuille de sprites : %zu frames, %zu clips\nGlisser dans la scène pour créer un sprite", uri.c_str(),
                                                 sheet->frames.size(), sheet->clips.size());
                    else if (isModelFile(name)) ImGui::SetTooltip("%s\nModèle 3D : glisser dans la scène pour l'ajouter", uri.c_str());
                    else if (fs::path(name).extension() == ".lua") ImGui::SetTooltip("%s\nScript : glisser sur une entité de la vue pour l'attacher", uri.c_str());
                    else if (isMaterial) ImGui::SetTooltip("%s\nMatériau : cliquer pour le modifier, glisser sur un MeshRenderer", uri.c_str());
                    else ImGui::SetTooltip("%s", uri.c_str());
                }
                if (ImGui::BeginPopupContextItem("##ctx")) {
                    if (ImGui::MenuItem("Copier le chemin")) ImGui::SetClipboardText(uri.c_str());
                    if (!folder && isImageFile(name) && ImGui::MenuItem("Découper en grille (feuille de sprites)...")) m_sliceTarget = rel;
                    if (ImGui::MenuItem("Renommer...")) {
                        m_renameTarget = rel;
                        m_renameBuffer = name;
                    }
                    ImGui::Separator();
                    if (ImGui::MenuItem("Supprimer...")) m_pendingDelete = rel;
                    ImGui::EndPopup();
                }
                // Name under the tile (clipped).
                ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + tile);
                ImGui::TextUnformatted(name.c_str());
                ImGui::PopTextWrapPos();
                ImGui::PopID();
            }
            ImGui::EndTable();
        }
        if (list && list.value().empty()) ImGui::TextDisabled("Dossier vide — importez des images ou glissez des fichiers sur la fenêtre.");
        ImGui::EndChild();
        ImGui::EndTable();
    }

    // ---- confirmation dialogs (no asset is ever deleted without explicit consent)
    if (!m_pendingDelete.empty()) ImGui::OpenPopup("Supprimer l'asset ?");
    if (ImGui::BeginPopupModal("Supprimer l'asset ?", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("Supprimer définitivement « assets/%s » du disque ?", m_pendingDelete.c_str());
        ImGui::TextColored(palette::toVec4(palette::kWarning), "Les scènes qui l'utilisent afficheront une texture manquante.");
        if (ImGui::Button("Supprimer", {120, 0})) {
            fs::remove_all(absoluteAsset(s, m_pendingDelete), ec);
            s.notify(ec ? "Suppression impossible : " + ec.message() : "Supprimé : assets/" + m_pendingDelete);
            m_pendingDelete.clear();
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Annuler", {120, 0}) || ImGui::IsKeyPressed(ImGuiKey_Escape)) {
            m_pendingDelete.clear();
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
    if (!m_renameTarget.empty()) ImGui::OpenPopup("Renommer l'asset");
    if (ImGui::BeginPopupModal("Renommer l'asset", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        if (ImGui::IsWindowAppearing()) ImGui::SetKeyboardFocusHere();
        const bool enter = ImGui::InputText("##name", &m_renameBuffer, ImGuiInputTextFlags_EnterReturnsTrue);
        ImGui::TextDisabled("Les références dans les scènes ne sont pas mises à jour automatiquement.");
        if ((ImGui::Button("Renommer", {120, 0}) || enter) && !m_renameBuffer.empty() &&
            m_renameBuffer.find_first_of("/\\") == std::string::npos) {
            const fs::path from = absoluteAsset(s, m_renameTarget);
            fs::rename(from, from.parent_path() / m_renameBuffer, ec);
            if (ec) s.notify("Renommage impossible : " + ec.message());
            m_renameTarget.clear();
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Annuler", {120, 0}) || ImGui::IsKeyPressed(ImGuiKey_Escape)) {
            m_renameTarget.clear();
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
    // ---- sprite sheet tools
    if (!m_sliceTarget.empty()) ImGui::OpenPopup("Découper en grille");
    if (ImGui::BeginPopupModal("Découper en grille", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("Image : assets/%s", m_sliceTarget.c_str());
        ImGui::SetNextItemWidth(200);
        ImGui::InputInt("Largeur de cellule (px)", &m_sliceW);
        ImGui::SetNextItemWidth(200);
        ImGui::InputInt("Hauteur de cellule (px)", &m_sliceH);
        ImGui::SetNextItemWidth(200);
        ImGui::InputInt("Marge (px)", &m_sliceMargin);
        ImGui::SetNextItemWidth(200);
        ImGui::InputInt("Espacement (px)", &m_sliceSpacing);
        ImGui::Checkbox("Pixel art (filtrage net)", &m_slicePixelArt);
        m_sliceW = std::clamp(m_sliceW, 1, 8192);
        m_sliceH = std::clamp(m_sliceH, 1, 8192);
        m_sliceMargin = std::clamp(m_sliceMargin, 0, 4096);
        m_sliceSpacing = std::clamp(m_sliceSpacing, 0, 4096);
        const fs::path imagePath(m_sliceTarget);
        const std::string folderRel = imagePath.parent_path().generic_string();
        const std::string outStem = uniqueFile(absoluteAsset(s, folderRel), imagePath.stem().string(), {".agrsheet"});
        const std::string outRel = join(folderRel, outStem + ".agrsheet");
        ImGui::TextDisabled("Crée assets/%s (une frame par cellule, 1 cellule = 1 unité).", outRel.c_str());
        if (ImGui::Button("Créer", {120, 0})) {
            auto sheet = buildGridSheetFile(s.engine.vfs(), "assets://" + m_sliceTarget, static_cast<u32>(m_sliceW), static_cast<u32>(m_sliceH),
                                            "assets://" + outRel, static_cast<u32>(m_sliceMargin), static_cast<u32>(m_sliceSpacing), m_slicePixelArt);
            if (sheet) s.notify("Feuille créée : assets/" + outRel + " (" + std::to_string(sheet.value().frames.size()) + " frames)");
            else s.notify("Découpe impossible : " + sheet.error().message);
            m_sliceTarget.clear();
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Annuler", {120, 0}) || ImGui::IsKeyPressed(ImGuiKey_Escape)) {
            m_sliceTarget.clear();
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
    if (m_atlasOpen) ImGui::OpenPopup("Créer un atlas");
    if (ImGui::BeginPopupModal("Créer un atlas", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        std::vector<std::string> images;
        if (auto list = s.engine.vfs().list("assets://" + s.assetFolder))
            for (const std::string& entry : list.value())
                if (!entry.empty() && entry.back() != '/' && isImageFile(entry)) images.push_back("assets://" + join(s.assetFolder, entry));
        ImGui::Text("%zu image(s) dans assets/%s", images.size(), s.assetFolder.c_str());
        ImGui::SetNextItemWidth(200);
        ImGui::InputText("Nom", &m_atlasName);
        ImGui::SetNextItemWidth(200);
        ImGui::DragFloat("Pixels par unité", &m_atlasPpu, 1.0f, 1.0f, 4096.0f, "%.0f");
        ImGui::Checkbox("Pixel art (filtrage net)", &m_atlasPixelArt);
        const bool validName = !m_atlasName.empty() && m_atlasName.find_first_of("/\\.") == std::string::npos;
        const std::string stem = validName ? uniqueFile(absoluteAsset(s, s.assetFolder), m_atlasName, {".png", ".agrsheet"}) : std::string();
        if (validName) ImGui::TextDisabled("Crée assets/%s.png et .agrsheet (frames nommées d'après les fichiers).", join(s.assetFolder, stem).c_str());
        else ImGui::TextColored(palette::toVec4(palette::kWarning), "Nom invalide.");
        ImGui::BeginDisabled(images.size() < 1 || !validName);
        if (ImGui::Button("Créer", {120, 0})) {
            const std::string base = "assets://" + join(s.assetFolder, stem);
            auto sheet = buildAtlasFiles(s.engine.vfs(), images, base + ".png", base + ".agrsheet", m_atlasPpu, m_atlasPixelArt);
            if (sheet)
                s.notify("Atlas créé : " + base + ".agrsheet (" + std::to_string(sheet.value().textureWidth) + "x" +
                         std::to_string(sheet.value().textureHeight) + ", " + std::to_string(sheet.value().frames.size()) + " frames)");
            else s.notify("Atlas impossible : " + sheet.error().message);
            m_atlasOpen = false;
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndDisabled();
        ImGui::SameLine();
        if (ImGui::Button("Annuler", {120, 0}) || ImGui::IsKeyPressed(ImGuiKey_Escape)) {
            m_atlasOpen = false;
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
    ImGui::End();
}

} // namespace agr::editor
