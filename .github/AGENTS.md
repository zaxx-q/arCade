<!-- Managed by agent: keep sections and order; edit content, not structure. Last updated: 2026-11-08 -->

# AGENTS.md - .github/

## Overview
CI and release automation. `workflows/ci.yml` (pull requests) and `workflows/release.yml` (push to `main`) both call the reusable `workflows/checks.yml`. A release is built only after the **full** checks pass. Releases are created from git tags; **workflows never commit to the repo**.

## Workflow files
| File | Trigger | Job names that matter |
|------|---------|-----------------------|
| `workflows/checks.yml` | `workflow_call` (`full: bool`) | `lint`, `linux`, `windows-mingw`; full lane: `sanitizers`, `static-analysis`, `scripts`, `windows-msvc`, `smoke` |
| `workflows/ci.yml` | pull_request, manual | `changes`, `pr-title`, `checks`, `ci-ok` (**the only required status check**) |
| `workflows/release.yml` | push to `main`, manual (`bump`, `prerelease`, `dry_run`) | `checks`, `version`, `build-linux`, `build-windows`, `publish` |
| `dependabot.yml` | weekly | bumps pinned action versions via PRs |

## Setup & environment
Validate workflow YAML locally with actionlint (`actionlint` in the repo root, needs a git repo). Version logic is plain bash: `bash tools/next_version.sh` previews the next version; `bash tools/tests/test_next_version.sh` tests it.

## Build & tests
- Presets used by CI: `logic`, `dev`, `release`, `asan`, `headless` (see `CMakePresets.json`). Reproduce a CI failure locally with the same preset.
- Dry-run a release without publishing: Actions -> Release -> Run workflow -> `dry_run = true`.

## Workflow conventions
- Pin actions to a **major version tag** (`actions/checkout@v7`); let Dependabot update them. Never use `@main`/`@latest`.
- Least privilege: top-level `permissions: contents: read`; only `publish` has `contents: write`.
- Never put `${{ github.event.* }}` (PR title, branch name, labels) directly in a `run:` script: pass it through `env:` and quote it (script injection).
- `concurrency`: CI cancels superseded runs; `release` serialises (never cancel a running release).
- `continue-on-error: true` jobs (`windows-msvc`, `smoke`) are not blocking yet: after one fully green run, remove the line.
- Branch protection to apply by hand: PRs only, squash merge, require the check **`CI OK`**.
- Versioning: tags `vX.Y.Z` only; the bump comes from Conventional Commits (`feat!` major, `feat` minor, else patch; `[skip release]` skips). No bump commits.

## Security & safety
- Never use `secrets: inherit`; the release uses only the built-in `github.token`.
- Do not add third-party actions without asking first; prefer plain `gh`/shell steps.

## PR/commit checklist
- [ ] `actionlint` is clean
- [ ] A release-affecting change was dry-run (`dry_run = true`) before merging
- [ ] Job names referenced in this file still exist

## Examples
- Good: `env: TITLE: ${{ github.event.pull_request.title }}` then `[[ "$TITLE" =~ $re ]]`.
- Bad: `run: echo "${{ github.event.pull_request.title }}"` (injection), `git commit` + `git push` from a workflow (re-triggers itself).

## When stuck
Open the failed job log, reproduce with the same preset locally, and check `docs/RELEASING.md`. Ask the user before changing triggers, permissions or required checks.
