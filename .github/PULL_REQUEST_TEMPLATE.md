<!-- JUDUL PR harus berupa Conventional Commit: judul inilah yang menentukan versi berikutnya saat di-squash-merge.
     feat(echo_maze): add sonar ping   -> minor      fix(sandbox): water leak   -> patch
     feat(core)!: change GameContext   -> major      docs/chore/test/ci/refactor -> patch -->

## Apa dan kenapa

## Checklist
- [ ] `cmake --preset logic && cmake --build --preset logic && ctest --preset logic` lolos
- [ ] `bash tools/format.sh` dan `bash tools/check_repo.sh` bersih
- [ ] Perubahan pada game: `GAME.md` (pseudocode / flowchart / tabel konsep kuliah) sudah diperbarui
- [ ] Tidak ada `TODO(owner)` tersisa untuk pekerjaan yang diklaim selesai oleh PR ini
