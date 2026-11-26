<!-- Managed by agent: keep sections and order; edit content, not structure. Last updated: 2026-11-26 -->

# AGENTS.md - games/

## Overview
Every folder `games/<id>/` is one game that is listed on the hub menu, and every folder `deferred/<id>/` is a game with the same contract that is kept off the menu (see `docs/DEFERRED.md`). Both are discovered automatically by CMake (folders starting with `_` are ignored); moving a game folder from one to the other is all it takes to list or unlist it. The rest of this file applies to both. A game = **pure logic** (`<id>_logic.[ch]`) + **raylib front-end** (`<id>_game.c`) + tests + `GAME.md` + `AGENTS.md`. This file is the contract all games follow. A portrait game may use the 480 x 684 playfield in `arcade/playfield.h` (logic in playfield-local pixels, `<id>_init_sized(s, seed, w, h)` for tests); the 8 games ported from simple-games use `GameDef.order` 100..170.

## Key Files
| File (per game) | Role |
|-----------------|------|
| `CMakeLists.txt` | one call: `arcade_add_game(ID <id> LOGIC .. GAME .. TESTS ..)`; folder name must equal `<id>` |
| `<id>_logic.[ch]` | state struct `<id>_State`; `<id>_init(s, seed)`, `<id>_step(s, in, dt)`, `<id>_is_over(s)`, `<id>_score(s)` |
| `<id>_game.c` | `static GameResult play(const GameContext*)` + `const GameDef game_<id>_def` |
| `tests/test_<id>.c` | unit tests of the logic (use `tests/arcade_test.h`) |
| `GAME.md` | design brief, pseudocode, mermaid flowchart, course-concept table (feeds the report) |

Create a new game: `cmake -DID=my_game -DTITLE="My Game" -DOWNER="Name" -P tools/new_game.cmake` (copies `_template/`), then build.

## Setup & environment
Logic-only builds need no raylib: `cmake --preset logic`. To see the game: `cmake --preset dev && cmake --build --preset dev && build/dev/bin/arCade`.

## Build & tests
- One game: `ctest --preset logic -R "^<id>\."` (tests are named `<id>.test_<id>`)
- Every game: `ctest --preset logic`; playable smoke: `build/headless/bin/arCade --smoke <id> --frames 300`
- Contract check: `bash tools/check_repo.sh`

## Code style & conventions
- **Loop pattern** (in `<id>_game.c`): `arcade_loop_begin(ctx); while (arcade_loop_running()) { arcade_poll(&in); step(...); BeginDrawing(); draw; arcade_frame_end(); if (over) break; }`. Never call `EndDrawing()`, `InitWindow()`, `WindowShouldClose()`, or read the keyboard directly.
- **Logic is pure and deterministic**: no raylib, no `rand()`/`time()`, no globals; randomness from an `Rng` seeded with `ctx->seed`; time only through the `dt`/fixed step passed in.
- **Return value**: `finished = true` means the run ended (game over, level complete, or leaving a game that has no end) and its score is always recorded and uploaded: there are no unranked games. ESC in a game that can be lost returns `finished = false` (nothing recorded). A game with no end (`sandbox`, `synthwave_city`) returns `finished = true` when the player leaves after really playing (the sandbox: painted something; the city: flew at least one kilometre), so a quick look and Esc records nothing. `score >= 0`.
- **Separate high-score tables (optional)**: a game with modes or levels can name the table of a run by writing it into `ctx->board` (`GAME_BOARD_LEN` bytes, may be NULL: check) before `play()` returns; the hub then records and ranks the score in `<id>:<board>` only. Leave it empty for one table per game (what every game did before). Example: `beat_dodge` (`bd_board_name`).
- **Memory**: fixed-size arrays (`MAX_*` constants); big state is `static` in `play()`. No `malloc` unless justified in `GAME.md`.
- **Window**: always 1280x720 (`ARC_SCREEN_W/H`), no fullscreen calls. HUD uses `gfx_hud()`; colours come from the `ARC_COL_*` palette.
- **Audio** is optional: guard with `arcade_audio_ready()`; smoke runs and CI have no sound device. Don't read assets when `ctx->headless`.
- **Naming**: function/struct prefix = `<id>_` (exception: `sandbox` uses `sbx_`). `GameDef.order`: 10 steps (menu order, lower first).
- Keep it simple: course topics only (arrays, structs, enums, functions, recursion, files, sorting/searching, pointers). Comments explain *why*.

## Security & safety
- Only write files under `ctx->data_dir`; read assets from `ctx->assets_dir`.
- Validate every file you parse (see `bd_song_load`): reject bad input, never crash.

## PR/commit checklist
- [ ] New/changed rule has a unit test; monkey test still passes
- [ ] `GAME.md` pseudocode, flowchart and concept table match the code
- [ ] `bash tools/format.sh`, `bash tools/check_repo.sh`, `ctest --preset logic` are clean
- [ ] Smoke run of the game works (`--smoke <id>`)

## Examples
- Good: `time_shooter_step(&s, &in, ARC_DT)` is called by the front-end; the unit test calls it with scripted `ArcadeInput`.
- Bad: `if (IsKeyDown(KEY_W))` inside `*_logic.c`; `srand(time(NULL))`; `EndDrawing()` in a game.

## When stuck
Copy the closest Golden Sample from the root `AGENTS.md`, read `docs/ADDING_A_GAME.md`, and run the monkey test to find crashes. Ask the user before changing the contract.
