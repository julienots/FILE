# Cross-compilation for Windows x64 from Linux with MinGW-w64 (GCC, POSIX threads).
# Debian / Ubuntu: apt install mingw-w64. Used by the "windows-x64" preset.
set(CMAKE_SYSTEM_NAME Windows)
set(CMAKE_SYSTEM_PROCESSOR x86_64)
set(AGR_MINGW_PREFIX x86_64-w64-mingw32)
set(CMAKE_C_COMPILER   ${AGR_MINGW_PREFIX}-gcc-posix)
set(CMAKE_CXX_COMPILER ${AGR_MINGW_PREFIX}-g++-posix)
set(CMAKE_RC_COMPILER  ${AGR_MINGW_PREFIX}-windres)
set(CMAKE_FIND_ROOT_PATH /usr/${AGR_MINGW_PREFIX})
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)
# A single self-contained .exe: no libstdc++ / libgcc / winpthread DLLs to ship.
set(CMAKE_EXE_LINKER_FLAGS_INIT "-static -static-libgcc -static-libstdc++")
set(CMAKE_SHARED_LINKER_FLAGS_INIT "-static-libgcc -static-libstdc++")
