# Writing the final report (Laporan Akhir) from this repository

The assignment needs: source code, demo video, report (one PDF). This repo already contains most of the material.

## Bab I - Pendahuluan
**Deskripsi Program**: purpose (an arcade of 8 small, unusual games), benefits, and the course concepts used. Use the table below, then one paragraph per game from its `GAME.md` ("Hook" + "Rules").
**Peran**: fill in real names:

| Module | Owner | What they did |
|--------|-------|---------------|
| Hub, leaderboard, sandbox, sort_it, CI/release | Leader | |
| time_shooter | Member 1 | |
| echo_maze | Member 2 | |
| laser_mirrors | Member 3 | |
| ghost_loop | Member 4 | |
| beat_dodge | Member 5 | |
| absorb_grow + report | Member 6 | |

## Bab II - Hasil Implementasi
- **Source code**: screenshots of key functions (each `GAME.md` lists the function names) + the repository / release link (**make sure the link opens without login**: public repo or add the lecturer).
- **Pseudocode**: copy the "Pseudocode" block of each `GAME.md`.
- **Flowchart**: each `GAME.md` has a Mermaid flowchart. Render it: paste into <https://mermaid.live> and export PNG/SVG, or `npx @mermaid-js/mermaid-cli -i flow.mmd -o flow.png`. GitHub also shows Mermaid inline.
- **Demo video** (1-6 min): screen-record on the machine you tested on. Suggested script: intro + name entry + menu with the sand playground (30 s) -> about 30 s per game (show the hook working) -> leaderboard after two plays, then restart the app to show scores persist (30 s). Upload to Google Drive, set "anyone with the link", put the link in the report.
- Handy screenshots for the report: `build/headless/bin/arCade --smoke all --frames 400 --out report-shots` produces one PNG per game.

## Bab III - Penutup
Kesimpulan: what the project demonstrates; Saran: the backlog (`README.md` "Remaining work": more levels, more sandbox elements, extra games).

## Course concepts, project-wide (Pertemuan -> where)
| Pertemuan | Concept | Where in the code |
|-----------|---------|-------------------|
| 2 | Input, output | keyboard/mouse input `arcade_poll`; screen output (raylib); console output `--list`, `--help`; file I/O |
| 3 | Tipe data, pseudocode | `float`, `int`, `bool`, `uint8_t`, `char[]`; `GAME.md` pseudocode |
| 4 | Operator | arithmetic and logic everywhere; bitwise in `laser_mirrors_logic.c` (`1u << d`, `d ^ 1`) |
| 5 | Percabangan | `if`/`switch`: `laser_mirrors_init`, `app/main.c` screen state machine |
| 6 | Perulangan | frame loop, nested loops over grids (`sandbox_step`, `advance_ping`) |
| 9 | Array, string | grids (`wall[][]`, `cells[]`), `bd_beatmap_load` parsing, `lb_find_player` |
| 10 | Pointer | `swap_int(int*, int*)`, every `State *s` parameter |
| 11 | Prosedur, fungsi | every `*_step`, `*_init`, helpers like `spawn_enemy` |
| 12 | Rekursif, math function | `carve()` (echo_maze), `trace()` (laser_mirrors), `sqrtf`, `sinf`, `cosf` |
| 13 | File handling, struct, enum | `lb_load/lb_save`, beat maps; `ScoreEntry`, `ElementId`, `LmCell` |
| 14 | Searching, sorting | `lb_sort` (insertion), `lb_find_player` (linear), `lb_binary_search_name`, `sortlib` + `sort_it` |

## Be ready to explain
The assignment replaces the final exam, so each member should be able to explain their own game: the state struct, the `step` function, one test, and the course-concept table. Ask your AI assistant to "explain `<id>_logic.c` line by line in simple terms" and "simplify anything convoluted" after the implementation is done. Also check your course's rules on AI assistance and mention it where required.
