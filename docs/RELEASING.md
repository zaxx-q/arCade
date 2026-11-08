# Releasing

## How versions work
- Tags `vMAJOR.MINOR.PATCH` are the single source of truth. **No workflow ever commits** a version bump (that would re-trigger itself and conflict with your PRs).
- On every push to `main` (a squash-merged PR), `release.yml` runs the **full** checks, then `tools/next_version.sh` decides the bump from the commit messages since the last tag:

  | Commit/PR title | Bump |
  |-----------------|------|
  | `feat!: ...`, `refactor(core)!: ...`, or a `BREAKING CHANGE:` line | major |
  | `feat: ...` / `feat(scope): ...` | minor |
  | anything else (`fix`, `docs`, `chore`, free text) | patch |
  | `[skip release]` anywhere in the newest commit | no release |

  The highest level wins. The first release is `v0.1.0`.
- Then Linux (`tar.gz`) and Windows (`zip`) builds are packaged, a `SHA256SUMS.txt` is added, and a GitHub Release with generated notes is published.
- Manual run: **Actions -> Release -> Run workflow**: `bump` (auto/patch/minor/major), `prerelease`, `dry_run` (builds everything, publishes nothing). A manual bump ignores `[skip release]`.

## One-time repository setup (do these by hand on GitHub)
1. Create the repository under **your personal account** (Education plan minutes apply to the owner; private is fine until submission).
2. Push `main`. Settings -> General -> Pull Requests: allow **squash merging only**, default to PR title.
3. Settings -> Actions -> General: allow actions and reusable workflows; Workflow permissions: **read repository contents** (the release job requests write itself).
4. Settings -> Branches (or Rulesets): protect `main`: require a pull request, require the status check **`CI OK`**, block force pushes. (Private repos need a plan that supports branch protection: GitHub Pro via Education does.)
5. Labels: create `full-ci` (a PR with this label also runs the full lane).
6. Run Actions -> Release with `dry_run = true` once to see every job pass before the first real release.

## Promoting non-blocking jobs
`windows-msvc` and `smoke` have `continue-on-error: true`. After they have been green on a few runs, delete that line in `.github/workflows/checks.yml` so they block.

## Submission snapshot
Before the deadline: make the repository public (or add the lecturer), merge everything, let the Release finish, and put the **release page link** and the **repository link** in the report (Bab II). Download the Windows zip yourself on a clean machine and run it once.

## Linux release portability
The Linux build runs on the oldest Ubuntu runner still offered (`ubuntu-22.04` in `release.yml`) so the binary needs an older glibc. When GitHub retires that image, move to the oldest remaining one.
