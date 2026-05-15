# Lua core source files.
file(GLOB LUA_CORE_HDRS "${CMAKE_CURRENT_SOURCE_DIR}/*.h")

set(LUA_CORE_SRCS
	lapi.c
	lauxlib.c
	lbaselib.c
	lcode.c
	ldblib.c
	ldebug.c
	ldo.c
	ldump.c
	lfunc.c
	lgc.c
	linit.c
	liolib.c
	llex.c
	lmathlib.c
	lmem.c
	loadlib.c
	lobject.c
	lopcodes.c
	loslib.c
	lparser.c
	lstate.c
	lstring.c
	lstrlib.c
	ltable.c
	ltablib.c
	ltm.c
	lundump.c
	lvm.c
	lzio.c
)

# Lua library
# Headers are included for IDE visibility but not compiled
add_library(lua STATIC ${LUA_CORE_SRCS} ${LUA_CORE_HDRS})
target_link_libraries(lua ${LIBS})
set_target_properties(lua PROPERTIES
	VERSION ${LUA_VERSION}
	CLEAN_DIRECT_OUTPUT 1
)

# Compile C source files as C++
set_source_files_properties(${LUA_CORE_SRCS} PROPERTIES LANGUAGE CXX)
