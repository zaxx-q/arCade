<!-- Managed by agent: keep sections and order; edit content, not structure. Last updated: 2026-11-12 -->

# AGENTS.md - Post It (`post_it`)

Scope: this folder only. Repo rules: `../../AGENTS.md`. Game contract: `../AGENTS.md`. Design brief: `GAME.md`. Owner: Leader | Ranked

## Overview
Post It: tilt the world with the pointer offset, glue post-its to your cluster, dodge viruses; score = cluster blocks. Rules and status: `GAME.md`.

## Key Files
- `post_it_logic.[ch]`: pure rules (no raylib), state struct, `init/init_sized/step/is_over/score`
- `post_it_game.c`: raylib loop + drawing + sounds + `game_post_it_def`
- `tests/test_post_it.c`: unit tests (logic only)

## Setup & environment
`cmake --preset logic` needs no raylib. To play: `cmake --preset dev && cmake --build --preset dev && build/dev/bin/arCade`.

## Build & tests
- `cmake --build --preset logic && ctest --preset logic -R "^post_it\."`
- Smoke run (headless build): `build/headless/bin/arCade --smoke post_it --frames 300 --out smoke-out`

## Code style & conventions
- `1 <= blockn <= PI_MAX_BLOCKS (256)`; reaching 256 ends the game (`capped`). Arrays are only indexed below `blockn` / `PI_GREENS` / `PI_REDS`.
- Evaluation order matters: all greens (move, then grab test with `blockn` re-read each pass), then all viruses. Items always stay within x in [-16, W+16], y in [-16, H+16].
- Tilt mapping is a pure function (pointer offset / 240, keys override); with zero tilt the world is frozen. Pause toggles on click or P and freezes the logic.
- Portrait playfield: logic works in playfield-local coordinates (`arcade/playfield.h`); `post_it_init_sized(s, seed, w, h)` lets tests use 480 x 800 so the original spec vectors apply.
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
- Good: `post_it_step(&s, &in, ARC_DT);` called by the front-end and by the unit tests
- Bad: reading the keyboard or calling raylib inside `post_it_logic.c`

## When stuck
Read `GAME.md`, copy a pattern from the nearest Golden Sample in the root `AGENTS.md`, run the monkey test to find crashes.
