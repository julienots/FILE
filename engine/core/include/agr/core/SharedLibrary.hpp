#pragma once
#include <string>

#include "agr/core/Result.hpp"
#include "agr/core/Types.hpp"

namespace agr {

/// RAII wrapper around dlopen / LoadLibrary, used by the plugin manager.
class SharedLibrary {
public:
    SharedLibrary() = default;
    ~SharedLibrary();
    SharedLibrary(SharedLibrary&& other) noexcept;
    SharedLibrary& operator=(SharedLibrary&& other) noexcept;
    SharedLibrary(const SharedLibrary&) = delete;
    SharedLibrary& operator=(const SharedLibrary&) = delete;

    static Result<SharedLibrary> open(const std::string& path);
    void* symbol(const char* name) const;
    bool isOpen() const { return m_handle != nullptr; }
    void close();

    /// "lib<name>.so", "<name>.dll"...
    static std::string decorate(const std::string& baseName);

private:
    void* m_handle = nullptr;
};

} // namespace agr
