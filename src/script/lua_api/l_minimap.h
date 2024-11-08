// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2017 Loic Blot <loic.blot@unix-experience.fr>

#pragma once

#include "l_base.h"

class Minimap;

class LuaMinimap : public ModApiBase
{
private:
	static const luaL_Reg methods[];

	// garbage collector
	static int gc_object(lua_State *L);

	static int l_get_pos(lua_State *L);
	static int l_set_pos(lua_State *L);

	static int l_get_angle(lua_State *L);
	static int l_set_angle(lua_State *L);

	static int l_get_mode(lua_State *L);
	static int l_set_mode(lua_State *L);

	static int l_show(lua_State *L);
	static int l_hide(lua_State *L);

	static int l_set_shape(lua_State *L);
	static int l_get_shape(lua_State *L);

	Minimap *m_minimap = nullptr;

public:
	LuaMinimap(Minimap *m);
	~LuaMinimap() = default;

	static void create(lua_State *L, Minimap *object);

	static Minimap *getobject(LuaMinimap *ref);

	static void Register(lua_State *L);

	static const char className[];
};
