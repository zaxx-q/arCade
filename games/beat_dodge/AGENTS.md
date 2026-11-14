<!-- Managed by agent: keep sections and order; edit content, not structure. Last updated: 2026-11-14 -->

# AGENTS.md - Beat Dodge (`beat_dodge`)

Scope: this folder only. Repo rules: `../../AGENTS.md`. Game contract: `../AGENTS.md`. Design brief: `GAME.md`. Owner: Member 5 | Ranked

## Overview
Rhythm dodger. Songs (`bd_Song`) have up to 6 charts (easy/normal/hard for Beat Dodge and for Beat Catch, 3-5 lanes) and a melody played by the built-in synth. A chart is made of beads (one note each); beads of fast runs are linked into ribbons, and a ribbon is one object: one life, one score. Songs are text files in `assets/beatmaps/`, listed in `assets/beatmaps/songs.txt`, converted from MIDI by `tools/midi2beatmap` (see `docs/SONGS.md`). Beat Catch (`../beat_catch/`) is a separate game that shares this module.

## Key Files
- `beat_dodge_logic.[ch]`: song format (load/save), song list, menu logic, the shared run (`bd_Run`: clock, lane, arrival), the reachable-lane check, Beat Dodge rules
- `beat_dodge_convert.[ch]`: MIDI -> song (ribbons, thinning, pitch-to-lane DP, repair), chart metrics; used by the tool and tests
- `beat_dodge_ui.h` + `beat_dodge_game.c`: select screen, music playback (WAV in memory) and playfield drawing shared with Beat Catch (`bd_ui_*`, implemented in the `_game.c`, no raylib types in the header), plus the Beat Dodge loop
- `tests/test_beat_dodge.c`, `tests/test_beat_dodge_convert.c`

## Setup & environment
`cmake --preset logic` needs no raylib. Play: `cmake --preset dev && cmake --build --preset dev && build/dev/bin/arCade`.

## Build & tests
- `cmake --build --preset logic && ctest --preset logic -R "^beat_dodge\."`
- Every song in `songs.txt` is loaded and checked (playable, a path a human can walk, no burst without a ribbon, anti-camping mean) by `test_every_shipped_song_is_playable`: regenerate songs with `bash tools/songs/build_songs.sh` after changing the converter.
- Smoke: `build/headless/bin/arCade --smoke beat_dodge --frames 420 --out smoke-out` (also writes `beat_dodge_select.png`)
- Chart measurements: `build/logic/bin/midi2beatmap --metrics assets/beatmaps/fnf/*.txt`

## Code style & conventions
- Time comes from `time`/`step_sec`/`lead`, never from frame counts. Bead `i` arrives at `lead + i * step_sec`; `bd_run_obstacle_y` and the judging must keep using `bd_run_arrival()`.
- Music timeline == game clock: a melody note sounds when its bead reaches the line (`bd_song_to_synth(song, chart, lead, ...)`); notes whose step holds a bead are lead (voice 0), the others quiet backing (voice 3). `beat_dodge_sync` only corrects drifts > 0.15 s. `BD_AUDIO_OFFSET_S` is shared with Beat Catch and stays as it is.
- Rules: every bead is judged when it arrives (same lane = hit); an object costs at most ONE life; an object that ends without a hit scores 10, or 20 for a close call (stood in a lane it touches within `BD_CLOSE_WINDOW_SEC` before its first bead); 50 per life left on a win; no combo. Point values exist only in `award_object` / `award_finish`, and `max_points` replays a perfect run through them, so a perfect run is exactly 1000 x the difficulty multiplier (integer maths).
- Audio is optional: guard with `arcade_audio_ready()`; smoke runs never load audio.
- `bd_song_load` must keep rejecting bad files without touching its output. Every chart must pass the reachable-lane check (`bd_chart_playable`); lone beads keep the preset's minimum gap, beads of one ribbon are at most 0.25 s apart.
- `bd_Run`, the song format, `bd_catch_tolerance` and `bd_chart_playable` are used by Beat Catch too: after changing them run the whole `ctest --preset logic`, and rebuild the songs when the check changes.
- Lanes in converted songs come from pitch (low = left); do not shuffle them.

## Security & safety
- Only read songs from `ctx->assets_dir/beatmaps`; validate everything you parse.

## PR/commit checklist
- [ ] `ctest --preset logic`, `bash tools/format.sh`, `bash tools/check_repo.sh` clean
- [ ] `GAME.md` and `docs/SONGS.md` match the code

## Examples
- Good: `arrive = beat_dodge_arrival(s, i);` for drawing and hit testing.
- Bad: `PlaySound()` without `arcade_audio_ready()`; hard-coding 3 lanes (use `s->lanes`).

## When stuck
Read `docs/SONGS.md`, then `GAME.md` (Implementation steps).
