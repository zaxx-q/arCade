#!/usr/bin/env bash
# Tests for tools/next_version.sh using throw-away git repositories.
#   bash tools/tests/test_next_version.sh
set -uo pipefail
HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
SCRIPT="$HERE/../next_version.sh"
TMP="$(mktemp -d)"
trap 'rm -rf "$TMP"' EXIT

pass=0; fail=0
repo="$TMP/repo"
newrepo() { rm -rf "$repo"; mkdir -p "$repo"; git -C "$repo" init -q; git -C "$repo" config user.email t@t; git -C "$repo" config user.name t; git -C "$repo" config commit.gpgsign false; git -C "$repo" config tag.gpgsign false; }
commit() { git -C "$repo" commit -q --allow-empty -m "$1" ${2:+-m "$2"}; }
tag() { git -C "$repo" tag "$1"; }
get() { "$SCRIPT" --repo "$repo" "$@" | grep "^$KEY=" | cut -d= -f2-; }
check() { # name key expected [script args...]
  local name="$1" KEY="$2" expected="$3"; shift 3
  local got; got="$(get "$@")"
  if [ "$got" = "$expected" ]; then pass=$((pass + 1)); else fail=$((fail + 1)); echo "FAIL: $name: $KEY expected '$expected' got '$got'"; fi
}

newrepo; commit "chore: init"
check "first release is 0.1.0" version 0.1.0
check "first release bump label" bump initial
check "first release, manual major" version 1.0.0 --bump major

tag v0.1.0; commit "fix: typo"
check "fix -> patch" version 0.1.1
check "fix -> patch (bump)" bump patch

commit "feat(core): add thing"
check "feat -> minor (highest wins over fix)" version 0.2.0
check "tag name" tag v0.2.0

commit "docs: words"
check "manual major overrides" version 1.0.0 --bump major
check "manual minor overrides" version 0.2.0 --bump minor
check "manual patch overrides" version 0.1.1 --bump patch

newrepo; commit "feat: a"; tag v1.4.2; commit "refactor!: drop old api"
check "bang -> major" version 2.0.0
newrepo; commit "feat: a"; tag v1.4.2; commit "fix: x" "BREAKING CHANGE: changes the save format"
check "BREAKING CHANGE footer -> major" version 2.0.0
newrepo; commit "feat: a"; tag v1.4.2; commit "Merge pull request #46 from owner/feat" "feat!: breaking change in PR body"
check "merge commit body with bang -> major" version 2.0.0
newrepo; commit "feat: a"; tag v1.4.2; commit "Merge pull request #45 from owner/feat" "feat: new feature in PR body"
check "merge commit body with feat -> minor" version 1.5.0
newrepo; commit "feat: a"; tag v1.4.2; commit "Merge pull request #44 from owner/fix" "fix: bugfix in PR body"
check "merge commit body with fix -> patch" version 1.4.3

newrepo; commit "feat: a"; tag v1.4.2; commit "update stuff"
check "free text -> patch" version 1.4.3

newrepo; commit "feat: a"; tag v1.4.2
check "nothing new -> skip" skip true
check "nothing new -> no version" version ""

newrepo; commit "feat: a"; tag v1.4.2; commit "feat: b [skip release]"
check "skip marker" skip true
check "skip marker ignored for manual bump" skip false --bump patch
check "manual bump after skip marker" version 1.4.3 --bump patch

newrepo; commit "feat: a"; tag v1.0.0; commit "fix: b"; tag v1.0.1-rc1; tag nightly; tag v2
check "other tags are ignored" previous_tag v1.0.0
check "other tags ignored: version" version 1.0.1

newrepo; commit "feat: a"; tag v1.9.9; commit "feat: b"; commit "chore: c"; commit "fix: d"
check "commit count" commits 3

newrepo
commit "fix: base"
tag v2.0.1
git -C "$repo" checkout -q -b release-tag
commit "feat: published on another branch"
tag v2.1.0
git -C "$repo" checkout -q master 2>/dev/null || git -C "$repo" checkout -q main
commit "feat: main branch change"
check "existing divergent tag is not reused" version 2.2.0

newrepo
commit "feat: base"
git -C "$repo" checkout -q --orphan orphan-branch
commit "feat: unmerged tag"
tag v0.1.0
git -C "$repo" checkout -q master 2>/dev/null || git -C "$repo" checkout -q main
commit "feat: commit on main"
check "existing initial tag advances cleanly" version 0.2.0
check "existing initial tag preserves initial bump" bump initial

echo "next_version tests: $pass passed, $fail failed"
[ "$fail" -eq 0 ]
