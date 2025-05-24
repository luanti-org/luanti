// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2013 celeron55, Perttu Ahola <celeron55@gmail.com>

#include "lua_api/l_inventory.h"
#include "lua_api/l_internal.h"
#include "lua_api/l_item.h"
#include "common/c_converter.h"
#include "common/c_content.h"
#include "server.h"
#include "server/serverinventorymgr.h"
#include "remoteplayer.h"

/*
	InvRef
*/

Inventory* InvRef::getinv(lua_State *L, InvRef *ref)
{
	return getServerInventoryMgr(L)->getInventory(ref->m_loc);
}

InventoryList* InvRef::getlist(lua_State *L, InvRef *ref,
		const char *listname)
{
	NO_MAP_LOCK_REQUIRED;
	Inventory *inv = getinv(L, ref);
	if(!inv)
		return NULL;
	return inv->getList(listname);
}

void InvRef::reportInventoryChange(lua_State *L, InvRef *ref)
{
	// Inform other things that the inventory has changed
	getServerInventoryMgr(L)->setInventoryModified(ref->m_loc);
}

// Exported functions

// garbage collector
int InvRef::gc_object(lua_State *L) {
	InvRef *o = *(InvRef **)(lua_touserdata(L, 1));
	delete o;
	return 0;
}

// is_empty(self, listname) -> true/false
int InvRef::l_is_empty(lua_State *L)
{
	NO_MAP_LOCK_REQUIRED;
	InvRef *ref = checkObject<InvRef>(L, 1);
	const char *listname = luaL_checkstring(L, 2);
	InventoryList *list = getlist(L, ref, listname);
	if(list && list->getUsedSlots() > 0){
		lua_pushboolean(L, false);
	} else {
		lua_pushboolean(L, true);
	}
	return 1;
}

// get_size(self, listname)
int InvRef::l_get_size(lua_State *L)
{
	NO_MAP_LOCK_REQUIRED;
	InvRef *ref = checkObject<InvRef>(L, 1);
	const char *listname = luaL_checkstring(L, 2);
	InventoryList *list = getlist(L, ref, listname);
	if(list){
		lua_pushinteger(L, list->getSize());
	} else {
		lua_pushinteger(L, 0);
	}
	return 1;
}

// get_width(self, listname)
int InvRef::l_get_width(lua_State *L)
{
	NO_MAP_LOCK_REQUIRED;
	InvRef *ref = checkObject<InvRef>(L, 1);
	const char *listname = luaL_checkstring(L, 2);
	InventoryList *list = getlist(L, ref, listname);
	if(list){
		lua_pushinteger(L, list->getWidth());
	} else {
		lua_pushinteger(L, 0);
	}
	return 1;
}

// set_size(self, listname, size)
int InvRef::l_set_size(lua_State *L)
{
	NO_MAP_LOCK_REQUIRED;
	InvRef *ref = checkObject<InvRef>(L, 1);
	const char *listname = luaL_checkstring(L, 2);
	Inventory *inv;
	InventoryList *list;

	int newsize = luaL_checknumber(L, 3);
	if (newsize < 0)
		goto fail;

	inv = getinv(L, ref);
	if (!inv)
		goto fail;

	if (newsize == 0) {
		inv->deleteList(listname);
		goto done;
	}

	list = inv->getList(listname);
	if (list) {
		list->setSize(newsize);
	} else {
		list = inv->addList(listname, newsize);
		if (!list)
			goto fail;
	}
done:
	reportInventoryChange(L, ref);
	lua_pushboolean(L, true);
	return 1;
fail:
	lua_pushboolean(L, false);
	return 1;
}

// set_width(self, listname, size)
int InvRef::l_set_width(lua_State *L)
{
	NO_MAP_LOCK_REQUIRED;
	InvRef *ref = checkObject<InvRef>(L, 1);
	const char *listname = luaL_checkstring(L, 2);
	Inventory *inv;
	InventoryList *list;

	int newwidth = luaL_checknumber(L, 3);
	if (newwidth < 0)
		goto fail;

	inv = getinv(L, ref);
	if (!inv)
		goto fail;

	list = inv->getList(listname);
	if (!list)
		goto fail;

	list->setWidth(newwidth);
	reportInventoryChange(L, ref);
	lua_pushboolean(L, true);
	return 1;
fail:
	lua_pushboolean(L, false);
	return 1;
}

