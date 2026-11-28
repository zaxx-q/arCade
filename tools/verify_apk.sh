#!/usr/bin/env bash
# Checks a finished arCade .apk with the real Android tools, independently of how tools/build_apk.sh made it.
# Every check that fails is reported (not only the first), then the script exits with 1.
#
#   tools/verify_apk.sh --apk dist/arCade-v1.2.3-android-arm64.apk --version 1.2.3 [--cert-sha256 HEX]
#
# What it looks at:
#   - aapt2 dump badging: package name, versionName/versionCode, minSdk 26, targetSdk 35, arm64-v8a only, the
#     NativeActivity as launcher, OpenGL ES 2, no permission asked for, not debuggable
#   - apksigner verify: the signature is valid (scheme v2); --cert-sha256 makes the certificate a known one (the
#     release key: a wrong key would make every later release impossible to install over this one)
#   - zipalign -c -P 16: the stored libraries are aligned for 16 KB pages; resources.arsc and the library are stored
#     (not compressed), and the library is the only file under lib/
#   - the library itself: every LOAD segment aligned to 16 KB (the phones of Android 15+ with 16 KB pages refuse it
#     otherwise), and it needs no shared library that Android does not provide (a missing one is a crash at launch)
# Tools: like tools/build_apk.sh (ANDROID_HOME, BUILD_TOOLS_VERSION, or AAPT2/APKSIGNER/ZIPALIGN), plus READELF
# (default: readelf; the NDK's llvm-readelf also works) and python3.
set -uo pipefail

PACKAGE=io.github.zaxx_q.arcade
MIN_SDK=26
TARGET_SDK=35
ABI=arm64-v8a
# Libraries every Android phone has (the NDK's stable APIs) and that raylib, EGL/GLES and audio use.
ALLOWED_NEEDED="libc.so libm.so libdl.so liblog.so libandroid.so libEGL.so libGLESv2.so libGLESv3.so libOpenSLES.so libaaudio.so libz.so"

here="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
die() {
  echo "verify_apk: $*" >&2
  exit 2
}
# shellcheck source=tools/android_sdk.sh
. "$here/tools/android_sdk.sh"

failures=0
fail() {
  echo "verify_apk: FAIL: $*" >&2
  failures=$((failures + 1))
}
pass() { echo "verify_apk: ok: $*"; }

apk="" version="" want_cert=""
while [ $# -gt 0 ]; do
  case "$1" in
  --apk)
    apk="${2:-}"
    shift 2
    ;;
  --version)
    version="${2:-}"
    shift 2
    ;;
  --cert-sha256)
    want_cert="${2:-}"
    shift 2
    ;;
  -h | --help)
    sed -n '2,/^set -uo/p' "${BASH_SOURCE[0]}" | sed '$d' | sed 's/^# \{0,1\}//'
    exit 0
    ;;
  *) die "unknown option '$1' (see --help)" ;;
  esac
done
[ -n "$apk" ] && [ -n "$version" ] || die "--apk and --version are required (see --help)"
[ -f "$apk" ] || die "not found: $apk"
code="$(bash "$here/tools/build_apk.sh" --version-code "$version")" || die "bad version '$version'"

aapt2="${AAPT2:-$(find_tool aapt2)}" || die "aapt2 not found (set ANDROID_HOME)"
apksigner="${APKSIGNER:-$(find_tool apksigner)}" || die "apksigner not found (set ANDROID_HOME)"
zipalign="${ZIPALIGN:-$(find_tool zipalign)}" || die "zipalign not found (set ANDROID_HOME)"
readelf="${READELF:-readelf}"
command -v "$readelf" >/dev/null || die "$readelf not found (binutils, or set READELF to the NDK's llvm-readelf)"
work="$(mktemp -d)"
trap 'rm -rf "$work"' EXIT

# ---- manifest, as Android reads it -------------------------------------------------------------------------
badging="$("$aapt2" dump badging "$apk" 2>&1)" || {
  printf '%s\n' "$badging" >&2
  die "aapt2 cannot read $apk"
}
has() { printf '%s\n' "$badging" | grep -Fq -- "$1"; }
check_badging() { # text what
  if has "$1"; then pass "$2"; else fail "$2 (expected '$1' in the output of aapt2 dump badging)"; fi
}
check_badging "package: name='$PACKAGE' versionCode='$code' versionName='$version'" "package $PACKAGE, version $version, code $code"
check_badging "sdkVersion:'$MIN_SDK'" "minSdk $MIN_SDK"
check_badging "targetSdkVersion:'$TARGET_SDK'" "targetSdk $TARGET_SDK"
# the whole line, so that a second ABI after the first would not pass
if printf '%s\n' "$badging" | grep -Fxq "native-code: '$ABI'"; then
  pass "native code for $ABI only"
else
  fail "native code for $ABI only (expected the line native-code: '$ABI' in the output of aapt2 dump badging)"
