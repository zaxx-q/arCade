# Mengembangkan sandbox

Engine falling-sand dibagi menjadi beberapa file supaya tiap file tetap mudah dibaca. Engine ini dipakai oleh background hub dan oleh `deferred/sandbox`.

| File | Isinya |
|------|--------|
| `core/include/arcade/sandbox.h` | API publik: `ElementId`, `MoveKind`, `ElementDef`, struct `Sandbox`, fungsi `sandbox_*` (termasuk `sandbox_temp` dan `sandbox_burst`) |
| `core/src/sandbox.c` | dasar grid (`init`, `get`, `set`, `paint`, `count`) dan loop per tick (`sandbox_step`) |
| `core/src/sandbox_elements.c` | tabel elemen `k_elements[]` dan fungsi palet |
| `core/src/sandbox_move.c` | aturan gerak: bubuk, cairan, gas, api, partikel terbang (percikan dan sinar gamma), rayap |
| `core/src/sandbox_react.c` | kimia: api, lava, asam, tanaman, TNT, uranium, rayap, Game of Life |
| `core/src/sandbox_heat.c` | suhu: tabel `k_thermal[]`, aliran panas antar sel, perubahan wujud (mendidih, membeku, meleleh), kalor laten, menyala sendiri |
| `core/src/sandbox_burst.c` | ledakan dan radiasi: muatan TNT/mesiu menjadi percikan, uranium kritis memancarkan sinar gamma; keduanya partikel yang terbang lurus |
| `core/src/sandbox_color.c` | warna satu sel (bayangan, cahaya, bara pijar, tampilan termal) supaya renderer tetap sederhana |
| `core/src/sandbox_internal.h` | helper privat untuk file `sandbox*.c` (bukan API publik) |

## Cara kerja satu tick
`sandbox_step` melakukan ini berurutan:

1. Bit `moved[]` digeser (bergerak di tick ini menjadi bergerak di tick lalu) dan `tick++`.
2. Setiap sel dari baris paling bawah ke atas, arah kiri-kanan bergantian. Untuk tiap sel: `sandbox_react` (kimia dan suhu: sel boleh berubah, menyulut tetangga, tumbuh) lalu, kalau sel itu tidak berubah, `sandbox_move` (gerak menurut `MoveKind`).
3. `sandbox_heat_step`: panas mengalir antar sel bertetangga (udara juga sel) dan ke tepi dunia.
4. Setiap 6 tick, `sandbox_life_step` memajukan Game of Life satu generasi.

Setiap sel hanya bertindak satu kali per tick (`moved[]`), jadi sel yang jatuh tidak dihitung dua kali dan reaksi tidak merambat ke seluruh grid dalam satu tick.

Tiap sel punya beberapa byte: `cells[]` (id elemen), `aux[]` (ingatan sel: arti byte ini berbeda untuk tiap elemen), `moved[]` (bit 0: sudah bergerak di tick ini, bit 1: bergerak di tick lalu, bit 3-6: arah terbang percikan dan sinar gamma, bit 7: tanda sementara untuk flood fill muatan) `temp[]` (suhu dalam derajat Celsius utuh, `int16_t`) dan `tfrac[]` (sisa pecahan derajat dalam 1/65536, `uint16_t`). `aux`, `temp` dan `tfrac` ikut berpindah bersama selnya, jadi minyak yang terbakar tetap terbakar, air yang sudah panas tetap panas saat mengalir, dan segumpal lava yang jatuh membawa panasnya.

## Elemen yang tersedia (34 di palet)
Urutan di tabel sama dengan urutan palet. Baris pertama palet adalah tombol angka `0`-`9`: Eraser, Sand, Water, Stone, Soil, Snow, Gunpowder, Seed, Ash, Thermite.

