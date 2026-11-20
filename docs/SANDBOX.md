# Mengembangkan sandbox

Engine falling-sand dibagi menjadi beberapa file supaya tiap file tetap mudah dibaca. Engine ini dipakai oleh background hub dan oleh `deferred/sandbox`.

| File | Isinya |
|------|--------|
| `core/include/arcade/sandbox.h` | API publik: `ElementId`, `MoveKind`, `ElementDef`, struct `Sandbox`, fungsi `sandbox_*` |
| `core/src/sandbox.c` | dasar grid (`init`, `get`, `set`, `paint`, `count`) dan loop per tick (`sandbox_step`) |
| `core/src/sandbox_elements.c` | tabel elemen `k_elements[]` dan fungsi palet |
| `core/src/sandbox_move.c` | aturan gerak: bubuk, cairan, gas, api, sinar gamma, rayap |
| `core/src/sandbox_react.c` | kimia: api, lava, asam, tanaman, peledak, uranium, rayap, Game of Life |
| `core/src/sandbox_color.c` | warna satu sel (bayangan, cahaya, kedip api) supaya renderer tetap sederhana |
| `core/src/sandbox_internal.h` | helper privat untuk file `sandbox*.c` (bukan API publik) |

## Cara kerja satu tick
`sandbox_step` menjalankan setiap sel dari baris paling bawah ke atas, arah kiri-kanan bergantian. Untuk tiap sel:

1. `sandbox_react`: kimia. Sel boleh berubah menjadi elemen lain, menyulut tetangga, tumbuh, dan sebagainya.
2. Kalau sel itu tidak berubah, `sandbox_move`: sel boleh bergerak sesuai `MoveKind`-nya.

Setiap sel hanya bertindak satu kali per tick (`moved[]`), jadi sel yang jatuh tidak dihitung dua kali dan reaksi tidak merambat ke seluruh grid dalam satu tick.

Tiap sel punya tiga byte: `cells[]` (id elemen), `aux[]` (ingatan sel: arti byte ini berbeda untuk tiap elemen) dan `moved[]` (bit 0: sudah bergerak di tick ini, bit 1: bergerak di tick lalu, bit 2: uranium yang "tereksitasi"). `aux` ikut berpindah bersama selnya, jadi minyak yang terbakar tetap terbakar saat mengalir.

## Elemen yang tersedia (29 di palet)
Urutan di tabel sama dengan urutan palet. Baris pertama palet adalah tombol angka `0`-`9`: Eraser, Sand, Water, Stone, Soil, Snow, Gunpowder, Seed, Ash, Thermite.

| Kelompok | Elemen | Perilaku singkat |
|----------|--------|------------------|
| Bubuk | Sand, Soil, Snow, Gunpowder, Seed, Ash, Thermite | jatuh beberapa sel per tick, menyebar saat mendarat, longsor di lereng; bubuk ringan (salju, abu, biji) mengapung di atas cairan yang lebih berat. Mesiu menyala seketika. Biji tumbuh di atas Soil. Thermite menyala menjadi lava yang melelehkan material di sekitarnya |
| Cairan | Water, Oil, Acid, Lava, Mercury, Nitrogen | mengalir ke "tempat jatuh" terdekat di sampingnya lalu diam, sehingga genangan rata dengan cepat. Lapisan menurut `density` (minyak di atas air di atas raksa). Minyak terbakar sambil mengalir. Asam melarutkan, lava mengeras jadi batu di air dan mengubah pasir jadi kaca, nitrogen membekukan air |
| Gas | Steam, Methane, Smoke | naik, lalu menyebar. Uap mengembun jadi air, asap memudar, metana meledak menjadi bola api |
| Padat | Wood, Ice, Glass, Wall, Plant, Fuse, TNT | kayu dan tanaman terbakar jadi abu, sumbu terbakar pelan dan menyulut TNT, es meleleh kena panas, kaca tahan asam, Wall tidak bisa dihancurkan oleh apa pun |
| Energi dan hidup | Fire, Uranium, Termite, Life | api merambat ke bahan yang mudah terbakar, uranium memicu reaksi berantai sinar gamma, rayap memakan kayu dan berkembang biak, Life adalah Conway's Game of Life |
| Tersembunyi | Gamma, Molten Uranium | hanya muncul lewat reaksi (`EF_HIDDEN`), tidak ada di palet |

