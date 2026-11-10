#include "agr/core/FileSystem.hpp"

#include <algorithm>
#include <filesystem>
#include <fstream>

namespace agr {
namespace fs = std::filesystem;

// --------------------------------------------------------- NativeFileProvider
NativeFileProvider::NativeFileProvider(std::string rootDirectory, bool readOnly)
    : m_root(std::move(rootDirectory)), m_readOnly(readOnly) {}

bool NativeFileProvider::exists(std::string_view relativePath) const {
    std::error_code ec;
    return fs::exists(fs::path(m_root) / fs::path(relativePath), ec);
}

Result<ByteBuffer> NativeFileProvider::read(std::string_view relativePath) const {
    const fs::path full = fs::path(m_root) / fs::path(relativePath);
    std::ifstream in(full, std::ios::binary | std::ios::ate);
    if (!in) return makeError(Error::Code::NotFound, "cannot open '" + full.string() + "'");
    const std::streamsize size = in.tellg();
    if (size < 0) return makeError(Error::Code::IOError, "cannot stat '" + full.string() + "'");
    ByteBuffer data(static_cast<usize>(size));
    in.seekg(0);
    if (size > 0 && !in.read(reinterpret_cast<char*>(data.data()), size))
        return makeError(Error::Code::IOError, "read failed for '" + full.string() + "'");
    return data;
}

Status NativeFileProvider::write(std::string_view relativePath, const void* data, usize size) {
    if (m_readOnly) return makeError(Error::Code::Unsupported, "mount is read-only");
    const fs::path full = fs::path(m_root) / fs::path(relativePath);
    std::error_code ec;
    if (full.has_parent_path()) fs::create_directories(full.parent_path(), ec);
    // Write to a temporary file then rename: a crash never leaves a half-written save.
    const fs::path tmp = full.string() + ".tmp";
    {
        std::ofstream out(tmp, std::ios::binary | std::ios::trunc);
        if (!out) return makeError(Error::Code::IOError, "cannot create '" + tmp.string() + "'");
        out.write(static_cast<const char*>(data), static_cast<std::streamsize>(size));
        if (!out) return makeError(Error::Code::IOError, "write failed for '" + tmp.string() + "'");
    }
    fs::rename(tmp, full, ec);
    if (ec) return makeError(Error::Code::IOError, "rename failed: " + ec.message());
    return {};
}

i64 NativeFileProvider::modifiedTime(std::string_view relativePath) const {
    std::error_code ec;
    const auto t = fs::last_write_time(fs::path(m_root) / fs::path(relativePath), ec);
    return ec ? 0 : static_cast<i64>(t.time_since_epoch().count());
}

Result<std::vector<std::string>> NativeFileProvider::list(std::string_view relativeDir) const {
    const fs::path dir = fs::path(m_root) / fs::path(relativeDir);
    std::error_code ec;
    if (!fs::is_directory(dir, ec)) return makeError(Error::Code::NotFound, "not a directory: '" + dir.string() + "'");
    std::vector<std::string> out;
    for (const auto& entry : fs::directory_iterator(dir, ec)) {
        std::string name = entry.path().filename().generic_string();
        if (entry.is_directory()) name += '/';
        out.push_back(std::move(name));
    }
    std::sort(out.begin(), out.end());
    return out;
}

// ---------------------------------------------------------- VirtualFileSystem
Result<VirtualFileSystem::ParsedUri> VirtualFileSystem::parse(std::string_view uri) {
    const auto sep = uri.find("://");
    if (sep == std::string_view::npos || sep == 0)
        return makeError(Error::Code::InvalidArgument, "URI must look like 'mount://path': '" + std::string(uri) + "'");
    ParsedUri out;
    out.scheme = std::string(uri.substr(0, sep));
    std::string_view rest = uri.substr(sep + 3);
    if (!rest.empty() && (rest.front() == '/' || rest.front() == '\\'))
        return makeError(Error::Code::InvalidArgument, "absolute paths are not allowed in URIs");

    // Normalize segments; refuse to climb above the mount root.
    std::vector<std::string_view> segments;
    usize start = 0;
    while (start <= rest.size()) {
        usize end = rest.find_first_of("/\\", start);
        if (end == std::string_view::npos) end = rest.size();
        std::string_view seg = rest.substr(start, end - start);
        if (seg == "..") {
            if (segments.empty()) return makeError(Error::Code::InvalidArgument, "path escapes its mount point");
            segments.pop_back();
        } else if (!seg.empty() && seg != ".") {
            if (seg.find(':') != std::string_view::npos)
                return makeError(Error::Code::InvalidArgument, "':' is not allowed in path segments");
            segments.push_back(seg);
        }
        start = end + 1;
    }
    for (usize i = 0; i < segments.size(); ++i) {
        if (i) out.path += '/';
        out.path += segments[i];
    }
    return out;
}

void VirtualFileSystem::mount(const std::string& scheme, std::unique_ptr<FileProvider> provider) {
    m_mounts[scheme] = std::move(provider);
}
void VirtualFileSystem::unmount(const std::string& scheme) { m_mounts.erase(scheme); }
bool VirtualFileSystem::isMounted(const std::string& scheme) const { return m_mounts.contains(scheme); }

Result<FileProvider*> VirtualFileSystem::resolve(std::string_view uri, std::string& outPath) const {
    auto parsed = parse(uri);
    if (!parsed) return parsed.error();
    auto it = m_mounts.find(parsed.value().scheme);
    if (it == m_mounts.end()) return makeError(Error::Code::NotFound, "no mount point '" + parsed.value().scheme + "'");
    outPath = std::move(parsed.value().path);
    return it->second.get();
}

bool VirtualFileSystem::exists(std::string_view uri) const {
    std::string path;
    auto p = resolve(uri, path);
    return p && p.value()->exists(path);
}

Result<ByteBuffer> VirtualFileSystem::readBytes(std::string_view uri) const {
    std::string path;
    auto p = resolve(uri, path);
    if (!p) return p.error();
    return p.value()->read(path);
}

Result<std::string> VirtualFileSystem::readText(std::string_view uri) const {
    auto bytes = readBytes(uri);
    if (!bytes) return bytes.error();
    return std::string(bytes.value().begin(), bytes.value().end());
}

Status VirtualFileSystem::writeBytes(std::string_view uri, const void* data, usize size) {
    std::string path;
    auto p = resolve(uri, path);
    if (!p) return p.error();
    if (path.empty()) return makeError(Error::Code::InvalidArgument, "empty file path");
    return p.value()->write(path, data, size);
}

Status VirtualFileSystem::writeText(std::string_view uri, std::string_view text) {
    return writeBytes(uri, text.data(), text.size());
}

i64 VirtualFileSystem::modifiedTime(std::string_view uri) const {
    std::string path;
    auto p = resolve(uri, path);
    return p ? p.value()->modifiedTime(path) : 0;
}

Result<std::vector<std::string>> VirtualFileSystem::list(std::string_view uriDir) const {
    std::string path;
    auto p = resolve(uriDir, path);
    if (!p) return p.error();
    return p.value()->list(path);
}

} // namespace agr
