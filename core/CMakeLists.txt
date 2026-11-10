# arcade_core: pure C (no raylib). Everything in here is unit-testable on any machine.
add_library(arcade_core STATIC
  src/rng.c src/str.c src/input.c src/leaderboard.c src/sortlib.c src/sandbox.c src/fx.c src/cli.c src/synth.c src/midi.c)
target_include_directories(arcade_core PUBLIC
  "${CMAKE_CURRENT_SOURCE_DIR}/include"
  "${CMAKE_BINARY_DIR}/generated") # generated arcade/version.h
if(NOT MSVC)
  target_link_libraries(arcade_core PUBLIC m)
endif()
arcade_apply_warnings(arcade_core)

# arcade_gfx: raylib front-end helpers (window, frame loop, input polling, drawing, hub UI).
if(NOT ARCADE_LOGIC_ONLY)
  add_library(arcade_gfx STATIC gfx/gfx.c gfx/gfx_draw.c gfx/gfx_text.c)
  target_include_directories(arcade_gfx PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include")
  target_link_libraries(arcade_gfx PUBLIC arcade_core raylib)
  if(ARCADE_HEADLESS)
    target_compile_definitions(arcade_gfx PUBLIC ARCADE_HEADLESS_BUILD=1)
  endif()
  arcade_apply_warnings(arcade_gfx)
endif()
