# Adding a game (10 minutes)

1. **Scaffold**: `cmake -DID=my_game -DTITLE="My Game" -DOWNER="Your Name" -P tools/new_game.cmake`
   (`ID`: lowercase letters, digits, underscores; becomes the folder, file prefix and leaderboard key).
2. **Build it**: `cmake --preset logic && cmake --build --preset logic && ctest --preset logic -R "^my_game\."` - the template is a working "collect the pellets" game with tests. No registration step: CMake finds `games/my_game/` by itself.
3. **Play it**: `cmake --preset dev && cmake --build --preset dev && build/dev/bin/arCade` -> it appears on the menu. (`--list` shows all registered games.)
4. **Replace the mechanic** in `my_game_logic.c` (pure C, deterministic, no raylib) and write the tests first or alongside. Keep the 4 functions: `init`, `step`, `is_over`, `score`.
5. **Draw it** in `my_game_game.c` with the `gfx_*` helpers and the `ARC_COL_*` palette. Keep the loop calls (`arcade_loop_begin`, `arcade_poll`, `arcade_frame_end`).
6. **Fill in the metadata**: the `GameDef` at the bottom of `my_game_game.c` (tagline, controls, `order`, `ranked`), then `GAME.md` (hook, rules, pseudocode, flowchart, concept table) and the "Game-specific rules" in `AGENTS.md`.
7. **Check**: `bash tools/format.sh && bash tools/check_repo.sh && ctest --preset logic`, then a smoke run: `build/headless/bin/arCade --smoke my_game --frames 300`.
8. **Open a PR** titled `feat(my_game): add my_game`.

Rules you cannot skip are listed in `games/AGENTS.md` (and enforced by `tools/check_repo.sh`).
