<!-- Managed by agent: keep sections and order; edit content, not structure. Last updated: 2026-11-08 -->

# AGENTS.md - Time Shooter (`time_shooter`)

Scope: this folder only. Repo rules: `../../AGENTS.md`. Game contract: `../AGENTS.md`. Design brief: `GAME.md`. Owner: Member 1 | Ranked

## Overview
Time only moves when you move. Rules and status: `GAME.md`.

## Key Files
- `time_shooter_logic.[ch]`: pure rules (no raylib), state struct, `init/step/is_over/score`
- `time_shooter_game.c`: raylib loop + drawing + `game_time_shooter_def`
- `tests/test_time_shooter.c`: unit tests (logic only)

## Setup & environment
`cmake --preset logic` needs no raylib. To play: `cmake --preset dev && cmake --build --preset dev && build/dev/bin/arCade`.

## Build & tests
- `cmake --build --preset logic && ctest --preset logic -R "^time_shooter\."`
- Smoke run (headless build): `build/headless/bin/arCade --smoke time_shooter --frames 300 --out smoke-out`

## Code style & conventions
- The player always moves in REAL time (`dt`); everything else uses `wdt = dt * time_scale`. Any new moving thing must use `wdt`, or the "time freezes" hook breaks.
- Arrays are fixed-size (`TS_MAX_*`); never allocate. A full array means "skip the spawn".
- The front-end spawns particles by watching `kills` / `last_kill`; keep those fields updated when adding new kill types.
- Test: `ctest --preset logic -R "^time_shooter\."`
- Follow the game contract in `../AGENTS.md` (loop pattern, determinism, fixed arrays, `finished` semantics).

## Security & safety
- Only write under `ctx->data_dir`; read assets from `ctx->assets_dir`; validate any file you parse.

## PR/commit checklist
- [ ] New rule has a unit test; `GAME.md` pseudocode, flowchart and concept table still match the code
- [ ] `bash tools/format.sh`, `bash tools/check_repo.sh` and the game's tests are clean

## Examples
- Good: `e->pos = vec2_add(e->pos, vec2_scale(dir, e->speed * wdt));` (world time)
- Bad: the same line with `dt`: enemies would move while the player stands still

## When stuck
Read `GAME.md` (Implementation steps), copy a pattern from the nearest Golden Sample in the root `AGENTS.md`, run the monkey test to find crashes.
