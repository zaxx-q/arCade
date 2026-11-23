arcade_add_game(
  ID    synthwave_city
  LOGIC synthwave_city_logic.c swc_city.c swc_traffic.c swc_song.c
  GAME  synthwave_city_game.c synthwave_city_render_game.c
  TESTS tests/test_synthwave_city.c tests/test_swc_city.c tests/test_swc_traffic.c tests/test_swc_song.c)
