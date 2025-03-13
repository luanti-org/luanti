// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2013 celeron55, Perttu Ahola <celeron55@gmail.com>

#include "cpp_api/s_inventory.h"
#include "cpp_api/s_internal.h"
#include "inventorymanager.h"
#include "lua_api/l_inventory.h"
#include "lua_api/l_item.h"
#include "log.h"

// Return number of accepted items to be moved
int ScriptApiDetached::detached_inventory_AllowMove(
		const MoveAction &ma, int count,
		ServerActiveObject *player)
{
	SCRIPTAPI_PRECHECKHEADER

	int error_handler = PUSH_ERROR_HANDLER(L);

	// Push callback function on stack
	if (!getDetachedInventoryCallback(ma.from_inv.name, "allow_move"))
		return count;

	// function(inv, from_list, from_index, to_list, to_index, count, player)
	// inv
	InvRef::create(L, ma.from_inv);
	lua_pushstring(L, ma.from_list.c_str()); // from_list
	lua_pushinteger(L, ma.from_i + 1);       // from_index
	lua_pushstring(L, ma.to_list.c_str());   // to_list
	lua_pushinteger(L, ma.to_i + 1);         // to_index
	lua_pushinteger(L, count);               // count
	objectrefGetOrCreate(L, player);         // player
	PCALL_RES(lua_pcall(L, 7, 1, error_handler));
	if(!lua_isnumber(L, -1))
		throw LuaError("allow_move should return a number. name=" + ma.from_inv.name);
	int ret = luaL_checkinteger(L, -1);
	lua_pop(L, 2); // Pop integer and error handler
	return ret;
}

// Return number of accepted items to be put
int ScriptApiDetached::detached_inventory_AllowPut(
		const MoveAction &ma, const ItemStack &stack,
		ServerActiveObject *player)
{
	SCRIPTAPI_PRECHECKHEADER

	int error_handler = PUSH_ERROR_HANDLER(L);

	// Push callback function on stack
	if (!getDetachedInventoryCallback(ma.to_inv.name, "allow_put"))
		return stack.count; // All will be accepted

	// Call function(inv, listname, index, stack, player)
	InvRef::create(L, ma.to_inv);          // inv
	lua_pushstring(L, ma.to_list.c_str()); // listname
	lua_pushinteger(L, ma.to_i + 1);       // index
	LuaItemStack::create(L, stack);      // stack
	objectrefGetOrCreate(L, player);     // player
	PCALL_RES(lua_pcall(L, 5, 1, error_handler));
	if (!lua_isnumber(L, -1))
		throw LuaError("allow_put should return a number. name=" + ma.to_inv.name);
	int ret = luaL_checkinteger(L, -1);
	lua_pop(L, 2); // Pop integer and error handler
	return ret;
}

// Return number of accepted items to be taken
int ScriptApiDetached::detached_inventory_AllowTake(
		const MoveAction &ma, const ItemStack &stack,
		ServerActiveObject *player)
{
	SCRIPTAPI_PRECHECKHEADER

	int error_handler = PUSH_ERROR_HANDLER(L);

	// Push callback function on stack
	if (!getDetachedInventoryCallback(ma.from_inv.name, "allow_take"))
		return stack.count; // All will be accepted

	// Call function(inv, listname, index, stack, player)
	InvRef::create(L, ma.from_inv);          // inv
	lua_pushstring(L, ma.from_list.c_str()); // listname
	lua_pushinteger(L, ma.from_i + 1);       // index
	LuaItemStack::create(L, stack);      // stack
	objectrefGetOrCreate(L, player);     // player
	PCALL_RES(lua_pcall(L, 5, 1, error_handler));
	if (!lua_isnumber(L, -1))
		throw LuaError("allow_take should return a number. name=" + ma.from_inv.name);
	int ret = luaL_checkinteger(L, -1);
	lua_pop(L, 2); // Pop integer and error handler
	return ret;
}

// Report moved items
void ScriptApiDetached::detached_inventory_OnMove(
		const MoveAction &ma, int count,
		ServerActiveObject *player)
{
	SCRIPTAPI_PRECHECKHEADER

	int error_handler = PUSH_ERROR_HANDLER(L);

	// Push callback function on stack
	if (!getDetachedInventoryCallback(ma.from_inv.name, "on_move"))
		return;

	// function(inv, from_list, from_index, to_list, to_index, count, player)
	// inv
	InvRef::create(L, ma.from_inv);
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
void ScriptApiDetached::detached_inventory_OnPut(
		const MoveAction &ma, const ItemStack &stack,
		ServerActiveObject *player)
{
	SCRIPTAPI_PRECHECKHEADER

	int error_handler = PUSH_ERROR_HANDLER(L);

	// Push callback function on stack
	if (!getDetachedInventoryCallback(ma.to_inv.name, "on_put"))
		return;

	// Call function(inv, listname, index, stack, player)
	// inv
	InvRef::create(L, ma.to_inv);
	lua_pushstring(L, ma.to_list.c_str()); // listname
	lua_pushinteger(L, ma.to_i + 1);       // index
	LuaItemStack::create(L, stack);        // stack
	objectrefGetOrCreate(L, player);       // player
	PCALL_RES(lua_pcall(L, 5, 0, error_handler));
	lua_pop(L, 1);  // Pop error handler
}

// Report taken items
void ScriptApiDetached::detached_inventory_OnTake(
		const MoveAction &ma, const ItemStack &stack,
		ServerActiveObject *player)
{
	SCRIPTAPI_PRECHECKHEADER

	int error_handler = PUSH_ERROR_HANDLER(L);

	// Push callback function on stack
	if (!getDetachedInventoryCallback(ma.from_inv.name, "on_take"))
		return;

	// Call function(inv, listname, index, stack, player)
	// inv
	InvRef::create(L, ma.from_inv);
	lua_pushstring(L, ma.from_list.c_str()); // listname
	lua_pushinteger(L, ma.from_i + 1);       // index
	LuaItemStack::create(L, stack);          // stack
	objectrefGetOrCreate(L, player);         // player
	PCALL_RES(lua_pcall(L, 5, 0, error_handler));
	lua_pop(L, 1);  // Pop error handler
}

// Retrieves core.detached_inventories[name][callbackname]
// If that is nil or on error, return false and stack is unchanged
// If that is a function, returns true and pushes the
// function onto the stack
bool ScriptApiDetached::getDetachedInventoryCallback(
		const std::string &name, const char *callbackname)
{
	lua_State *L = getStack();

	lua_getglobal(L, "core");
	lua_getfield(L, -1, "detached_inventories");
	lua_remove(L, -2);
	luaL_checktype(L, -1, LUA_TTABLE);
	lua_getfield(L, -1, name.c_str());
	lua_remove(L, -2);
	// Should be a table
	if (lua_type(L, -1) != LUA_TTABLE) {
		errorstream<<"Detached inventory \""<<name<<"\" not defined"<<std::endl;
		lua_pop(L, 1);
		return false;
	}

	setOriginFromTable(-1);

	lua_getfield(L, -1, callbackname);
	lua_remove(L, -2);
	// Should be a function or nil
	if (lua_type(L, -1) == LUA_TFUNCTION) {
		return true;
	}

	if (lua_isnil(L, -1)) {
		lua_pop(L, 1);
		return false;
	}

	errorstream << "Detached inventory \"" << name << "\" callback \""
		<< callbackname << "\" is not a function" << std::endl;
	lua_pop(L, 1);
	return false;
}
