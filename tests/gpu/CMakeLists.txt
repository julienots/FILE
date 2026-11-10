# GPU tests: need a display (X11 / Wayland / Windows desktop). In CI they run under xvfb-run.
add_executable(agr_gpu_tests main.cpp test_rhi.cpp test_renderer.cpp test_renderer2d.cpp test_renderer3d.cpp)
target_link_libraries(agr_gpu_tests PRIVATE agr::platform agr::render agr::twod agr::compiler_options doctest::doctest)
set_target_properties(agr_gpu_tests PROPERTIES FOLDER "Tests")
add_test(NAME agr_gpu_tests_gl COMMAND agr_gpu_tests)
add_test(NAME agr_gpu_tests_gles COMMAND agr_gpu_tests)
set(_lsan "LSAN_OPTIONS=suppressions=${PROJECT_SOURCE_DIR}/tools/lsan.supp")
set_tests_properties(agr_gpu_tests_gl PROPERTIES ENVIRONMENT "${_lsan}")
set_tests_properties(agr_gpu_tests_gles PROPERTIES ENVIRONMENT "AGR_TEST_API=gles;${_lsan}")
if(AGR_ENABLE_VULKAN)
    add_test(NAME agr_gpu_tests_vulkan COMMAND agr_gpu_tests)
    # lavapipe's worker threads keep two small allocations (no engine frame in their
    # stacks): leak detection is off for this suite only; ASan / UBSan stay active.
    set_tests_properties(agr_gpu_tests_vulkan PROPERTIES ENVIRONMENT "AGR_TEST_API=vulkan;ASAN_OPTIONS=detect_leaks=0")
endif()
