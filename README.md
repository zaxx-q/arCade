# arCade

[![CI Status](https://github.com/zaxx-q/arCade/actions/workflows/release.yml/badge.svg)](https://github.com/zaxx-q/arCade/actions/workflows/release.yml)
[![Release](https://img.shields.io/github/v/release/zaxx-q/arCade?color=blue)](https://github.com/zaxx-q/arCade/releases/latest)
[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](LICENSE)
[![Language: C99](https://img.shields.io/badge/Language-C99-blue.svg)](https://en.wikipedia.org/wiki/C99)
[![Graphics: raylib 6.2](https://img.shields.io/badge/Graphics-raylib%206.2-red.svg)](https://www.raylib.com)

**arCade** is a multi-game retro arcade hub featuring 8 distinct, unusual mini-games, an interactive falling-sand menu background, and persistent local leaderboards. Built entirely in **portable procedural C99** with **raylib 6.2**.

Every game is decoupled into a **deterministic, pure-C logic core** and a modular raylib visual layer, allowing full unit-test coverage, deterministic replayability, and headless continuous integration.

---

## 🎮 The Games

<table>
  <tr>
    <td width="50%" align="center">
      <h3><a href="games/time_shooter/GAME.md">Time Shooter</a></h3>
      <a href="games/time_shooter/GAME.md">
        <img src="docs/screenshots/time_shooter.png" width="380" alt="Time Shooter Screenshot"/>
      </a>
      <p align="left">
        <b>Hook:</b> <i>Time only moves when you move.</i> Stand still to freeze the world, assess bullets in flight, and plan your next shot. Survive incoming enemy waves.<br/>
        <b>Controls:</b> <code>WASD</code> / Arrows move &bull; Mouse aim &bull; Left Click shoot.
      </p>
    </td>
    <td width="50%" align="center">
      <h3><a href="games/echo_maze/GAME.md">Echo Maze</a></h3>
      <a href="games/echo_maze/GAME.md">
        <img src="docs/screenshots/echo_maze.png" width="380" alt="Echo Maze Screenshot"/>
      </a>
      <p align="left">
        <b>Hook:</b> The maze is pitch-black. Send out sonar pings that temporarily reveal walls within line of sight. Navigate to the green exit in minimal pings.<br/>
        <b>Controls:</b> <code>WASD</code> / Arrows move &bull; <code>Space</code> sonar ping.
      </p>
    </td>
  </tr>
  <tr>
    <td width="50%" align="center">
      <h3><a href="games/laser_mirrors/GAME.md">Laser Mirrors</a></h3>
      <a href="games/laser_mirrors/GAME.md">
        <img src="docs/screenshots/laser_mirrors.png" width="380" alt="Laser Mirrors Screenshot"/>
      </a>
      <p align="left">
        <b>Hook:</b> Precision optics puzzle. Rotate mirrors and beam-splitters to bounce and branch laser beams across the grid until every target is lit.<br/>
        <b>Controls:</b> Left Click mirrors to rotate.
      </p>
    </td>
    <td width="50%" align="center">
      <h3><a href="games/ghost_loop/GAME.md">Ghost Loop</a></h3>
      <a href="games/ghost_loop/GAME.md">
        <img src="docs/screenshots/ghost_loop.png" width="380" alt="Ghost Loop Screenshot"/>
      </a>
      <p align="left">
        <b>Hook:</b> Cooperate with your past self. Every 8-second attempt is recorded; past runs replay as ghosts to hold pressure pads and unlock doors.<br/>
        <b>Controls:</b> <code>WASD</code> / Arrows move.
      </p>
    </td>
  </tr>
  <tr>
    <td width="50%" align="center">
      <h3><a href="games/beat_dodge/GAME.md">Beat Dodge</a></h3>
      <a href="games/beat_dodge/GAME.md">
        <img src="docs/screenshots/beat_dodge.png" width="380" alt="Beat Dodge Screenshot"/>
      </a>
      <p align="left">
        <b>Hook:</b> Rhythm bullet-dodger. Obstacles fall across 3 lanes and cross the beat line exactly on tempo according to plain-text song beatmaps.<br/>
        <b>Controls:</b> <code>A</code> / <code>D</code> or <code>Left</code> / <code>Right</code> change lane.
      </p>
    </td>
    <td width="50%" align="center">
      <h3><a href="games/absorb_grow/GAME.md">Absorb & Grow</a></h3>
      <a href="games/absorb_grow/GAME.md">
        <img src="docs/screenshots/absorb_grow.png" width="380" alt="Absorb & Grow Screenshot"/>
      </a>
      <p align="left">
        <b>Hook:</b> Katamari in a petri dish. Swallow smaller green blobs to grow your mass; flee larger red predators that scale relative to your size.<br/>
        <b>Controls:</b> <code>WASD</code> / Arrows move (speed decreases with mass).
      </p>
    </td>
  </tr>
  <tr>
    <td width="50%" align="center">
      <h3><a href="games/sort_it/GAME.md">Sort It</a></h3>
      <a href="games/sort_it/GAME.md">
        <img src="docs/screenshots/sort_it.png" width="380" alt="Sort It Screenshot"/>
      </a>
      <p align="left">
        <b>Hook:</b> <i>You are the sorting algorithm.</i> Swap bars to sort shuffled permutations within a swap budget, measured against Bubble and Selection Sort.<br/>
        <b>Controls:</b> Arrows / Click select &bull; <code>Space</code> / Click swap.
      </p>
    </td>
    <td width="50%" align="center">
      <h3><a href="games/sandbox/GAME.md">Sandbox</a></h3>
      <a href="games/sandbox/GAME.md">
        <img src="docs/screenshots/sandbox.png" width="380" alt="Sandbox Screenshot"/>
      </a>
      <p align="left">
        <b>Hook:</b> Interactive falling-sand cellular automata toy with sand, water, and stone interactions (the same physics engine animates the hub backdrop).<br/>
        <b>Controls:</b> Left Click paint &bull; Right Click erase &bull; <code>0-3</code> select element &bull; <code>+</code>/<code>-</code> brush size &bull; <code>Space</code> pause.
      </p>
    </td>
  </tr>
</table>

---

## ✨ Features

- **8 Mini-Games in One Hub:** Select, play, and switch between games seamlessly with zero shared-file merge conflicts.
- **Falling-Sand Menu Simulation:** The arcade hub background runs a live cellular automata simulation with real-time fluid and powder physics.
- **Persistent Leaderboard:** Saves high scores locally to `scores.txt` with insertion sorting and linear/binary search.
- **Pure-C Architecture:** Every game's rule engine has zero graphics dependencies and can be compiled and unit-tested in isolation in milliseconds.
- **Synthesized Audio:** Sound effects and rhythm clicks are generated procedurally in code without external audio assets.
- **Deterministic Simulation:** Uses an embedded PCG32 random number generator and fixed `1/60s` physics timesteps for reproducible gameplay and ghost replays.
- **Cross-Platform:** Runs natively on Linux (X11/Wayland) and Windows (MSYS2 UCRT64 / MSVC).

---

## 🚀 Quick Start

### Download Prebuilt Releases

Ready-to-run releases for Linux and Windows are available on the [**Releases Page**](https://github.com/zaxx-q/arCade/releases/latest):

- **Windows:** Download and unzip `arCade-vX.Y.Z-windows-x86_64.zip`, then run `arCade.exe`.
- **Linux:** Download and extract `arCade-vX.Y.Z-linux-x86_64.tar.gz`, then execute `./arCade`.

### Build from Source

**Prerequisites:** CMake $\ge$ 3.25, a C99 compiler (GCC, Clang, or MSVC), and Ninja. Raylib 6.2 is fetched and compiled automatically via CMake FetchContent.

```bash
# 1. Fast build & run all unit tests (no raylib download needed)
cmake --preset logic && cmake --build --preset logic && ctest --preset logic

# 2. Build the full interactive hub with raylib graphics
cmake --preset dev && cmake --build --preset dev

# 3. Launch arCade!
build/dev/bin/arCade        # On Windows: build\dev\bin\arCade.exe
```

For full environment setup guides (MSYS2 on Windows, packages on Debian/Ubuntu/Fedora), see [docs/DEVELOPMENT.md](docs/DEVELOPMENT.md).

---

## 🕹️ Hub Controls

| Key                        | Action                                |
| -------------------------- | ------------------------------------- |
| `Up` / `Down` or `W` / `S` | Navigate game cards in the menu       |
| `Enter` / `Space`          | Launch selected game                  |
| `Tab`                      | View the global Leaderboard           |
| `Esc`                      | Return to menu / Exit game / Quit hub |
| `F11`                      | Toggle Fullscreen                     |

Command-line utilities:

```bash
arCade --list                # List all registered games and exit
arCade --version             # Print application and raylib versions
arCade --smoke all           # Run automated headless smoke tests for all games
arCade --smoke <id>          # Run smoke test for a specific game
```

---

## 🏛️ Architecture Overview

arCade enforces strict architectural separation between game logic and graphics:

```
app/ (hub menu, leaderboard, smoke runner)
 └── games/<id>/<id>_game.c    (raylib drawing loop & front-end)
      ├── core/gfx/            (window, loop helpers, neon drawing primitives)
      └── games/<id>/<id>_logic.c (pure C99: simulation step, collision, score)
           └── core/src/       (pure C99: RNG, input monkey, sandbox, sortlib, leaderboard)
```

1. **Pure Logic:** `games/<id>/<id>_logic.c` implements game mechanics without including `raylib.h`. It is completely headless, deterministic, and unit-tested in isolation via CTest.
2. **Visual Front-End:** `games/<id>/<id>_game.c` handles input polling and raylib rendering calls.
3. **Automated Discovery:** Drop a new folder into `games/<id>/` with `games/<id>/CMakeLists.txt`, and CMake automatically detects, registers, and surfaces the game on the menu.

For deep-dive diagrams, game contract details, and data formats, see [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md).

---

## 📚 Documentation Index

- **[docs/DEVELOPMENT.md](docs/DEVELOPMENT.md):** Toolchain installation, everyday commands, verification matrix, and project roadmap.
- **[docs/ARCHITECTURE.md](docs/ARCHITECTURE.md):** Subsystem architecture, the GameDef contract, and technical decisions.
- **[docs/ADDING_A_GAME.md](docs/ADDING_A_GAME.md):** Guide and scaffold tool (`tools/new_game.cmake`) for adding new games.
- **[docs/SANDBOX.md](docs/SANDBOX.md):** How the cellular automata sand simulation engine works and how to add new elements.
- **[docs/RELEASING.md](docs/RELEASING.md):** Versioning rules, conventional commits, and automated GitHub Release workflow.
- **[docs/REPORT_GUIDE.md](docs/REPORT_GUIDE.md):** Academic course concept mapping (functions, arrays, structs, pointers, recursion, algorithms, file I/O).

---

## 📄 License

This project is licensed under the [MIT License](LICENSE). Third-party dependencies and notices are detailed in [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md).
