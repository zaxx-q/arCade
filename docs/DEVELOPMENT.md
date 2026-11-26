# Setup development

Semuanya di-build dengan **CMake + compiler C**. raylib diunduh dan dibangun otomatis saat configure pertama (butuh Internet sekali).

## Struktur repository

```
app/         Executable hub: main loop, menu, layar leaderboard, smoke runner
assets/      Font, suara, tekstur, dan beatmap (lagu); dikompilasi ke dalam executable (cmake/ArcadeAssets.cmake)
cmake/       Konfigurasi build, raylib FetchContent, warning, toolchain
core/        Library C murni (rng, input, leaderboard, sandbox, sortlib, fx, cli) dan core/gfx (helper raylib)
docs/        Dokumentasi, panduan, catatan arsitektur, dan screenshot
games/       Paket game: <id>_logic.[ch], <id>_game.c, test, GAME.md
packaging/   Petunjuk cara bermain (PLAY.txt; tidak ikut dirilis, rilis hanya berisi satu executable)
tests/       Harness (arcade_test.h) dan unit test untuk library core
tools/       Script untuk format, pengecekan aturan, scaffolding game, dan versioning
```

### Bahasa dokumen

Dokumen untuk manusia (`README.md`, `docs/`, `GAME.md`) ditulis dalam bahasa Indonesia, dengan istilah teknis tetap dalam bahasa Inggris. Pengecualiannya:

- Semua `AGENTS.md` sengaja tetap berbahasa Inggris karena dibaca oleh AI agent.
- `docs/raylib_subset.md` dibuat otomatis (`tools/gen_raylib_subset.sh`), jangan diedit manual.
- Komentar di dalam kode (`*.c`, `*.h`) masih berbahasa Inggris.
- `LICENSE` dan `THIRD_PARTY_NOTICES.md` ditulis dalam bahasa Inggris.

## 1. Pasang tool

### Windows (VS Code)

Pakai **MSYS2 UCRT64** (gcc, perilakunya sama dengan Linux). Di shell _MSYS2 UCRT64_:

```bash
pacman -S --needed git mingw-w64-ucrt-x86_64-gcc mingw-w64-ucrt-x86_64-cmake mingw-w64-ucrt-x86_64-ninja mingw-w64-ucrt-x86_64-gdb
```

Lalu tambahkan `C:\msys64\ucrt64\bin` (dan `C:\msys64\usr\bin` untuk `bash`) ke PATH Windows, atau buka VS Code dari shell MSYS2 UCRT64 (`code .`). Pasang Git for Windows kalau belum ada (menyediakan Git Bash, yang menjalankan script `tools/*.sh`).
`gcc`, **`g++`** (project CMake milik raylib mengaktifkan C++; kode kita sendiri C murni), dan CMake >= 3.25 wajib ada.
Opsional, untuk leaderboard online: tidak perlu memasang paket apa pun. Konfigurasikan dengan `-DARCADE_ONLINE=ON -DARCADE_ONLINE_BUNDLE_CURL=ON`: libcurl dibangun dari source dan ditautkan statis, dengan Schannel (TLS milik Windows) sebagai TLS-nya. Itu persis konfigurasi rilis. Sudah diperiksa lewat cross-compile MinGW di Linux (tautan benar, hanya DLL milik Windows yang diimpor), belum dijalankan di Windows asli. Tanpa opsi itu game tetap dibangun, tanpa tab Online.

### Linux (Debian/Ubuntu)

```bash
sudo apt install build-essential cmake ninja-build git \
  libx11-dev libxrandr-dev libxinerama-dev libxcursor-dev libxi-dev libgl1-mesa-dev libxext-dev
```

Opsional, untuk leaderboard online: `sudo apt install libcurl4-openssl-dev` (Fedora: `libcurl-devel`). Tanpa libcurl game tetap dibangun, hanya tanpa tab Online. Untuk membangun seperti rilis (libcurl ditautkan statis, tanpa memakai libcurl sistem) pasang `libssl-dev` (Fedora: `openssl-devel openssl-static`) dan konfigurasikan dengan `-DARCADE_ONLINE=ON -DARCADE_ONLINE_BUNDLE_CURL=ON`.

CMake harus **versi 3.25 atau lebih baru** (untuk preset). cmake dari apt di Ubuntu 22.04 lebih lama: pakai `pip install --user cmake` atau repo apt Kitware.

