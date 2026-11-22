<!-- Managed by agent: keep sections and order; edit content, not structure. Last updated: 2026-11-22 -->

# AGENTS.md - Synthwave City (`synthwave_city`)

Scope: this folder only. Repo rules: `../../AGENTS.md`. Game contract: `../AGENTS.md`. Design brief: `GAME.md`. Owner: Leader | Not ranked

## Overview
Synthwave City: a C99 port of the procedural neon megacity of `highsierralabs/Synthwave_City` (MIT). Fly freely or let the tour drone fly; score = whole kilometres flown. The game never ends and is never ranked. Rules and status: `GAME.md`.

## Key Files
- `swc_core.h`, `swc_city.[ch]`, `swc_traffic.[ch]`, `swc_song.[ch]`: pure generators (city plan, traffic, composition); bit-exact with the TypeScript original
- `synthwave_city_logic.[ch]`: pure rules (no raylib): flight, tour, viewpoints, clocks; `init/step/is_over/score`
- `synthwave_city_front.h`: the small interface between the front-end files (no raylib types)
- `swc_pack.[ch]`, `swc_shapes.[ch]`: pure data of the GPU buildings layer: instance attributes per box and roof light (same numbers as the web version's `pack.ts`), and the unit box and cylinder
- `swc_ground.[ch]`: pure data of the GPU ground layer: where the street lamps stand (and their colours), the disc the street shader is drawn on, and the one lamp-post mesh; same numbers as the web version's `layers/ground/layout.ts`
- `swc_beams.[ch]`: pure data of the GPU beams layer: which sites are lit, and the seven attributes per beam (searchlights, pillars, laser fans); same numbers as the web version's beams `pack.ts`, and the CPU mirror of the direction function in the shader
- `swc_air.[ch]`: pure data of the GPU air traffic layer: the craft of every sky lane (16 floats each, sorted by shape), the routes of the six hero ships, the hull meshes (lofts of rings), the light tables, and the CPU mirror of the motion in the vertex shaders; same numbers as the web version's `layers/airtraffic/pack.ts` and `shapes.ts`. The layer's seed is the city seed mixed with the layer name (`swc_air_layer_seed`), as the web engine does.
- `swc_street.[ch]`: pure data of the GPU street life layer: the lane table, the CPU mirror of the shader's hash, the field of near cars and the crowd field (nearest first), the vehicle mesh; same numbers as the web version's `layers/streetlife/`
- `swc_holo.[ch]`: pure data of the GPU holograms layer: the atlas layout (28 cells in 2048 x 2048), the ad panels (21 floats each), the ticker rings, the wire solids (about 103 thousand vertices); same numbers as the web version's `layers/holograms/` (`layout.ts`, `pack.ts`, `wire.ts`)
- `swc_raster.[ch]`, `swc_font.[ch]`, `swc_atlas.[ch]`: the ad atlas drawn by code at start-up (about 0.14 s): a small software 2D rasteriser with a canvas-like model (paths, fills, strokes, gradients, clips, transforms), a stroke-built bold sans face (the letters are this port's own, not the web version's system font), and the 16 wide ads, 8 tall ads and 4 ticker strips ported from `atlas.ts`. Cells are independent of drawing order. No image or font file is read.
- `swc_layers.[ch]`: pure states of the layer switch (the `L` key cycles everything, beams only, air only, street only, holograms only, nothing: six states) and their HUD labels
- `swc_look.[ch]`: pure look of the GPU renderer: the three moods in linear light, their easing, the neon palette, view and projection matrices
- `synthwave_city_render_game.c`: the CPU renderer (rlgl immediate mode, no shaders); also supplies the city geometry to the GPU frame until the GPU layers replace it. It is the fallback and what headless smoke runs use.
- `synthwave_city_render_gpu_game.c`, `swc_gpu.h`, `swc_gpu_core_game.c`, `swc_gpu_sky_game.c`, `swc_gpu_buildings_game.c`, `swc_gpu_beams_game.c`, `swc_gpu_ground_game.c`, `swc_gpu_air_game.c`, `swc_gpu_street_game.c`, `swc_gpu_holo_game.c`, `swc_gpu_post_game.c`: the GPU renderer (GLSL 3.30 through rlgl): HDR target, sky, instanced buildings and their lights, instanced beam ribbons and flares, the ground (street shader on a disc, instanced lamp posts, lamp glows) with the half-size mirror pass the wet streets read, the air traffic (instanced craft hulls, engine lights and trails, hero ships and their lights; also mirrored), street life (a grid of cars worked out in the vertex shader from a lane table, the crowd, lamps and light on the road), holograms (instanced ad panels, wire solids, ticker rings, all additive, read the atlas as a linear half-float texture; mirrored), 7-level bloom, final composite
- `shaders/*`: GLSL sources; `embed_shaders.cmake` turns them into `<build>/swc_shaders.c` at configure time (byte arrays, so no C99 string-length warning); `swc_shaders.h` is its header
- `synthwave_city_game.c`: loop, HUD, soundtrack, scripted smoke input, renderer choice (`G` key), layer switch (`L` key), headless-only debug switches, `game_step()`/`game_draw()` helpers (shared by `play()` and the guest session), the `swc_session_*` guest API, `game_synthwave_city_def`
- `synthwave_city_session.h`: the guest session API (no raylib types): `swc_session_begin/update/draw/end`, `set_control`, audio pause/mute. The hub runs the city behind its menu through this header (`app/backdrop.c`).
- `tests/test_swc_city.c`, `test_swc_traffic.c`, `test_swc_song.c`, `test_swc_look.c`, `test_swc_pack.c`, `test_swc_shapes.c`, `test_swc_beams.c`, `test_swc_ground.c`, `test_swc_air.c`, `test_swc_street.c`, `test_swc_holo.c`, `test_swc_raster.c`, `test_swc_font.c`, `test_swc_atlas.c`, `test_swc_layers.c`, `test_synthwave_city.c`: unit tests (logic only)

## Setup & environment
`cmake --preset logic` needs no raylib. To play: `cmake --preset dev && cmake --build --preset dev && build/dev/bin/arCade`.

## Build & tests
- `cmake --build --preset logic && ctest --preset logic -R "^synthwave_city\."`
- Smoke run (headless build, CPU rasteriser, about 20 s for 300 frames): `build/headless/bin/arCade --smoke synthwave_city --frames 300 --out smoke-out`. The software rasteriser reports `RL_OPENGL_SOFTWARE`, so this always takes the CPU path; its image must stay unchanged by GPU work (only the HUD help text may change, when a key is added).
- GPU frames cannot come from CI. Check them locally by building raylib against a real GL 3.3 context (Mesa llvmpipe is enough) and running the same `--smoke` with the debug switches below, then compare with a render of the web version at seed 1984, clock frozen at 12, one layer at a time (SSIM and MAE over rows 40 to 535, outside the HUD).
- Debug switches, read only when `ctx->headless`: `SWC_RENDERER=cpu`, `SWC_VIEW=1..6`, `SWC_MOOD=0..2`, `SWC_FREEZE=<seconds>`, `SWC_LAYERS=<bit 1 sky, bit 2 legacy city, bit 4 GPU buildings, bit 8 GPU beams, bit 16 GPU ground, bit 32 GPU air traffic, bit 64 GPU street life, bit 128 GPU holograms; default 255>`, `SWC_POST=0`, `SWC_BLOOM=0`, `SWC_REFLECT=0`, `SWC_LIVE=0..15` (the layer switch of the `L` key: 1 beams, 2 air traffic, 4 street life, 8 holograms; works on both renderers), `SWC_POSE=x,y,z,yaw,pitch` (hold that pose instead of a viewpoint). Never read them in a real window.
- The `L` key is a real feature, not a switch for the harness: `swc_live_layers_set()` (in `synthwave_city_render_game.c`) holds the mask and both renderers leave out what is cleared, whoever would have drawn it (the GPU pass through its `show_*` checks, the CPU renderer through its `g_no_*` flags). A layer added later joins it by adding a bit to `swc_layers.h`.

## Code style & conventions
- `synthwave_city_State` is about 2.5 MB: keep it in static storage, never on the stack.
- Parity with the web version is the point of the generators: draw RNG values into locals in the original's order, use `double` for generation and `float` only where the original uses `Math.fround`, use stable insertion sorts, and keep `-ffp-contract=off`. Any change to `swc_city.c`, `swc_traffic.c` or `swc_song.c` must keep `test_matches_the_web_version` passing (or update the reference numbers with a reason).
- The city, traffic and song are pure functions of the seed and time; the only state that moves is in `synthwave_city_State`.
- Pure files (everything except `*_game.c`) never include raylib, rlgl, `arcade/gfx.h` or call `rand/srand/time/clock`.
- The CPU renderer is camera-relative and uses only rlgl immediate mode (no shaders): solid pass first, then an additive pass with the depth mask off. Keep the clip planes set and restored around `BeginMode3D`. Leave its output alone: it is the fallback and the smoke image.
- The GPU renderer draws in linear light into an RGBA16F target and does tone, grade and sRGB only in the last pass (`post.frag`), as the web version does. Shader text lives in `shaders/`; a new file there is picked up at the next CMake configure. Only `*_game.c` files include raylib or rlgl; `swc_gpu.h` and `swc_look.h` stay plain C. A GPU failure (shader compile, incomplete framebuffer) must fall back to the CPU renderer, never abort.
- Shaders taken from the web version need no per-file attribution header: the MIT notice in `THIRD_PARTY_NOTICES.md` covers them. The layer-by-layer match against the web version is the acceptance test of every GPU layer, so keep the reference method above working.
- Smoke runs use `scripted_input()` (the headless monkey would stick to walls); keep it touching every control.
- Keep `GAME.md` in sync when mechanics change (the report is written from it).
- Follow the game contract in `../AGENTS.md` (loop pattern, determinism, fixed arrays, `finished` semantics).

## Security & safety
- Only write under `ctx->data_dir`; read assets from `ctx->assets_dir`; validate any file you parse. This game reads and writes no files; the soundtrack is rendered into memory with `synth_render`.
- Code and text of the original web app (algorithms, GLSL shader source, hologram ad names) may be used under its MIT licence with the notice in `THIRD_PARTY_NOTICES.md`. No images, fonts or audio files from it may be added; all drawing is generated by code and all audio is synthesised.

## PR/commit checklist
- [ ] New rule has a unit test; `GAME.md` pseudocode, flowchart and concept table still match the code
- [ ] `bash tools/format.sh`, `bash tools/check_repo.sh` and the game's tests are clean
- [ ] Generators still match the web version (`test_matches_the_web_version` in the city and song tests)

## Examples
- Good: `synthwave_city_step(&s, &in, ARC_DT);` called by the front-end and by the unit tests
- Bad: reading the keyboard or calling raylib inside `synthwave_city_logic.c`, or calling `rand()` anywhere in a generator

## When stuck
Read `GAME.md`, copy a pattern from the nearest Golden Sample in the root `AGENTS.md`, run the monkey test to find crashes.
