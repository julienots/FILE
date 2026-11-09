add_executable(agr_tests
    main.cpp
    test_core.cpp
    test_jobs.cpp
    test_math.cpp
    test_ecs.cpp
    test_scene.cpp
    test_runtime.cpp)
target_link_libraries(agr_tests PRIVATE agr::runtime agr::compiler_options doctest::doctest)
if(AGR_BUILD_PLUGINS)
    add_dependencies(agr_tests agr_plugin_spin)
    target_compile_definitions(agr_tests PRIVATE AGR_TEST_PLUGIN_PATH="$<TARGET_FILE:agr_plugin_spin>")
endif()
set_target_properties(agr_tests PROPERTIES FOLDER "Tests")
add_test(NAME agr_tests COMMAND agr_tests WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})
