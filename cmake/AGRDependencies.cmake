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

# --- 3D model import: cgltf (glTF 2.0 / GLB, MIT) and ufbx (FBX, MIT or public domain).
#     Pinned to release commits (cgltf v1.15, ufbx v0.23.1).
FetchContent_Declare(cgltf
    GIT_REPOSITORY https://github.com/jkuhlmann/cgltf.git
    GIT_TAG        360db1a95480fe102ae9c69b27c5d101167ff5ba
    SOURCE_SUBDIR  agr-do-not-add)
FetchContent_Declare(ufbx
    GIT_REPOSITORY https://github.com/ufbx/ufbx.git
    GIT_TAG        26a482ae66871d7de36eb722aa060bce95bce274
    SOURCE_SUBDIR  agr-do-not-add)
FetchContent_MakeAvailable(cgltf ufbx)
add_library(agr_cgltf INTERFACE)  # header-only; the implementation is compiled in agr::assets
target_include_directories(agr_cgltf SYSTEM INTERFACE ${cgltf_SOURCE_DIR})
add_library(agr::cgltf ALIAS agr_cgltf)
add_library(agr_ufbx STATIC ${ufbx_SOURCE_DIR}/ufbx.c)
target_include_directories(agr_ufbx SYSTEM PUBLIC ${ufbx_SOURCE_DIR})
set_target_properties(agr_ufbx PROPERTIES POSITION_INDEPENDENT_CODE ON FOLDER "ThirdParty")
if(NOT MSVC)
    target_compile_options(agr_ufbx PRIVATE -w)  # third-party code: not held to the engine warning level
endif()
add_library(agr::ufbx ALIAS agr_ufbx)

# --- 2D physics: Box2D v3.1.1 (MIT).
set(BOX2D_SAMPLES OFF CACHE BOOL "" FORCE)
set(BOX2D_BENCHMARKS OFF CACHE BOOL "" FORCE)
set(BOX2D_DOCS OFF CACHE BOOL "" FORCE)
set(BOX2D_UNIT_TESTS OFF CACHE BOOL "" FORCE)
set(BOX2D_VALIDATE OFF CACHE BOOL "" FORCE)
set(BOX2D_PROFILE OFF CACHE BOOL "" FORCE)
FetchContent_Declare(box2d
    GIT_REPOSITORY https://github.com/erincatto/box2d.git
    GIT_TAG        8c661469c9507d3ad6fbd2fea3f1aa71669c2fe3)
FetchContent_MakeAvailable(box2d)
set_target_properties(box2d PROPERTIES FOLDER "ThirdParty" POSITION_INDEPENDENT_CODE ON)

# --- 3D physics: Jolt Physics v5.6.0 (MIT). Portable instruction set (SSE4.2 on x86, NEON on ARM),
#     no debug renderer / profiler / GPU compute, RTTI on (sanitizer builds check virtual calls).
foreach(_opt USE_AVX USE_AVX2 USE_AVX512 USE_LZCNT USE_TZCNT USE_F16C USE_FMADD INTERPROCEDURAL_OPTIMIZATION
        FLOATING_POINT_EXCEPTIONS_ENABLED ENABLE_ALL_WARNINGS DEBUG_RENDERER_IN_DEBUG_AND_RELEASE PROFILER_IN_DEBUG_AND_RELEASE
        ENABLE_OBJECT_STREAM ENABLE_INSTALL JPH_USE_DX12 JPH_USE_VK JPH_USE_MTL JPH_USE_CPU_COMPUTE OVERRIDE_CXX_FLAGS
        TARGET_UNIT_TESTS TARGET_HELLO_WORLD TARGET_PERFORMANCE_TEST TARGET_SAMPLES TARGET_VIEWER GENERATE_DEBUG_SYMBOLS)
    set(${_opt} OFF CACHE BOOL "" FORCE)
endforeach()
set(CPP_RTTI_ENABLED ON CACHE BOOL "" FORCE)
FetchContent_Declare(JoltPhysics
    GIT_REPOSITORY https://github.com/jrouwe/JoltPhysics.git
    GIT_TAG        e77f175595e64cb44218cc9d9d56fc365ad0e36a
    GIT_SHALLOW    FALSE
    SOURCE_SUBDIR  Build)
FetchContent_MakeAvailable(JoltPhysics)
set_target_properties(Jolt PROPERTIES FOLDER "ThirdParty" POSITION_INDEPENDENT_CODE ON)

# --- Scripting: Lua 5.4.9 (MIT), compiled from the official sources (interpreter and tests excluded).
FetchContent_Declare(lua
    GIT_REPOSITORY https://github.com/lua/lua.git
    GIT_TAG        312b9efaa1061c2c4cad08554dbc1351c3270eef
    SOURCE_SUBDIR  agr-do-not-add)
FetchContent_MakeAvailable(lua)
set(_lua_sources lapi lauxlib lbaselib lcode lcorolib lctype ldblib ldebug ldo ldump lfunc lgc linit liolib llex lmathlib
    lmem loadlib lobject lopcodes loslib lparser lstate lstring lstrlib ltable ltablib ltm lundump lutf8lib lvm lzio)