| Kelompok | Elemen | Perilaku singkat |
|----------|--------|------------------|
| Bubuk | Sand, Soil, Snow, Gunpowder, Seed, Ash, Thermite, Gravel | jatuh beberapa sel per tick, menyebar saat mendarat, longsor di lereng; bubuk ringan (salju, abu, biji) mengapung di atas cairan yang lebih berat. Mesiu dan TNT meledak sekaligus menjadi percikan (lihat Ledakan). Biji tumbuh di atas Soil. Thermite menyala menjadi lava super panas. Gravel (kerikil) adalah pecahan batu dan bata akibat ledakan |
| Cairan | Water, Oil, Acid, Lava, Mercury, Nitrogen | mengalir ke "tempat jatuh" terdekat di sampingnya lalu diam, sehingga genangan rata dengan cepat. Lapisan menurut `density` (minyak di atas air di atas raksa). Cairan panas naik dan yang dingin turun (konveksi), jadi panci yang dipanasi dari bawah menghangat sampai ke atas. Asam melarutkan, nitrogen membekukan air. Lava berubah warna menurut suhunya (merah tua, jingga, kuning, hampir putih), makin kental saat dingin, lalu mengeras menjadi Basalt (lihat Lava) |
| Gas | Steam, Methane, Smoke | naik, lalu menyebar. Uap mengembun jadi air saat dingin, asap memudar, metana hanya terbakar menjadi api |
| Padat | Wood, Ice, Glass, Wall, Steel, Brick, Plant, Fuse, TNT | kayu dan tanaman terbakar jadi abu, sumbu terbakar pelan dan menyulut TNT, es meleleh kena panas, kaca tahan asam, Wall tidak bisa dihancurkan oleh apa pun. Steel (baja) penghantar panas yang sangat baik dan tahan ledakan; Brick (bata) penyekat panas |
| Panas dan dingin | Heater, Cooler | sumber panas (700 C) dan pendingin (-80 C) yang suhunya tetap. Tahan asam |
| Energi dan hidup | Fire, Uranium, Termite, Life | api (800 C) merambat ke bahan yang mudah terbakar, uranium memanaskan dirinya sendiri sampai meleleh lalu kritis dan memancarkan sinar gamma, rayap memakan kayu dan berkembang biak, Life adalah Conway's Game of Life |
| Tersembunyi | Gamma, Molten Uranium, Spark, Basalt | hanya muncul lewat reaksi (`EF_HIDDEN`), tidak ada di palet |

## Suhu (`sandbox_heat.c`)
Setiap sel punya suhu (-273 sampai 9000 C, udara sekitar 20 C), **termasuk sel kosong**: udara juga bahan, hanya saja ia nyaris tidak menyimpan panas (kapasitas 2, air 4200). Suhu disimpan sebagai derajat utuh (`temp[]`) ditambah pecahan 1/65536 derajat (`tfrac[]`), sebab lava di udara kehilangan jauh kurang dari satu derajat per tick. Aturannya, per tick:

