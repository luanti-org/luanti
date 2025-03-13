// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2013 celeron55, Perttu Ahola <celeron55@gmail.com>

#include "cpp_api/s_player.h"
#include "cpp_api/s_internal.h"
#include "common/c_converter.h"
#include "common/c_content.h"
#include "debug.h"
#include "inventorymanager.h"
#include "lua_api/l_inventory.h"
#include "lua_api/l_item.h"
#include "util/string.h"

void ScriptApiPlayer::on_newplayer(ServerActiveObject *player)
{
	SCRIPTAPI_PRECHECKHEADER

	// Get core.registered_on_newplayers
	lua_getglobal(L, "core");
	lua_getfield(L, -1, "registered_on_newplayers");
	// Call callbacks
	objectrefGetOrCreate(L, player);
	runCallbacks(1, RUN_CALLBACKS_MODE_FIRST);
}

void ScriptApiPlayer::on_dieplayer(ServerActiveObject *player, const PlayerHPChangeReason &reason)
{
	SCRIPTAPI_PRECHECKHEADER

	// Get callback table
	lua_getglobal(L, "core");
	lua_getfield(L, -1, "registered_on_dieplayers");

	// Push arguments
	objectrefGetOrCreate(L, player);
	pushPlayerHPChangeReason(L, reason);

	// Run callbacks
	runCallbacks(2, RUN_CALLBACKS_MODE_FIRST);
}

bool ScriptApiPlayer::on_punchplayer(ServerActiveObject *player,
		ServerActiveObject *hitter,
		float time_from_last_punch,
		const ToolCapabilities *toolcap,
		v3f dir,
		s32 damage)
{
	SCRIPTAPI_PRECHECKHEADER
	// Get core.registered_on_punchplayers
	lua_getglobal(L, "core");
	lua_getfield(L, -1, "registered_on_punchplayers");
	// Call callbacks
	objectrefGetOrCreate(L, player);
	if (hitter)
		objectrefGetOrCreate(L, hitter);
	else
		lua_pushnil(L);
	lua_pushnumber(L, time_from_last_punch);
	push_tool_capabilities(L, *toolcap);
	push_v3f(L, dir);
	lua_pushnumber(L, damage);
	runCallbacks(6, RUN_CALLBACKS_MODE_OR);
	return readParam<bool>(L, -1);
}

void ScriptApiPlayer::on_rightclickplayer(ServerActiveObject *player,
		ServerActiveObject *clicker)
{
	SCRIPTAPI_PRECHECKHEADER
	// Get core.registered_on_rightclickplayers
	lua_getglobal(L, "core");
	lua_getfield(L, -1, "registered_on_rightclickplayers");
	// Call callbacks
	objectrefGetOrCreate(L, player);
	objectrefGetOrCreate(L, clicker);
	runCallbacks(2, RUN_CALLBACKS_MODE_FIRST);
}

s32 ScriptApiPlayer::on_player_hpchange(ServerActiveObject *player,
	s32 hp_change, const PlayerHPChangeReason &reason)
{
	SCRIPTAPI_PRECHECKHEADER

	int error_handler = PUSH_ERROR_HANDLER(L);

	// Get core.registered_on_player_hpchange
	lua_getglobal(L, "core");
	lua_getfield(L, -1, "registered_on_player_hpchange");
	lua_remove(L, -2);

	// Push arguments
	objectrefGetOrCreate(L, player);
	lua_pushnumber(L, hp_change);
	pushPlayerHPChangeReason(L, reason);

	// Call callbacks
	PCALL_RES(lua_pcall(L, 3, 1, error_handler));
	hp_change = lua_tointeger(L, -1);
	lua_pop(L, 2); // Pop result and error handler
	return hp_change;
}

bool ScriptApiPlayer::on_respawnplayer(ServerActiveObject *player)
{
	SCRIPTAPI_PRECHECKHEADER

	// Get core.registered_on_respawnplayers
	lua_getglobal(L, "core");
	lua_getfield(L, -1, "registered_on_respawnplayers");
	// Call callbacks
	objectrefGetOrCreate(L, player);
	runCallbacks(1, RUN_CALLBACKS_MODE_OR);
	return readParam<bool>(L, -1);
}

bool ScriptApiPlayer::on_prejoinplayer(
	const std::string &name,
	const std::string &ip,
	std::string *reason)
{
	SCRIPTAPI_PRECHECKHEADER

	// Get core.registered_on_prejoinplayers
	lua_getglobal(L, "core");
	lua_getfield(L, -1, "registered_on_prejoinplayers");
	lua_pushstring(L, name.c_str());
	lua_pushstring(L, ip.c_str());
	runCallbacks(2, RUN_CALLBACKS_MODE_OR);
	if (lua_isstring(L, -1)) {
		reason->assign(readParam<std::string>(L, -1));
		return true;
	}
	return false;
}

