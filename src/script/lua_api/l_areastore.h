// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2015 est31 <mtest31@outlook.com>

#pragma once

#include "lua_api/l_base.h"

class AreaStore;

class LuaAreaStore : public ModApiBase
{
private:
	static const luaL_Reg methods[];

	static int gc_object(lua_State *L);

	static int l_get_area(lua_State *L);

	static int l_get_areas_for_pos(lua_State *L);
	static int l_get_areas_in_area(lua_State *L);
	static int l_insert_area(lua_State *L);
	static int l_reserve(lua_State *L);
	static int l_remove_area(lua_State *L);

	static int l_set_cache_params(lua_State *L);

	static int l_to_string(lua_State *L);
	static int l_to_file(lua_State *L);

	static int l_from_string(lua_State *L);
	static int l_from_file(lua_State *L);

public:
	AreaStore *as = nullptr;

	LuaAreaStore();
	LuaAreaStore(const std::string &type);
	~LuaAreaStore();

	// AreaStore()
	// Creates an AreaStore and leaves it on top of stack
	static int create_object(lua_State *L);

	static void Register(lua_State *L);

	static const char className[];
};