- **Panas hanya berpindah lewat sentuhan.** Setiap pasangan sel bertetangga bertukar panas satu kali per tick sebesar selisih suhunya dikali sebuah koefisien. Koefisien itu adalah yang **lebih kecil** dari dua batas: (a) daya hantar pasangan itu, dihitung seperti dua resistor seri `ab/(a+b)`, jadi satu sel penyekat (bata) menahan seluruh pasangan sebagus apa pun tetangganya (baja); dan (b) panas yang membuat kedua suhu menjadi sama.
- **Batas (b) membuat udara istimewa.** Kapasitas udara 2 melawan 3000 untuk lava, jadi sel udara hanya bisa mengambil sedikit panas lava: ia langsung ikut sama panas, tetapi lava nyaris tidak kehilangan apa-apa. Lava yang jatuh atau melayang di udara bebas hanya kehilangan sekitar 0,15% dari selisih suhunya per tick: jatuh 150 sel dari 1150 C hanya menurunkannya ke sekitar 970 C, jadi lava tidak pernah membatu di tengah udara. Begitu menyentuh baja, air, atau batu, panasnya terkuras dengan laju yang ditentukan daya hantar dan kapasitas bahan itu: genangan lava di atas pelat baja membatu dalam sekitar 40 tick, di atas pelat bata masih cair setelah ratusan tick.
- **Udara bukan penampung tak berdasar.** Udara panas menyebar (sel udara yang bersentuhan langsung menyamakan suhu) dan hanya meninggalkan dunia lewat tepinya: sel di tepi grid bertukar panas dengan bingkai dunia yang diperlakukan sebagai penghantar baik bersuhu 20 C (`EDGE_CAP`). Benda yang menempel pada tepi dunia (lava ke dinding, lantai batu) cepat dingin. Selain itu dunia adalah irisan dari ruangan yang lebih besar, jadi udara juga kehilangan 1/256 dari kelebihan suhunya per tick ke samping (`AIR_LEAK_SHIFT`); tanpa itu Heater yang menyala lama akan mengisi seluruh dunia dengan udara panas.
- **Cara menghitung.** Bilangan titik tetap (1/65536 derajat). Tiap pertukaran memberi satu sel persis apa yang hilang dari sel lain, jadi panas tidak tercipta atau lenyap dan tidak perlu pembulatan acak; sepasang sel tidak pernah menukar lebih dari yang membuat suhu keduanya sama, jadi tidak ada lonjakan atau osilasi. Empat sapuan (pasangan kiri-kanan genap dan ganjil, pasangan atas-bawah genap dan ganjil) mengerjakan pasangan yang saling lepas, jadi hasilnya tidak bergantung pada urutan sel, dan urutan sapuan bergantian tiap tick supaya panas tidak punya arah favorit. Tabel pecahan tiap pasang elemen dihitung sekali (`build_tables`).
- **Kapasitas dan daya hantar** (`cap`, `cond` di `k_thermal[]`): baja 3800 / 3000 (menyerap dan melepas panas dengan cepat), air 4200 / 300 (menampung banyak panas, menghantar sedang), batu 2400 / 220, bata 1500 / 25 dan kayu 1000 / 12 (penyekat), uranium 2200 / 40 (penghantar buruk, itulah yang menyimpan panas gumpalan di dalamnya), udara 2. Kecepatan rambat per tick kira-kira `cond / cap`: baja menyamakan suhu hampir seketika, batu sekitar 5% dari selisih per tick, kayu setengah persen.
- **Sumber tetap** (`fixed`): Fire (800), Heater (700), Cooler (-80), percikan (1200), dan padat yang sedang terbakar (dijaga minimal 600 C). Kapasitasnya besar, jadi suhunya hampir tidak turun saat memberi panas.
- **Perubahan wujud**: air mendidih di 100 C dan membeku di 0 C, es dan salju meleleh di 0 C, uap mengembun di 96 C ke bawah, nitrogen menguap di atas -150 C, lava mengeras menjadi Basalt di bawah 500 C, Basalt melebur lagi menjadi lava di 1100 C, uranium meleleh di 1000 C dan membeku kembali di bawah 800 C, pasir menjadi kaca di 1100 C, batu dan kerikil menjadi lava di 1300 C, baja di 1500 C, kaca di 1700 C, bata di 1900 C.
- **Kalor laten**: air yang mencapai 100 C tidak langsung menjadi uap. Suhunya ditahan di 100 sementara sel menyerap panas (disimpan di `aux`, satuan dua derajat suhu sel itu sendiri) dan baru berubah setelah cukup (110 satuan untuk mendidih, 60 untuk membeku, 120 untuk melelehkan es, 80 untuk salju, 150 untuk menguapkan nitrogen). Uap mengembun dengan cara yang sama di 96 C (100 satuan): selama mengembun ia menahan suhunya dan melepas panas ke sekitarnya, jadi awan uap bertahan beberapa saat dan menghangatkan udara. Itu sebabnya panci air butuh waktu untuk mendidih dan es butuh waktu untuk meleleh.
- **Menyala sendiri**: bahan mudah terbakar menyala begitu suhunya mencapai suhu nyalanya, tanpa api di dekatnya: Fuse 250, Gunpowder 260, Seed 260, Oil 280, Wood 300, Termite 300, Plant 320, TNT 330, Methane 480, Thermite 700.
- **Baris tidak aktif dilewati** (`heat_rows[]`): baris yang hanya berisi sel bersuhu udara (dalam 1/8 C) tidak dihitung, dan sel yang sudah sedekat itu dengan suhu udara dibulatkan ke 20 C persis, jadi dunia yang tenang hampir tidak membebani CPU dan udara panas yang sudah reda benar-benar menjadi tenang.

Contoh yang bisa dicoba di game: panci baja berisi air di atas Heater (adegan awal). Baja langsung membara dan memanaskan air dari bawah, air mendidih, uapnya naik membawa panas (tekan `T`: kolom udara hangat terlihat di atas panci) dan mengembun lagi di tempat yang dingin. Panci yang sama dari bata hampir tidak mendidihkan air karena bata menyekat. Arahkan mouse ke sel mana saja, juga ke udara, untuk membaca suhunya.

Catatan: `sandbox_set_temp` hanya memberi panas sekali. Sel yang dipaksa ke suhu tertentu yang jauh dari yang dibutuhkan perubahan wujud tidak akan berubah kalau panasnya kurang (kalor laten harus dibayar); sumber panas atau dingin yang menempel (Heater, Cooler, Fire, lava) memberi panas terus-menerus. Benda panas yang melayang di udara bebas hampir tidak mendingin: itu disengaja (panas harus punya jalan keluar: sentuhan dengan bahan lain, atau tepi dunia).

