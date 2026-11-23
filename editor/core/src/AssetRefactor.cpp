#include "agr/editor/AssetRefactor.hpp"

#include <fstream>
#include <iterator>
#include <set>

namespace agr::editor {

namespace fs = std::filesystem;

namespace {
bool pathChar(char c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_' || c == '-' || c == '.' || c == '/' ||
           static_cast<unsigned char>(c) >= 0x80;  // UTF-8 names
}

const std::set<std::string>& textExtensions() {
    static const std::set<std::string> ext{".agrscene", ".agrprefab", ".agrmat", ".agranim", ".agrctrl", ".agrlogic", ".agrparticle",
                                           ".agrsheet", ".agrshadergraph", ".agrshader", ".agrcube", ".agrproject", ".agrenv", ".agrterrain",
                                           ".lua", ".json", ".txt", ".csv", ".md"};
    return ext;
}

bool normalizedRel(const std::string& rel, std::string& out) {
    const fs::path p = fs::path(rel).lexically_normal();
    out = p.generic_string();
    while (!out.empty() && out.back() == '/') out.pop_back();
    return !out.empty() && !p.is_absolute() && out.rfind("..", 0) != 0 && out != ".";
}
} // namespace

usize rewriteAssetReferences(std::string& text, std::string_view fromUri, std::string_view toUri, bool folder) {
    if (fromUri.empty() || fromUri == toUri) return 0;
    usize count = 0;
    std::string out;
    out.reserve(text.size());
    usize at = 0;
    while (true) {
        const usize hit = text.find(fromUri, at);
        if (hit == std::string::npos) break;
        const usize end = hit + fromUri.size();
        const char next = end < text.size() ? text[end] : '\0';
        // A file: the path must end here (quote, '#', space...). A folder: a '/' must follow.
        const bool whole = folder ? next == '/' : !pathChar(next);
        out.append(text, at, hit - at);
        if (whole) {
            out.append(toUri);
            ++count;
        } else {
            out.append(fromUri);
        }
        at = end;
    }
    if (count == 0) return 0;
    out.append(text, at, std::string::npos);
    text = std::move(out);
    return count;
}

Result<AssetMoveReport> moveAsset(const fs::path& projectRoot, const fs::path& assetsDir, const std::string& fromRelIn, const std::string& toRelIn) {
    std::string fromRel, toRel;
    if (!normalizedRel(fromRelIn, fromRel) || !normalizedRel(toRelIn, toRel)) return makeError(Error::Code::InvalidArgument, "invalid asset path");
    if (fromRel == toRel) return AssetMoveReport{};
    std::error_code ec;
    const fs::path from = assetsDir / fromRel, to = assetsDir / toRel;
    if (!fs::exists(from, ec)) return makeError(Error::Code::NotFound, "asset not found: " + fromRel);
    if (fs::exists(to, ec)) return makeError(Error::Code::AlreadyExists, "an asset already exists there: " + toRel);
    const bool folder = fs::is_directory(from, ec);
    if (folder && (toRel + "/").rfind(fromRel + "/", 0) == 0) return makeError(Error::Code::InvalidArgument, "a folder cannot move into itself");
    fs::create_directories(to.parent_path(), ec);
    fs::rename(from, to, ec);
    if (ec) return makeError(Error::Code::IOError, "move failed: " + ec.message());

    AssetMoveReport report;
    const std::string fromUri = "assets://" + fromRel, toUri = "assets://" + toRel;
    for (auto it = fs::recursive_directory_iterator(projectRoot, fs::directory_options::skip_permission_denied, ec);
         it != fs::recursive_directory_iterator(); it.increment(ec)) {
        if (ec) break;
        const fs::path& p = it->path();
        const std::string name = p.filename().string();
        if (it->is_directory(ec)) {
            if (name == "build" || name == ".git" || name == ".agr" || (name.size() > 1 && name[0] == '.')) it.disable_recursion_pending();
            continue;
        }
        if (!textExtensions().count(p.extension().string()) || it->file_size(ec) > 32u * 1024 * 1024) continue;
        std::string text;
        {
            std::ifstream in(p, std::ios::binary);
            text.assign(std::istreambuf_iterator<char>(in), {});
        }
        const usize n = rewriteAssetReferences(text, fromUri, toUri, folder);
        if (n == 0) continue;
        std::ofstream out(p, std::ios::binary | std::ios::trunc);
        out.write(text.data(), static_cast<std::streamsize>(text.size()));
        if (!out) return makeError(Error::Code::IOError, "could not update " + p.string());
        report.updatedFiles.push_back(p);
        report.references += n;
    }
    return report;
}

} // namespace agr::editor
