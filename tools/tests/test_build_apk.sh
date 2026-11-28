#!/usr/bin/env bash
# Tests for tools/build_apk.sh with stand-in SDK tools (the real aapt2, zipalign and apksigner are not needed):
# the version arithmetic, the checks on the input, and the order and options of the steps. Whether the .apk
# really installs is what the Android workflow checks with the real tools (.github/workflows/android.yml).
#   bash tools/tests/test_build_apk.sh
set -uo pipefail
HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
SCRIPT="$HERE/../build_apk.sh"
TMP="$(mktemp -d)"
trap 'rm -rf "$TMP"' EXIT

pass=0
fail=0
ok() { pass=$((pass + 1)); }
bad() {
  fail=$((fail + 1))
  echo "FAIL: $*"
}
expect_eq() { # name expected got
  if [ "$2" = "$3" ]; then ok; else bad "$1: expected '$2' got '$3'"; fi
}
expect_file() { # name path
  if [ -f "$2" ]; then ok; else bad "$1: $2 does not exist"; fi
}
expect_nofile() { # name path
  if [ ! -e "$2" ]; then ok; else bad "$1: $2 exists"; fi
}
expect_has() { # name needle haystack
  if [[ "$3" == *"$2"* ]]; then ok; else bad "$1: '$2' not found in: $3"; fi
}

# ---- versionCode --------------------------------------------------------------------------------------
vc() { bash "$SCRIPT" --version-code "$1" 2>&1; }
expect_eq "1.2.3" 1002003 "$(vc 1.2.3)"
expect_eq "0.1.0" 1000 "$(vc 0.1.0)"
expect_eq "10.20.30" 10020030 "$(vc 10.20.30)"
expect_eq "leading zeros are not octal" 8009 "$(vc 0.8.09)"
expect_eq "development build: the numbers decide" 1002003 "$(vc 1.2.3-dev+abc)"
expect_eq "0.0.0-dev is 1 (Android dislikes 0)" 1 "$(vc 0.0.0-dev+v0.3.1-4-gabc-dirty)"
expect_has "not a version" "not a version" "$(vc latest)"
expect_has "minor too big" "too big" "$(vc 1.1000.0)"
expect_has "patch too big" "too big" "$(vc 1.0.1000)"
expect_has "major too big (a versionCode is a signed 32 bit number)" "too big" "$(vc 2000.0.0)"
expect_eq "the biggest version that fits" 1999999999 "$(vc 1999.999.999)"
# order: a newer version always has a bigger code
prev=0
order_ok=yes
for v in 0.1.0 0.1.1 0.2.0 0.10.0 0.999.999 1.0.0 1.0.1 1.1.0 2.0.0; do
  c="$(vc "$v")"
  [ "$c" -gt "$prev" ] || order_ok="no ($v)"
  prev="$c"
done
expect_eq "codes grow with the version" yes "$order_ok"

