// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2013 celeron55, Perttu Ahola <celeron55@gmail.com>

#pragma once

#include "cpp_api/s_base.h"


#define CHECK_SECURE_PATH_INTERNAL(L, path, write_required, ptr) \
	if (!ScriptApiSecurity::checkPath(L, path, write_required, ptr)) { \
		throw LuaError(std::string("Mod security: Blocked attempted ") + \
				(write_required ? "write to " : "read from ") + path); \
	}
#define CHECK_SECURE_PATH(L, path, write_required) \
	if (ScriptApiSecurity::isSecure(L)) { \
		CHECK_SECURE_PATH_INTERNAL(L, path, write_required, NULL); \
	}
#define CHECK_SECURE_PATH_POSSIBLE_WRITE(L, path, ptr) \
	if (ScriptApiSecurity::isSecure(L)) { \
		CHECK_SECURE_PATH_INTERNAL(L, path, false, ptr); \
	}


class ScriptApiSecurity : virtual public ScriptApiBase
{
public:
	// Sets up security on the ScriptApi's Lua state
	void initializeSecurity();
	void initializeSecurityClient();
	// Checks if the Lua state has been secured
	static bool isSecure(lua_State *L);
	// Loads a string as Lua code safely (doesn't allow bytecode).
	static bool safeLoadString(lua_State *L, const std::string &code, const char *chunk_name);
	// Loads a file as Lua code safely (doesn't allow bytecode).
	static bool safeLoadFile(lua_State *L, const char *path, const char *display_name = NULL);
	// Check if mod is whitelisted in the given setting
	// This additionally checks that the mod's main file scope is executing.
	static bool checkWhitelisted(lua_State *L, const std::string &setting);
	// Checks if mods are allowed to read (and optionally write) to the path
	static bool checkPath(lua_State *L, const char *path, bool write_required,
			bool *write_allowed=NULL);

private:
	int getThread(lua_State *L);
	// sets the enviroment to the table thats on top of the stack
	void setLuaEnv(lua_State *L, int thread);
	// creates an empty Lua environment
	void createEmptyEnv(lua_State *L);

	// Syntax: "sl_" <Library name or 'g' (global)> '_' <Function name>
	// (sl stands for Secure Lua)

	static int sl_g_dofile(lua_State *L);
	static int sl_g_load(lua_State *L);
	static int sl_g_loadfile(lua_State *L);
	static int sl_g_loadstring(lua_State *L);
	static int sl_g_require(lua_State *L);

	static int sl_io_open(lua_State *L);
	static int sl_io_input(lua_State *L);
	static int sl_io_output(lua_State *L);
	static int sl_io_lines(lua_State *L);

	static int sl_os_rename(lua_State *L);
	static int sl_os_remove(lua_State *L);
	static int sl_os_setlocale(lua_State *L);
};
