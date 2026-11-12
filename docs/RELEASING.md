# Rilis (Releasing)

## Cara kerja versi
- Tag `vMAJOR.MINOR.PATCH` adalah satu-satunya sumber kebenaran. **Tidak ada workflow yang meng-commit** kenaikan versi (itu akan memicu dirinya sendiri lagi dan bentrok dengan PR kalian).
- Setiap push ke `main` (hasil squash-merge sebuah PR), `release.yml` menjalankan pengecekan **lengkap**, lalu `tools/next_version.sh` menentukan kenaikan versi dari pesan commit sejak tag terakhir:

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
2. Push `main`. Settings -> General -> Pull Requests: izinkan **hanya squash merging**, dan default ke judul PR.
3. Settings -> Actions -> General: izinkan actions dan reusable workflows; Workflow permissions: **read repository contents** (job rilis meminta izin write sendiri).
4. Settings -> Branches (atau Rulesets): lindungi `main`: wajibkan pull request, wajibkan status check **`CI OK`**, blokir force push. (Repo private butuh plan yang mendukung branch protection: GitHub Pro lewat Education mendukungnya.)
5. Label: buat `full-ci` (PR dengan label ini juga menjalankan jalur lengkap).
6. Jalankan Actions -> Release dengan `dry_run = true` sekali untuk melihat semua job lolos sebelum rilis sungguhan pertama.

## Menaikkan job yang tidak memblokir
`windows-msvc` dan `smoke` memakai `continue-on-error: true`. Setelah keduanya hijau beberapa kali berturut-turut, hapus baris itu di `.github/workflows/checks.yml` supaya mereka ikut memblokir.

## Portabilitas rilis Linux
Build Linux berjalan di runner Ubuntu tertua yang masih disediakan (`ubuntu-22.04` di `release.yml`) supaya binary-nya cukup butuh glibc yang lebih lama. Saat GitHub menghentikan image itu, pindah ke yang tertua berikutnya.
