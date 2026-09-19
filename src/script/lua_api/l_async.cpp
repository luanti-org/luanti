// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "lua_api/l_internal.h"
#include "lua_api/l_async.h"
#include "cpp_api/s_async.h"

// do_async_callback(func, params, mod_origin)
int ModApiAsync::l_do_async_callback(lua_State *L)
{
	NO_MAP_LOCK_REQUIRED;
	ScriptApiAsync *script = getScriptApi<ScriptApiAsync>(L);

	luaL_checktype(L, 1, LUA_TFUNCTION);
	luaL_checktype(L, 2, LUA_TTABLE);
	luaL_checktype(L, 3, LUA_TSTRING);

	auto serialized_func = dump_function_to_string(L, 1);
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
