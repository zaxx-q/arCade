<!-- Managed by agent: keep sections and order; edit content, not structure. Last updated: 2026-11-08 -->

# AGENTS.md - Ghost Loop (`ghost_loop`)

Scope: this folder only. Repo rules: `../../AGENTS.md`. Game contract: `../AGENTS.md`. Design brief: `GAME.md`. Owner: Member 4 | Ranked

## Overview
Past runs replay as ghosts that help you. Rules and status: `GAME.md`.

## Key Files
- `ghost_loop_logic.[ch]`: pure rules (no raylib), state struct, `init/step/is_over/score`
- `ghost_loop_game.c`: raylib loop + drawing + `game_ghost_loop_def`
- `tests/test_ghost_loop.c`: unit tests (logic only)

## Setup & environment
`cmake --preset logic` needs no raylib. To play: `cmake --preset dev && cmake --build --preset dev && build/dev/bin/arcade-hub`.

## Build & tests
- `cmake --build --preset logic && ctest --preset logic -R "^ghost_loop\."`
- Smoke run (headless build): `build/headless/bin/arcade-hub --smoke ghost_loop --frames 300 --out smoke-out`

## Code style & conventions
- The simulation ignores `dt` and always advances by `ARC_DT`: replays must be bit-for-bit identical. Never use real time here.
- Player and ghosts MUST move through the same function (`move_actor`). A second movement code path breaks replays.
- Ghosts store inputs (`gl_Move`), not positions. Keep inputs quantized to -1/0/1.
- `GL_ROUND_STEPS` bounds every array; do not index past `step`.
- Test: `ctest --preset logic -R "^ghost_loop\."`
- Follow the game contract in `../AGENTS.md` (loop pattern, determinism, fixed arrays, `finished` semantics).

## Security & safety
- Only write under `ctx->data_dir`; read assets from `ctx->assets_dir`; validate any file you parse.

## PR/commit checklist
- [ ] New rule has a unit test; `GAME.md` pseudocode, flowchart and concept table still match the code
- [ ] `bash tools/format.sh`, `bash tools/check_repo.sh` and the game's tests are clean

## Examples
- Good: ghosts replay `gl_Move` inputs through `move_actor()`
- Bad: a second movement function for ghosts, or `dt`-based movement

## When stuck
Read `GAME.md` (Implementation steps), copy a pattern from the nearest Golden Sample in the root `AGENTS.md`, run the monkey test to find crashes.
