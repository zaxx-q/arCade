# The hub executable. Every game found in games/ and deferred/ is linked in through the generated registry.
# backdrop.c draws the Synthwave City game behind the menu (through synthwave_city_session.h, which reaches the
# hub because every game library exports its own folder as an include directory), so that game must exist.
# It may sit in games/ or in deferred/: the folder only decides whether it is also a menu entry.
if(NOT "synthwave_city" IN_LIST ARCADE_REGISTERED_IDS)
  message(FATAL_ERROR "app/backdrop.c needs the game 'synthwave_city' (games/ or deferred/): it is the hub's backdrop")
endif()

add_executable(arCade main.c screens.c backdrop.c smoke.c "${ARCADE_REGISTRY_SOURCE}")
target_link_libraries(arCade PRIVATE arcade_gfx)
foreach(_id IN LISTS ARCADE_REGISTERED_IDS)
  target_link_libraries(arCade PRIVATE game_${_id})
endforeach()
target_compile_definitions(arCade PRIVATE ARCADE_SOURCE_ASSETS_DIR="${CMAKE_SOURCE_DIR}/assets")
arcade_apply_warnings(arCade)

# Put assets/ next to the executable so the build tree has the same layout as a release zip.
add_custom_command(TARGET arCade POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/assets" "$<TARGET_FILE_DIR:arCade>/assets"
  COMMENT "Copying assets next to arCade")
