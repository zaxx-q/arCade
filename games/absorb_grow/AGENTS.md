<!-- Managed by agent: keep sections and order; edit content, not structure. Last updated: 2026-11-08 -->

# AGENTS.md - Absorb and Grow (`absorb_grow`)

Scope: this folder only. Repo rules: `../../AGENTS.md`. Game contract: `../AGENTS.md`. Design brief: `GAME.md`. Owner: Member 6 | Ranked

## Overview
Swallow the small, avoid the big, keep growing. Rules and status: `GAME.md`.

## Key Files
- `absorb_grow_logic.[ch]`: pure rules (no raylib), state struct, `init/step/is_over/score`
- `absorb_grow_game.c`: raylib loop + drawing + `game_absorb_grow_def`
- `tests/test_absorb_grow.c`: unit tests (logic only)

## Setup & environment
`cmake --preset logic` needs no raylib. To play: `cmake --preset dev && cmake --build --preset dev && build/dev/bin/arCade`.

## Build & tests
- `cmake --build --preset logic && ctest --preset logic -R "^absorb_grow\."`
- Smoke run (headless build): `build/headless/bin/arCade --smoke absorb_grow --frames 300 --out smoke-out`

## Code style & conventions
- All size rules go through the `1.1x` margin and the area formula in `absorb_grow_step`; keep them in one place so "similar size = harmless" stays true.
- `spawn_blob` sizes blobs relative to the player: do not spawn at a fixed size or the game stops scaling.
- The player radius must never decrease (a test checks this); changing that rule needs a `GAME.md` update.
- Front-end particles trigger from `absorb_events` / `last_absorb`.
- Test: `ctest --preset logic -R "^absorb_grow\."`
- Follow the game contract in `../AGENTS.md` (loop pattern, determinism, fixed arrays, `finished` semantics).

## Security & safety
- Only write under `ctx->data_dir`; read assets from `ctx->assets_dir`; validate any file you parse.

## PR/commit checklist
- [ ] New rule has a unit test; `GAME.md` pseudocode, flowchart and concept table still match the code
- [ ] `bash tools/format.sh`, `bash tools/check_repo.sh` and the game's tests are clean

## Examples
- Good: `s->r = sqrtf(s->r * s->r + 0.7f * b->r * b->r);` (areas add up)
- Bad: `s->r += b->r;` (radii do not add: balance breaks)

## When stuck
Read `GAME.md` (Implementation steps), copy a pattern from the nearest Golden Sample in the root `AGENTS.md`, run the monkey test to find crashes.
