#pragma once

#include "lua.h"

#define LUA_UTF8LIBNAME "utf8"
LUALIB_API int luaopen_utf8(lua_State *L);
