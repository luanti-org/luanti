// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2025 grorp

#pragma once

#include "l_base.h"

class ModApiPauseMenu: public ModApiBase
{
private:
	static int l_show_touchscreen_layout(lua_State *L);
	static int l_is_internal_server(lua_State *L);

public:
	static void Initialize(lua_State *L, int top);
};
