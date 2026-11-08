# Cross-compile the Windows build from Linux (handy to check Windows compiler errors without a Windows PC):
#   sudo apt install gcc-mingw-w64-x86-64-posix g++-mingw-w64-x86-64-posix
#   cmake -S . -B build/win-cross -G Ninja -DCMAKE_TOOLCHAIN_FILE=cmake/toolchains/mingw-w64-x86_64.cmake \
#         -DCMAKE_BUILD_TYPE=Release -DCMAKE_EXE_LINKER_FLAGS=-static
#   cmake --build build/win-cross
# The resulting .exe files cannot be run on Linux (unless you use Wine), but they must COMPILE and LINK.
set(CMAKE_SYSTEM_NAME Windows)
set(CMAKE_SYSTEM_PROCESSOR x86_64)
set(CMAKE_C_COMPILER x86_64-w64-mingw32-gcc)
set(CMAKE_CXX_COMPILER x86_64-w64-mingw32-g++) # raylib's CMake project enables C++ too (our own code is C only)
set(CMAKE_RC_COMPILER x86_64-w64-mingw32-windres)
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
