<!-- Managed by agent: keep sections and order; edit content, not structure. Last updated: 2026-11-12 -->

# AGENTS.md - New Star (`new_star`)

Scope: this folder only. Repo rules: `../../AGENTS.md`. Game contract: `../AGENTS.md`. Design brief: `GAME.md`. Owner: Leader | Ranked

## Overview
New Star: tap the newest star after each fade; tapping an old star ends the run; from star 10 each round has a 15 s limit; score = new stars found. Rules and status: `GAME.md`.

## Key Files
- `new_star_logic.[ch]`: pure rules (no raylib), state struct, `init/init_sized/step/is_over/score`
- `new_star_game.c`: raylib loop + drawing + sounds + `game_new_star_def`
- `tests/test_new_star.c`: unit tests (logic only)

## Setup & environment
`cmake --preset logic` needs no raylib. To play: `cmake --preset dev && cmake --build --preset dev && build/dev/bin/arCade`.

## Build & tests
- `cmake --build --preset logic && ctest --preset logic -R "^new_star\."`
- Smoke run (headless build): `build/headless/bin/arCade --smoke new_star --frames 300 --out smoke-out`

## Code style & conventions
- `1 <= n <= 512`, `score <= n`, `n - score <= 1`; confirming the 512th star ends the game (`capped`). Placement uses at most 2000 attempts per star (last candidate accepted).
- All millisecond thresholds of the original are kept as seconds accumulated from `dt` (overlay phases 0.999/1.199/2.199, creation 1.05, tap lock 1.4, alarm 10.001, limit 15.001).
- Taps are converted from screen to playfield-local pixels once in `step`; clicks outside the playfield and clicks while the overlay is locked do nothing. Hit box = star box + 10 px, rotation ignored.
- Portrait playfield: logic works in playfield-local coordinates (`arcade/playfield.h`); `new_star_init_sized(s, seed, w, h)` lets tests use 480 x 800 so the original spec vectors apply.
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
- Good: `new_star_step(&s, &in, ARC_DT);` called by the front-end and by the unit tests
- Bad: reading the keyboard or calling raylib inside `new_star_logic.c`

## When stuck
Read `GAME.md`, copy a pattern from the nearest Golden Sample in the root `AGENTS.md`, run the monkey test to find crashes.
