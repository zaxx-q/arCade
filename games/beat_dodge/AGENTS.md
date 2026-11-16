<!-- Managed by agent: keep sections and order; edit content, not structure. Last updated: 2026-11-14 -->

# AGENTS.md - Beat Dodge & Catch (`beat_dodge`)

Scope: this folder only. Repo rules: `../../AGENTS.md`. Game contract: `../AGENTS.md`. Design brief: `GAME.md`. Owner: Member 5 | Ranked

## Overview
One rhythm game with two modes chosen by the big DODGE / CATCH buttons on the song select screen (click, or the M key): **Dodge** (stay out of the notes' lane) and **Catch** (be in it). One `GameDef` (`beat_dodge`) but one high-score table per mode, song and difficulty (see the "Boards" convention below). Songs (`bd_Song`) have up to 6 charts (easy/normal/hard for dodge and for catch, 3-5 lanes) and a melody played by the built-in synth. A chart is made of beads (one note each); beads of fast runs are linked into ribbons, and a ribbon is one object: one life, one score. Songs are text files in `assets/beatmaps/`, listed in `assets/beatmaps/songs.txt`, converted from MIDI by `tools/midi2beatmap` (see `docs/SONGS.md`). The catch mode (rules in `beat_catch_logic.[ch]`, run loop `bd_ui_play_catch` in `beat_catch_game.c`) lives in this folder too and shares the song format, the clock, the select screen and the drawing with the dodge mode.

## Key Files
- `beat_dodge_logic.[ch]`: song format (load/save), song list, menu logic (`bd_menu_set_kind` switches the mode), the shared run (`bd_Run`: clock, lane, arrival), the reachable-lane check, dodge rules
- `beat_dodge_convert.[ch]`: MIDI -> song (ribbons, thinning, pitch-to-lane DP, repair), chart metrics; used by the tool and tests
- `beat_dodge_ui.h` + `beat_dodge_game.c`: select screen with the mode buttons, music playback (WAV in memory) and playfield drawing shared by both modes (`bd_ui_*`, implemented in the `_game.c`, no raylib types in the header), the dodge run loop, `play()` and `game_beat_dodge_def`
- `beat_catch_logic.[ch]`: catch rules (pure): state, `judge_bead`, combo, health, score. `beat_catch_game.c`: the catch run loop (`bd_ui_play_catch`), health bar, combo text
- `tests/test_beat_dodge.c`, `tests/test_beat_dodge_convert.c`, `tests/test_beat_catch.c`
- Boards: `bd_board_name` (logic) names the table of a chart; `bd_ui_report_board` (in `beat_dodge_game.c`, called by both run loops) writes it to `ctx->board`; the hub stores the score under `beat_dodge:<board>` (`lb_board_key`, `core/include/arcade/leaderboard.h`).

## Setup & environment
`cmake --preset logic` needs no raylib. Play: `cmake --preset dev && cmake --build --preset dev && build/dev/bin/arCade`.

## Build & tests
- `cmake --build --preset logic && ctest --preset logic -R "^beat_dodge\."`
- Every song in `songs.txt` is loaded and checked (playable, a path a human can walk, no burst without a ribbon, anti-camping mean) by `test_every_shipped_song_is_playable`: regenerate songs with `bash tools/songs/build_songs.sh` after changing the converter.
- Smoke: `build/headless/bin/arCade --smoke beat_dodge --frames 420 --out smoke-out` (plays dodge, then catch, half of the frames each; also writes `beat_dodge_select.png` and `beat_dodge_catch.png`)
- Chart measurements: `build/logic/bin/midi2beatmap --metrics assets/beatmaps/fnf/*.txt`

## Code style & conventions
- Time comes from `time`/`step_sec`/`lead`, never from frame counts. Bead `i` arrives at `lead + i * step_sec`; `bd_run_obstacle_y` and the judging must keep using `bd_run_arrival()`.
- Music timeline == game clock: a melody note sounds when its bead reaches the line (`bd_song_to_synth(song, chart, lead, ...)`); notes whose step holds a bead are lead (voice 0), the others quiet backing (voice 3). `beat_dodge_sync` only corrects drifts > 0.15 s. `BD_AUDIO_OFFSET_S` is shared by both modes and stays as it is.
- Rules: every bead is judged when it arrives (same lane = hit); an object costs at most ONE life; an object that ends without a hit scores 10, or 20 for a close call (stood in a lane it touches within `BD_CLOSE_WINDOW_SEC` before its first bead); 50 per life left on a win; no combo. Point values exist only in `award_object` / `award_finish`, and `max_points` replays a perfect run through them, so a perfect run is exactly 1000 x the difficulty multiplier (integer maths).
- Audio is optional: guard with `arcade_audio_ready()`; smoke runs never load audio.
- `bd_song_load` must keep rejecting bad files without touching its output. Every chart must pass the reachable-lane check (`bd_chart_playable`); lone beads keep the preset's minimum gap, beads of one ribbon are at most 0.25 s apart.
- `bd_Run`, the song format, `bd_catch_tolerance` and `bd_chart_playable` are used by the catch mode too: after changing them run the whole `ctest --preset logic`, and rebuild the songs when the check changes.
- Catch rules (`beat_catch_logic.h`): every rule number is a `BC_*` constant and points come only from `award_bead`/`award_finish` (`max_points` replays a perfect run through them, so a perfect run is exactly 1000 x the difficulty multiplier). Health is per OBJECT (`BC_HEALTH_PER_MISS` once per object with a missed bead, `BC_HEALTH_PER_CLEAN_OBJECT` per object caught completely, 0..`BC_START_HEALTH`); combo counts beads and any miss resets it. The tolerance of a bead is `bd_catch_tolerance(bd_chart_object_span(chart, step))`, the same call the chart check and the converter use: never type a 0 or a 1 for it.
- Standing still must lose on every shipped catch chart and the human-speed bot must win it (`test_standing_still_loses_on_every_shipped_catch_chart`, `test_the_human_speed_bot_clears_every_shipped_catch_chart`): re-run both after touching health numbers, tolerance or the converter.
- The menu keeps the mode (`bd_Menu.kind`) between plays; `bd_menu_set_kind` keeps the song and the difficulty. `beat_dodge_ui.h` must stay free of raylib types. The practice song is gone from `songs.txt`; `bd_song_default` is only a test fixture and the fallback when no song file loads.
- Lanes in converted songs come from pitch (low = left); do not shuffle them.
- Boards: a run's table is `"<Mode> / <song title> / <chart name>"` (`bd_board_name`), so renaming a song title or a chart name in a song file starts a new, empty table (the old scores stay under the old name). The name must always fit `GAME_BOARD_LEN`: only the title is shortened, never the mode or the difficulty, so two charts never share a table. Scores saved before boards existed sit under the plain id `beat_dodge` and show up as "older scores"; they cannot be assigned to a mode or song. Call `bd_ui_report_board` from any new run loop, and never write `scores.txt` from the game (the hub owns it).

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