### Leaderboard online (opsional)

Opsi CMake `ARCADE_ONLINE`: `AUTO` (bawaan: dipakai kalau libcurl ditemukan), `ON` (wajib ada), atau `OFF`. `ARCADE_ONLINE_BUNDLE_CURL=ON` (dipakai rilis; `OFF` bawaannya, dan `ARCADE_ONLINE=OFF` selalu menang) membangun libcurl dari source (`cmake/ArcadeCurl.cmake`, versi dan hash terpasang di sana) dan menautkannya statis, sehingga program tidak butuh libcurl saat berjalan; bila tidak bisa, configure berhenti dengan galat, tidak diam-diam tanpa tab Online. Di Linux program mencari file sertifikat CA saat berjalan (`$SSL_CERT_FILE`, lalu lokasi umum distro; `app/net_curl.c`), jadi `SSL_CERT_FILE=/path/ca.pem` berguna untuk menguji server HTTPS lokal dengan CA buatan sendiri. Alamat server bawaan ada di `ARCADE_ONLINE_URL` (`-DARCADE_ONLINE_URL=https://...`) dan bisa diganti saat dijalankan dengan variabel lingkungan `ARCADE_ONLINE_URL`. Hanya `https://` yang diterima, kecuali `http://localhost` atau `http://127.0.0.1` untuk server di komputer yang sama.

Alat bantu `arcade_online_probe` menjalankan kode klien yang asli terhadap server yang asli dan melaporkan tiap langkah (butuh jaringan, jadi bukan bagian dari `ctest`):

```bash
build/headless/bin/arcade_online_probe https://<server> different_color probe-1 7
```

Baris yang ditambahkannya nyata; hapus setelahnya (lihat README repositori server).

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
| Menambah / mengubah asset | taruh file di `assets/` lalu build ulang (CMake mendeteksi file baru sendiri). Untuk mencoba tanpa build ulang: `arCade --assets-dir assets` |

## 4. Alur kerja Git

- Kerjakan di branch, buka PR ke `main`, lalu **merge commit**. Judul PR harus berupa Conventional Commit (`feat(echo_maze): add pings`): judul itulah yang menjadi judul merge commit dan menentukan versi berikutnya (`docs/RELEASING.md`).
- Penjaga opsional untuk pesan commit di lokal: `git config core.hooksPath .githooks`.
- Line ending adalah LF di semua tempat (`.gitattributes`). Jangan commit `build/` dan `scores.txt`.

### Membuka proteksi force-push sementara (GitHub CLI)

Jika perlu memperbaiki commit di `main` (misalnya membetulkan format pesan commit yang menentukan rilis versi) sementara branch protection memblokir force-push, gunakan GitHub CLI (`gh`) untuk mengizinkan force-push sementara tanpa menghapus aturan proteksi lainnya:

1. **Izinkan force-push di branch `main`:**
   ```bash
   gh api -X PUT repos/{owner}/{repo}/branches/main/protection --input - << 'EOF'
   {
     "required_status_checks": {
       "strict": false,
       "contexts": ["CI OK"]
     },
     "enforce_admins": false,
     "required_pull_request_reviews": {
       "required_approving_review_count": 1
     },
     "restrictions": null,
     "allow_force_pushes": true
   }
   EOF
   ```

2. **Lakukan push secara aman:**
   ```bash
   git push --force-with-lease origin main
   ```

3. **Kunci kembali proteksi force-push:**
   ```bash
   gh api -X PUT repos/{owner}/{repo}/branches/main/protection --input - << 'EOF'
   {
     "required_status_checks": {
       "strict": false,
       "contexts": ["CI OK"]
     },
     "enforce_admins": false,
     "required_pull_request_reviews": {
       "required_approving_review_count": 1
     },
     "restrictions": null,
     "allow_force_pushes": false
   }
   EOF
   ```

## 5. Troubleshooting

