# Menulis Laporan Akhir dari repository ini

Tugasnya membutuhkan: source code, video demo, dan laporan (satu PDF). Repository ini sudah berisi sebagian besar bahannya.

## Bab I - Pendahuluan
**Deskripsi Program**: tujuan (arcade berisi kumpulan game kecil yang tidak biasa), manfaat, dan konsep kuliah yang dipakai. Pakai tabel di bawah, lalu satu paragraf per game dari `GAME.md`-nya (bagian "Hook" + "Aturan dan skor").
**Peran**: isi dengan nama asli:

| Modul | Pemilik | Yang dikerjakan |
|-------|---------|-----------------|
| Hub, leaderboard, sandbox, sort_it, CI/release | Leader | |
| time_shooter | Member 1 | |
| echo_maze | Member 2 | |
| laser_mirrors | Member 3 | |
| ghost_loop | Member 4 | |
| beat_dodge | Member 5 | |
| beat_catch | Member 5 | |
| absorb_grow + laporan | Member 6 | |
| post_it | Leader | |
| master_juggler | Leader | |
| bw_reflex | Leader | |
| different_color | Leader | |
| spike_dart | Leader | |
| loop_lap | Leader | |
| tower_stacker | Leader | |
| new_star | Leader | |
| gritty_golf (integrasi engine upstream karya zaxx-q; sebutkan sumbernya di Daftar Pustaka) | isi nama | |

## Bab II - Hasil Implementasi
- **Source code**: screenshot fungsi-fungsi kunci (setiap `GAME.md` mencantumkan nama fungsinya, lihat bagian "Fungsi-fungsi penting" dan tabel konsep kuliah) + link repository / rilis (**pastikan link bisa dibuka tanpa login**: repo public, atau tambahkan dosen).
- **Pseudocode**: salin blok "Pseudocode (untuk laporan)" dari setiap `GAME.md`.
- **Flowchart**: setiap `GAME.md` punya flowchart Mermaid di bagian "Flowchart (untuk laporan)". Render-nya: tempel ke <https://mermaid.live> lalu ekspor PNG/SVG, atau `npx @mermaid-js/mermaid-cli -i flow.mmd -o flow.png`. GitHub juga menampilkan Mermaid langsung di halaman.
- **Video demo** (1-6 menit): rekam layar di mesin tempat kamu menguji. Usulan skrip: intro + input nama + menu dengan playground pasir (30 detik) -> sekitar 30 detik per game (tunjukkan hook-nya bekerja) -> leaderboard setelah dua kali main, lalu restart aplikasi untuk menunjukkan skor tetap tersimpan (30 detik). Unggah ke Google Drive, atur "anyone with the link", lalu masukkan linknya ke laporan.
- Screenshot yang berguna untuk laporan: `build/headless/bin/arCade --smoke all --frames 400 --out report-shots` menghasilkan satu PNG per game.

## Bab III - Penutup
Kesimpulan: apa yang diperlihatkan proyek ini. Saran: backlog (bagian 7 "Roadmap dan pekerjaan yang tersisa" di `docs/DEVELOPMENT.md`, dan daftar "Prioritas dari diskusi tim" di tiap `GAME.md`: level lebih banyak, elemen sandbox lebih banyak, game tambahan).

## Konsep kuliah untuk seluruh proyek (Pertemuan -> di mana)
| Pertemuan | Konsep | Di mana dalam kode |
|-----------|--------|--------------------|
| 2 | Input, output | input keyboard/mouse `arcade_poll`; output layar (raylib); output console `--list`, `--help`; file I/O |
| 3 | Tipe data, pseudocode | `float`, `int`, `bool`, `uint8_t`, `char[]`; pseudocode di `GAME.md` |
| 4 | Operator | aritmetika dan logika di mana-mana; bitwise di `laser_mirrors_logic.c` (`1u << d`, `d ^ 1`) |
| 5 | Percabangan | `if`/`switch`: `laser_mirrors_init`, state machine layar di `app/main.c` |
| 6 | Perulangan | frame loop, loop bersarang pada grid (`sandbox_step`, `advance_ping`) |
| 9 | Array, string | grid (`wall[][]`, `cells[]`), parsing di `bd_song_load`, `lb_find_player` |
| 10 | Pointer | `swap_int(int*, int*)`, setiap parameter `State *s` |
| 11 | Prosedur, fungsi | setiap `*_step`, `*_init`, helper seperti `spawn_enemy` |
| 12 | Rekursif, fungsi matematika | `carve()` (echo_maze), `trace()` (laser_mirrors), `sqrtf`, `sinf`, `cosf` |
| 13 | File handling, struct, enum | `lb_load/lb_save`, file lagu Beat Dodge; `ScoreEntry`, `ElementId`, `LmCell` |
| 14 | Searching, sorting | `lb_sort` (insertion), `lb_find_player` (linear), `lb_binary_search_name`, `sortlib` + `sort_it` |

## Siap menjelaskan
Tugas ini menggantikan ujian akhir, jadi setiap anggota harus bisa menjelaskan game miliknya sendiri: struct state-nya, fungsi `step`, satu test, dan tabel konsep kuliahnya. Minta AI assistant kamu untuk "jelaskan `<id>_logic.c` baris per baris dengan bahasa sederhana" dan "sederhanakan bagian yang berbelit" setelah implementasi selesai. Cek juga aturan mata kuliahmu soal penggunaan AI, dan cantumkan di tempat yang diwajibkan.
