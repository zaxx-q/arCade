# Setup development

Semuanya di-build dengan **CMake + compiler C**. raylib diunduh dan dibangun otomatis saat configure pertama (butuh Internet sekali).

## Struktur repository

```
app/         Executable hub: main loop, menu, layar leaderboard, smoke runner
assets/      Beatmap (lagu) dan data game
cmake/       Konfigurasi build, raylib FetchContent, warning, toolchain
core/        Library C murni (rng, input, leaderboard, sandbox, sortlib, fx, cli) dan core/gfx (helper raylib)
docs/        Dokumentasi, panduan, catatan arsitektur, dan screenshot
games/       Paket game: <id>_logic.[ch], <id>_game.c, test, GAME.md
packaging/   Catatan packaging rilis dan petunjuk saat dijalankan
tests/       Harness (arcade_test.h) dan unit test untuk library core
tools/       Script untuk format, pengecekan aturan, scaffolding game, dan versioning
```

### Bahasa dokumen

Dokumen untuk manusia (`README.md`, `docs/`, `GAME.md`) ditulis dalam bahasa Indonesia, dengan istilah teknis tetap dalam bahasa Inggris. Pengecualiannya:

- Semua `AGENTS.md` sengaja tetap berbahasa Inggris karena dibaca oleh AI agent.
- `docs/raylib_subset.md` dibuat otomatis (`tools/gen_raylib_subset.sh`), jangan diedit manual.
- Komentar di dalam kode (`*.c`, `*.h`) masih berbahasa Inggris.
- Teks lisensi di `LICENSE` dan `THIRD_PARTY_NOTICES.md` tetap dalam bahasa aslinya.

## 1. Pasang tool

### Windows (VS Code)

Pakai **MSYS2 UCRT64** (gcc, perilakunya sama dengan Linux). Di shell _MSYS2 UCRT64_:

```bash
pacman -S --needed git mingw-w64-ucrt-x86_64-gcc mingw-w64-ucrt-x86_64-cmake mingw-w64-ucrt-x86_64-ninja mingw-w64-ucrt-x86_64-gdb
```

Lalu tambahkan `C:\msys64\ucrt64\bin` (dan `C:\msys64\usr\bin` untuk `bash`) ke PATH Windows, atau buka VS Code dari shell MSYS2 UCRT64 (`code .`). Pasang Git for Windows kalau belum ada (menyediakan Git Bash, yang menjalankan script `tools/*.sh`).
`gcc`, **`g++`** (project CMake milik raylib mengaktifkan C++; kode kita sendiri C murni), dan CMake >= 3.25 wajib ada.

### Linux (Debian/Ubuntu)

```bash
sudo apt install build-essential cmake ninja-build git \
  libx11-dev libxrandr-dev libxinerama-dev libxcursor-dev libxi-dev libgl1-mesa-dev libxext-dev
```

CMake harus **versi 3.25 atau lebih baru** (untuk preset). cmake dari apt di Ubuntu 22.04 lebih lama: pakai `pip install --user cmake` atau repo apt Kitware.

### Keduanya

```bash
pip install clang-format==18.1.8     # versi persis yang dipakai CI (tools/format.sh)
```

## 2. VS Code

Pasang extension yang direkomendasikan (editor akan menawarkannya; daftarnya di `.vscode/extensions.json`): _CMake Tools_, _C/C++_, _GitHub Actions_, _EditorConfig_, _Markdown Preview Mermaid Support_.

- Command Palette -> **CMake: Select Configure Preset** -> `dev`, lalu **CMake: Build** dan **CMake: Debug** (menjalankan `arCade`).
- **Terminal -> Run Task...** punya `logic: test`, `dev: build`, `format`, `check repo`, `new game`.
- Jangan aktifkan "format on save" dengan extension C/C++: clang-format bawaannya berbeda dengan yang dipakai CI. Jalankan `bash tools/format.sh` saja.

## 3. Perintah sehari-hari

