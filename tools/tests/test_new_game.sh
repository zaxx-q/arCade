#!/usr/bin/env bash
# Proves that "add a game" works: scaffolds a throw-away game, checks repo rules, builds it and runs its tests.
#   bash tools/tests/test_new_game.sh
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
ID=zz_scaffold_check
BUILD="$ROOT/build/scaffold-check"
cleanup() { rm -rf "$ROOT/games/$ID" "$BUILD"; }
trap cleanup EXIT
cleanup

cmake -DID=$ID -DTITLE="Scaffold Check" -DOWNER=ci -P "$ROOT/tools/new_game.cmake"
bash "$ROOT/tools/check_repo.sh"
cmake -S "$ROOT" -B "$BUILD" -DARCADE_LOGIC_ONLY=ON -DARCADE_WERROR=ON -DCMAKE_BUILD_TYPE=Debug
cmake --build "$BUILD" --target "game_${ID}_logic" "gtest_${ID}_test_${ID}"
ctest --test-dir "$BUILD" -R "^${ID}\." --output-on-failure
echo "new_game test: OK"
