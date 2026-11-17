<!-- Managed by agent: keep sections and order; edit content, not structure. Last updated: 2026-11-14 -->

# AGENTS.md - Master Juggler (`master_juggler`)

Scope: this folder only. Repo rules: `../../AGENTS.md`. Game contract: `../AGENTS.md`. Design brief: `GAME.md`. Owner: Leader | Ranked

## Overview
Master Juggler: keep balls bouncing on a trampoline that follows the pointer; 3 lives; score = highest level completed. The level reached is saved per player, so the next run continues from it. Rules and status: `GAME.md`.

## Key Files
- `master_juggler_logic.[ch]`: pure rules (no raylib), state struct, `init/init_sized/step/is_over/score`, `resume`, and the `master_juggler_progress_*` file helpers
- `master_juggler_game.c`: raylib loop + drawing + sounds + `game_master_juggler_def`
- `tests/test_master_juggler.c`: unit tests (logic only)

## Setup & environment
`cmake --preset logic` needs no raylib. To play: `cmake --preset dev && cmake --build --preset dev && build/dev/bin/arCade`.

## Build & tests
- `cmake --build --preset logic && ctest --preset logic -R "^master_juggler\."`
- Smoke run (headless build): `build/headless/bin/arCade --smoke master_juggler --frames 300 --out smoke-out`

## Code style & conventions
- `1 <= level <= 32`, `0 <= lives <= MJ_LIVES (3)`, `nb <= level` and `nb <= 32`; completing level 32 or reaching 0 lives ends the game. Score is the highest level COMPLETED.
- Vertical constants scale with `gh` computed in `init_sized`; damping is `powf(0.98, k)`; the bar always stays fully inside the court.
- The first lost ball makes every ball grey and disables bouncing for the rest of the try; losing all balls costs one life and repeats the level.
- The "a ball was lost: try again" hint is driven by `last_try_failed` (set when a try is lost, cleared when the next try starts), NOT by `lives < MJ_LIVES`, which stays true for the rest of the run.
- Resume: `master_juggler_resume(s, level)` starts at the saved level and counts the levels below as completed (`best = level - 1`). On the first start screen (`fresh`) Up/Down/wheel lower the start level between 1 and `resume_level`; once a try starts it is locked. `master_juggler_progress_level` = max(saved level, level now), so progress never goes down.
- Portrait playfield: logic works in playfield-local coordinates (`arcade/playfield.h`); `master_juggler_init_sized(s, seed, w, h)` lets tests use 480 x 800 so the original spec vectors apply.
- Per-tick constants of the original 50 Hz game are scaled by `k = dt * 50`.
- Logic never plays sound: it exposes event counters and the front-end compares them frame to frame.
- Keep `GAME.md` in sync when mechanics change (the report is written from it).
- Follow the game contract in `../AGENTS.md` (loop pattern, determinism, fixed arrays, `finished` semantics).

## Security & safety
- Only write under `ctx->data_dir`; read assets from `ctx->assets_dir`; validate any file you parse. This game keeps one small file, `master_juggler_progress.txt` (one `player|level` line per player), in `ctx->data_dir`; bad lines are skipped, the file is written via a temp file, and headless/smoke runs never touch it.
- No art, sound, font or text from the original app may be added (all drawing is raylib shapes, all audio is `gfx_make_tone`).

## PR/commit checklist
- [ ] New rule has a unit test; `GAME.md` pseudocode, flowchart and concept table still match the code
- [ ] `bash tools/format.sh`, `bash tools/check_repo.sh` and the game's tests are clean

## Examples
- Good: `master_juggler_step(&s, &in, ARC_DT);` called by the front-end and by the unit tests
- Bad: reading the keyboard or calling raylib inside `master_juggler_logic.c`

## When stuck
Read `GAME.md`, copy a pattern from the nearest Golden Sample in the root `AGENTS.md`, run the monkey test to find crashes.
