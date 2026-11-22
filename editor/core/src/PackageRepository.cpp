#include "agr/editor/PackageRepository.hpp"

#include <algorithm>
#include <fstream>
#include <functional>
#include <set>

#include <nlohmann/json.hpp>

#include "agr/core/Crypto.hpp"
#include "agr/core/Log.hpp"
#include "agr/net/Http.hpp"

namespace agr::editor {

namespace fs = std::filesystem;
using nlohmann::json;

namespace {
std::optional<std::string> readFile(const fs::path& p) {
    std::ifstream in(p, std::ios::binary);
    if (!in) return std::nullopt;
    return std::string((std::istreambuf_iterator<char>(in)), {});
}
bool isHttp(const std::string& url) { return url.starts_with("http://") || url.starts_with("https://"); }
fs::path filePath(const std::string& url) { return fs::path(url.starts_with("file://") ? url.substr(7) : url); }
/// Fetches a URL's bytes (http(s) or local file).
Result<std::string> fetch(const std::string& url) {
    if (isHttp(url)) {
        net::HttpRequest r;
        r.url = url;
        r.timeoutSeconds = 120;
        net::HttpResponse resp = net::httpRequest(r);
        if (!resp.error.empty()) return makeError(Error::Code::IOError, url + ": " + resp.error);
        if (!resp.ok()) return makeError(Error::Code::NotFound, url + ": HTTP " + std::to_string(resp.status));
        return std::move(resp.body);
    }
    auto text = readFile(filePath(url));
    if (!text) return makeError(Error::Code::NotFound, "cannot read " + url);
    return *text;
}
/// Version order "1.10.0" > "1.9.2" (numeric parts).
bool newer(const std::string& a, const std::string& b) {
    auto parts = [](const std::string& v) {
        std::vector<long> out;
        usize i = 0;
        while (i < v.size()) {
            out.push_back(std::strtol(v.c_str() + i, nullptr, 10));
            const auto dot = v.find('.', i);
            if (dot == std::string::npos) break;
            i = dot + 1;
        }
        return out;
    };
    return parts(a) > parts(b);
}
}  // namespace

const OnlinePackage* OnlineIndex::find(const std::string& name) const {
    for (const OnlinePackage& p : packages)
        if (p.name == name) return &p;
    return nullptr;
}

Result<OnlineIndex> parseRepositoryIndex(std::string_view text, const std::string& indexUrl) {
    const json j = json::parse(text, nullptr, false);
    if (j.is_discarded() || !j.is_object() || j.value("format", "") != "agr.repository")
        return makeError(Error::Code::ParseError, "not an AGR repository index (format: agr.repository)");
    OnlineIndex idx;
    idx.title = j.value("title", "");
    const std::string base = indexUrl.substr(0, indexUrl.find_last_of('/') + 1);
    try {
        for (const json& p : j.value("packages", json::array())) {
            OnlinePackage o;
            o.name = p.value("name", "");
            o.version = p.value("version", "1.0.0");
            o.author = p.value("author", "");
            o.description = p.value("description", "");
            o.dependencies = p.value("dependencies", std::vector<std::string>{});
            o.sha256 = p.value("sha256", "");
            o.size = p.value("size", u64{0});
            const std::string file = p.value("file", "");
            if (!validatePackageName(o.name) || file.empty() || o.sha256.size() != 64) continue;  // malformed entry
            o.url = file.find("://") != std::string::npos ? file : base + file;
            idx.packages.push_back(std::move(o));
        }
    } catch (const json::exception& e) {
        return makeError(Error::Code::ParseError, std::string("repository index: ") + e.what());
    }
    std::stable_sort(idx.packages.begin(), idx.packages.end(), [](const OnlinePackage& a, const OnlinePackage& b) {
        return a.name != b.name ? a.name < b.name : newer(a.version, b.version);
    });
    return idx;
}

Result<OnlineIndex> writeRepositoryIndex(const fs::path& folder, const std::string& title) {
    json packages = json::array();
    for (const RepositoryEntry& e : listRepository(folder)) {
        auto bytes = readFile(e.path);
        if (!bytes) continue;
        const PackageManifest& m = e.manifest;
        packages.push_back({{"name", m.name},
                            {"version", m.version},
                            {"author", m.author},
                            {"description", m.description},
                            {"dependencies", m.dependencies},
                            {"file", e.path.filename().generic_string()},
                            {"size", bytes->size()},
                            {"sha256", crypto::sha256Hex(*bytes)}});
    }
    const json j = {{"format", "agr.repository"}, {"version", 1}, {"title", title}, {"packages", packages}};
    const std::string text = j.dump(2);
    std::ofstream out(folder / "index.json", std::ios::binary);
    if (!out || !(out << text)) return makeError(Error::Code::IOError, "cannot write index.json");
    out.close();
    return parseRepositoryIndex(text, (folder / "index.json").generic_string());
}

Result<OnlineIndex> fetchRepositoryIndex(const std::string& url) {
    std::string indexUrl = url;
    if (!indexUrl.ends_with(".json")) indexUrl += indexUrl.ends_with("/") ? "index.json" : "/index.json";
    auto text = fetch(indexUrl);
    if (!text) return text.error();
    return parseRepositoryIndex(text.value(), indexUrl);
}

Result<fs::path> downloadPackage(const OnlinePackage& p, const fs::path& folder) {
    auto bytes = fetch(p.url);
    if (!bytes) return bytes.error();
    if ((p.size && bytes.value().size() != p.size) || crypto::sha256Hex(bytes.value()) != p.sha256)
        return makeError(Error::Code::ParseError, p.name + ": the downloaded file does not match the index (size / SHA-256): refused");
    std::error_code ec;
    fs::create_directories(folder, ec);
    const fs::path out = folder / (p.name + "-" + p.version + ".agrpkg");
    std::ofstream f(out, std::ios::binary);
    if (!f || !f.write(bytes.value().data(), static_cast<std::streamsize>(bytes.value().size())))
        return makeError(Error::Code::IOError, "cannot write " + out.string());
    return out;
}

Result<std::vector<std::string>> resolveInstallOrder(const std::string& name, const OnlineIndex& index, const std::vector<std::string>& installed) {
    std::vector<std::string> order;
    std::set<std::string> done(installed.begin(), installed.end()), visiting;
    std::string error;
    std::function<bool(const std::string&, const std::string&)> visit = [&](const std::string& n, const std::string& from) {
        if (done.count(n)) return true;
        if (visiting.count(n)) {
            error = "dependency cycle through '" + n + "'";
            return false;
        }
        const OnlinePackage* p = index.find(n);
        if (!p) {
            error = from.empty() ? "'" + n + "' is not in the repository" : "'" + from + "' needs '" + n + "', which is not in the repository";
            return false;
        }
        visiting.insert(n);
        for (const std::string& d : p->dependencies)
            if (!visit(d, n)) return false;
        visiting.erase(n);
        done.insert(n);
        order.push_back(n);
        return true;
    };
    if (!visit(name, "")) return makeError(Error::Code::NotFound, error);
    return order;
}

Result<std::vector<std::string>> installFromRepository(const std::string& name, const OnlineIndex& index, const fs::path& downloadFolder,
                                                       const fs::path& assetsRoot, bool replaceChanged) {
    std::vector<std::string> installed;
    for (const InstalledPackage& p : installedPackages(assetsRoot)) installed.push_back(p.name);
    auto order = resolveInstallOrder(name, index, installed);
    if (!order) return order.error();
    // Download everything first: nothing is installed if one download fails.
    std::vector<fs::path> files;
    for (const std::string& n : order.value()) {
        auto f = downloadPackage(*index.find(n), downloadFolder);
        if (!f) return f.error();
        files.push_back(f.value());
    }
    for (usize i = 0; i < files.size(); ++i) {
        if (Status s = installPackage(files[i], assetsRoot, replaceChanged); !s) return makeError(s.error().code, order.value()[i] + ": " + s.error().message);
        AGR_LOG_INFO("Packages", "installed {} from the repository", order.value()[i]);
    }
    return order.value();
}

} // namespace agr::editor
