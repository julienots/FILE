# AGR editor
#   agr_editor_core : editing logic without UI (projects, selection, undo, play mode,
#                     picking, editor camera) — unit tested.
#   agr_editor_ui   : Dear ImGui panels rendered through the engine RHI.
#   agr_editor      : the application.
add_library(agr_editor_core STATIC
    core/src/Project.cpp
    core/src/SceneTemplates.cpp
    core/src/UndoStack.cpp
    core/src/EditorCamera.cpp
    core/src/Picking.cpp
    core/src/EditorContext.cpp)
target_include_directories(agr_editor_core PUBLIC core/include)
target_link_libraries(agr_editor_core PUBLIC agr::render PRIVATE agr::compiler_options)
add_library(agr::editor_core ALIAS agr_editor_core)
set_target_properties(agr_editor_core PROPERTIES FOLDER "Editor")

add_library(agr_editor_ui STATIC
    ui/src/EditorShared.cpp
    ui/src/Theme.cpp
    ui/src/ImGuiRenderer.cpp
    ui/src/HierarchyPanel.cpp
    ui/src/InspectorPanel.cpp
    ui/src/ViewportPanel.cpp
    ui/src/AssetBrowserPanel.cpp
    ui/src/ConsolePanel.cpp
    ui/src/EditorApp.cpp)
target_include_directories(agr_editor_ui PUBLIC ui/include)
target_link_libraries(agr_editor_ui PUBLIC agr_editor_core agr::platform agr::imgui PRIVATE agr::compiler_options)
set_target_properties(agr_editor_ui PROPERTIES FOLDER "Editor")

add_executable(agr_editor app/main.cpp)
target_link_libraries(agr_editor PRIVATE agr_editor_ui agr::compiler_options)
set_target_properties(agr_editor PROPERTIES FOLDER "Editor")
if(AGR_BUILD_PLUGINS)
    add_dependencies(agr_editor agr_plugin_spin)
endif()
# Fonts and template assets are shipped next to the executable.
add_custom_command(TARGET agr_editor POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_CURRENT_SOURCE_DIR}/resources $<TARGET_FILE_DIR:agr_editor>/editor_resources
    COMMAND ${CMAKE_COMMAND} -E copy_directory ${PROJECT_SOURCE_DIR}/apps/sandbox/assets/textures
            $<TARGET_FILE_DIR:agr_editor>/editor_resources/templates/textures)

# End-to-end editor tests (need a display; CI runs them under xvfb-run).
if(AGR_BUILD_TESTS)
    set(_apis opengl gles)
    if(AGR_ENABLE_VULKAN)
        list(APPEND _apis vulkan)
    endif()
    foreach(api ${_apis})
        add_test(NAME agr_editor_smoke_${api}
                 COMMAND agr_editor --${api} --smoke-test ${CMAKE_BINARY_DIR}/editor_smoke/${api})
        if(api STREQUAL "vulkan")
            set(_env "ASAN_OPTIONS=detect_leaks=0")
        else()
            set(_env "LSAN_OPTIONS=suppressions=${PROJECT_SOURCE_DIR}/tools/lsan.supp")
        endif()
        set_tests_properties(agr_editor_smoke_${api} PROPERTIES SKIP_RETURN_CODE 77 TIMEOUT 300 ENVIRONMENT "${_env}")
    endforeach()
endif()
