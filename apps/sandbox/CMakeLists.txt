add_executable(agr_sandbox main.cpp)
target_link_libraries(agr_sandbox PRIVATE agr::platform agr::render agr::compiler_options)
set_target_properties(agr_sandbox PROPERTIES FOLDER "Apps")
if(AGR_BUILD_PLUGINS)
    add_dependencies(agr_sandbox agr_plugin_spin)
endif()
# Ship the demo assets (shared with the editor templates) next to the executable (mounted as assets://).
add_custom_command(TARGET agr_sandbox POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory ${PROJECT_SOURCE_DIR}/editor/resources/templates/assets
            $<TARGET_FILE_DIR:agr_sandbox>/assets)
