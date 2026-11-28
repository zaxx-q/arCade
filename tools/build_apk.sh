#!/usr/bin/env bash
# Packs the hub's shared library (libarcade.so, built with the NDK) into a signed .apk.
# No Gradle and no Java code: aapt2 (manifest + resources), zip (the library), zipalign, apksigner.
#
#   tools/build_apk.sh --lib build/android-arm64/lib/libarcade.so --version 1.2.3 \
#                      --out dist/arCade-v1.2.3-android-arm64.apk [--keystore release.jks]
#   tools/build_apk.sh --version-code 1.2.3      # prints the versionCode of a version and stops
#
# Signing:
#   --keystore FILE   the release key. Its passwords come from the environment, never from the command line:
#                     ARCADE_KEYSTORE_PASSWORD (required), ARCADE_KEY_ALIAS (default "arcade"),
#                     ARCADE_KEY_PASSWORD (default: the same as the keystore's).
#   no --keystore     a throwaway key made on the spot: fine for trying the app on a phone (Android only needs
#                     *a* signature), but an app signed like that cannot be updated by a release, and the other way
#                     round (the phone asks to uninstall first, and the scores go with it).
# The Android SDK is found through ANDROID_HOME (or ANDROID_SDK_ROOT): build-tools 35.0.0 or newer (zipalign -P 16
# is how the library gets its 16 KB alignment) and a platform folder (android.jar). AAPT2, ZIPALIGN, APKSIGNER and
# ANDROID_JAR override the search; BUILD_TOOLS_VERSION picks one build-tools folder.
#
# versionCode = (major * 1000 + minor) * 1000 + patch: it only ever grows with the version, which is what Android
# compares when it decides whether an .apk is an update. So minor and patch must stay below 1000.
set -euo pipefail

MIN_SDK=26    # Android 8.0: ALooper_pollOnce, AAudio/OpenSL, adaptive icons
TARGET_SDK=35 # Android 15. Not 36 yet: from 36 on the Back button no longer arrives as a key unless the app opts out
ABI=arm64-v8a

work="" # the temporary folder, removed when the script ends
trap '[ -z "$work" ] || rm -rf "$work"' EXIT
here="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
die() {
  echo "build_apk: $*" >&2
  exit 1
}

# shellcheck source=tools/android_sdk.sh
. "$here/tools/android_sdk.sh" # sdk_root, find_tool, find_android_jar

