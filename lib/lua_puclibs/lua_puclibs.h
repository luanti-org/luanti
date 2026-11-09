#pragma once

#include "lua.h"
#include "lauxlib.h""

#define LUAEXP_API

/// PUC version of string.format
LUAEXP_API int luaEstr_format(lua_State *L);

/// PUC Lua os.date
LUAEXP_API int luaEos_date (lua_State *L);
/// PUC Lua os.time
LUAEXP_API int luaEos_time(lua_State *L);
/// PUC Lua os.difftime
LUAEXP_API int luaEos_difftime(lua_State *L);