## Ledakan (`sandbox_burst.c`)
Ledakan bukan satu hitungan besar, melainkan ratusan partikel kecil (`EL_SPARK`) yang hidup di grid sebagai sel biasa. Tiap percikan terbang lurus dan memutuskan sendiri apa yang terjadi, jadi hasilnya berupa bintang yang bergerigi: panjang tiap sinar berbeda dan bentuk kawah bergantung pada apa yang menghalangi.

**Muatan.** `sandbox_detonate(x, y)` mengumpulkan semua TNT dan Gunpowder yang bersentuhan dengan sel yang tersulut (flood fill 8 arah) dan mengubah *setiap* sel menjadi satu percikan dengan arah acak, seketika. Muatan sebesar layar tidak meledak bergulir selama beberapa tick; semuanya langsung terbang. Layar tidak pernah memutih atau berkedip karena ledakan. TNT yang tidak bersentuhan dengan muatan itu tidak ikut dikumpulkan, tetapi disulut kalau tersambar percikan.

**Percikan.** Terbang `SPARK_STEPS` = 5 sel per tick (sama cepatnya dengan pasir yang jatuh), sejauh 30 sampai 99 sel (TNT) atau 15 sampai 44 sel (mesiu, muatan yang lebih lemah), lalu padam. Di setiap sel yang dilewati ia meninggalkan api, jadi bola api penuh terbentuk dalam sekitar 12 tick dan padam dalam sekitar 40. Arah adalah salah satu dari 16 (8 arah mata angin dan 8 arah di antaranya yang berganti-ganti antara dua arah tetangga), jadi bintangnya tidak punya celah. Apa yang ada di depan percikan menentukan nasibnya:

| Di depan | Akibat |
|----------|--------|
| Udara, gas, api | terbang terus |
| Wall, Steel, Heater, Cooler | percikan padam |
| Percikan lain | menunggu, atau 80/256 keduanya padam, supaya muatan padat tidak menumpuk |
| TNT atau mesiu | muatan itu meledak juga |
| Apa pun yang lain (pasir, air, batu, kayu...) | sel itu dihapus dan percikan maju, tetapi tiap sel menghabiskan percikan dengan peluang 80/256 (`SPARK_DIE`) |

Sesekali (`SPARK_FORK` = 6/256 per langkah) percikan melahirkan kembaran yang melesat tegak lurus. Percikan yang padam meninggalkan api dengan peluang 60/256. Hasilnya: pasir terbongkar menjadi kawah, tumpukan kayu menyala, dinding bata tergores, dinding Wall dan Steel tetap utuh, dan ledakan di bawah air melubangi air dan menguapkan sebagian kecilnya.

`sandbox_burst(sb, x, y, sparks)` melempar sejumlah percikan dari satu titik tanpa muatan; dipakai alat bantu dan tes.

## Uranium
Uranium tidak lagi meledak karena kena ledakan. Ia memanaskan dirinya sendiri, meleleh, dan baru kemudian menjadi kritis, seperti reaktor yang lepas kendali.

1. **Memanaskan diri.** Sel Uranium atau Molten Uranium membuat panas sebanding dengan banyaknya uranium di sekitarnya: ia menghitung uranium di 24 sel di sekelilingnya (kotak 5 x 5) dan baru membuat panas kalau lebih dari `URANIUM_QUIET` = 10, dengan daya penuh `URANIUM_POWER` = 16000 satuan panas per tick (sekitar 7 C per tick kalau tak ada yang membawa panas pergi) saat dikelilingi penuh. Reaksi berantai butuh uranium yang cukup berdekatan, jadi satu sel, jejak tipis, dan gumpalan kecil tetap sejuk. Gumpalan besar memanas di tengahnya: uranium penghantar buruk (`cond` 40) dan udara hampir tidak menyerap panas, jadi panas tertahan di dalam gumpalan. Ukuran menentukan nasib (percobaan di lantai batu):

| Gumpalan | Meleleh pada tick | Kritis pada tick |
|----------|------------------|------------------|
| 3 x 3 atau 4 x 4 | tidak pernah | tidak pernah |
| 5 x 5 | sekitar 380 | sekitar 630 |
| 6 x 6 | sekitar 220 | sekitar 300 |
| 8 x 8 | sekitar 150 | sekitar 200 |
| 10 x 10 atau lebih | sekitar 140 | sekitar 180 |

