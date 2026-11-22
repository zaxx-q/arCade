include("${CMAKE_CURRENT_SOURCE_DIR}/embed_shaders.cmake")
swc_embed_shaders(SWC_SHADER_C)

arcade_add_game(
  ID    synthwave_city
  LOGIC synthwave_city_logic.c swc_city.c swc_traffic.c swc_song.c swc_look.c swc_pack.c swc_shapes.c swc_beams.c swc_ground.c swc_air.c swc_layers.c swc_street.c swc_holo.c swc_raster.c swc_font.c swc_atlas.c
  GAME  synthwave_city_game.c synthwave_city_render_game.c synthwave_city_render_gpu_game.c
        swc_gpu_core_game.c swc_gpu_post_game.c swc_gpu_sky_game.c swc_gpu_buildings_game.c swc_gpu_beams_game.c swc_gpu_ground_game.c swc_gpu_air_game.c swc_gpu_street_game.c swc_gpu_holo_game.c "${SWC_SHADER_C}"
  TESTS tests/test_synthwave_city.c tests/test_swc_city.c tests/test_swc_traffic.c tests/test_swc_song.c
        tests/test_swc_look.c tests/test_swc_pack.c tests/test_swc_shapes.c tests/test_swc_beams.c tests/test_swc_ground.c tests/test_swc_air.c tests/test_swc_layers.c tests/test_swc_street.c tests/test_swc_holo.c tests/test_swc_raster.c tests/test_swc_font.c tests/test_swc_atlas.c)
