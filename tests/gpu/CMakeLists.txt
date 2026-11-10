# GPU tests: need a display (X11 / Wayland / Windows desktop). In CI they run under xvfb-run.
add_executable(agr_gpu_tests main.cpp test_rhi.cpp test_renderer.cpp)
target_link_libraries(agr_gpu_tests PRIVATE agr::platform agr::render agr::compiler_options doctest::doctest)
set_target_properties(agr_gpu_tests PROPERTIES FOLDER "Tests")
add_test(NAME agr_gpu_tests_gl COMMAND agr_gpu_tests)
add_test(NAME agr_gpu_tests_gles COMMAND agr_gpu_tests)
set_tests_properties(agr_gpu_tests_gles PROPERTIES ENVIRONMENT "AGR_TEST_GLES=1")