2. **Meleleh.** Di **1000 C** Uranium menjadi Molten Uranium (cairan kental yang berkilau: jingga saat baru meleleh lalu memutih menjelang kritis) dan terus memanas. Di bawah 800 C ia membeku lagi. Molten Uranium yang menyentuh air langsung membeku kembali menjadi Uranium dan membuat uap. Air dan nitrogen di sekitar gumpalan membawa panas pergi: gumpalan 6 x 4 di dalam air tidak pernah kritis, gumpalan 10 x 10 tetap kritis (sekitar tick 180).
3. **Kritis.** Sel yang mencapai `URANIUM_CRIT` = 1230 C meledak menjadi **sinar gamma** (`EL_GAMMA`). Tidak ada kilatan layar.
4. **Sinar gamma.** Terbang 3 sel per tick selama 40 sampai 63 tick, tidak berbelok, dan hanya berhenti di Wall atau tepi dunia. Lava, gas, api, dan sinar lain dilewati. Apa pun yang lain di jalurnya terbakar habis dan sisanya tertinggal di belakang sinar: air, es, dan salju menjadi uap; pasir menjadi kaca (180/256); bahan mudah terbakar menjadi api; cairan dan bubuk lain menjadi asap atau hilang; padat menjadi kerikil (1/4) atau hilang. TNT yang terkena meledak. **Uranium atau Molten Uranium yang terkena menjadi kritis juga** dan memancarkan sinar barunya sendiri: itulah reaksi berantai yang menyapu seluruh gumpalan. Sinar yang langsung dari uranium (generasi 0) sesekali (`GAMMA_SCATTER` = 16/256 per langkah) menghamburkan satu sinar anak ke arah lain, jadi gumpalan besar menyapu seluruh daerah, bukan hanya beberapa garis tipis. Gumpalan 144 sel membersihkan sebagian besar adegan 320 x 155.

## Lava
Suhu lava menentukan hampir semuanya (`sandbox_heat.c`, `sandbox_color.c`, `sandbox_move.c`):

- **Warna** mengikuti suhu (`lava_color`): merah tua di sekitar 500 C, merah, jingga, kuning, sampai hampir putih di atas 1500 C. Lava yang baru dituang (1150 C) berwarna jingga terang; bagian yang menyentuh batu, logam, atau air lebih cepat dingin dan lebih gelap.
- **Kekentalan**: kemampuan mengalir (`slip`) dikalikan `(suhu - 450) / 800` (dibatasi 4 sampai 800), jadi lava panas menyebar seperti cairan dan lava yang dingin merayap seperti pasta, lalu berhenti. Itu hanya berlaku untuk menyebar ke samping: lava jatuh sama bebasnya dengan cairan lain (gravitasi tidak peduli kekentalan), jadi tidak menggantung di udara.
- **Dingin pelan di udara, cepat kena sentuhan**: di udara bebas lava hampir tidak kehilangan panas (lihat Suhu); di atas lantai batu yang menyentuh tepi dunia, atau di atas penghantar seperti baja, panasnya terkuras cepat; di atas penyekat seperti bata atau kayu ia tetap panas lama.
- **Mengeras**: di bawah 500 C lava menjadi **Basalt** (batu gelap, tersembunyi dari palet). Basalt melebur lagi menjadi lava kalau dipanaskan sampai 1100 C. Waktunya (percobaan di lantai batu): genangan 6 x 3 mengeras dalam sekitar 100 tick, kolam 24 x 12 dalam sekitar 700 tick, dan gumpalan besar 36 x 30 membentuk kerak lebih dulu dan butuh lebih dari 2000 tick untuk membatu seluruhnya.
- **Air**: lava yang bertemu air langsung menjadi Basalt dan membuat uap; nitrogen juga membatukannya.
- **Api**: lava di atas 700 C sesekali melemparkan nyala ke udara di atasnya (`LAVA_FLAME` = 6/256 per tick), dan memanaskan tetangganya sampai bahan mudah terbakar menyala sendiri. Pasir di sampingnya meleleh menjadi kaca. Lava thermite masih membakar terus menembus material.

## Menambah elemen (5 langkah)
1. `ElementId` di `sandbox.h`: tambahkan satu nilai sebelum `EL_COUNT` (urutan = urutan palet; `EL_EMPTY` harus tetap 0).
2. Satu baris di `k_elements[]` (`sandbox_elements.c`): nama, warna, `density`, `MoveKind` dan angka perilaku (`speed`, `spread`, `slip`, `grip`, `ignite`, `shade`, `spray`). Kolomnya dijelaskan di komentar di atas tabel.
3. Satu baris di `k_thermal[]` (`sandbox_heat.c`): suhu awal, daya hantar, kapasitas panas, suhu menyala sendiri, dan perubahan wujud. Baris yang terlupa berisi nol dan tidak berguna; salin baris elemen yang mirip.
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
