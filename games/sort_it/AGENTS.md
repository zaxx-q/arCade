<!-- Managed by agent: keep sections and order; edit content, not structure. Last updated: 2026-11-08 -->

# AGENTS.md - Sort It (`sort_it`)

Scope: this folder only. Repo rules: `../../AGENTS.md`. Game contract: `../AGENTS.md`. Design brief: `GAME.md`. Owner: Leader | Ranked

## Overview
Be the sorting algorithm: fewest swaps wins. Rules and status: `GAME.md`.

## Key Files
- `sort_it_logic.[ch]`: pure rules (no raylib), state struct, `init/step/is_over/score`
- `sort_it_game.c`: raylib loop + drawing + `game_sort_it_def`
- `tests/test_sort_it.c`: unit tests (logic only)

## Setup & environment
`cmake --preset logic` needs no raylib. To play: `cmake --preset dev && cmake --build --preset dev && build/dev/bin/arcade-hub`.

## Build & tests
- `cmake --build --preset logic && ctest --preset logic -R "^sort_it\."`
- Smoke run (headless build): `build/headless/bin/arcade-hub --smoke sort_it --frames 300 --out smoke-out`

## Code style & conventions
- Turn-based: `dt` is intentionally unused. Do not add real-time behaviour without updating `GAME.md`.
- Values are always a permutation of 1..n (needed for the exact `par` from `sort_min_swaps`). If you allow duplicates, `par` becomes only an upper bound.
- `SI_X0` and `SI_AREA_W` are shared by the logic (click hit-testing) and the front-end (drawing): change them in one place.
- Test: `ctest --preset logic -R "^sort_it\."`
- Follow the game contract in `../AGENTS.md` (loop pattern, determinism, fixed arrays, `finished` semantics).

## Security & safety
- Only write under `ctx->data_dir`; read assets from `ctx->assets_dir`; validate any file you parse.

## PR/commit checklist
- [ ] New rule has a unit test; `GAME.md` pseudocode, flowchart and concept table still match the code
- [ ] `bash tools/format.sh`, `bash tools/check_repo.sh` and the game's tests are clean

## Examples
- Good: `swap_int(&s->values[a], &s->values[b]);`
- Bad: using `dt` or real time in `sort_it_step` (the game is turn-based)

## When stuck
Read `GAME.md` (Implementation steps), copy a pattern from the nearest Golden Sample in the root `AGENTS.md`, run the monkey test to find crashes.
