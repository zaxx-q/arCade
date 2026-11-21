# Mengembangkan sandbox

Engine falling-sand dibagi menjadi beberapa file supaya tiap file tetap mudah dibaca. Engine ini dipakai oleh background hub dan oleh `deferred/sandbox`.

| File | Isinya |
|------|--------|
| `core/include/arcade/sandbox.h` | API publik: `ElementId`, `MoveKind`, `ElementDef`, struct `Sandbox`, fungsi `sandbox_*` (termasuk `sandbox_temp` dan `sandbox_blast`) |
| `core/src/sandbox.c` | dasar grid (`init`, `get`, `set`, `paint`, `count`) dan loop per tick (`sandbox_step`) |
| `core/src/sandbox_elements.c` | tabel elemen `k_elements[]` dan fungsi palet |
| `core/src/sandbox_move.c` | aturan gerak: bubuk, cairan, gas, api, sinar gamma, rayap |
| `core/src/sandbox_react.c` | kimia: api, lava, asam, tanaman, TNT, uranium, rayap, Game of Life |
| `core/src/sandbox_heat.c` | suhu: tabel `k_thermal[]`, aliran panas antar sel, perubahan wujud (mendidih, membeku, meleleh), kalor laten, menyala sendiri |
| `core/src/sandbox_blast.c` | ledakan: gelombang kejut yang dikirim lewat banyak sinar lurus |
| `core/src/sandbox_color.c` | warna satu sel (bayangan, cahaya, bara pijar, tampilan termal) supaya renderer tetap sederhana |
| `core/src/sandbox_internal.h` | helper privat untuk file `sandbox*.c` (bukan API publik) |

## Cara kerja satu tick
`sandbox_step` melakukan ini berurutan:

1. Bit `moved[]` digeser (bergerak di tick ini menjadi bergerak di tick lalu) dan `tick++`.
2. `sandbox_blast_step`: gelombang kejut besar yang sedang merambat maju beberapa sel, kilatan putih memudar.
3. Setiap sel dari baris paling bawah ke atas, arah kiri-kanan bergantian. Untuk tiap sel: `sandbox_react` (kimia dan suhu: sel boleh berubah, menyulut tetangga, tumbuh) lalu, kalau sel itu tidak berubah, `sandbox_move` (gerak menurut `MoveKind`).
4. `sandbox_heat_step`: panas mengalir antar sel dan ke udara.
5. Setiap 6 tick, `sandbox_life_step` memajukan Game of Life satu generasi.

Setiap sel hanya bertindak satu kali per tick (`moved[]`), jadi sel yang jatuh tidak dihitung dua kali dan reaksi tidak merambat ke seluruh grid dalam satu tick.

Tiap sel punya beberapa byte: `cells[]` (id elemen), `aux[]` (ingatan sel: arti byte ini berbeda untuk tiap elemen), `moved[]` (bit 0: sudah bergerak di tick ini, bit 1: bergerak di tick lalu, bit 2: uranium yang "tereksitasi", bit 3: sudah terkena gelombang kejut di tick ini) dan `temp[]` (suhu dalam derajat Celsius, `int16_t`). `aux` dan `temp` ikut berpindah bersama selnya, jadi minyak yang terbakar tetap terbakar dan air yang sudah panas tetap panas saat mengalir.

## Elemen yang tersedia (34 di palet)
Urutan di tabel sama dengan urutan palet. Baris pertama palet adalah tombol angka `0`-`9`: Eraser, Sand, Water, Stone, Soil, Snow, Gunpowder, Seed, Ash, Thermite.

