<!-- Managed by agent: keep sections and order; edit content, not structure. Last updated: 2026-11-08 -->

# AGENTS.md - Echo Maze (`echo_maze`)

Scope: this folder only. Repo rules: `../../AGENTS.md`. Game contract: `../AGENTS.md`. Design brief: `GAME.md`. Owner: Member 2 | Ranked

## Overview
Pitch-black maze navigated by sonar pings. Rules and status: `GAME.md`.

## Key Files
- `echo_maze_logic.[ch]`: pure rules (no raylib), state struct, `init/step/is_over/score`
- `echo_maze_game.c`: raylib loop + drawing + `game_echo_maze_def`
- `tests/test_echo_maze.c`: unit tests (logic only)

## Setup & environment
`cmake --preset logic` needs no raylib. To play: `cmake --preset dev && cmake --build --preset dev && build/dev/bin/arcade-hub`.

## Build & tests
- `cmake --build --preset logic && ctest --preset logic -R "^echo_maze\."`
- Smoke run (headless build): `build/headless/bin/arcade-hub --smoke echo_maze --frames 300 --out smoke-out`

## Code style & conventions
- `EM_W` and `EM_H` must stay ODD (the carving lattice uses odd coordinates); the border tiles stay walls.
- Generation is recursive (`carve`): keep recursion depth bounded by the lattice size; never recurse per frame.
- The wall grid is the single source of truth for collision, line of sight and drawing.
- `tests/test_echo_maze.c` checks that every seed gives a connected perfect maze: run it after touching `carve`.
- Test: `ctest --preset logic -R "^echo_maze\."`
- Follow the game contract in `../AGENTS.md` (loop pattern, determinism, fixed arrays, `finished` semantics).

## Security & safety
- Only write under `ctx->data_dir`; read assets from `ctx->assets_dir`; validate any file you parse.

## PR/commit checklist
- [ ] New rule has a unit test; `GAME.md` pseudocode, flowchart and concept table still match the code
- [ ] `bash tools/format.sh`, `bash tools/check_repo.sh` and the game's tests are clean

## Examples
- Good: `carve(s, nx, ny)`: recursion on the odd-coordinate lattice
- Bad: `#define EM_W 30` (even width breaks the generator)

## When stuck
Read `GAME.md` (Implementation steps), copy a pattern from the nearest Golden Sample in the root `AGENTS.md`, run the monkey test to find crashes.
