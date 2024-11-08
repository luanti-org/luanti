// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2013 celeron55, Perttu Ahola <celeron55@gmail.com>

#include "cpp_api/s_server.h"
#include "cpp_api/s_internal.h"
#include "common/c_converter.h"
#include "util/numeric.h" // myrand

bool ScriptApiServer::getAuth(const std::string &playername,
		std::string *dst_password,
		std::set<std::string> *dst_privs,
		s64 *dst_last_login)
{
	SCRIPTAPI_PRECHECKHEADER

	int error_handler = PUSH_ERROR_HANDLER(L);
	getAuthHandler();
	lua_getfield(L, -1, "get_auth");
	if (lua_type(L, -1) != LUA_TFUNCTION)
		throw LuaError("Authentication handler missing get_auth");
	lua_pushstring(L, playername.c_str());
	PCALL_RES(lua_pcall(L, 1, 1, error_handler));
	lua_remove(L, -2); // Remove auth handler
	lua_remove(L, error_handler);

	// nil = login not allowed
	if (lua_isnil(L, -1))
		return false;
	luaL_checktype(L, -1, LUA_TTABLE);

	std::string password;
	if (!getstringfield(L, -1, "password", password))
		throw LuaError("Authentication handler didn't return password");
	if (dst_password)
		*dst_password = password;

	lua_getfield(L, -1, "privileges");
	if (!lua_istable(L, -1))
		throw LuaError("Authentication handler didn't return privilege table");
	if (dst_privs)
		readPrivileges(-1, *dst_privs);
	lua_pop(L, 1);  // Remove key from privs table

	s64 last_login;
	if(!getintfield(L, -1, "last_login", last_login))
		throw LuaError("Authentication handler didn't return last_login");
	if (dst_last_login)
		*dst_last_login = (s64)last_login;

	return true;
}

void ScriptApiServer::getAuthHandler()
{
	lua_State *L = getStack();

	lua_getglobal(L, "core");
	lua_getfield(L, -1, "registered_auth_handler");
	if (lua_isnil(L, -1)){
		lua_pop(L, 1);
		lua_getfield(L, -1, "builtin_auth_handler");
	}

	setOriginFromTable(-1);

	lua_remove(L, -2); // Remove core
	if (lua_type(L, -1) != LUA_TTABLE)
		throw LuaError("Authentication handler table not valid");
}

void ScriptApiServer::readPrivileges(int index, std::set<std::string> &result)
{
	lua_State *L = getStack();

	result.clear();
	lua_pushnil(L);
	if (index < 0)
		index -= 1;
	while (lua_next(L, index) != 0) {
		// key at index -2 and value at index -1
		std::string key = luaL_checkstring(L, -2);
		bool value = readParam<bool>(L, -1);
		if (value)
			result.insert(key);
		// removes value, keeps key for next iteration
		lua_pop(L, 1);
	}
}

void ScriptApiServer::createAuth(const std::string &playername,
		const std::string &password)
{
	SCRIPTAPI_PRECHECKHEADER

	int error_handler = PUSH_ERROR_HANDLER(L);
	getAuthHandler();
	lua_getfield(L, -1, "create_auth");
	lua_remove(L, -2); // Remove auth handler
	if (lua_type(L, -1) != LUA_TFUNCTION)
		throw LuaError("Authentication handler missing create_auth");
	lua_pushstring(L, playername.c_str());
	lua_pushstring(L, password.c_str());
	PCALL_RES(lua_pcall(L, 2, 0, error_handler));
	lua_pop(L, 1); // Pop error handler
}

bool ScriptApiServer::setPassword(const std::string &playername,
		const std::string &password)
{
	SCRIPTAPI_PRECHECKHEADER

	int error_handler = PUSH_ERROR_HANDLER(L);
	getAuthHandler();
	lua_getfield(L, -1, "set_password");
	lua_remove(L, -2); // Remove auth handler
	if (lua_type(L, -1) != LUA_TFUNCTION)
		throw LuaError("Authentication handler missing set_password");
	lua_pushstring(L, playername.c_str());
	lua_pushstring(L, password.c_str());
	PCALL_RES(lua_pcall(L, 2, 1, error_handler));
	lua_remove(L, error_handler);
	return lua_toboolean(L, -1);
}