| Tujuan                   | Perintah                                                                                                                           |
| ------------------------ | ---------------------------------------------------------------------------------------------------------------------------------- |
| Loop cepat (tanpa raylib) | `cmake --preset logic && cmake --build --preset logic && ctest --preset logic`                                                    |
| Test satu game           | `ctest --preset logic -R "^echo_maze\."`                                                                                           |
| Main                     | `cmake --preset dev && cmake --build --preset dev` lalu `build/dev/bin/arCade` (`.exe` di Windows)                                 |
| Screenshot headless      | `cmake --preset headless && cmake --build --preset headless && build/headless/bin/arCade --smoke all --frames 300 --out smoke-out` |
| Sanitizer (Linux/macOS)  | `cmake --preset asan && cmake --build --preset asan && ctest --preset asan`                                                        |
| Build Windows dari Linux | lihat `cmake/toolchains/mingw-w64-x86_64.cmake`                                                                                    |
| Offline / raylib lokal   | tambahkan `-DFETCHCONTENT_SOURCE_DIR_RAYLIB=/path/to/raylib` ke perintah configure                                                 |

## 4. Alur kerja Git

- Kerjakan di branch, buka PR ke `main`, lalu **squash merge**. Judul PR harus berupa Conventional Commit (`feat(echo_maze): add pings`): judul itulah yang menentukan versi berikutnya (`docs/RELEASING.md`).
- Penjaga opsional untuk pesan commit di lokal: `git config core.hooksPath .githooks`.
- Line ending adalah LF di semua tempat (`.gitattributes`). Jangan commit `build/` dan `scores.txt`.

## 5. Troubleshooting

| Gejala                                             | Solusi                                                                                   |
| -------------------------------------------------- | ---------------------------------------------------------------------------------------- |
| `CMake Presets ... version 6 not supported`        | CMake terlalu lama: pasang 3.25+                                                         |
| `The C++ compiler ... is not able to compile`      | pasang `g++` (MSYS2: paket gcc sudah termasuk; Linux: `build-essential`)                 |
| `X11/Xlib.h: No such file`                         | pasang paket Linux di atas                                                               |
| Configure pertama menggantung/gagal saat offline   | clone raylib (tag dari `ARCADE_RAYLIB_TAG`) dan berikan `-DFETCHCONTENT_SOURCE_DIR_RAYLIB=` |
| Window tidak terbuka di server / WSL tanpa GUI     | pakai preset `headless` dan `--smoke`                                                    |
| Peringatan ALSA / audio di console                 | tidak berbahaya: game berjalan tanpa suara kalau tidak ada perangkat suara               |

## 6. Verifikasi dan matriks test

Jalankan pengecekan ini untuk memverifikasi setiap layer proyek di lokal:

| Pengecekan                   | Perintah                                                             | Hasil yang diharapkan                          |
| ---------------------------- | -------------------------------------------------------------------- | ---------------------------------------------- |
| Unit test logic murni        | `ctest --preset logic`                                               | semua test lolos (saat ini 17 test)            |
| Aturan kontrak & portabilitas | `bash tools/check_repo.sh`                                          | `check_repo: OK (9 games)`                     |
| Cek format                   | `bash tools/format.sh --check`                                       | `format: OK`                                   |
| Perhitungan versi            | `bash tools/tests/test_next_version.sh`                              | `21 passed, 0 failed`                          |
| Tool scaffolding game        | `bash tools/tests/test_new_game.sh`                                  | `new_game test: OK`                            |
| Registry game di hub         | `build/dev/bin/arCade --list`                                        | 9 game terdaftar                               |
| Smoke run headless           | `build/headless/bin/arCade --smoke all --frames 300 --out smoke-out` | 9 baris berakhir `-> OK`, PNG ter-ekspor       |

### Jalur Continuous Integration (CI)

| Jalur | Pemicu                                                | Pengecekan                                                                                                                                                                                                                                                  |
| ----- | ----------------------------------------------------- | ----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| Fast  | Setiap pull request                                   | Cek clang-format, aturan repo, build Linux GCC dan unit test dengan `-Werror`, build Windows MinGW GCC, format judul PR sebagai conventional commit                                                                                                         |
| Full  | Push ke `main`, PR berlabel `full-ci`, atau run manual | Semua pengecekan jalur Fast ditambah AddressSanitizer dan UBSan dengan Clang, analisis statis cppcheck, script test perhitungan versi dan scaffolding, kompilasi Windows MSVC, dan smoke run headless di Linux dan Windows yang mengekspor artifact screenshot |

### Status verifikasi platform

- Linux (x86_64): Terverifikasi dengan GCC 13, Clang 18 (ASan dan UBSan), software rendering headless, dan X11 OpenGL.
- Windows (x86_64): Terverifikasi dengan MinGW GCC (MSYS2 UCRT64) dan MSVC di runner CI. Eksekusi native di hardware lokal dan output audio fisik masih perlu diuji di mesin Windows yang bersih.
- macOS dan Wayland: Tidak ditargetkan secara resmi.

