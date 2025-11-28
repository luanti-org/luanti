#pragma once

#include "lua.h"
#include "lauxlib.h"

/// Populates string.{pack,unpack,packsize}
void setup_lstrpack(lua_State *L);