bool ScriptApiServer::on_chat_message(const std::string &name,
		const std::string &message)
{
	SCRIPTAPI_PRECHECKHEADER

	// Get core.registered_on_chat_messages
	lua_getglobal(L, "core");
	lua_getfield(L, -1, "registered_on_chat_messages");
	// Call callbacks
	lua_pushstring(L, name.c_str());
	lua_pushstring(L, message.c_str());
	runCallbacks(2, RUN_CALLBACKS_MODE_OR_SC);
	return readParam<bool>(L, -1);
}

void ScriptApiServer::on_mods_loaded()
{
	SCRIPTAPI_PRECHECKHEADER

	// Get registered shutdown hooks
	lua_getglobal(L, "core");
	lua_getfield(L, -1, "registered_on_mods_loaded");
	// Call callbacks
	runCallbacks(0, RUN_CALLBACKS_MODE_FIRST);
}

void ScriptApiServer::on_shutdown()
{
	SCRIPTAPI_PRECHECKHEADER

	// Get registered shutdown hooks
	lua_getglobal(L, "core");
	lua_getfield(L, -1, "registered_on_shutdown");
	// Call callbacks
	runCallbacks(0, RUN_CALLBACKS_MODE_FIRST);
}

std::string ScriptApiServer::formatChatMessage(const std::string &name,
	const std::string &message)
{
	SCRIPTAPI_PRECHECKHEADER

	// Push function onto stack
	lua_getglobal(L, "core");
	lua_getfield(L, -1, "format_chat_message");

	// Push arguments onto stack
	lua_pushstring(L, name.c_str());
	lua_pushstring(L, message.c_str());

	// Actually call the function
	lua_call(L, 2, 1);

	// Fetch return value
	std::string ret = lua_tostring(L, -1);
	lua_pop(L, 1);

	return ret;
}

u32 ScriptApiServer::allocateDynamicMediaCallback(lua_State *L, int f_idx)
{
	if (f_idx < 0)
		f_idx = lua_gettop(L) + f_idx + 1;

	lua_getglobal(L, "core");
	lua_getfield(L, -1, "dynamic_media_callbacks");
	luaL_checktype(L, -1, LUA_TTABLE);

	// Find a randomly generated token that doesn't exist yet
	int tries = 100;
	u32 token;
	while (1) {
		token = myrand();
		lua_rawgeti(L, -2, token);
		bool is_free = lua_isnil(L, -1);
		lua_pop(L, 1);
		if (is_free)
			break;
		if (--tries < 0)
			FATAL_ERROR("Ran out of callbacks IDs?!");
	}

	// core.dynamic_media_callbacks[token] = callback_func
	lua_pushvalue(L, f_idx);
	lua_rawseti(L, -2, token);

	lua_pop(L, 2);

	verbosestream << "allocateDynamicMediaCallback() = " << token << std::endl;
	return token;
}

void ScriptApiServer::freeDynamicMediaCallback(u32 token)
{
	SCRIPTAPI_PRECHECKHEADER

	verbosestream << "freeDynamicMediaCallback(" << token << ")" << std::endl;

	// core.dynamic_media_callbacks[token] = nil
	lua_getglobal(L, "core");
	lua_getfield(L, -1, "dynamic_media_callbacks");
	luaL_checktype(L, -1, LUA_TTABLE);
	lua_pushnil(L);
	lua_rawseti(L, -2, token);
	lua_pop(L, 2);
}

void ScriptApiServer::on_dynamic_media_added(u32 token, const std::string &playername)
{
	SCRIPTAPI_PRECHECKHEADER

	int error_handler = PUSH_ERROR_HANDLER(L);
	lua_getglobal(L, "core");
	lua_getfield(L, -1, "dynamic_media_callbacks");
	luaL_checktype(L, -1, LUA_TTABLE);
	lua_rawgeti(L, -1, token);
	luaL_checktype(L, -1, LUA_TFUNCTION);

	lua_pushstring(L, playername.c_str());
	PCALL_RES(lua_pcall(L, 1, 0, error_handler));
}
