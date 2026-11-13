# Gritty Golf = the upstream engine (gg/, vendored: see gg/VENDORED.md) + a thin arCade adapter.
set(GG_ENGINE_SOURCES
  gg/algo.c gg/bmfont.c gg/game.c gg/level.c gg/levels.c gg/mathx.c gg/palette.c
  gg/physics.c gg/player.c gg/raster.c gg/records.c gg/save.c gg/util.c)

arcade_add_game(
  ID    gritty_golf
  LOGIC gritty_golf_logic.c ${GG_ENGINE_SOURCES}
  GAME  gritty_golf_game.c gritty_golf_render_game.c gritty_golf_audio_game.c)

# The engine and arCade both define Vec2 (same layout): reuse arCade's so one file can see both headers.
target_compile_definitions(game_gritty_golf_logic PUBLIC GG_VEC2_FROM_ARCADE)
# The recorded winning runs only replay tick for tick if floats are never fused (a*b+c -> one FMA).
# PUBLIC: the front-end and the tests compile the engine's inline vector helpers too.
if(NOT MSVC)
  target_compile_options(game_gritty_golf_logic PUBLIC -ffp-contract=off)
endif()

# Tests. arcade_add_game(TESTS ...) builds one executable per file and links ONLY the logic library, but the
# engine calls Audio_PlaySfx(), which the real game implements with raylib. These two suites therefore link
# tests/gg/audio_stub.c (counts calls, plays nothing) and are registered by hand with the same naming scheme.
if(ARCADE_BUILD_TESTS)
  file(GLOB GG_ENGINE_TESTS CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/tests/gg/test_*.c")

  # upstream suite: physics, levels, player, rasteriser, save files, tick-exact replays of every level
  add_executable(gtest_gritty_golf_engine ${GG_ENGINE_TESTS} tests/gg/audio_stub.c)
  target_include_directories(gtest_gritty_golf_engine PRIVATE gg tests/gg)
  target_link_libraries(gtest_gritty_golf_engine PRIVATE game_gritty_golf_logic)
  target_compile_definitions(gtest_gritty_golf_engine PRIVATE
    GG_ASSETS_DIR="${CMAKE_SOURCE_DIR}/assets/gritty_golf")
  arcade_apply_warnings(gtest_gritty_golf_engine)
  add_test(NAME gritty_golf.engine COMMAND gtest_gritty_golf_engine)

  # the arCade adapter: input mapping, layout, profile files, monkey test
  add_executable(gtest_gritty_golf_adapter tests/test_gritty_golf.c tests/gg/audio_stub.c)
  target_include_directories(gtest_gritty_golf_adapter PRIVATE gg tests/gg)
  target_link_libraries(gtest_gritty_golf_adapter PRIVATE game_gritty_golf_logic arcade_test)
  arcade_apply_warnings(gtest_gritty_golf_adapter)
  add_test(NAME gritty_golf.adapter COMMAND gtest_gritty_golf_adapter)
endif()
