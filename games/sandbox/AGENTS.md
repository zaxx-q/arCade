<!-- Managed by agent: keep sections and order; edit content, not structure. Last updated: 2026-11-08 -->

# AGENTS.md - Sandbox (`sandbox`)

Scope: this folder only. Repo rules: `../../AGENTS.md`. Game contract: `../AGENTS.md`. Design brief: `GAME.md`. Owner: Leader | Unranked

## Overview
Falling-sand toy on the shared `core/sandbox` engine (also animates the hub background). Rules and status: `GAME.md`.

## Key Files
- `sandbox_logic.[ch]`: pure rules (no raylib), state struct, `init/step/is_over/score`
- `sandbox_game.c`: raylib loop + drawing + `game_sandbox_def`
- `tests/test_sandbox.c`: unit tests (logic only)

## Setup & environment
`cmake --preset logic` needs no raylib. To play: `cmake --preset dev && cmake --build --preset dev && build/dev/bin/arcade-hub`.

## Build & tests
- `cmake --build --preset logic && ctest --preset logic -R "^sandbox\."`
- Smoke run (headless build): `build/headless/bin/arcade-hub --smoke sandbox --frames 300 --out smoke-out`

## Code style & conventions
- Function prefix is `sbx_` (NOT `sandbox_`): `sandbox_*` belongs to the core engine.
- The world struct is ~115 KB: keep it `static`, never a local variable.
- New elements are added in `core/` (enum + table row + rule + test), not here; this folder only maps keys to elements.
- Test: `ctest --preset logic -R "^sandbox\."` (also `core.sandbox`).
- Follow the game contract in `../AGENTS.md` (loop pattern, determinism, fixed arrays, `finished` semantics).

## Security & safety
- Only write under `ctx->data_dir`; read assets from `ctx->assets_dir`; validate any file you parse.

## PR/commit checklist
- [ ] New rule has a unit test; `GAME.md` pseudocode, flowchart and concept table still match the code
- [ ] `bash tools/format.sh`, `bash tools/check_repo.sh` and the game's tests are clean

## Examples
- Good: `sandbox_paint(&s->world, cx, cy, s->brush, s->element);` (engine call from `sbx_step`)
- Bad: a `sbx_State` local variable (115 KB on the stack) or a new element rule written in this folder instead of `core/`

## When stuck
Read `GAME.md` (Implementation steps), copy a pattern from the nearest Golden Sample in the root `AGENTS.md`, run the monkey test to find crashes.