bool ScriptApiPlayer::can_bypass_userlimit(const std::string &name, const std::string &ip)
{
	SCRIPTAPI_PRECHECKHEADER

	// Get core.registered_on_prejoinplayers
	lua_getglobal(L, "core");
	lua_getfield(L, -1, "registered_can_bypass_userlimit");
	lua_pushstring(L, name.c_str());
	lua_pushstring(L, ip.c_str());
	runCallbacks(2, RUN_CALLBACKS_MODE_OR);
	return lua_toboolean(L, -1);
}

void ScriptApiPlayer::on_joinplayer(ServerActiveObject *player, s64 last_login)
{
	SCRIPTAPI_PRECHECKHEADER

	// Get core.registered_on_joinplayers
	lua_getglobal(L, "core");
	lua_getfield(L, -1, "registered_on_joinplayers");
	// Call callbacks
	objectrefGetOrCreate(L, player);
	if (last_login != -1)
		lua_pushinteger(L, last_login);
	else
		lua_pushnil(L);
	runCallbacks(2, RUN_CALLBACKS_MODE_FIRST);
}

void ScriptApiPlayer::on_leaveplayer(ServerActiveObject *player,
		bool timeout)
{
	SCRIPTAPI_PRECHECKHEADER

	// Get core.registered_on_leaveplayers
	lua_getglobal(L, "core");
	lua_getfield(L, -1, "registered_on_leaveplayers");
	// Call callbacks
	objectrefGetOrCreate(L, player);
	lua_pushboolean(L, timeout);
	runCallbacks(2, RUN_CALLBACKS_MODE_FIRST);
}

void ScriptApiPlayer::on_cheat(ServerActiveObject *player,
		const std::string &cheat_type)
{
	SCRIPTAPI_PRECHECKHEADER

	// Get core.registered_on_cheats
	lua_getglobal(L, "core");
	lua_getfield(L, -1, "registered_on_cheats");
	// Call callbacks
	objectrefGetOrCreate(L, player);
	lua_newtable(L);
	lua_pushlstring(L, cheat_type.c_str(), cheat_type.size());
	lua_setfield(L, -2, "type");
	runCallbacks(2, RUN_CALLBACKS_MODE_FIRST);
}

void ScriptApiPlayer::on_playerReceiveFields(ServerActiveObject *player,
		const std::string &formname,
		const StringMap &fields)
{
	SCRIPTAPI_PRECHECKHEADER

	// Get core.registered_on_player_receive_fields
	lua_getglobal(L, "core");
	lua_getfield(L, -1, "registered_on_player_receive_fields");
	// Call callbacks
	// param 1
	objectrefGetOrCreate(L, player);
	// param 2
	lua_pushstring(L, formname.c_str());
	// param 3
	lua_newtable(L);
	StringMap::const_iterator it;
	for (it = fields.begin(); it != fields.end(); ++it) {
		const std::string &name = it->first;
		const std::string &value = it->second;
		lua_pushstring(L, name.c_str());
		lua_pushlstring(L, value.c_str(), value.size());
		lua_settable(L, -3);
	}
	runCallbacks(3, RUN_CALLBACKS_MODE_OR_SC);
}

void ScriptApiPlayer::on_authplayer(const std::string &name, const std::string &ip, bool is_success)
{
	SCRIPTAPI_PRECHECKHEADER

	// Get core.registered_on_authplayers
	lua_getglobal(L, "core");
	lua_getfield(L, -1, "registered_on_authplayers");

	// Call callbacks
	lua_pushstring(L, name.c_str());
	lua_pushstring(L, ip.c_str());
	lua_pushboolean(L, is_success);
	runCallbacks(3, RUN_CALLBACKS_MODE_FIRST);
}

void ScriptApiPlayer::pushMoveArguments(
		const MoveAction &ma, int count,
		ServerActiveObject *player)
{
	lua_State *L = getStack();
	objectrefGetOrCreate(L, player); // player
	lua_pushstring(L, "move");       // action
	InvRef::create(L, ma.from_inv);  // inventory
	lua_newtable(L);
	{
		// Table containing the action information
		lua_pushstring(L, ma.from_list.c_str());
		lua_setfield(L, -2, "from_list");
		lua_pushstring(L, ma.to_list.c_str());
		lua_setfield(L, -2, "to_list");

		lua_pushinteger(L, ma.from_i + 1);
		lua_setfield(L, -2, "from_index");
		lua_pushinteger(L, ma.to_i + 1);
		lua_setfield(L, -2, "to_index");

		lua_pushinteger(L, count);
		lua_setfield(L, -2, "count");
	}
}