# ---- versions ---------------------------------------------------------------------------------------------
# "1.2.3", or "1.2.3-dev+abc" (a development build): the numbers decide.
version_code() {
  local v="$1" core major minor patch
  [[ "$v" =~ ^[0-9]+\.[0-9]+\.[0-9]+([-+][0-9A-Za-z.+-]*)?$ ]] || die "'$v' is not a version like 1.2.3"
  core="${v%%[-+]*}"
  IFS=. read -r major minor patch <<<"$core"
  # 10# so that 08 is eight and not a bad octal number
  major=$((10#$major)) minor=$((10#$minor)) patch=$((10#$patch))
  ((minor < 1000 && patch < 1000 && major < 2000)) || die "version $v is too big for a versionCode (minor and patch < 1000)"
  local code=$(((major * 1000 + minor) * 1000 + patch))
  ((code >= 1)) || code=1 # 0.0.0-dev builds: Android does not like 0
  echo "$code"
}

# A 64-bit ARM ELF shared object: 0x7f 'E' 'L' 'F', class 2 (64-bit), e_machine 0xB7 (AArch64) at byte 18.
check_arm64_elf() {
  local head
  head="$(od -An -tx1 -N20 "$1" | tr -d ' \n')"
  [[ "$head" == 7f454c46* ]] || die "$1 is not an ELF file"
  [[ "${head:8:2}" == "02" ]] || die "$1 is not a 64-bit library"
  [[ "${head:36:4}" == "b700" ]] || die "$1 is not built for arm64 (AArch64): did the NDK toolchain run?"
}

main() {
  local lib="" version="" out="" keystore=""
  while [ $# -gt 0 ]; do
    case "$1" in
    --lib)
      lib="${2:-}"
      shift 2
      ;;
    --version)
      version="${2:-}"
      shift 2
      ;;
    --out)
      out="${2:-}"
      shift 2
      ;;
    --keystore)
      keystore="${2:-}"
      shift 2
      ;;
    --version-code)
      version_code "${2:-}"
      return 0
      ;;
    -h | --help)
      sed -n '2,/^set -euo/p' "${BASH_SOURCE[0]}" | sed '$d' | sed 's/^# \{0,1\}//'
      return 0
      ;;
    *) die "unknown option '$1' (see --help)" ;;
    esac
  done
  [ -n "$lib" ] && [ -n "$version" ] && [ -n "$out" ] || die "--lib, --version and --out are required (see --help)"
  [ -f "$lib" ] || die "library not found: $lib"
  local code
  code="$(version_code "$version")"
  check_arm64_elf "$lib"

  local aapt2 zipalign apksigner jar
  aapt2="${AAPT2:-$(find_tool aapt2)}" || die "aapt2 not found (set ANDROID_HOME to an SDK with build-tools 35.0.0 or newer)"
  zipalign="${ZIPALIGN:-$(find_tool zipalign)}" || die "zipalign not found (set ANDROID_HOME)"
  apksigner="${APKSIGNER:-$(find_tool apksigner)}" || die "apksigner not found (set ANDROID_HOME)"
  jar="$(find_android_jar)" || die "android.jar not found (install the platform android-$TARGET_SDK, or set ANDROID_JAR)"
  command -v zip >/dev/null || die "zip not found"
  if [ -n "$keystore" ]; then
    [ -f "$keystore" ] || die "keystore not found: $keystore"
    [ -n "${ARCADE_KEYSTORE_PASSWORD:-}" ] || die "set ARCADE_KEYSTORE_PASSWORD (the keystore's password)"
  fi

  work="$(mktemp -d)"

  echo "build_apk: version $version (versionCode $code), minSdk $MIN_SDK, targetSdk $TARGET_SDK, $ABI"
  # 1. resources (icons, strings, theme) -> compiled; manifest + resources -> a first .apk without the library
  "$aapt2" compile --dir "$here/android/res" -o "$work/res.zip"
  "$aapt2" link -o "$work/base.apk" -I "$jar" --manifest "$here/android/AndroidManifest.xml" \
    --min-sdk-version "$MIN_SDK" --target-sdk-version "$TARGET_SDK" \
    --version-code "$code" --version-name "$version" -R "$work/res.zip"
  # 2. the library goes in STORED (-0, not compressed): the manifest says extractNativeLibs=false, so Android runs
  #    it straight from the .apk, and that needs it uncompressed and page aligned. -X: no extra file attributes.
  mkdir -p "$work/pkg/lib/$ABI"
  cp "$lib" "$work/pkg/lib/$ABI/libarcade.so"
  (cd "$work/pkg" && zip -0 -X -q "$work/base.apk" "lib/$ABI/libarcade.so")
  # 3. align (4 bytes for everything, 16 KB for the stored libraries), THEN sign: signing covers the bytes, so
  #    changing the file afterwards would break the signature.
  #    -P 16 is the zipalign option for 16 KB pages (build-tools 35.0.0 and newer); without it the library would
  #    be aligned for 4 KB pages only and would not load from the .apk on a device with 16 KB pages.
  "$zipalign" -f -P 16 4 "$work/base.apk" "$work/aligned.apk" ||
    die "zipalign failed (-P needs build-tools 35.0.0 or newer: set BUILD_TOOLS_VERSION or ANDROID_HOME)"
  # 4. sign
  local -a sign_args
  if [ -n "$keystore" ]; then
    # apksigner asks on stdin for a password that is not given (its default), which would hang a build: say it
    sign_args=(--ks "$keystore" --ks-key-alias "${ARCADE_KEY_ALIAS:-arcade}" --ks-pass env:ARCADE_KEYSTORE_PASSWORD)
    if [ -n "${ARCADE_KEY_PASSWORD:-}" ]; then
      sign_args+=(--key-pass env:ARCADE_KEY_PASSWORD)
    else
      sign_args+=(--key-pass env:ARCADE_KEYSTORE_PASSWORD)
    fi
    echo "build_apk: signing with the release key $keystore"
  else
    command -v keytool >/dev/null || die "keytool not found (it comes with the JDK): needed for the throwaway key"
    keytool -genkeypair -keystore "$work/throwaway.jks" -storepass arcade-throwaway -keypass arcade-throwaway \
      -alias throwaway -keyalg RSA -keysize 2048 -validity 36500 \
      -dname "CN=arCade throwaway key (test builds only)" >/dev/null 2>&1
    sign_args=(--ks "$work/throwaway.jks" --ks-key-alias throwaway --ks-pass pass:arcade-throwaway
      --key-pass pass:arcade-throwaway)
    echo "build_apk: THROWAWAY key: this .apk is for testing, it cannot be updated by a release"
  fi
  "$apksigner" sign "${sign_args[@]}" --v4-signing-enabled false --out "$work/signed.apk" "$work/aligned.apk"
  # 5. check what was made, with the same tools
  "$apksigner" verify --verbose --print-certs "$work/signed.apk"
  "$zipalign" -c -P 16 4 "$work/signed.apk" || die "the .apk is not aligned (zipalign -c failed)"

  mkdir -p "$(dirname "$out")"
  mv "$work/signed.apk" "$out"
  rm -f "$out.idsig"
  echo "build_apk: wrote $out ($(wc -c <"$out") bytes)"
}

main "$@"
