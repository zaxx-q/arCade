# Arsitektur

## Layer

```
app/ (hub: menu, leaderboard, smoke runner)  ->  games/<id>/<id>_game.c (front-end raylib)  ->  core/gfx (helper raylib)  ->  raylib
                                                          |
                                                          v
                                  games/<id>/<id>_logic.c (C murni)  ->  core/src (C murni: rng, input, leaderboard, sandbox, sortlib, fx)
```

Panahnya tidak pernah berbalik: **kode murni tidak pernah meng-include raylib**. CMake memaksakannya (library `arcade_core` dan `game_<id>_logic` tidak di-link ke raylib) dan `tools/check_repo.sh` mengeceknya sekali lagi.

## Kontrak game (`core/include/arcade/game.h`)

Setiap game mengekspor satu `const GameDef game_<id>_def` yang berisi `play(const GameContext*)`. Hub memanggil `play`, yang menjalankan seluruh game loop dan mengembalikan `GameResult {score, finished}`. Game `ranked` dengan `finished == true` ditulis ke `scores.txt`.

Di dalam `play`, setiap game mengikuti loop yang sama:

```c
arcade_loop_begin(ctx);
while (arcade_loop_running()) {          // false saat ESC, window ditutup, atau max_frames (smoke)
    ArcadeInput in; arcade_poll(&in);    // keyboard/mouse ATAU input "monkey" berskrip saat smoke run
    xxx_step(&s, &in, ARC_DT);           // logic murni, step tetap 1/60 detik
    BeginDrawing(); ...draw...; arcade_frame_end();   // EndDrawing + snapshot PNG saat smoke run
    if (xxx_is_over(&s)) break;
}
```

## Registry game otomatis

`games/CMakeLists.txt` melakukan glob pada `games/<id>/CMakeLists.txt`; masing-masing memanggil `arcade_add_game(...)` (`cmake/ArcadeGame.cmake`), yang membangun `game_<id>_logic` (murni), `game_<id>` (raylib), dan satu executable test per file test, lalu mencatat id-nya. `cmake/ArcadeRegistry.cmake` membuat `build/<preset>/generated/game_registry.c` (sebuah array berisi `&game_<id>_def`). Hub me-link semua game dan mengurutkan array itu berdasarkan `GameDef.order`, lalu id. Tidak ada yang perlu mengedit file bersama untuk menambah game.

## Piramida testing

1. **Unit test** (`ctest`, kode murni, OS apa pun, tanpa raylib): aturan, determinisme, "setiap seed bisa diselesaikan", parser file, dan satu monkey test per game (ribuan input acak, invariant harus tetap terjaga).
2. **Sanitizer**: test yang sama dijalankan dengan ASan + UBSan (clang, Linux).
3. **Smoke run** (`arCade --smoke all`): tiap game dijalankan N frame dengan input berskrip dan mengekspor PNG; run dinyatakan gagal kalau ada game yang crash atau menggambar satu warna polos. Ada dua backend: CPU renderer milik raylib 6 (preset `headless`, tanpa GPU/window) dan window sungguhan di bawah `xvfb`.
4. **Cek kompilasi**: gcc (Linux), MinGW gcc (Windows), MSVC (tidak memblokir), `-Werror`.
   Yang tidak bisa dicek CI: apakah game-nya seru, atau output piksel yang persis.

## Catatan headless rendering

`PLATFORM=Memory` (raylib >= 6.0) merender ke framebuffer di memori. Hasil bacanya berformat BGRA dan terbalik secara vertikal (bottom-up), jadi `core/gfx/gfx.c:snapshot()` menormalkannya (hanya ketika `ARCADE_HEADLESS_BUILD` didefinisikan). Audio dinonaktifkan pada smoke run.

## File data

- `scores.txt` (di sebelah exe, atau `--data-dir`): satu baris `game|player|score|unix_time`; baris yang rusak dilewati.
- `gritty_save_<nama>.dat` (di folder data yang sama): progres Gritty Golf per pemain, ditulis oleh engine yang disalin ke `games/gritty_golf/gg/` (checksum; file rusak diganti nama menjadi `.bad`).
- `assets/beatmaps/*.txt`: file lagu (beat map) untuk Beat Dodge dan Beat Catch (formatnya didokumentasikan di `beat_dodge_logic.h`, lihat juga `docs/SONGS.md`).

## Versioning

`ARCADE_VERSION` (variabel cache CMake) ditanam ke `arcade/version.h`; CI meneruskan `X.Y.Z` hasil perhitungan; build lokal memakai `git describe` atau `0.0.0-dev`. Ditampilkan di menu dan oleh `--version`.

## Keputusan utama

| Keputusan                                                | Alasan                                                                                                 |
| -------------------------------------------------------- | ------------------------------------------------------------------------------------------------------ |
| C99 murni, warning ketat (`-Werror`)                     | Syarat akademik (C prosedural); perilaku yang sama di GCC, Clang, dan MSVC.                            |
| raylib 6.2 yang di-pin lewat CMake `FetchContent`        | Setup otomatis dengan satu perintah di semua platform tanpa memasang library secara manual.            |
| Logic / front-end dipisah                                | Memungkinkan CI headless yang cepat, unit test tanpa GPU/display, dan backend rendering yang bisa diganti. |
| Simulasi deterministik (RNG PCG32, step tetap `1/60s`)   | Test yang bisa diprediksi, sistem replay (misalnya Ghost Loop), dan smoke run yang bisa direproduksi.  |
| Penemuan game otomatis (`games/<id>/`)                   | Anggota tim bekerja di folder masing-masing tanpa konflik merge di registry pusat.                     |
| Git tag + Conventional Commits                           | Semantic versioning yang deterministik langsung dari riwayat commit, tanpa commit bump yang tidak perlu. |
| Audio disintesis lewat kode                              | Tidak ada masalah lisensi audio, dan tidak ada crash karena file aset yang hilang.                     |
