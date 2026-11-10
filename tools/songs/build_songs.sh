#!/usr/bin/env bash
# Converts every MIDI listed in tools/songs/songs.list into a Beat Dodge song in assets/beatmaps/fnf/
# and makes sure each song is listed in assets/beatmaps/songs.txt.
#
#   bash tools/songs/build_songs.sh [path/to/midi2beatmap]
#
# Build the converter first:  cmake --preset logic && cmake --build --preset logic
set -euo pipefail
cd "$(dirname "${BASH_SOURCE[0]}")/../.."
TOOL="${1:-}"
if [ -z "$TOOL" ]; then
  for c in build/logic/bin/midi2beatmap build/dev/bin/midi2beatmap build/release/bin/midi2beatmap \
           build/logic/bin/midi2beatmap.exe build/dev/bin/midi2beatmap.exe; do
    if [ -x "$c" ]; then TOOL="$c"; break; fi
  done
fi
if [ -z "$TOOL" ] || [ ! -x "$TOOL" ]; then
  echo "midi2beatmap not found. Build it first: cmake --preset logic && cmake --build --preset logic" >&2
  exit 1
fi
OUT=assets/beatmaps/fnf
INDEX=assets/beatmaps/songs.txt
mkdir -p "$OUT"
touch "$INDEX"
while IFS='|' read -r slug title midi; do
  case "$slug" in ''|\#*) continue ;; esac
  "$TOOL" "tools/songs/$midi" -o "$OUT/$slug.txt" --title "$title"
  grep -qx "fnf/$slug.txt" "$INDEX" || echo "fnf/$slug.txt" >> "$INDEX"
done < tools/songs/songs.list
echo "done: $(grep -c '^fnf/' "$INDEX") FNF songs listed in $INDEX"
