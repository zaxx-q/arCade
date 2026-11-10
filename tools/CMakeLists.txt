# midi2beatmap: converts MIDI files into Beat Dodge songs (pure C, shares code with the game).
if(TARGET game_beat_dodge_logic)
  add_executable(midi2beatmap midi2beatmap.c)
  target_link_libraries(midi2beatmap PRIVATE game_beat_dodge_logic arcade_core)
  arcade_apply_warnings(midi2beatmap)
endif()
