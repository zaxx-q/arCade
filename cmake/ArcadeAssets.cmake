# arcade_embed_assets(<output variable>)
#
# Compiles everything in assets/ (plus LICENSE and THIRD_PARTY_NOTICES.md) into the executable, so the game is one
# single file. Sets <output variable> to the generated C file; add it to a library's sources (core/CMakeLists.txt).
#
# The file is generated at BUILD time by cmake/embed_assets.cmake and only again when one of the assets changed.
# CONFIGURE_DEPENDS makes CMake notice added or removed assets by itself: just drop a file into assets/ and build.
set(ARCADE_ASSET_ROOT_FILES LICENSE THIRD_PARTY_NOTICES.md)

function(arcade_embed_assets out_var)
  file(GLOB_RECURSE _assets CONFIGURE_DEPENDS "${CMAKE_SOURCE_DIR}/assets/*")
  list(SORT _assets)
  set(_root_files "")
  foreach(_f IN LISTS ARCADE_ASSET_ROOT_FILES)
    list(APPEND _root_files "${CMAKE_SOURCE_DIR}/${_f}")
  endforeach()
  string(REPLACE ";" "," _root_arg "${ARCADE_ASSET_ROOT_FILES}") # a list cannot travel through -D: "," stands for ";"

  set(_out "${CMAKE_BINARY_DIR}/generated/assets_data.c")
  add_custom_command(
    OUTPUT "${_out}"
    COMMAND "${CMAKE_COMMAND}" "-DASSETS_DIR=${CMAKE_SOURCE_DIR}/assets" "-DROOT_DIR=${CMAKE_SOURCE_DIR}"
            "-DROOT_FILES=${_root_arg}" "-DOUT=${_out}" -P "${CMAKE_SOURCE_DIR}/cmake/embed_assets.cmake"
    DEPENDS ${_assets} ${_root_files} "${CMAKE_SOURCE_DIR}/cmake/embed_assets.cmake"
    COMMENT "Embedding assets/ into the executable"
    VERBATIM)
  set(${out_var} "${_out}" PARENT_SCOPE)
endfunction()
