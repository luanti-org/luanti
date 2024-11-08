// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2013 celeron55, Perttu Ahola <celeron55@gmail.com>

#include "cpp_api/s_nodemeta.h"
#include "cpp_api/s_internal.h"
#include "common/c_converter.h"
#include "nodedef.h"
#include "mapnode.h"
#include "server.h"
#include "environment.h"
#include "lua_api/l_item.h"

// Return number of accepted items to be moved
int ScriptApiNodemeta::nodemeta_inventory_AllowMove(
		const MoveAction &ma, int count,
		ServerActiveObject *player)
{
	SCRIPTAPI_PRECHECKHEADER

	int error_handler = PUSH_ERROR_HANDLER(L);

	const NodeDefManager *ndef = getServer()->ndef();

	// If node doesn't exist, we don't know what callback to call
	MapNode node = getEnv()->getMap().getNode(ma.to_inv.p);
	if (node.getContent() == CONTENT_IGNORE)
		return 0;

	// Push callback function on stack
	const auto &nodename = ndef->get(node).name;
	if (!getItemCallback(nodename.c_str(), "allow_metadata_inventory_move", &ma.to_inv.p))
		return count;

	// function(pos, from_list, from_index, to_list, to_index, count, player)
	push_v3s16(L, ma.to_inv.p);              // pos
	lua_pushstring(L, ma.from_list.c_str()); // from_list
	lua_pushinteger(L, ma.from_i + 1);       // from_index
	lua_pushstring(L, ma.to_list.c_str());   // to_list
	lua_pushinteger(L, ma.to_i + 1);         // to_index
	lua_pushinteger(L, count);               // count
	objectrefGetOrCreate(L, player);         // player
	PCALL_RES(lua_pcall(L, 7, 1, error_handler));
	if (!lua_isnumber(L, -1))
		throw LuaError("allow_metadata_inventory_move should"
				" return a number. node=" + nodename);
	int num = luaL_checkinteger(L, -1);
	lua_pop(L, 2); // Pop integer and error handler
	return num;
}

// Return number of accepted items to be put
int ScriptApiNodemeta::nodemeta_inventory_AllowPut(
		const MoveAction &ma, const ItemStack &stack,
		ServerActiveObject *player)
{
	SCRIPTAPI_PRECHECKHEADER

	int error_handler = PUSH_ERROR_HANDLER(L);

	const NodeDefManager *ndef = getServer()->ndef();

	// If node doesn't exist, we don't know what callback to call
	MapNode node = getEnv()->getMap().getNode(ma.to_inv.p);
	if (node.getContent() == CONTENT_IGNORE)
		return 0;

	// Push callback function on stack
	const auto &nodename = ndef->get(node).name;
	if (!getItemCallback(nodename.c_str(), "allow_metadata_inventory_put", &ma.to_inv.p))
		return stack.count;

	// Call function(pos, listname, index, stack, player)
	push_v3s16(L, ma.to_inv.p);            // pos
	lua_pushstring(L, ma.to_list.c_str()); // listname
	lua_pushinteger(L, ma.to_i + 1);       // index
	LuaItemStack::create(L, stack);        // stack
	objectrefGetOrCreate(L, player);       // player
	PCALL_RES(lua_pcall(L, 5, 1, error_handler));
	if(!lua_isnumber(L, -1))
		throw LuaError("allow_metadata_inventory_put should"
				" return a number. node=" + nodename);
	int num = luaL_checkinteger(L, -1);
	lua_pop(L, 2); // Pop integer and error handler
	return num;
}

// Return number of accepted items to be taken
int ScriptApiNodemeta::nodemeta_inventory_AllowTake(
		const MoveAction &ma, const ItemStack &stack,
		ServerActiveObject *player)
{
	SCRIPTAPI_PRECHECKHEADER

	int error_handler = PUSH_ERROR_HANDLER(L);

	const NodeDefManager *ndef = getServer()->ndef();

	// If node doesn't exist, we don't know what callback to call
	MapNode node = getEnv()->getMap().getNode(ma.from_inv.p);
	if (node.getContent() == CONTENT_IGNORE)
		return 0;

	// Push callback function on stack
	const auto &nodename = ndef->get(node).name;
	if (!getItemCallback(nodename.c_str(), "allow_metadata_inventory_take", &ma.from_inv.p))
		return stack.count;

	// Call function(pos, listname, index, count, player)
	push_v3s16(L, ma.from_inv.p);            // pos
	lua_pushstring(L, ma.from_list.c_str()); // listname
	lua_pushinteger(L, ma.from_i + 1);       // index
	LuaItemStack::create(L, stack);          // stack
	objectrefGetOrCreate(L, player);         // player
	PCALL_RES(lua_pcall(L, 5, 1, error_handler));
	if (!lua_isnumber(L, -1))
		throw LuaError("allow_metadata_inventory_take should"
				" return a number. node=" + nodename);
	int num = luaL_checkinteger(L, -1);
	lua_pop(L, 2); // Pop integer and error handler
	return num;
}

