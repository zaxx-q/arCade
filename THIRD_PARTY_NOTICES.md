# Third-party notices

A release is a single executable: everything in `assets/`, this file and `LICENSE` are compiled into it, and `arCade --licenses` prints them together with the two font licenses below.

Apart from the items listed below, no assets are bundled: all other game graphics are drawn with raylib's shapes and its built-in font, and sounds are synthesized in code.

<!-- Adding an asset? List it here with its creator, license, and URL, and cite it in the report's bibliography (e.g. a CC0 pack from kenney.nl). -->

## raylib (version set in `ARCADE_RAYLIB_TAG`, currently 6.2)

Downloaded and statically linked at build time. zlib license. https://www.raylib.com / https://github.com/raysan5/raylib

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

## curl / libcurl (version set in `ARCADE_CURL_VERSION` in `cmake/ArcadeCurl.cmake`, currently 8.23.0)

Used by the online leaderboard for HTTPS. Release builds download the source (pinned by SHA-256) and link it statically; development builds use the system's libcurl when there is one, and that is not redistributed. https://curl.se / https://github.com/curl/curl

The license text below is the `COPYING` of that release, copied exactly.

```
COPYRIGHT AND PERMISSION NOTICE

Copyright (c) 1996 - 2026, Daniel Stenberg, <daniel@haxx.se>, and many
contributors, see the THANKS file.

All rights reserved.

Permission to use, copy, modify, and distribute this software for any purpose
with or without fee is hereby granted, provided that the above copyright
notice and this permission notice appear in all copies.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT OF THIRD PARTY RIGHTS. IN
NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR
OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE
OR OTHER DEALINGS IN THE SOFTWARE.

Except as contained in this notice, the name of a copyright holder shall not
be used in advertising or otherwise to promote the sale, use or other dealings
in this Software without prior written authorization of the copyright holder.
```

## OpenSSL (Linux release builds only)

libcurl uses OpenSSL for TLS on Linux, and the Linux release links it statically (OpenSSL 3.x, from `libssl-dev` on the release runner). Windows builds use Schannel, the TLS of Windows, and contain no OpenSSL. Copyright (c) The OpenSSL Project Authors. Apache License 2.0, full text at https://www.apache.org/licenses/LICENSE-2.0. https://www.openssl.org / https://github.com/openssl/openssl

## Synthwave City (game): `deferred/synthwave_city/`

A C99 port of *Synthwave City* by Kris E. Granholm (https://github.com/highsierralabs/Synthwave_City), MIT license (full text at the end of this file). The city, traffic, and song generators, the flight model, and the hologram data (including the made-up ad brand names) were ported from TypeScript to C and matched to the original number for number. The GLSL scene shaders in `deferred/synthwave_city/shaders/` were copied with adjustments to GLSL 3.30: all of them except `bloom_*.frag`, `post.frag`, and the glue files `fullscreen.vert`, `prelude_vs.glsl`, and `prelude_fs.glsl`. No images, fonts, or audio files from that project were copied: the ad images are drawn by code and the sound is rendered by arCade's own synthesizer.

The post-processing shaders (`bloom_*.frag`, `post.frag`) follow algorithms from pmndrs/postprocessing (https://github.com/pmndrs/postprocessing, zlib license) and three.js (https://github.com/mrdoob/three.js, MIT license). They were rewritten in GLSL 3.30, not copied, so this is a credit only.

## Gritty Golf (game): `games/gritty_golf/gg/`, `games/gritty_golf/tests/gg/`, `assets/gritty_golf/`

*Gritty Golf: C Edition* by zaxx-q (github.com/zaxx-q/gritty-golf), copied with minimal changes; the list of changes and how to re-sync are in `games/gritty_golf/gg/VENDORED.md`. Its parts are under different licenses:

- **Code, tests, textures, and logo** (`games/gritty_golf/gg/`, `games/gritty_golf/tests/gg/`, `assets/gritty_golf/textures/`): MIT, Copyright (c) 2026 zaxx-q (full text at the end of this file, also in `games/gritty_golf/gg/LICENSE`). Same terms as arCade.
- **Silkscreen font** (`assets/gritty_golf/font/`): SIL Open Font License 1.1, Copyright 2001 The Silkscreen Project Authors (https://github.com/googlefonts/silkscreen). The full text is in `assets/gritty_golf/font/OFL-Silkscreen.txt` and must accompany the font in every distribution (it is compiled into the executable; `arCade --licenses` prints it).
- **Data for the 25 levels** (`games/gritty_golf/gg/levels.c`, `levels.h`): copyright of the original creator, *Escada Games*.
- **`1bit-styx` color palette** (`games/gritty_golf/gg/palette_colors.h`): from Lospec, https://lospec.com/palette-list/1bit-styx

## Poiret One (font): `assets/fonts/PoiretOne-Regular.ttf`

Copyright (c) 2011, Denis Masharov. SIL Open Font License 1.1: the full text is in `assets/fonts/OFL.txt` and must accompany the font in every distribution (it is compiled into the executable; `arCade --licenses` prints it).

## Song data derived from Friday Night Funkin' MIDI transcriptions

`tools/songs/fnf/*.mid` and `assets/beatmaps/fnf/*.txt`. MIDI file source: "MIDI-FNF-Full-And-Most-Accurate" by FujiwaraZachWatterson (GitHub; see `tools/songs/fnf/SOURCE_README.md`). Composition: Kawai Sprite / Funkin' Crew (Friday Night Funkin').

## MIT License (Synthwave City, Gritty Golf code)

```
MIT License

Copyright (c) 2026 Kris E. Granholm
Copyright (c) 2026 zaxx-q

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
