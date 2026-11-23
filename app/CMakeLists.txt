# The hub executable. Every game found in games/ is linked in through the generated registry.
add_executable(arCade main.c screens.c backdrop.c smoke.c online.c "${ARCADE_REGISTRY_SOURCE}")
target_link_libraries(arCade PRIVATE arcade_gfx)
foreach(_id IN LISTS ARCADE_REGISTERED_IDS)
  target_link_libraries(arCade PRIVATE game_${_id})
endforeach()
target_compile_definitions(arCade PRIVATE ARCADE_SOURCE_ASSETS_DIR="${CMAKE_SOURCE_DIR}/assets")
arcade_apply_warnings(arCade)

# ---- Online leaderboard ----
# libcurl is the one network dependency, and an optional one: AUTO uses it when it is installed and builds
# without it otherwise (the Online tab then says so), ON makes it required, OFF never looks for it.
set(ARCADE_ONLINE "AUTO" CACHE STRING "Online leaderboard: AUTO (use libcurl when found), ON (require it) or OFF")
set_property(CACHE ARCADE_ONLINE PROPERTY STRINGS AUTO ON OFF)
set(ARCADE_ONLINE_URL "https://arcade-leaderboard.zaxx-q.workers.dev" CACHE STRING
    "Server of the online leaderboard (https://host; the ARCADE_ONLINE_URL environment variable overrides it)")
if(NOT ARCADE_ONLINE STREQUAL "OFF")
  if(ARCADE_ONLINE STREQUAL "ON")
    find_package(CURL REQUIRED)
  else()
    find_package(CURL QUIET)
  endif()
endif()
if(CURL_FOUND)
  target_sources(arCade PRIVATE net_curl.c)
  target_link_libraries(arCade PRIVATE CURL::libcurl)
  target_compile_definitions(arCade PRIVATE ARCADE_HAVE_CURL=1)
  message(STATUS "Online leaderboard: libcurl ${CURL_VERSION_STRING}, server ${ARCADE_ONLINE_URL}")
  # Developer tool: runs the real client code against a real server (not part of ctest: it needs a network).
  add_executable(arcade_online_probe online_probe.c net_curl.c)
  target_link_libraries(arcade_online_probe PRIVATE arcade_core CURL::libcurl)
  arcade_apply_warnings(arcade_online_probe)
  # The player's side of it (opt-in, what is remembered and forgotten), tested without a network.
  add_executable(test_online ../tests/test_online.c online.c net_curl.c)
  target_include_directories(test_online PRIVATE ${CMAKE_CURRENT_SOURCE_DIR} ${PROJECT_SOURCE_DIR}/tests)
  target_compile_definitions(test_online PRIVATE ARCADE_HAVE_CURL=1 ARCADE_ONLINE_DEFAULT_URL="http://127.0.0.1:9")
  target_link_libraries(test_online PRIVATE arcade_core CURL::libcurl)
  arcade_apply_warnings(test_online)
  add_test(NAME app.online COMMAND test_online)
else()
  message(STATUS "Online leaderboard: off (libcurl not found; install its development files, or pass -DARCADE_ONLINE=OFF)")
endif()
target_compile_definitions(arCade PRIVATE ARCADE_ONLINE_DEFAULT_URL="${ARCADE_ONLINE_URL}")

# Put assets/ next to the executable so the build tree has the same layout as a release zip.
add_custom_command(TARGET arCade POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/assets" "$<TARGET_FILE_DIR:arCade>/assets"
  COMMENT "Copying assets next to arCade")