fi
check_badging "launchable-activity: name='android.app.NativeActivity'" "NativeActivity is the launcher"
check_badging "uses-gl-es: '0x20000'" "OpenGL ES 2.0"
if printf '%s\n' "$badging" | grep -Eq '^uses-permission'; then
  fail "the app asks for a permission (the Online leaderboard is off, nothing needs one): $(printf '%s\n' "$badging" | grep -E '^uses-permission')"
else
  pass "no permissions"
fi
if has "application-debuggable"; then fail "the app is debuggable"; else pass "not debuggable"; fi

# ---- signature -----------------------------------------------------------------------------------------------
sigout="$("$apksigner" verify --verbose --print-certs "$apk" 2>&1)"
sigrc=$?
if [ "$sigrc" -eq 0 ] && printf '%s\n' "$sigout" | grep -Fq "(APK Signature Scheme v2): true"; then
  pass "signature valid (scheme v2)"
else
  printf '%s\n' "$sigout" >&2
  fail "apksigner verify did not confirm a v2 signature (exit status $sigrc)"
fi
cert="$(printf '%s\n' "$sigout" | sed -n 's/.*certificate SHA-256 digest: *//p' | head -n 1 | tr -d ': ' | tr 'A-F' 'a-f')"
echo "verify_apk: certificate SHA-256: ${cert:-unknown}"
if [ -n "$want_cert" ]; then
  want="$(printf '%s' "$want_cert" | tr -d ': ' | tr 'A-F' 'a-f')"
  if [ "$cert" = "$want" ]; then
    pass "signed with the expected key"
  else
    fail "signed with a different key than expected (got ${cert:-none}, expected $want)"
  fi
fi

# ---- alignment and the files in the zip ---------------------------------------------------------------------
alignrc=0
alignout="$("$zipalign" -c -P 16 -v 4 "$apk" 2>&1)" || alignrc=$?
if [ "$alignrc" -eq 0 ] && printf '%s\n' "$alignout" | grep -Fq "Verification successful"; then
  pass "zipalign: 16 KB aligned"
else
  printf '%s\n' "$alignout" | tail -n 5 >&2
  fail "zipalign -c -P 16 did not report 'Verification successful'"
fi

lib_name="lib/$ABI/libarcade.so"
zipinfo="$(python3 -I - "$apk" "$lib_name" "$work/libarcade.so" <<'PY'
import sys, zipfile
apk, lib, out = sys.argv[1:4]
problems = []
with zipfile.ZipFile(apk) as z:
    names = z.namelist()
    for n in ("resources.arsc", lib):
        if n not in names:
            problems.append("%s is missing" % n)
        elif z.getinfo(n).compress_type != zipfile.ZIP_STORED:
            problems.append("%s is compressed (it must be stored)" % n)
    others = [n for n in names if n.startswith("lib/") and n != lib]
    if others:
        problems.append("unexpected files under lib/: %s" % ", ".join(others))
    if lib in names:
        with open(out, "wb") as f:
            f.write(z.read(lib))
print("\n".join(problems))
PY
)"
if [ -z "$zipinfo" ]; then
  pass "resources.arsc and $lib_name are stored, nothing else under lib/"
else
  while IFS= read -r line; do fail "$line"; done <<<"$zipinfo"
fi

# ---- the library -----------------------------------------------------------------------------------------------
if [ -f "$work/libarcade.so" ]; then
  loads="$("$readelf" -lW "$work/libarcade.so" 2>&1 | awk '$1 == "LOAD" { print $NF }')"
  if [ -z "$loads" ]; then
    fail "no LOAD segments found in the library (is $readelf able to read it?)"
  else
    bad=""
    for a in $loads; do
      if [[ ! "$a" =~ ^0x[0-9a-fA-F]+$ ]] || [ $((a)) -lt 16384 ]; then bad="$bad $a"; fi
    done
    if [ -z "$bad" ]; then pass "every LOAD segment is aligned to 16 KB or more ($(tr '\n' ' ' <<<"$loads"))"; else fail "LOAD segment alignment below 16 KB:$bad (link flags -z max-page-size=16384)"; fi
  fi
  needed="$("$readelf" -dW "$work/libarcade.so" 2>&1 | sed -n 's/.*Shared library: \[\(.*\)\].*/\1/p')"
  unknown=""
  for n in $needed; do
    case " $ALLOWED_NEEDED " in *" $n "*) ;; *) unknown="$unknown $n" ;; esac
  done
  if [ -z "$needed" ]; then
    fail "the library lists no needed libraries (is $readelf able to read it?)"
  elif [ -z "$unknown" ]; then
    pass "needs only libraries Android provides ($(tr '\n' ' ' <<<"$needed"))"
  else
    fail "needs libraries that are not in the allowed list:$unknown (they would be missing on the phone; if one is really provided by Android, add it to ALLOWED_NEEDED in tools/verify_apk.sh)"
  fi
fi

echo "verify_apk: $(wc -c <"$apk") bytes, $failures problem(s)"
[ "$failures" -eq 0 ]
