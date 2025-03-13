// Minetest
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "lua_api/l_ipc.h"
#include "lua_api/l_internal.h"
#include "common/c_packer.h"
#include "server.h"
#include "debug.h"
#include <chrono>

typedef std::shared_lock<std::shared_mutex> SharedReadLock;
typedef std::unique_lock<std::shared_mutex> SharedWriteLock;

static inline auto read_pv(lua_State *L, int idx)
{
	std::unique_ptr<PackedValue> ret;
	if (!lua_isnil(L, idx)) {
		ret.reset(script_pack(L, idx));
		if (ret->contains_userdata)
			throw LuaError("Userdata not allowed");
	}
	return ret;
}

int ModApiIPC::l_ipc_get(lua_State *L)
{
	auto *store = getGameDef(L)->getModIPCStore();

	auto key = readParam<std::string>(L, 1);

	{
		SharedReadLock autolock(store->mutex);
		auto it = store->map.find(key);
		if (it == store->map.end())
			lua_pushnil(L);
		else
			script_unpack(L, it->second.get());
	}
	return 1;
}

int ModApiIPC::l_ipc_set(lua_State *L)
{
	auto *store = getGameDef(L)->getModIPCStore();

	auto key = readParam<std::string>(L, 1);

	luaL_checkany(L, 2);
	auto pv = read_pv(L, 2);

	{
		SharedWriteLock autolock(store->mutex);
		if (pv)
			store->map[key] = std::move(pv);
		else
			store->map.erase(key); // delete the map value for nil
	}
	store->signal();
	return 0;
}

int ModApiIPC::l_ipc_cas(lua_State *L)
{
	auto *store = getGameDef(L)->getModIPCStore();

	auto key = readParam<std::string>(L, 1);

	luaL_checkany(L, 2);
	const int idx_old = 2;

	luaL_checkany(L, 3);
	auto pv_new = read_pv(L, 3);

	bool ok = false;
	{
		SharedWriteLock autolock(store->mutex);
		// unpack and compare old value
		auto it = store->map.find(key);
		if (it == store->map.end()) {
			ok = lua_isnil(L, idx_old);
		} else {
			script_unpack(L, it->second.get());
			ok = lua_equal(L, idx_old, -1);
			lua_pop(L, 1);
		}
		// put new value
		if (ok) {
			if (pv_new)
				store->map[key] = std::move(pv_new);
			else
				store->map.erase(key);
		}
	}

	if (ok)
		store->signal();
	lua_pushboolean(L, ok);
	return 1;
}

int ModApiIPC::l_ipc_poll(lua_State *L)
{
	auto *store = getGameDef(L)->getModIPCStore();

	auto key = readParam<std::string>(L, 1);

	auto timeout = std::chrono::milliseconds(
		std::max<int>(0, luaL_checkinteger(L, 2))
	);

	bool ret;
	{
		SharedReadLock autolock(store->mutex);

		// wait until value exists or timeout
		ret = store->condvar.wait_for(autolock, timeout, [&] () -> bool {
			return store->map.count(key) != 0;
		});
	}

	lua_pushboolean(L, ret);
	return 1;
}

/*
 * Implementation note:
 * Iterating over the IPC table is intentionally not supported.
 * Mods should know what they have set.
 * This has the nice side effect that mods are able to use a randomly generated key
 * if they really *really* want to avoid other code touching their data.
 */

void ModApiIPC::Initialize(lua_State *L, int top)
{
	FATAL_ERROR_IF(!getGameDef(L)->getModIPCStore(), "ModIPCStore missing from gamedef");

	API_FCT(ipc_get);
	API_FCT(ipc_set);
	API_FCT(ipc_cas);
	API_FCT(ipc_poll);
}
