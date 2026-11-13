# gg/ - vendored Gritty Golf engine

This folder is a **copy of a standalone project**, kept as close to the original as possible so it can be re-synced.
Do not "clean it up": a tidy-up here becomes a merge conflict at the next re-sync.

| | |
|---|---|
| Upstream | `gritty-golf` ("Gritty Golf: C Edition", `project(gritty_golf VERSION 1.0.0)`), github.com/zaxx-q/gritty-golf |
| Upstream language | C11 + raylib 5.5 (arCade is C99 + raylib 6.2: the engine files compile as C99 without changes) |
| Which files | upstream `src/{algo,bmfont,game,level,mathx,palette,physics,player,raster,records,save,util}.[ch]`, `src/{common.h,vec2.h,palette_colors.h,audio.h}` and `data/levels.[ch]` |

## Why the folder is called `gg/` and not `gritty_golf_...`

`games/AGENTS.md` asks for the prefix `<id>_` on every function and struct. The engine's own names
(`Game_Tick`, `Save_Score`, `Level`, `Input`, ...) are the upstream API and the upstream tests call them directly, so
renaming every symbol would make each future re-sync a hand-merge. The rule applies to the arCade-written adapter
files (`gritty_golf_logic.[ch]`, `gritty_golf_game.c`, `gritty_golf_render_game.c`, `gritty_golf_audio_game.c`), which do use
the `gritty_golf_` prefix. The same exception already exists for `sandbox` (`sbx_`).

## What is NOT vendored, on purpose

| Upstream file | Why it is left out |
|---------------|--------------------|
| `src/app.[ch]`, `src/main.c` | own window, menu, command line and name prompt: the arCade hub does all of that |
| `src/console.[ch]`, `tests/test_console.c` | the text-mode console menu and "Physics Lab" (needs stdin/stdout loop, not part of the arcade game) |
| `src/audio.c` | rewritten as `../gritty_golf_audio_game.c` (the hub owns the audio device) |
| `src/render.[ch]`, `src/ui.[ch]` | ported to `../gritty_golf_render_game.c` (arCade HUD, 2x canvas, side panels, no Z/X/C keys) |
| `data/level_data.json`, `tools/`, `docs/`, `Makefile`, `CMakeLists.txt` | build-time/authoring material, not needed to run |
The assets (`assets/audio`, `assets/font`, `assets/textures`) live in the arCade tree under `assets/gritty_golf/`.

## Patches against upstream (10 in total)

Anything not listed here must be byte-identical to upstream. Patches 5 to 7 only add comments; the CI cppcheck version can differ from the one used here (2.13), so a future run may need more of them: add each one to this table. Check with `diff -r` (see "Re-sync" below).

| # | File | Change | Why |
|---|------|--------|-----|
| 1 | `levels.h` | the `typedef struct { float x; float y; } Vec2;` is wrapped in `#ifdef GG_VEC2_FROM_ARCADE ... #include "arcade/mathx.h" ... #else <the original typedef> #endif` | arCade already has a `Vec2` of the same layout; two typedefs of the same name in one translation unit are a compile error. `CMakeLists.txt` defines `GG_VEC2_FROM_ARCADE` for the game targets. Standalone, nothing changes. |
| 2 | `game.c` | new `static void write_save(Game *g)` that calls `Save_Write(&g->save, g->save_path)` only when `g->save_path[0] != '\0'`; both former `Save_Write` call sites call it instead | arCade tests and headless smoke runs must never write files. An empty save path now means "keep the profile in memory". Upstream behaviour is unchanged when a path is given. |
| 3 | `../tests/gg/test_font_palette.c` | the font path is built from the `GG_ASSETS_DIR` compile definition (`"%s/font/%s"`) instead of the relative `assets/font/...` | the test must find the assets from any working directory (`ctest` runs inside the build tree) |
| 4 | `../tests/gg/test_main.c` | the console test suite is removed from the list | the console is not vendored (see above) |
| 5 | `mathx.c` | one comment line `cppcheck-suppress arrayIndexOutOfBoundsCond` above `if (memo[n] != 0)` in `Math_Fibonacci` | CI runs cppcheck over every `games/**` file and fails on a finding. It is a false positive: `n` is clamped to 0..90 two lines earlier. Comment only, no code change. |
| 6 | `save.c` | one comment line `cppcheck-suppress arrayIndexThenCheck` above the `for` loop in `Save_PathFor` | same reason: `name` is a C string, so it ends before `name[i]` can pass the bound. Comment only. |
| 7 | `../tests/gg/test_algo.c` | two comment lines `cppcheck-suppress uninitvar` above the two `qsort(ref, ...)` calls | same reason: `ref[0..n-1]` is filled by the loop right above. Comment only. |
| 8 | `save.c` | replace the C11 `_Static_assert` with a C99 typedef-based compile-time size check | arCade compiles all C sources as strict C99, and MinGW rejects `_Static_assert` with `-Wpedantic -Werror`. |
| 9 | `../tests/gg/test_files.c` | check `fopen()` results before corrupting save and records test files | Cppcheck reports the test's deliberate file operations as possible null-pointer dereferences when resource allocation fails. |
| 10 | `../tests/gg/test_font_palette.c` | check the temporary font file `fopen()` result before writing | Cppcheck reports a possible null-pointer dereference when resource allocation fails. |

## Build notes

* `-ffp-contract=off` is added for the logic target on non-MSVC compilers. The engine is deterministic (the bundled
  solutions in `tests/gg/solutions.h` replay a shot sequence and must hit the exact same cell), and a fused
  multiply-add would change the last bits of the physics. Do not drop it in the preset flags.
* The engine allocates the current level with `calloc` (`level.c`) and a scratch array in `algo.c`: this is the
  "`malloc` justified in `GAME.md`" case of the game contract. Every allocation has one `free`, and the adapter's
  `gritty_golf_shutdown()` is the single release point.
* `util.c` (`Str_Today`) calls `time()`/`localtime()` to date a record. It is **not** on the gameplay path:
  its only caller is `Records_SubmitProfile` (`records.c`), which upstream calls from `main.c` (not vendored) and
  from the engine's own tests (they write to a temp file). `gritty_golf_step()` never reaches it, and the arCade
  leaderboard replaces the upstream records file. `tools/check_repo.sh` scans only `*_logic.[ch]` for `time()`, so
  `gg/` is exempt: keep it that way by never calling `Records_SubmitProfile` or `Str_Today` from the adapter.
* `.clang-format` in this folder contains `DisableFormat: true`, so `tools/format.sh` leaves vendored files untouched
  (same for `../tests/gg/`).

## Re-sync with upstream

1. Unpack the new upstream next to the old copy.
2. `diff -r <old upstream>/src games/gritty_golf/gg` should show only the files and patches listed above. If it shows
   more, someone edited vendored code: undo it or add it to the table.
3. Copy the new `src/*.[ch]` (minus the files in "NOT vendored") and `data/levels.[ch]` over `gg/`.
4. Re-apply patches 1, 2, 5 and 6 (patches 3, 4 and 7 live in `tests/gg/`: same procedure with upstream `tests/`). Then run the CI cppcheck command from `.github/workflows/checks.yml` (job `static-analysis`) to see which suppressions are still needed.
5. Compare the new `src/render.c`, `src/ui.c` and `src/audio.c` with `gritty_golf_render_game.c` and
   `gritty_golf_audio_game.c` by eye; port what matters.
6. `cmake --preset logic && cmake --build --preset logic && ctest --preset logic -R "^gritty_golf\."` must pass, both
   `gritty_golf.engine` (upstream's own tests) and `gritty_golf.adapter` (arCade's).
