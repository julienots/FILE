add_executable(agr_tests
    main.cpp
    test_core.cpp
    test_jobs.cpp
    test_math.cpp
    test_ecs.cpp
    test_scene.cpp
    test_runtime.cpp
    test_assets.cpp
    test_render.cpp
    test_shader_dialect.cpp
    test_twod.cpp
    test_models.cpp
    test_environment.cpp
    test_prefab.cpp
    test_input_actions.cpp
    test_localization.cpp
    test_custom_shader.cpp
    test_net.cpp
    test_navigation.cpp
    test_terrain.cpp
    test_physics2d.cpp
    test_script.cpp
    test_physics3d.cpp
    test_audio.cpp
    test_ui.cpp
    test_animation.cpp)
target_link_libraries(agr_tests PRIVATE agr::animation agr::ui agr::audio agr::physics3d agr::navigation agr::net agr::script agr::physics2d agr::twod agr::render agr::compiler_options doctest::doctest)
if(AGR_BUILD_PLUGINS)
    add_dependencies(agr_tests agr_plugin_spin)
    target_compile_definitions(agr_tests PRIVATE AGR_TEST_PLUGIN_PATH="$<TARGET_FILE:agr_plugin_spin>")
endif()
target_compile_definitions(agr_tests PRIVATE AGR_TEST_FONT="${PROJECT_SOURCE_DIR}/editor/resources/fonts/Inter-Regular.ttf")
set_target_properties(agr_tests PROPERTIES FOLDER "Tests")
add_test(NAME agr_tests COMMAND agr_tests WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})
set_tests_properties(agr_tests PROPERTIES ENVIRONMENT "LSAN_OPTIONS=suppressions=${PROJECT_SOURCE_DIR}/tools/lsan.supp")

if(AGR_BUILD_EDITOR)
    target_sources(agr_tests PRIVATE test_editor.cpp test_game_templates.cpp)
    target_link_libraries(agr_tests PRIVATE agr::editor_core)
    target_compile_definitions(agr_tests PRIVATE AGR_TEST_TEMPLATE_ASSETS="${PROJECT_SOURCE_DIR}/editor/resources/templates/assets")
endif()