# ---- stand-in tools -----------------------------------------------------------------------------------
BIN="$TMP/bin"
LOG="$TMP/calls.log"
mkdir -p "$BIN"
# every stand-in writes its command line to the log; the ones that make a file make a (valid, empty) zip
cat >"$BIN/aapt2" <<'EOF'
#!/usr/bin/env bash
echo "aapt2 $*" >>"$FAKE_LOG"
case "$1" in
compile) while [ $# -gt 0 ]; do [ "$1" = "-o" ] && python3 -c "import zipfile,sys; zipfile.ZipFile(sys.argv[1],'w').close()" "$2"; shift; done ;;
link) while [ $# -gt 0 ]; do [ "$1" = "-o" ] && python3 -c "import zipfile,sys; z=zipfile.ZipFile(sys.argv[1],'w'); z.writestr('AndroidManifest.xml','x'); z.close()" "$2"; shift; done ;;
esac
EOF
cat >"$BIN/zipalign" <<'EOF'
#!/usr/bin/env bash
echo "zipalign $*" >>"$FAKE_LOG"
[ "${FAKE_ZIPALIGN_FAILS:-}" = 1 ] && [ "$1" = "-f" ] && { echo "Unknown option 'P'" >&2; exit 2; }
[ "$1" = "-f" ] && cp "${@: -2:1}" "${@: -1}"
exit 0
EOF
cat >"$BIN/apksigner" <<'EOF'
#!/usr/bin/env bash
echo "apksigner $*" >>"$FAKE_LOG"
if [ "$1" = "sign" ]; then
  out=""; in=""
  while [ $# -gt 0 ]; do [ "$1" = "--out" ] && out="$2"; in="$1"; shift; done
  cp "$in" "$out"
fi
exit 0
EOF
cat >"$BIN/keytool" <<'EOF'
#!/usr/bin/env bash
echo "keytool $*" >>"$FAKE_LOG"
while [ $# -gt 0 ]; do [ "$1" = "-keystore" ] && : >"$2"; shift; done
exit 0
EOF
chmod +x "$BIN"/*
export FAKE_LOG="$LOG"
export AAPT2="$BIN/aapt2" ZIPALIGN="$BIN/zipalign" APKSIGNER="$BIN/apksigner" ANDROID_JAR="$TMP/android.jar"
: >"$ANDROID_JAR"
PATH="$BIN:$PATH"

# a library that looks like an arm64 shared object (the script only checks the header), and one that does not
python3 - "$TMP" <<'EOF'
import sys
d = sys.argv[1]
hdr = bytearray(64)
hdr[0:4] = b"\x7fELF"
hdr[4] = 2  # 64-bit
hdr[18:20] = (183).to_bytes(2, "little")  # AArch64
open(d + "/libarm64.so", "wb").write(bytes(hdr) + b"payload")
hdr[18:20] = (62).to_bytes(2, "little")  # x86-64
open(d + "/libx64.so", "wb").write(bytes(hdr))
open(d + "/notelf.so", "wb").write(b"hello world, not a library")
EOF

run() { bash "$SCRIPT" "$@" 2>&1; }

# ---- the checks on the input --------------------------------------------------------------------------
expect_has "missing options" "required" "$(run --lib "$TMP/libarm64.so")"
expect_has "unknown option" "unknown option" "$(run --nope)"
expect_has "no library" "library not found" "$(run --lib "$TMP/none.so" --version 1.0.0 --out "$TMP/o.apk")"
expect_has "x86 library refused" "not built for arm64" "$(run --lib "$TMP/libx64.so" --version 1.0.0 --out "$TMP/o.apk")"
expect_has "not an ELF" "not an ELF" "$(run --lib "$TMP/notelf.so" --version 1.0.0 --out "$TMP/o.apk")"
expect_has "bad version" "not a version" "$(run --lib "$TMP/libarm64.so" --version v1 --out "$TMP/o.apk")"
expect_has "no keystore file" "keystore not found" "$(run --lib "$TMP/libarm64.so" --version 1.0.0 --out "$TMP/o.apk" --keystore "$TMP/none.jks")"
: >"$TMP/release.jks"
expect_has "keystore needs its password" "ARCADE_KEYSTORE_PASSWORD" \
  "$(env -u ARCADE_KEYSTORE_PASSWORD bash "$SCRIPT" --lib "$TMP/libarm64.so" --version 1.0.0 --out "$TMP/o.apk" --keystore "$TMP/release.jks" 2>&1)"
expect_nofile "no .apk when the input is refused" "$TMP/o.apk"

# ---- throwaway key (no --keystore) --------------------------------------------------------------------
: >"$LOG"
out="$(run --lib "$TMP/libarm64.so" --version 1.2.3 --out "$TMP/dist/test.apk")"
calls="$(cat "$LOG")"
expect_file "throwaway .apk written" "$TMP/dist/test.apk"
expect_has "throwaway is announced" "THROWAWAY" "$out"
expect_has "versionCode passed to aapt2" "--version-code 1002003" "$calls"
expect_has "versionName passed to aapt2" "--version-name 1.2.3" "$calls"
expect_has "minSdk" "--min-sdk-version 26" "$calls"
expect_has "targetSdk" "--target-sdk-version 35" "$calls"
expect_has "manifest from android/" "android/AndroidManifest.xml" "$calls"
expect_has "resources compiled from android/res" "compile --dir" "$calls"
expect_has "16 KB alignment" "zipalign -f -P 16 4" "$calls"
expect_has "alignment verified" "zipalign -c -P 16 4" "$calls"
expect_has "no v4 signature file" "--v4-signing-enabled false" "$calls"
expect_has "throwaway key made" "keytool -genkeypair" "$calls"
expect_has "signature verified" "apksigner verify" "$calls"
# order: link, then zipalign, then sign, then verify (signing must be the last change to the file)
order="$(grep -oE '^(aapt2 link|zipalign -f|apksigner sign|apksigner verify|zipalign -c)' "$LOG" | tr '\n' ',')"
expect_eq "step order" "aapt2 link,zipalign -f,apksigner sign,apksigner verify,zipalign -c," "$order"
# the library is stored (not compressed) under lib/arm64-v8a/: the stand-in aapt2 made the base .apk, zip added it
mkdir -p "$TMP/inspect"
python3 - "$TMP" <<'EOF'
import sys, zipfile
z = zipfile.ZipFile(sys.argv[1] + "/dist/test.apk")
info = z.getinfo("lib/arm64-v8a/libarcade.so")
print("stored" if info.compress_type == zipfile.ZIP_STORED else "compressed", file=open(sys.argv[1] + "/inspect/mode", "w"))
EOF
expect_eq "library stored, not compressed" stored "$(cat "$TMP/inspect/mode")"

# ---- release key ---------------------------------------------------------------------------------------
: >"$LOG"
secret="hunter2-do-not-print"
out="$(ARCADE_KEYSTORE_PASSWORD="$secret" ARCADE_KEY_ALIAS=release ARCADE_KEY_PASSWORD="$secret" \
  run --lib "$TMP/libarm64.so" --version 2.0.0 --out "$TMP/dist/release.apk" --keystore "$TMP/release.jks")"
calls="$(cat "$LOG")"
expect_file "release .apk written" "$TMP/dist/release.apk"
expect_has "release key used" "--ks $TMP/release.jks" "$calls"
expect_has "alias from the environment" "--ks-key-alias release" "$calls"
expect_has "password read from the environment" "--ks-pass env:ARCADE_KEYSTORE_PASSWORD" "$calls"
expect_has "key password read from the environment" "--key-pass env:ARCADE_KEY_PASSWORD" "$calls"
case "$out$calls" in *"$secret"*) bad "the password appears in the output or in a command line" ;; *) ok ;; esac
case "$calls" in *keytool*) bad "release: a throwaway key was made" ;; *) ok ;; esac
case "$out" in *THROWAWAY*) bad "release: announced as throwaway" ;; *) ok ;; esac
expect_has "versionCode of 2.0.0" "--version-code 2000000" "$calls"

# ---- an old zipalign without -P: a clear message, no .apk ------------------------------------------------
rm -f "$TMP/dist/old.apk"
out="$(FAKE_ZIPALIGN_FAILS=1 run --lib "$TMP/libarm64.so" --version 1.0.0 --out "$TMP/dist/old.apk")"
expect_has "old zipalign explained" "build-tools 35.0.0" "$out"
expect_nofile "no .apk when zipalign fails" "$TMP/dist/old.apk"

# ---- the files in android/ ----------------------------------------------------------------------------
# aapt2 is the real judge (CI), but a typo in a name or a broken XML file can be caught here already.
ROOT="$(cd "$HERE/../.." && pwd)"
problems="$(python3 -I - "$ROOT" <<'PY'
import os, re, sys
import xml.etree.ElementTree as ET

root = sys.argv[1]
android = os.path.join(root, "android")
problems = []
xml_files = [os.path.join(android, "AndroidManifest.xml")]
for d, _, files in os.walk(os.path.join(android, "res")):
    xml_files += [os.path.join(d, f) for f in files if f.endswith(".xml")]
texts = {}
for f in xml_files:
    try:
        ET.parse(f)
        texts[f] = open(f, encoding="utf-8").read()
    except Exception as e:  # not well-formed
        problems.append("%s: %s" % (os.path.relpath(f, root), e))

# every @type/name used must exist as a file of res/<type>*/ or as a <string>/<style> entry
defined = set()
res = os.path.join(android, "res")
for d, _, files in os.walk(res):
    kind = os.path.basename(d).split("-")[0]
    for f in files:
        if f.endswith(".xml") and kind == "values":
            for m in re.finditer(r'<(string|style|color)\s+name="([^"]+)"', open(os.path.join(d, f), encoding="utf-8").read()):
                defined.add((m.group(1), m.group(2)))
        else:
            defined.add((kind, os.path.splitext(f)[0]))
for f, text in texts.items():
    text = re.sub(r"<!--.*?-->", "", text, flags=re.S)
    for m in re.finditer(r'@(drawable|mipmap|string|style|color)/([A-Za-z0-9_.]+)', text):
        if (m.group(1), m.group(2)) not in defined:
            problems.append("%s: @%s/%s is not defined" % (os.path.relpath(f, root), m.group(1), m.group(2)))

# the library the activity loads is the one CMake builds
manifest = texts.get(os.path.join(android, "AndroidManifest.xml"), "")
m = re.search(r'android:name="android.app.lib_name"\s+android:value="([^"]+)"', manifest)
cmake = open(os.path.join(root, "app", "CMakeLists.txt"), encoding="utf-8").read()
c = re.search(r"OUTPUT_NAME\s+(\w+)", cmake)
if not m or not c or m.group(1) != c.group(1):
    problems.append("lib_name in the manifest (%s) is not the OUTPUT_NAME in app/CMakeLists.txt (%s)" % (m and m.group(1), c and c.group(1)))
if 'android:hasCode="false"' not in manifest:
    problems.append("the manifest must say hasCode=false (there is no Java code)")
if "uses-permission" in re.sub(r"<!--.*?-->", "", manifest, flags=re.S):
    problems.append("the manifest asks for a permission: the Online leaderboard is off, nothing needs one yet")

# the icon layers: PNG, 432 x 432 (108 dp at xxxhdpi)
for name in ("ic_launcher_background", "ic_launcher_foreground"):
    path = os.path.join(res, "drawable-nodpi", name + ".png")
    try:
        with open(path, "rb") as fh:
            head = fh.read(24)
        w, h = int.from_bytes(head[16:20], "big"), int.from_bytes(head[20:24], "big")
        if head[:8] != b"\x89PNG\r\n\x1a\n" or (w, h) != (432, 432):
            problems.append("%s is not a 432 x 432 PNG" % name)
    except OSError:
        problems.append("%s.png is missing" % name)
print("\n".join(problems))
PY
)"
if [ -z "$problems" ]; then ok; else bad "android/ files: $problems"; fi

echo "build_apk tests: $pass passed, $fail failed"
[ "$fail" -eq 0 ]
