# Arcade Hub

A hub of 8 small, unusual games written in **plain C99** with **raylib 6.2**: an animated falling-sand menu, a persistent leaderboard, and one game per teammate. Built for the *Dasar Pemrograman* final project (procedural programming in C), designed so the leader can keep adding games without touching shared files.

> **Handoff note.** This repository is a working starting point, not a finished product: every game runs, is unit-tested and renders, but each is a *placeholder* with its core mechanic working. The remaining work is listed below and marked in code with `TODO(<owner>)`. Search with `grep -rn "TODO(" games core app`.

## Quick start
```bash
# 1. tools: see docs/DEVELOPMENT.md (gcc + g++, CMake >= 3.25, Ninja; Linux also needs X11/GL dev packages)
cmake --preset logic && cmake --build --preset logic && ctest --preset logic      # fast, no download: expect "100% tests passed ... out of 12"
cmake --preset dev && cmake --build --preset dev                                   # downloads raylib 6.2 once
build/dev/bin/arcade-hub                                                           # Windows: build\dev\bin\arcade-hub.exe
```
`arcade-hub --list` prints the registered games, `--version` the version, `--smoke all` runs every game automatically (see Testing).

## What is in the box
| Game (`id`) | Owner | Hook | State |
|-------------|-------|------|-------|
| `time_shooter` | Member 1 | time only moves when you move | core mechanic, particles, shake |
| `echo_maze` | Member 2 | black maze, sonar pings reveal walls (line of sight) | recursive generator, pings, win |
| `laser_mirrors` | Member 3 | rotate mirrors to bounce a beam, splitters branch | one solvable level, recursive tracing |
| `ghost_loop` | Member 4 | your past runs replay as ghosts that hold doors open | record/replay, pad + door puzzle |
| `beat_dodge` | Member 5 | dodge obstacles that arrive exactly on the beat | beat clock, map file, synthesised tick |
| `absorb_grow` | Member 6 (+ report) | swallow the small, avoid the big | growth rules, respawn scaling |
| `sort_it` | Leader | be the sorting algorithm: fewest swaps | 3 rounds, par and algorithm comparison |
| `sandbox` | Leader | falling-sand toy (sand, water, stone) | v1 working; same engine animates the hub |

Plus: hub (`app/`), leaderboard (file I/O, insertion sort, linear + binary search), CI/release automation, and docs. Backlog for the leader's own additions: orbital slingshot, territory claim, (anything via `tools/new_game.cmake`).

## Layout
```
core/        pure-C library (rng, input, leaderboard, sandbox engine, sortlib, fx, cli) + core/gfx (raylib helpers)
app/         the hub executable: menu, leaderboard screen, smoke runner
games/<id>/  <id>_logic.[ch] (pure) + <id>_game.c (raylib) + tests/ + GAME.md + AGENTS.md      games/_template = scaffold source
cmake/ tools/ tests/ docs/ assets/ packaging/ .github/ .vscode/ .githooks/
AGENTS.md    instructions for AI coding agents (root + core/ + games/ + each game + .github/)
```
Why the logic/front-end split: it is what makes automated tests meaningful for visual games, and it keeps the drawing layer replaceable (if the lecturer rejects raylib, only `core/gfx` and `*_game.c` need replacing). Details: `docs/ARCHITECTURE.md`.

## Testing (what CI runs)
| Lane | When | What |
|------|------|------|
| fast | every PR | clang-format check, repo rules, Linux gcc (logic then full build, `-Werror`), Windows MinGW gcc, PR title check |
| full | push to `main`, PR labelled `full-ci`, manual | fast + ASan/UBSan (clang), cppcheck, version-script + add-a-game tests, MSVC compile (non-blocking), headless smoke on Linux and Windows (non-blocking) with PNG screenshots as artifacts |

Locally: the table in `AGENTS.md` ("Commands"). CI cannot judge whether a game is fun; it checks rules, determinism, crashes and that every game draws something.

## Versions and releases
Tags `vX.Y.Z`; the bump comes from Conventional Commits (`feat` minor, `feat!` major, otherwise patch, `[skip release]` skips). Every push to `main` that passes the full checks publishes a GitHub Release with `arcade-hub-vX.Y.Z-linux-x86_64.tar.gz`, `...-windows-x86_64.zip`, `SHA256SUMS.txt`. Manual run with bump/prerelease/dry-run inputs: Actions -> Release. Nothing ever commits back to the repo. See `docs/RELEASING.md`.

## Remaining work (in this order)
**A. Repository setup (Leader, ~30 min, by hand on GitHub)** - follow `docs/RELEASING.md` "One-time repository setup":
1. `git init`, commit, create a private repo under your personal account (Education plan), push `main`.
2. Squash-merge only; protect `main`; require the check **`CI OK`**; create the `full-ci` label.
3. Open Actions -> Release -> *Run workflow* with `dry_run = true`. Fix anything that is red (see "Not verified" below), then merge a small PR titled `chore: first release` to get **v0.1.0**.
4. Make each teammate install the toolchain (`docs/DEVELOPMENT.md`), and run the 3 quick-start commands on their own machine (Windows and Linux).

