find_package(Threads REQUIRED)
agr_add_module(core
    SOURCES
        src/Log.cpp
        src/Assert.cpp
        src/Memory.cpp
        src/JobSystem.cpp
        src/FileSystem.cpp
        src/SharedLibrary.cpp
        src/Uuid.cpp
    PUBLIC_DEPS
        Threads::Threads
        ${CMAKE_DL_LIBS})
