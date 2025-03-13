// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2013 celeron55, Perttu Ahola <celeron55@gmail.com>

#pragma once

#include "common/c_types.h"
#include "common/c_internal.h"
#include "common/helper.h"
#include "gamedef.h"
#include <unordered_map>

extern "C" {
#include <lua.h>
#include <lauxlib.h>
}

#if CHECK_CLIENT_BUILD()
class Client;
class GUIEngine;
#endif
class EmergeThread;
class ScriptApiBase;
class Server;
class Environment;
class ServerInventoryManager;

class ModApiBase : protected LuaHelper {
public:
	static ScriptApiBase*   getScriptApiBase(lua_State *L);
	static Server*          getServer(lua_State *L);
	static ServerInventoryManager *getServerInventoryMgr(lua_State *L);
	#if CHECK_CLIENT_BUILD()
	static Client*          getClient(lua_State *L);
	static GUIEngine*       getGuiEngine(lua_State *L);
	#endif // !SERVER
	static EmergeThread*    getEmergeThread(lua_State *L);

	static IGameDef*        getGameDef(lua_State *L);

	static Environment*     getEnv(lua_State *L);

	// When we are not loading the mod, this function returns "."
	static std::string      getCurrentModPath(lua_State *L);

	// Get an arbitrary subclass of ScriptApiBase
	// by using dynamic_cast<> on getScriptApiBase()
	template<typename T>
	static T* getScriptApi(lua_State *L) {
		ScriptApiBase *scriptIface = getScriptApiBase(L);
		T *scriptIfaceDowncast = dynamic_cast<T*>(scriptIface);
		if (!scriptIfaceDowncast) {
			throw LuaError("Requested unavailable ScriptApi - core engine bug!");
		}
		return scriptIfaceDowncast;
	}

	static bool registerFunction(lua_State *L,
			const char* name,
			lua_CFunction func,
			int top);

	static void registerClass(lua_State *L, const char *name,
			const luaL_Reg *methods,
			const luaL_Reg *metamethods);

	template<typename T>
	static inline T *checkObject(lua_State *L, int narg)
	{
		return *reinterpret_cast<T**>(luaL_checkudata(L, narg, T::className));
	}

	/**
	 * A wrapper for deprecated functions.
	 *
	 * When called, handles the deprecation according to user settings and then calls `func`.
	 *
	 * @throws Lua Error if required by the user settings.
	 *
	 * @param L Lua state
	 * @param good Name of good function/method
	 * @param bad Name of deprecated function/method
	 * @param func Actual implementation of function
	 * @return value from `func`
	 */
	static int l_deprecated_function(lua_State *L, const char *good, const char *bad, lua_CFunction func);
};
