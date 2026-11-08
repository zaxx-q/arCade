# arCade

[![CI Status](https://github.com/zaxx-q/arCade/actions/workflows/release.yml/badge.svg)](https://github.com/zaxx-q/arCade/actions/workflows/release.yml)
[![Release](https://img.shields.io/github/v/release/zaxx-q/arCade?color=blue)](https://github.com/zaxx-q/arCade/releases/latest)
[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](LICENSE)
[![Language: C99](https://img.shields.io/badge/Language-C99-blue.svg)](https://en.wikipedia.org/wiki/C99)
[![Graphics: raylib 6.2](https://img.shields.io/badge/Graphics-raylib%206.2-red.svg)](https://www.raylib.com)

arCade is an arcade hub of 8 small games written in procedural C99 using raylib 6.2, built for a university coursework project (Dasar Pemrograman). It includes an animated falling-sand menu background and a persistent local leaderboard.

Each game is split into a pure C logic library and a raylib front-end. The logic runs with a fixed timestep and has no graphics dependencies, so the entire suite builds and tests in seconds without opening a window.

---

## Games

<table>
  <tr>
    <td width="50%" align="center">
      <h3><a href="games/time_shooter/GAME.md">Time Shooter</a></h3>
      <a href="games/time_shooter/GAME.md">
        <img src="docs/screenshots/time_shooter.png" width="380" alt="Time Shooter screenshot"/>
      </a>
      <p align="left">
        Time moves only when you move or shoot. Standing still freezes enemies and bullets in place, giving you time to plan each step.<br/>
        <b>Controls:</b> WASD or arrow keys to move, mouse to aim, left click to shoot.
      </p>
    </td>
    <td width="50%" align="center">
      <h3><a href="games/echo_maze/GAME.md">Echo Maze</a></h3>
      <a href="games/echo_maze/GAME.md">
        <img src="docs/screenshots/echo_maze.png" width="380" alt="Echo Maze screenshot"/>
      </a>
      <p align="left">
        The maze is dark. Pressing Space sends a sonar pulse that briefly lights up walls in direct line of sight. Reach the exit using as few pulses as possible.<br/>
        <b>Controls:</b> WASD or arrow keys to move, Space to pulse.
      </p>
    </td>
  </tr>
  <tr>
    <td width="50%" align="center">
      <h3><a href="games/laser_mirrors/GAME.md">Laser Mirrors</a></h3>
      <a href="games/laser_mirrors/GAME.md">
        <img src="docs/screenshots/laser_mirrors.png" width="380" alt="Laser Mirrors screenshot"/>
      </a>
      <p align="left">
        Click mirrors to rotate them and redirect laser beams. Splitter tiles branch the beam so you can hit every target on the grid.<br/>
        <b>Controls:</b> Left click on yellow mirrors to rotate.
      </p>
    </td>
    <td width="50%" align="center">
      <h3><a href="games/ghost_loop/GAME.md">Ghost Loop</a></h3>
      <a href="games/ghost_loop/GAME.md">
        <img src="docs/screenshots/ghost_loop.png" width="380" alt="Ghost Loop screenshot"/>
      </a>
      <p align="left">
        Each round lasts 8 seconds. When a round resets, your past attempts replay as ghosts. Stand on floor switches in early rounds so your ghosts hold gates open for you later.<br/>
        <b>Controls:</b> WASD or arrow keys to move.
      </p>
    </td>
  </tr>
  <tr>
    <td width="50%" align="center">
      <h3><a href="games/beat_dodge/GAME.md">Beat Dodge</a></h3>
      <a href="games/beat_dodge/GAME.md">
        <img src="docs/screenshots/beat_dodge.png" width="380" alt="Beat Dodge screenshot"/>
      </a>
      <p align="left">
        Rhythm game where obstacles drop down three lanes and reach the bottom on the beat. Song maps are stored in plain text files that you can edit or extend.<br/>
        <b>Controls:</b> A and D or left and right arrow keys to switch lanes.
      </p>
    </td>
    <td width="50%" align="center">
      <h3><a href="games/absorb_grow/GAME.md">Absorb & Grow</a></h3>
      <a href="games/absorb_grow/GAME.md">
        <img src="docs/screenshots/absorb_grow.png" width="380" alt="Absorb & Grow screenshot"/>
      </a>
      <p align="left">
        Absorb smaller blobs to gain mass and avoid larger ones. New blobs scale with your current size, and your movement slows down as you grow.<br/>
        <b>Controls:</b> WASD or arrow keys to move.
      </p>
    </td>
  </tr>
  <tr>
    <td width="50%" align="center">
      <h3><a href="games/sort_it/GAME.md">Sort It</a></h3>
      <a href="games/sort_it/GAME.md">
        <img src="docs/screenshots/sort_it.png" width="380" alt="Sort It screenshot"/>
      </a>
      <p align="left">
        Swap bars to sort them before running out of swaps. The screen shows how many moves you took compared to selection sort and bubble sort benchmarks.<br/>
        <b>Controls:</b> Arrow keys to select, Space to swap.
      </p>
    </td>
    <td width="50%" align="center">
      <h3><a href="games/sandbox/GAME.md">Sandbox</a></h3>
      <a href="games/sandbox/GAME.md">
        <img src="docs/screenshots/sandbox.png" width="380" alt="Sandbox screenshot"/>
      </a>
      <p align="left">
        Falling-sand toy with sand, water, and stone interactions. The hub menu uses this exact simulation engine to animate its background.<br/>
        <b>Controls:</b> Left click to draw, right click to erase, 0 to 3 to select material, Space to pause.
      </p>
    </td>
  </tr>
</table>

---

## Highlights

- The hub background runs an active falling-sand simulation.
- Scores save to a plain-text file (scores.txt) using insertion sort and binary search for lookups.
- Sound effects and rhythm clicks are generated in code through audio tones, without needing external wav or ogg files.
- Logic updates run at a fixed 60 Hz rate using a PCG32 random number generator, so runs and replays stay consistent.
- Games register automatically from folders in games/; adding a new one does not require editing existing hub source files.
- Builds and runs on both Linux and Windows.

---

## Quick start

### Prebuilt releases

Packaged builds for Linux and Windows are available on the [releases page](https://github.com/zaxx-q/arCade/releases/latest).

- Windows: unzip `arCade-vX.Y.Z-windows-x86_64.zip` and run `arCade.exe`.
- Linux: extract `arCade-vX.Y.Z-linux-x86_64.tar.gz` and run `./arCade`.

### Building from source

Requirements: CMake 3.25 or newer, a C99 compiler (GCC, Clang, or MSVC), and Ninja. CMake downloads and compiles raylib 6.2 on the first build.

```bash
# 1. Build and run unit tests without graphics
cmake --preset logic && cmake --build --preset logic && ctest --preset logic

# 2. Build the full game hub with raylib
cmake --preset dev && cmake --build --preset dev

# 3. Launch arCade
build/dev/bin/arCade        # Windows: build\dev\bin\arCade.exe
```

For platform setup details (MSYS2 packages on Windows, dev libraries on Linux), see [docs/DEVELOPMENT.md](docs/DEVELOPMENT.md).

---

## Hub controls

| Key                | Action                    |
| ------------------ | ------------------------- |
| Up / Down or W / S | Select a game in the menu |
| Enter or Space     | Start selected game       |
| Tab                | Open the leaderboard      |
| Esc                | Back to menu, or quit     |
| F11                | Toggle fullscreen         |

Command line flags:

```bash
arCade --list                # List all registered games
arCade --version             # Show version information
arCade --smoke all           # Run automated headless checks for every game
arCade --smoke <id>          # Run headless checks for a single game
```

---

## Architecture

The project maintains a strict split between game rules and rendering:

```
app/ (hub menu, leaderboard, smoke runner)
 └── games/<id>/<id>_game.c    (raylib drawing and input polling)
      ├── core/gfx/            (window setup, frame timing, drawing helpers)
      └── games/<id>/<id>_logic.c (pure C99: state, collision, scoring)
           └── core/src/       (RNG, input monkey, sandbox engine, leaderboard)
```

The logic layer in `games/<id>/<id>_logic.c` never includes `raylib.h`. This separation allows headless testing in CI and ensures core rules can be evaluated independently from raylib.

For details on the loop contract and memory layout, see [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md).

---

## Documentation

- [docs/DEVELOPMENT.md](docs/DEVELOPMENT.md): toolchain setup, common build commands, and roadmap.
- [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md): system structure and design decisions.
- [docs/ADDING_A_GAME.md](docs/ADDING_A_GAME.md): how to create a new game using the scaffolding script.
- [docs/SANDBOX.md](docs/SANDBOX.md): cellular automata rules and how to add new materials.
- [docs/RELEASING.md](docs/RELEASING.md): tag conventions and release workflows.
- [docs/REPORT_GUIDE.md](docs/REPORT_GUIDE.md): mapping course requirements to code locations.
- [docs/raylib_subset.md](docs/raylib_subset.md): signatures for the raylib 6.2 API subset used across games.

---

## License

This project is released under the [MIT License](LICENSE). Third-party license notices are in [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md).
