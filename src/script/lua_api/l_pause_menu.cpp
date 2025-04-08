// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2025 grorp

#include "l_pause_menu.h"
#include "gui/mainmenumanager.h"
#include "lua_api/l_internal.h"


int ModApiPauseMenu::l_show_keys_menu(lua_State *L)
{
	g_gamecallback->keyConfig();
	return 0;
}


int ModApiPauseMenu::l_show_touchscreen_layout(lua_State *L)
{
	g_gamecallback->touchscreenLayout();
	return 0;
}


void ModApiPauseMenu::Initialize(lua_State *L, int top)
{
	API_FCT(show_keys_menu);
	API_FCT(show_touchscreen_layout);
}
