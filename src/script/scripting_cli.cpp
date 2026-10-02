// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2013 celeron55, Perttu Ahola <celeron55@gmail.com>

#include "scripting_cli.h"
#include "cpp_api/s_internal.h"
#include "lua_api/l_base.h"
#include "lua_api/l_http.h"
#include "lua_api/l_util.h"
#include "lua_api/l_settings.h"
#include "common/c_converter.h"
#include "log.h"
#include "filesys.h"
#include "porting.h"

extern "C" {
#include "lualib.h"
}

CLIScripting::CLIScripting() :
	ScriptApiBase(ScriptingType::CLI)
{
	SCRIPTAPI_PRECHECKHEADER

	initializeSecurity();

	lua_getglobal(L, "core");
	int top = lua_gettop(L);

	initializeModApi(L, top);
	lua_pop(L, 1);

	lua_pushstring(L, "cli");
	lua_setglobal(L, "INIT");

	infostream << "Initialized Lua environment for CLI." << std::endl;

	// Initialize async environment
	// this doesn't work yet
	//asyncEngine.initialize(0, "async", true);
}

void CLIScripting::loadBuiltin()
{
	auto path = ScriptApiBase::getBuiltinLuaPath() + DIR_DELIM "init.lua";
	loadMod(path, BUILTIN_MOD_NAME);
	checkSetByBuiltin();
}

int CLIScripting::run(const char *code)
{
	auto *L = getStack();

	m_error_caught = false;

	if (luaL_loadstring(L, code) != 0) {
		errorstream << lua_tostring(L, -1) << std::endl;
		return 1;
	}
	if (lua_pcall(L, 0, 0, 0) != 0) {
		errorstream << lua_tostring(L, -1) << std::endl;
		return 1;
	}

	return m_error_caught ? 2 : 0;
}

void CLIScripting::initializeModApi(lua_State *L, int top)
{
	registerLuaClasses(L, top);

	// Initialize mod API modules
	ModApiUtil::Initialize(L, top);
	ModApiHttp::Initialize(L, top);

	lua_pushcfunction(L, [](lua_State *L) -> int {
		int n = luaL_checkinteger(L, -1);
		sleep_ms(MYMAX(n, 0));
		return 0;
	});
	lua_setfield(L, top, "sleep");

	asyncEngine.registerStateInitializer(registerLuaClasses);
	asyncEngine.registerStateInitializer(ModApiUtil::InitializeAsync);
	asyncEngine.registerStateInitializer(ModApiHttp::InitializeAsync);
}

void CLIScripting::registerLuaClasses(lua_State *L, int top)
{
	LuaSettings::Register(L);
}

void CLIScripting::reportAsyncError(const std::string &msg)
{
	errorstream << msg << std::endl;
	m_error_caught = true;
}
