if(TARGET SDL3::SDL3-static)
    set(AGR_SDL_TARGET SDL3::SDL3-static)
else()
    set(AGR_SDL_TARGET SDL3::SDL3)
endif()

agr_add_module(platform
    SOURCES
        src/SdlPlatform.cpp
        src/SdlFileProvider.cpp
    PUBLIC_DEPS
        agr::runtime
    PRIVATE_DEPS
        ${AGR_SDL_TARGET})
