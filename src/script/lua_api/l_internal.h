// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2013 celeron55, Perttu Ahola <celeron55@gmail.com>

/******************************************************************************/
/******************************************************************************/
/* WARNING!!!! do NOT add this header in any include file or any code file    */
/*             not being a modapi file!!!!!!!!                                */
/******************************************************************************/
/******************************************************************************/

#pragma once

#include "common/c_internal.h"

#define luamethod(class, name) {#name, class::l_##name}

#define luamethod_dep(class, good, bad)                                     \
		{#bad, [](lua_State *L) -> int {                                    \
			return l_deprecated_function(L, #good, #bad, &class::l_##good); \
		}}

#define luamethod_aliased(class, good, bad) \
		luamethod(class, good),               \
		luamethod_dep(class, good, bad)

#define API_FCT(name) registerFunction(L, #name, l_##name, top)

// For future use
#define MAP_LOCK_REQUIRED ((void)0)
#define NO_MAP_LOCK_REQUIRED ((void)0)

/* In debug mode ensure no code tries to retrieve the server env when it isn't
 * actually available (in CSM) */
#if CHECK_CLIENT_BUILD() && !defined(NDEBUG)
#define DEBUG_ASSERT_NO_CLIENTAPI                    \
	FATAL_ERROR_IF(getClient(L) != nullptr, "Tried " \
		"to retrieve ServerEnvironment on client")
#else
#define DEBUG_ASSERT_NO_CLIENTAPI ((void)0)
#endif

// Retrieve ServerEnvironment pointer as `env` (no map lock)
#define GET_ENV_PTR_NO_MAP_LOCK                              \
	DEBUG_ASSERT_NO_CLIENTAPI;                               \
	ServerEnvironment *env = (ServerEnvironment *)getEnv(L); \
	if (env == NULL)                                         \
		return 0

// Retrieve ServerEnvironment pointer as `env`
#define GET_ENV_PTR         \
	MAP_LOCK_REQUIRED;      \
	GET_ENV_PTR_NO_MAP_LOCK

// Retrieve Environment pointer as `env` (no map lock)
#define GET_PLAIN_ENV_PTR_NO_MAP_LOCK            \
	Environment *env = getEnv(L);                \
	if (env == NULL)                             \
		return 0

// Retrieve Environment pointer as `env`
#define GET_PLAIN_ENV_PTR         \
	MAP_LOCK_REQUIRED;            \
	GET_PLAIN_ENV_PTR_NO_MAP_LOCK
