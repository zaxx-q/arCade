# Fetches and builds raylib. Pinned by tag; override the tag with -DARCADE_RAYLIB_TAG=...
# Offline / local copy:  -DFETCHCONTENT_SOURCE_DIR_RAYLIB=/path/to/raylib
include(FetchContent)

set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
if(ARCADE_HEADLESS)
  # raylib >= 6.0: CPU software renderer rendering into a memory framebuffer. No window, no GPU.
  set(PLATFORM "Memory" CACHE STRING "" FORCE)
else()
  set(PLATFORM "Desktop" CACHE STRING "" FORCE)
endif()

FetchContent_Declare(raylib
  GIT_REPOSITORY https://github.com/raysan5/raylib.git
  GIT_TAG        ${ARCADE_RAYLIB_TAG}
  GIT_SHALLOW    TRUE)
FetchContent_MakeAvailable(raylib)