## 7. Roadmap dan pekerjaan yang tersisa

### Fase 1: Fondasi & Infrastruktur (Selesai)

- [x] Engine logic C murni & fixed timestep yang deterministik (`ARC_DT = 1/60`).
- [x] State machine layar hub dan backdrop menu falling-sand.
- [x] Leaderboard permanen dengan insertion sort dan pencarian.
- [x] Scaffold awal untuk ke-8 game tim, lengkap dengan unit test dan front-end raylib.
- [x] Matriks CI otomatis (Linux gcc/clang, Windows MinGW, Windows MSVC, AddressSanitizer, cppcheck, smoke run headless).
- [x] Pipeline rilis otomatis dengan packaging lintas platform dan artifact screenshot (`v0.1.1`).

### Fase 2: Polish Game & Mekanik (Sedang Berjalan)

### Pembagian tugas tim

| Game (`id`)   | Folder                 | Pemilik                    | Status                                                                     |
| ------------- | ---------------------- | -------------------------- | -------------------------------------------------------------------------- |
| Time Shooter  | `games/time_shooter/`  | Member 1                   | Mekanik inti jalan (slow-motion, wave, shake)                              |
| Echo Maze     | `games/echo_maze/`     | Member 2                   | Mekanik inti jalan (generator rekursif, pulse sonar, line of sight)        |
| Laser Mirrors | `games/laser_mirrors/` | Member 3                   | Mekanik inti jalan (level contoh, pelacakan sinar rekursif)                |
| Ghost Loop    | `games/ghost_loop/`    | Member 4                   | Mekanik inti jalan (record/replay, saklar lantai, logic pintu)             |
| Beat Dodge    | `games/beat_dodge/`    | Member 5                   | Mekanik inti jalan (beat clock, loader beatmap teks, audio click)          |
| Absorb & Grow | `games/absorb_grow/`   | Member 6 (+ editor laporan) | Mekanik inti jalan (aturan pertumbuhan, skala respawn predator)           |
| Sort It       | `games/sort_it/`       | Leader                     | Jalan (3 ronde, par dan benchmark algoritma)                               |
| Sandbox       | `games/sandbox/`       | Leader                     | Jalan (mainan falling-sand; engine-nya menjalankan background menu)        |
| Gritty Golf   | `games/gritty_golf/`   | zaxx-q (upstream), integrasi ke arCade | Jalan (25 level, progres tersimpan; engine disalin ke `gg/`, lihat `gg/VENDORED.md`) |

Setiap pemilik game mengubah placeholder miliknya menjadi game yang utuh:

1. Baca `games/<id>/GAME.md` -> "Langkah implementasi (Implementation steps)". Bagian "Prioritas dari diskusi tim" di dalamnya berisi daftar pekerjaan yang sudah dibahas.
2. Cari `TODO(<owner>)`: `grep -rn "TODO(" games core app`.
3. Ikuti aturan test-driven: tulis dulu unit test di `games/<id>/tests/`, implementasikan logic murninya di `games/<id>/<id>_logic.c`, baru perbarui tampilannya di `games/<id>/<id>_game.c`.
4. Jaga `GAME.md` (pseudocode, flowchart, tabel konsep kuliah) tetap sinkron dengan implementasinya.

### Fase 3: Peningkatan Hub & Backlog

- [ ] Beatmap tambahan untuk Beat Dodge (`assets/beatmaps/`).
- [ ] Level puzzle tambahan untuk Laser Mirrors dan Ghost Loop.
- [ ] Elemen pasir tambahan (api, minyak, lava, asam - lihat `docs/SANDBOX.md`).
- [ ] Game tambahan (opsional) dengan `cmake -DID=... -DTITLE=... -DOWNER=... -P tools/new_game.cmake`.
- [ ] Rincian per game (mis. membuat Beat Dodge lebih sulit, memperbaiki level tutorial) ada di bagian "Prioritas dari diskusi tim" di tiap `GAME.md`.

### Fase 4: Laporan Akhir & Presentasi

- [ ] Pakai `docs/REPORT_GUIDE.md` untuk memetakan konsep kuliah ke kode.
- [ ] Ekspor screenshot dengan `build/headless/bin/arCade --smoke all --frames 400 --out report-shots`.
- [ ] Ekspor flowchart dari definisi Mermaid di tiap `GAME.md`.