// get_stack(self, listname, i) -> itemstack
int InvRef::l_get_stack(lua_State *L)
{
	NO_MAP_LOCK_REQUIRED;
	InvRef *ref = checkObject<InvRef>(L, 1);
	const char *listname = luaL_checkstring(L, 2);
	int i = luaL_checknumber(L, 3) - 1;
	InventoryList *list = getlist(L, ref, listname);
	ItemStack item;
	if(list != NULL && i >= 0 && i < (int) list->getSize())
		item = list->getItem(i);
	LuaItemStack::create(L, item);
	return 1;
}

// set_stack(self, listname, i, stack) -> true/false
int InvRef::l_set_stack(lua_State *L)
{
	NO_MAP_LOCK_REQUIRED;
	InvRef *ref = checkObject<InvRef>(L, 1);
	const char *listname = luaL_checkstring(L, 2);
	int i = luaL_checknumber(L, 3) - 1;
	ItemStack newitem = read_item(L, 4, getServer(L)->idef());
	InventoryList *list = getlist(L, ref, listname);
	if(list != NULL && i >= 0 && i < (int) list->getSize()){
		list->changeItem(i, newitem);
		reportInventoryChange(L, ref);
		lua_pushboolean(L, true);
	} else {
		lua_pushboolean(L, false);
	}
	return 1;
}

// get_list(self, listname) -> list or nil
int InvRef::l_get_list(lua_State *L)
{
	NO_MAP_LOCK_REQUIRED;
	InvRef *ref = checkObject<InvRef>(L, 1);
	const char *listname = luaL_checkstring(L, 2);
	Inventory *inv = getinv(L, ref);
	if (!inv) {
		lua_pushnil(L);
		return 1;
	}
	InventoryList *invlist = inv->getList(listname);
	if (!invlist) {
		lua_pushnil(L);
		return 1;
	}
	push_inventory_list(L, *invlist);
	return 1;
}

// set_list(self, listname, list)
int InvRef::l_set_list(lua_State *L)
{
	NO_MAP_LOCK_REQUIRED;
	InvRef *ref = checkObject<InvRef>(L, 1);
	const char *listname = luaL_checkstring(L, 2);
	Inventory *inv = getinv(L, ref);
	if(inv == NULL){
		return 0;
	}
	InventoryList *list = inv->getList(listname);
	if(list)
		read_inventory_list(L, 3, inv, listname,
				getServer(L), list->getSize());
	else
		read_inventory_list(L, 3, inv, listname, getServer(L));
	reportInventoryChange(L, ref);
	return 0;
}

// get_lists(self) -> table that maps listnames to InventoryLists
int InvRef::l_get_lists(lua_State *L)
{
	NO_MAP_LOCK_REQUIRED;
	InvRef *ref = checkObject<InvRef>(L, 1);
	Inventory *inv = getinv(L, ref);
	if (!inv) {
		return 0;
	}
	push_inventory_lists(L, *inv);
	return 1;
}

// set_lists(self, lists)
int InvRef::l_set_lists(lua_State *L)
{
	NO_MAP_LOCK_REQUIRED;
	InvRef *ref = checkObject<InvRef>(L, 1);
	Inventory *inv = getinv(L, ref);
	if (!inv) {
		return 0;
	}

	// Make a temporary inventory in case reading fails
	Inventory tempInv(*inv);
	tempInv.clear();

	Server *server = getServer(L);

	lua_pushnil(L);
	luaL_checktype(L, 2, LUA_TTABLE);
	while (lua_next(L, 2)) {
		const char *listname = luaL_checkstring(L, -2);
		read_inventory_list(L, -1, &tempInv, listname, server);
		lua_pop(L, 1);
	}
	*inv = tempInv;
	return 0;
}

