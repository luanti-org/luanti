// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2013 celeron55, Perttu Ahola <celeron55@gmail.com>

#include "common/c_internal.h"
#include "cpp_api/s_security.h"
#include "util/numeric.h"
#include "debug.h"
#include "log.h"
#include "porting.h"
#include "settings.h"
#include <algorithm> // std::find
#include "common/c_types.h" // LuaError

std::string script_get_backtrace(lua_State *L)
{
	lua_getglobal(L, "debug");
	lua_getfield(L, -1, "traceback");
	lua_call(L, 0, 1);
	std::string result = luaL_checkstring(L, -1);
	lua_pop(L, 2);
	return result;
}

int script_exception_wrapper(lua_State *L, lua_CFunction f)
{
	try {
		return f(L);  // Call wrapped function and return result.
	} catch (const char *s) {  // Catch and convert exceptions.
		lua_pushstring(L, s);
	} catch (std::exception &e) {
		std::string e_descr = debug_describe_exc(e);
		lua_pushlstring(L, e_descr.c_str(), e_descr.size());
	}
	return lua_error(L);  // Rethrow as a Lua error.
}

int script_error_handler(lua_State *L)
{
	lua_getglobal(L, "core");
	lua_getfield(L, -1, "error_handler");
	if (!lua_isnil(L, -1)) {
		lua_pushvalue(L, 1);
	} else {
		// No Lua error handler available. Call debug.traceback(tostring(#1), level).
		lua_getglobal(L, "debug");
		lua_getfield(L, -1, "traceback");
		lua_getglobal(L, "tostring");
		lua_pushvalue(L, 1);
		lua_call(L, 1, 1);
	}
	lua_pushinteger(L, 2); // Stack level
	lua_call(L, 2, 1);
	return 1;
}

/*
 * Note that we can't get tracebacks for LUA_ERRMEM or LUA_ERRERR (without
 * hacking Lua internals).  For LUA_ERRMEM, this is because memory errors will
 * not execute the error handler, and by the time lua_pcall returns the
 * execution stack will have already been unwound.  For LUA_ERRERR, there was
 * another error while trying to generate a backtrace from a LUA_ERRRUN.  It is
 * presumed there is an error with the internal Lua state and thus not possible
 * to gather a coherent backtrace.  Realistically, the best we can do here is
 * print which C function performed the failing pcall.
 */
void script_error(lua_State *L, int pcall_result, const char *mod, const char *fxn)
{
	if (pcall_result == 0)
		return;

	const char *err_type;
	switch (pcall_result) {
	case LUA_ERRRUN:
		err_type = "Runtime";
		break;
	case LUA_ERRMEM:
		err_type = "OOM";
		break;
	case LUA_ERRERR:
		err_type = "Double fault";
		break;
	default:
		err_type = "Unknown";
	}

	if (!mod)
		mod = "??";

	if (!fxn)
		fxn = "??";

	const char *err_descr = lua_tostring(L, -1);
	if (!err_descr)
		err_descr = "<no description>";

	char buf[256];
	porting::mt_snprintf(buf, sizeof(buf), "%s error from mod '%s' in callback %s(): ",
		err_type, mod, fxn);

	std::string err_msg(buf);
	err_msg += err_descr;

	if (pcall_result == LUA_ERRMEM) {
		err_msg += "\nCurrent Lua memory usage: "
			+ itos(lua_gc(L, LUA_GCCOUNT, 0) >> 10) + " MB";
	}

	throw LuaError(err_msg);
}

[[nodiscard]] static std::string script_log_add_source(lua_State *L,
	std::string_view message, int stack_depth)
{
	std::string ret(message);
	if (stack_depth <= 0)
		return ret;

	lua_Debug ar;
	if (lua_getstack(L, stack_depth, &ar)) {
		FATAL_ERROR_IF(!lua_getinfo(L, "Sl", &ar), "lua_getinfo() failed");
		ret.append(" (at ");
		// Use the full path for files, only use the shortened source for strings
		ret.append(ar.source[0] == '@' ? &ar.source[1] : ar.short_src);
		ret.append(":" + std::to_string(ar.currentline) + ")");
	} else {
		ret.append(" (at ?:?)");
	}
	return ret;
}

bool script_log_unique(lua_State *L, std::string_view message_in, std::ostream &log_to,
	int stack_depth)
{
	thread_local std::vector<u64> logged_messages;

	auto message = script_log_add_source(L, message_in, stack_depth);
	u64 hash = murmur_hash_64_ua(message.data(), message.length(), 0xBADBABE);

	if (std::find(logged_messages.begin(), logged_messages.end(), hash)
			== logged_messages.end()) {

		logged_messages.emplace_back(hash);
		log_to << message << std::endl;
		return true;
	}
	return false;
}

DeprecatedHandlingMode get_deprecated_handling_mode()
{
	static thread_local bool configured = false;
	static thread_local DeprecatedHandlingMode ret = DeprecatedHandlingMode::Ignore;

	// Only read settings on first call
	if (!configured) {
		std::string value = g_settings->get("deprecated_lua_api_handling");
		if (value == "log") {
			ret = DeprecatedHandlingMode::Log;
		} else if (value == "error") {
			ret = DeprecatedHandlingMode::Error;
		}
		configured = true;
	}

	return ret;
}

void log_deprecated(lua_State *L, std::string_view message, int stack_depth, bool once)
{
	DeprecatedHandlingMode mode = get_deprecated_handling_mode();
	if (mode == DeprecatedHandlingMode::Ignore)
		return;

	bool log = true;
	if (once) {
		log = script_log_unique(L, message, warningstream, stack_depth);
	} else {
		auto message2 = script_log_add_source(L, message, stack_depth);
		warningstream << message2 << std::endl;
	}

	if (mode == DeprecatedHandlingMode::Error)
		throw LuaError(std::string(message));
	else if (log)
		infostream << script_get_backtrace(L) << std::endl;
}

void call_string_dump(lua_State *L, int idx)
{
	// Retrieve string.dump from untampered env
	ScriptApiSecurity::getGlobalsBackup(L);
	lua_getfield(L, -1, "string");
	lua_getfield(L, -1, "dump");
	lua_remove(L, -2); // remove _G
	lua_remove(L, -2); // remove 'string' table
	lua_pushvalue(L, idx);
	lua_call(L, 1, 1);
}
