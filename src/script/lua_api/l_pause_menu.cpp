// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2025 grorp

#include "l_pause_menu.h"
#include "client/keycode.h"
#include "gui/mainmenumanager.h"
#include "lua_api/l_internal.h"


int ModApiPauseMenu::l_show_touchscreen_layout(lua_State *L)
{
	g_gamecallback->touchscreenLayout();
	return 0;
}

int ModApiPauseMenu::l_are_keycodes_equal(lua_State *L)
{
	auto k1 = luaL_checkstring(L, 1);
	auto k2 = luaL_checkstring(L, 2);
	lua_pushboolean(L, KeyPress(k1) == KeyPress(k2));
	return 1;
}


void ModApiPauseMenu::Initialize(lua_State *L, int top)
{
	API_FCT(show_touchscreen_layout);
	API_FCT(are_keycodes_equal);
}