// add_item(self, listname, itemstack or itemstring or table or nil) -> itemstack
// Returns the leftover stack
int InvRef::l_add_item(lua_State *L)
{
	NO_MAP_LOCK_REQUIRED;
	InvRef *ref = checkObject<InvRef>(L, 1);
	const char *listname = luaL_checkstring(L, 2);
	ItemStack item = read_item(L, 3, getServer(L)->idef());
	InventoryList *list = getlist(L, ref, listname);
	if(list){
		ItemStack leftover = list->addItem(item);
		if(leftover.count != item.count)
			reportInventoryChange(L, ref);
		LuaItemStack::create(L, leftover);
	} else {
		LuaItemStack::create(L, item);
	}
	return 1;
}

// room_for_item(self, listname, itemstack or itemstring or table or nil) -> true/false
// Returns true if the item completely fits into the list
int InvRef::l_room_for_item(lua_State *L)
{
	NO_MAP_LOCK_REQUIRED;
	InvRef *ref = checkObject<InvRef>(L, 1);
	const char *listname = luaL_checkstring(L, 2);
	ItemStack item = read_item(L, 3, getServer(L)->idef());
	InventoryList *list = getlist(L, ref, listname);
	if(list){
		lua_pushboolean(L, list->roomForItem(item));
	} else {
		lua_pushboolean(L, false);
	}
	return 1;
}

// contains_item(self, listname, itemstack or itemstring or table or nil, [match_meta]) -> true/false
// Returns true if the list contains the given count of the given item
int InvRef::l_contains_item(lua_State *L)
{
	NO_MAP_LOCK_REQUIRED;
	InvRef *ref = checkObject<InvRef>(L, 1);
	const char *listname = luaL_checkstring(L, 2);
	ItemStack item = read_item(L, 3, getServer(L)->idef());
	InventoryList *list = getlist(L, ref, listname);
	bool match_meta = readParam<bool>(L, 4, false);
	if (list) {
		lua_pushboolean(L, list->containsItem(item, match_meta));
	} else {
		lua_pushboolean(L, false);
	}
	return 1;
}

// remove_item(self, listname, itemstack or itemstring or table or nil, [match_meta]) -> itemstack
// Returns the items that were actually removed
int InvRef::l_remove_item(lua_State *L)
{
	NO_MAP_LOCK_REQUIRED;
	InvRef *ref = checkObject<InvRef>(L, 1);
	const char *listname = luaL_checkstring(L, 2);
	ItemStack item = read_item(L, 3, getServer(L)->idef());
	InventoryList *list = getlist(L, ref, listname);
	bool match_meta = readParam<bool>(L, 4, false);
	if(list){
		ItemStack removed = list->removeItem(item, match_meta);
		if(!removed.empty())
			reportInventoryChange(L, ref);
		LuaItemStack::create(L, removed);
	} else {
		LuaItemStack::create(L, ItemStack());
	}
	return 1;
}

// get_location() -> location (like get_inventory(location))
int InvRef::l_get_location(lua_State *L)
{
	NO_MAP_LOCK_REQUIRED;
	InvRef *ref = checkObject<InvRef>(L, 1);
	const InventoryLocation &loc = ref->m_loc;
	switch(loc.type){
	case InventoryLocation::PLAYER:
		lua_newtable(L);
		lua_pushstring(L, "player");
		lua_setfield(L, -2, "type");
		lua_pushstring(L, loc.name.c_str());
		lua_setfield(L, -2, "name");
		return 1;
	case InventoryLocation::NODEMETA:
		lua_newtable(L);
		lua_pushstring(L, "node");
		lua_setfield(L, -2, "type");
		push_v3s16(L, loc.p);
		lua_setfield(L, -2, "pos");
		return 1;
	case InventoryLocation::DETACHED:
		lua_newtable(L);
		lua_pushstring(L, "detached");
		lua_setfield(L, -2, "type");
		lua_pushstring(L, loc.name.c_str());
		lua_setfield(L, -2, "name");
		return 1;
	case InventoryLocation::UNDEFINED:
	case InventoryLocation::CURRENT_PLAYER:
		break;
	}
	lua_newtable(L);
	lua_pushstring(L, "undefined");
	lua_setfield(L, -2, "type");
	return 1;
}