**B. Ask the lecturer** whether the raylib graphics library is allowed (suggested message in Indonesian: *"Pak/Bu, untuk project akhir, apakah boleh menggunakan library grafis raylib (bahasa C) untuk tampilan game?"*). If not, the logic layer survives; only `core/gfx` and `*_game.c` need a different front-end.

**C. Each owner: turn the placeholder into the real game.** Open `games/<id>/GAME.md` -> "Implementation steps", grep your `TODO(owner)`, and for every new rule: write the test first, then the logic, then the drawing. "Done" for the report means: mechanic beyond the placeholder, tests green, `GAME.md` pseudocode/flowchart/concept table updated, one good screenshot.

**D. Leader: extras** - add games from the backlog with `cmake -DID=... -DTITLE=... -DOWNER=... -P tools/new_game.cmake` (`docs/ADDING_A_GAME.md`), extend the sandbox (`docs/SANDBOX.md`: fire, oil, lava, plant, acid), add more beat maps in `assets/beatmaps/`, add more levels to laser_mirrors.

**E. After the first green CI runs** - promote `windows-msvc` and `smoke` to blocking (delete `continue-on-error` in `.github/workflows/checks.yml`); check that Windows zips run on a clean machine.

**F. Report and video** - `docs/REPORT_GUIDE.md` maps every report section to files in this repo (pseudocode and flowcharts already live in each `GAME.md`). Make the repository public (or add the lecturer) before submission.

## How to verify the starter
| Check | Command | Expected |
|-------|---------|----------|
| unit tests | `ctest --preset logic` | 12 tests, all passed |
| contract + portability rules | `bash tools/check_repo.sh` | `check_repo: OK (8 games)` |
| formatting | `bash tools/format.sh --check` | `format: OK` |
| version logic | `bash tools/tests/test_next_version.sh` | `21 passed, 0 failed` |
| add-a-game | `bash tools/tests/test_new_game.sh` | `new_game test: OK` |
| hub registry | `build/dev/bin/arcade-hub --list` | 8 games, last line `8 game(s)` |
| headless smoke | `build/headless/bin/arcade-hub --smoke all --frames 300 --out smoke-out` | 8 lines ending `-> OK`, exit code 0, PNGs in `smoke-out/` |

## Key decisions
| Decision | Why |
|----------|-----|
| Plain C99, strict warnings, no C++/POSIX/Windows-only APIs | the course is C; identical behaviour on Linux and Windows; errors found by `-Werror` and cppcheck |
| raylib 6.2 via CMake `FetchContent` (pinned tag) | no manual library install on 7 laptops; 6.x adds a CPU renderer that enables headless CI |
| Logic/front-end split + deterministic logic (own PCG32 RNG, fixed step) | unit-testable and replayable (ghosts, tests, smoke runs) |
| Auto-registered games | the leader can add games and 7 people can work in parallel without merge conflicts |
| Tags as version source, no bump commits | no workflow loops, no conflicts; the PR title decides the release |
| Release only from `main` after the full lane | broken code never becomes a download |
| MinGW (MSYS2 UCRT64) as the Windows toolchain, MSVC as a compile check | same compiler family as Linux; MSVC catches VLAs and other gcc-only habits |
| AGENTS.md only (no CLAUDE.md/GEMINI.md mirrors) | requested; Copilot, Cursor, Codex, Roo read AGENTS.md natively. Claude Code, Cline and Continue do not: if a teammate uses one of them, add a one-line pointer file for that tool |
| Audio synthesised in code, no asset files | nothing to license; games stay silent if there is no sound device |

## Verified in the build sandbox (Linux x86_64) vs. not verified
Verified: desktop build with gcc 13 `-Werror`; 12/12 unit tests; ASan+UBSan (clang 18); cppcheck 2.13; clang-format 18.1.8; actionlint 1.7.12 on all workflows; 8/8 games in the headless smoke run and under `xvfb` with real OpenGL; the interactive hub driven by synthetic keystrokes (name entry, menu, launching a game, ESC, result screen, leaderboard, quit); Windows cross-compile with MinGW (static link, `-Werror`); `next_version.sh` (21 cases); scaffold test.
**Not verified (first things to check when CI runs):** GitHub-hosted runs themselves (MSYS2 setup, `windows-latest`, the `msvc-dev-cmd` job, the publish step); a native Windows run of the exe; MSVC warnings (the job does not use `-Werror` yet); real audio output (no sound device here); macOS and Wayland (not targeted). Newer compilers on the CI runners (MSYS2 gcc, Ubuntu gcc/clang) may add warnings that `-Werror` turns into errors: fix the code, or as a stopgap pass `-DARCADE_WERROR=OFF` in the preset.

## More documentation
`docs/DEVELOPMENT.md` setup and troubleshooting - `docs/ARCHITECTURE.md` - `docs/ADDING_A_GAME.md` - `docs/RELEASING.md` - `docs/REPORT_GUIDE.md` - `docs/SANDBOX.md` - `docs/raylib_subset.md` (exact raylib 6.2 signatures; regenerate with `tools/gen_raylib_subset.sh`) - `THIRD_PARTY_NOTICES.md`.
