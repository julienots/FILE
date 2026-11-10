#include "agr/platform/SdlFileProvider.hpp"

#include <SDL3/SDL.h>

namespace agr {

std::string SdlFileProvider::path(std::string_view relative) const {
    if (m_root.empty()) return std::string(relative);
    return m_root + "/" + std::string(relative);
}

bool SdlFileProvider::exists(std::string_view relativePath) const {
    SDL_IOStream* io = SDL_IOFromFile(path(relativePath).c_str(), "rb");
    if (!io) return false;
    SDL_CloseIO(io);
    return true;
}

Result<ByteBuffer> SdlFileProvider::read(std::string_view relativePath) const {
    const std::string p = path(relativePath);
    SDL_IOStream* io = SDL_IOFromFile(p.c_str(), "rb");
    if (!io) return makeError(Error::Code::NotFound, "cannot open '" + p + "': " + SDL_GetError());
    ByteBuffer data;
    const Sint64 size = SDL_GetIOSize(io);
    if (size > 0) data.resize(static_cast<usize>(size));
    usize total = 0;
    for (;;) {
        if (total == data.size()) data.resize(std::max<usize>(4096, data.size() * 2));
        const usize n = SDL_ReadIO(io, data.data() + total, data.size() - total);
        total += n;
        if (n == 0) break;
    }
    const bool failed = SDL_GetIOStatus(io) == SDL_IO_STATUS_ERROR;
    SDL_CloseIO(io);
    if (failed) return makeError(Error::Code::IOError, "error while reading '" + p + "'");
    data.resize(total);
    return data;
}

Status SdlFileProvider::write(std::string_view relativePath, const void*, usize) {
    return makeError(Error::Code::Unsupported, "game data is read-only: " + std::string(relativePath));
}

Result<std::vector<std::string>> SdlFileProvider::list(std::string_view) const { return std::vector<std::string>{}; }

} // namespace agr
