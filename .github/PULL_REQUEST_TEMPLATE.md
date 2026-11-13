<!-- The PR title must be a Conventional Commit: it determines the next version when squash-merged.
     feat(echo_maze): add sonar ping   -> minor      fix(sandbox): water leak   -> patch
     feat(core)!: change GameContext   -> major      docs/chore/test/ci/refactor -> patch -->

## What and why

## Checklist
- [ ] `cmake --preset logic && cmake --build --preset logic && ctest --preset logic` passes
- [ ] `bash tools/format.sh` and `bash tools/check_repo.sh` pass
- [ ] Game changes: `GAME.md` (pseudocode / flowchart / course-concept table) is up to date
- [ ] No `TODO(owner)` remains for work claimed as complete by this PR
