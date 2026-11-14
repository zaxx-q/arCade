<!-- Managed by agent: keep sections and order; edit content, not structure. Last updated: 2026-11-13 -->

# AGENTS.md - Beat Catch (`beat_catch`)

Scope: this folder only. Repo rules: `../../AGENTS.md`. Game contract: `../AGENTS.md`. Design brief: `GAME.md`. Owner: Member 5 | Ranked

## Overview
TODO(Member 5): one-sentence hook. Rules and status: `GAME.md`.

## Key Files
- `beat_catch_logic.[ch]`: pure rules (no raylib), state struct, `init/step/is_over/score`
- `beat_catch_game.c`: raylib loop + drawing + `game_beat_catch_def`
- `tests/test_beat_catch.c`: unit tests (logic only)

## Setup & environment
`cmake --preset logic` needs no raylib. To play: `cmake --preset dev && cmake --build --preset dev && build/dev/bin/arCade`.

## Build & tests
- `cmake --build --preset logic && ctest --preset logic -R "^beat_catch\."`
- Smoke run (headless build): `build/headless/bin/arCade --smoke beat_catch --frames 300 --out smoke-out`

## Code style & conventions
- TODO(Member 5): list the invariants of this game (bounds, caps, win/lose rules, anything that must stay true).
- Keep `GAME.md` in sync when mechanics change (the report is written from it).
- Follow the game contract in `../AGENTS.md` (loop pattern, determinism, fixed arrays, `finished` semantics).

## Security & safety
- Only write under `ctx->data_dir`; read assets from `ctx->assets_dir`; validate any file you parse.

## PR/commit checklist
- [ ] New rule has a unit test; `GAME.md` pseudocode, flowchart and concept table still match the code
- [ ] `bash tools/format.sh`, `bash tools/check_repo.sh` and the game's tests are clean

## Examples
- Good: `beat_catch_step(&s, &in, ARC_DT);` called by the front-end and by the unit tests
- Bad: reading the keyboard or calling raylib inside `beat_catch_logic.c`

## When stuck
Read `GAME.md` (Implementation steps), copy a pattern from the nearest Golden Sample in the root `AGENTS.md`, run the monkey test to find crashes.
