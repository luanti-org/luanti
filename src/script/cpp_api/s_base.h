// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2013 celeron55, Perttu Ahola <celeron55@gmail.com>

#pragma once

#include <iostream>
#include <string>
#include <thread>
#include <mutex>
#include <unordered_map>
#include "common/helper.h"
#include "util/basic_macros.h"

extern "C" {
#include <lua.h>
#include <lualib.h>
}

#include "irrlichttypes.h"
#include "common/c_internal.h"
#include "debug.h"
#include "config.h"

#define SCRIPTAPI_LOCK_DEBUG

// MUST be an invalid mod name so that mods can't
// use that name to bypass security!
#define BUILTIN_MOD_NAME "*builtin*"

#define PCALL_RES(RES) {                    \
	int result_ = (RES);                    \
	if (result_ != 0) {                     \
		scriptError(result_, __FUNCTION__); \
	}                                       \
}

#define runCallbacks(nargs, mode) \
	runCallbacksRaw((nargs), (mode), __FUNCTION__)

#define setOriginFromTable(index) \
	setOriginFromTableRaw(index, __FUNCTION__)

enum class ScriptingType: u8 {
	Async, // either mainmenu (client) or ingame (server)
	Client,
	MainMenu,
	Server,
	Emerge,
	PauseMenu,
};

class Server;
#if CHECK_CLIENT_BUILD()
class Client;
#endif
class EmergeThread;
class IGameDef;
class Environment;
class GUIEngine;
class ServerActiveObject;
struct PlayerHPChangeReason;

class ScriptApiBase : protected LuaHelper {
public:
	ScriptApiBase(ScriptingType type);
	// fake constructor to allow script API classes (e.g ScriptApiEnv) to virtually inherit from this one.
	ScriptApiBase()
	{
		FATAL_ERROR("ScriptApiBase created without ScriptingType!");
	}
	virtual ~ScriptApiBase();
	DISABLE_CLASS_COPY(ScriptApiBase);

	// These throw a ModError on failure
	void loadMod(const std::string &script_path, const std::string &mod_name);
	void loadScript(const std::string &script_path);

#if CHECK_CLIENT_BUILD()
	void loadModFromMemory(const std::string &mod_name);
#endif

	void runCallbacksRaw(int nargs,
		RunCallbacksMode mode, const char *fxn);

	/* object */
	void addObjectReference(ServerActiveObject *cobj);
	void removeObjectReference(ServerActiveObject *cobj);

	ScriptingType getType() { return m_type; }

	IGameDef *getGameDef() { return m_gamedef; }
	Server* getServer();
#if CHECK_CLIENT_BUILD()
	Client* getClient();
#endif

	// IMPORTANT: These cannot be used for any security-related uses, they exist
	// only to enrich error messages.
	const std::string &getOrigin() { return m_last_run_mod; }
	void setOriginDirect(const char *origin);
	void setOriginFromTableRaw(int index, const char *fxn);

	/**
	 * Returns the currently running mod, only during init time.
	 * The reason this is insecure is that mods can mess with each others code,
	 * so the boundary of who is responsible is fuzzy.
	 * @note Checking this against BUILTIN_MOD_NAME is always safe (not spoofable).
	 * @note See ScriptApiSecurity::getCurrentModName() for the secure equivalent.
	 * @return mod name or "" on error
	 */
	static std::string getCurrentModNameInsecure(lua_State *L);

#if !CHECK_CLIENT_BUILD()
	inline void clientOpenLibs(lua_State *L) { assert(false); }
#else
	void clientOpenLibs(lua_State *L);
#endif

	// Check things that should be set by the builtin mod.
	void checkSetByBuiltin();

protected:
	friend class LuaABM;
	friend class LuaLBM;
	friend class InvRef;
	friend class ObjectRef;
	friend class NodeMetaRef;
	friend class ModApiBase;
	friend class ModApiEnv;
	friend class LuaVoxelManip;
	friend class TestMoveAction; // needs getStack()

	/*
		Subtle edge case with coroutines: If for whatever reason you have a
		method in a subclass that's called from existing lua_CFunction
		(any of the l_*.cpp files) then make it static and take the lua_State*
		as an argument. This is REQUIRED because getStack() will not return the
		correct state if called inside coroutines.

		Also note that src/script/common/ is the better place for such helpers.
	*/
	lua_State* getStack()
		{ return m_luastack; }

	// Checks that stack size is sane
	void realityCheck();
	// Takes an error from lua_pcall and throws it as a LuaError
	void scriptError(int result, const char *fxn);
	// Dumps stack contents for debugging
	void stackDump(std::ostream &o);

	void setGameDef(IGameDef* gamedef) { m_gamedef = gamedef; }

	Environment* getEnv() { return m_environment; }
	void setEnv(Environment* env) { m_environment = env; }

#if CHECK_CLIENT_BUILD()
	GUIEngine* getGuiEngine() { return m_guiengine; }
	void setGuiEngine(GUIEngine* guiengine) { m_guiengine = guiengine; }
#endif

	EmergeThread* getEmergeThread() { return m_emerge; }
	void setEmergeThread(EmergeThread *emerge) { m_emerge = emerge; }

	void objectrefGetOrCreate(lua_State *L, ServerActiveObject *cobj);

	void pushPlayerHPChangeReason(lua_State *L, const PlayerHPChangeReason& reason);

	std::recursive_mutex m_luastackmutex;
	std::string     m_last_run_mod;

#ifdef SCRIPTAPI_LOCK_DEBUG
	int             m_lock_recursion_count{};
	std::thread::id m_owning_thread;
#endif

private:
	static int luaPanic(lua_State *L);

	lua_State      *m_luastack = nullptr;

	IGameDef       *m_gamedef = nullptr;
	Environment    *m_environment = nullptr;
#if CHECK_CLIENT_BUILD()
	GUIEngine      *m_guiengine = nullptr;
#endif
	EmergeThread   *m_emerge = nullptr;

	ScriptingType  m_type;
};
