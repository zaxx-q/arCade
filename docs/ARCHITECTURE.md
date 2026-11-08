# Architecture

## Layers
```
app/ (hub: menu, leaderboard, smoke runner)  ->  games/<id>/<id>_game.c (raylib front-end)  ->  core/gfx (raylib helpers)  ->  raylib
                                                          |
                                                          v
                                  games/<id>/<id>_logic.c (pure C)  ->  core/src (pure C: rng, input, leaderboard, sandbox, sortlib, fx)
```
The arrow never goes the other way: **pure code never includes raylib**. CMake enforces it (`arcade_core` and `game_<id>_logic` libraries do not link raylib) and `tools/check_repo.sh` double-checks.

## The game contract (`core/include/arcade/game.h`)
Every game exports one `const GameDef game_<id>_def` containing `play(const GameContext*)`. The hub calls `play`, which runs the whole game loop and returns a `GameResult {score, finished}`. `ranked` games with `finished == true` are written to `scores.txt`.

Inside `play`, every game follows the same loop:
```c
arcade_loop_begin(ctx);
while (arcade_loop_running()) {          // false on ESC, window close, or max_frames (smoke)
    ArcadeInput in; arcade_poll(&in);    // keyboard/mouse OR scripted "monkey" input in smoke runs
    xxx_step(&s, &in, ARC_DT);           // pure logic, fixed 1/60 s step
    BeginDrawing(); ...draw...; arcade_frame_end();   // EndDrawing + PNG snapshot in smoke runs
    if (xxx_is_over(&s)) break;
}
```

## Automatic game registry
`games/CMakeLists.txt` globs `games/<id>/CMakeLists.txt`; each calls `arcade_add_game(...)` (`cmake/ArcadeGame.cmake`), which builds `game_<id>_logic` (pure), `game_<id>` (raylib) and one test executable per test file, and records the id. `cmake/ArcadeRegistry.cmake` generates `build/<preset>/generated/game_registry.c` (an array of `&game_<id>_def`). The hub links every game and sorts the array by `GameDef.order`, then id. Nobody edits a shared file to add a game.

## Testing pyramid
1. **Unit tests** (`ctest`, pure code, any OS, no raylib): rules, determinism, "every seed is solvable", file parsers, a monkey test per game (thousands of random inputs, invariants must hold).
2. **Sanitizers**: the same tests under ASan + UBSan (clang, Linux).
3. **Smoke runs** (`arcade-hub --smoke all`): each game runs N frames with scripted input and exports PNGs; the run fails if a game crashes or draws a single flat colour. Two backends: raylib 6's CPU renderer (`headless` preset, no GPU/window) and a real window under `xvfb`.
4. **Compile checks**: gcc (Linux), MinGW gcc (Windows), MSVC (non-blocking), `-Werror`.
What CI cannot check: whether a game is fun, or exact pixel output.

## Headless rendering notes
`PLATFORM=Memory` (raylib >= 6.0) renders into a memory framebuffer. Its read-back is BGRA and bottom-up, so `core/gfx/gfx.c:snapshot()` normalises it (only when `ARCADE_HEADLESS_BUILD` is defined). Audio is disabled in smoke runs.

## Data files
- `scores.txt` (next to the exe, or `--data-dir`): one line `game|player|score|unix_time`; corrupt lines are skipped.
- `assets/beatmaps/*.txt`: beat maps for Beat Dodge (format documented in `beat_dodge_logic.h`).

## Versioning
`ARCADE_VERSION` (CMake cache variable) is baked into `arcade/version.h`; CI passes the computed `X.Y.Z`; local builds use `git describe` or `0.0.0-dev`. Shown on the menu and by `--version`.
