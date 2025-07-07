# add_subdirectory(async)
# add_subdirectory(client)
# add_subdirectory(common)
# add_subdirectory(emerge)
# add_subdirectory(fstk)
add_subdirectory(game)
# add_subdirectory(mainmenu)
# add_subdirectory(pause_menu)
# add_subdirectory(profiler)

set(builtin_SRCS
	${CMAKE_CURRENT_SOURCE_DIR}/init.lua
	${builtin_GAME_SRCS}
	PARENT_SCOPE)

set(BUILTIN_BASE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/ PARENT_SCOPE)