InvRef::InvRef(const InventoryLocation &loc):
	m_loc(loc)
{
}

// Creates an InvRef and leaves it on top of stack
// Not callable from Lua; all references are created on the C side.
void InvRef::create(lua_State *L, const InventoryLocation &loc)
{
	NO_MAP_LOCK_REQUIRED;
	InvRef *o = new InvRef(loc);
	*(void **)(lua_newuserdata(L, sizeof(void *))) = o;
	luaL_getmetatable(L, className);
	lua_setmetatable(L, -2);
}

void InvRef::Register(lua_State *L)
{
	static const luaL_Reg metamethods[] = {
		{"__gc", gc_object},
		{0, 0}
	};
	registerClass<InvRef>(L, methods, metamethods);

	// Cannot be created from Lua
	//lua_register(L, className, create_object);
}

const char InvRef::className[] = "InvRef";
const luaL_Reg InvRef::methods[] = {
	luamethod(InvRef, is_empty),
	luamethod(InvRef, get_size),
	luamethod(InvRef, set_size),
	luamethod(InvRef, get_width),
	luamethod(InvRef, set_width),
	luamethod(InvRef, get_stack),
	luamethod(InvRef, set_stack),
	luamethod(InvRef, get_list),
	luamethod(InvRef, set_list),
	luamethod(InvRef, get_lists),
	luamethod(InvRef, set_lists),
	luamethod(InvRef, add_item),
	luamethod(InvRef, room_for_item),
	luamethod(InvRef, contains_item),
	luamethod(InvRef, remove_item),
	luamethod(InvRef, get_location),
	{0,0}
};

// get_inventory(location)
int ModApiInventory::l_get_inventory(lua_State *L)
{
	InventoryLocation loc;

	lua_getfield(L, 1, "type");
	std::string type = luaL_checkstring(L, -1);
	lua_pop(L, 1);

	if(type == "node"){
		MAP_LOCK_REQUIRED;
		lua_getfield(L, 1, "pos");
		v3s16 pos = check_v3s16(L, -1);
		loc.setNodeMeta(pos);

		if (getServerInventoryMgr(L)->getInventory(loc) != NULL)
			InvRef::create(L, loc);
		else
			lua_pushnil(L);
		return 1;
	}

	NO_MAP_LOCK_REQUIRED;
	if (type == "player") {
		lua_getfield(L, 1, "name");
		loc.setPlayer(luaL_checkstring(L, -1));
		lua_pop(L, 1);
	} else if (type == "detached") {
		lua_getfield(L, 1, "name");
		loc.setDetached(luaL_checkstring(L, -1));
		lua_pop(L, 1);
	}

	if (getServerInventoryMgr(L)->getInventory(loc) != NULL)
		InvRef::create(L, loc);
	else
		lua_pushnil(L);
	return 1;
	// END NO_MAP_LOCK_REQUIRED;

}

// create_detached_inventory_raw(name, [player_name])
int ModApiInventory::l_create_detached_inventory_raw(lua_State *L)
{
	NO_MAP_LOCK_REQUIRED;
	const char *name = luaL_checkstring(L, 1);
	std::string player = readParam<std::string>(L, 2, "");
	if (getServerInventoryMgr(L)->createDetachedInventory(name, getServer(L)->idef(), player) != NULL) {
		InventoryLocation loc;
		loc.setDetached(name);
		InvRef::create(L, loc);
	} else {
		lua_pushnil(L);
	}
	return 1;
}

// remove_detached_inventory_raw(name)
int ModApiInventory::l_remove_detached_inventory_raw(lua_State *L)
{
	NO_MAP_LOCK_REQUIRED;
	const std::string &name = luaL_checkstring(L, 1);
	lua_pushboolean(L, getServerInventoryMgr(L)->removeDetachedInventory(name));
	return 1;
}

void ModApiInventory::Initialize(lua_State *L, int top)
{
	API_FCT(create_detached_inventory_raw);
	API_FCT(remove_detached_inventory_raw);
	API_FCT(get_inventory);
}
