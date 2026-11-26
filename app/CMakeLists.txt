# The hub executable. Every game found in games/ and deferred/ is linked in through the generated registry.
# backdrop.c draws the Synthwave City game behind the menu (through synthwave_city_session.h, which reaches the
# hub because every game library exports its own folder as an include directory), so that game must exist.
# It may sit in games/ or in deferred/: the folder only decides whether it is also a menu entry.
if(NOT "synthwave_city" IN_LIST ARCADE_REGISTERED_IDS)
  message(FATAL_ERROR "app/backdrop.c needs the game 'synthwave_city' (games/ or deferred/): it is the hub's backdrop")
endif()

add_executable(arCade main.c screens.c backdrop.c smoke.c online.c "${ARCADE_REGISTRY_SOURCE}")
target_link_libraries(arCade PRIVATE arcade_gfx)
foreach(_id IN LISTS ARCADE_REGISTERED_IDS)
  target_link_libraries(arCade PRIVATE game_${_id})
endforeach()
arcade_apply_warnings(arCade)

# ---- Online leaderboard ----
# libcurl is the one network dependency, and an optional one: AUTO uses it when it is installed and builds
# without it otherwise (the Online tab then says so), ON makes it required, OFF never looks for it.
# ARCADE_ONLINE_BUNDLE_CURL (what releases use) builds libcurl from source and links it statically instead of
# using the system's, so that the program needs no libcurl when it runs (cmake/ArcadeCurl.cmake). OFF wins.
set(ARCADE_ONLINE "AUTO" CACHE STRING "Online leaderboard: AUTO (use libcurl when found), ON (require it) or OFF")
set_property(CACHE ARCADE_ONLINE PROPERTY STRINGS AUTO ON OFF)
option(ARCADE_ONLINE_BUNDLE_CURL "Online leaderboard: build libcurl from source and link it statically (releases)" OFF)
set(ARCADE_ONLINE_URL "https://arcade-leaderboard.zaxx-q.workers.dev" CACHE STRING
    "Server of the online leaderboard (https://host; the ARCADE_ONLINE_URL environment variable overrides it)")

set(ARCADE_CURL_DEFS "") # compile definitions of every target that builds net_curl.c (empty = no libcurl)
set(ARCADE_CURL_DESC "")
if(NOT ARCADE_ONLINE STREQUAL "OFF")
  if(ARCADE_ONLINE_BUNDLE_CURL)
    include(ArcadeCurl) # stops with an error when it cannot: a release must never silently lose the Online tab
    set(ARCADE_CURL_DESC "libcurl ${ARCADE_CURL_VERSION}, built in (static)")
    set(ARCADE_CURL_DEFS ARCADE_HAVE_CURL=1)
    if(NOT WIN32) # no certificate store of its own there: net_curl.c looks for the system's when it runs
      list(APPEND ARCADE_CURL_DEFS ARCADE_CURL_NEEDS_CA_BUNDLE=1)
    endif()
  else()
    if(ARCADE_ONLINE STREQUAL "ON")
      find_package(CURL REQUIRED)
    else()
      find_package(CURL QUIET)
    endif()
    if(CURL_FOUND)
      set(ARCADE_CURL_DESC "libcurl ${CURL_VERSION_STRING}")
      set(ARCADE_CURL_DEFS ARCADE_HAVE_CURL=1)
    endif()
  endif()
endif()

if(ARCADE_CURL_DEFS)
  target_sources(arCade PRIVATE net_curl.c)
  target_link_libraries(arCade PRIVATE CURL::libcurl)
  target_compile_definitions(arCade PRIVATE ${ARCADE_CURL_DEFS})
  message(STATUS "Online leaderboard: ${ARCADE_CURL_DESC}, server ${ARCADE_ONLINE_URL}")
  # Developer tool: runs the real client code against a real server (not part of ctest: it needs a network).
  add_executable(arcade_online_probe online_probe.c net_curl.c)
  target_link_libraries(arcade_online_probe PRIVATE arcade_core CURL::libcurl)
  target_compile_definitions(arcade_online_probe PRIVATE ${ARCADE_CURL_DEFS})
  arcade_apply_warnings(arcade_online_probe)
  # The player's side of it (opt-in, what is remembered and forgotten), tested without a network.
  add_executable(test_online ../tests/test_online.c online.c net_curl.c)
  target_include_directories(test_online PRIVATE ${CMAKE_CURRENT_SOURCE_DIR} ${PROJECT_SOURCE_DIR}/tests)
  target_compile_definitions(test_online PRIVATE ${ARCADE_CURL_DEFS} ARCADE_ONLINE_DEFAULT_URL="http://127.0.0.1:9")
  target_link_libraries(test_online PRIVATE arcade_core CURL::libcurl)
  arcade_apply_warnings(test_online)
  add_test(NAME app.online COMMAND test_online)
elseif(ARCADE_ONLINE STREQUAL "OFF")
  message(STATUS "Online leaderboard: off (ARCADE_ONLINE=OFF)")
else()
  message(STATUS "Online leaderboard: off (libcurl not found; install its development files, or pass -DARCADE_ONLINE=OFF)")
endif()
target_compile_definitions(arCade PRIVATE ARCADE_ONLINE_DEFAULT_URL="${ARCADE_ONLINE_URL}")

# Nothing else to ship: assets/ is compiled into the executable (cmake/ArcadeAssets.cmake), so build/<preset>/bin/arCade
# is the same single file that a release contains.
