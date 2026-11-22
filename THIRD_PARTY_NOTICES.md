# Pemberitahuan pihak ketiga (Third-party notices)

## raylib (versi ada di `ARCADE_RAYLIB_TAG`, saat ini 6.2)

Diunduh dan di-link secara statis saat build. https://www.raylib.com / https://github.com/raysan5/raylib

Teks lisensi di bawah ini sengaja dibiarkan dalam bahasa Inggris asli, karena itulah teks yang mengikat secara hukum.

```
Copyright (c) 2013-2026 Ramon Santamaria (@raysan5)

This software is provided "as-is", without any express or implied warranty. In no event
will the authors be held liable for any damages arising from the use of this software.

Permission is granted to anyone to use this software for any purpose, including commercial
applications, and to alter it and redistribute it freely, subject to the following restrictions:

  1. The origin of this software must not be misrepresented; you must not claim that you
  wrote the original software. If you use this software in a product, an acknowledgment
  in the product documentation would be appreciated but is not required.

  2. Altered source versions must be plainly marked as such, and must not be misrepresented
  as being the original software.

  3. This notice may not be removed or altered from any source distribution.
```

## Font, audio, art

Selain yang tercantum di bawah, tidak ada aset yang disertakan: semua grafis game lain digambar dengan bentuk-bentuk raylib dan font bawaannya; suara disintesis lewat kode. Kalau kamu menambahkan aset (misalnya paket CC0 dari kenney.nl), tuliskan di sini lengkap dengan pembuat, lisensi, dan URL-nya, lalu kutip di Daftar Pustaka laporan.

## Gritty Golf (game): `games/gritty_golf/gg/`, `games/gritty_golf/tests/gg/`, `assets/gritty_golf/`
*Gritty Golf: C Edition* oleh zaxx-q (github.com/zaxx-q/gritty-golf). Disalin dengan perubahan minimal; daftar perubahan dan cara sinkron ulang ada di `games/gritty_golf/gg/VENDORED.md`. Bagian-bagiannya punya lisensi berbeda:

