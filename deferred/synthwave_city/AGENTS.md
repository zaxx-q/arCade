<!-- Managed by agent: keep sections and order; edit content, not structure. Last updated: 2026-11-16 -->

# AGENTS.md - Synthwave City (`synthwave_city`)

Scope: this folder only. Repo rules: `../../AGENTS.md`. Game contract: `../AGENTS.md`. Design brief: `GAME.md`. Owner: Leader | Not ranked

## Overview
Synthwave City: a C99 port of the procedural neon megacity of `highsierralabs/Synthwave_City` (MIT). Fly freely or let the tour drone fly; score = whole kilometres flown. The game never ends and is never ranked. Rules and status: `GAME.md`.

**This game has two jobs.** It is a normal game in `deferred/` (listed in the hub's archive, started through `play()`), and it is the hub's live backdrop: `app/backdrop.c` runs it behind the menu as a guest through `synthwave_city_session.h`. The hub needs it by id, so the folder must keep existing (`app/CMakeLists.txt` stops with a clear message otherwise).

## Key Files
- `swc_core.h`, `swc_city.[ch]`, `swc_traffic.[ch]`, `swc_song.[ch]`: pure generators (city plan, traffic, composition); bit-exact with the TypeScript original
- `synthwave_city_logic.[ch]`: pure rules (no raylib): flight, tour, viewpoints, clocks; `init/step/is_over/score`
- `synthwave_city_front.h`: the small interface between the front-end files (no raylib types)
- `synthwave_city_render_game.c`: all 3D drawing (rlgl, no shaders); `synthwave_city_game.c`: loop, HUD, soundtrack, scripted smoke input, the guest session, `game_synthwave_city_def`
- `synthwave_city_session.h`: the hub-facing API (`swc_session_*`, no raylib types): begin/end, who holds the camera, update, draw, audio pause and mute
- `tests/test_swc_city.c`, `test_swc_traffic.c`, `test_swc_song.c`, `test_synthwave_city.c`: unit tests (logic only)

## Setup & environment
`cmake --preset logic` needs no raylib. To play: `cmake --preset dev && cmake --build --preset dev && build/dev/bin/arCade`.

## Build & tests
- `cmake --build --preset logic && ctest --preset logic -R "^synthwave_city\."`
- Smoke run (headless build, CPU rasteriser, about 20 s for 300 frames): `build/headless/bin/arCade --smoke synthwave_city --frames 300 --out smoke-out`
- The guest session and the hub screens: `build/headless/bin/arCade --smoke hub --out smoke-out` (previews `hub_menu`, `hub_menu_sandbox`, `hub_fly`, and the `hub_city` camera hand-off check)

## Code style & conventions
- `synthwave_city_State` is about 2.5 MB: keep it in static storage, never on the stack. `play()` and the guest session each have their own static state and their own `Soundtrack`, so they never share a stream; the two must stay independent (the archive can start `play()` while the hub's guest is alive).
- `swc_render_init()` / `swc_render_shutdown()` are reference counted: the guest and `play()` share one set of textures. Every init needs exactly one shutdown. The smoke check `hub_city` fails if a game started on top of the guest leaves the backdrop blank.
- The hub gives the guest empty input while its menu is up, so only the drone moves the camera. Handing the camera back (Esc) is a lone `action_pressed`, the same press as Space; the logic tests `test_an_idle_host_leaves_the_drone_flying`, `test_a_lone_space_press_hands_the_camera_back_to_the_drone` and `test_the_click_that_takes_the_camera_does_not_turn_it` pin what the hub relies on. Changing the tour/drag/Space rules in `synthwave_city_step` must keep them passing.
- Parity with the web version is the point of the generators: draw RNG values into locals in the original's order, use `double` for generation and `float` only where the original uses `Math.fround`, use stable insertion sorts, and keep `-ffp-contract=off`. Any change to `swc_city.c`, `swc_traffic.c` or `swc_song.c` must keep `test_matches_the_web_version` passing (or update the reference numbers with a reason).
- The city, traffic and song are pure functions of the seed and time; the only state that moves is in `synthwave_city_State`.
- Pure files (everything except `*_game.c`) never include raylib, rlgl, `arcade/gfx.h` or call `rand/srand/time/clock`.
- The renderer is camera-relative and uses only rlgl immediate mode (no shaders): solid pass first, then an additive pass with the depth mask off. Keep the clip planes set and restored around `BeginMode3D`.
- Smoke runs use `scripted_input()` (the headless monkey would stick to walls); keep it touching every control.
- Keep `GAME.md` in sync when mechanics change (the report is written from it).
- Follow the game contract in `../AGENTS.md` (loop pattern, determinism, fixed arrays, `finished` semantics).

## Security & safety
- Only write under `ctx->data_dir`; read assets from `ctx->assets_dir`; validate any file you parse. This game reads and writes no files; the soundtrack is rendered into memory with `synth_render`.
- No art, sound, font or text from the original web app may be added beyond the algorithms ported under its MIT licence (the notice is in `THIRD_PARTY_NOTICES.md`); all drawing is raylib shapes and generated textures, all audio is generated.

## PR/commit checklist
- [ ] New rule has a unit test; `GAME.md` pseudocode, flowchart and concept table still match the code
- [ ] `bash tools/format.sh`, `bash tools/check_repo.sh` and the game's tests are clean
- [ ] Generators still match the web version (`test_matches_the_web_version` in the city and song tests)

## Examples
- Good: `synthwave_city_step(&s, &in, ARC_DT);` called by the front-end and by the unit tests
- Bad: reading the keyboard or calling raylib inside `synthwave_city_logic.c`, or calling `rand()` anywhere in a generator

## When stuck
Read `GAME.md`, copy a pattern from the nearest Golden Sample in the root `AGENTS.md`, run the monkey test to find crashes.