// Report moved items
void ScriptApiNodemeta::nodemeta_inventory_OnMove(
		const MoveAction &ma, int count,
		ServerActiveObject *player)
{
	SCRIPTAPI_PRECHECKHEADER

	int error_handler = PUSH_ERROR_HANDLER(L);

	const NodeDefManager *ndef = getServer()->ndef();

	// If node doesn't exist, we don't know what callback to call
	MapNode node = getEnv()->getMap().getNode(ma.from_inv.p);
	if (node.getContent() == CONTENT_IGNORE)
		return;

	// Push callback function on stack
	const auto &nodename = ndef->get(node).name;
	if (!getItemCallback(nodename.c_str(), "on_metadata_inventory_move", &ma.from_inv.p))
		return;

	// function(pos, from_list, from_index, to_list, to_index, count, player)
	push_v3s16(L, ma.from_inv.p);            // pos
	lua_pushstring(L, ma.from_list.c_str()); // from_list
	lua_pushinteger(L, ma.from_i + 1);       // from_index
	lua_pushstring(L, ma.to_list.c_str());   // to_list
	lua_pushinteger(L, ma.to_i + 1);         // to_index
	lua_pushinteger(L, count);               // count
	objectrefGetOrCreate(L, player);         // player
	PCALL_RES(lua_pcall(L, 7, 0, error_handler));
	lua_pop(L, 1);  // Pop error handler
}

// Report put items
void ScriptApiNodemeta::nodemeta_inventory_OnPut(
		const MoveAction &ma, const ItemStack &stack,
		ServerActiveObject *player)
{
	SCRIPTAPI_PRECHECKHEADER

	int error_handler = PUSH_ERROR_HANDLER(L);

	const NodeDefManager *ndef = getServer()->ndef();

	// If node doesn't exist, we don't know what callback to call
	MapNode node = getEnv()->getMap().getNode(ma.to_inv.p);
	if (node.getContent() == CONTENT_IGNORE)
		return;

	// Push callback function on stack
	const auto &nodename = ndef->get(node).name;
	if (!getItemCallback(nodename.c_str(), "on_metadata_inventory_put", &ma.to_inv.p))
		return;

	// Call function(pos, listname, index, stack, player)
	push_v3s16(L, ma.to_inv.p);            // pos
	lua_pushstring(L, ma.to_list.c_str()); // listname
	lua_pushinteger(L, ma.to_i + 1);       // index
	LuaItemStack::create(L, stack);        // stack
	objectrefGetOrCreate(L, player);       // player
	PCALL_RES(lua_pcall(L, 5, 0, error_handler));
	lua_pop(L, 1);  // Pop error handler
}

// Report taken items
void ScriptApiNodemeta::nodemeta_inventory_OnTake(
		const MoveAction &ma, const ItemStack &stack,
		ServerActiveObject *player)
{
	SCRIPTAPI_PRECHECKHEADER

	int error_handler = PUSH_ERROR_HANDLER(L);

	const NodeDefManager *ndef = getServer()->ndef();

	// If node doesn't exist, we don't know what callback to call
	MapNode node = getEnv()->getMap().getNode(ma.from_inv.p);
	if (node.getContent() == CONTENT_IGNORE)
		return;

	// Push callback function on stack
	const auto &nodename = ndef->get(node).name;
	if (!getItemCallback(nodename.c_str(), "on_metadata_inventory_take", &ma.from_inv.p))
		return;

	// Call function(pos, listname, index, stack, player)
	push_v3s16(L, ma.from_inv.p);            // pos
	lua_pushstring(L, ma.from_list.c_str()); // listname
	lua_pushinteger(L, ma.from_i + 1);       // index
	LuaItemStack::create(L, stack);          // stack
	objectrefGetOrCreate(L, player);         // player
	PCALL_RES(lua_pcall(L, 5, 0, error_handler));
	lua_pop(L, 1);  // Pop error handler
}
