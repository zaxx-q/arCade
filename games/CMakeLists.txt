# Auto-discovery: every folder games/<id>/ that contains a CMakeLists.txt is a game.
# Folders starting with "_" (e.g. _template) are ignored.
file(GLOB _entries CONFIGURE_DEPENDS RELATIVE "${CMAKE_CURRENT_SOURCE_DIR}" "${CMAKE_CURRENT_SOURCE_DIR}/*")
list(SORT _entries)
foreach(_e IN LISTS _entries)
  if(IS_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/${_e}"
     AND NOT _e MATCHES "^[_.]"
     AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/${_e}/CMakeLists.txt")
    add_subdirectory("${_e}")
  endif()
endforeach()
