# Fetches and builds raylib. Pinned by tag; override the tag with -DARCADE_RAYLIB_TAG=...
# Offline / local copy:  -DFETCHCONTENT_SOURCE_DIR_RAYLIB=/path/to/raylib
include(FetchContent)

set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
if(ARCADE_HEADLESS)
  # raylib >= 6.0: CPU software renderer rendering into a memory framebuffer. No window, no GPU.
  set(PLATFORM "Memory" CACHE STRING "" FORCE)
elseif(ANDROID)
  # raylib's own Android platform (NativeActivity + OpenGL ES 2). It reads ANDROID_NDK to compile the NDK's
  # native_app_glue into libraylib.a, so the variable must be known here.
  # ANDROID_NDK and ARCADE_ANDROID_GLUE_DIR are resolved early in the root CMakeLists.txt (core needs it).
  if(NOT EXISTS "${ANDROID_NDK}/sources/android/native_app_glue/android_native_app_glue.c")
    message(FATAL_ERROR "Android build: the NDK was not found (ANDROID_NDK='${ANDROID_NDK}'). Set ANDROID_NDK_HOME to an NDK r28 or newer, see docs/ANDROID.md")
  endif()
  set(PLATFORM "Android" CACHE STRING "" FORCE)
else()
  set(PLATFORM "Desktop" CACHE STRING "" FORCE)
endif()

FetchContent_Declare(raylib
  GIT_REPOSITORY https://github.com/raysan5/raylib.git
  GIT_TAG        ${ARCADE_RAYLIB_TAG}
  GIT_SHALLOW    TRUE)
FetchContent_MakeAvailable(raylib)
