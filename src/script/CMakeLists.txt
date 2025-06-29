add_subdirectory(common)
add_subdirectory(cpp_api)
add_subdirectory(lua_api)

# Used by server and client
file(GLOB common_SCRIPT_HDRS "${CMAKE_CURRENT_SOURCE_DIR}/*.h")

set(common_SCRIPT_SRCS
	${common_SCRIPT_HDRS}
	${CMAKE_CURRENT_SOURCE_DIR}/scripting_server.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/scripting_emerge.cpp

	${common_SCRIPT_COMMON_SRCS}
	${common_SCRIPT_CPP_API_SRCS}
	${common_SCRIPT_LUA_API_SRCS}
	PARENT_SCOPE)

# Used by client only
set(client_SCRIPT_SRCS
	${CMAKE_CURRENT_SOURCE_DIR}/scripting_mainmenu.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/scripting_client.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/scripting_pause_menu.cpp

	${client_SCRIPT_COMMON_SRCS}
	${client_SCRIPT_CPP_API_SRCS}
	${client_SCRIPT_LUA_API_SRCS}
	PARENT_SCOPE)

