<!-- Managed by agent: keep sections and order; edit content, not structure. Last updated: @DATE@ -->

# AGENTS.md - @TITLE@ (`@ID@`)

Scope: this folder only. Repo rules: `../../AGENTS.md`. Game contract: `../AGENTS.md`. Design brief: `GAME.md`. Owner: @OWNER@ | Ranked

## Overview
TODO(@OWNER@): one-sentence hook. Rules and status: `GAME.md`.

## Key Files
- `@ID@_logic.[ch]`: pure rules (no raylib), state struct, `init/step/is_over/score`
- `@ID@_game.c`: raylib loop + drawing + `game_@ID@_def`
- `tests/test_@ID@.c`: unit tests (logic only)

## Setup & environment
`cmake --preset logic` needs no raylib. To play: `cmake --preset dev && cmake --build --preset dev && build/dev/bin/arCade`.

## Build & tests
- `cmake --build --preset logic && ctest --preset logic -R "^@ID@\."`
- Smoke run (headless build): `build/headless/bin/arCade --smoke @ID@ --frames 300 --out smoke-out`

## Code style & conventions
- TODO(@OWNER@): list the invariants of this game (bounds, caps, win/lose rules, anything that must stay true).
- Keep `GAME.md` in sync when mechanics change (the report is written from it).
- Follow the game contract in `../AGENTS.md` (loop pattern, determinism, fixed arrays, `finished` semantics).

## Security & safety
- Only write under `ctx->data_dir`; read assets from `ctx->assets_dir`; validate any file you parse.

## PR/commit checklist
- [ ] New rule has a unit test; `GAME.md` pseudocode, flowchart and concept table still match the code
- [ ] `bash tools/format.sh`, `bash tools/check_repo.sh` and the game's tests are clean

## Examples
- Good: `@ID@_step(&s, &in, ARC_DT);` called by the front-end and by the unit tests
- Bad: reading the keyboard or calling raylib inside `@ID@_logic.c`

## When stuck
Read `GAME.md` (Implementation steps), copy a pattern from the nearest Golden Sample in the root `AGENTS.md`, run the monkey test to find crashes.
