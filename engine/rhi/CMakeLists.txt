agr_add_module(rhi
    SOURCES
        src/Rhi.cpp
        src/gl/GLDevice.cpp
    PUBLIC_DEPS
        agr::core
        agr::math)
