<!-- Managed by agent: keep sections and order; edit content, not structure. Last updated: 2026-11-12 -->

# AGENTS.md - Loop Lap (`loop_lap`)

Scope: this folder only. Repo rules: `../../AGENTS.md`. Game contract: `../AGENTS.md`. Design brief: `GAME.md`. Owner: Leader | Ranked

## Overview
Loop Lap: control only speed (brake / cruise / accelerate) on a loop while enemies circle the overlapping lower loop; score = laps. Rules and status: `GAME.md`.

## Key Files
- `loop_lap_logic.[ch]`: pure rules (no raylib), state struct, `init/init_sized/step/is_over/score`
- `loop_lap_game.c`: raylib loop + drawing + sounds + `game_loop_lap_def`
- `tests/test_loop_lap.c`: unit tests (logic only)

## Setup & environment
`cmake --preset logic` needs no raylib. To play: `cmake --preset dev && cmake --build --preset dev && build/dev/bin/arCade`.

## Build & tests
- `cmake --build --preset logic && ctest --preset logic -R "^loop_lap\."`
- Smoke run (headless build): `build/headless/bin/arCade --smoke loop_lap --frames 300 --out smoke-out`

## Code style & conventions
- At most 8 enemies; they join at laps 1, 3, 8, 17, 31, 51, 78, 113. Angles are `double`; enemy angles are never wrapped.
- Speeds are per original 50 Hz tick scaled by `k = dt * 50`; touching distance is `my_w/2 + 24 - 2` (52 / 46 / 40).
- Held input maps to brake/cruise/accelerate by a pure function (accelerate wins); pause toggles with P and freezes the logic.
- Portrait playfield: logic works in playfield-local coordinates (`arcade/playfield.h`); `loop_lap_init_sized(s, seed, w, h)` lets tests use 480 x 800 so the original spec vectors apply.
- Per-tick constants of the original 50 Hz game are scaled by `k = dt * 50`.
- Logic never plays sound: it exposes event counters and the front-end compares them frame to frame.
- Keep `GAME.md` in sync when mechanics change (the report is written from it).
- Follow the game contract in `../AGENTS.md` (loop pattern, determinism, fixed arrays, `finished` semantics).

## Security & safety
- Only write under `ctx->data_dir`; read assets from `ctx->assets_dir`; validate any file you parse. This game reads no files.
- No art, sound, font or text from the original app may be added (all drawing is raylib shapes, all audio is `gfx_make_tone`).

## PR/commit checklist
- [ ] New rule has a unit test; `GAME.md` pseudocode, flowchart and concept table still match the code
- [ ] `bash tools/format.sh`, `bash tools/check_repo.sh` and the game's tests are clean

## Examples
- Good: `loop_lap_step(&s, &in, ARC_DT);` called by the front-end and by the unit tests
- Bad: reading the keyboard or calling raylib inside `loop_lap_logic.c`

## When stuck
Read `GAME.md`, copy a pattern from the nearest Golden Sample in the root `AGENTS.md`, run the monkey test to find crashes.
