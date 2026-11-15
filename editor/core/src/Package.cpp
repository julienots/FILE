#include "agr/editor/Package.hpp"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <map>
#include <set>

#include <nlohmann/json.hpp>

namespace agr::editor {

namespace fs = std::filesystem;
using json = nlohmann::ordered_json;

namespace {

constexpr char kMagic[8] = {'A', 'G', 'R', 'P', 'K', 'G', '0', '1'};
constexpr u32 kMaxManifest = 16u * 1024u * 1024u;
constexpr const char* kRegistry = "packages.json";

Result<std::string> readFile(const fs::path& p) {
    std::ifstream in(p, std::ios::binary);
    if (!in) return makeError(Error::Code::IOError, "lecture impossible : " + p.string());
    std::string data((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    return data;
}

Status writeFile(const fs::path& p, std::string_view bytes) {
    std::error_code ec;
    fs::create_directories(p.parent_path(), ec);
    std::ofstream out(p, std::ios::binary | std::ios::trunc);
    if (!out) return makeError(Error::Code::IOError, "écriture impossible : " + p.string());
    out.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
    if (!out) return makeError(Error::Code::IOError, "écriture incomplète : " + p.string());
    return {};
}

json manifestToJson(const PackageManifest& m) {
    json j;
    j["format"] = "agr.package";
    j["version"] = 1;
    j["name"] = m.name;
    j["packageVersion"] = m.version;
    j["author"] = m.author;
    j["description"] = m.description;
    j["dependencies"] = m.dependencies;
    j["engineVersion"] = m.engineVersion;
    j["files"] = json::array();
    for (const PackageFile& f : m.files) j["files"].push_back({{"path", f.path}, {"size", f.size}, {"hash", f.hash}});
    return j;
}

Result<PackageManifest> manifestFromJson(const json& j) {
    if (!j.is_object() || j.value("format", "") != "agr.package") return makeError(Error::Code::ParseError, "ce fichier n'est pas un paquet AGR");
    if (j.value("version", 0) > 1) return makeError(Error::Code::VersionMismatch, "paquet créé par une version plus récente du moteur");
    PackageManifest m;
    m.name = j.value("name", "");
    m.version = j.value("packageVersion", "");
    m.author = j.value("author", "");
    m.description = j.value("description", "");
    m.engineVersion = j.value("engineVersion", "");
    for (const auto& d : j.value("dependencies", json::array()))
        if (d.is_string()) m.dependencies.push_back(d.get<std::string>());
    for (const auto& f : j.value("files", json::array())) m.files.push_back({f.value("path", ""), f.value("size", u64{0}), f.value("hash", "")});
    if (Status s = validatePackageName(m.name); !s) return s.error();
    for (const PackageFile& f : m.files)
        if (Status s = validatePackagePath(f.path); !s) return s.error();
    return m;
}

/// Opens a package: manifest, and the offset where the file bytes start.
Result<PackageManifest> openPackage(std::ifstream& in, const fs::path& p, u64& dataOffset) {
    char magic[8];
    if (!in.read(magic, 8) || std::memcmp(magic, kMagic, 8) != 0) return makeError(Error::Code::ParseError, "ce fichier n'est pas un paquet AGR : " + p.string());
    unsigned char sizeBytes[4];
    if (!in.read(reinterpret_cast<char*>(sizeBytes), 4)) return makeError(Error::Code::ParseError, "paquet tronqué");
    const u32 size = static_cast<u32>(sizeBytes[0]) | (static_cast<u32>(sizeBytes[1]) << 8) | (static_cast<u32>(sizeBytes[2]) << 16) |
                     (static_cast<u32>(sizeBytes[3]) << 24);
    if (size == 0 || size > kMaxManifest) return makeError(Error::Code::ParseError, "manifeste de paquet invalide");
    std::string text(size, '\0');
    if (!in.read(text.data(), size)) return makeError(Error::Code::ParseError, "paquet tronqué (manifeste)");
    dataOffset = 12u + size;
    const json j = json::parse(text, nullptr, false);
    if (j.is_discarded()) return makeError(Error::Code::ParseError, "manifeste de paquet illisible");
    return manifestFromJson(j);
}

json loadRegistry(const fs::path& assetsRoot) {
    auto text = readFile(assetsRoot / kRegistry);
    if (!text) return json{{"format", "agr.packages"}, {"packages", json::array()}};
    json j = json::parse(text.value(), nullptr, false);
    if (j.is_discarded() || !j.is_object() || !j.contains("packages")) return json{{"format", "agr.packages"}, {"packages", json::array()}};
    return j;
}

Status saveRegistry(const fs::path& assetsRoot, const json& j) { return writeFile(assetsRoot / kRegistry, j.dump(2)); }

void removeEmptyParents(fs::path dir, const fs::path& root) {
    std::error_code ec;
    while (!dir.empty() && dir != root && fs::is_directory(dir, ec) && fs::is_empty(dir, ec)) {
        fs::remove(dir, ec);
        dir = dir.parent_path();
    }
}

} // namespace

std::string packageHash(std::string_view bytes) {
    u64 h = 1469598103934665603ull;
    for (unsigned char c : bytes) {
        h ^= c;
        h *= 1099511628211ull;
    }
    char buf[17];
    std::snprintf(buf, sizeof buf, "%016llx", static_cast<unsigned long long>(h));
    return buf;
}

Status validatePackageName(const std::string& name) {
    if (name.empty() || name.size() > 64) return makeError(Error::Code::InvalidArgument, "nom de paquet vide ou trop long");
    for (char c : name)
        if (!(std::islower(static_cast<unsigned char>(c)) || std::isdigit(static_cast<unsigned char>(c)) || c == '-' || c == '_' || c == '.'))
            return makeError(Error::Code::InvalidArgument, "nom de paquet « " + name + " » : minuscules, chiffres, - _ . seulement");
    return {};
}

Status validatePackagePath(const std::string& relative) {
    if (relative.empty() || relative.front() == '/' || relative.find('\\') != std::string::npos || relative.find(':') != std::string::npos)
        return makeError(Error::Code::InvalidArgument, "chemin de fichier refusé dans un paquet : " + relative);
    for (const auto& part : fs::path(relative))
        if (part == ".." || part == ".") return makeError(Error::Code::InvalidArgument, "chemin de fichier refusé dans un paquet : " + relative);
    if (relative == kRegistry) return makeError(Error::Code::InvalidArgument, "un paquet ne peut pas contenir packages.json");
    return {};
}

Result<PackageManifest> writePackage(const fs::path& output, PackageManifest manifest, const fs::path& assetsRoot, const std::vector<std::string>& relativePaths) {
    if (Status s = validatePackageName(manifest.name); !s) return s.error();
    std::set<std::string> files;
    std::error_code ec;
    for (const std::string& rel : relativePaths) {
        const fs::path p = assetsRoot / rel;
        if (fs::is_directory(p, ec)) {
            for (const auto& entry : fs::recursive_directory_iterator(p, ec))
                if (entry.is_regular_file()) files.insert(fs::relative(entry.path(), assetsRoot).generic_string());
        } else if (fs::is_regular_file(p, ec)) {
            files.insert(fs::relative(p, assetsRoot).generic_string());
        } else {
            return makeError(Error::Code::NotFound, "introuvable dans les assets : " + rel);
        }
    }
    files.erase(kRegistry);
    if (files.empty()) return makeError(Error::Code::InvalidArgument, "le paquet ne contient aucun fichier");
    manifest.files.clear();
    std::string payload;
    for (const std::string& rel : files) {
        if (Status s = validatePackagePath(rel); !s) return s.error();
        auto bytes = readFile(assetsRoot / rel);
        if (!bytes) return bytes.error();
        manifest.files.push_back({rel, bytes.value().size(), packageHash(bytes.value())});
        payload += bytes.value();
    }
    const std::string text = manifestToJson(manifest).dump(2);
    std::string out(kMagic, 8);
    const u32 size = static_cast<u32>(text.size());
    for (int i = 0; i < 4; ++i) out += static_cast<char>((size >> (8 * i)) & 0xFF);
    out += text;
    out += payload;
    if (Status s = writeFile(output, out); !s) return s.error();
    return manifest;
}

Result<PackageManifest> readPackageManifest(const fs::path& package) {
    std::ifstream in(package, std::ios::binary);
    if (!in) return makeError(Error::Code::IOError, "lecture impossible : " + package.string());
    u64 offset = 0;
    return openPackage(in, package, offset);
}

std::vector<InstalledPackage> installedPackages(const fs::path& assetsRoot) {
    std::vector<InstalledPackage> out;
    const json reg = loadRegistry(assetsRoot);  // kept alive: the loop reads into it
    for (const auto& p : reg["packages"]) {
        InstalledPackage ip;
        ip.name = p.value("name", "");
        ip.version = p.value("version", "");
        ip.source = p.value("source", "");
        for (const auto& f : p.value("files", json::array())) ip.files.push_back({f.value("path", ""), f.value("size", u64{0}), f.value("hash", "")});
        out.push_back(std::move(ip));
    }
    return out;
}

Result<InstallPlan> planInstall(const fs::path& package, const fs::path& assetsRoot) {
    auto manifest = readPackageManifest(package);
    if (!manifest) return manifest.error();
    InstallPlan plan;
    plan.manifest = std::move(manifest.value());
    const auto installed = installedPackages(assetsRoot);
    for (const std::string& dep : plan.manifest.dependencies)
        if (std::none_of(installed.begin(), installed.end(), [&](const InstalledPackage& p) { return p.name == dep; })) plan.missingDependencies.push_back(dep);
    for (const InstalledPackage& p : installed)
        if (p.name == plan.manifest.name) plan.installedVersion = p.version;
    for (const PackageFile& f : plan.manifest.files) {
        const fs::path target = assetsRoot / f.path;
        std::error_code ec;
        if (!fs::exists(target, ec)) {
            plan.newFiles.push_back(f.path);
            continue;
        }
        auto bytes = readFile(target);
        if (bytes && bytes.value().size() == f.size && packageHash(bytes.value()) == f.hash) plan.identicalFiles.push_back(f.path);
        else plan.changedFiles.push_back(f.path);
    }
    return plan;
}

Status installPackage(const fs::path& package, const fs::path& assetsRoot, bool replaceChanged) {
    auto plan = planInstall(package, assetsRoot);
    if (!plan) return plan.error();
    const InstallPlan& p = plan.value();
    if (!p.missingDependencies.empty()) {
        std::string list;
        for (const std::string& d : p.missingDependencies) list += (list.empty() ? "" : ", ") + d;
        return makeError(Error::Code::NotFound, "dépendances manquantes : " + list);
    }
    if (!p.changedFiles.empty() && !replaceChanged)
        return makeError(Error::Code::AlreadyExists, std::to_string(p.changedFiles.size()) + " fichier(s) existant(s) différent(s) seraient remplacés");
    // Read and verify everything before writing anything.
    std::ifstream in(package, std::ios::binary);
    u64 offset = 0;
    auto manifest = openPackage(in, package, offset);
    if (!manifest) return manifest.error();
    std::vector<std::string> contents;
    for (const PackageFile& f : manifest.value().files) {
        std::string bytes(static_cast<usize>(f.size), '\0');
        if (f.size && !in.read(bytes.data(), static_cast<std::streamsize>(f.size)))
            return makeError(Error::Code::ParseError, "paquet tronqué : " + f.path);
        if (packageHash(bytes) != f.hash) return makeError(Error::Code::ParseError, "paquet corrompu : " + f.path + " (empreinte différente)");
        contents.push_back(std::move(bytes));
    }
    for (usize i = 0; i < contents.size(); ++i)
        if (Status s = writeFile(assetsRoot / manifest.value().files[i].path, contents[i]); !s) return s;
    // Record it (replacing a previous version's record).
    json reg = loadRegistry(assetsRoot);
    json list = json::array();
    for (const auto& e : reg["packages"])
        if (e.value("name", "") != p.manifest.name) list.push_back(e);
    json entry{{"name", p.manifest.name},
               {"version", p.manifest.version},
               {"source", package.filename().string()},
               {"dependencies", p.manifest.dependencies},
               {"files", json::array()}};
    for (const PackageFile& f : p.manifest.files) entry["files"].push_back({{"path", f.path}, {"size", f.size}, {"hash", f.hash}});
    list.push_back(entry);
    reg["packages"] = list;
    return saveRegistry(assetsRoot, reg);
}

Result<UninstallPlan> planUninstall(const std::string& name, const fs::path& assetsRoot) {
    const auto installed = installedPackages(assetsRoot);
    auto it = std::find_if(installed.begin(), installed.end(), [&](const InstalledPackage& p) { return p.name == name; });
    if (it == installed.end()) return makeError(Error::Code::NotFound, "paquet non installé : " + name);
    UninstallPlan plan;
    for (const PackageFile& f : it->files) {
        if (!validatePackagePath(f.path)) continue;  // never trust a hand-edited registry
        const fs::path target = assetsRoot / f.path;
        std::error_code ec;
        if (!fs::exists(target, ec)) {
            plan.missing.push_back(f.path);
            continue;
        }
        auto bytes = readFile(target);
        if (bytes && packageHash(bytes.value()) == f.hash) plan.remove.push_back(f.path);
        else plan.modified.push_back(f.path);
    }
    // Installed packages that depend on it (dependencies are recorded at install).
    const json reg = loadRegistry(assetsRoot);
    for (const auto& e : reg["packages"])
        for (const auto& d : e.value("dependencies", json::array()))
            if (d.is_string() && d.get<std::string>() == name) plan.neededBy.push_back(e.value("name", ""));
    return plan;
}

Status uninstallPackage(const std::string& name, const fs::path& assetsRoot, bool removeModified) {
    auto plan = planUninstall(name, assetsRoot);
    if (!plan) return plan.error();
    if (!plan.value().neededBy.empty()) return makeError(Error::Code::InvalidArgument, "d'autres paquets en dépendent : " + plan.value().neededBy.front());
    std::vector<std::string> targets = plan.value().remove;
    if (removeModified) targets.insert(targets.end(), plan.value().modified.begin(), plan.value().modified.end());
    std::error_code ec;
    for (const std::string& rel : targets) {
        fs::remove(assetsRoot / rel, ec);
        removeEmptyParents((assetsRoot / rel).parent_path(), assetsRoot);
    }
    json reg = loadRegistry(assetsRoot);
    json list = json::array();
    for (const auto& e : reg["packages"])
        if (e.value("name", "") != name) list.push_back(e);
    reg["packages"] = list;
    return saveRegistry(assetsRoot, reg);
}

std::vector<RepositoryEntry> listRepository(const fs::path& folder) {
    std::vector<RepositoryEntry> out;
    std::error_code ec;
    if (!fs::is_directory(folder, ec)) return out;
    for (const auto& entry : fs::directory_iterator(folder, ec)) {
        if (!entry.is_regular_file() || entry.path().extension() != ".agrpkg") continue;
        if (auto m = readPackageManifest(entry.path())) out.push_back({entry.path(), std::move(m.value())});
    }
    std::sort(out.begin(), out.end(), [](const RepositoryEntry& a, const RepositoryEntry& b) { return a.manifest.name < b.manifest.name; });
    return out;
}

} // namespace agr::editor
