# Development setup

Everything builds with **CMake + a C compiler**. raylib is downloaded and built automatically on the first configure (needs Internet once).

## 1. Install the tools

### Windows (VS Code)

Use **MSYS2 UCRT64** (gcc, same behaviour as Linux). In an _MSYS2 UCRT64_ shell:

```bash
pacman -S --needed git mingw-w64-ucrt-x86_64-gcc mingw-w64-ucrt-x86_64-cmake mingw-w64-ucrt-x86_64-ninja mingw-w64-ucrt-x86_64-gdb
```

Then add `C:\msys64\ucrt64\bin` (and `C:\msys64\usr\bin` for `bash`) to your Windows PATH, or open VS Code from the MSYS2 UCRT64 shell (`code .`). Install Git for Windows if you do not have it (provides Git Bash, which runs the `tools/*.sh` scripts).
`gcc`, **`g++`** (raylib's CMake project enables C++; our code is plain C) and CMake >= 3.25 are required.

### Linux (Debian/Ubuntu)

```bash
sudo apt install build-essential cmake ninja-build git \
  libx11-dev libxrandr-dev libxinerama-dev libxcursor-dev libxi-dev libgl1-mesa-dev libxext-dev
```

CMake must be **3.25 or newer** (presets). Ubuntu 22.04's apt cmake is older: `pip install --user cmake` or use Kitware's apt repo.

### Both

```bash
pip install clang-format==18.1.8     # the exact version CI uses (tools/format.sh)
```

## 2. VS Code

Install the recommended extensions (the editor prompts you; list in `.vscode/extensions.json`): _CMake Tools_, _C/C++_, _GitHub Actions_, _EditorConfig_, _Markdown Preview Mermaid Support_.

- Command Palette -> **CMake: Select Configure Preset** -> `dev`, then **CMake: Build** and **CMake: Debug** (launches `arCade`).
- **Terminal -> Run Task...** has `logic: test`, `dev: build`, `format`, `check repo`, `new game`.
- Do not turn on "format on save" with the C/C++ extension: its bundled clang-format differs from CI's. Run `bash tools/format.sh` instead.

## 3. Everyday commands

| Goal                     | Command                                                                                                                            |
| ------------------------ | ---------------------------------------------------------------------------------------------------------------------------------- |
| Fast loop (no raylib)    | `cmake --preset logic && cmake --build --preset logic && ctest --preset logic`                                                     |
| One game's tests         | `ctest --preset logic -R "^echo_maze\."`                                                                                           |
| Play                     | `cmake --preset dev && cmake --build --preset dev` then `build/dev/bin/arCade` (`.exe` on Windows)                                 |
| Headless screenshots     | `cmake --preset headless && cmake --build --preset headless && build/headless/bin/arCade --smoke all --frames 300 --out smoke-out` |
| Sanitizers (Linux/macOS) | `cmake --preset asan && cmake --build --preset asan && ctest --preset asan`                                                        |
| Windows build from Linux | see `cmake/toolchains/mingw-w64-x86_64.cmake`                                                                                      |
| Offline / local raylib   | add `-DFETCHCONTENT_SOURCE_DIR_RAYLIB=/path/to/raylib` to the configure command                                                    |

## 4. Git workflow

- Work on a branch, open a PR into `main`, **squash merge**. The PR title must be a Conventional Commit (`feat(echo_maze): add pings`): it decides the next version (`docs/RELEASING.md`).
- Optional local guard for commit messages: `git config core.hooksPath .githooks`.
- Line endings are LF everywhere (`.gitattributes`). Do not commit `build/`, `scores.txt`.

## 5. Troubleshooting

| Symptom                                            | Fix                                                                                      |
| -------------------------------------------------- | ---------------------------------------------------------------------------------------- |
| `CMake Presets ... version 6 not supported`        | CMake too old: install 3.25+                                                             |
| `The C++ compiler ... is not able to compile`      | install `g++` (MSYS2: the gcc package includes it; Linux: `build-essential`)             |
| `X11/Xlib.h: No such file`                         | install the Linux packages above                                                         |
| First configure hangs/fails offline                | clone raylib (tag from `ARCADE_RAYLIB_TAG`) and pass `-DFETCHCONTENT_SOURCE_DIR_RAYLIB=` |
| Window does not open on a server / WSL without GUI | use the `headless` preset and `--smoke`                                                  |
| ALSA / audio warnings in the console               | harmless: games run silently without a sound device                                      |

## 6. Verification and test matrix

Run these checks to verify every layer of the project locally:

| Check                        | Command                                                              | Expected                              |
| ---------------------------- | -------------------------------------------------------------------- | ------------------------------------- |
| Pure logic unit tests        | `ctest --preset logic`                                               | 12 tests passed                       |
| Contract & portability rules | `bash tools/check_repo.sh`                                           | `check_repo: OK (8 games)`            |
| Formatting check             | `bash tools/format.sh --check`                                       | `format: OK`                          |
| Version calculation          | `bash tools/tests/test_next_version.sh`                              | `21 passed, 0 failed`                 |
| Game scaffolding tool        | `bash tools/tests/test_new_game.sh`                                  | `new_game test: OK`                   |
| Hub game registry            | `build/dev/bin/arCade --list`                                        | 8 games registered                    |
| Headless smoke run           | `build/headless/bin/arCade --smoke all --frames 300 --out smoke-out` | 8 lines ending `-> OK`, PNGs exported |

## 7. Roadmap and remaining work

### Phase 1: Foundation & Infrastructure (Completed)

- [x] Pure-C logic engine & deterministic fixed timestep (`ARC_DT = 1/60`).
- [x] Hub screen state machine and falling-sand menu backdrop.
- [x] Persistent leaderboard with insertion sort and search.
- [x] Initial scaffold for all 8 team games with unit tests and raylib front-ends.
- [x] Automated CI matrix (Linux gcc/clang, Windows MinGW, Windows MSVC, AddressSanitizer, cppcheck, headless smoke runs).
- [x] Automated release pipeline with cross-platform packaging and screenshot artifacts (`v0.1.1`).

### Phase 2: Game Polish & Mechanics (In Progress)

Each game owner turns their placeholder into a full game:

1. Consult `games/<id>/GAME.md` -> "Implementation steps".
2. Search for `TODO(<owner>)`: `grep -rn "TODO(" games core app`.
3. Follow the test-driven rule: write the unit test in `games/<id>/tests/` first, implement the pure logic in `games/<id>/<id>_logic.c`, then update drawing in `games/<id>/<id>_game.c`.
4. Keep `GAME.md` (pseudocode, flowchart, concept table) in sync with the implementation.

### Phase 3: Hub Enhancements & Backlog

- [ ] Additional beatmaps for Beat Dodge (`assets/beatmaps/`).
- [ ] Additional puzzle levels for Laser Mirrors and Ghost Loop.
- [ ] Extended sand elements (fire, oil, lava, acid - see `docs/SANDBOX.md`).
- [ ] Optional extra games using `cmake -DID=... -DTITLE=... -DOWNER=... -P tools/new_game.cmake`.

### Phase 4: Final Report & Presentation

- [ ] Use `docs/REPORT_GUIDE.md` to map course concepts to code.
- [ ] Export screenshots using `build/headless/bin/arCade --smoke all --frames 400 --out report-shots`.
- [ ] Export flowcharts from Mermaid definitions in each `GAME.md`.
