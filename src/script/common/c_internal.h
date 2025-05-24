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

#include <string>
#include <string_view>

extern "C" {
#include <lua.h>
#include <lauxlib.h>
}

#include "config.h"
#include "util/enum_string.h"


/*
	Define our custom indices into the Lua registry table.

	Lua 5.2 and above define the LUA_RIDX_LAST macro. Only numbers above that
	may be used for custom indices, anything else is reserved.

	Lua 5.1 / LuaJIT do not use any numeric indices (only string indices),
	so we can use numeric indices freely.
*/
enum {
#ifdef LUA_RIDX_LAST
	CUSTOM_RIDX_BEFORE_ = LUA_RIDX_LAST,
#else
	CUSTOM_RIDX_BEFORE_ = 0,
#endif

	CUSTOM_RIDX_SCRIPTAPI,
	/// @warning don't use directly, `ScriptApiSecurity` has wrappers
	CUSTOM_RIDX_GLOBALS_BACKUP,
	CUSTOM_RIDX_CURRENT_MOD_NAME,
	CUSTOM_RIDX_ERROR_HANDLER,
	CUSTOM_RIDX_HTTP_API_LUA,
	CUSTOM_RIDX_METATABLE_MAP,

	// The following functions are implemented in Lua because LuaJIT can
	// trace them and optimize tables/string better than from the C API.
	CUSTOM_RIDX_READ_VECTOR,
	CUSTOM_RIDX_PUSH_VECTOR,
	CUSTOM_RIDX_READ_NODE,
	CUSTOM_RIDX_PUSH_NODE,
	CUSTOM_RIDX_PUSH_MOVERESULT1,
};


// Determine if CUSTOM_RIDX_SCRIPTAPI will hold a light or full userdata
#if defined(__aarch64__) && USE_LUAJIT
/* LuaJIT has a 47-bit limit for lightuserdata on this platform and we cannot
 * assume that the ScriptApi class was allocated at a fitting address. */
#define INDIRECT_SCRIPTAPI_RIDX 1
#else
#define INDIRECT_SCRIPTAPI_RIDX 0
#endif

// Pushes the error handler onto the stack and returns its index
#define PUSH_ERROR_HANDLER(L) \
	(lua_rawgeti((L), LUA_REGISTRYINDEX, CUSTOM_RIDX_ERROR_HANDLER), lua_gettop((L)))

#define PCALL_RESL(L, RES) {                            \
	int result_ = (RES);                                \
	if (result_ != 0) {                                 \
		script_error((L), result_, NULL, __FUNCTION__); \
	}                                                   \
}

// What script_run_callbacks does with the return values of callbacks.
// Regardless of the mode, if only one callback is defined,
// its return value is the total return value.
// Modes only affect the case where 0 or >= 2 callbacks are defined.
enum RunCallbacksMode
{
	// Returns the return value of the first callback
	// Returns nil if list of callbacks is empty
	RUN_CALLBACKS_MODE_FIRST,
	// Returns the return value of the last callback
	// Returns nil if list of callbacks is empty
	RUN_CALLBACKS_MODE_LAST,
	// If any callback returns a false value, the first such is returned
	// Otherwise, the first callback's return value (trueish) is returned
	// Returns true if list of callbacks is empty
	RUN_CALLBACKS_MODE_AND,
	// Like above, but stops calling callbacks (short circuit)
	// after seeing the first false value
	RUN_CALLBACKS_MODE_AND_SC,
	// If any callback returns a true value, the first such is returned
	// Otherwise, the first callback's return value (falseish) is returned
	// Returns false if list of callbacks is empty
	RUN_CALLBACKS_MODE_OR,
	// Like above, but stops calling callbacks (short circuit)
	// after seeing the first true value
	RUN_CALLBACKS_MODE_OR_SC,
	// Note: "a true value" and "a false value" refer to values that
	// are converted by readParam<bool> to true or false, respectively.
};

// Gets a backtrace of the current execution point
std::string script_get_backtrace(lua_State *L);
// Wrapper for CFunction calls that converts C++ exceptions to Lua errors
int script_exception_wrapper(lua_State *L, lua_CFunction f);
// Acts as the error handler for lua_pcall
int script_error_handler(lua_State *L);
// Takes an error from lua_pcall and throws it as a LuaError
void script_error(lua_State *L, int pcall_result, const char *mod, const char *fxn);

bool script_log_unique(lua_State *L, std::string_view message, std::ostream &log_to,
	int stack_depth = 1);

enum DeprecatedHandlingMode {
	Ignore,
	Log,
	Error
};

/**
 * Reads `deprecated_lua_api_handling` in settings, returns cached value.
 *
 * @return DeprecatedHandlingMode
 */
DeprecatedHandlingMode get_deprecated_handling_mode();

/**
 * Handles a deprecation warning based on user settings
 *
 * @param L Lua State
 * @param message The deprecation method
 * @param stack_depth How far on the stack to the first user function
 *        (ie: not builtin or core). -1 to disabled.
 * @param once Log the deprecation warning only once per callsite.
 */
void log_deprecated(lua_State *L, std::string_view message,
	int stack_depth = 1, bool once = false);

// Safely call string.dump on a function value
// (does not pop, leaves one value on stack)
void call_string_dump(lua_State *L, int idx);