| Gejala                                             | Solusi                                                                                   |
| -------------------------------------------------- | ---------------------------------------------------------------------------------------- |
| `CMake Presets ... version 6 not supported`        | CMake terlalu lama: pasang 3.25+                                                         |
| `The C++ compiler ... is not able to compile`      | pasang `g++` (MSYS2: paket gcc sudah termasuk; Linux: `build-essential`)                 |
| `X11/Xlib.h: No such file`                         | pasang paket Linux di atas                                                               |
| Configure pertama menggantung/gagal saat offline   | clone raylib (tag dari `ARCADE_RAYLIB_TAG`) dan berikan `-DFETCHCONTENT_SOURCE_DIR_RAYLIB=` |
| Window tidak terbuka di server / WSL tanpa GUI     | pakai preset `headless` dan `--smoke`                                                    |
| Peringatan ALSA / audio di console                 | tidak berbahaya: game berjalan tanpa suara kalau tidak ada perangkat suara               |
| `Protected branch update failed: ... force-push`   | Buka proteksi sementara lewat `gh api` (lihat panduan di bagian *Alur kerja Git*)        |

## 6. Verifikasi dan matriks test

Jalankan pengecekan ini untuk memverifikasi setiap layer proyek di lokal:

| Pengecekan                   | Perintah                                                             | Hasil yang diharapkan                          |
| ---------------------------- | -------------------------------------------------------------------- | ---------------------------------------------- |
| Unit test logic murni        | `ctest --preset logic`                                               | semua test lolos (saat ini 31 test)            |
| Aturan kontrak & portabilitas | `bash tools/check_repo.sh`                                          | `check_repo: OK (18 games)`                     |
| Cek format                   | `bash tools/format.sh --check`                                       | `format: OK`                                   |
| Perhitungan versi            | `bash tools/tests/test_next_version.sh`                              | `21 passed, 0 failed`                          |
| Tool scaffolding game        | `bash tools/tests/test_new_game.sh`                                  | `new_game test: OK`                            |
| Registry game di hub         | `build/dev/bin/arCade --list` dan `--list --all`                     | 5 game di menu; 18 dengan `--all` (13 deferred) |
| Smoke run headless           | `build/headless/bin/arCade --smoke all --frames 300 --out smoke-out` | 18 baris game berakhir `-> OK`, preview hub dan `hub_vault` OK, PNG ter-ekspor |

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
- [x] Port 8 game dari proyek saudara `simple-games` (hanya aturan, tanpa aset): `post_it`, `master_juggler`, `bw_reflex`, `different_color`, `spike_dart`, `loop_lap`, `tower_stacker`, `new_star`, lengkap dengan unit test, front-end, dan `GAME.md`.
- [x] Port `synthwave_city` dari proyek web Synthwave_City: rencana kota dan lagu dicocokkan angka demi angka dengan versi TypeScript, tampilan 3D rlgl tanpa shader, lengkap dengan unit test, front-end, dan `GAME.md`.
- [x] Hub memakai Synthwave City sebagai latar belakang hidup di belakang menu (klik latar belakang untuk bermain, `Esc` kembali ke menu, tombol kecil/`B` menukar ke sandbox dengan vault); game-nya dipindah ke `deferred/` dan dipanggil lewat `synthwave_city_session.h`.

### Fase 2: Polish Game & Mekanik (Sedang Berjalan)

### Pembagian tugas tim

