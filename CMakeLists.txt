cmake_minimum_required(VERSION 3.22) # raylib 6.2 itself requires 3.22
project(arCade LANGUAGES C)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake")

# ---- Options ---------------------------------------------------------------
option(ARCADE_LOGIC_ONLY  "Build only raylib-free code: core, game logic, tests (fast, no downloads)" OFF)
option(ARCADE_HEADLESS    "Build raylib with the CPU 'Memory' platform: no window, no GPU (CI smoke tests)" OFF)
option(ARCADE_BUILD_TESTS "Build unit tests" ON)
option(ARCADE_WERROR      "Treat compiler warnings as errors" OFF)
option(ARCADE_SANITIZE    "Enable AddressSanitizer + UBSan (GCC/Clang only)" OFF)
set(ARCADE_VERSION "" CACHE STRING "Version baked into the binary, e.g. 1.2.3 (CI passes this; empty = derive from git)")
set(ARCADE_RAYLIB_TAG "6.2" CACHE STRING "raylib git tag to fetch (see docs/raylib_subset.md before bumping)")

# ---- Language rules: strict portable C99 ----------------------------------
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF) # -std=c99, not gnu99: keeps us honest about portability
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")

include(ArcadeVersion)
include(ArcadeWarnings)
include(ArcadeGame)
include(ArcadeRegistry)

if(ARCADE_BUILD_TESTS)
  enable_testing()
endif()

# ---- Targets ---------------------------------------------------------------
add_subdirectory(core)

if(ARCADE_BUILD_TESTS)
  add_library(arcade_test INTERFACE)
  target_include_directories(arcade_test INTERFACE "${CMAKE_SOURCE_DIR}/tests")
endif()

if(NOT ARCADE_LOGIC_ONLY)
  include(ArcadeRaylib) # fetches + builds raylib (needs Internet on first configure)
endif()

add_subdirectory(games)
add_subdirectory(tools)

if(ARCADE_BUILD_TESTS)
  add_subdirectory(tests)
endif()

if(NOT ARCADE_LOGIC_ONLY)
  arcade_generate_registry()
  add_subdirectory(app)
endif()

# ---- Summary ---------------------------------------------------------------
get_property(_ids GLOBAL PROPERTY ARCADE_GAME_IDS)
message(STATUS "arCade ${ARCADE_VERSION_RESOLVED} | logic-only=${ARCADE_LOGIC_ONLY} headless=${ARCADE_HEADLESS} werror=${ARCADE_WERROR} sanitize=${ARCADE_SANITIZE}")
message(STATUS "Games: ${_ids}")
