// Plugins and packages window: the engine's plugins (built-in and dynamic: unload / reload),
// the project's plugin folder (load now, load when the project opens), installed packages
// (uninstall with a preview that keeps modified files), the local repository (install with a
// preview of new / replaced / identical files) and package creation from project assets.
#include <chrono>
#include <future>
#include <algorithm>
#include <filesystem>

#include <SDL3/SDL.h>
#include <imgui.h>
#include <misc/cpp/imgui_stdlib.h>

#include "agr/editor/Package.hpp"
#include "agr/editor/ui/Panels.hpp"
#include "agr/editor/ui/Theme.hpp"

namespace agr::editor {

namespace fs = std::filesystem;

namespace {

void listBlock(const char* title, const std::vector<std::string>& files, ImU32 color) {
    if (files.empty()) return;
    ImGui::TextColored(palette::toVec4(color), "%s (%zu)", title, files.size());
    ImGui::Indent();
    for (usize i = 0; i < files.size() && i < 12; ++i) ImGui::TextUnformatted(files[i].c_str());
    if (files.size() > 12) ImGui::TextDisabled("... et %zu autre(s)", files.size() - 12);
    ImGui::Unindent();
}

bool isPluginLibrary(const fs::path& p) {
    const std::string n = p.filename().string();
    const std::string ext = p.extension().string();
    return n.find("agr_plugin") != std::string::npos && (ext == ".so" || ext == ".dll" || ext == ".dylib");
}

} // namespace

fs::path PackagesPanel::repository(EditorShared& s) const {
    const ProjectSettings& st = s.ctx.project().settings();
    return st.packageRepository.empty() ? s.ctx.project().root() / "packages" : fs::path(st.packageRepository);
}

void PackagesPanel::loadProjectPlugins(EditorShared& s) {
    if (!s.ctx.hasProject()) return;
    PluginManager& pm = s.engine.plugins();
    const fs::path dir = s.ctx.project().root() / "plugins";
    bool added = false;
    for (const std::string& name : s.ctx.project().settings().plugins) {
        const fs::path lib = dir / name;
        std::error_code ec;
        if (!fs::exists(lib, ec)) {
            s.notify("Plugin du projet introuvable : " + lib.string());
            continue;
        }
        if (Status st = pm.addDynamic(lib.string()); !st) {
            if (st.error().code != Error::Code::AlreadyExists) s.notify("Plugin « " + name + " » : " + st.error().message);
            continue;
        }
        added = true;
    }
    if (added) (void)pm.loadAll(s.engine);
}

void PackagesPanel::drawPlugins(EditorShared& s) {
    PluginManager& pm = s.engine.plugins();
    ImGui::TextWrapped("Plugins chargés par le moteur. Les plugins dynamiques (bibliothèques « agr_plugin ») se déchargent et se "
                       "rechargent sans redémarrer l'éditeur : recompilez-les puis « Recharger ».");
    if (ImGui::BeginTable("##plugins", 5, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingStretchProp)) {
        ImGui::TableSetupColumn("Plugin");
        ImGui::TableSetupColumn("Version");
        ImGui::TableSetupColumn("Type");
        ImGui::TableSetupColumn("État");
        ImGui::TableSetupColumn("Actions");
        ImGui::TableHeadersRow();
        std::string toUnload, toReload, reloadPath;
        for (const PluginManager::Entry& e : pm.entries()) {
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::TextUnformatted(e.info.name.c_str());
            if (ImGui::IsItemHovered()) {
                std::string deps;
                for (const std::string& d : e.info.dependencies) deps += (deps.empty() ? "" : ", ") + d;
                ImGui::SetTooltip("%s\nAuteur : %s\nDépendances : %s%s%s", e.info.description.c_str(), e.info.author.c_str(), deps.empty() ? "aucune" : deps.c_str(),
                                  e.path.empty() ? "" : "\nFichier : ", e.path.c_str());
            }
            ImGui::TableNextColumn();
            ImGui::TextUnformatted(e.info.version.c_str());
            ImGui::TableNextColumn();
            ImGui::TextUnformatted(e.dynamic ? "Dynamique" : "Intégré");
            ImGui::TableNextColumn();
            if (e.loaded) ImGui::TextColored(palette::toVec4(palette::kSuccess), "Chargé");
            else ImGui::TextColored(palette::toVec4(palette::kError), "%s", e.error.empty() ? "Non chargé" : e.error.c_str());
            ImGui::TableNextColumn();
            if (e.dynamic) {
                ImGui::PushID(e.info.name.c_str());
                if (ImGui::SmallButton("Décharger")) toUnload = e.info.name;
                ImGui::SameLine();
                if (ImGui::SmallButton("Recharger")) {
                    toReload = e.info.name;
                    reloadPath = e.path;
                }
                ImGui::PopID();
            } else {
                ImGui::TextDisabled("—");
            }
        }
        ImGui::EndTable();
        // Applied after the table: the list changes.
        if (!toUnload.empty() || !toReload.empty()) {
            if (s.ctx.playState() != PlayState::Edit) {
                s.notify("Arrêtez le jeu avant de décharger un plugin");
            } else {
                const std::string name = toUnload.empty() ? toReload : toUnload;
                if (Status st = pm.unloadDynamic(s.engine, name); !st) {
                    s.notify("Décharger « " + name + " » : " + st.error().message);
                } else if (!toReload.empty()) {
                    if (Status a = pm.addDynamic(reloadPath); !a) s.notify("Recharger : " + a.error().message);
                    else (void)pm.loadAll(s.engine);
                    s.notify(pm.isLoaded(name) ? "Plugin rechargé : " + name : "Le plugin « " + name + " » n'a pas pu se recharger (voir la console)");
                } else {
                    s.notify("Plugin déchargé : " + name);
                }
            }
        }
    }
    if (!s.ctx.hasProject()) return;
    ImGui::SeparatorText("Plugins du projet");
    const fs::path dir = s.ctx.project().root() / "plugins";
    ImGui::TextWrapped("Dossier : %s", dir.string().c_str());
    ImGui::BeginDisabled(!kDesktopFileAccess);
    if (ImGui::SmallButton("Ouvrir le dossier")) {
        std::error_code ec;
        fs::create_directories(dir, ec);
        if (!SDL_OpenURL(("file://" + fs::absolute(dir, ec).generic_string()).c_str())) s.notify(std::string("Impossible d'ouvrir le dossier : ") + SDL_GetError());
    }
    ImGui::EndDisabled();
    if (!kDesktopFileAccess && ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
        ImGui::SetTooltip("Les plugins natifs ne se chargent pas dans l'éditeur Android (bibliothèques signées dans l'APK)");
    std::error_code ec;
    bool any = false;
    for (const auto& entry : fs::directory_iterator(dir, ec)) {
        if (!entry.is_regular_file() || !isPluginLibrary(entry.path())) continue;
        any = true;
        const std::string file = entry.path().filename().string();
        ImGui::PushID(file.c_str());
        auto& list = s.ctx.project().settings().plugins;
        bool autoLoad = std::find(list.begin(), list.end(), file) != list.end();
        if (ImGui::Checkbox("##auto", &autoLoad)) {
            if (autoLoad) list.push_back(file);
            else list.erase(std::remove(list.begin(), list.end(), file), list.end());
            (void)s.ctx.project().save();
        }
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Charger à l'ouverture du projet");
        ImGui::SameLine();
        ImGui::TextUnformatted(file.c_str());
        ImGui::SameLine();
        if (ImGui::SmallButton("Charger maintenant")) {
            if (Status st = s.engine.plugins().addDynamic(entry.path().string()); !st) s.notify("Chargement : " + st.error().message);
            else {
                (void)s.engine.plugins().loadAll(s.engine);
                s.notify("Plugin chargé : " + file);
            }
        }
        ImGui::PopID();
    }
    if (!any) ImGui::TextDisabled("Aucune bibliothèque « *agr_plugin* » dans ce dossier (voir Aide › Guides › Plugins).");
}

void PackagesPanel::drawInstalled(EditorShared& s) {
    const fs::path assets = s.ctx.project().assetsDirectory();
    const auto installed = installedPackages(assets);
    if (installed.empty()) ImGui::TextDisabled("Aucun paquet installé dans ce projet.");
    for (const InstalledPackage& p : installed) {
        ImGui::PushID(p.name.c_str());
        ImGui::Text("%s  %s", p.name.c_str(), p.version.c_str());
        ImGui::SameLine();
        ImGui::TextDisabled("(%zu fichier(s), depuis %s)", p.files.size(), p.source.c_str());
        ImGui::SameLine();
        if (ImGui::SmallButton("Désinstaller...")) {
            m_uninstall = p.name;
            m_removeModified = false;
            auto plan = planUninstall(p.name, assets);
            m_uninstallPlan = plan ? plan.value() : UninstallPlan{};
            ImGui::OpenPopup("Désinstaller un paquet");
        }
        ImGui::PopID();
    }
    ImGui::SetNextWindowSize({560, 0}, ImGuiCond_Appearing);
    if (ImGui::BeginPopupModal("Désinstaller un paquet", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("Désinstaller « %s » ?", m_uninstall.c_str());
        listBlock("Fichiers supprimés", m_uninstallPlan.remove, palette::kWarning);
        listBlock("Fichiers modifiés depuis l'installation (gardés)", m_uninstallPlan.modified, palette::kSuccess);
        listBlock("Fichiers déjà absents", m_uninstallPlan.missing, palette::kTextMuted);
        if (!m_uninstallPlan.neededBy.empty()) {
            listBlock("Paquets qui en dépendent : désinstallez-les d'abord", m_uninstallPlan.neededBy, palette::kError);
        }
        if (!m_uninstallPlan.modified.empty()) ImGui::Checkbox("Supprimer aussi les fichiers modifiés", &m_removeModified);
        ImGui::BeginDisabled(!m_uninstallPlan.neededBy.empty());
        if (ImGui::Button("Désinstaller")) {
            if (Status st = uninstallPackage(m_uninstall, s.ctx.project().assetsDirectory(), m_removeModified); st) s.notify("Paquet désinstallé : " + m_uninstall);
            else s.notify("Désinstallation : " + st.error().message);
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndDisabled();
        ImGui::SameLine();
        if (ImGui::Button("Annuler")) ImGui::CloseCurrentPopup();
        ImGui::EndPopup();
    }
}

void PackagesPanel::drawRepository(EditorShared& s) {
    ProjectSettings& st = s.ctx.project().settings();
    ImGui::TextWrapped("Dépôt local : un dossier de fichiers .agrpkg (partagé entre projets, sur une clé USB, un dossier synchronisé...). "
                       "Aucun service en ligne n'est utilisé.");
    std::string repo = st.packageRepository;
    ImGui::SetNextItemWidth(-90);
    if (ImGui::InputTextWithHint("##repo", (s.ctx.project().root() / "packages").string().c_str(), &repo)) {
        st.packageRepository = repo;
        (void)s.ctx.project().save();
    }
    ImGui::SameLine();
    if (ImGui::Button("Actualiser")) m_repoScanned = false;
    if (!m_repoScanned) {
        m_repo = listRepository(repository(s));
        m_repoScanned = true;
    }
    if (m_repo.empty()) ImGui::TextDisabled("Aucun paquet dans %s", repository(s).string().c_str());
    for (const RepositoryEntry& e : m_repo) {
        ImGui::PushID(e.path.string().c_str());
        ImGui::Text("%s  %s", e.manifest.name.c_str(), e.manifest.version.c_str());
        ImGui::SameLine();
        ImGui::TextDisabled("%s — %zu fichier(s)", e.manifest.author.c_str(), e.manifest.files.size());
        if (!e.manifest.description.empty()) {
            ImGui::Indent();
            ImGui::TextWrapped("%s", e.manifest.description.c_str());
            ImGui::Unindent();
        }
        if (ImGui::SmallButton("Installer...")) {
            m_installPath = e.path;
            m_replaceChanged = false;
            auto plan = planInstall(e.path, s.ctx.project().assetsDirectory());
            m_installError = plan ? std::string() : plan.error().message;
            m_installPlan = plan ? plan.value() : InstallPlan{};
            ImGui::OpenPopup("Installer un paquet");
        }
        ImGui::Separator();
        ImGui::PopID();
    }
    ImGui::SetNextWindowSize({580, 0}, ImGuiCond_Appearing);
    if (ImGui::BeginPopupModal("Installer un paquet", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        const InstallPlan& p = m_installPlan;
        if (!m_installError.empty()) {
            ImGui::TextColored(palette::toVec4(palette::kError), "%s", m_installError.c_str());
        } else {
            ImGui::Text("« %s » %s", p.manifest.name.c_str(), p.manifest.version.c_str());
            if (!p.installedVersion.empty()) ImGui::TextColored(palette::toVec4(palette::kAmber), "Mise à jour : la version %s est installée", p.installedVersion.c_str());
            listBlock("Nouveaux fichiers", p.newFiles, palette::kSuccess);
            listBlock("Fichiers existants DIFFÉRENTS (seraient remplacés)", p.changedFiles, palette::kWarning);
            listBlock("Fichiers identiques", p.identicalFiles, palette::kTextMuted);
            listBlock("Dépendances manquantes : installez-les d'abord", p.missingDependencies, palette::kError);
            if (!p.changedFiles.empty()) ImGui::Checkbox("Remplacer les fichiers différents", &m_replaceChanged);
        }
        const bool can = m_installError.empty() && p.missingDependencies.empty() && (p.changedFiles.empty() || m_replaceChanged);
        ImGui::BeginDisabled(!can);
        if (ImGui::Button("Installer")) {
            if (Status done = installPackage(m_installPath, s.ctx.project().assetsDirectory(), m_replaceChanged); done)
                s.notify("Paquet installé : " + p.manifest.name + " (" + std::to_string(p.manifest.files.size()) + " fichier(s) dans les assets)");
            else s.notify("Installation : " + done.error().message);
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndDisabled();
        ImGui::SameLine();
        if (ImGui::Button("Annuler")) ImGui::CloseCurrentPopup();
        ImGui::EndPopup();
    }
}

void PackagesPanel::drawCreate(EditorShared& s) {
    const fs::path assets = s.ctx.project().assetsDirectory();
    ImGui::InputText("Nom", &m_new.name);
    ImGui::SameLine();
    ImGui::TextDisabled("(minuscules, chiffres, - _ .)");
    ImGui::InputText("Version", &m_new.version);
    ImGui::InputText("Auteur", &m_new.author);
    ImGui::InputTextMultiline("Description", &m_new.description, {0, 60});
    ImGui::InputTextWithHint("Dépendances", "nom1, nom2", &m_deps);
    ImGui::SeparatorText("Contenu (dossiers et fichiers des assets)");
    std::error_code ec;
    std::vector<fs::directory_entry> entries;
    for (const auto& e : fs::directory_iterator(assets, ec)) entries.push_back(e);
    std::sort(entries.begin(), entries.end(), [](const auto& a, const auto& b) { return a.path().filename() < b.path().filename(); });
    ImGui::BeginChild("##content", {0, 180}, ImGuiChildFlags_Borders);
    for (const auto& e : entries) {
        const std::string rel = e.path().filename().generic_string();
        if (rel == "packages.json") continue;
        bool on = m_selection.count(rel) != 0;
        if (ImGui::Checkbox((rel + (e.is_directory() ? "/" : "")).c_str(), &on)) {
            if (on) m_selection.insert(rel);
            else m_selection.erase(rel);
        }
    }
    ImGui::EndChild();
    ImGui::TextWrapped("Le paquet sera écrit dans le dépôt : %s", repository(s).string().c_str());
    const bool valid = validatePackageName(m_new.name).ok() && !m_selection.empty();
    ImGui::BeginDisabled(!valid);
    if (ImGui::Button("Créer le paquet")) {
        PackageManifest m = m_new;
        m.dependencies.clear();
        std::string d;
        for (char c : m_deps + ",") {
            if (c == ',') {
                const auto a = d.find_first_not_of(' '), b = d.find_last_not_of(' ');
                if (a != std::string::npos) m.dependencies.push_back(d.substr(a, b - a + 1));
                d.clear();
            } else {
                d += c;
            }
        }
        m.engineVersion = "0.1.0";
        const fs::path out = repository(s) / (m.name + "-" + m.version + ".agrpkg");
        auto written = writePackage(out, m, assets, std::vector<std::string>(m_selection.begin(), m_selection.end()));
        if (written) {
            s.notify("Paquet créé : " + out.string() + " (" + std::to_string(written.value().files.size()) + " fichier(s))");
            m_repoScanned = false;
        } else {
            s.notify("Création du paquet : " + written.error().message);
        }
    }
    ImGui::EndDisabled();
    if (!valid && ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) ImGui::SetTooltip("Un nom valide et au moins un élément à inclure");
}

void PackagesPanel::drawOnline(EditorShared& s) {
    using namespace std::chrono_literals;
    ProjectSettings& ps = s.ctx.project().settings();
    if (m_onlineUrl.empty()) m_onlineUrl = ps.onlineRepository;
    ImGui::TextWrapped("Un dépôt en ligne = un index.json et des fichiers .agrpkg sur n'importe quel site statique "
                       "(GitHub Pages, serveur web, dossier file://). Les téléchargements sont vérifiés (taille, SHA-256) ; "
                       "les dépendances s'installent d'abord.");
    const bool fetchingNow = m_indexJob.valid();
    ImGui::BeginDisabled(fetchingNow || s.resourcesDir.empty());
    if (ImGui::Button("Bibliothèque officielle")) {
        m_onlineUrl = "file://" + std::filesystem::absolute(s.resourcesDir + "/packages").generic_string() + "/";
        m_onlineError.clear();
        m_indexJob = std::async(std::launch::async, [url = m_onlineUrl] { return fetchRepositoryIndex(url); });
    }
    ImGui::EndDisabled();
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
        ImGui::SetTooltip("Paquets livrés avec l'éditeur : contrôleurs FPS et plateforme, caméra, cycle jour / nuit,\n"
                          "dialogues, inventaire, machine à états, points de vie, shaders en graphe");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(-140);
    ImGui::InputTextWithHint("##url", "https://exemple.github.io/paquets/", &m_onlineUrl);
    ImGui::SameLine();
    const bool fetching = m_indexJob.valid();
    ImGui::BeginDisabled(fetching || m_onlineUrl.empty());
    if (ImGui::Button(fetching ? "Chargement..." : "Charger l'index")) {
        ps.onlineRepository = m_onlineUrl;
        (void)s.ctx.project().save();
        m_onlineError.clear();
        m_indexJob = std::async(std::launch::async, [url = m_onlineUrl] { return fetchRepositoryIndex(url); });
    }
    ImGui::EndDisabled();
    if (m_indexJob.valid() && m_indexJob.wait_for(0s) == std::future_status::ready) {
        auto r = m_indexJob.get();
        if (r) m_online = std::move(r.value());
        else m_onlineError = r.error().message, m_online.reset();
    }
    if (!m_onlineError.empty()) ImGui::TextColored(palette::toVec4(palette::kError), "%s", m_onlineError.c_str());
    if (!m_online) return;
    if (!m_online->title.empty()) ImGui::SeparatorText(m_online->title.c_str());
    ImGui::SetNextItemWidth(240);
    ImGui::InputTextWithHint("##filter", "Rechercher", &m_onlineFilter);
    std::set<std::string> installed;
    for (const InstalledPackage& p : installedPackages(s.ctx.project().assetsDirectory())) installed.insert(p.name + "@" + p.version);
    const bool busy = m_onlineInstall.valid();
    if (busy && m_onlineInstall.wait_for(0s) == std::future_status::ready) {
        auto r = m_onlineInstall.get();
        m_onlineResult = r ? "Installé : " + std::to_string(r.value().size()) + " paquet(s)" : "Échec : " + r.error().message;
        s.notify(m_onlineResult);
    }
    if (!m_onlineResult.empty()) ImGui::TextWrapped("%s", m_onlineResult.c_str());
    std::string lastName;
    if (ImGui::BeginChild("##online", {0, 0}, ImGuiChildFlags_Borders)) {
        for (const OnlinePackage& p : m_online->packages) {
            if (p.name == lastName) continue;  // older versions
            lastName = p.name;
            const std::string hay = p.name + " " + p.description + " " + p.author;
            std::string f = m_onlineFilter;
            if (!f.empty()) {
                auto low = [](std::string t) {
                    for (char& c : t) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
                    return t;
                };
                if (low(hay).find(low(f)) == std::string::npos) continue;
            }
            ImGui::PushID(p.name.c_str());
            ImGui::Text("%s  %s", p.name.c_str(), p.version.c_str());
            ImGui::SameLine();
            ImGui::TextDisabled("%s  %.1f Ko", p.author.c_str(), static_cast<double>(p.size) / 1024.0);
            ImGui::SameLine(ImGui::GetContentRegionAvail().x - 90 + ImGui::GetCursorPosX());
            const bool have = installed.count(p.name + "@" + p.version) > 0;
            ImGui::BeginDisabled(busy || have);
            if (ImGui::SmallButton(have ? "Installé" : "Installer")) {
                m_onlineResult = "Installation de " + p.name + "...";
                m_onlineInstall = std::async(std::launch::async, [idx = *m_online, name = p.name, dl = s.ctx.project().root() / "packages",
                                                                  assets = s.ctx.project().assetsDirectory()] {
                    return installFromRepository(name, idx, dl, assets, false);
                });
            }
            ImGui::EndDisabled();
            if (!p.description.empty()) ImGui::TextWrapped("%s", p.description.c_str());
            if (!p.dependencies.empty()) {
                std::string d;
                for (const std::string& x : p.dependencies) d += (d.empty() ? "" : ", ") + x;
                ImGui::TextDisabled("Dépend de : %s", d.c_str());
            }
            ImGui::Separator();
            ImGui::PopID();
        }
    }
    ImGui::EndChild();
}

void PackagesPanel::draw(EditorShared& s) {
    if (!s.showPackages) return;
    ImGui::SetNextWindowSize({760, 560}, ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("Plugins et paquets", &s.showPackages)) {
        ImGui::End();
        return;
    }
    if (ImGui::SmallButton("Aide")) s.openDocRequest = "PACKAGES.md";
    if (ImGui::BeginTabBar("##pkgTabs")) {
        if (ImGui::BeginTabItem("Plugins")) {
            drawPlugins(s);
            ImGui::EndTabItem();
        }
        if (s.ctx.hasProject()) {
            if (ImGui::BeginTabItem("Paquets installés")) {
                drawInstalled(s);
                ImGui::EndTabItem();
            }
            if (ImGui::BeginTabItem("Installer")) {
                drawRepository(s);
                ImGui::EndTabItem();
            }
            if (ImGui::BeginTabItem("En ligne")) {
                drawOnline(s);
                ImGui::EndTabItem();
            }
            if (ImGui::BeginTabItem("Créer un paquet")) {
                drawCreate(s);
                ImGui::EndTabItem();
            }
        }
        ImGui::EndTabBar();
    }
    ImGui::End();
}

} // namespace agr::editor
