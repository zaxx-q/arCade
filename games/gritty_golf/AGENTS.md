<!-- Managed by agent: keep sections and order; edit content, not structure. Last updated: 2026-11-13 -->

# AGENTS.md - Gritty Golf (`gritty_golf`)

Scope: this folder only. Repo rules: `../../AGENTS.md`. Game contract: `../AGENTS.md`. Design brief: `GAME.md`. Owner: zaxx-q (upstream author) | Ranked

## Overview
Pixel mini-golf: 25 hand-made courses, three strikes per attempt, freeze the ball, aim, set the power. The rules are a **vendored engine** (`gg/`, see `gg/VENDORED.md`); this folder adds an arCade adapter, front-end and sound. Rules and status: `GAME.md`.

## Key Files
- `gg/`: vendored upstream engine (physics, levels, save files). **Do not edit or reformat it**: 7 documented patches only (4 functional, 3 comment-only cppcheck suppressions), listed in `gg/VENDORED.md`
- `gritty_golf_logic.[ch]`: pure adapter: `ArcadeInput` -> engine `Input`, `init/step/is_over/score`, profile load/save, screen layout rectangles
- `gritty_golf_game.c`: raylib loop + `game_gritty_golf_def`
- `gritty_golf_render_game.c`: drawing (256x256 render texture, 2x blit, side panels). `gritty_golf_audio_game.c`: sound effects. `gritty_golf_front.h`: their raylib-free interface
- `tests/test_gritty_golf.c`: adapter tests. `tests/gg/`: upstream's own tests (run as `gritty_golf.engine`)
- Assets: `../../assets/gritty_golf/` (textures, Silkscreen bitmap font, 6 WAVs)

## Setup & environment
`cmake --preset logic` needs no raylib. To play: `cmake --preset dev && cmake --build --preset dev && build/dev/bin/arCade`.

## Build & tests
- `cmake --build --preset logic && ctest --preset logic -R "^gritty_golf\."` (two tests: `gritty_golf.engine`, `gritty_golf.adapter`; ~6 s in Debug)
- Smoke run (headless build): `build/headless/bin/arCade --smoke gritty_golf --frames 300 --out smoke-out`. It keeps the profile in memory and writes no file.

## Code style & conventions
- The engine is fixed-step: one `gritty_golf_step()` call is exactly one engine tick (1/60 s = `ARC_DT`). `dt` is ignored on purpose. Never feed it a different step size.
- Layout rectangles come from ONE function each (`gritty_golf_rect_*`), used by both the drawing and the click handling. Change them there, nowhere else.
- ESC always goes to the hub (the arCade loop owns it). Stage select is `L` or the on-canvas button. Finish is `F` or the panel button and needs a second press within `GG_FINISH_CONFIRM_FRAMES`.
- Score = engine `Save_Score` (1000 per cleared level - 40 per best stroke - 2 per best second, min 0): higher is better. `finished` is true only when the player finished AND `score > 0`.
- Persistence: only through `gritty_golf_open_profile()` under `ctx->data_dir` (`gritty_save_<player>.dat`). An empty save path means "never touch the disk": tests and smoke runs rely on it.
- Determinism: keep `-ffp-contract=off` on the logic target (see `CMakeLists.txt`). The replay solutions in `tests/gg/solutions.h` depend on it.
- The hub owns the audio device: no `InitAudioDevice`/`CloseAudioDevice`/`SetMasterVolume` here; guard with `arcade_audio_ready()`.
- The engine uses `calloc` for the current level (one `free` in `gritty_golf_shutdown`): the `malloc` justification the contract asks for is in `GAME.md`.
- Keep `GAME.md` in sync when mechanics change (the report is written from it).
- Follow the game contract in `../AGENTS.md` (loop pattern, determinism, `finished` semantics).

## Security & safety
- Only write under `ctx->data_dir`; read assets from `ctx->assets_dir`. Save files are validated by the engine (`Save_Load`: size, magic, version, checksum); a damaged file is renamed to `*.bad`, never trusted.
- Licensing: the level data and the 6 WAVs are **not MIT** (Escada Games, per the upstream README). See `gg/VENDORED.md` and `THIRD_PARTY_NOTICES.md` before shipping them.

## PR/commit checklist
- [ ] New rule has a unit test; `GAME.md` pseudocode, flowchart and concept table still match the code
- [ ] `bash tools/format.sh`, `bash tools/check_repo.sh` and the game's tests are clean
- [ ] `gg/` untouched, or the change is a new row in the patch table of `gg/VENDORED.md`

## Examples
- Good: `gritty_golf_step(&s, &in, ARC_DT);` called by the front-end and by the unit tests with scripted `ArcadeInput`
- Bad: calling `Game_Tick()` or `Save_Write()` directly from `gritty_golf_game.c`, or editing a physics constant inside `gg/`

## When stuck
Read `GAME.md` (Implementation steps) and `gg/VENDORED.md`, copy a pattern from the nearest Golden Sample in the root `AGENTS.md` (`games/sandbox/` for engine reuse and big static state), run the monkey test to find crashes.
