<!-- FOR AI AGENTS - Human readability is a side effect, not a goal -->
<!-- Managed by agent: keep sections and order; edit content, not structure -->
<!-- Last updated: 2026-11-08 | Last verified: 2026-11-08 -->

# AGENTS.md

**Precedence:** the **closest `AGENTS.md`** to the files you're changing wins. Root holds global defaults only.

arCade: a hub plus 8 small games in **plain C99** drawn with **raylib 6.2** (pinned). University project (Dasar Pemrograman): keep code simple and procedural.

## Commands
> Source: `CMakePresets.json`, `.github/workflows/*.yml`, `tools/`. Windows: use Git Bash or MSYS2 for `bash`; the exe is `arCade.exe`.

| Task | Command | ~Time |
|------|---------|-------|
| Build + test, no raylib (use this first) | `cmake --preset logic && cmake --build --preset logic && ctest --preset logic` | ~5s |
| Test one game | `ctest --preset logic -R "^echo_maze\."` | ~1s |
| Format (CI pins clang-format 18.1.8) | `bash tools/format.sh` (`--check` to verify) | ~2s |
| Repo rules (game contract, portability) | `bash tools/check_repo.sh` | ~1s |
| Full build (downloads raylib once) | `cmake --preset dev && cmake --build --preset dev` | ~2 min first |
| Run the hub | `build/dev/bin/arCade` | - |
| Headless smoke of every game + PNGs | `cmake --preset headless && cmake --build --preset headless && build/headless/bin/arCade --smoke all --frames 300 --out smoke-out` | ~1 min |
| Sanitizers (Linux/macOS) | `cmake --preset asan && cmake --build --preset asan && ctest --preset asan` | ~30s |
| New game | `cmake -DID=my_game -DTITLE="My Game" -DOWNER="Name" -P tools/new_game.cmake` | ~1s |
| Next version preview | `bash tools/next_version.sh` | ~1s |
| Script tests | `bash tools/tests/test_next_version.sh && bash tools/tests/test_new_game.sh` | ~30s |

> If commands fail, check `docs/DEVELOPMENT.md` (toolchain setup) or ask the user.

## Response Style
- Answer first, elaborate only if needed. No filler openers.
- When asked to explain code, use plain language and name the course concept (see `docs/REPORT_GUIDE.md`).

## Workflow
1. **Before coding**: read the nearest `AGENTS.md` and the game's `GAME.md`; copy a Golden Sample.
2. **Logic first**: write rules in pure C with a unit test, run `ctest --preset logic`, only then draw it.
3. **Before committing**: `bash tools/format.sh`, `bash tools/check_repo.sh`, logic tests. Run the full build if you touched `core/`, `app/` or a `*_game.c`.
4. **Before claiming done**: paste the command output as evidence. Never say "should work" without running it.

## File Map
```
core/include/arcade/  pure-C headers (game.h = contract, input.h, rng.h, mathx.h, leaderboard.h, sandbox.h, ...); gfx.h = raylib helpers
core/src/             pure-C implementations (unit-tested)      core/gfx/  raylib helpers (window, loop, drawing)
app/                  hub: main.c (CLI + state machine), screens.c (menu, leaderboard), backdrop.c, smoke.c
games/<id>/           one game: <id>_logic.[ch] (pure) + <id>_game.c (raylib) + tests/ + GAME.md + AGENTS.md
games/_template/      source of `tools/new_game.cmake` (placeholders @ID@ @TITLE@ @OWNER@; never compiled directly)
cmake/                helpers: arcade_add_game(), registry generator, raylib fetch, warnings, MinGW toolchain
tools/                new_game.cmake, next_version.sh, check_repo.sh, format.sh, gen_raylib_subset.sh, tests/
tests/                arcade_test.h (tiny test framework) + core unit tests
docs/                 DEVELOPMENT, ARCHITECTURE, RELEASING, REPORT_GUIDE, ADDING_A_GAME, SANDBOX, raylib_subset (generated)
assets/               shipped with releases (beatmaps/)          .github/  CI + release workflows
```

## Golden Samples (follow these patterns)
| For | Reference | Key patterns |
|-----|-----------|--------------|
| New game from scratch | `games/_template/` | logic/front-end split, REQUIRED loop calls, tests |
| Recursion + generator tests | `games/echo_maze/` | `carve()`, "every seed is solvable" test |
| Deterministic replay | `games/ghost_loop/` | fixed steps, shared `move_actor()` |
| Turn-based + algorithms | `games/sort_it/` | `core/sortlib`, optimal-play test |
| Big static state + engine reuse | `games/sandbox/` | `static` state, `core/sandbox` |
| File parsing with validation | `games/beat_dodge/beat_dodge_logic.c` | `bd_beatmap_load` rejects bad input |

