# agr_add_module(<name> [SOURCES ...] [PUBLIC_DEPS ...] [PRIVATE_DEPS ...] [HEADER_ONLY])
#
# Declares an engine module named agr_<name> with alias agr::<name>.
# Layout convention:
#   engine/<name>/include/agr/<name>/*.hpp   public API
#   engine/<name>/src/*.cpp                  implementation
function(agr_add_module NAME)
    cmake_parse_arguments(ARG "HEADER_ONLY" "" "SOURCES;PUBLIC_DEPS;PRIVATE_DEPS" ${ARGN})
    set(target agr_${NAME})
    if(ARG_HEADER_ONLY)
        add_library(${target} INTERFACE)
        target_include_directories(${target} INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/include)
        target_link_libraries(${target} INTERFACE ${ARG_PUBLIC_DEPS})
    else()
        add_library(${target} STATIC ${ARG_SOURCES})
        target_include_directories(${target} PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include
                                             PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
        target_link_libraries(${target} PUBLIC ${ARG_PUBLIC_DEPS}
                                        PRIVATE agr::compiler_options ${ARG_PRIVATE_DEPS})
    endif()
    add_library(agr::${NAME} ALIAS ${target})
    set_target_properties(${target} PROPERTIES FOLDER "Engine")
endfunction()
