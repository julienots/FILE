#pragma once
// Compile-time platform / compiler detection for AGR ENGINE.

#if defined(__ANDROID__)
    #define AGR_PLATFORM_ANDROID 1
    #define AGR_PLATFORM_NAME "Android"
#elif defined(_WIN32)
    #define AGR_PLATFORM_WINDOWS 1
    #define AGR_PLATFORM_NAME "Windows"
#elif defined(__linux__)
    #define AGR_PLATFORM_LINUX 1
    #define AGR_PLATFORM_NAME "Linux"
#elif defined(__APPLE__)
    #define AGR_PLATFORM_APPLE 1
    #define AGR_PLATFORM_NAME "Apple"
#else
    #error "AGR ENGINE: unsupported platform"
#endif

#if defined(AGR_PLATFORM_ANDROID)
    #define AGR_PLATFORM_MOBILE 1
#else
    #define AGR_PLATFORM_DESKTOP 1
#endif

#if defined(_MSC_VER)
    #define AGR_COMPILER_MSVC 1
    #define AGR_FORCE_INLINE __forceinline
    #define AGR_DEBUG_BREAK() __debugbreak()
#elif defined(__clang__) || defined(__GNUC__)
    #define AGR_COMPILER_GCC_CLANG 1
    #define AGR_FORCE_INLINE inline __attribute__((always_inline))
    #if defined(__i386__) || defined(__x86_64__)
        #define AGR_DEBUG_BREAK() __asm__ volatile("int $0x03")
    #else
        #define AGR_DEBUG_BREAK() __builtin_trap()
    #endif
#endif

#if defined(_WIN32)
    #define AGR_PLUGIN_EXPORT extern "C" __declspec(dllexport)
#else
    #define AGR_PLUGIN_EXPORT extern "C" __attribute__((visibility("default")))
#endif

namespace agr {
inline constexpr const char* kEngineName = "AGR ENGINE";
inline constexpr const char* kEngineSlogan = "Build Beyond Limits.";
inline constexpr const char* kStudioName = "Superessence Studio";
inline constexpr const char* kPlatformName = AGR_PLATFORM_NAME;
} // namespace agr
