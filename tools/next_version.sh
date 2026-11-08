#!/usr/bin/env bash
# Computes the next release version from git tags and Conventional Commit messages.
#
#   tools/next_version.sh [--bump auto|patch|minor|major] [--repo DIR] [--ref REF]
#
# Prints key=value lines (append them to $GITHUB_OUTPUT in CI):
#   skip=true|false        true when there is nothing to release
#   reason=...             why it was skipped (empty when skip=false)
#   previous_tag=vA.B.C|none
#   bump=initial|patch|minor|major
#   version=X.Y.Z          (empty when skip=true)
#   tag=vX.Y.Z             (empty when skip=true)
#   commits=N              number of commits since the previous tag
#
# Rules (highest wins across all commits since the last v*.*.* tag):
#   "type!:" in the subject, or a "BREAKING CHANGE:" line  -> major
#   "feat:" / "feat(scope):"                              -> minor
#   anything else (fix, docs, chore, free text...)        -> patch
#   "[skip release]" in the newest commit                 -> no release (ignored for a manual --bump)
# The very first release is v0.1.0 (v1.0.0 if you ask for --bump major).
set -euo pipefail

BUMP="auto"
REPO="."
REF="HEAD"
while [ $# -gt 0 ]; do
  case "$1" in
    --bump) BUMP="${2:-}"; shift 2 ;;
    --repo) REPO="${2:-}"; shift 2 ;;
    --ref)  REF="${2:-}"; shift 2 ;;
    -h|--help) sed -n '2,22p' "$0"; exit 0 ;;
    *) echo "unknown argument: $1" >&2; exit 2 ;;
  esac
done
case "$BUMP" in auto|patch|minor|major) ;; *) echo "invalid --bump '$BUMP'" >&2; exit 2 ;; esac

git_() { git -C "$REPO" "$@"; }

# latest plain SemVer tag (v1.2.3 only; v1.2.3-rc1 and other tags are ignored) reachable from REF
prev_tag="$(git_ tag --list 'v[0-9]*.[0-9]*.[0-9]*' --merged "$REF" --sort=-v:refname | grep -E '^v[0-9]+\.[0-9]+\.[0-9]+$' | head -n 1 || true)"

if [ -n "$prev_tag" ]; then range="$prev_tag..$REF"; else range="$REF"; fi
commits="$(git_ rev-list --count "$range")"

emit() { # skip reason previous bump version
  echo "skip=$1"; echo "reason=$2"; echo "previous_tag=${3:-none}"; echo "bump=$4"; echo "version=$5"
  if [ -n "$5" ]; then echo "tag=v$5"; else echo "tag="; fi
  echo "commits=$commits"
}

if [ "$commits" -eq 0 ]; then
  emit true "no new commits since ${prev_tag:-the beginning}" "${prev_tag:-}" none ""; exit 0
fi

level="patch"
re_breaking_subject='^[a-zA-Z]+(\([^)]*\))?!:'
re_feat='^feat(\([^)]*\))?:'
re_breaking_footer='^BREAKING[ -]CHANGE:'
for c in $(git_ rev-list "$range"); do
  subject="$(git_ log -1 --format=%s "$c")"
  body="$(git_ log -1 --format=%b "$c")"
  if [[ "$subject" =~ $re_breaking_subject ]] || printf '%s\n' "$body" | grep -Eq "$re_breaking_footer"; then
    level="major"; break
  elif [[ "$subject" =~ $re_feat ]]; then
    level="minor"
  fi
done

if [ "$BUMP" = "auto" ]; then
  newest="$(git_ log -1 --format=%B "$REF")"
  if printf '%s' "$newest" | grep -Eqi '\[skip release\]'; then
    emit true "newest commit contains [skip release]" "${prev_tag:-}" none ""; exit 0
  fi
else
  level="$BUMP"
fi

if [ -z "$prev_tag" ]; then
  if [ "$level" = "major" ]; then version="1.0.0"; else version="0.1.0"; fi
  emit false "" "" initial "$version"; exit 0
fi

IFS=. read -r major minor patch <<<"${prev_tag#v}"
case "$level" in
  major) major=$((major + 1)); minor=0; patch=0 ;;
  minor) minor=$((minor + 1)); patch=0 ;;
  patch) patch=$((patch + 1)) ;;
esac
emit false "" "$prev_tag" "$level" "$major.$minor.$patch"
