<!-- Managed by agent: keep sections and order; edit content, not structure. Last updated: 2026-11-08 -->

# AGENTS.md - core/

## Overview
Shared library for every game and arCade. Two parts: **pure C** (`core/src`, headers in `core/include/arcade/` except `gfx.h`) that is unit-tested without raylib, and **raylib helpers** (`core/gfx/`, `gfx.h`) for window, frame loop, input polling, drawing and audio.

## Key Files
| File | Role |
|------|------|
| `include/arcade/game.h` | the contract: `GameContext`, `GameResult`, `GameDef` (changing it affects every game) |
| `include/arcade/input.h`, `src/input.c` | `ArcadeInput`, reproducible `ArcadeMonkey` |
| `include/arcade/rng.h` | deterministic PCG32 |
| `include/arcade/leaderboard.h` | text-file scores, insertion sort, linear + binary search |
| `include/arcade/sandbox.h`, `src/sandbox.c` | falling-sand engine (hub background + `games/sandbox`) |
| `include/arcade/sortlib.h` | bubble/selection/insertion sort with swap counters |
| `include/arcade/synth.h`, `src/synth.c` | pure software synth (notes + drums -> 16-bit PCM, WAV header) |
| `include/arcade/midi.h`, `src/midi.c` | Standard MIDI File reader (note events per track) |
| `gfx/gfx_text.c` | project font (Poiret One): one atlas per pixel size; ALWAYS draw text via `gfx_text()` |
| `gfx/gfx.c`, `gfx/gfx_draw.c` | the only code that calls raylib for loop/input/drawing helpers |

## Setup & environment
Pure part: any C99 compiler + CMake (`cmake --preset logic`). Front-end part needs raylib (downloaded by CMake on first configure; Linux needs the X11/GL dev packages listed in `docs/DEVELOPMENT.md`).

## Build & tests
- `cmake --preset logic && cmake --build --preset logic && ctest --preset logic` (core tests are `core.*`)
- `cmake --preset asan && cmake --build --preset asan && ctest --preset asan` before changing memory-handling code
- `bash tools/format.sh` and `bash tools/check_repo.sh` before committing

## Code style & conventions
- Portable C99 only: use `ARC_PI` (not `M_PI`), `arc_strlcpy`/`arc_stricmp` (not `strlcpy`/`strcasecmp`/`strdup`), no VLAs, no POSIX/Windows headers.
- Pure files never include `raylib.h` or `arcade/gfx.h`; `gfx.h` is the only header that does.
- Fixed-size arrays and caller-owned structs: no `malloc` in hot paths; large structs are passed by pointer.
- Every public function in `core/src` has a unit test in `tests/` (`test_core_basics`, `test_leaderboard`, `test_sortlib`, `test_sandbox`).
- API changes to `GameContext`, `GameDef`, `ArcadeInput`, `arcade_loop_*`, `arcade_poll`: update **all** of `core/gfx/gfx.c`, `input.c` (monkey), `app/`, `games/_template`, and every game; ask first.

## Security & safety
- File readers (`lb_load`) must tolerate corrupt input: skip bad lines, never overflow (`arc_strlcpy`, size-limited `fgets`).
- Player names are sanitised before they are written to `scores.txt` (no `|`, no control characters).

## PR/commit checklist
- [ ] Logic tests and `asan` tests pass; new code has a test
- [ ] No raylib include in pure files (`bash tools/check_repo.sh`)
- [ ] `docs/` updated if an API changed; `docs/raylib_subset.md` regenerated if raylib calls changed (`bash tools/gen_raylib_subset.sh`)

## Examples
- Good: `rng_range(&s->rng, 0, 3)` in logic; `arc_strlcpy(dst, src, sizeof dst)`.
- Bad: `rand() % 4`, `strncpy` without terminator, `M_PI`, `#include <unistd.h>`.

## When stuck
Read `core/include/arcade/game.h` first, then `games/_template/`. Ask the user before changing shared APIs.
