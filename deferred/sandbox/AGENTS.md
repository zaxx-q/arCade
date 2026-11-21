<!-- Managed by agent: keep sections and order; edit content, not structure. Last updated: 2026-11-21 -->

# AGENTS.md - Sandbox (`sandbox`)

Scope: this folder only. Repo rules: `../../AGENTS.md`. Game contract: `../AGENTS.md`. Design brief: `GAME.md`. Owner: Leader | Unranked

## Overview
Falling-sand toy on the shared `core/sandbox` engine (also animates the hub background). Rules and status: `GAME.md`.

## Key Files
- `sandbox_logic.[ch]`: pure rules (no raylib), state struct, palette geometry, `init/step/is_over/score`
- `sandbox_game.c`: raylib loop + drawing (palette, brush ring, HUD, hover temperature label) + `game_sandbox_def`
- `tests/test_sandbox.c`: unit tests (logic only)
- Engine (not in this folder): `core/src/sandbox*.c`; how it works and how to add an element: `../../docs/SANDBOX.md`

## Setup & environment
`cmake --preset logic` needs no raylib. To play: `cmake --preset dev && cmake --build --preset dev && build/dev/bin/arCade`.

## Build & tests
- `cmake --build --preset logic && ctest --preset logic -R "^sandbox\."`
- Smoke run (headless build): `build/headless/bin/arCade --smoke sandbox --frames 300 --out smoke-out`
- Keys handled here: `0`-`9`, `[` `]`, wheel, `+`/`-`, `T` (thermal view), `C`, `R`, Space. The hover temperature comes from `sbx_probe` (pure) and is drawn in `sandbox_game.c`.

## Code style & conventions
- Function prefix is `sbx_` (NOT `sandbox_`): `sandbox_*` belongs to the core engine.
- The world struct is ~400 KB: keep it `static`, never a local variable.
- New elements are added in `core/` (enum + `k_elements` row + `k_thermal` row + rule + test), not here; the palette, `[` `]`, wheel and clicks pick them up by themselves. This folder only maps input to elements.
- Test: `ctest --preset logic -R "^sandbox\."` (also `core.sandbox` for the engine).
- Follow the game contract in `../AGENTS.md` (loop pattern, determinism, fixed arrays, `finished` semantics).

## Security & safety
- Only write under `ctx->data_dir`; read assets from `ctx->assets_dir`; validate any file you parse.

## PR/commit checklist
- [ ] New rule has a unit test; `GAME.md` pseudocode, flowchart and concept table still match the code
- [ ] `bash tools/format.sh`, `bash tools/check_repo.sh` and the game's tests are clean

## Examples
- Good: `sandbox_paint(&s->world, cx, cy, s->brush, s->element);` (engine call from `sbx_step`)
- Bad: a `sbx_State` local variable (400 KB on the stack) or a new element rule written in this folder instead of `core/`

## When stuck
Read `GAME.md` (Implementation steps), copy a pattern from the nearest Golden Sample in the root `AGENTS.md`, run the monkey test to find crashes.
