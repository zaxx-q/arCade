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

Setiap game mengekspor satu `const GameDef game_<id>_def` yang berisi `play(const GameContext*)`. Hub memanggil `play`, yang menjalankan seluruh game loop dan mengembalikan `GameResult {score, finished}`. Game `ranked` dengan `finished == true` ditulis ke `scores.txt`. Game yang punya beberapa mode atau level boleh mengisi `GameContext.board` (opsional, bisa `NULL`) dengan nama papan skor run itu, misalnya `Dodge / Bopeebo / normal`; hub lalu menyimpan dan meranking skornya hanya di papan itu (kunci `<id>:<papan>`, lihat "papan" di `leaderboard.h`). Game yang tidak mengisinya tetap satu papan per game, seperti sebelumnya.

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

`games/CMakeLists.txt` dan `deferred/CMakeLists.txt` sama-sama memanggil `arcade_discover_games()` (`cmake/ArcadeGame.cmake`), yang melakukan glob pada `<folder>/<id>/CMakeLists.txt` (folder berawalan `_` atau `.` dilewati). Masing-masing memanggil `arcade_add_game(...)`, yang membangun `game_<id>_logic` (murni), `game_<id>` (raylib), dan satu executable test per file test, lalu mencatat id-nya ke daftar `ARCADE_LISTED_IDS` (game di `games/`) atau `ARCADE_DEFERRED_IDS` (game di `deferred/`). `cmake/ArcadeRegistry.cmake` membuat `build/<preset>/generated/game_registry.c` dengan dua array: `g_arcade_games` (menu hub) dan `g_arcade_deferred` (arsip). `GameDef` tidak berubah, jadi sebuah game tidak tahu ia ada di folder yang mana. Hub me-link semua game dan mengurutkan tiap array berdasarkan `GameDef.order`, lalu id. Tidak ada yang perlu mengedit file bersama untuk menambah game atau memindahkannya antar-folder.

Menu hub hanya menampilkan `g_arcade_games`. Isi `g_arcade_deferred` baru ikut tampil setelah arsip dibuka: lewat "vault" di latar belakang hub (`core/vault`, ditaruh di atas engine sandbox) atau lewat `arCade --all`. Test, smoke test (`--smoke all`), dan CI tetap mencakup kedua folder. Detailnya ada di [DEFERRED.md](./DEFERRED.md).

## Piramida testing

1. **Unit test** (`ctest`, kode murni, OS apa pun, tanpa raylib): aturan, determinisme, "setiap seed bisa diselesaikan", parser file, logika kirim-ulang leaderboard online dengan jaringan palsu (`tests/test_lbnet.c`), dan satu monkey test per game (ribuan input acak, invariant harus tetap terjaga).
2. **Sanitizer**: test yang sama dijalankan dengan ASan + UBSan (clang, Linux).
3. **Smoke run** (`arCade --smoke all`): tiap game dijalankan N frame dengan input berskrip dan mengekspor PNG; run dinyatakan gagal kalau ada game yang crash atau menggambar satu warna polos. Ada dua backend: CPU renderer milik raylib 6 (preset `headless`, tanpa GPU/window) dan window sungguhan di bawah `xvfb`.
4. **Cek kompilasi**: gcc (Linux), MinGW gcc (Windows), MSVC (tidak memblokir), `-Werror`.
   Yang tidak bisa dicek CI: apakah game-nya seru, atau output piksel yang persis.

## Catatan headless rendering

`PLATFORM=Memory` (raylib >= 6.0) merender ke framebuffer di memori. Hasil bacanya berformat BGRA dan terbalik secara vertikal (bottom-up), jadi `core/gfx/gfx.c:snapshot()` menormalkannya (hanya ketika `ARCADE_HEADLESS_BUILD` didefinisikan). Audio dinonaktifkan pada smoke run.

## File data

