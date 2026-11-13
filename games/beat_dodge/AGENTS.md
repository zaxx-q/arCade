<!-- Managed by agent: keep sections and order; edit content, not structure. Last updated: 2026-11-09 -->

# AGENTS.md - Beat Dodge (`beat_dodge`)

Scope: this folder only. Repo rules: `../../AGENTS.md`. Game contract: `../AGENTS.md`. Design brief: `GAME.md`. Owner: Member 5 | Ranked

## Overview
Rhythm dodger. Songs (`bd_Song`) have 1-3 charts (easy/normal/hard, 3-5 lanes) and a melody played by the built-in synth. Songs are text files in `assets/beatmaps/`, listed in `assets/beatmaps/songs.txt`, converted from MIDI by `tools/midi2beatmap` (see `docs/SONGS.md`).

## Key Files
- `beat_dodge_logic.[ch]`: song format (load/save), song list, menu logic, game rules
- `beat_dodge_convert.[ch]`: MIDI -> song (thinning, pitch-to-lane DP), used by the tool and tests
- `beat_dodge_game.c`: select screen, music playback (WAV in memory), drawing
- `tests/test_beat_dodge.c`, `tests/test_beat_dodge_convert.c`

## Setup & environment
`cmake --preset logic` needs no raylib. Play: `cmake --preset dev && cmake --build --preset dev && build/dev/bin/arCade`.

## Build & tests
- `cmake --build --preset logic && ctest --preset logic -R "^beat_dodge\."`
- Every song in `songs.txt` is loaded and checked (playable, a path a human can walk, no burst without a ribbon, anti-camping mean) by `test_every_shipped_song_is_playable`: regenerate songs with `bash tools/songs/build_songs.sh` after changing the converter.
- Smoke: `build/headless/bin/arCade --smoke beat_dodge --frames 420 --out smoke-out` (also writes `beat_dodge_select.png`)

## Code style & conventions
- Time comes from `time`/`step_sec`/`lead`, never from frame counts. Obstacle `i` arrives at `lead + i * step_sec`; `beat_dodge_obstacle_y` and the hit test must keep using `beat_dodge_arrival()`.
- Music timeline == game clock: a melody note sounds when its obstacle reaches the line (`bd_song_to_synth(song, lead, ...)`). `beat_dodge_sync` only corrects drifts > 0.15 s.
- Audio is optional: guard with `arcade_audio_ready()`; smoke runs never load audio.
- `bd_song_load` must keep rejecting bad files without touching its output. Every chart must pass the reachable-lane check (`bd_chart_playable`); lone beads keep the preset's minimum gap, beads of one ribbon are at most 0.25 s apart.
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
