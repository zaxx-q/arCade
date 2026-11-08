# Development setup

Everything builds with **CMake + a C compiler**. raylib is downloaded and built automatically on the first configure (needs Internet once).

## 1. Install the tools

### Windows (VS Code)
Use **MSYS2 UCRT64** (gcc, same behaviour as Linux). In an *MSYS2 UCRT64* shell:
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
Install the recommended extensions (the editor prompts you; list in `.vscode/extensions.json`): *CMake Tools*, *C/C++*, *GitHub Actions*, *EditorConfig*, *Markdown Preview Mermaid Support*.
- Command Palette -> **CMake: Select Configure Preset** -> `dev`, then **CMake: Build** and **CMake: Debug** (launches `arCade`).
- **Terminal -> Run Task...** has `logic: test`, `dev: build`, `format`, `check repo`, `new game`.
- Do not turn on "format on save" with the C/C++ extension: its bundled clang-format differs from CI's. Run `bash tools/format.sh` instead.

## 3. Everyday commands
| Goal | Command |
|------|---------|
| Fast loop (no raylib) | `cmake --preset logic && cmake --build --preset logic && ctest --preset logic` |
| One game's tests | `ctest --preset logic -R "^echo_maze\."` |
| Play | `cmake --preset dev && cmake --build --preset dev` then `build/dev/bin/arCade` (`.exe` on Windows) |
| Headless screenshots | `cmake --preset headless && cmake --build --preset headless && build/headless/bin/arCade --smoke all --frames 300 --out smoke-out` |
| Sanitizers (Linux/macOS) | `cmake --preset asan && cmake --build --preset asan && ctest --preset asan` |
| Windows build from Linux | see `cmake/toolchains/mingw-w64-x86_64.cmake` |
| Offline / local raylib | add `-DFETCHCONTENT_SOURCE_DIR_RAYLIB=/path/to/raylib` to the configure command |

## 4. Git workflow
- Work on a branch, open a PR into `main`, **squash merge**. The PR title must be a Conventional Commit (`feat(echo_maze): add pings`): it decides the next version (`docs/RELEASING.md`).
- Optional local guard for commit messages: `git config core.hooksPath .githooks`.
- Line endings are LF everywhere (`.gitattributes`). Do not commit `build/`, `scores.txt`.

## 5. Troubleshooting
| Symptom | Fix |
|---------|-----|
| `CMake Presets ... version 6 not supported` | CMake too old: install 3.25+ |
| `The C++ compiler ... is not able to compile` | install `g++` (MSYS2: the gcc package includes it; Linux: `build-essential`) |
| `X11/Xlib.h: No such file` | install the Linux packages above |
| First configure hangs/fails offline | clone raylib (tag from `ARCADE_RAYLIB_TAG`) and pass `-DFETCHCONTENT_SOURCE_DIR_RAYLIB=` |
| Window does not open on a server / WSL without GUI | use the `headless` preset and `--smoke` |
| ALSA / audio warnings in the console | harmless: games run silently without a sound device |
