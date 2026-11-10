# Songs for Beat Dodge (MIDI -> playable charts)

## Do you need 4 lanes like FNF? No.
The Friday Night Funkin' MIDI files contain each character's **melody** (note times and pitches), not the arrow lanes (the lane information lives in FL Studio colour groups, which `.mid` does not store). So lanes are always *derived*: `tools/midi2beatmap` puts low pitches on the left and high pitches on the right and balances the lanes. The lane count is a gameplay choice: easy/normal use 3 lanes, hard uses 4 (`k_preset` in `games/beat_dodge/beat_dodge_convert.c`). Songs support 2-5 lanes; you can press `1`-`4` to jump to a lane.

## What the pipeline does
1. `core/src/midi.c` reads the MIDI (notes per track, first tempo). All FNF files are format 1 at 96 ticks/beat with a constant tempo; ~95-100% of notes sit on the 16th-note grid.
2. `beat_dodge_convert.c` merges the opponent and player ("Boyfriend") tracks, keeps one note per 16th-note step (player wins ties), thins them per difficulty (grid + minimum gap in seconds), maps pitch to lane (optimal contiguous partition, so lanes are used about equally), and keeps the full melody for the music.
3. The song text file (`assets/beatmaps/fnf/<slug>.txt`) holds 3 charts + the melody. The game renders the melody (voices + simple drums) with `core/src/synth.c` into an in-memory WAV and plays it; the notes sound exactly when their obstacles reach the line.

## Add or rebuild songs
```bash
cmake --preset logic && cmake --build --preset logic          # builds build/logic/bin/midi2beatmap
build/logic/bin/midi2beatmap my.mid --report                  # tracks, notes per second per difficulty
build/logic/bin/midi2beatmap my.mid -o assets/beatmaps/my_song.txt --title "My Song"
echo my_song.txt >> assets/beatmaps/songs.txt                 # shown in the menu in this order
bash tools/songs/build_songs.sh                               # rebuild all FNF songs from tools/songs/fnf/*.mid
```
`--player TEXT` selects the player track by part of its name (default: "boyfriend"/"bf", else the last track). Only the first tempo is used (the tool warns about tempo changes). Songs may hold up to 2048 steps and 1536 melody notes (`BD_MAX_*`). `ctest` loads and perfect-plays every song in `songs.txt`.

## Status and next steps
- Verified: parsing, conversion, charts are perfect-playable (unit tests), synth pitch/timing (unit tests), rendering of the select screen and game (headless screenshots).
- NOT verified (no sound device in the build sandbox): actual audio playback and music/obstacle sync. If the music sounds early or late, adjust `BD_AUDIO_OFFSET_S` in `beat_dodge_game.c`.
- Ideas: per-song leaderboard keys, preview of the melody in the select screen, sustained "hold" obstacles from long notes, a song with real audio (`LoadMusicStream` of an .ogg) using the same charts.