## Utilities (check before creating new)
| Need | Use | Location |
|------|-----|----------|
| Random numbers (never `rand()`) | `Rng`, `rng_range`, `rng_float` | `core/include/arcade/rng.h` |
| 2D math, collisions | `Vec2`, `vec2_*`, `arc_circles_overlap`, `ARC_PI` (no `M_PI`) | `mathx.h`, `common.h` |
| Safe strings | `arc_strlcpy`, `arc_stricmp` | `str.h` |
| Input in logic + tests | `ArcadeInput`, `ArcadeMonkey` | `input.h` |
| Particles, screen shake | `fx_burst`, `shake_start` | `fx.h` |
| High scores, sorting, search | `lb_*`, `sort_*` | `leaderboard.h`, `sortlib.h` |
| Falling sand | `sandbox_*` | `sandbox.h` |
| Drawing, HUD, glow, audio tone | `gfx_*`, `arcade_*` | `gfx.h` (front-ends only) |

## Heuristics (quick decisions)
| When | Do |
|------|-----|
| Need randomness in game logic | `Rng` seeded from `ctx->seed` |
| Need a key that `ArcadeInput` lacks | use `in->typed` (ASCII) or extend `ArcadeInput` + `arcade_poll` + monkey together (ask first) |
| Large struct (> ~50 KB) | `static` storage in `*_game.c`, never a local |
| raylib function unsure | `docs/raylib_subset.md`, else `build/<preset>/_deps/raylib-src/src/raylib.h` (6.x differs from 5.x) |
| Need file/network/OS API | don't: standard C only |
| Adding dependency | Ask first |

## Key Decisions
- Strict `-std=c99 -Wall -Wextra -Wpedantic -Werror`; no VLAs, no `conio.h`, no `system()`, no C++.
- Game logic never includes raylib (`ARCADE_LOGIC_ONLY` preset proves it) so it is unit-tested and smoke-tested headless.
- Games are discovered automatically from `games/<id>/` (no shared file to edit).
- Versions: git tags + Conventional Commits (`tools/next_version.sh`); workflows never commit.

## Boundaries

### Always Do
- Use Conventional Commits: `feat(scope): ...` (minor), `fix: ...` (patch), `feat!: ...` (major). PR titles too: squash-merge decides the release.
- Add a unit test for every new rule; keep `GAME.md` (pseudocode, flowchart, concept table) in sync.
- Keep code beginner-readable: small functions, comments on non-obvious lines, course concepts only (see `docs/REPORT_GUIDE.md`).

### Ask First
- Changing `GameContext`, `GameDef`, `ArcadeInput` or `arcade_*` loop functions (affects every game)
- Adding a dependency, bumping raylib (`ARCADE_RAYLIB_TAG`), editing `.github/workflows/`
- Renaming/removing a game or changing leaderboard file format (breaks saved scores)

### Never Do
- Include `raylib.h` / `arcade/gfx.h` from `*_logic.*`, `core/src`, or tests
- Use `rand()`, `time()`, `M_PI`, VLAs, POSIX/Windows-only APIs in shared code
- Call `EndDrawing()` in a game (use `arcade_frame_end()`); skip `arcade_loop_begin(ctx)`
- Commit `build/`, `scores.txt`, secrets; push directly to `main`; edit generated files (`build/**/generated`, `docs/raylib_subset.md`)

## Codebase State
- 8 games run and are unit-tested; each is a **placeholder** with its core mechanic working. Remaining work is marked `TODO(<owner>)`: `grep -rn "TODO(" games core app`.
- Not verified locally: Windows MSVC build and native Windows runs (CI job `windows-msvc` is non-blocking until green once).

## Terminology
| Term | Means |
|------|-------|
| logic / front-end | pure `*_logic.c` rules vs raylib `*_game.c` drawing loop |
| step | one fixed `ARC_DT` (1/60 s) update of game logic |
| monkey | reproducible random `ArcadeInput` for tests and smoke runs |
| smoke run | `arCade --smoke`: scripted input, bounded frames, PNG screenshots |
| ranked | `GameDef.ranked`: result goes to the leaderboard |

## Scoped AGENTS.md (MUST read when working in these directories)
| Directory | File |
|-----------|------|
| `core/` | [core/AGENTS.md](./core/AGENTS.md) |
| `games/` | [games/AGENTS.md](./games/AGENTS.md) (the game contract) |
| `games/<id>/` | [absorb_grow](./games/absorb_grow/AGENTS.md), [beat_dodge](./games/beat_dodge/AGENTS.md), [echo_maze](./games/echo_maze/AGENTS.md), [ghost_loop](./games/ghost_loop/AGENTS.md), [laser_mirrors](./games/laser_mirrors/AGENTS.md), [sandbox](./games/sandbox/AGENTS.md), [sort_it](./games/sort_it/AGENTS.md), [time_shooter](./games/time_shooter/AGENTS.md) |
| `.github/` | [.github/AGENTS.md](./.github/AGENTS.md) |

> **Agents**: when you read or edit files in a listed directory, load its AGENTS.md first.

## When instructions conflict
The nearest `AGENTS.md` wins. Explicit user prompts override files.