| Kelompok | Elemen | Perilaku singkat |
|----------|--------|------------------|
| Bubuk | Sand, Soil, Snow, Gunpowder, Seed, Ash, Thermite, Gravel | jatuh beberapa sel per tick, menyebar saat mendarat, longsor di lereng; bubuk ringan (salju, abu, biji) mengapung di atas cairan yang lebih berat. Mesiu menyala seketika. Biji tumbuh di atas Soil. Thermite menyala menjadi lava super panas. Gravel (kerikil) adalah pecahan batu dan bata akibat ledakan |
| Cairan | Water, Oil, Acid, Lava, Mercury, Nitrogen | mengalir ke "tempat jatuh" terdekat di sampingnya lalu diam, sehingga genangan rata dengan cepat. Lapisan menurut `density` (minyak di atas air di atas raksa). Cairan panas naik dan yang dingin turun (konveksi), jadi panci yang dipanasi dari bawah menghangat sampai ke atas. Asam melarutkan, lava mendingin menjadi batu, nitrogen membekukan air |
| Gas | Steam, Methane, Smoke | naik, lalu menyebar. Uap mengembun jadi air saat dingin, asap memudar, metana meledak kecil |
| Padat | Wood, Ice, Glass, Wall, Steel, Brick, Plant, Fuse, TNT | kayu dan tanaman terbakar jadi abu, sumbu terbakar pelan dan menyulut TNT, es meleleh kena panas, kaca tahan asam, Wall tidak bisa dihancurkan oleh apa pun. Steel (baja) penghantar panas yang sangat baik dan tahan ledakan; Brick (bata) penyekat panas |
| Panas dan dingin | Heater, Cooler | sumber panas (700 C) dan pendingin (-80 C) yang suhunya tetap. Tahan asam |
| Energi dan hidup | Fire, Uranium, Termite, Life | api (800 C) merambat ke bahan yang mudah terbakar, uranium memicu reaksi berantai dan bisa meledak raksasa, rayap memakan kayu dan berkembang biak, Life adalah Conway's Game of Life |
| Tersembunyi | Gamma, Molten Uranium | hanya muncul lewat reaksi (`EF_HIDDEN`), tidak ada di palet |

## Suhu (`sandbox_heat.c`)
Setiap sel punya suhu (-273 sampai 9000 C, udara sekitar 20 C). Sel kosong adalah udara dan selalu 20 C; ia menyerap atau memberi panas lewat setiap sisi yang menyentuhnya, itulah cara sepotong baja panas pelan-pelan dingin. Aturannya, per tick:

- **Aliran panas** antar dua sel bertetangga sebanding dengan selisih suhunya dikali daya hantar pasangan itu. Daya hantar pasangan dihitung seperti dua resistor seri (`2ab/(a+b)`): satu sel penyekat (bata) menahan seluruh pasangan sebagus apa pun tetangganya (baja). Aliran dihitung dua kali per tick (`THERMAL_PASSES`) supaya panas merambat lebih cepat dari 1 sel per tick, dengan pembulatan acak supaya aliran yang kecil tidak hilang.
- **Kapasitas panas** (`cap`) menentukan seberapa lambat sel menghangat: air (cap 8) lambat, baja (cap 1) dan gas cepat. **Kehilangan ke udara** (`loss`) dihitung per sisi yang terbuka.
- **Sumber tetap** (`fixed`): Fire (800), Heater (700), Cooler (-80), dan padat yang sedang terbakar (dijaga minimal 600 C).
- **Perubahan wujud**: air mendidih di 100 C dan membeku di 0 C, es dan salju meleleh di 0 C, uap mengembun di 96 C ke bawah, nitrogen menguap di atas -150 C, lava membeku jadi batu di bawah 700 C, uranium cair kembali padat di bawah 800 C, pasir menjadi kaca di 1100 C, batu dan kerikil menjadi lava di 1300 C, baja di 1500 C, kaca di 1700 C, bata di 1900 C.
- **Kalor laten**: air yang mencapai 100 C tidak langsung menjadi uap. Suhunya ditahan di 100 sementara sel menyerap panas (disimpan di `aux`, satuan 16) dan baru berubah setelah cukup (110 satuan untuk mendidih, 60 untuk membeku dan melelehkan es, 30 untuk salju). Itu sebabnya panci air butuh waktu untuk mendidih dan es butuh waktu untuk meleleh.
- **Menyala sendiri**: bahan mudah terbakar menyala begitu suhunya mencapai suhu nyalanya, tanpa api di dekatnya: Fuse 250, Gunpowder 260, Seed 260, Oil 280, Wood 300, Termite 300, Plant 320, TNT 330, Methane 480, Thermite 700.
- **Baris tidak aktif dilewati** (`heat_rows[]`): baris yang hanya berisi sel bersuhu udara tidak dihitung, jadi dunia yang tenang hampir tidak membebani CPU.

Lava yang dituang di lantai membeku dari permukaannya: kolam kecil menjadi batu dalam sekitar 900 tick, kolam besar membentuk kerak batu yang menebal pelan-pelan.

