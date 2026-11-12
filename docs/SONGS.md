# Lagu untuk Beat Dodge (MIDI -> chart yang bisa dimainkan)

## Apakah perlu 4 lane seperti FNF? Tidak.
File MIDI Friday Night Funkin' berisi **melodi** tiap karakter (waktu dan pitch not), bukan lane panahnya (informasi lane ada di color group FL Studio, yang tidak disimpan oleh `.mid`). Jadi lane selalu *diturunkan*: `tools/midi2beatmap` menaruh pitch rendah di kiri dan pitch tinggi di kanan, lalu menyeimbangkan lane-nya. Jumlah lane adalah keputusan gameplay: easy/normal memakai 3 lane, hard memakai 4 (`k_preset` di `games/beat_dodge/beat_dodge_convert.c`). Lagu mendukung 2 sampai 5 lane; kamu bisa menekan `1`-`4` untuk loncat ke sebuah lane.

## Yang dilakukan pipeline
1. `core/src/midi.c` membaca MIDI (not per track, tempo pertama). Semua file FNF berformat 1 dengan 96 tick/beat dan tempo konstan; sekitar 95-100% not berada di grid not 16th.
2. `beat_dodge_convert.c` menggabungkan track lawan dan track pemain ("Boyfriend"), menyisakan satu not per step 16th (not pemain menang kalau bentrok), menipiskannya per kesulitan (grid + jeda minimum dalam detik), memetakan pitch ke lane (partisi berurutan yang optimal, sehingga lane terpakai kira-kira sama banyak), dan menyimpan seluruh melodi untuk musik.
3. File teks lagu (`assets/beatmaps/fnf/<slug>.txt`) berisi 3 chart + melodi. Game me-render melodi itu (voice + drum sederhana) dengan `core/src/synth.c` menjadi WAV di memori lalu memainkannya; not berbunyi tepat saat rintangannya sampai di garis.

## Menambah atau membangun ulang lagu
```bash
cmake --preset logic && cmake --build --preset logic          # membangun build/logic/bin/midi2beatmap
build/logic/bin/midi2beatmap my.mid --report                  # track, not per detik untuk tiap kesulitan
build/logic/bin/midi2beatmap my.mid -o assets/beatmaps/my_song.txt --title "My Song"
echo my_song.txt >> assets/beatmaps/songs.txt                 # tampil di menu sesuai urutan ini
bash tools/songs/build_songs.sh                               # bangun ulang semua lagu FNF dari tools/songs/fnf/*.mid
```
`--player TEKS` memilih track pemain lewat sebagian namanya (default: "boyfriend"/"bf", kalau tidak ada maka track terakhir). Hanya tempo pertama yang dipakai (tool memberi peringatan kalau ada perubahan tempo). Satu lagu boleh berisi sampai 2048 step dan 1536 not melodi (`BD_MAX_*`). `ctest` memuat dan memainkan sempurna setiap lagu di `songs.txt`.

## Status dan langkah berikutnya
- Sudah diverifikasi: parsing, konversi, chart bisa dimainkan sempurna (unit test), pitch/timing synth (unit test), tampilan layar pilih lagu dan game (screenshot headless).
- BELUM diverifikasi (tidak ada perangkat suara di sandbox build): pemutaran audio yang sebenarnya dan sinkronisasi musik dengan rintangan. Kalau musik terdengar terlalu cepat atau terlambat, atur `BD_AUDIO_OFFSET_S` di `beat_dodge_game.c`.
- Ide: key leaderboard per lagu, preview melodi di layar pilih lagu, rintangan "hold" panjang dari not yang panjang, lagu dengan audio asli (`LoadMusicStream` dari file .ogg) memakai chart yang sama. Ide-ide soal tingkat kesulitan ada di `games/beat_dodge/GAME.md` (bagian "Langkah implementasi").
