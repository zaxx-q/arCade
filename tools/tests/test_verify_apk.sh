#!/usr/bin/env bash
# Tests for tools/verify_apk.sh with stand-in tools whose output is what the real ones print (aapt2 dump badging,
# apksigner verify, zipalign -c, readelf). One good .apk must pass, and each check must be able to fail on its own.
#   bash tools/tests/test_verify_apk.sh
set -uo pipefail
HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
SCRIPT="$HERE/../verify_apk.sh"
TMP="$(mktemp -d)"
trap 'rm -rf "$TMP"' EXIT

pass=0
fail=0
ok() { pass=$((pass + 1)); }
bad() {
  fail=$((fail + 1))
  echo "FAIL: $*"
}
expect_has() { # name needle haystack
  if [[ "$3" == *"$2"* ]]; then ok; else bad "$1: '$2' not found in: $3"; fi
}

BIN="$TMP/bin"
mkdir -p "$BIN"
# The stand-ins print the contents of files that each case writes (FAKE_*), so a case changes one thing at a time.
cat >"$BIN/aapt2" <<'EOF'
#!/usr/bin/env bash
cat "$FAKE_BADGING"
EOF
cat >"$BIN/apksigner" <<'EOF'
#!/usr/bin/env bash
cat "$FAKE_SIG"
[ "${FAKE_SIG_FAILS:-}" = 1 ] && exit 1
exit 0
EOF
cat >"$BIN/zipalign" <<'EOF'
#!/usr/bin/env bash
cat "$FAKE_ALIGN"
[ "${FAKE_ALIGN_FAILS:-}" = 1 ] && exit 1
exit 0
EOF
cat >"$BIN/readelf" <<'EOF'
#!/usr/bin/env bash
case "$1" in
-lW) cat "$FAKE_LOADS" ;;
-dW) cat "$FAKE_DYNAMIC" ;;
esac
EOF
chmod +x "$BIN"/*
export AAPT2="$BIN/aapt2" APKSIGNER="$BIN/apksigner" ZIPALIGN="$BIN/zipalign" READELF="$BIN/readelf"

make_apk() { # path [lib method] [extra entry]: a zip with the entries the script looks at
  python3 - "$@" <<'PY'
import sys, zipfile
path = sys.argv[1]
method = zipfile.ZIP_DEFLATED if (len(sys.argv) > 2 and sys.argv[2] == "deflate") else zipfile.ZIP_STORED
extra = sys.argv[3] if len(sys.argv) > 3 else None
with zipfile.ZipFile(path, "w") as z:
    z.writestr("AndroidManifest.xml", "x")
    z.writestr("resources.arsc", "y")
    z.writestr("lib/arm64-v8a/libarcade.so", "z" * 100, compress_type=method)
    if extra:
        z.writestr(extra, "e")
PY
}

CERT="AB:CD:EF:01:23:45:67:89:AB:CD:EF:01:23:45:67:89:AB:CD:EF:01:23:45:67:89:AB:CD:EF:01:23:45:67:89"
CERT_PLAIN="abcdef0123456789abcdef0123456789abcdef0123456789abcdef0123456789"
good_files() {
  cat >"$TMP/badging" <<EOF
package: name='dev.arcade.hub' versionCode='1002003' versionName='1.2.3' platformBuildVersionName='15' compileSdkVersion='35'
sdkVersion:'26'
targetSdkVersion:'35'
uses-feature: name='android.hardware.touchscreen'
uses-gl-es: '0x20000'
application-label:'arCade'
launchable-activity: name='android.app.NativeActivity'  label='' icon=''
native-code: 'arm64-v8a'
EOF
  cat >"$TMP/sig" <<EOF
Verifies
Verified using v1 scheme (JAR signing): false
Verified using v2 scheme (APK Signature Scheme v2): true
Verified using v3 scheme (APK Signature Scheme v3): true
Number of signers: 1
Signer #1 certificate DN: CN=arCade
Signer #1 certificate SHA-256 digest: $CERT_PLAIN
EOF
  cat >"$TMP/align" <<'EOF'
Verifying alignment of x.apk (4)...
      50 AndroidManifest.xml (OK - compressed)
    4096 lib/arm64-v8a/libarcade.so (OK)
Verification successful
EOF
  cat >"$TMP/loads" <<'EOF'
Program Headers:
  Type           Offset   VirtAddr           PhysAddr           FileSiz  MemSiz   Flg Align
  PHDR           0x000040 0x0000000000000040 0x0000000000000040 0x000230 0x000230 R   0x8
  LOAD           0x000000 0x0000000000000000 0x0000000000000000 0x1a2b30 0x1a2b30 R   0x4000
  LOAD           0x1a2b30 0x00000000001a6b30 0x00000000001a6b30 0x0c0000 0x0c0000 R E 0x4000
  LOAD           0x2b0000 0x00000000002b4000 0x00000000002b4000 0x010000 0x018000 RW  0x4000
EOF
  cat >"$TMP/dynamic" <<'EOF'
Dynamic section at offset 0x2b0000 contains 30 entries:
  Tag        Type                         Name/Value
 0x0000000000000001 (NEEDED)             Shared library: [liblog.so]
 0x0000000000000001 (NEEDED)             Shared library: [libandroid.so]
 0x0000000000000001 (NEEDED)             Shared library: [libEGL.so]
 0x0000000000000001 (NEEDED)             Shared library: [libGLESv2.so]
 0x0000000000000001 (NEEDED)             Shared library: [libm.so]
 0x0000000000000001 (NEEDED)             Shared library: [libdl.so]
 0x0000000000000001 (NEEDED)             Shared library: [libc.so]
EOF
  export FAKE_BADGING="$TMP/badging" FAKE_SIG="$TMP/sig" FAKE_ALIGN="$TMP/align" FAKE_LOADS="$TMP/loads" FAKE_DYNAMIC="$TMP/dynamic"
  unset FAKE_SIG_FAILS FAKE_ALIGN_FAILS
  make_apk "$TMP/test.apk"
}
run() { bash "$SCRIPT" --apk "$TMP/test.apk" --version 1.2.3 "$@" 2>&1; }

# ---- the good one --------------------------------------------------------------------------------------
good_files
out="$(run)"
rc=$?
if [ "$rc" -eq 0 ]; then ok; else bad "good .apk refused (exit $rc): $out"; fi
expect_has "good: 0 problems" "0 problem(s)" "$out"
expect_has "good: certificate printed" "certificate SHA-256: $CERT_PLAIN" "$out"
out="$(run --cert-sha256 "$CERT")"
rc=$?
if [ "$rc" -eq 0 ]; then ok; else bad "the expected certificate (with colons, upper case) was refused: $out"; fi
expect_has "expected key" "signed with the expected key" "$out"

# ---- each check on its own -----------------------------------------------------------------------------
case_fails() { # name expected-message [args for run]
  local name="$1" msg="$2" out rc
  shift 2
  out="$(run "$@")"
  rc=$?
  if [ "$rc" -eq 1 ]; then ok; else bad "$name: expected exit 1, got $rc"; fi
  expect_has "$name" "$msg" "$out"
  good_files
}

sed -i "s/versionCode='1002003'/versionCode='1002004'/" "$TMP/badging"
case_fails "wrong versionCode" "FAIL: package dev.arcade.hub"
sed -i "s/name='dev.arcade.hub'/name='com.example.other'/" "$TMP/badging"
case_fails "wrong package" "FAIL: package dev.arcade.hub"
sed -i "s/targetSdkVersion:'35'/targetSdkVersion:'36'/" "$TMP/badging"
case_fails "wrong targetSdk" "FAIL: targetSdk 35"
sed -i "s/native-code: 'arm64-v8a'/native-code: 'arm64-v8a' 'x86_64'/" "$TMP/badging"
case_fails "second ABI" "FAIL: native code for arm64-v8a only"
echo "uses-permission: name='android.permission.INTERNET'" >>"$TMP/badging"
case_fails "a permission" "FAIL: the app asks for a permission"
echo "application-debuggable" >>"$TMP/badging"
case_fails "debuggable" "FAIL: the app is debuggable"
sed -i "s/(APK Signature Scheme v2): true/(APK Signature Scheme v2): false/" "$TMP/sig"
case_fails "no v2 signature" "FAIL: apksigner verify did not confirm"
FAKE_SIG_FAILS=1 case_fails "apksigner fails" "FAIL: apksigner verify did not confirm"
case_fails "another key" "FAIL: signed with a different key" --cert-sha256 "$(printf '1%.0s' {1..64})"
echo "ERROR: lib/arm64-v8a/libarcade.so offset 123 is not aligned" >"$TMP/align"
case_fails "unaligned" "FAIL: zipalign"
FAKE_ALIGN_FAILS=1 case_fails "zipalign fails" "FAIL: zipalign"
make_apk "$TMP/test.apk" deflate
case_fails "compressed library" "FAIL: lib/arm64-v8a/libarcade.so is compressed"
make_apk "$TMP/test.apk" store lib/x86_64/libarcade.so
case_fails "a second ABI in the zip" "FAIL: unexpected files under lib/"
sed -i 's/0x4000$/0x1000/' "$TMP/loads"
case_fails "4 KB segments" "FAIL: LOAD segment alignment below 16 KB"
echo " 0x0000000000000001 (NEEDED)             Shared library: [libcurl.so]" >>"$TMP/dynamic"
case_fails "a library Android does not have" "FAIL: needs libraries that are not in the allowed list: libcurl.so"
: >"$TMP/dynamic"
case_fails "the tool could not read the library" "FAIL: the library lists no needed libraries"

# ---- more than one problem: all are reported ---------------------------------------------------------------
echo "uses-permission: name='android.permission.INTERNET'" >>"$TMP/badging"
sed -i 's/0x4000$/0x1000/' "$TMP/loads"
out="$(run)"
expect_has "all problems reported (permission)" "FAIL: the app asks for a permission" "$out"
expect_has "all problems reported (alignment)" "FAIL: LOAD segment alignment" "$out"
expect_has "problems counted" "2 problem(s)" "$out"

echo "verify_apk tests: $pass passed, $fail failed"
[ "$fail" -eq 0 ]
