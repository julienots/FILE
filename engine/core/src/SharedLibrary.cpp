#include "agr/core/SharedLibrary.hpp"

#include "agr/core/Platform.hpp"

#if defined(AGR_PLATFORM_WINDOWS)
    #ifndef WIN32_LEAN_AND_MEAN
        #define WIN32_LEAN_AND_MEAN
    #endif
    #include <windows.h>
#else
    #include <dlfcn.h>
#endif

namespace agr {

SharedLibrary::~SharedLibrary() { close(); }

SharedLibrary::SharedLibrary(SharedLibrary&& other) noexcept : m_handle(other.m_handle) { other.m_handle = nullptr; }

SharedLibrary& SharedLibrary::operator=(SharedLibrary&& other) noexcept {
    if (this != &other) {
        close();
        m_handle = other.m_handle;
        other.m_handle = nullptr;
    }
    return *this;
}

Result<SharedLibrary> SharedLibrary::open(const std::string& path) {
    SharedLibrary lib;
#if defined(AGR_PLATFORM_WINDOWS)
    lib.m_handle = reinterpret_cast<void*>(LoadLibraryA(path.c_str()));
    if (!lib.m_handle) return makeError(Error::Code::NotFound, "LoadLibrary failed for '" + path + "'");
#else
    lib.m_handle = dlopen(path.c_str(), RTLD_NOW | RTLD_LOCAL);
    if (!lib.m_handle) {
        const char* err = dlerror();
        return makeError(Error::Code::NotFound, err ? err : ("dlopen failed for '" + path + "'"));
    }
#endif
    return lib;
}

void* SharedLibrary::symbol(const char* name) const {
    if (!m_handle) return nullptr;
#if defined(AGR_PLATFORM_WINDOWS)
    return reinterpret_cast<void*>(GetProcAddress(static_cast<HMODULE>(m_handle), name));
#else
    return dlsym(m_handle, name);
#endif
}

void SharedLibrary::close() {
    if (!m_handle) return;
#if defined(AGR_PLATFORM_WINDOWS)
    FreeLibrary(static_cast<HMODULE>(m_handle));
#else
    dlclose(m_handle);
#endif
    m_handle = nullptr;
}

std::string SharedLibrary::decorate(const std::string& baseName) {
#if defined(AGR_PLATFORM_WINDOWS)
    return baseName + ".dll";
#elif defined(AGR_PLATFORM_APPLE)
    return "lib" + baseName + ".dylib";
#else
    return "lib" + baseName + ".so";
#endif
}

} // namespace agr
