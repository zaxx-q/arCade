#!/usr/bin/env bash
# Finding the Android SDK tools. Sourced (not run) by tools/build_apk.sh and tools/verify_apk.sh, which define die().
# The SDK is ANDROID_HOME (or ANDROID_SDK_ROOT); BUILD_TOOLS_VERSION picks one build-tools folder (default: the
# newest); TARGET_SDK (set by the caller) is the platform whose android.jar is used.

sdk_root() { echo "${ANDROID_HOME:-${ANDROID_SDK_ROOT:-}}"; }

# find_tool NAME: the build-tools program NAME, from the newest (or BUILD_TOOLS_VERSION) folder of the SDK, else PATH.
find_tool() {
  local name="$1" sdk dir
  sdk="$(sdk_root)"
  if [ -n "$sdk" ]; then
    if [ -n "${BUILD_TOOLS_VERSION:-}" ]; then
      dir="$sdk/build-tools/$BUILD_TOOLS_VERSION"
    else
      dir="$(printf '%s\n' "$sdk"/build-tools/*/ | sort -V | tail -n 1)"
    fi
    if [ -x "${dir%/}/$name" ]; then
      echo "${dir%/}/$name"
      return 0
    fi
  fi
  command -v "$name"
}

find_android_jar() {
  local sdk jar
  if [ -n "${ANDROID_JAR:-}" ]; then
    echo "$ANDROID_JAR"
    return 0
  fi
  sdk="$(sdk_root)"
  [ -n "$sdk" ] || return 1
  jar="$sdk/platforms/android-$TARGET_SDK/android.jar"
  if [ ! -f "$jar" ]; then
    # the newest platform there is (aapt2 only reads the resource names from it)
    jar="$(printf '%s\n' "$sdk"/platforms/android-[0-9]*/android.jar | sort -V | tail -n 1)"
  fi
  [ -f "$jar" ] || return 1
  echo "$jar"
}
