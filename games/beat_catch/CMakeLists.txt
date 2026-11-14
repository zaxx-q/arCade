arcade_add_game(
  ID    beat_catch
  LOGIC beat_catch_logic.c
  GAME  beat_catch_game.c
  TESTS tests/test_beat_catch.c)

# Beat Catch shares the song format, the chart check, the clock and the select screen with Beat Dodge.
# Link the OTHER game's libraries; never list beat_dodge_logic.c here too (duplicate symbols at link time).
target_link_libraries(game_beat_catch_logic PUBLIC game_beat_dodge_logic)
if(NOT ARCADE_LOGIC_ONLY)
  target_link_libraries(game_beat_catch PUBLIC game_beat_dodge)
endif()
