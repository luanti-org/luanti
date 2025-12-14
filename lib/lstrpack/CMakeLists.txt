add_library(lstrpack STATIC lstrpack.c)
target_include_directories(lstrpack
	PUBLIC
		${CMAKE_CURRENT_SOURCE_DIR}
	PRIVATE
		${LUA_INCLUDE_DIR}
)