| Game (`id`)   | Folder                 | Pemilik                    | Status                                                                     |
| ------------- | ---------------------- | -------------------------- | -------------------------------------------------------------------------- |
| Time Shooter  | `deferred/time_shooter/`  | Member 1                   | Mekanik inti jalan (slow-motion, wave, shake)                              |
| Echo Maze     | `deferred/echo_maze/`     | Member 2                   | Mekanik inti jalan (generator rekursif, pulse sonar, line of sight)        |
| Laser Mirrors | `deferred/laser_mirrors/` | Member 3                   | Jalan (kampanye 12 level, par per level; solver membuktikan satu solusi per level) |
| Ghost Loop    | `deferred/ghost_loop/`    | Member 4                   | Jalan (kampanye 6 level: dinding, banyak pad dan pintu, relay antar ghost; tanpa celah, dibuktikan flood fill) |
| Beat Dodge & Catch | `games/beat_dodge/` | Member 5                | Jalan (dua mode, Dodge dan Catch, dipilih di layar pilih lagu; lagu dari MIDI, ribbon, close call, combo, health, musik sintesis, 3 kesulitan) |
| Absorb & Grow | `deferred/absorb_grow/`   | Member 6 (+ editor laporan) | Mekanik inti jalan (aturan pertumbuhan, skala respawn predator)           |
| Sort It       | `deferred/sort_it/`       | Leader                     | Jalan (3 ronde, par dan benchmark algoritma)                               |
| Sandbox       | `deferred/sandbox/`       | Leader                     | Jalan (falling-sand 34 elemen dengan suhu, ledakan dari percikan, uranium yang meleleh lalu kritis, dan lava yang membatu; engine-nya jalankan background menu)        |
| Post It       | `deferred/post_it/`       | Leader                     | Jalan (miring dunia lewat mouse, gulusan post-it, virus, jeda)             |
| Master Juggler | `deferred/master_juggler/` | Leader                     | Jalan (trampolin, level 1..32, 3 nyawa, lanjut dari level terakhir)        |
| BW Reflex     | `deferred/bw_reflex/`     | Leader                     | Jalan (balik warna dasar, kecepatan naik tiap cocok)                       |
| A Different Color | `games/different_color/` | Leader                     | Jalan (dua papan warna acak, timer 30 detik)                               |
| Spike Dart    | `deferred/spike_dart/`    | Leader                     | Jalan (lempar paku, orbit, hitbox = segitiga yang digambar, batas 64)      |
| Loop Lap      | `games/loop_lap/`      | Leader                     | Jalan (rem/biasa/gas, musuh bergabung per lap, ukuran & kecepatan acak)    |
| Synthwave City | `games/synthwave_city/` | Leader                    | Jalan (terbang bebas atau tur di kota neon prosedural; port C dari Synthwave_City, MIT) |
| Tower Stacker | `deferred/tower_stacker/` | Leader                     | Jalan (langkah 5 px, potong, scroll, bar dari kiri/kanan acak, zoom akhir, tap untuk lanjut) |
| New Star      | `deferred/new_star/`      | Leader                     | Jalan (layar gelap, bintang baru, batas waktu dari bintang ke-10)          |
| Gritty Golf   | `games/gritty_golf/`   | zaxx-q (upstream), integrasi ke arCade | Jalan (25 level, progres tersimpan; engine disalin ke `gg/`, lihat `gg/VENDORED.md`) |

Setiap pemilik game mengubah placeholder miliknya menjadi game yang utuh:

1. Baca `games/<id>/GAME.md` (atau `deferred/<id>/GAME.md` untuk game yang ditunda; strukturnya sama) -> "Langkah implementasi (Implementation steps)". Bagian "Prioritas dari diskusi tim" di dalamnya berisi daftar pekerjaan yang sudah dibahas.
2. Cari `TODO(<owner>)`: `grep -rn "TODO(" games deferred core app`.
3. Ikuti aturan test-driven: tulis dulu unit test di `games/<id>/tests/`, implementasikan logic murninya di `games/<id>/<id>_logic.c`, baru perbarui tampilannya di `games/<id>/<id>_game.c`.
4. Jaga `GAME.md` (pseudocode, flowchart, tabel konsep kuliah) tetap sinkron dengan implementasinya.

### Fase 3: Peningkatan Hub & Backlog

- [ ] Beatmap tambahan untuk Beat Dodge & Catch (`assets/beatmaps/`).
- [x] Level puzzle tambahan untuk Laser Mirrors (12 level) dan Ghost Loop (6 level). Level baru untuk keduanya masih bisa ditambah lewat tabel level masing-masing.
- [ ] Elemen pasir tambahan (emitter, kabel, tumbuhan lain - lihat "Ide berikutnya" di `docs/SANDBOX.md`).
- [ ] Game tambahan (opsional) dengan `cmake -DID=... -DTITLE=... -DOWNER=... -P tools/new_game.cmake`.
- [ ] Rincian per game (mis. membuat Beat Dodge lebih sulit, memperbaiki level tutorial) ada di bagian "Prioritas dari diskusi tim" di tiap `GAME.md`.

### Fase 4: Laporan Akhir & Presentasi

- [ ] Pakai `docs/REPORT_GUIDE.md` untuk memetakan konsep kuliah ke kode.
- [ ] Ekspor screenshot dengan `build/headless/bin/arCade --smoke all --frames 400 --out report-shots`.
- [ ] Ekspor flowchart dari definisi Mermaid di tiap `GAME.md`.
