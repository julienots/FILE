#include "agr/core/Pack.hpp"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <set>

namespace agr {

namespace {
constexpr char kMagic[8] = {'A', 'G', 'R', 'P', 'A', 'K', '0', '1'};
constexpr usize kHeader = 8 + 4 + 16 + 8 + 4;

void putU16(ByteBuffer& b, u16 v) { b.push_back(static_cast<u8>(v)), b.push_back(static_cast<u8>(v >> 8)); }
void putU32(ByteBuffer& b, u32 v) {
    for (int i = 0; i < 4; ++i) b.push_back(static_cast<u8>(v >> (8 * i)));
}
void putU64(ByteBuffer& b, u64 v) {
    for (int i = 0; i < 8; ++i) b.push_back(static_cast<u8>(v >> (8 * i)));
}
u64 getLE(const u8* p, int n) {
    u64 v = 0;
    for (int i = 0; i < n; ++i) v |= u64(p[i]) << (8 * i);
    return v;
}
std::array<u8, 16> keyCheck(const crypto::AesKey& key) {
    std::string s(reinterpret_cast<const char*>(key.data()), key.size());
    s += "agr-pack-check";
    const auto d = crypto::sha256(s);
    std::array<u8, 16> out;
    std::memcpy(out.data(), d.data(), 16);
    return out;
}
u64 randomNonce() {
    u8 b[8];
    crypto::randomBytes(b, 8);
    return getLE(b, 8);
}
std::string normalize(std::string_view p) {
    std::string s(p);
    std::replace(s.begin(), s.end(), '\\', '/');
    while (!s.empty() && s.front() == '/') s.erase(s.begin());
    return s;
}
}  // namespace

crypto::AesKey packKeyFromText(std::string_view text) { return crypto::sha256(std::string("agr-pack:") + std::string(text)); }

// ------------------------------------------------------------------ writer
void PackWriter::add(std::string path, ByteBuffer bytes) { m_files[normalize(path)] = std::move(bytes); }

Status PackWriter::write(const std::filesystem::path& output, const crypto::AesKey* key) const {
    std::optional<crypto::Aes256> aes;
    if (key) aes.emplace(*key);
    // Index first (offsets are relative to the end of the index).
    std::vector<PackEntry> entries;
    u64 offset = 0;
    for (const auto& [path, bytes] : m_files) {
        if (path.size() > 0xFFFF) return makeError(Error::Code::InvalidArgument, "pack: path too long");
        PackEntry e;
        e.path = path;
        e.offset = offset;
        e.size = bytes.size();
        e.nonce = aes ? randomNonce() : 0;
        e.hash = crypto::sha256(bytes.data(), bytes.size());
        offset += e.size;
        entries.push_back(std::move(e));
    }
    ByteBuffer index;
    putU32(index, static_cast<u32>(entries.size()));
    for (const PackEntry& e : entries) {
        putU16(index, static_cast<u16>(e.path.size()));
        index.insert(index.end(), e.path.begin(), e.path.end());
        putU64(index, e.offset), putU64(index, e.size), putU64(index, e.nonce);
        index.insert(index.end(), e.hash.begin(), e.hash.end());
    }
    const u64 indexNonce = aes ? randomNonce() : 0;
    if (aes) crypto::aes256Ctr(*aes, indexNonce, 0, index.data(), index.size());

    const std::filesystem::path tmp = output.string() + ".tmp";
    FILE* f = std::fopen(tmp.string().c_str(), "wb");
    if (!f) return makeError(Error::Code::IOError, "pack: cannot write " + output.string());
    ByteBuffer head(kMagic, kMagic + 8);
    putU32(head, aes ? 1u : 0u);
    if (aes) {
        const auto check = keyCheck(*key);
        head.insert(head.end(), check.begin(), check.end());
    } else {
        head.insert(head.end(), 16, 0);
    }
    putU64(head, indexNonce);
    putU32(head, static_cast<u32>(index.size()));
    bool ok = std::fwrite(head.data(), 1, head.size(), f) == head.size() && std::fwrite(index.data(), 1, index.size(), f) == index.size();
    usize i = 0;
    for (const auto& [path, bytes] : m_files) {
        const PackEntry& e = entries[i++];
        if (bytes.empty()) continue;
        if (aes) {
            ByteBuffer enc = bytes;
            crypto::aes256Ctr(*aes, e.nonce, 0, enc.data(), enc.size());
            ok = ok && std::fwrite(enc.data(), 1, enc.size(), f) == enc.size();
        } else {
            ok = ok && std::fwrite(bytes.data(), 1, bytes.size(), f) == bytes.size();
        }
    }
    ok = std::fclose(f) == 0 && ok;
    std::error_code ec;
    if (ok) std::filesystem::rename(tmp, output, ec);
    if (!ok || ec) {
        std::filesystem::remove(tmp, ec);
        return makeError(Error::Code::IOError, "pack: cannot write " + output.string());
    }
    return {};
}

// ------------------------------------------------------------------ reader
PackReader packFileReader(const std::filesystem::path& path) {
    struct Handle {
        FILE* f = nullptr;
        std::mutex mutex;
        ~Handle() {
            if (f) std::fclose(f);
        }
    };
    auto h = std::make_shared<Handle>();
    h->f = std::fopen(path.string().c_str(), "rb");
    return [h](u64 offset, void* out, u64 size) {
        if (!h->f) return false;
        std::lock_guard lock(h->mutex);
#if defined(_WIN32)
        if (_fseeki64(h->f, static_cast<long long>(offset), SEEK_SET) != 0) return false;
#else
        if (fseeko(h->f, static_cast<off_t>(offset), SEEK_SET) != 0) return false;
#endif
        return std::fread(out, 1, static_cast<usize>(size), h->f) == size;
    };
}

PackReader packMemoryReader(ByteBuffer bytes) {
    auto data = std::make_shared<const ByteBuffer>(std::move(bytes));
    return [data](u64 offset, void* out, u64 size) {
        if (offset > data->size() || size > data->size() - offset) return false;
        std::memcpy(out, data->data() + offset, static_cast<usize>(size));
        return true;
    };
}

namespace {
std::optional<crypto::AesKey> g_gameKey;
}
void setGamePackKey(const crypto::AesKey* key) { g_gameKey = key ? std::optional<crypto::AesKey>(*key) : std::nullopt; }
const crypto::AesKey* gamePackKey() { return g_gameKey ? &*g_gameKey : nullptr; }

Result<std::unique_ptr<PackFileProvider>> PackFileProvider::openFile(const std::filesystem::path& path, const crypto::AesKey* key) {
    std::error_code ec;
    if (!std::filesystem::is_regular_file(path, ec)) return makeError(Error::Code::NotFound, "pack not found: " + path.string());
    return open(packFileReader(path), key);
}

Result<std::unique_ptr<PackFileProvider>> PackFileProvider::open(PackReader reader, const crypto::AesKey* key) {
    u8 head[kHeader];
    if (!reader || !reader(0, head, kHeader) || std::memcmp(head, kMagic, 8) != 0)
        return makeError(Error::Code::ParseError, "not an AGR data pack");
    const u32 flags = static_cast<u32>(getLE(head + 8, 4));
    auto p = std::unique_ptr<PackFileProvider>(new PackFileProvider());
    if (flags & 1u) {
        if (!key) return makeError(Error::Code::Unsupported, "the pack is encrypted: a key is needed");
        const auto check = keyCheck(*key);
        if (std::memcmp(check.data(), head + 12, 16) != 0) return makeError(Error::Code::Unsupported, "wrong key for this pack");
        p->m_aes.emplace(*key);
    }
    const u64 indexNonce = getLE(head + 28, 8);
    const u32 indexSize = static_cast<u32>(getLE(head + 36, 4));
    if (indexSize < 4 || indexSize > (256u << 20)) return makeError(Error::Code::ParseError, "pack: bad index");
    ByteBuffer index(indexSize);
    if (!reader(kHeader, index.data(), indexSize)) return makeError(Error::Code::ParseError, "pack: truncated index");
    if (p->m_aes) crypto::aes256Ctr(*p->m_aes, indexNonce, 0, index.data(), index.size());
    const u8* c = index.data();
    const u8* end = c + index.size();
    const u32 count = static_cast<u32>(getLE(c, 4));
    c += 4;
    const u64 base = kHeader + indexSize;
    for (u32 i = 0; i < count; ++i) {
        if (end - c < 2) return makeError(Error::Code::ParseError, "pack: bad index");
        const u16 len = static_cast<u16>(getLE(c, 2));
        c += 2;
        if (end - c < static_cast<std::ptrdiff_t>(len) + 56) return makeError(Error::Code::ParseError, "pack: bad index");
        PackEntry e;
        e.path.assign(reinterpret_cast<const char*>(c), len);
        c += len;
        e.offset = base + getLE(c, 8), e.size = getLE(c + 8, 8), e.nonce = getLE(c + 16, 8);
        std::memcpy(e.hash.data(), c + 24, 32);
        c += 56;
        p->m_entries.push_back(std::move(e));
    }
    std::sort(p->m_entries.begin(), p->m_entries.end(), [](const PackEntry& a, const PackEntry& b) { return a.path < b.path; });
    p->m_reader = std::move(reader);
    return p;
}

const PackEntry* PackFileProvider::find(std::string_view path) const {
    const std::string key = normalize(path);
    auto it = std::lower_bound(m_entries.begin(), m_entries.end(), key, [](const PackEntry& e, const std::string& k) { return e.path < k; });
    return it != m_entries.end() && it->path == key ? &*it : nullptr;
}

bool PackFileProvider::exists(std::string_view p) const {
    if (find(p)) return true;
    std::string dir = normalize(p);
    if (!dir.empty() && dir.back() != '/') dir += '/';
    auto it = std::lower_bound(m_entries.begin(), m_entries.end(), dir, [](const PackEntry& e, const std::string& k) { return e.path < k; });
    return it != m_entries.end() && it->path.starts_with(dir);
}

Result<ByteBuffer> PackFileProvider::read(std::string_view path) const {
    const PackEntry* e = find(path);
    if (!e) return makeError(Error::Code::NotFound, "not in the pack: '" + std::string(path) + "'");
    ByteBuffer out(static_cast<usize>(e->size));
    if (e->size && !m_reader(e->offset, out.data(), e->size)) return makeError(Error::Code::IOError, "pack: cannot read '" + e->path + "'");
    if (m_aes) crypto::aes256Ctr(*m_aes, e->nonce, 0, out.data(), out.size());
    return out;
}

Status PackFileProvider::write(std::string_view, const void*, usize) { return makeError(Error::Code::Unsupported, "data packs are read-only"); }

Result<std::vector<std::string>> PackFileProvider::list(std::string_view relativeDir) const {
    std::string dir = normalize(relativeDir);
    if (!dir.empty() && dir.back() != '/') dir += '/';
    std::set<std::string> names;
    for (const PackEntry& e : m_entries) {
        if (!e.path.starts_with(dir)) continue;
        const std::string rest = e.path.substr(dir.size());
        const auto slash = rest.find('/');
        names.insert(slash == std::string::npos ? rest : rest.substr(0, slash + 1));
    }
    if (names.empty() && !dir.empty()) return makeError(Error::Code::NotFound, "not a directory in the pack: '" + dir + "'");
    return std::vector<std::string>(names.begin(), names.end());
}

// ------------------------------------------------------------------ subdir / overlay
SubdirFileProvider::SubdirFileProvider(std::shared_ptr<FileProvider> inner, std::string prefix) : m_inner(std::move(inner)), m_prefix(normalize(prefix)) {
    if (!m_prefix.empty() && m_prefix.back() != '/') m_prefix += '/';
}
std::string SubdirFileProvider::full(std::string_view p) const { return m_prefix + normalize(p); }

void OverlayFileProvider::addLayer(std::shared_ptr<FileProvider> layer) {
    std::lock_guard lock(m_mutex);
    if (layer) m_layers.push_back(std::move(layer));
}
usize OverlayFileProvider::layerCount() const {
    std::lock_guard lock(m_mutex);
    return m_layers.size();
}
std::vector<std::shared_ptr<FileProvider>> OverlayFileProvider::snapshot() const {
    std::lock_guard lock(m_mutex);
    return m_layers;
}
bool OverlayFileProvider::exists(std::string_view p) const {
    for (const auto& l : snapshot())
        if (l->exists(p)) return true;
    return false;
}
Result<ByteBuffer> OverlayFileProvider::read(std::string_view p) const {
    const auto layers = snapshot();
    for (auto it = layers.rbegin(); it != layers.rend(); ++it)
        if ((*it)->exists(p)) return (*it)->read(p);
    return makeError(Error::Code::NotFound, "file not found: '" + std::string(p) + "'");
}
Status OverlayFileProvider::write(std::string_view, const void*, usize) { return makeError(Error::Code::Unsupported, "game data is read-only"); }
Result<std::vector<std::string>> OverlayFileProvider::list(std::string_view d) const {
    std::set<std::string> names;
    bool any = false;
    for (const auto& l : snapshot())
        if (auto r = l->list(d)) {
            any = true;
            names.insert(r.value().begin(), r.value().end());
        }
    if (!any) return makeError(Error::Code::NotFound, "not a directory: '" + std::string(d) + "'");
    return std::vector<std::string>(names.begin(), names.end());
}
i64 OverlayFileProvider::modifiedTime(std::string_view p) const {
    const auto layers = snapshot();
    for (auto it = layers.rbegin(); it != layers.rend(); ++it)
        if ((*it)->exists(p)) return (*it)->modifiedTime(p);
    return 0;
}

} // namespace agr