list(TRANSFORM _lua_sources PREPEND ${lua_SOURCE_DIR}/)
list(TRANSFORM _lua_sources APPEND .c)
# Compiled as C++: Lua errors then unwind with exceptions, so C++ objects in engine bindings that raise
# Lua errors are destroyed properly (with C, longjmp would skip their destructors).
set_source_files_properties(${_lua_sources} PROPERTIES LANGUAGE CXX)
add_library(agr_lua STATIC ${_lua_sources})
target_include_directories(agr_lua SYSTEM PUBLIC ${lua_SOURCE_DIR})
set_target_properties(agr_lua PROPERTIES FOLDER "ThirdParty" POSITION_INDEPENDENT_CODE ON LINKER_LANGUAGE CXX)
if(NOT MSVC)
    target_compile_options(agr_lua PRIVATE -w)
endif()
if(CMAKE_SYSTEM_NAME MATCHES "Linux|Android|Darwin")
    target_compile_definitions(agr_lua PRIVATE LUA_USE_POSIX)  # mkstemp etc.; dynamic C modules stay disabled
endif()
add_library(agr::lua ALIAS agr_lua)

# --- Audio: miniaudio 0.11.25 (public domain / MIT-0) with stb_vorbis for Ogg Vorbis.
#     Built from the single-file sources; the resource manager is not used (clips are decoded by AGR).
FetchContent_Declare(miniaudio
    GIT_REPOSITORY https://github.com/mackron/miniaudio.git
    GIT_TAG        9634bedb5b5a2ca38c1ee7108a9358a4e233f14d
    SOURCE_SUBDIR  agr-do-not-add)
FetchContent_MakeAvailable(miniaudio)
file(CONFIGURE OUTPUT ${CMAKE_BINARY_DIR}/agr_miniaudio/miniaudio_impl.c CONTENT [=[
#define STB_VORBIS_HEADER_ONLY
#include "extras/stb_vorbis.c"
#define MINIAUDIO_IMPLEMENTATION
#include "miniaudio.h"
#undef STB_VORBIS_HEADER_ONLY
#include "extras/stb_vorbis.c"
]=])
add_library(agr_miniaudio STATIC ${CMAKE_BINARY_DIR}/agr_miniaudio/miniaudio_impl.c)
target_include_directories(agr_miniaudio SYSTEM PUBLIC ${miniaudio_SOURCE_DIR})
# Must be identical in every translation unit that includes miniaudio.h (structure layouts).
target_compile_definitions(agr_miniaudio PUBLIC MA_NO_RESOURCE_MANAGER MA_NO_ENCODING)
find_package(Threads REQUIRED)
target_link_libraries(agr_miniaudio PUBLIC Threads::Threads ${CMAKE_DL_LIBS})
if(UNIX AND NOT APPLE)
    target_link_libraries(agr_miniaudio PUBLIC m)
endif()
if(NOT MSVC)
    target_compile_options(agr_miniaudio PRIVATE -w)
endif()
set_target_properties(agr_miniaudio PROPERTIES FOLDER "ThirdParty" POSITION_INDEPENDENT_CODE ON)
add_library(agr::miniaudio ALIAS agr_miniaudio)

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

# --- Editor UI: Dear ImGui docking branch (MIT) + ImGuizmo gizmos (MIT) --------
if(AGR_BUILD_EDITOR)
    FetchContent_Declare(imgui
        GIT_REPOSITORY https://github.com/ocornut/imgui.git
        GIT_TAG        v1.92.9-docking
        GIT_SHALLOW    TRUE
        SOURCE_SUBDIR  agr-do-not-add)
    FetchContent_Declare(imguizmo
        GIT_REPOSITORY https://github.com/CedricGuillemet/ImGuizmo.git
        GIT_TAG        18cef5e031d8c6973d80284c67f60549fafd78c1
        SOURCE_SUBDIR  agr-do-not-add)
    FetchContent_MakeAvailable(imgui imguizmo)

    add_library(agr_imgui STATIC
        ${imgui_SOURCE_DIR}/imgui.cpp
        ${imgui_SOURCE_DIR}/imgui_draw.cpp
        ${imgui_SOURCE_DIR}/imgui_tables.cpp
        ${imgui_SOURCE_DIR}/imgui_widgets.cpp
        ${imgui_SOURCE_DIR}/imgui_demo.cpp
        ${imgui_SOURCE_DIR}/misc/cpp/imgui_stdlib.cpp
        ${imgui_SOURCE_DIR}/backends/imgui_impl_sdl3.cpp
        ${imguizmo_SOURCE_DIR}/src/ImGuizmo.cpp)
    target_include_directories(agr_imgui SYSTEM PUBLIC
        ${imgui_SOURCE_DIR} ${imgui_SOURCE_DIR}/backends ${imgui_SOURCE_DIR}/misc/cpp ${imguizmo_SOURCE_DIR}/src)
    target_compile_definitions(agr_imgui PUBLIC IMGUI_DISABLE_OBSOLETE_FUNCTIONS IMGUI_DEFINE_MATH_OPERATORS)
    if(TARGET SDL3::SDL3-static)
        target_link_libraries(agr_imgui PUBLIC SDL3::SDL3-static)
    else()
        target_link_libraries(agr_imgui PUBLIC SDL3::SDL3)
    endif()
    if(NOT MSVC)
        target_compile_options(agr_imgui PRIVATE -w)
    endif()
    add_library(agr::imgui ALIAS agr_imgui)
endif()
