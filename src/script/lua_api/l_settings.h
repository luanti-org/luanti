// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2013 PilzAdam <pilzadam@minetest.net>

#pragma once

#include "common/c_content.h"
#include "lua_api/l_base.h"

class Settings;

class LuaSettings : public ModApiBase
{
private:
	static const luaL_Reg methods[];

	// garbage collector
	static int gc_object(lua_State *L);

	// get(self, key) -> value
	static int l_get(lua_State *L);

	// get_bool(self, key) -> boolean
	static int l_get_bool(lua_State *L);

	// get_np_group(self, key) -> noiseparam
	static int l_get_np_group(lua_State *L);

	// get_flags(self, key) -> key/value table
	static int l_get_flags(lua_State *L);

	// get_pos(self, key) -> vector or nil
	static int l_get_pos(lua_State *L);

	// set(self, key, value)
	static int l_set(lua_State *L);

	// set_bool(self, key, value)
	static int l_set_bool(lua_State *L);

	// set_np_group(self, key, value)
	static int l_set_np_group(lua_State *L);

	// set_pos(self, key, value)
	static int l_set_pos(lua_State *L);

	// remove(self, key) -> success
	static int l_remove(lua_State *L);

	// get_names(self) -> {key1, ...}
	static int l_get_names(lua_State *L);

	// has(self, key) -> boolean
	static int l_has(lua_State *L);

	// write(self) -> success
	static int l_write(lua_State *L);

	// to_table(self) -> {[key1]=value1,...}
	static int l_to_table(lua_State *L);

	Settings *m_settings = nullptr;
	std::string m_filename;
	bool m_is_own_settings = false;
	bool m_write_allowed = true;

public:
	LuaSettings(Settings *settings, const std::string &filename);
	LuaSettings(const std::string &filename, bool write_allowed);
	~LuaSettings();

	static void create(lua_State *L, Settings *settings, const std::string &filename);

	// LuaSettings(filename)
	// Creates a LuaSettings and leaves it on top of the stack
	static int create_object(lua_State *L);

	static void Register(lua_State *L);

	static const char className[];
};