## Menambah elemen (4 langkah)
1. `ElementId` di `sandbox.h`: tambahkan satu nilai sebelum `EL_COUNT` (urutan = urutan palet; `EL_EMPTY` harus tetap 0).
2. Satu baris di `k_elements[]` (`sandbox_elements.c`): nama, warna, `density`, `MoveKind` dan angka perilaku (`speed`, `spread`, `slip`, `grip`, `ignite`, `shade`, `spray`). Kolomnya dijelaskan di komentar di atas tabel.
3. Kalau elemen punya perilaku khusus, tambahkan satu `case` di `sandbox_react()` (`sandbox_react.c`). Kalau butuh ingatan per sel, pakai `aux` dan tulis artinya di komentar di bagian atas file itu. Kalau butuh tampilan khusus (menyala, berkedip), tambahkan `case` di `sandbox_pixel()` (`sandbox_color.c`).
4. Satu test di `tests/test_sandbox.c`.

Palet di layar, tombol `[` `]`, scroll mouse, dan klik pada palet mengenali elemen baru otomatis. Palet punya 30 slot (3 baris x 10); kalau elemen yang terlihat lebih banyak dari itu, ubah `SBX_PAL_COLS`/`SBX_PAL_ROWS` di `deferred/sandbox/sandbox_logic.h`.

Aturan yang perlu diingat:
- Engine tetap murni dan deterministik: hanya pakai `sb->rng` lewat `sandbox_chance()` / `sandbox_rand()`, jangan `rand()` atau waktu.
- Ubah sel lewat `sandbox_put()` (mengganti isi sel) atau `sandbox_swap()` (menukar dua sel); keduanya menandai sel sudah bergerak.
- `sandbox_step` berjalan untuk semua sel dunia di setiap frame (51.200 di game ini, sampai 57.600 di background hub). Di grid penuh sekarang butuh sekitar 1 ms per tick; jaga agar reaksi murah (cek tetangga hanya kalau perlu).
- Fungsi internal memakai awalan `sandbox_` dan file `sandbox*.c`; fungsi game memakai `sbx_`.

## Bagaimana perilaku pasir dan air disetel
Perilaku pasir dan air disetel supaya mirip game sand sejenis (jatuh cepat, percikan saat mendarat, air yang cepat rata), tetapi semua kodenya, tabelnya, dan warnanya ditulis sendiri untuk proyek ini. Bandingkan perilaku dengan tes di `tests/test_sandbox.c`:

- Bubuk jatuh sampai `speed` sel per tick (bukan 1 sel). Gumpalan besar yang mendarat memercik ke samping (jadi gundukan yang lebih landai), sedangkan aliran tipis membentuk kerucut curam.
- Cairan jatuh seperti bubuk, lalu mencari sel terdekat yang punya ruang kosong di bawahnya (sampai `spread` sel) dan bergeser ke sana, paling banyak 4 sel per tick. Sel di dalam hamparan boleh melompati sel air di sebelahnya, jadi seluruh genangan meratakan diri sekaligus. Sisa sel di permukaan berkumpul menjadi satu "undakan", dan genangan yang sudah rata benar-benar diam.
- Gas adalah bayangan cermin cairan: naik dengan peluang `speed`/256, kalau tidak melayang ke samping.

## Ide berikutnya
| Ide | Sketsa |
|-----|--------|
| Emitter | sel tetap yang memunculkan sebuah elemen tiap k tick (array `struct` di `sbx_State`) |
| Kabel, baterai, LED, gerbang logika | butuh `aux` sebagai muatan dan urutan update yang tidak bergantung pada arah penyisiran |
| Tumbuhan lain (lumut, bambu) | cabang baru di `react_plant` dengan aturan tumbuh berbeda |
| Simpan/muat scene | file I/O ke `ctx->data_dir` (pola ada di `core/src/leaderboard.c`) |
