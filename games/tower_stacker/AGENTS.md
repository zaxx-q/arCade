<!-- Managed by agent: keep sections and order; edit content, not structure. Last updated: 2026-11-12 -->

# AGENTS.md - Tower Stacker (`tower_stacker`)

Scope: this folder only. Repo rules: `../../AGENTS.md`. Game contract: `../AGENTS.md`. Design brief: `GAME.md`. Owner: Leader | Ranked

## Overview
Tower Stacker: drop a sliding bar onto a tower; overhang is cut off, a perfect landing keeps the width; score = floors. Rules and status: `GAME.md`.

## Key Files
- `tower_stacker_logic.[ch]`: pure rules (no raylib), state struct, `init/init_sized/step/is_over/score`
- `tower_stacker_game.c`: raylib loop + drawing + sounds + `game_tower_stacker_def`
- `tests/test_tower_stacker.c`: unit tests (logic only)

## Setup & environment
`cmake --preset logic` needs no raylib. To play: `cmake --preset dev && cmake --build --preset dev && build/dev/bin/arCade`.

## Build & tests
- `cmake --build --preset logic && ctest --preset logic -R "^tower_stacker\."`
- Smoke run (headless build): `build/headless/bin/arCade --smoke tower_stacker --frames 300 --out smoke-out`

## Code style & conventions
- D6 exception: the bar moves in whole 5 px steps on a 50 Hz sub-clock, so `bar_x` is always congruent to `base_x` (mod 5) and a perfect is reachable. Everything else uses `k = dt * 50` or plain `dt`.
- `0 <= score <= TS_MAX_FLOORS (1023)`; reaching 1023 ends the game. Floors store x/width rounded half up; windows only on floors >= 31 px wide.
- Game over reveal: 0.8 s for towers under 10 floors, 2.5 s (zoom-to-fit as a pure function of `over_t`) for 10+ floors.
- Portrait playfield: logic works in playfield-local coordinates (`arcade/playfield.h`); `tower_stacker_init_sized(s, seed, w, h)` lets tests use 480 x 800 so the original spec vectors apply.
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
- Good: `tower_stacker_step(&s, &in, ARC_DT);` called by the front-end and by the unit tests
- Bad: reading the keyboard or calling raylib inside `tower_stacker_logic.c`

## When stuck
Read `GAME.md`, copy a pattern from the nearest Golden Sample in the root `AGENTS.md`, run the monkey test to find crashes.
