add_executable(agr_benchmarks main.cpp)
target_link_libraries(agr_benchmarks PRIVATE agr::scene agr::compiler_options)
set_target_properties(agr_benchmarks PROPERTIES FOLDER "Benchmarks")