void ScriptApiPlayer::pushPutTakeArguments(
		const char *method, const InventoryLocation &loc,
		const std::string &listname, int index, const ItemStack &stack,
		ServerActiveObject *player)
{
	lua_State *L = getStack();
	objectrefGetOrCreate(L, player); // player
	lua_pushstring(L, method);       // action
	InvRef::create(L, loc);          // inventory
	lua_newtable(L);
	{
		// Table containing the action information
		lua_pushstring(L, listname.c_str());
		lua_setfield(L, -2, "listname");

		lua_pushinteger(L, index + 1);
		lua_setfield(L, -2, "index");

		LuaItemStack::create(L, stack);
		lua_setfield(L, -2, "stack");
	}
}

// Return number of accepted items to be moved
int ScriptApiPlayer::player_inventory_AllowMove(
		const MoveAction &ma, int count,
		ServerActiveObject *player)
{
	SCRIPTAPI_PRECHECKHEADER

	lua_getglobal(L, "core");
	lua_getfield(L, -1, "registered_allow_player_inventory_actions");
	pushMoveArguments(ma, count, player);
	runCallbacks(4, RUN_CALLBACKS_MODE_OR_SC);

	return lua_type(L, -1) == LUA_TNUMBER ? lua_tonumber(L, -1) : count;
}

// Return number of accepted items to be put
int ScriptApiPlayer::player_inventory_AllowPut(
		const MoveAction &ma, const ItemStack &stack,
		ServerActiveObject *player)
{
	SCRIPTAPI_PRECHECKHEADER

	lua_getglobal(L, "core");
	lua_getfield(L, -1, "registered_allow_player_inventory_actions");
	pushPutTakeArguments("put", ma.to_inv, ma.to_list, ma.to_i, stack, player);
	runCallbacks(4, RUN_CALLBACKS_MODE_OR_SC);

	return lua_type(L, -1) == LUA_TNUMBER ? lua_tonumber(L, -1) : stack.count;
}

// Return number of accepted items to be taken
int ScriptApiPlayer::player_inventory_AllowTake(
		const MoveAction &ma, const ItemStack &stack,
		ServerActiveObject *player)
{
	SCRIPTAPI_PRECHECKHEADER

	lua_getglobal(L, "core");
	lua_getfield(L, -1, "registered_allow_player_inventory_actions");
	pushPutTakeArguments("take", ma.from_inv, ma.from_list, ma.from_i, stack, player);
	runCallbacks(4, RUN_CALLBACKS_MODE_OR_SC);

	return lua_type(L, -1) == LUA_TNUMBER ? lua_tonumber(L, -1) : stack.count;
}

// Report moved items
void ScriptApiPlayer::player_inventory_OnMove(
		const MoveAction &ma, int count,
		ServerActiveObject *player)
{
	SCRIPTAPI_PRECHECKHEADER

	lua_getglobal(L, "core");
	lua_getfield(L, -1, "registered_on_player_inventory_actions");
	pushMoveArguments(ma, count, player);
	runCallbacks(4, RUN_CALLBACKS_MODE_FIRST);
}

// Report put items
void ScriptApiPlayer::player_inventory_OnPut(
		const MoveAction &ma, const ItemStack &stack,
		ServerActiveObject *player)
{
	SCRIPTAPI_PRECHECKHEADER

	lua_getglobal(L, "core");
	lua_getfield(L, -1, "registered_on_player_inventory_actions");
	pushPutTakeArguments("put", ma.to_inv, ma.to_list, ma.to_i, stack, player);
	runCallbacks(4, RUN_CALLBACKS_MODE_FIRST);
}

// Report taken items
void ScriptApiPlayer::player_inventory_OnTake(
		const MoveAction &ma, const ItemStack &stack,
		ServerActiveObject *player)
{
	SCRIPTAPI_PRECHECKHEADER

	lua_getglobal(L, "core");
	lua_getfield(L, -1, "registered_on_player_inventory_actions");
	pushPutTakeArguments("take", ma.from_inv, ma.from_list, ma.from_i, stack, player);
	runCallbacks(4, RUN_CALLBACKS_MODE_FIRST);
}