Contoh yang bisa dicoba di game: panci baja berisi air di atas Heater (adegan awal). Baja langsung membara dan memanaskan air dari bawah, air naik-turun (konveksi), mendidih pelan, uapnya naik dan mengembun lagi di tempat yang dingin. Panci yang sama dari bata hampir tidak mendidihkan air karena bata menyekat. Tekan `T` untuk tampilan termal dan arahkan mouse ke sel mana saja untuk membaca suhunya.

Catatan: `sandbox_set_temp` hanya memberi energi sekali. Sel yang dipaksa ke suhu tertentu yang jauh dari yang dibutuhkan perubahan wujud tidak akan berubah kalau energinya kurang (kalor laten harus dibayar); sumber panas atau dingin yang menempel (Heater, Cooler, Fire, lava) memberi energi terus-menerus.

## Ledakan (`sandbox_blast.c`)
`sandbox_blast(sb, x, y, power)` mengirim gelombang kejut: `8 * power` sinar lurus dari satu titik, satu sinar untuk tiap sel di tepi sebuah persegi, jadi sinar tidak pernah lebih dari satu sel dari tetangganya di ujung. Energi sinar berkurang 1 per langkah di udara dan sebesar `resist` sel yang dilewati (kolom `resist` di `k_thermal`), jadi dinding membentuk bayangan dan gelombang cepat habis di tumpukan pasir. Apa yang terjadi pada sel bergantung pada energi yang sampai:

- **Bubuk, cairan, gas** dilempar keluar: mereka bertukar tempat dengan sel kosong pertama di depan sinar (paling jauh `THROW_RANGE` = 24 sel). Muatan yang terkubur menggali kawah dan material jatuh sebagai bibir di sekelilingnya; jumlah selnya tetap. Kalau energinya sangat besar, cairan menguap dan benda lepas terbakar habis.
- **Padat rapuh** (batu, bata, kaca, kayu, es, tanaman) pecah begitu energi mencapai nilai `tough`-nya: batu dan bata menjadi Gravel yang ikut dilempar. Baja dan Heater butuh energi jauh lebih besar. **Wall tidak pernah pecah dan menghentikan gelombang sepenuhnya.**
- Yang bertahan dipanaskan, bahan yang mudah terbakar menyala, **TNT lain disulut** (rantai ledakan bergulir) dan **uranium dipanaskan** (ledakan di dekat tumpukan uranium bisa memicunya).
- Di udara terbuka bagian gelombang yang paling kuat meninggalkan api: bola api.

Ledakan kecil (`power < 40`) selesai dalam tick yang sama. Ledakan besar (`BIG_BLAST` = 40 ke atas) merambat `SHOCK_SPEED` = 4 sel per tick supaya cincinnya terlihat (hanya satu gelombang besar yang berjalan sekaligus; gelombang kedua menyelesaikan yang pertama seketika), dan yang sangat besar (`FLASH_BLAST` = 60 ke atas) membuat layar putih sesaat (`flash`). Kekuatan maksimum 174 (batas jumlah sinar `SANDBOX_MAX_RAYS`).

| Sumber | Kekuatan |
|--------|----------|
| Metana yang meletus | 5 (jarang, kecil) |
| Satu sel TNT (`TNT_POWER`) | 56 (bola api, kawah, batu pecah, pilar batu 30 sel jauhnya rusak) |
| Gumpalan uranium kritis | `18 + 7 * akar(jumlah sel)`, maksimum 174 |

TNT menunggu 4 tick (`TNT_DELAY`) setelah tersulut. Paling banyak 12 TNT yang meledak per tick (`SANDBOX_BLASTS_PER_TICK`), sisanya menunggu giliran, jadi setumpuk TNT sebesar layar meledak bergulir selama beberapa tick tanpa membuat game tersendat (tanpa batas itu satu tick bisa 160 ms).

## Uranium
Uranium memakai suhu sebagai "panasnya": tidak ada lagi hitungan panas di `aux`.

