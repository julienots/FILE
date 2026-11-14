// LOST KINGDOM — mods (1.7.0). A mod is a folder in <app storage>/mods/<id>/ (imported from a .zip on Android):
//   mod.txt     name / author / version / description ("key = value" lines)
//   data.txt    content patches, one per line (see applyModDataLine in Data.cpp and docs/MODDING.md)
//   assets/...  files that replace the packaged assets with the same path (models, textures, music, sounds, UI)
// Mods are data only (no code). The enabled list is <app storage>/mods_enabled.txt; changes apply at next launch.
#include "Game.h"
#include "../engine/core/Platform.h"
#include <sstream>

namespace lk {

static std::string modsDir() { return platform::writableDir() + "/mods"; }

static std::string trimS(const std::string& s) {
    size_t a = s.find_first_not_of(" \t\r\n"), b = s.find_last_not_of(" \t\r\n");
    return a == std::string::npos ? std::string() : s.substr(a, b - a + 1);
}

void Game::scanMods() {
    std::set<std::string> enabled;
    std::string list;
    if (platform::readFile(platform::writableDir() + "/mods_enabled.txt", list)) {
        std::istringstream in(list); std::string l;
        while (std::getline(in, l)) { l = trimS(l); if (!l.empty()) enabled.insert(l); }
    }
    std::vector<ModInfo> old = mods;
    mods.clear();
    for (const std::string& id : platform::listDirectories(modsDir())) {
        ModInfo m; m.id = id; m.name = id;
        std::string txt;
        if (platform::readFile(modsDir() + "/" + id + "/mod.txt", txt)) {
            std::istringstream in(txt); std::string l;
            while (std::getline(in, l)) {
                size_t eq = l.find('=');
                if (eq == std::string::npos) continue;
                std::string k = trimS(l.substr(0, eq)), v = trimS(l.substr(eq + 1));
                if (k == "name") m.name = v; else if (k == "author") m.author = v; else if (k == "version") m.version = v; else if (k == "description") m.desc = v;
            }
        }
        // every installed mod is active unless the player turned it off ("!id" in mods_enabled.txt)
        m.enabled = enabled.count("!" + id) == 0;
        for (const auto& o : old) if (o.id == id) { m.applied = o.applied; m.patches = o.patches; m.errors = o.errors; m.firstError = o.firstError; }
        mods.push_back(m);
    }
}

void Game::saveModList() {
    std::string out;
    for (const auto& m : mods) out += (m.enabled ? "" : "!") + m.id + "\n";
    platform::writeFileAtomic(platform::writableDir() + "/mods_enabled.txt", out);
}

void Game::applyMods(bool force) {
    static bool done = false;   // content patches are applied once per launch (before anything uses the data)
    if (done && !force) return;
    done = true;
    platform::makeDirectory(modsDir());
    scanMods();
    std::vector<std::string> roots;
    for (auto& m : mods) {
        if (!m.enabled) continue;
        std::string root = modsDir() + "/" + m.id;
        roots.push_back(root);
        m.patches = m.errors = 0; m.firstError.clear();
        std::string data;
        if (platform::readFile(root + "/data.txt", data)) {
            std::istringstream in(data); std::string l; int ln = 0;
            while (std::getline(in, l)) {
                ln++;
                std::string err;
                std::string t = trimS(l);
                if (t.empty() || t[0] == '#') continue;
                if (applyModDataLine(l, err)) m.patches++;
                else { m.errors++; if (m.firstError.empty()) m.firstError = "ligne " + std::to_string(ln) + " : " + err; }
            }
        }
        m.applied = true;
        AGR_LOG("mod '%s' (%s): %d patches, %d errors%s%s", m.id.c_str(), m.name.c_str(), m.patches, m.errors, m.firstError.empty() ? "" : " - ", m.firstError.c_str());
    }
    platform::setAssetOverrideRoots(roots);
    // forget models that were looked up and not found: a mod may provide them now
    for (auto it = importedModels.begin(); it != importedModels.end();) { if (!it->second) it = importedModels.erase(it); else ++it; }
}

// Android: the Java side writes mods/.imported after unpacking a picked zip
void Game::pollModImport() {
    std::string marker = modsDir() + "/.imported", status;
    if (!platform::readFile(marker, status)) return;
    platform::removeFile(marker);
    scanMods();
    status = trimS(status);
    if (status.rfind("ok ", 0) == 0) notify("Mod importé : " + status.substr(3) + " (redémarrez le jeu pour l'appliquer)", vec3(0.7f, 1.f, 0.7f), "i_settings");
    else if (status.rfind("refused", 0) == 0) notify("Mod refusé : le zip contient des chemins interdits", vec3(1.f, 0.6f, 0.5f), "i_settings");
    else notify("Import du mod impossible (zip illisible ou trop gros)", vec3(1.f, 0.6f, 0.5f), "i_settings");
}

}  // namespace lk
