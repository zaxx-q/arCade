# The hub executable. Every game found in games/ is linked in through the generated registry.
add_executable(arcade-hub main.c screens.c backdrop.c smoke.c "${ARCADE_REGISTRY_SOURCE}")
target_link_libraries(arcade-hub PRIVATE arcade_gfx)
foreach(_id IN LISTS ARCADE_REGISTERED_IDS)
  target_link_libraries(arcade-hub PRIVATE game_${_id})
endforeach()
target_compile_definitions(arcade-hub PRIVATE ARCADE_SOURCE_ASSETS_DIR="${CMAKE_SOURCE_DIR}/assets")
arcade_apply_warnings(arcade-hub)

# Put assets/ next to the executable so the build tree has the same layout as a release zip.
add_custom_command(TARGET arcade-hub POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/assets" "$<TARGET_FILE_DIR:arcade-hub>/assets"
  COMMENT "Copying assets next to arcade-hub")
