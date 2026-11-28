# Core (raylib-free) unit tests. Game tests live in games/<id>/tests/.
foreach(_t core_basics view touch osk leaderboard lbnet sortlib sandbox vault synth midi assets)
  add_executable(test_${_t} test_${_t}.c)
  target_link_libraries(test_${_t} PRIVATE arcade_core arcade_test)
  arcade_apply_warnings(test_${_t})
  add_test(NAME core.${_t} COMMAND test_${_t})
endforeach()

# test_assets compares the assets built into the executable with the files they were made from.
file(GLOB_RECURSE _asset_files CONFIGURE_DEPENDS "${CMAKE_SOURCE_DIR}/assets/*")
list(LENGTH _asset_files _asset_count)
list(LENGTH ARCADE_ASSET_ROOT_FILES _root_count)
math(EXPR _asset_total "${_asset_count} + ${_root_count}")
target_compile_definitions(test_assets PRIVATE
  ARCADE_ASSETS_DIR="${CMAKE_SOURCE_DIR}/assets"
  ARCADE_ROOT_DIR="${CMAKE_SOURCE_DIR}"
  ARCADE_ASSET_TOTAL=${_asset_total})
