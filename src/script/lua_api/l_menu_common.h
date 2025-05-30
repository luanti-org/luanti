// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2013 sapier
// Copyright (C) 2025 grorp

#pragma once

#include "l_base.h"

class ModApiMenuCommon: public ModApiBase
{
private:
	static int l_gettext(lua_State *L);
	static int l_get_active_driver(lua_State *L);
	static int l_irrlicht_device_supports_touch(lua_State *L);
	static int l_normalize_keycode(lua_State *L);

public:
	static void Initialize(lua_State *L, int top);
	static void InitializeAsync(lua_State *L, int top);
};
