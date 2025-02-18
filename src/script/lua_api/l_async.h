// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include "lua_api/l_base.h"

class ModApiAsync : public ModApiBase
{
public:
	static void Initialize(lua_State *L, int top);
private:
	// do_async_callback(func, params, mod_origin)
	static int l_do_async_callback(lua_State *L);
	// cancel_async_callback(id)
	static int l_cancel_async_callback(lua_State *L);
	// get_async_threading_capacity()
	static int l_get_async_threading_capacity(lua_State *L);
};
