agr_add_module(net
    SOURCES
        src/NetSession.cpp
        src/NetPlugin.cpp
    PUBLIC_DEPS
        agr::runtime)
if(WIN32)
    target_link_libraries(agr_net PRIVATE ws2_32)
endif()
