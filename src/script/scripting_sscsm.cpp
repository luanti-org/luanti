// SPDX-FileCopyrightText: 2025 Luanti authors
//
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "scripting_sscsm.h"
#include "cpp_api/s_internal.h"
#include "lua_api/l_sscsm.h"
#include "lua_api/l_util.h"
#include "lua_api/l_client.h"

SSCSMScripting::SSCSMScripting(SSCSMEnvironment *env) :
	ScriptApiBase(ScriptingType::SSCSM) //TODO: use different CUSTOM_RIDX_ERROR_HANDLER, or set debug.traceback
{
	setSSCSMEnv(env);

	SCRIPTAPI_PRECHECKHEADER

	initializeSecuritySSCSM();

	lua_getglobal(L, "core");
	int top = lua_gettop(L);

	// Initialize our lua_api modules
	initializeModApi(L, top);
	lua_pop(L, 1);

	// Push builtin initialization type
	lua_pushstring(L, "sscsm");
	lua_setglobal(L, "INIT");

	// infostream << "SCRIPTAPI: Initialized SSCSM modules" << std::endl;
}

void SSCSMScripting::initializeModApi(lua_State *L, int top)
{
	ModApiUtil::InitializeClient(L, top); //TODO: probably needs an InitializeSSCSM
	ModApiClient::InitializeSSCSM(L, top);
	ModApiSSCSM::Initialize(L, top);
}
