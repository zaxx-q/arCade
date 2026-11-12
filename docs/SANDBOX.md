# Mengembangkan sandbox

Engine falling-sand ada di `core/include/arcade/sandbox.h` + `core/src/sandbox.c`. Engine ini dipakai oleh background hub dan oleh `games/sandbox`.

## Menambah elemen (4 edit)
1. `ElementId` di `sandbox.h`: tambahkan satu nilai sebelum `EL_COUNT`.
2. `k_elements[]` di `sandbox.c`: nama, warna RGB, density, dan `MoveKind`.
3. Kalau perilakunya baru, tambahkan satu `MoveKind` dan satu cabang di `sandbox_step()`.
4. Satu test di `tests/test_sandbox.c` (misalnya "minyak mengapung di atas air"). Palet dan tombol angka (`handle_keys` di `games/sandbox/sandbox_logic.c`) akan otomatis mengenalinya sampai tombol 9.

> **Catatan (ditambahkan saat dokumen ini diterjemahkan, sesuai isi kode):** tombol angka memang mencakup elemen sampai tombol 9, tetapi palet di layar (`draw_palette` di `games/sandbox/sandbox_game.c`) menggambar tiap elemen dengan jarak 170 px mulai dari x = 16, sehingga hanya sekitar 7 elemen yang muat di lebar 1280 px. Kalau elemen yang ditambahkan lebih dari itu, perlu mengubah tata letak palet. Detailnya ada di `games/sandbox/GAME.md`.

## Ide yang sudah siap pakai
| Elemen | Sketsa aturan |
|--------|---------------|
| Oil (minyak) | cair, density 1 (mengapung di atas air), mudah terbakar |
| Fire (api) | berumur pendek: tiap tick bergerak ke atas secara acak, menyulut tetangga berupa minyak/tanaman, mati setelah N tick (tambahkan array umur per sel) |
| Lava | cairan lambat (bergerak hanya tiap tick ke-3); mengubah tetangga berupa air menjadi batu |
| Plant (tanaman) | statis; tumbuh ke sel kosong di atasnya kalau ada air di dekatnya |
| Acid (asam) | cairan yang menghapus satu tetangga batu/pasir dan dirinya sendiri dengan probabilitas tertentu |
| Gas / uap | naik, bukan jatuh (`MOVE_GAS`: bayangan cermin dari cairan), mengembun kembali menjadi air |
| Emitter | sel tetap yang memunculkan sebuah elemen tiap k tick (array `struct` di `sbx_State`) |

Jaga engine tetap murni dan deterministik (pakai `sb->rng`), dan jaga `sandbox_step` tetap cepat: fungsi ini berjalan untuk setiap sel di setiap frame.
