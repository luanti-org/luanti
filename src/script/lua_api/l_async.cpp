// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "lua_api/l_internal.h"
#include "lua_api/l_async.h"
#include "cpp_api/s_async.h"

static std::string get_serialized_function(lua_State *L, int index)
{
	luaL_checktype(L, index, LUA_TFUNCTION);
	call_string_dump(L, index);
	size_t func_length;
	const char *serialized_func_raw = lua_tolstring(L, -1, &func_length);
	std::string serialized_func(serialized_func_raw, func_length);
	lua_pop(L, 1);
	return serialized_func;
}

// do_async_callback(func, params, mod_origin)
int ModApiAsync::l_do_async_callback(lua_State *L)
{
	NO_MAP_LOCK_REQUIRED;
	ScriptApiAsync *script = getScriptApi<ScriptApiAsync>(L);

	luaL_checktype(L, 2, LUA_TTABLE);
	luaL_checktype(L, 3, LUA_TSTRING);

	auto serialized_func = get_serialized_function(L, 1);
	PackedValue *param = script_pack(L, 2);
	std::string mod_origin = readParam<std::string>(L, 3);

	u32 jobId = script->queueAsync(
		std::move(serialized_func),
		param, mod_origin);

	lua_pushinteger(L, jobId);
	return 1;
}

// cancel_async_callback(id)
int ModApiAsync::l_cancel_async_callback(lua_State *L)
{
	NO_MAP_LOCK_REQUIRED;
	ScriptApiAsync *script = getScriptApi<ScriptApiAsync>(L);
	u32 id = luaL_checkinteger(L, 1);
	lua_pushboolean(L, script->cancelAsync(id));
	return 1;
}

// get_async_capacity()
int ModApiAsync::l_get_async_threading_capacity(lua_State *L)
{
	NO_MAP_LOCK_REQUIRED;
	ScriptApiAsync *script = getScriptApi<ScriptApiAsync>(L);
	lua_pushinteger(L, script->getThreadingCapacity());
	return 1;
}

void ModApiAsync::Initialize(lua_State *L, int top)
{
	API_FCT(do_async_callback);
	API_FCT(cancel_async_callback);
	API_FCT(get_async_threading_capacity);
}
