// Single translation unit compiling volk and VulkanMemoryAllocator with
// their warnings silenced (third-party code, not held to the engine's -Werror).
#if defined(__GNUC__)
    #pragma GCC diagnostic push
    #pragma GCC diagnostic ignored "-Wall"
    #pragma GCC diagnostic ignored "-Wextra"
    #pragma GCC diagnostic ignored "-Wshadow"
    #pragma GCC diagnostic ignored "-Wnull-dereference"
    #pragma GCC diagnostic ignored "-Wcast-align"
    #pragma GCC diagnostic ignored "-Wmissing-field-initializers"
    #pragma GCC diagnostic ignored "-Wunused-parameter"
    #pragma GCC diagnostic ignored "-Wunused-variable"
    #pragma GCC diagnostic ignored "-Wpedantic"
    #if !defined(__clang__)
        #pragma GCC diagnostic ignored "-Wclass-memaccess"
        #pragma GCC diagnostic ignored "-Wparentheses"
        #pragma GCC diagnostic ignored "-Wunused-but-set-variable"
    #else
        #pragma GCC diagnostic ignored "-Wnullability-completeness"
    #endif
#endif

#define VOLK_IMPLEMENTATION
#include <volk.h>

#define VMA_IMPLEMENTATION
#define VMA_STATIC_VULKAN_FUNCTIONS 0
#define VMA_DYNAMIC_VULKAN_FUNCTIONS 1
#include <vk_mem_alloc.h>

#if defined(__GNUC__)
    #pragma GCC diagnostic pop
#endif
