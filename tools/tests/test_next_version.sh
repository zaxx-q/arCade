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

echo "next_version tests: $pass passed, $fail failed"
[ "$fail" -eq 0 ]
