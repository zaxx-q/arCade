# Builds libcurl from source and links it STATICALLY. Used by release builds (-DARCADE_ONLINE_BUNDLE_CURL=ON).
#
# Why: a release must run on a computer that has nothing installed but the desktop libraries. A system libcurl
# would be a run-time dependency (libcurl.so.4 on Linux, libcurl-4.dll on Windows): without it the game would
# not start at all. Built in, it is just part of the executable.
#
# What is built: the library only, HTTP(S) only, nothing optional (no zlib, brotli, zstd, HTTP/2, IDN, PSL, SSH).
#   Windows: TLS by Schannel, the TLS library of Windows. No OpenSSL, and the certificates come from the Windows
#            certificate store.
#   Others:  TLS by OpenSSL, linked statically (needs libssl.a and libcrypto.a: Debian/Ubuntu `libssl-dev`,
#            Fedora `openssl-devel openssl-static`). No CA bundle path is built in (the build computer's path would
#            be wrong on other distributions): app/net_curl.c looks for one when the program runs
#            (ARCADE_CURL_NEEDS_CA_BUNDLE, set by app/CMakeLists.txt).
#
# Result: the target CURL::libcurl, and ARCADE_CURL_VERSION. A bundled build that cannot be made stops the
# configure step with an error: it never turns into a build without the Online tab.
#
# Pinned by URL + SHA-256 (the way raylib is pinned by tag). To update: change ARCADE_CURL_VERSION and
# ARCADE_CURL_SHA256 together (sha256sum of the .tar.xz from https://github.com/curl/curl/releases) and re-read
# the options below against that release's CMakeLists.txt.
# Offline / local copy:  -DFETCHCONTENT_SOURCE_DIR_CURL=/path/to/curl-<version>
include_guard(GLOBAL)
include(FetchContent)

set(ARCADE_CURL_VERSION "8.23.0")
set(ARCADE_CURL_SHA256 "8dadd7d2536a314f982ad05a651b713f426bf7d02cc2c76cffb9796f2218e6c5")

# A function, so that the settings below stay out of the rest of the project (cache entries excepted).
function(arcade_bundle_curl)
  string(REPLACE "." "_" _tag "${ARCADE_CURL_VERSION}")
  set(_url "https://github.com/curl/curl/releases/download/curl-${_tag}/curl-${ARCADE_CURL_VERSION}.tar.xz")

  if(POLICY CMP0135) # the extracted files get the time of the download, not of the archive
    cmake_policy(SET CMP0135 NEW)
  endif()

  # libcurl is written for the compiler's own C dialect: the strict -std=c99 of arCade's code is not for it.
  unset(CMAKE_C_STANDARD)
  unset(CMAKE_C_STANDARD_REQUIRED)
  set(CMAKE_C_EXTENSIONS ON)

  # The library only: no curl tool, no tests, no examples, no manual pages, no install rules.
  set(_off BUILD_CURL_EXE BUILD_SHARED_LIBS BUILD_TESTING BUILD_EXAMPLES BUILD_LIBCURL_DOCS BUILD_MISC_DOCS
           ENABLE_CURL_MANUAL)
  foreach(_name IN LISTS _off)
    set(${_name} OFF CACHE BOOL "" FORCE)
  endforeach()
  set(BUILD_STATIC_LIBS ON CACHE BOOL "" FORCE)
  set(CURL_DISABLE_INSTALL ON CACHE BOOL "" FORCE)

  # HTTP and HTTPS only (net_curl.c allows nothing else, whatever a server answers with).
  foreach(_proto DICT FILE FTP GOPHER IMAP LDAP LDAPS MQTT POP3 RTSP SMTP TELNET TFTP)
    set(CURL_DISABLE_${_proto} ON CACHE BOOL "" FORCE)
  endforeach()

  # Nothing optional: every one of these is a library that would have to be found, built and shipped as well.
  foreach(_name CURL_ZLIB CURL_BROTLI CURL_ZSTD)
    set(${_name} OFF CACHE STRING "" FORCE)
  endforeach()
  set(_off USE_NGHTTP2 USE_LIBIDN2 CURL_USE_LIBPSL CURL_USE_LIBSSH2 CURL_USE_LIBSSH ENABLE_ARES)
  foreach(_name IN LISTS _off)
    set(${_name} OFF CACHE BOOL "" FORCE)
  endforeach()

  # TLS.
  if(WIN32)
    set(CURL_USE_SCHANNEL ON CACHE BOOL "" FORCE)
    set(CURL_USE_OPENSSL OFF CACHE BOOL "" FORCE)
    set(CURL_DISABLE_CA_SEARCH ON CACHE BOOL "" FORCE) # never trust a certificate file found in PATH
  else()
    set(OPENSSL_USE_STATIC_LIBS ON) # read by find_package(OpenSSL), here and inside curl's own CMakeLists.txt
    find_package(OpenSSL QUIET)
    if(NOT OpenSSL_FOUND)
      message(FATAL_ERROR
        "ARCADE_ONLINE_BUNDLE_CURL needs the static OpenSSL libraries (libssl.a, libcrypto.a) and headers "
        "(Debian/Ubuntu: libssl-dev, Fedora: openssl-devel openssl-static).")
    endif()
    set(CURL_USE_OPENSSL ON CACHE BOOL "" FORCE)
    set(CURL_DISABLE_OPENSSL_AUTO_LOAD_CONFIG ON CACHE BOOL "" FORCE) # this computer's openssl.cnf is not ours
    set(CURL_CA_BUNDLE "none" CACHE STRING "" FORCE)
    set(CURL_CA_PATH "none" CACHE STRING "" FORCE)
  endif()

  FetchContent_Declare(curl
    URL      ${_url}
    URL_HASH SHA256=${ARCADE_CURL_SHA256})
  FetchContent_MakeAvailable(curl)

  if(NOT TARGET CURL::libcurl)
    message(FATAL_ERROR "ArcadeCurl: libcurl ${ARCADE_CURL_VERSION} did not define the target CURL::libcurl.")
  endif()
endfunction()

arcade_bundle_curl()
