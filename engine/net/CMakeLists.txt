agr_add_module(net
    SOURCES
        src/NetSession.cpp
        src/NetPlugin.cpp
        src/Replication.cpp
        src/Http.cpp
        src/Sockets.cpp
    PUBLIC_DEPS
        agr::runtime)
target_link_libraries(agr_net PRIVATE mbedtls mbedx509 mbedcrypto)
if(WIN32)
    target_link_libraries(agr_net PRIVATE ws2_32 crypt32 bcrypt)
endif()
