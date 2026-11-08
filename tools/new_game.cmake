# Scaffolds a new game from games/_template.
#
#   cmake -DID=my_game -DTITLE="My Game" -DOWNER="Your Name" -P tools/new_game.cmake
#
# Then re-run CMake (or just build): games/CMakeLists.txt discovers the folder automatically.
if(NOT ID)
  message(FATAL_ERROR "Missing -DID=<snake_case_id>\nExample: cmake -DID=my_game -DTITLE=\"My Game\" -P tools/new_game.cmake")
endif()
if(NOT ID MATCHES "^[a-z][a-z0-9_]*$")
  message(FATAL_ERROR "ID '${ID}' must match ^[a-z][a-z0-9_]*$ (lowercase letters, digits, underscores)")
endif()
if(NOT TITLE)
  set(TITLE "${ID}")
endif()
if(NOT OWNER)
  set(OWNER "TODO")
endif()
string(TOUPPER "${ID}" IDU)
string(TIMESTAMP DATE "%Y-%m-%d")

get_filename_component(ROOT "${CMAKE_CURRENT_LIST_DIR}/.." ABSOLUTE)
set(SRC "${ROOT}/games/_template")
set(DEST "${ROOT}/games/${ID}")
if(EXISTS "${DEST}")
  message(FATAL_ERROR "games/${ID} already exists")
endif()

file(GLOB_RECURSE _files RELATIVE "${SRC}" "${SRC}/*")
foreach(_f IN LISTS _files)
  string(REPLACE "GAMEID" "${ID}" _out "${_f}")
  configure_file("${SRC}/${_f}" "${DEST}/${_out}" @ONLY NEWLINE_STYLE UNIX)
endforeach()
message(STATUS "Created games/${ID}  (owner: ${OWNER})")
message(STATUS "Next: read games/${ID}/GAME.md, then build:  cmake --preset dev && cmake --build --preset dev")
