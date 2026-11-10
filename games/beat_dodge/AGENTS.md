<!-- Managed by agent: keep sections and order; edit content, not structure. Last updated: 2026-11-08 -->

# AGENTS.md - Beat Dodge (`beat_dodge`)

Scope: this folder only. Repo rules: `../../AGENTS.md`. Game contract: `../AGENTS.md`. Design brief: `GAME.md`. Owner: Member 5 | Ranked

## Overview
Dodge obstacles that arrive exactly on the beat. Rules and status: `GAME.md`.

## Key Files
- `beat_dodge_logic.[ch]`: pure rules (no raylib), state struct, `init/step/is_over/score`
- `beat_dodge_game.c`: raylib loop + drawing + `game_beat_dodge_def`
- `tests/test_beat_dodge.c`: unit tests (logic only)

## Setup & environment
`cmake --preset logic` needs no raylib. To play: `cmake --preset dev && cmake --build --preset dev && build/dev/bin/arCade`.

## Build & tests
- `cmake --build --preset logic && ctest --preset logic -R "^beat_dodge\."`
- Smoke run (headless build): `build/headless/bin/arCade --smoke beat_dodge --frames 300 --out smoke-out`

## Code style & conventions
- Timing is derived from `time` and `spb` (seconds per beat), never from frame counts, so the BPM can change per map.
- An obstacle for beat `i` arrives at `(i + BD_LEAD_BEATS) * spb`; `beat_dodge_obstacle_y` and the hit test must keep using that same formula.
- Audio is optional: guard every sound call with `arcade_audio_ready()` (smoke runs and CI have no audio device). The front-end skips loading map files when `ctx->headless` so smoke runs are deterministic.
- `bd_beatmap_load` must keep rejecting bad files without touching its output (see `tests/test_beat_dodge.c`).
- Test: `ctest --preset logic -R "^beat_dodge\."`
- Follow the game contract in `../AGENTS.md` (loop pattern, determinism, fixed arrays, `finished` semantics).

## Security & safety
- Only write under `ctx->data_dir`; read assets from `ctx->assets_dir`; validate any file you parse.

## PR/commit checklist
- [ ] New rule has a unit test; `GAME.md` pseudocode, flowchart and concept table still match the code
- [ ] `bash tools/format.sh`, `bash tools/check_repo.sh` and the game's tests are clean

## Examples
- Good: `arrive = ((float)i + BD_LEAD_BEATS) * s->spb;` used for both drawing and hit testing
- Bad: `PlaySound(tick)` without `arcade_audio_ready()`

## When stuck
Read `GAME.md` (Implementation steps), copy a pattern from the nearest Golden Sample in the root `AGENTS.md`, run the monkey test to find crashes.
