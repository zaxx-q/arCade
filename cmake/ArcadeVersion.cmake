# Resolves ARCADE_VERSION_RESOLVED:
#   1. -DARCADE_VERSION=1.2.3 (used by CI releases), else
#   2. `git describe` on the latest v* tag, else
#   3. 0.0.0-dev
# Then generates <build>/generated/arcade/version.h
if(ARCADE_VERSION)
  set(ARCADE_VERSION_RESOLVED "${ARCADE_VERSION}")
else()
  set(ARCADE_VERSION_RESOLVED "0.0.0-dev")
  find_package(Git QUIET)
  if(GIT_FOUND)
    execute_process(
      COMMAND "${GIT_EXECUTABLE}" describe --tags --match "v[0-9]*" --dirty --always
      WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
      OUTPUT_VARIABLE _desc RESULT_VARIABLE _rc
      OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET)
    if(_rc EQUAL 0 AND _desc)
      if(_desc MATCHES "^v[0-9]+\\.[0-9]+\\.[0-9]+")
        string(REGEX REPLACE "^v" "" ARCADE_VERSION_RESOLVED "${_desc}")
      else()
        set(ARCADE_VERSION_RESOLVED "0.0.0-dev+${_desc}")
      endif()
    endif()
  endif()
endif()

configure_file("${CMAKE_SOURCE_DIR}/cmake/version.h.in"
               "${CMAKE_BINARY_DIR}/generated/arcade/version.h" @ONLY)
