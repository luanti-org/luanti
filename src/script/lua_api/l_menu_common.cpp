// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2013 sapier
// Copyright (C) 2025 grorp

#include "l_menu_common.h"

#include "client/renderingengine.h"
#include "gettext.h"
#include "lua_api/l_internal.h"


int ModApiMenuCommon::l_gettext(lua_State *L)
{
	const char *srctext = luaL_checkstring(L, 1);
	const char *text = *srctext ? gettext(srctext) : "";
	lua_pushstring(L, text);

	return 1;
}


int ModApiMenuCommon::l_get_active_driver(lua_State *L)
{
	auto drivertype = RenderingEngine::get_video_driver()->getDriverType();
	lua_pushstring(L, RenderingEngine::getVideoDriverInfo(drivertype).name.c_str());
	return 1;
}


int ModApiMenuCommon::l_irrlicht_device_supports_touch(lua_State *L)
{
	lua_pushboolean(L, RenderingEngine::get_raw_device()->supportsTouchEvents());
	return 1;
}


int ModApiMenuCommon::l_are_keycodes_equal(lua_State *L)
{
	auto k1 = luaL_checkstring(L, 1);
	auto k2 = luaL_checkstring(L, 2);
	lua_pushboolean(L, KeyPress(k1) == KeyPress(k2));
	return 1;
}


int ModApiMenuCommon::l_get_keycode_name(lua_State *L)
{
	KeyPress kp(luaL_checkstring(L, 1));
	if (kp)
		lua_pushstring(L, strgettext(kp.name()).c_str());
	else
		lua_pushnil(L);
	return 1;
}


void ModApiMenuCommon::Initialize(lua_State *L, int top)
{
	API_FCT(gettext);
	API_FCT(get_active_driver);
	API_FCT(irrlicht_device_supports_touch);
	API_FCT(are_keycodes_equal);
	API_FCT(get_keycode_name);
}


void ModApiMenuCommon::InitializeAsync(lua_State *L, int top)
{
	API_FCT(gettext);
}
