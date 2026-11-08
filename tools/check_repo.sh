#!/usr/bin/env bash
# Repository structure and portability rules. Run by CI; run it yourself before pushing:
#   bash tools/check_repo.sh
set -uo pipefail
cd "$(dirname "${BASH_SOURCE[0]}")/.."
errors=0
err() { echo "ERROR: $*"; errors=$((errors + 1)); }
# Prints "line:code" for a file with comments removed (so words inside comments never trigger a rule).
code_lines() { sed -e 's://.*$::' -e 's:/\*.*\*/::g' "$1" | grep -nvE '^[[:space:]]*(\*|/\*)' || true; }

# ---- 1. every game folder follows the contract -------------------------------------------------
games=0
for dir in games/*/; do
  id="$(basename "$dir")"
  case "$id" in _*) continue ;; esac
  games=$((games + 1))
  for f in CMakeLists.txt AGENTS.md GAME.md "${id}_logic.c" "${id}_logic.h" "${id}_game.c"; do
    [ -f "${dir}${f}" ] || err "games/$id: missing $f"
  done
  ls "${dir}"tests/*.c >/dev/null 2>&1 || err "games/$id: needs at least one tests/*.c"
  if [ -f "${dir}${id}_game.c" ]; then
    grep -q "game_${id}_def" "${dir}${id}_game.c" || err "games/$id/${id}_game.c must define 'const GameDef game_${id}_def'"
    grep -q "arcade_loop_begin(" "${dir}${id}_game.c" || err "games/$id/${id}_game.c must call arcade_loop_begin(ctx)"
    grep -q "arcade_frame_end(" "${dir}${id}_game.c" || err "games/$id/${id}_game.c must end frames with arcade_frame_end()"
    grep -qE "BeginDrawing\(\)" "${dir}${id}_game.c" || err "games/$id/${id}_game.c never calls BeginDrawing()"
    if code_lines "${dir}${id}_game.c" | grep -E '\bEndDrawing\(' >/dev/null; then
      err "games/$id/${id}_game.c calls EndDrawing(): use arcade_frame_end() instead"
    fi
  fi
  if [ -f "${dir}CMakeLists.txt" ]; then
    grep -qE "ID +${id}( |$)" "${dir}CMakeLists.txt" || err "games/$id/CMakeLists.txt: arcade_add_game(ID ${id} ...) must match the folder name"
  fi
done
[ "$games" -gt 0 ] || err "no games found in games/"
[ -d games/_template ] || err "games/_template is missing (needed by tools/new_game.cmake)"

# ---- 2. pure code must not touch raylib or the OS -----------------------------------------------
mapfile -t pure < <(find core/src core/include games tests -type f \( -name '*.c' -o -name '*.h' \) \
  ! -path 'games/_template/*' ! -name '*_game.c' ! -name 'gfx.h' | sort)
for f in "${pure[@]}"; do
  if grep -nE '#[[:space:]]*include[[:space:]]*[<"](raylib|rlgl|raymath|arcade/gfx)\.h[>"]' "$f" >/dev/null; then
    err "$f: pure code must not include raylib/gfx (only *_game.c, core/gfx and app may)"
  fi
done
mapfile -t logic < <(find games -type f -name '*_logic.[ch]' ! -path 'games/_template/*' | sort)
for f in "${logic[@]}"; do
  if code_lines "$f" | grep -E '\b(rand|srand|time|clock)[[:space:]]*\(' >/dev/null; then
    err "$f: game logic must be deterministic: use arcade/rng.h instead of rand()/time()"
  fi
done

# ---- 3. portability: things that only exist on one OS -------------------------------------------
mapfile -t all < <(find core app games tests -type f \( -name '*.c' -o -name '*.h' \) ! -path 'games/_template/*' | sort)
for f in "${all[@]}"; do
  if grep -nE '#[[:space:]]*include[[:space:]]*[<"](conio|windows|unistd|sys/[a-z_]+|dirent|pthread|curses|ncurses)\.h[>"]' "$f" >/dev/null; then
    err "$f: platform-specific header (conio.h, windows.h, unistd.h, sys/*, ...). Use standard C or raylib"
  fi
  if code_lines "$f" | grep -E '\bsystem[[:space:]]*\(' >/dev/null; then
    err "$f: system() is not portable (and not allowed)"
  fi
  if grep -nE '(^|[^a-zA-Z_])(std::|#include[[:space:]]*<(iostream|vector|string)>)' "$f" >/dev/null; then
    err "$f: C++ code found. This project is plain C99"
  fi
done
if find core app games tests -type f \( -name '*.cpp' -o -name '*.cc' -o -name '*.hpp' -o -name '*.cxx' \) | grep -q .; then
  err "C++ files found: this project is plain C99"
fi

# ---- 4. agent instructions -----------------------------------------------------------------------
for f in AGENTS.md core/AGENTS.md games/AGENTS.md .github/AGENTS.md; do
  [ -f "$f" ] || err "missing $f"
done
if [ -f AGENTS.md ]; then
  lines="$(wc -l < AGENTS.md)"
  [ "$lines" -lt 150 ] || err "AGENTS.md has $lines lines: keep the root file under 150 (move details into scoped files)"
fi

if [ "$errors" -gt 0 ]; then
  echo "check_repo: $errors problem(s)"
  exit 1
fi
echo "check_repo: OK ($games games)"
