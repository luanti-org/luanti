add_subdirectory(async)
add_subdirectory(client)
add_subdirectory(common)
add_subdirectory(emerge)
add_subdirectory(fstk)
add_subdirectory(game)
add_subdirectory(mainmenu)
add_subdirectory(pause_menu)
add_subdirectory(profiler)

set(BUILTIN_SRCS
	init.lua
	${BUILTIN_ASYNC_SRCS}
	${BUILTIN_CLIENT_SRCS}
	${BUILTIN_COMMON_SRCS}
	${BUILTIN_EMERGE_SRCS}
	${BUILTIN_FSTK_SRCS}
	${BUILTIN_GAME_SRCS}
	${BUILTIN_MAINMENU_SRCS}
	${BUILTIN_PAUSE_MENU_SRCS}
	${BUILTIN_PROFILER_SRCS}
	PARENT_SCOPE)

set(BUILTIN_BASE_PATH ${CMAKE_CURRENT_SOURCE_DIR} PARENT_SCOPE)
