#include <algorithm>
#include <cctype>
#include <filesystem>

#include <SDL3/SDL.h>
#include <imgui.h>
#include <imgui_stdlib.h>

#include "agr/editor/ui/Panels.hpp"

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
    return icons::Kind::File;
}

std::string uniqueName(const fs::path& dir, const std::string& base) {
    std::error_code ec;
    std::string name = base;
    for (int i = 2; fs::exists(dir / name, ec); ++i) name = base + " " + std::to_string(i);
    return name;
}

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
        if (!isImageFile(src.filename().string()))
            AGR_LOG_WARN("Editor", "'{}' copied, but this file type has no importer yet (images only in this version)",
                         src.filename().string());
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
        static const SDL_DialogFileFilter filters[] = {{"Images", "png;jpg;jpeg;tga;bmp;psd;gif;hdr"}, {"Tous les fichiers", "*"}};
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
                if (!folder && isImageFile(name)) {
                    const float pad = 6.0f;
                    dl->AddImage(ImTextureRef(s.thumbnail(uri)), {p.x + pad, p.y + pad}, {p.x + tile - pad, p.y + tile - pad});
                } else {
                    icons::draw(dl, iconFor(name, folder), {p.x + tile * 0.5f, p.y + tile * 0.5f}, tile * 0.45f,
                                folder ? palette::kAmber : palette::kTextMuted);
                }
                if (folder && hovered && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) s.assetFolder = rel;
                if (!folder && ImGui::BeginDragDropSource()) {
                    ImGui::SetDragDropPayload(kPayloadAsset, uri.c_str(), uri.size() + 1);
                    ImGui::Text("%s", name.c_str());
                    ImGui::EndDragDropSource();
                }
                if (hovered) ImGui::SetTooltip("%s", uri.c_str());
                if (ImGui::BeginPopupContextItem("##ctx")) {
                    if (ImGui::MenuItem("Copier le chemin")) ImGui::SetClipboardText(uri.c_str());
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
    ImGui::End();
}

} // namespace agr::editor
