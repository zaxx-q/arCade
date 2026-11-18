# Server leaderboard online

Cloudflare Worker + database D1. Klien (game C) berbicara dengan server ini lewat HTTPS; format datanya ditulis di komentar atas `core/include/arcade/lbnet.h` dan sisi server-nya ada di `src/protocol.ts`. Server ini bukan bagian dari kode C yang dinilai: foldernya terpisah dan tidak disentuh CMake.

URL yang sedang berjalan: `https://arcade-leaderboard.zaxxqgeminicli.workers.dev` (publik; subdomain `workers.dev` mengikuti akun Cloudflare).

| Endpoint | Fungsi |
|---|---|
| `POST /v1/register` | memberi token install (sekali per install) |
| `POST /v1/scores` | kirim `game\|pemain\|skor`; jawabannya `ok\|peringkat\|jumlah_pemain` |
| `GET /v1/top?board=<game>&limit=<n>` | tabel teratas (publik, tanpa token) |

Server hanya menyimpan skor **terbaik** tiap install per game, jadi mengirim skor yang sama dua kali tidak berbahaya (klien memang mengulang kirim kalau jaringan putus).

## Menjalankan

Syarat: `bun`, `wrangler` >= 4.36 (binding `ratelimits`), dan sudah `wrangler login`.

```sh
cd server
bun install
bun test              # 61 tes: protokol, SQL asli lewat bun:sqlite, semua jalur penolakan
bun run typecheck
```

Deploy pertama kali (sudah dilakukan; `database_id` ada di `wrangler.jsonc`):

```sh
wrangler d1 create arcade-leaderboard        # salin database_id ke wrangler.jsonc
bun run db:remote                            # terapkan migrations/ ke database di Cloudflare
bun run deploy
BASE_URL=https://<worker>.workers.dev bun run e2e
```

Perubahan berikutnya: edit kode, `bun test`, lalu `bun run deploy`. Perubahan skema selalu **file migrasi baru** (`migrations/0003_...sql`), jangan mengubah yang sudah diterapkan.

**`wrangler dev` tidak jalan di bawah Bun.** Wrangler menjawab 503 ("can't forward requests to your Worker under Bun"). Kalau ingin menjalankan server lokal, `wrangler dev` perlu Node (belum dicoba di sini). Alternatifnya: `bun test` sudah menjalankan handler dan SQL asli, dan `bun run e2e` menguji server yang sudah ter-deploy.

`bun run e2e` menambahkan beberapa baris bernama `e2e-...` ke tabel `different_color`. Hapus setelah menguji server produksi (lihat Moderasi).

## Menambah game

Tambahkan satu baris di `GAMES` (`src/protocol.ts`) berisi `id` game dan skor tertinggi yang mungkin, lalu deploy **sebelum** merilis klien yang mengirim game itu: klien membuang skor yang ditolak server. Papan per mode (`game:papan`) sengaja ditolak dulu, karena nama papan bebas dan setiap nama baru akan membuat tabel baru.

## Moderasi

Tidak ada endpoint admin (lebih sedikit yang bisa diserang). Semuanya lewat `wrangler`:

```sh
wrangler d1 execute arcade-leaderboard --remote --command "SELECT board, player, score, install FROM scores ORDER BY board, score DESC LIMIT 50"
wrangler d1 execute arcade-leaderboard --remote --command "DELETE FROM scores WHERE board='loop_lap' AND player='nama'"
wrangler d1 execute arcade-leaderboard --remote --command "DELETE FROM scores WHERE player LIKE 'e2e-%'"
# memblokir satu install: hapus skornya dan tokennya (kolom install = hash token)
wrangler d1 execute arcade-leaderboard --remote --command "DELETE FROM scores WHERE install='<hash>'; DELETE FROM installs WHERE token_hash='<hash>'"
wrangler d1 export arcade-leaderboard --remote --output backup.sql      # cadangan
```

## Apa yang dijaga, dan apa yang tidak

Dijaga (dihitung pasti di kode):
- game dikenal, skor bulat tidak negatif dan tidak lebih dari yang mungkin di game itu (`GAMES`), nama 1 sampai 15 karakter ASCII yang bisa diketik, satu baris saja, isi permintaan paling banyak 512 byte;
- satu baris per install per game, dan skor yang lebih buruk tidak menulis apa pun;
- token tidak disimpan, hanya hash SHA-256-nya;
- install baru paling banyak `MAX_REGISTRATIONS_PER_HOUR` (300) per jam untuk seluruh server, dihitung dari tabel `installs`. Inilah batas yang pasti: setiap pemain palsu butuh satu install.

**Tidak** dijaga:
- Binding rate limit Cloudflare hanya *best effort*: penghitungnya per mesin dan disinkronkan pelan-pelan. Diukur pada server ini: tepat di satu koneksi yang dipakai ulang (59 dari 200 lolos pada batas 60), tetapi hampir tidak berpengaruh pada 20 koneksi paralel (398 dari 400 lolos). Anggap itu penghambat skrip sederhana, bukan perlindungan.
- Skor palsu yang masuk akal. Kode game publik dan file di komputer pemain bisa diubah, jadi siapa pun bisa mengirim permintaan sendiri dengan skor di bawah batas game. Pencegahan yang sebenarnya (server memutar ulang rekaman input) belum ada dan butuh perubahan di setiap game. Untuk sekarang: batas skor, batas pendaftaran, dan hapus manual.
- Serangan banjir permintaan. Paket gratis punya kuota harian; di atas itu Cloudflare menolak permintaan untuk semua orang sampai kuota pulih.
