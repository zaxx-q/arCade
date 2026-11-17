# Game yang ditunda (`deferred/`)

Lingkup tugas hanya membutuhkan sebagian kecil dari game yang sudah dibuat. Daripada menghapus sisanya, game tersebut dipindahkan ke folder `deferred/`. Kode, test, dan dokumentasinya tetap utuh; yang berubah hanya satu hal: **menu hub tidak menampilkannya** sampai arsip dibuka.

## Dua folder, satu kontrak

| Folder | Tampil di menu hub? | Dibangun, di-test, di-smoke-test? | Isi saat ini |
| --- | --- | --- | --- |
| `games/<id>/` | Ya | Ya | `beat_dodge`, `different_color`, `gritty_golf`, `loop_lap`, `synthwave_city` |
| `deferred/<id>/` | Tidak (hanya setelah arsip dibuka) | Ya | 13 game lainnya |

Struktur folder, `GameDef`, aturan di `games/AGENTS.md`, dan pengecekan `tools/check_repo.sh` sama persis untuk keduanya. `GameDef` tidak tahu ia berada di folder yang mana; keputusan itu dibuat CMake saat registry dibuat (lihat [ARCHITECTURE.md](./ARCHITECTURE.md), bagian "Registry game otomatis").

## Menyembunyikan atau memulihkan sebuah game

Pindahkan foldernya, lalu konfigurasi ulang CMake:

```sh
git mv games/beat_catch deferred/beat_catch      # sembunyikan dari menu
git mv deferred/echo_maze games/echo_maze        # tampilkan di menu
cmake --preset logic                             # registry dibuat ulang
```

Tidak ada file lain yang perlu diedit. `id` game tidak berubah, jadi skor yang sudah tersimpan di `scores.txt` tetap valid dan format leaderboard tidak berubah. Setelah memindahkan, perbarui jalur di dokumen yang menyebut game tersebut (`grep -rn "games/<id>" .`) dan tabel game di `README.md`.

## Membuka arsip di hub

Ada dua cara.

1. **Vault di latar belakang menu.** Di sisi kanan latar belakang ada bejana batu bergembok bertuliskan "archive". Klik kiri di luar panel menggambar pasir, klik kanan menggambar air. Setelah bejana terisi sekitar 90% oleh pasir atau air, gembok terbuka, lantai bejana berlubang sehingga isinya mengalir keluar, dan 13 game muncul di bawah pembatas "archive" di menu (juga sebagai tab di leaderboard). Pasir atau air yang menyala dari atas layar tidak cukup untuk membukanya sendirian; pemain harus menuangkan sendiri. Status terbuka hanya berlaku selama sesi dan tidak disimpan.
2. **Flag `--all`** (untuk pengembangan dan demo): `arCade --all` membuka arsip sejak awal.

Perintah CLI lain yang terkait:

| Perintah | Hasil |
| --- | --- |
| `arCade --list` | game di menu saja (isi `games/`) |
| `arCade --list --all` | semua game; yang ditunda diberi tag `[deferred]` |
| `arCade --smoke all` | smoke test semua game (termasuk `deferred/`), preview layar hub, dan cek vault |

## Bagaimana vault bekerja

Aturannya ada di `core/include/arcade/vault.h` dan `core/src/vault.c`; kode ini murni (tanpa raylib) dan diuji di `tests/test_vault.c`.

- `vault_build()` menggambar bejana di grid sandbox: dinding kiri dan kanan, lantai, dan bagian dalam yang dikosongkan.
- `vault_progress()` menghitung sel pasir dan air di dalam bejana dibagi kapasitas yang dibutuhkan (`VAULT_OPEN_SHARE`, 90%), dibatasi 0..1. Pasir saja sudah cukup supaya bejana tidak bisa macet.
- `vault_update()` dipanggil tiap frame. Saat progres mencapai 1, ia memanggil `vault_open_now()` (membuang dua sel lantai di tengah) dan mengembalikan `true` hanya pada tick itu.
- `app/backdrop.c` membangun adegannya (dua rak berlubang di bawah aliran pasir dan air, bejana di kanan, baris paling bawah sebagai saluran buang) dan menggambar gembok dan penanda tingkat isi. `app/screens.c` memeriksa `backdrop_vault_is_open()` dan memanggil `app_open_archive()`.
- `app/smoke.c` (`run_vault_check`) memastikan bejana tidak terbuka sendiri dalam 6000 frame dan terbuka kalau dituangi pasir, sehingga perubahan pada adegan yang merusak mekanik ini ketahuan di CI.

## Cakupan CI dan test

- `cmake` membangun dan mendaftarkan test kedua folder, jadi `ctest --preset logic` menjalankan semua test game, termasuk yang ada di `deferred/`.
- `check_repo.sh`, `format.sh`, dan `gen_raylib_subset.sh` memindai `games/` dan `deferred/`.
- Workflow GitHub tidak diubah. Pengecekan jumlah game di CI memakai `arCade --list`, yang hanya menghitung isi `games/`.
