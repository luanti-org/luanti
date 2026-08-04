// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2013 kwolekr, Ryan Kwolek <kwolekr@minetest.net>

#pragma once

#include "lua_api/l_base.h"
#include "util/basic_macros.h"
#include <list>

class Map;
class MMVManip;

/*
  VoxelManip
 */
class LuaVoxelManip : public ModApiBase
{
private:
	bool is_mapgen_vm = false;
	std::list<MMVManip **>::iterator vm_ref_tracker{};

	static const luaL_Reg methods[];

	static LuaVoxelManip *checkObjectValid(lua_State *L, int narg);

	static int gc_object(lua_State *L);

	static int l_read_from_map(lua_State *L);
	static int l_initialize(lua_State *L);
	static int l_get_data(lua_State *L);
	static int l_set_data(lua_State *L);
	static int l_write_to_map(lua_State *L);

	static int l_get_node_at(lua_State *L);
	static int l_set_node_at(lua_State *L);

	static int l_update_map(lua_State *L);
	static int l_update_liquids(lua_State *L);

	static int l_calc_lighting(lua_State *L);
	static int l_set_lighting(lua_State *L);
	static int l_get_light_data(lua_State *L);
	static int l_set_light_data(lua_State *L);

	static int l_get_param2_data(lua_State *L);
	static int l_set_param2_data(lua_State *L);

	static int l_was_modified(lua_State *L);
	static int l_get_emerged_area(lua_State *L);

	static int l_close(lua_State *L);

public:
	MMVManip *vm = nullptr;

	LuaVoxelManip(MMVManip *mmvm, bool is_mapgen_vm);
	LuaVoxelManip(Map *map);
	~LuaVoxelManip();
	DISABLE_CLASS_COPY(LuaVoxelManip)

	// LuaVoxelManip()
	// Creates a LuaVoxelManip and leaves it on top of stack
	static int create_object(lua_State *L);
	// Not callable from Lua
	static void create(lua_State *L, MMVManip *mmvm, bool is_mapgen_vm);

	static void *packIn(lua_State *L, int idx);
	static void packOut(lua_State *L, void *ptr);

	static void Register(lua_State *L);

	static const char className[];
};