| Bagian | Lokasi | Lisensi |
|--------|--------|---------|
| Kode C engine, test, dan tekstur serta logo (digambar ulang lewat skrip upstream) | `games/gritty_golf/gg/`, `games/gritty_golf/tests/gg/`, `assets/gritty_golf/textures/` | MIT, Copyright (c) 2026 zaxx-q (`games/gritty_golf/gg/LICENSE`). Syarat yang sama dengan arCade. |
| Font Silkscreen | `assets/gritty_golf/font/` | SIL Open Font License 1.1, Copyright 2001 The Silkscreen Project Authors (https://github.com/googlefonts/silkscreen). Teks lengkapnya ada di `assets/gritty_golf/font/OFL-Silkscreen.txt` dan harus menyertai font ini di setiap distribusi (paket rilis sudah menyertakan `assets/`). |
| **Data 25 level** (`games/gritty_golf/gg/levels.c`, `levels.h`) | `games/gritty_golf/gg/` | Hak cipta pencipta aslinya, *Escada Games* |
| Palet warna `1bit-styx` | `games/gritty_golf/gg/palette_colors.h` | Dari Lospec: https://lospec.com/palette-list/1bit-styx |


## Game hasil port dari `simple-games` (hanya aturan)

Delapan game (`post_it`, `master_juggler`, `bw_reflex`, `different_color`, `spike_dart`, `loop_lap`, `tower_stacker`, `new_star`) mengimplementasikan ulang mekanik permainan yang didokumentasikan di proyek saudara `simple-games` (spesifikasi aturan dan vektor uji). Tidak ada gambar, suara, font, teks, maupun kode dari proyek itu yang disalin: semua grafis digambar dengan bentuk raylib, semua suara disintesis lewat `gfx_make_tone()`, dan teks bantuan ditulis ulang.

## Synthwave City (game): `deferred/synthwave_city/`
Port ke C99 dari *Synthwave City* oleh Kris E. Granholm (https://github.com/highsierralabs/Synthwave_City), lisensi MIT. Yang diterjemahkan adalah algoritmanya: pembangkit rencana kota (distrik, gedung, situs, jalur), paket kendaraan udara (jalur langit, kapal besar, bentuk lambung, tabel lampu), kehidupan jalan (tabel jalur, mobil, orang), data hologram (tata letak atlas, panel iklan, cincin teks berjalan, benda kawat) beserta nama merek iklan rekaan dan tata letak gambar iklan di atlas (digambar ulang oleh kode C sendiri, `swc_atlas.c`), RNG `mulberry32`/`hash32`, model terbang kamera dan drone tur, lalu lintas periodik, dan komposisi lagu; keluaran C dicocokkan dengan versi TypeScript-nya angka demi angka. Tampilannya dibawa dalam dua jalur: jalur CPU yang ditulis ulang dengan rlgl, dan jalur GPU berbasis shader GLSL. Pada jalur GPU, teks shader dari versi web (`deferred/synthwave_city/shaders/common.glsl`, `common_vertex.glsl`, `sky.vert`, `sky.frag`, `buildings.vert`, `buildings.frag`, `lights.vert`, `lights.frag`, `beams.vert`, `beams.frag`, `flare.vert`, `flare.frag`, `ground_defines.glsl`, `ground.vert`, `ground.frag`, `lamp_post.vert`, `lamp_post.frag`, `lamp_glow.vert`, `lamp_glow.frag`, `hull.vert`, `hull.frag`, `craft_light.vert`, `air_light.frag`, `hero.vert`, `hero.frag`, `hero_light.vert`, `sl_car.vert`, `sl_car.frag`, `sl_far.vert`, `sl_far.frag`, `sl_lamp.vert`, `sl_lamp.frag`, `sl_pool.vert`, `sl_pool.frag`, `sl_walker.vert`, `sl_walker.frag`, `holo_panel.vert`, `holo_panel.frag`, `holo_wire.vert`, `holo_wire.frag`, `holo_ticker.vert`, `holo_ticker.frag`) disalin dengan penyesuaian ke GLSL 3.30 dan membawa baris hak ciptanya; konstanta suasana, palet, dan nilai pasca-proses (bloom, grade) juga diambil dari sana. Suaranya dirender oleh synthesizer bawaan arCade. Tidak ada gambar, font, atau berkas audio dari proyek itu yang disalin: gambar iklan digambar oleh kode dengan rasteriser dan huruf goresan milik port ini. Teks lisensi di bawah ini sengaja dibiarkan dalam bahasa Inggris asli, karena itulah teks yang mengikat secara hukum, dan harus menyertai bagian turunan ini di setiap distribusi.

```
MIT License

Copyright (c) 2026 Kris E. Granholm

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
```

### Algoritma pasca-proses yang diikuti jalur GPU: pmndrs/postprocessing dan three.js
Bloom (`deferred/synthwave_city/shaders/bloom_luma.frag`, `bloom_down.frag`, `bloom_up.frag`), aberrasi kromatik dan komposit akhir (`post.frag`) ditulis mengikuti algoritma `LuminanceMaterial`, `DownsamplingMaterial`, `UpsamplingMaterial`, `ChromaticAberrationEffect` dari pmndrs/postprocessing (https://github.com/pmndrs/postprocessing, lisensi zlib) dan pengodean sRGB beserta dither-nya dari three.js (https://github.com/mrdoob/three.js, lisensi MIT), yang dipakai versi web. Algoritmanya ditulis ulang dalam GLSL 3.30 dan bukan salinan berkas aslinya; tetap saja, berikut pemberitahuan keduanya (teks dibiarkan dalam bahasa Inggris asli):

```
pmndrs/postprocessing - zlib license

Copyright (c) 2015 Raoul van Rüschen

This software is provided 'as-is', without any express or implied warranty. In
no event will the authors be held liable for any damages arising from the use of
this software.

Permission is granted to anyone to use this software for any purpose, including
commercial applications, and to alter it and redistribute it freely, subject to
the following restrictions:

1. The origin of this software must not be misrepresented; you must not claim
   that you wrote the original software. If you use this software in a product,
   an acknowledgment in the product documentation would be appreciated but is
   not required.

2. Altered source versions must be plainly marked as such, and must not be
   misrepresented as being the original software.

3. This notice may not be removed or altered from any source distribution.
```

```
three.js - MIT license

Copyright (c) 2010-2026 three.js authors

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
```

## Poiret One (font): `assets/fonts/PoiretOne-Regular.ttf`
Copyright (c) 2011, Denis Masharov. SIL Open Font License 1.1: teks lengkapnya ada di `assets/fonts/OFL.txt` dan harus menyertai font ini di setiap distribusi (paket rilis sudah menyertakan `assets/`).

## Data lagu yang diturunkan dari transkripsi MIDI Friday Night Funkin'
`tools/songs/fnf/*.mid` dan `assets/beatmaps/fnf/*.txt`. Sumber file MIDI: "MIDI-FNF-Full-And-Most-Accurate" oleh FujiwaraZachWatterson (GitHub; lihat `tools/songs/fnf/SOURCE_README.md`). Komposisi: Kawai Sprite / Funkin' Crew (Friday Night Funkin').
