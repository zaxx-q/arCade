# Generates <build>/generated/game_registry.c with two lists:
#   g_arcade_games     games found in games/     (the hub menu shows these)
#   g_arcade_deferred  games found in deferred/  (only reachable through the hub's archive)
# Nobody edits a shared file to add a game: drop a folder into games/ (or deferred/) and re-run CMake.
function(arcade_generate_registry)
  get_property(_listed GLOBAL PROPERTY ARCADE_LISTED_IDS)
  get_property(_deferred GLOBAL PROPERTY ARCADE_DEFERRED_IDS)
  list(SORT _listed)
  list(SORT _deferred)

  set(ARCADE_REGISTRY_EXTERNS "")
  set(ARCADE_REGISTRY_ENTRIES "")
  set(ARCADE_DEFERRED_ENTRIES "")
  foreach(_id IN LISTS _listed)
    string(APPEND ARCADE_REGISTRY_EXTERNS "extern const GameDef game_${_id}_def;\n")
    string(APPEND ARCADE_REGISTRY_ENTRIES "    &game_${_id}_def,\n")
  endforeach()
  foreach(_id IN LISTS _deferred)
    string(APPEND ARCADE_REGISTRY_EXTERNS "extern const GameDef game_${_id}_def;\n")
    string(APPEND ARCADE_DEFERRED_ENTRIES "    &game_${_id}_def,\n")
  endforeach()
  list(LENGTH _listed ARCADE_GAME_COUNT)
  list(LENGTH _deferred ARCADE_DEFERRED_COUNT)
  configure_file("${CMAKE_SOURCE_DIR}/cmake/game_registry.c.in"
                 "${CMAKE_BINARY_DIR}/generated/game_registry.c" @ONLY)
  set(ARCADE_REGISTRY_SOURCE "${CMAKE_BINARY_DIR}/generated/game_registry.c" PARENT_SCOPE)
  # every game (listed + deferred) is linked into the hub, so the archive can launch the deferred ones
  set(ARCADE_REGISTERED_IDS "${_listed};${_deferred}" PARENT_SCOPE)
endfunction()
