# =============================================================================
#  AGR ENGINE — Build Beyond Limits.
#  Superessence Studio
# =============================================================================
cmake_minimum_required(VERSION 3.24)

project(AGREngine
    VERSION 0.1.0
    DESCRIPTION "AGR ENGINE - modular, mobile-first game engine by Superessence Studio"
    LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

# ---------------------------------------------------------------------------
# Options
# ---------------------------------------------------------------------------
option(AGR_BUILD_PLATFORM   "Build the SDL3 platform layer (windowing / input)" ON)
option(AGR_BUILD_TESTS      "Build unit tests"                                 ON)
option(AGR_BUILD_BENCHMARKS "Build benchmarks"                                 ON)
option(AGR_BUILD_SANDBOX    "Build the sandbox runtime application"            ON)
option(AGR_BUILD_PLUGINS    "Build bundled example plugins"                    ON)
option(AGR_BUILD_EDITOR     "Build the AGR editor (needs the platform layer)"  ON)
option(AGR_WARNINGS_AS_ERRORS "Treat compiler warnings as errors"              OFF)
option(AGR_ENABLE_ASAN      "Enable AddressSanitizer + UBSan (GCC/Clang)"      OFF)
option(AGR_ENABLE_VULKAN    "Build the Vulkan backend of the RHI"              ON)

if(NOT AGR_BUILD_PLATFORM)
    set(AGR_BUILD_SANDBOX OFF CACHE BOOL "" FORCE)
    set(AGR_BUILD_EDITOR OFF CACHE BOOL "" FORCE)
endif()

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE RelWithDebInfo CACHE STRING "Build type" FORCE)
endif()

# Put every binary (exe, shared libs, plugins) in one folder so that the runtime
# finds plugins next to the executable on every OS.
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib")

if(AGR_BUILD_TESTS)
    enable_testing()  # before any add_subdirectory that registers tests (editor smoke tests)
endif()

include(AGRCompilerOptions)
include(AGRDependencies)
include(AGRModule)

# ---------------------------------------------------------------------------
# Engine modules (dependency order)
# ---------------------------------------------------------------------------
add_subdirectory(engine/core)
add_subdirectory(engine/math)
add_subdirectory(engine/ecs)
add_subdirectory(engine/scene)
add_subdirectory(engine/rhi)
add_subdirectory(engine/assets)
if(AGR_BUILD_PLATFORM)
    add_subdirectory(engine/platform)
endif()
add_subdirectory(engine/runtime)
add_subdirectory(engine/render)
add_subdirectory(engine/twod)
add_subdirectory(engine/physics2d)
add_subdirectory(engine/physics3d)
add_subdirectory(engine/script)

if(AGR_BUILD_PLUGINS)
    add_subdirectory(plugins/spin)
endif()
if(AGR_BUILD_SANDBOX)
    add_subdirectory(apps/sandbox)
endif()
if(AGR_BUILD_EDITOR)
    add_subdirectory(editor)
endif()
if(AGR_BUILD_TESTS)
    add_subdirectory(tests)
    if(AGR_BUILD_PLATFORM)
        add_subdirectory(tests/gpu)
    endif()
endif()
if(AGR_BUILD_BENCHMARKS)
    add_subdirectory(benchmarks)
endif()

message(STATUS "")
message(STATUS "AGR ENGINE ${PROJECT_VERSION} — Build Beyond Limits.")
message(STATUS "  Platform layer : ${AGR_BUILD_PLATFORM}")
message(STATUS "  Vulkan backend : ${AGR_ENABLE_VULKAN}")
message(STATUS "  Tests          : ${AGR_BUILD_TESTS}")
message(STATUS "  Benchmarks     : ${AGR_BUILD_BENCHMARKS}")
message(STATUS "  Sandbox        : ${AGR_BUILD_SANDBOX}")
message(STATUS "  Editor         : ${AGR_BUILD_EDITOR}")
message(STATUS "")
