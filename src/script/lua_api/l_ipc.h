// Minetest
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include "lua_api/l_base.h"

class ModApiIPC : public ModApiBase {
private:
	static int l_ipc_get(lua_State *L);
	static int l_ipc_set(lua_State *L);
	static int l_ipc_cas(lua_State *L);
	static int l_ipc_poll(lua_State *L);

public:
	static void Initialize(lua_State *L, int top);
};
