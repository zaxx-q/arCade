<!-- Managed by agent: keep sections and order; edit content, not structure. Last updated: 2026-11-12 -->

# AGENTS.md - BW Reflex (`bw_reflex`)

Scope: this folder only. Repo rules: `../../AGENTS.md`. Game contract: `../AGENTS.md`. Design brief: `GAME.md`. Owner: Leader | Ranked

## Overview
BW Reflex: flip the base colour so it matches the falling block when it lands; score = blocks matched. Rules and status: `GAME.md`.

## Key Files
- `bw_reflex_logic.[ch]`: pure rules (no raylib), state struct, `init/init_sized/step/is_over/score`
- `bw_reflex_game.c`: raylib loop + drawing + sounds + `game_bw_reflex_def`
- `tests/test_bw_reflex.c`: unit tests (logic only)

## Setup & environment
`cmake --preset logic` needs no raylib. To play: `cmake --preset dev && cmake --build --preset dev && build/dev/bin/arCade`.

## Build & tests
- `cmake --build --preset logic && ctest --preset logic -R "^bw_reflex\."`
- Smoke run (headless build): `build/headless/bin/arCade --smoke bw_reflex --frames 300 --out smoke-out`

## Code style & conventions
- Score = matched blocks, never negative. Game ends only on a colour mismatch (after the 0.9 s reveal); there is no cap because speed saturates (mult decays).
- Playfield panel is mid-grey on purpose (black/white are the mechanic); do not switch it to the neon palette.
- `last_note` is always 0..7 and is computed from the score BEFORE the increment.
- Portrait playfield: logic works in playfield-local coordinates (`arcade/playfield.h`); `bw_reflex_init_sized(s, seed, w, h)` lets tests use 480 x 800 so the original spec vectors apply.
- Per-tick constants of the original 50 Hz game are scaled by `k = dt * 50`.
- Logic never plays sound: it exposes event counters and the front-end compares them frame to frame.
- Keep `GAME.md` in sync when mechanics change (the report is written from it).
- Follow the game contract in `../AGENTS.md` (loop pattern, determinism, fixed arrays, `finished` semantics).

## Security & safety
- Only write under `ctx->data_dir`; read assets from `ctx->assets_dir`; validate any file you parse. This game reads no files.
- No art, sound, font or text from the original app may be added (all drawing is raylib shapes, all audio is `gfx_make_tone`).

## PR/commit checklist
- [ ] New rule has a unit test; `GAME.md` pseudocode, flowchart and concept table still match the code
- [ ] `bash tools/format.sh`, `bash tools/check_repo.sh` and the game's tests are clean

## Examples
- Good: `bw_reflex_step(&s, &in, ARC_DT);` called by the front-end and by the unit tests
- Bad: reading the keyboard or calling raylib inside `bw_reflex_logic.c`

## When stuck
Read `GAME.md`, copy a pattern from the nearest Golden Sample in the root `AGENTS.md`, run the monkey test to find crashes.
