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
		CHECK_SECURE_PATH_INTERNAL(L, path, write_required, nullptr); \
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
#if CHECK_CLIENT_BUILD()
	void initializeSecurityClient();
#else
	inline void initializeSecurityClient() { assert(0); }
#endif

	// Checks if the Lua state has been secured
	static bool isSecure(lua_State *L);
	// Leaves the untampered globals (table) on top of the stack
	static void getGlobalsBackup(lua_State *L);

	/// Loads a string as Lua code safely (doesn't allow bytecode).
	static bool safeLoadString(lua_State *L, std::string_view code, const char *chunk_name);
	/// Loads a file as Lua code safely (doesn't allow bytecode).
	/// @warning path is not validated in any way
	static bool safeLoadFile(lua_State *L, const char *path, const char *display_name = nullptr);

	/**
	 * Returns the currently running mod, only during init time.
	 * This checks the Lua stack to only permit direct calls in the file
	 * scope. That way it is assured that it's really the mod it claims to be.
	 * @return mod name or "" on error
	 */
	static std::string getCurrentModName(lua_State *L);
	/// Check if mod is whitelisted in the given setting.
	/// This additionally does main scope checks (see above method).
	/// @note check is performed even in non-secured Lua state
	static bool checkWhitelisted(lua_State *L, const std::string &setting);

	/// Checks if mods are allowed to read (and optionally write) to the path
	/// @note invalid to call in non-secured Lua state
	static bool checkPath(lua_State *L, const char *path, bool write_required,
			bool *write_allowed = nullptr);

protected:
	// To be implemented by descendants:

	/**
	 * Specify if the mod names during init time(!) can be trusted.
	 * It needs to be assured that no tampering happens before any call to `loadMod()`.
	 * @note disabling this implies that mod whitelisting never works
	 * @return boolean value
	 */
	virtual bool modNamesAreTrusted() { return false; }

	/**
	 * Should check if the given path may be accessed.
	 * If `write_required` is true test for write access, if false test for read access.
	 * @param abs_path absolute file/directory path, may not exist
	 * @param write_required was write access requested?
	 * @param write_allowed output parameter (nullable): set to true if writing is allowed
	 * @return true if access is allowed
	 */
	virtual bool checkPathInternal(const std::string &abs_path, bool write_required,
		bool *write_allowed) = 0;

	// Ready-made implementation of `checkPathInternal` suitable for server-related uses
	static bool checkPathWithGamedef(lua_State *L, const std::string &abs_path,
		bool write_required, bool *write_allowed);

private:
	int getThread(lua_State *L);
	// sets the enviroment to the table thats on top of the stack
	void setLuaEnv(lua_State *L, int thread);
	// creates an empty Lua environment
	void createEmptyEnv(lua_State *L);

	bool m_secure = false;

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
