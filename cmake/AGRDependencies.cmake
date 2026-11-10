# Third-party dependencies, pinned to exact released versions.
# Every dependency is justified in docs/adr/0002-third-party-dependencies.md.
include(FetchContent)
set(FETCHCONTENT_QUIET ON)

# --- nlohmann/json : serialization of scenes, projects, configs (MIT) --------
FetchContent_Declare(nlohmann_json
    URL https://github.com/nlohmann/json/releases/download/v3.11.3/json.tar.xz
    URL_HASH SHA256=d6c65aca6b1ed68e7a182f4757257b107ae403032760ed6ef121c9d55e81757d
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE)
FetchContent_MakeAvailable(nlohmann_json)

# --- SDL3 : windowing, input, touch, sensors on every platform (zlib) ---------
if(AGR_BUILD_PLATFORM)
    set(SDL_SHARED OFF CACHE BOOL "" FORCE)
    set(SDL_STATIC ON  CACHE BOOL "" FORCE)
    set(SDL_TEST_LIBRARY OFF CACHE BOOL "" FORCE)
    set(SDL_TESTS OFF CACHE BOOL "" FORCE)
    set(SDL_EXAMPLES OFF CACHE BOOL "" FORCE)
    set(SDL_INSTALL OFF CACHE BOOL "" FORCE)
    FetchContent_Declare(SDL3
        GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
        GIT_TAG        release-3.4.16
        GIT_SHALLOW    TRUE)
    FetchContent_MakeAvailable(SDL3)
endif()

# --- doctest : unit testing framework (MIT) ----------------------------------
if(AGR_BUILD_TESTS)
    set(DOCTEST_NO_INSTALL ON CACHE BOOL "" FORCE)
    FetchContent_Declare(doctest
        GIT_REPOSITORY https://github.com/doctest/doctest.git
        GIT_TAG        v2.4.11
        GIT_SHALLOW    TRUE)
    FetchContent_MakeAvailable(doctest)
endif()

# --- stb_image / stb_image_write : PNG, JPG, TGA, BMP decoding; PNG screenshots
#     (public domain / MIT). stb has no release tags: pinned to a commit.
FetchContent_Declare(stb
    GIT_REPOSITORY https://github.com/nothings/stb.git
    GIT_TAG        2c980bb59875b0d32144a71867fbdebb2f77cd20)
FetchContent_MakeAvailable(stb)
add_library(agr_stb INTERFACE)
target_include_directories(agr_stb SYSTEM INTERFACE ${stb_SOURCE_DIR})
add_library(agr::stb ALIAS agr_stb)

# --- Vulkan backend: headers (Apache-2.0/MIT), volk loader (MIT),
#     VulkanMemoryAllocator (MIT), glslang GLSL -> SPIR-V (BSD/MIT-like). -----
if(AGR_ENABLE_VULKAN)
    set(AGR_VULKAN_SDK_TAG vulkan-sdk-1.4.350.0)
    FetchContent_Declare(VulkanHeaders
        GIT_REPOSITORY https://github.com/KhronosGroup/Vulkan-Headers.git
        GIT_TAG        ${AGR_VULKAN_SDK_TAG}
        GIT_SHALLOW    TRUE)
    FetchContent_MakeAvailable(VulkanHeaders)

    # volk and VMA are compiled into agr_rhi directly: only fetch the sources.
    FetchContent_Declare(volk
        GIT_REPOSITORY https://github.com/zeux/volk.git
        GIT_TAG        ${AGR_VULKAN_SDK_TAG}
        GIT_SHALLOW    TRUE
        SOURCE_SUBDIR  agr-do-not-add)
    FetchContent_Declare(VulkanMemoryAllocator
        GIT_REPOSITORY https://github.com/GPUOpen-LibrariesAndSDKs/VulkanMemoryAllocator.git
        GIT_TAG        v3.4.0
        GIT_SHALLOW    TRUE
        SOURCE_SUBDIR  agr-do-not-add)
    FetchContent_MakeAvailable(volk VulkanMemoryAllocator)

    set(ENABLE_GLSLANG_BINARIES OFF CACHE BOOL "" FORCE)
    set(ENABLE_HLSL OFF CACHE BOOL "" FORCE)
    set(ENABLE_RTTI ON CACHE BOOL "" FORCE)  # UBSan (-fsanitize=vptr) needs typeinfo
    set(ENABLE_OPT OFF CACHE BOOL "" FORCE)
    set(ENABLE_SPVREMAPPER OFF CACHE BOOL "" FORCE)
    set(GLSLANG_TESTS OFF CACHE BOOL "" FORCE)
    set(GLSLANG_ENABLE_INSTALL OFF CACHE BOOL "" FORCE)
    set(BUILD_EXTERNAL OFF CACHE BOOL "" FORCE)
    FetchContent_Declare(glslang
        GIT_REPOSITORY https://github.com/KhronosGroup/glslang.git
        GIT_TAG        ${AGR_VULKAN_SDK_TAG}
        GIT_SHALLOW    TRUE)
    FetchContent_MakeAvailable(glslang)
    # Third-party sources: their warnings are not actionable here and would hide ours.
    foreach(t glslang glslang-default-resource-limits)
        if(TARGET ${t} AND NOT MSVC)
            target_compile_options(${t} PRIVATE -w)
        endif()
    endforeach()
endif()
