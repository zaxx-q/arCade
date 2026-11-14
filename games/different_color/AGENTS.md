<!-- Managed by agent: keep sections and order; edit content, not structure. Last updated: 2026-11-12 -->

# AGENTS.md - A Different Color (`different_color`)

Scope: this folder only. Repo rules: `../../AGENTS.md`. Game contract: `../AGENTS.md`. Design brief: `GAME.md`. Owner: Leader | Ranked

## Overview
A Different Color: find the one cell whose colour differs slightly on the bottom board; score = boards solved. Rules and status: `GAME.md`.

## Key Files
- `different_color_logic.[ch]`: pure rules (no raylib), state struct, `init/init_sized/step/is_over/score`
- `different_color_game.c`: raylib loop + drawing + sounds + `game_different_color_def`
- `tests/test_different_color.c`: unit tests (logic only)

## Setup & environment
`cmake --preset logic` needs no raylib. To play: `cmake --preset dev && cmake --build --preset dev && build/dev/bin/arCade`.

## Build & tests
- `cmake --build --preset logic && ctest --preset logic -R "^different_color\."`
- Smoke run (headless build): `build/headless/bin/arCade --smoke different_color --frames 300 --out smoke-out`

## Code style & conventions
- `cols * rows <= DC_MAX_CELLS (400)` and `dif < n`; solving the 400-cell board (score 399) ends the game (`capped`).
- Colours are float triplets in logic; rejection sampling is capped at 10000 tries with a fixed fallback.
- Clicks are converted from screen to playfield-local pixels once in `step`; clicks outside the field or on the separator strip are ignored. The first board has no timer.
- Portrait playfield: logic works in playfield-local coordinates (`arcade/playfield.h`); `different_color_init_sized(s, seed, w, h)` lets tests use 480 x 800 so the original spec vectors apply.
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
- Good: `different_color_step(&s, &in, ARC_DT);` called by the front-end and by the unit tests
- Bad: reading the keyboard or calling raylib inside `different_color_logic.c`

## When stuck
Read `GAME.md`, copy a pattern from the nearest Golden Sample in the root `AGENTS.md`, run the monkey test to find crashes.
