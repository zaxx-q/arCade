arcade_add_game(
  ID    beat_dodge
  LOGIC beat_dodge_logic.c beat_dodge_convert.c
  GAME  beat_dodge_game.c
  TESTS tests/test_beat_dodge.c tests/test_beat_dodge_convert.c)
