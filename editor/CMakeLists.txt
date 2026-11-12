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
    core/src/EditorContext.cpp
    core/src/GameExport.cpp
    core/src/AndroidExport.cpp
    core/src/games/Platformer.cpp
    core/src/games/Adventure25D.cpp
    core/src/games/Arena.cpp
    core/src/games/Racing.cpp
    core/src/games/Strategy.cpp
    core/src/games/Rpg3D.cpp
    core/src/games/Survival.cpp
    core/src/games/Tycoon.cpp
    core/src/games/Idle.cpp
    core/src/games/Farm.cpp
    core/src/games/Cards.cpp
    core/src/games/TowerDefense.cpp
    core/src/games/Rpg2D.cpp)
target_include_directories(agr_editor_core PUBLIC core/include)
target_link_libraries(agr_editor_core PUBLIC agr::render agr::twod agr::script agr::physics2d agr::physics3d agr::navigation agr::audio agr::ui agr::animation agr::assets PRIVATE agr::compiler_options)
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
    ui/src/MaterialPreview.cpp
    ui/src/ConsolePanel.cpp
    ui/src/MixerPanel.cpp
    ui/src/InputPanel.cpp
    ui/src/LocalizationPanel.cpp
    ui/src/ShaderEditorPanel.cpp
    ui/src/TouchSupport.cpp
    ui/src/AnimationPanel.cpp
    ui/src/StateMachinePanel.cpp
    ui/src/ExportPanel.cpp
    ui/src/EditorApp.cpp)
target_include_directories(agr_editor_ui PUBLIC ui/include)
target_link_libraries(agr_editor_ui PUBLIC agr_editor_core agr::platform agr::imgui PRIVATE agr::compiler_options)
set_target_properties(agr_editor_ui PROPERTIES FOLDER "Editor")

if(ANDROID)
    # On Android the editor is the native library (libmain.so) loaded by the activity; its
    # resources travel in the APK and are extracted on first launch (app/main.cpp).
    add_library(agr_editor SHARED app/main.cpp)
    set_target_properties(agr_editor PROPERTIES OUTPUT_NAME main LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/editor_lib)
    target_link_libraries(agr_editor PRIVATE agr_editor_ui agr::compiler_options android log)
else()
    add_executable(agr_editor app/main.cpp)
    target_link_libraries(agr_editor PRIVATE agr_editor_ui agr::compiler_options)
endif()
set_target_properties(agr_editor PROPERTIES FOLDER "Editor")
add_dependencies(agr_editor agr_player)  # desktop export copies the player next to the game
if(AGR_BUILD_PLUGINS)
    add_dependencies(agr_editor agr_plugin_spin)
endif()
# Fonts and template assets are shipped next to the executable.
if(NOT ANDROID)
add_custom_command(TARGET agr_editor POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_CURRENT_SOURCE_DIR}/resources $<TARGET_FILE_DIR:agr_editor>/editor_resources)
endif()

# End-to-end editor tests (need a display; CI runs them under xvfb-run).
if(AGR_BUILD_TESTS)
    # Command-line export of a template project, then the exported game runs in the player.
    set(_export ${CMAKE_BINARY_DIR}/player_test)
    add_test(NAME agr_export_desktop
             COMMAND ${CMAKE_COMMAND} -DEDITOR=$<TARGET_FILE:agr_editor> -DOUT=${_export}
                     -P ${PROJECT_SOURCE_DIR}/cmake/ExportTest.cmake)
    set_tests_properties(agr_export_desktop PROPERTIES FIXTURES_SETUP agr_exported_game TIMEOUT 120)
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
        add_test(NAME agr_player_${api}
                 COMMAND ${_export}/out/Showcase/Showcase --${api} --frames 240 --autotest --user ${_export}/user_${api}
                         --screenshot user://player.png)
        set_tests_properties(agr_player_${api} PROPERTIES FIXTURES_REQUIRED agr_exported_game TIMEOUT 120 ENVIRONMENT "${_env}")
    endforeach()
endif()
