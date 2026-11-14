# arcade_add_game(ID <id> LOGIC <files...> GAME <files...> [TESTS <files...>])
#
#   LOGIC  pure C (no raylib):   <id>_logic.c   -> static lib game_<id>_logic
#   GAME   raylib front-end:     <id>_game.c    -> static lib game_<id>   (skipped when ARCADE_LOGIC_ONLY=ON)
#   TESTS  unit tests, one executable each, registered with CTest
#
# The folder name MUST equal ID. Games are discovered automatically by games/CMakeLists.txt.
function(arcade_add_game)
  cmake_parse_arguments(AG "" "ID" "LOGIC;GAME;TESTS" ${ARGN})

  if(NOT AG_ID MATCHES "^[a-z][a-z0-9_]*$")
    message(FATAL_ERROR "arcade_add_game: ID '${AG_ID}' must match ^[a-z][a-z0-9_]*$")
  endif()
  get_filename_component(_dir_name "${CMAKE_CURRENT_SOURCE_DIR}" NAME)
  if(NOT _dir_name STREQUAL AG_ID)
    message(FATAL_ERROR "arcade_add_game: folder '${_dir_name}' must be named exactly like ID '${AG_ID}'")
  endif()

  # --- pure logic library: sees only core/include, never raylib ---
  add_library(game_${AG_ID}_logic STATIC ${AG_LOGIC})
  target_include_directories(game_${AG_ID}_logic PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
  target_link_libraries(game_${AG_ID}_logic PUBLIC arcade_core)
  arcade_apply_warnings(game_${AG_ID}_logic)

  # --- unit tests ---
  if(ARCADE_BUILD_TESTS)
    foreach(_t IN LISTS AG_TESTS)
      get_filename_component(_n "${_t}" NAME_WE)
      # target name includes the game id so two games (or a game and a core test) can never collide
      add_executable(gtest_${AG_ID}_${_n} "${_t}")
      target_link_libraries(gtest_${AG_ID}_${_n} PRIVATE game_${AG_ID}_logic arcade_test)
      arcade_apply_warnings(gtest_${AG_ID}_${_n})
      # tests may read shipped assets (e.g. every song in assets/beatmaps must load)
      target_compile_definitions(gtest_${AG_ID}_${_n} PRIVATE ARCADE_ASSETS_DIR="${CMAKE_SOURCE_DIR}/assets")
      add_test(NAME ${AG_ID}.${_n} COMMAND gtest_${AG_ID}_${_n})
    endforeach()
  endif()

  # --- raylib front-end ---
  if(NOT ARCADE_LOGIC_ONLY)
    add_library(game_${AG_ID} STATIC ${AG_GAME})
    target_link_libraries(game_${AG_ID} PUBLIC game_${AG_ID}_logic arcade_gfx)
    arcade_apply_warnings(game_${AG_ID})
  endif()

  # The folder decides whether the hub lists the game: games/<id> = shown on the menu, deferred/<id> =
  # built, unit-tested and smoke-tested like any other game, but only reachable through the hub's archive.
  get_filename_component(_group_dir "${CMAKE_CURRENT_SOURCE_DIR}" DIRECTORY)
  get_filename_component(_group "${_group_dir}" NAME)
  if(_group STREQUAL "deferred")
    set_property(GLOBAL APPEND PROPERTY ARCADE_DEFERRED_IDS ${AG_ID})
  else()
    set_property(GLOBAL APPEND PROPERTY ARCADE_LISTED_IDS ${AG_ID})
  endif()
  set_property(GLOBAL APPEND PROPERTY ARCADE_GAME_IDS ${AG_ID})
endfunction()

# arcade_discover_games()
#
#   Call it from games/CMakeLists.txt and deferred/CMakeLists.txt. Every sub-folder that contains a
#   CMakeLists.txt is a game; folders starting with "_" or "." (e.g. _template) are ignored.
function(arcade_discover_games)
  file(GLOB _entries CONFIGURE_DEPENDS RELATIVE "${CMAKE_CURRENT_SOURCE_DIR}" "${CMAKE_CURRENT_SOURCE_DIR}/*")
  list(SORT _entries)
  foreach(_e IN LISTS _entries)
    if(IS_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/${_e}"
       AND NOT _e MATCHES "^[_.]"
       AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/${_e}/CMakeLists.txt")
      add_subdirectory("${_e}")
    endif()
  endforeach()
endfunction()
