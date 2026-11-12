# Menambah game (10 menit)

1. **Scaffold**: `cmake -DID=my_game -DTITLE="My Game" -DOWNER="Nama Kamu" -P tools/new_game.cmake`
   (`ID`: huruf kecil, angka, dan underscore; menjadi nama folder, prefix file, dan key leaderboard).
2. **Build**: `cmake --preset logic && cmake --build --preset logic && ctest --preset logic -R "^my_game\."` - template-nya sudah berupa game "kumpulkan pellet" yang jalan dan punya test. Tidak ada langkah registrasi: CMake menemukan `games/my_game/` sendiri.
3. **Mainkan**: `cmake --preset dev && cmake --build --preset dev && build/dev/bin/arCade` -> game muncul di menu. (`--list` menampilkan semua game yang terdaftar.)
4. **Ganti mekaniknya** di `my_game_logic.c` (C murni, deterministik, tanpa raylib), dan tulis test-nya lebih dulu atau bersamaan. Pertahankan 4 fungsi: `init`, `step`, `is_over`, `score`.
5. **Gambar** di `my_game_game.c` dengan helper `gfx_*` dan palet `ARC_COL_*`. Pertahankan pemanggilan loop (`arcade_loop_begin`, `arcade_poll`, `arcade_frame_end`).
6. **Isi metadata**: `GameDef` di bagian bawah `my_game_game.c` (tagline, controls, `order`, `ranked`), lalu `GAME.md` dan bagian "Game-specific rules" di `AGENTS.md`.
7. **Cek**: `bash tools/format.sh && bash tools/check_repo.sh && ctest --preset logic`, lalu smoke run: `build/headless/bin/arCade --smoke my_game --frames 300`.
8. **Buka PR** dengan judul `feat(my_game): add my_game`.

Aturan yang tidak boleh dilewati ada di `games/AGENTS.md` (dan dicek otomatis oleh `tools/check_repo.sh`).

## Mengisi `GAME.md`

`GAME.md` hasil scaffold sudah berisi kerangka semua bagian, dengan `TODO(<owner>)` sebagai penanda yang harus diisi. Bagian-bagiannya, berurutan:

| Bagian | Isinya |
|--------|--------|
| Hook | 1 sampai 2 kalimat: apa yang membuat game ini unik |
| Penjelasan sederhana | Penjelasan untuk orang yang belum pernah melihat kodenya |
| Kontrol, Aturan dan skor | Tombol, aturan main, dan cara menghitung skor |
| Cara kerja per frame | Apa yang terjadi di setiap frame, berurutan |
| Peta file | File mana berisi apa, dan kapan harus diedit |
| Data penting (struct), Fungsi-fungsi penting | Tabel field `State` dan fungsi-fungsi utamanya |
| Angka yang bisa diutak-atik | Konstanta yang aman diubah untuk tuning |
| Cara menjalankan dan menguji | Perintah build, test, dan smoke run, plus daftar test dan fungsinya |
| Jebakan dan hal yang perlu diingat | Hal yang gampang salah atau tidak terduga |
| Langkah implementasi (Implementation steps) | Checklist prioritas tim dan langkah dari design awal |
| Pseudocode, Flowchart, Konsep kuliah | Bahan untuk laporan (lihat `docs/REPORT_GUIDE.md`) |

Contoh yang paling lengkap ada di `games/time_shooter/GAME.md`; tiru bentuknya. Komentar di kode sering menyebut "Implementation steps" atau nomor langkahnya, jadi judul bagian `Langkah implementasi (Implementation steps)` dan nomor langkah yang sudah ada jangan diubah.
