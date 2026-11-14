arcade_add_game(
  ID    beat_dodge
  LOGIC beat_dodge_logic.c beat_dodge_convert.c beat_catch_logic.c
  GAME  beat_dodge_game.c beat_catch_game.c
  TESTS tests/test_beat_dodge.c tests/test_beat_dodge_convert.c tests/test_beat_catch.c)
