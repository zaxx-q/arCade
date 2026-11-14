<!-- Managed by agent: keep sections and order; edit content, not structure. Last updated: 2026-11-12 -->

# AGENTS.md - Master Juggler (`master_juggler`)

Scope: this folder only. Repo rules: `../../AGENTS.md`. Game contract: `../AGENTS.md`. Design brief: `GAME.md`. Owner: Leader | Ranked

## Overview
Master Juggler: keep balls bouncing on a trampoline that follows the pointer; 3 lives; score = highest level completed. Rules and status: `GAME.md`.

## Key Files
- `master_juggler_logic.[ch]`: pure rules (no raylib), state struct, `init/init_sized/step/is_over/score`
- `master_juggler_game.c`: raylib loop + drawing + sounds + `game_master_juggler_def`
- `tests/test_master_juggler.c`: unit tests (logic only)

## Setup & environment
`cmake --preset logic` needs no raylib. To play: `cmake --preset dev && cmake --build --preset dev && build/dev/bin/arCade`.

## Build & tests
- `cmake --build --preset logic && ctest --preset logic -R "^master_juggler\."`
- Smoke run (headless build): `build/headless/bin/arCade --smoke master_juggler --frames 300 --out smoke-out`

## Code style & conventions
- `1 <= level <= 32`, `0 <= lives <= MJ_LIVES (3)`, `nb <= level` and `nb <= 32`; completing level 32 or reaching 0 lives ends the game. Score is the highest level COMPLETED.
- Vertical constants scale with `gh` computed in `init_sized`; damping is `powf(0.98, k)`; the bar always stays fully inside the court.
- The first lost ball makes every ball grey and disables bouncing for the rest of the try; losing all balls costs one life and repeats the level.
- Portrait playfield: logic works in playfield-local coordinates (`arcade/playfield.h`); `master_juggler_init_sized(s, seed, w, h)` lets tests use 480 x 800 so the original spec vectors apply.
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
- Good: `master_juggler_step(&s, &in, ARC_DT);` called by the front-end and by the unit tests
- Bad: reading the keyboard or calling raylib inside `master_juggler_logic.c`

## When stuck
Read `GAME.md`, copy a pattern from the nearest Golden Sample in the root `AGENTS.md`, run the monkey test to find crashes.
