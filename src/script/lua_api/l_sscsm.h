// SPDX-FileCopyrightText: 2025 Luanti authors
//
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include "lua_api/l_base.h"

class ModApiSSCSM : public ModApiBase
{
private:
	// get_node_or_nil(pos)
	static int l_get_node_or_nil(lua_State *L);

	// hud_add(form)
	static int l_hud_add(lua_State *L);
	// hud_remove(id)
	static int l_hud_remove(lua_State *L);
	// hud_change(id, stat, data)
	static int l_hud_change(lua_State *L);
	// hud_get(id)
	static int l_hud_get(lua_State *L);
	// hud_get_all()
	static int l_hud_get_all(lua_State *L);

public:
	static void Initialize(lua_State *L, int top);
};