- Sinar gamma yang mengenai uranium membuatnya membelah (`GAMMA_HIT` = 140/256 per sinar) pada gilirannya sendiri, jadi reaksi merambat satu sel per tick. Satu pembelahan memancarkan 2-3 sinar baru dan memanaskan selnya `FISSION_HEAT` = 110 C; sinar yang tidak membelah memanaskan 30 C. Sinar yang tidak punya ruang langsung masuk ke tetangga padatnya, jadi balok padat meneruskan reaksi dari dalam. Air menyerap sinar. Uranium juga membelah spontan pelan-pelan.
- Pada **900 C** (`URANIUM_CRIT`) gumpalan menjadi kritis (di bawah suhu lava, di atas Heater dan Fire, jadi hanya lava, thermite atau reaksinya sendiri yang bisa membawanya ke sana). Seluruh gumpalan yang bersentuhan (8 arah, dikumpulkan dengan flood fill) menghilang seketika dan meledak dengan kekuatan `18 + 7 * akar(N)`. Gumpalan di bawah 12 sel (`NUKE_MIN`) hanya meleleh jadi Molten Uranium.
- Hasil uji di lab: gumpalan 4 x 4 atau lebih kecil diam selamanya, 5 x 5 meledak sendiri setelah sekitar 560 tick, 6 x 6 setelah sekitar 80 tick, 8 x 8 atau lebih dalam sekitar 40 tick; 14 x 14 meratakan seluruh dunia dengan kilatan putih. Gumpalan 4 x 4 di samping kolam lava besar (20 x 10) meledak setelah sekitar 300 tick, di samping Heater setelah sekitar 600 tick (panas sendiri membantu); lava sedikit terlalu cepat dingin untuk memicunya, dan Fire saja tidak cukup. Ledakan lain (TNT) di dekatnya juga bisa memicunya.
- Sisa jatuhan (hingga 60 sel Molten Uranium) berserakan dan kembali menjadi uranium padat saat dingin atau kena air.

## Menambah elemen (5 langkah)
1. `ElementId` di `sandbox.h`: tambahkan satu nilai sebelum `EL_COUNT` (urutan = urutan palet; `EL_EMPTY` harus tetap 0).
2. Satu baris di `k_elements[]` (`sandbox_elements.c`): nama, warna, `density`, `MoveKind` dan angka perilaku (`speed`, `spread`, `slip`, `grip`, `ignite`, `shade`, `spray`). Kolomnya dijelaskan di komentar di atas tabel.
3. Satu baris di `k_thermal[]` (`sandbox_heat.c`): suhu awal, daya hantar, kapasitas, kehilangan ke udara, suhu menyala sendiri, `resist`/`tough` untuk ledakan, dan perubahan wujud. Baris yang terlupa berisi nol dan tidak berguna; salin baris elemen yang mirip.
4. Kalau elemen punya perilaku khusus, tambahkan satu `case` di `sandbox_react()` (`sandbox_react.c`). Kalau butuh ingatan per sel, pakai `aux` dan tulis artinya di komentar di bagian atas file itu. Kalau butuh tampilan khusus (menyala, berkedip), tambahkan `case` di `sandbox_pixel()` (`sandbox_color.c`).
5. Satu test di `tests/test_sandbox.c`.

Palet di layar, tombol `[` `]`, scroll mouse, dan klik pada palet mengenali elemen baru otomatis. Palet punya 40 slot (4 baris x 10); kalau elemen yang terlihat lebih banyak dari itu, ubah `SBX_PAL_COLS`/`SBX_PAL_ROWS` di `deferred/sandbox/sandbox_logic.h`.

Aturan yang perlu diingat:
- Engine tetap murni dan deterministik: hanya pakai `sb->rng` lewat `sandbox_chance()` / `sandbox_rand()`, jangan `rand()` atau waktu.
- Ubah sel lewat `sandbox_put()` (mengganti isi sel; suhunya dipertahankan, lihat `sandbox_heat_on_put`) atau `sandbox_swap()` (menukar dua sel, termasuk suhunya); keduanya menandai sel sudah bergerak.
- `Sandbox` berukuran sekitar 400 KB: selalu `static`, jangan di stack.
- `sandbox_step` berjalan untuk semua sel dunia di setiap frame (49.600 di game ini, sampai 57.600 di background hub). Adegan biasa sekitar 1-2 ms per tick; adegan penuh elemen campuran sekitar 5 ms. Jaga agar reaksi murah (cek tetangga hanya kalau perlu).
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
| Tekanan dan gas yang mengembang | suhu sudah ada; tekanan bisa dihitung dari jumlah gas per sel tertutup |
| Simpan/muat scene | file I/O ke `ctx->data_dir` (pola ada di `core/src/leaderboard.c`) |
