<!-- Managed by agent: keep sections and order; edit content, not structure. Last updated: 2026-11-14 -->

# AGENTS.md - Beat Catch (`beat_catch`)

Scope: this folder only. Repo rules: `../../AGENTS.md`. Game contract: `../AGENTS.md`. Design brief: `GAME.md`. Owner: Member 5 | Ranked

## Overview
Rhythm catcher, the mirror image of Beat Dodge: the same songs, but you stand in the lane of every falling note when it reaches the line. A caught bead scores (x combo), a missed one breaks the combo, and an object with a missed bead costs health. It has its own score and leaderboard (`beat_catch`, ranked, order 55) and plays the `catch` charts of the songs in `assets/beatmaps/`. Rules and status: `GAME.md`.

## Key Files
- `beat_catch_logic.[ch]`: pure rules (no raylib), state struct (embeds `bd_Run`), `init/use_song/step/sync/is_over/score`
- `beat_catch_game.c`: raylib loop, health bar, combo text, `game_beat_catch_def`; the song select screen, the music and the playfield come from `../beat_dodge/beat_dodge_ui.h`
- `tests/test_beat_catch.c`: unit tests (logic only), including every shipped catch chart
- Shared with Beat Dodge (edit there): song format, `bd_Run` clock, `bd_catch_tolerance`, `bd_chart_playable`, the converter

## Setup & environment
`cmake --preset logic` needs no raylib. To play: `cmake --preset dev && cmake --build --preset dev && build/dev/bin/arCade`.

## Build & tests
- `cmake --build --preset logic && ctest --preset logic -R "^beat_catch\."`
- Smoke run (headless build): `build/headless/bin/arCade --smoke beat_catch --frames 300 --out smoke-out`

## Code style & conventions
- Every rule number is a `BC_*` constant in `beat_catch_logic.h`, and points come only from `award_bead`/`award_finish`; `max_points` replays a perfect run through those same two functions, so a perfect run scores exactly 1000 x the difficulty multiplier. Do not write a second copy of a point value.
- Health is per OBJECT: one `BC_HEALTH_PER_MISS` per object with a missed bead, `BC_HEALTH_PER_CLEAN_OBJECT` per object caught completely, always within 0..`BC_START_HEALTH`. Combo counts beads and any miss resets it.
- The tolerance of a bead is `bd_catch_tolerance(bd_chart_object_span(chart, step))`, the same call the chart check and the converter use. Never type a 0 or a 1 for it here: charts are accepted by the check with that rule, so changing it requires `bash tools/songs/build_songs.sh`.
- `beat_catch` links `game_beat_dodge_logic` and `game_beat_dodge`: do not add `beat_dodge_logic.c` to this game's `CMakeLists.txt`. `beat_dodge_ui.h` must stay free of raylib types.
- `BD_AUDIO_OFFSET_S` is shared with Beat Dodge; do not tune it for this game alone.
- Standing still must lose on every shipped catch chart and the human-speed bot must win it (`test_standing_still_loses_on_every_shipped_catch_chart`, `test_the_human_speed_bot_clears_every_shipped_catch_chart`): re-run both after touching health numbers, tolerance or the converter.
- Keep `GAME.md` in sync when mechanics change (the report is written from it).
- Follow the game contract in `../AGENTS.md` (loop pattern, determinism, fixed arrays, `finished` semantics).

## Security & safety
- Only write under `ctx->data_dir`; read assets from `ctx->assets_dir`; validate any file you parse.

## PR/commit checklist
- [ ] New rule has a unit test; `GAME.md` pseudocode, flowchart and concept table still match the code
- [ ] `bash tools/format.sh`, `bash tools/check_repo.sh` and the game's tests are clean

## Examples
- Good: `beat_catch_step(&s, &in, ARC_DT);` called by the front-end and by the unit tests
- Bad: reading the keyboard or calling raylib inside `beat_catch_logic.c`

## When stuck
Read `GAME.md` (Implementation steps), copy a pattern from the nearest Golden Sample in the root `AGENTS.md`, run the monkey test to find crashes.