- `scores.txt` (di sebelah exe, atau `--data-dir`): satu baris `game|player|score|unix_time`; baris yang rusak dilewati. Kolom `game` adalah id game polos (`echo_maze`) atau kunci papan `<id>:<papan>` (`beat_dodge:Catch / Bopeebo / hard`); formatnya sama, kolomnya hanya boleh sampai 63 karakter (`LB_GAME_LEN`). Setiap percobaan yang selesai adalah satu baris (file ini juga riwayat), tetapi peringkat memakai satu baris terbaik per pemain per papan (`lb_top_best`, `lb_rank_of`). Satu papan menyimpan paling banyak `LB_MAX_PER_BOARD` (50) baris dan seluruh file paling banyak `LB_MAX_ENTRIES` (2000). Kalau penuh, yang dibuang lebih dulu adalah percobaan lama pemain yang sudah ia lampaui sendiri, baru skor terendah; kalau seluruh file yang penuh, barisnya diambil dari papan yang paling penuh, jadi game dengan skala skor kecil tidak tersingkir oleh game dengan skala besar. Format file tidak berubah.
- `online.cfg`, `outbox.txt`, `online_cache.txt`, `identity.txt` (leaderboard online): protokol, antrean, dan jadwal kirim ulang ada di `core/src/lbnet.c` (C murni, diuji dengan jaringan palsu), pilihan pemain di `app/online.c`, dan satu-satunya kode yang menyentuh jaringan di `app/net_curl.c` (libcurl). `online.cfg` berisi pilihan pemain (`enabled|1` atau `enabled|0`). Fitur ini menyala secara bawaan: kalau file belum ada, layar pemberitahuan muncul sekali di awal, sebelum ada yang terkirim, dan jawaban pemain disimpan di sini; isi yang bukan persis `enabled|1` dianggap mati, supaya file yang rusak tidak menyalakan kembali apa yang sudah dimatikan; mematikan juga membuang skor yang masih mengantre; `outbox.txt` berisi skor yang belum terkirim, satu baris per (papan, pemain) dengan format yang sama seperti `scores.txt`; `online_cache.txt` menyimpan tabel terakhir yang diterima dari server (`@papan|waktu|jumlah_pemain` lalu `peringkat|pemain|skor|waktu`); `identity.txt` berisi token install dari server (jangan di-commit). Aturannya: game tidak pernah menunggu server. Skor ditulis ke outbox lebih dulu, dikirim satu per satu dari loop frame hub (`lbn_update` tidak pernah memblokir), dan dicoba lagi dengan jeda 30 dtk yang berlipat ganda sampai 15 menit kalau server tidak terjangkau; pengalihan (3xx) dan jawaban yang bukan format server dianggap "tidak terjangkau", jadi skor tidak pernah dibuang karenanya. Server hanya menyimpan skor terbaik tiap pemain per papan, jadi mengirim skor yang sama dua kali tidak berbahaya. Semua file itu teks biasa dan bisa diubah siapa saja; yang memeriksa data adalah server, bukan klien. Server-nya (Cloudflare Worker + D1) ada di repositori terpisah, `arcade-leaderboard-server`, supaya repositori ini tetap C murni. Tanpa libcurl (opsi CMake `ARCADE_ONLINE`) game tetap dibangun dan tab Online menjelaskan bahwa build ini tidak mendukungnya. Leaderboard dibuka di tab Online kalau skor online menyala (`online_leaderboard_start` di `app/online.c`, pada game yang sedang dipilih di menu), kalau tidak di tab lokal. Protokolnya (teks dengan `|`, tanpa JSON) ditulis di komentar atas `core/include/arcade/lbnet.h`.
- `gritty_save_<nama>.dat` (di folder data yang sama): progres Gritty Golf per pemain, ditulis oleh engine yang disalin ke `games/gritty_golf/gg/` (checksum; file rusak diganti nama menjadi `.bad`).
- `assets/beatmaps/*.txt`: file lagu (beat map) untuk Beat Dodge & Catch (formatnya didokumentasikan di `beat_dodge_logic.h`, lihat juga `docs/SONGS.md`).

## Versioning

`ARCADE_VERSION` (variabel cache CMake) ditanam ke `arcade/version.h`; CI meneruskan `X.Y.Z` hasil perhitungan; build lokal memakai `git describe` atau `0.0.0-dev`. Ditampilkan di menu dan oleh `--version`.

## Keputusan utama

| Keputusan                                                | Alasan                                                                                                 |
| -------------------------------------------------------- | ------------------------------------------------------------------------------------------------------ |
| C99 murni, warning ketat (`-Werror`)                     | Syarat akademik (C prosedural); perilaku yang sama di GCC, Clang, dan MSVC.                            |
| raylib 6.2 yang di-pin lewat CMake `FetchContent`        | Setup otomatis dengan satu perintah di semua platform tanpa memasang library secara manual.            |
| Logic / front-end dipisah                                | Memungkinkan CI headless yang cepat, unit test tanpa GPU/display, dan backend rendering yang bisa diganti. |
| Simulasi deterministik (RNG PCG32, step tetap `1/60s`)   | Test yang bisa diprediksi, sistem replay (misalnya Ghost Loop), dan smoke run yang bisa direproduksi.  |
| Penemuan game otomatis (`games/<id>/`, `deferred/<id>/`) | Anggota tim bekerja di folder masing-masing tanpa konflik merge; memindah folder cukup untuk menampilkan atau menyembunyikan game. |
| Git tag + Conventional Commits                           | Semantic versioning yang deterministik langsung dari riwayat commit, tanpa commit bump yang tidak perlu. |
| Audio disintesis lewat kode                              | Tidak ada masalah lisensi audio, dan tidak ada crash karena file aset yang hilang.                     |
