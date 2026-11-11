# Core (raylib-free) unit tests. Game tests live in games/<id>/tests/.
foreach(_t core_basics leaderboard sortlib sandbox synth midi)
  add_executable(test_${_t} test_${_t}.c)
  target_link_libraries(test_${_t} PRIVATE arcade_core arcade_test)
  arcade_apply_warnings(test_${_t})
  add_test(NAME core.${_t} COMMAND test_${_t})
endforeach()
