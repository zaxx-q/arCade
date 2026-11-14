# Lagu untuk Beat Dodge dan Beat Catch (MIDI -> chart yang bisa dimainkan)

## Apakah perlu 4 lane seperti FNF? Tidak.
File MIDI Friday Night Funkin' berisi **melodi** tiap karakter (waktu dan pitch not), bukan lane panahnya (informasi lane ada di color group FL Studio, yang tidak disimpan oleh `.mid`). Jadi lane selalu *diturunkan*: `tools/midi2beatmap` menaruh pitch rendah di kiri dan pitch tinggi di kanan, lalu menyeimbangkan lane-nya. Jumlah lane adalah keputusan gameplay: untuk Beat Dodge easy/normal memakai 3 lane dan hard 4; untuk Beat Catch easy 3, normal dan hard 4 (`k_preset` di `games/beat_dodge/beat_dodge_convert.c`). Lagu mendukung 2 sampai 5 lane; kamu bisa menekan `1`-`4` untuk loncat ke sebuah lane.

## Yang dilakukan pipeline
1. `core/src/midi.c` membaca MIDI (not per track, tempo pertama). Semua file FNF berformat 1 dengan 96 tick/beat dan tempo konstan; sekitar 95-100% not berada di grid not 16th.
2. `beat_dodge_convert.c` menggabungkan track lawan dan track pemain ("Boyfriend"), menyisakan satu not per step 16th (not pemain menang kalau bentrok), lalu membuat **enam chart**: easy/normal/hard untuk Beat Dodge (dari kedua voice) dan easy/normal/hard untuk Beat Catch (hanya dari voice pemain). Setiap chart dibuat begini:
   1. Not yang berjarak paling lama `BD_RIBBON_GAP_SEC` = 0,25 detik membentuk rantai; rantai yang lebih panjang dari `BD_RIBBON_MAX_SEC` = 1,2 detik dipotong di celah terbesarnya. Setiap potongan adalah **ribbon**: bunyi yang cepat tidak dibuang, ia digambar sebagai pita bead yang disambung dan dihitung sebagai satu objek.
   2. Not tunggal ditipiskan per kesulitan (grid + jeda minimum dalam detik); ribbon selalu dipertahankan.
   3. Pitch dipetakan ke lane (partisi berurutan yang optimal dengan dynamic programming, sehingga lane terpakai kira-kira sama banyak). Ribbon hanya boleh menyentuh sejumlah lane tertentu (lebar ribbon 1 / 2 / 3 untuk easy / normal / hard); lane yang berlebih dilipat ke lane terdekat, dengan penalti untuk jendela lane yang membiarkan lane lain menunggu terlalu lama (melawan camping).
   4. Chart divalidasi dengan cek lane yang terjangkau (manusia model: 0,12 detik per langkah lane). Ribbon yang membuat pemain tidak punya tempat berdiri dipindah ke jendela lane lain, not tunggal yang melakukan hal serupa dibuang. Cek itu tidak pernah dilonggarkan.
   Seluruh melodi disimpan untuk musik.
3. File teks lagu (`assets/beatmaps/fnf/<slug>.txt`, format versi 3: huruf `a`-`e` untuk bead yang melanjutkan ribbon, kata `catch` di baris `chart`) berisi 6 chart + melodi. Game me-render melodi itu (voice + drum sederhana) dengan `core/src/synth.c` menjadi WAV di memori lalu memainkannya; not berbunyi tepat saat beadnya sampai di garis. Not yang langkahnya punya bead di chart yang dimainkan berbunyi sebagai **lead** (voice 0, terang), not lainnya sebagai **backing** (voice 3, pelan), jadi yang terdengar jelas adalah yang terlihat.

## Menambah atau membangun ulang lagu
```bash
cmake --preset logic && cmake --build --preset logic          # membangun build/logic/bin/midi2beatmap
build/logic/bin/midi2beatmap my.mid --report                  # track, bead per detik dan ukuran untuk tiap chart
build/logic/bin/midi2beatmap --metrics assets/beatmaps/fnf/*.txt   # ukuran semua lagu yang sudah jadi (rata-rata per kesulitan)
build/logic/bin/midi2beatmap my.mid -o assets/beatmaps/my_song.txt --title "My Song"
echo my_song.txt >> assets/beatmaps/songs.txt                 # tampil di menu sesuai urutan ini
bash tools/songs/build_songs.sh                               # bangun ulang semua lagu FNF dari tools/songs/fnf/*.mid
```
`--player TEKS` memilih track pemain lewat sebagian namanya (default: "boyfriend"/"bf", kalau tidak ada maka track terakhir). Hanya tempo pertama yang dipakai (tool memberi peringatan kalau ada perubahan tempo). Satu lagu boleh berisi sampai 2048 step, 6 chart, dan 1536 not melodi (`BD_MAX_*`). `ctest` memuat setiap lagu di `songs.txt`, memeriksa semua chartnya (lolos cek lane terjangkau, punya jalur yang bisa diikuti manusia, tidak ada burst tanpa ribbon, tidak ada lane yang diabaikan terlalu lama) dan memainkannya dengan bot di Beat Dodge dan Beat Catch. Setelah mengubah konverter atau cek lane terjangkau, jalankan `bash tools/songs/build_songs.sh` untuk membangun ulang semua lagu.

## Status dan langkah berikutnya
- Sudah diverifikasi: parsing, konversi, chart bisa dimainkan (bot kecepatan manusia menang tanpa kena di semua chart dodge dan menang di semua chart catch dengan kurang dari 10% objek gagal (terukur: tidak ada)), pembagian lead/backing, pitch/timing synth (unit test), tampilan layar pilih lagu dan game (screenshot headless).
- BELUM diverifikasi (tidak ada perangkat suara di sandbox build): pemutaran audio yang sebenarnya dan sinkronisasi musik dengan rintangan. Menurut pemilik sinkron audio sudah baik, jadi `BD_AUDIO_OFFSET_S` di `beat_dodge_game.c` (dipakai bersama Beat Dodge dan Beat Catch) tidak diubah.
- Ide: key leaderboard per lagu, preview melodi di layar pilih lagu, rintangan "hold" panjang dari not yang panjang, bead drum untuk lane yang lama kosong (dicoba, tidak dikirim), lagu dengan audio asli (`LoadMusicStream` dari file .ogg) memakai chart yang sama. Hasil pengukuran dan keputusan desain ada di `games/beat_dodge/GAME.md` (bagian "Langkah implementasi"), dan aturan Beat Catch di `games/beat_catch/GAME.md`.
