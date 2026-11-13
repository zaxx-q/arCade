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


## Poiret One (font): `assets/fonts/PoiretOne-Regular.ttf`
Copyright (c) 2011, Denis Masharov. SIL Open Font License 1.1: teks lengkapnya ada di `assets/fonts/OFL.txt` dan harus menyertai font ini di setiap distribusi (paket rilis sudah menyertakan `assets/`).

## Data lagu yang diturunkan dari transkripsi MIDI Friday Night Funkin'
`tools/songs/fnf/*.mid` dan `assets/beatmaps/fnf/*.txt`. Sumber file MIDI: "MIDI-FNF-Full-And-Most-Accurate" oleh FujiwaraZachWatterson (GitHub; lihat `tools/songs/fnf/SOURCE_README.md`). Komposisi: Kawai Sprite / Funkin' Crew (Friday Night Funkin').
