# Rilis (Releasing)

## Cara kerja versi
- Tag `vMAJOR.MINOR.PATCH` adalah satu-satunya sumber kebenaran. **Tidak ada workflow yang meng-commit** kenaikan versi (itu akan memicu dirinya sendiri lagi dan bentrok dengan PR kalian).
- Setiap push ke `main` (hasil merge-commit sebuah PR), `release.yml` menjalankan pengecekan **lengkap**, lalu `tools/next_version.sh` menentukan kenaikan versi dari pesan commit sejak tag terakhir:

  | Judul commit/PR | Kenaikan |
  |-----------------|----------|
  | `feat!: ...`, `refactor(core)!: ...`, atau baris `BREAKING CHANGE:` | major |
  | `feat: ...` / `feat(scope): ...` | minor |
  | selain itu (`fix`, `docs`, `chore`, teks bebas) | patch |
  | `[skip release]` di mana saja pada commit terbaru | tidak ada rilis |

  Level tertinggi yang menang. Rilis pertama adalah `v0.1.0`.
- Setelah itu build Linux (`tar.gz`) dan Windows (`zip`) dipaketkan, `SHA256SUMS.txt` ditambahkan, dan GitHub Release dengan release notes otomatis dipublikasikan.
- Jalankan manual: **Actions -> Release -> Run workflow**: `bump` (auto/patch/minor/major), `prerelease`, `dry_run` (membangun semuanya tanpa mempublikasikan apa pun). Bump manual mengabaikan `[skip release]`.

## Setup repository sekali saja (lakukan manual di GitHub)
1. Buat repository di **akun pribadi kamu** (jatah menit Education plan berlaku untuk pemilik repo; private tidak masalah sampai pengumpulan).
2. Push `main`. Settings -> General -> Pull Requests: izinkan **hanya merge commit** (squash dan rebase dinonaktifkan agar seluruh history commit tersimpan), dan default ke judul PR.
3. Settings -> Actions -> General: izinkan actions dan reusable workflows; Workflow permissions: **read repository contents** (job rilis meminta izin write sendiri).
4. Settings -> Branches (atau Rulesets): lindungi `main`: wajibkan pull request, wajibkan status check **`CI OK`**, blokir force push. (Repo private butuh plan yang mendukung branch protection: GitHub Pro lewat Education mendukungnya.)
5. Label: buat `full-ci` (PR dengan label ini juga menjalankan jalur lengkap).
6. Jalankan Actions -> Release dengan `dry_run = true` sekali untuk melihat semua job lolos sebelum rilis sungguhan pertama.

## Leaderboard online di rilis
Setiap rilis membawa tab Online. Supaya game tetap jalan di komputer yang hanya punya library desktop, libcurl **tidak diambil dari sistem**: ia dibangun dari source dan ditautkan statis (`cmake/ArcadeCurl.cmake`, dipakai lewat `-DARCADE_ONLINE=ON -DARCADE_ONLINE_BUNDLE_CURL=ON` di `release.yml`).
- **Windows:** TLS lewat Schannel (bagian dari Windows, sertifikat dari penyimpanan sertifikat Windows). Tidak ada OpenSSL dan tidak ada DLL tambahan; janji "tanpa DLL tambahan" tetap berlaku.
- **Linux:** OpenSSL ditautkan statis (runner perlu `libssl-dev`, yang menyertakan `libssl.a` dan `libcrypto.a`). Lokasi file sertifikat CA berbeda di tiap distro, jadi `app/net_curl.c` mencarinya saat game berjalan: `$SSL_CERT_FILE`, lalu lokasi umum Debian/Ubuntu, Fedora/RHEL, openSUSE, Alpine, dan FreeBSD. Kalau tidak ada, tab Online tampil offline: skor **tidak pernah** dikirim tanpa memeriksa sertifikat server.
- `ON` berarti dependensi yang hilang **menggagalkan build**, bukan rilis diam-diam tanpa tab Online. Setelah build, job rilis memeriksa bahwa binary berisi kode online dan tidak bergantung pada libcurl/OpenSSL (Linux: `ldd`) atau DLL yang bukan milik Windows (Windows: `lib*.dll`, `zlib*.dll`).
- PR juga memeriksanya: job `linux` membangun konfigurasi yang sama (`build/bundled`, tanpa membangun raylib) dan `windows-mingw` memakainya untuk build penuh. `windows-msvc` memakai `-DARCADE_ONLINE=OFF`.
- **Dry-run:** perubahan pada langkah build rilis harus dicoba dengan `dry_run = true` sebelum merge.

### Menaikkan versi curl
1. Ubah `ARCADE_CURL_VERSION` dan `ARCADE_CURL_SHA256` **bersamaan** di `cmake/ArcadeCurl.cmake` (hash dari `sha256sum` file `.tar.xz` di https://github.com/curl/curl/releases), dan baca ulang opsi CMake versi barunya (nama opsi curl bisa berubah antarversi).
2. Ganti teks lisensi curl di `THIRD_PARTY_NOTICES.md` dengan isi `COPYING` dari tarball itu, disalin persis.
3. Jalankan Release dengan `dry_run = true`.

## Menaikkan job yang tidak memblokir
`windows-msvc` dan `smoke` memakai `continue-on-error: true`. Setelah keduanya hijau beberapa kali berturut-turut, hapus baris itu di `.github/workflows/checks.yml` supaya mereka ikut memblokir.

## Portabilitas rilis Linux
Build Linux berjalan di runner Ubuntu tertua yang masih disediakan (`ubuntu-22.04` di `release.yml`) supaya binary-nya cukup butuh glibc yang lebih lama. Saat GitHub menghentikan image itu, pindah ke yang tertua berikutnya.
