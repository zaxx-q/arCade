# Generates <build>/generated/game_registry.c listing every game found by arcade_add_game().
# Nobody edits a shared file to add a game: drop a folder into games/ and re-run CMake.
function(arcade_generate_registry)
  get_property(_ids GLOBAL PROPERTY ARCADE_GAME_IDS)
  list(SORT _ids)
  set(ARCADE_REGISTRY_EXTERNS "")
  set(ARCADE_REGISTRY_ENTRIES "")
  foreach(_id IN LISTS _ids)
    string(APPEND ARCADE_REGISTRY_EXTERNS "extern const GameDef game_${_id}_def;\n")
    string(APPEND ARCADE_REGISTRY_ENTRIES "    &game_${_id}_def,\n")
  endforeach()
  list(LENGTH _ids ARCADE_GAME_COUNT)
  configure_file("${CMAKE_SOURCE_DIR}/cmake/game_registry.c.in"
                 "${CMAKE_BINARY_DIR}/generated/game_registry.c" @ONLY)
  set(ARCADE_REGISTRY_SOURCE "${CMAKE_BINARY_DIR}/generated/game_registry.c" PARENT_SCOPE)
  set(ARCADE_REGISTERED_IDS "${_ids}" PARENT_SCOPE)
endfunction()
