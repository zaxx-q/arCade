<!-- Managed by agent: keep sections and order; edit content, not structure. Last updated: 2026-11-08 -->

# AGENTS.md - Laser Mirrors (`laser_mirrors`)

Scope: this folder only. Repo rules: `../../AGENTS.md`. Game contract: `../AGENTS.md`. Design brief: `GAME.md`. Owner: Member 3 | Ranked

## Overview
Rotate mirrors so the beam hits every target. Rules and status: `GAME.md`.

## Key Files
- `laser_mirrors_logic.[ch]`: pure rules (no raylib), state struct, `init/step/is_over/score`
- `laser_mirrors_game.c`: raylib loop + drawing + `game_laser_mirrors_def`
- `tests/test_laser_mirrors.c`: unit tests (logic only)

## Setup & environment
`cmake --preset logic` needs no raylib. To play: `cmake --preset dev && cmake --build --preset dev && build/dev/bin/arcade-hub`.

## Build & tests
- `cmake --build --preset logic && ctest --preset logic -R "^laser_mirrors\."`
- Smoke run (headless build): `build/headless/bin/arcade-hub --smoke laser_mirrors --frames 300 --out smoke-out`

## Code style & conventions
- Directions are 0=right, 1=down, 2=left, 3=up. `'/'` maps `d -> 3 - d`, `'\'` maps `d -> d ^ 1`. Keep these identities in one place (`trace`).
- Always call `laser_mirrors_recompute()` after changing any tile; never edit `beam[][]` or `lit[][]` by hand.
- Every level must pass `laser_mirrors_can_solve()` (add a test when adding a level) and must start unsolved.
- The visited-bit check in `trace` is what prevents infinite loops: do not remove it.
- Test: `ctest --preset logic -R "^laser_mirrors\."`
- Follow the game contract in `../AGENTS.md` (loop pattern, determinism, fixed arrays, `finished` semantics).

## Security & safety
- Only write under `ctx->data_dir`; read assets from `ctx->assets_dir`; validate any file you parse.

## PR/commit checklist
- [ ] New rule has a unit test; `GAME.md` pseudocode, flowchart and concept table still match the code
- [ ] `bash tools/format.sh`, `bash tools/check_repo.sh` and the game's tests are clean

## Examples
- Good: `laser_mirrors_toggle(s, tx, ty);` which re-traces the beam
- Bad: `s->beam[y][x] |= 1;` written by hand

## When stuck
Read `GAME.md` (Implementation steps), copy a pattern from the nearest Golden Sample in the root `AGENTS.md`, run the monkey test to find crashes.
