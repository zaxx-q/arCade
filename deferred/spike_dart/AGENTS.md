<!-- Managed by agent: keep sections and order; edit content, not structure. Last updated: 2026-11-14 -->

# AGENTS.md - Spike Dart (`spike_dart`)

Scope: this folder only. Repo rules: `../../AGENTS.md`. Game contract: `../AGENTS.md`. Design brief: `GAME.md`. Owner: Leader | Ranked

## Overview
Spike Dart: throw spikes into a spinning target without touching a stuck spike that is passing the launch lane; score = spikes stuck. Rules and status: `GAME.md`.

## Key Files
- `spike_dart_logic.[ch]`: pure rules (no raylib), state struct, `init/init_sized/step/is_over/score`
- `spike_dart_game.c`: raylib loop + drawing + sounds + `game_spike_dart_def`
- `tests/test_spike_dart.c`: unit tests (logic only)

## Setup & environment
`cmake --preset logic` needs no raylib. To play: `cmake --preset dev && cmake --build --preset dev && build/dev/bin/arCade`.

## Build & tests
- `cmake --build --preset logic && ctest --preset logic -R "^spike_dart\."`
- Smoke run (headless build): `build/headless/bin/arCade --smoke spike_dart --frames 300 --out smoke-out`

## Code style & conventions
- `0 <= stuck <= SD_MAX_SPIKES (64)`; the 64th stuck spike ends the game (`capped`). Arrays are never indexed past `stuck`.
- The hit box IS the drawn triangle: `spike_dart_shape` gives the 3 corners and the front-end draws exactly those. `spike_dart_would_touch` runs a separating-axis test against every stuck spike and only counts an overlap bigger than `SD_HIT_SLACK` (1.5 px). The original's centre-point x test (`|x - spike_x| < 8`) made 2 of 3 losses happen with the drawn spikes visibly apart, so do not bring it back.
- Orbit uses real `dt` (pi/2 rad/s); flight uses `k = dt * 50` and is cut into sub-steps of at most 4 px, each one moving the ring a little and testing the pose, so the result is the same at any frame rate and nothing is jumped over. A spike arrives when its centre reaches the orbit radius; the landing pose is tested before it sticks. `spike_dart_lane_blocked` = would a spike at the landing pose touch.
- Portrait playfield: logic works in playfield-local coordinates (`arcade/playfield.h`); `spike_dart_init_sized(s, seed, w, h)` lets tests use 480 x 800 so the original spec vectors apply.
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
- Good: `spike_dart_step(&s, &in, ARC_DT);` called by the front-end and by the unit tests
- Bad: reading the keyboard or calling raylib inside `spike_dart_logic.c`

## When stuck
Read `GAME.md`, copy a pattern from the nearest Golden Sample in the root `AGENTS.md`, run the monkey test to find crashes.
