#!/usr/bin/env bash
# Formats all C sources with clang-format using the repo's .clang-format.
#   tools/format.sh           format in place
#   tools/format.sh --check   fail if anything would change (this is what CI runs)
# CI pins clang-format 18.1.8:  pip install clang-format==18.1.8
# (VS Code's bundled formatter may differ slightly: prefer this script over "format on save".)
set -euo pipefail
cd "$(dirname "${BASH_SOURCE[0]}")/.."
if ! command -v clang-format >/dev/null 2>&1; then
  echo "clang-format not found. Install the pinned version:  pip install clang-format==18.1.8" >&2
  exit 2
fi
version="$(clang-format --version | grep -oE '[0-9]+\.[0-9]+\.[0-9]+' | head -n 1)"
[ "$version" = "18.1.8" ] || echo "note: clang-format $version differs from CI's 18.1.8; results may differ slightly" >&2
mapfile -t files < <(find core app games tests -type f \( -name '*.c' -o -name '*.h' \) ! -path 'games/_template/*' | sort)
if [ "${1:-}" = "--check" ]; then
  clang-format --dry-run --Werror "${files[@]}"
  echo "format: OK (${#files[@]} files)"
else
  clang-format -i "${files[@]}"
  echo "format: formatted ${#files[@]} files"
fi
