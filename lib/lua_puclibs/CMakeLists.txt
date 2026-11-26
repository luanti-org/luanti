add_library(lua_puclibs STATIC
	lstrlib.c
	loslib.c
)

get_target_property(LUA_SOURCES lua_puclibs SOURCES)
set_source_files_properties(${LUA_SOURCES} PROPERTIES LANGUAGE CXX)
target_compile_definitions(lua_puclibs PRIVATE LUAEXP_API=)
target_include_directories(lua_puclibs
	PUBLIC
		${CMAKE_CURRENT_SOURCE_DIR}
	PRIVATE
		${LUA_INCLUDE_DIR}
)
