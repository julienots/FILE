# Common compiler configuration shared by every AGR target.
add_library(agr_compiler_options INTERFACE)
add_library(agr::compiler_options ALIAS agr_compiler_options)

if(MSVC)
    target_compile_options(agr_compiler_options INTERFACE /W4 /permissive- /utf-8 /Zc:__cplusplus /MP)
    target_compile_definitions(agr_compiler_options INTERFACE _CRT_SECURE_NO_WARNINGS NOMINMAX WIN32_LEAN_AND_MEAN)
    if(AGR_WARNINGS_AS_ERRORS)
        target_compile_options(agr_compiler_options INTERFACE /WX)
    endif()
else()
    target_compile_options(agr_compiler_options INTERFACE
        -Wall -Wextra -Wpedantic -Wshadow -Wnon-virtual-dtor -Wcast-align
        -Woverloaded-virtual -Wnull-dereference -Wno-missing-field-initializers)
    if(AGR_WARNINGS_AS_ERRORS)
        target_compile_options(agr_compiler_options INTERFACE -Werror)
    endif()
    if(AGR_ENABLE_ASAN)
        target_compile_options(agr_compiler_options INTERFACE -fsanitize=address,undefined -fno-omit-frame-pointer)
        target_link_options(agr_compiler_options INTERFACE -fsanitize=address,undefined)
    endif()
endif()

target_compile_definitions(agr_compiler_options INTERFACE
    $<$<CONFIG:Debug>:AGR_DEBUG=1>
    AGR_VERSION_MAJOR=${PROJECT_VERSION_MAJOR}
    AGR_VERSION_MINOR=${PROJECT_VERSION_MINOR}
    AGR_VERSION_PATCH=${PROJECT_VERSION_PATCH})
