// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2013 celeron55, Perttu Ahola <celeron55@gmail.com>

#include "lua_api/l_item.h"
#include "lua_api/l_itemstackmeta.h"
#include "lua_api/l_internal.h"
#include "common/c_converter.h"
#include "common/c_content.h"
#include "common/c_packer.h"
#include "itemdef.h"
#include "nodedef.h"
#include "server.h"
#include "inventory.h"
#include "log.h"


// garbage collector
int LuaItemStack::gc_object(lua_State *L)
{
	LuaItemStack *o = *(LuaItemStack **)(lua_touserdata(L, 1));
	o->drop();
	return 0;
}

// __tostring metamethod
int LuaItemStack::mt_tostring(lua_State *L)
{
	LuaItemStack *o = checkObject<LuaItemStack>(L, 1);
	std::string itemstring = o->m_stack.getItemString(false);
	lua_pushfstring(L, "ItemStack(\"%s\")", itemstring.c_str());
	return 1;
}

// is_empty(self) -> true/false
int LuaItemStack::l_is_empty(lua_State *L)
{
	NO_MAP_LOCK_REQUIRED;
	LuaItemStack *o = checkObject<LuaItemStack>(L, 1);
	ItemStack &item = o->m_stack;
	lua_pushboolean(L, item.empty());
	return 1;
}

// get_name(self) -> string
int LuaItemStack::l_get_name(lua_State *L)
{
	NO_MAP_LOCK_REQUIRED;
	LuaItemStack *o = checkObject<LuaItemStack>(L, 1);
	ItemStack &item = o->m_stack;
	lua_pushstring(L, item.name.c_str());
	return 1;
}

// set_name(self, name)
int LuaItemStack::l_set_name(lua_State *L)
{
	NO_MAP_LOCK_REQUIRED;
	LuaItemStack *o = checkObject<LuaItemStack>(L, 1);
	ItemStack &item = o->m_stack;

	bool status = true;
	item.name = luaL_checkstring(L, 2);
	if (item.name.empty() || item.empty()) {
		item.clear();
		status = false;
	}

	lua_pushboolean(L, status);
	return 1;
}

// get_count(self) -> number
int LuaItemStack::l_get_count(lua_State *L)
{
	NO_MAP_LOCK_REQUIRED;
	LuaItemStack *o = checkObject<LuaItemStack>(L, 1);
	ItemStack &item = o->m_stack;
	lua_pushinteger(L, item.count);
	return 1;
}

// set_count(self, number)
int LuaItemStack::l_set_count(lua_State *L)
{
	NO_MAP_LOCK_REQUIRED;
	LuaItemStack *o = checkObject<LuaItemStack>(L, 1);
	ItemStack &item = o->m_stack;

	bool status;
	lua_Integer count = luaL_checkinteger(L, 2);
	if (count > 0 && count <= 65535) {
		item.count = count;
		status = true;
	} else {
		item.clear();
		status = false;
	}

	lua_pushboolean(L, status);
	return 1;
}

// get_wear(self) -> number
int LuaItemStack::l_get_wear(lua_State *L)
{
	NO_MAP_LOCK_REQUIRED;
	LuaItemStack *o = checkObject<LuaItemStack>(L, 1);
	ItemStack &item = o->m_stack;
	lua_pushinteger(L, item.wear);
	return 1;
}

// set_wear(self, number)
int LuaItemStack::l_set_wear(lua_State *L)
{
	NO_MAP_LOCK_REQUIRED;
	LuaItemStack *o = checkObject<LuaItemStack>(L, 1);
	ItemStack &item = o->m_stack;

	bool status;
	lua_Integer wear = luaL_checkinteger(L, 2);
	if (wear <= 65535) {
		item.wear = wear;
		status = true;
	} else {
		item.clear();
		status = false;
	}

	lua_pushboolean(L, status);
	return 1;
}

// get_meta(self) -> string
int LuaItemStack::l_get_meta(lua_State *L)
{
	NO_MAP_LOCK_REQUIRED;
	LuaItemStack *o = checkObject<LuaItemStack>(L, 1);
	ItemStackMetaRef::create(L, o);
	return 1;
}

// DEPRECATED
// get_metadata(self) -> string
int LuaItemStack::l_get_metadata(lua_State *L)
{
	NO_MAP_LOCK_REQUIRED;
	LuaItemStack *o = checkObject<LuaItemStack>(L, 1);
	ItemStack &item = o->m_stack;

	log_deprecated(L, "ItemStack:get_metadata is deprecated", 1, true);

	const std::string &value = item.metadata.getString("");
	lua_pushlstring(L, value.c_str(), value.size());
	return 1;
}

// DEPRECATED
// set_metadata(self, string)
int LuaItemStack::l_set_metadata(lua_State *L)
{
	NO_MAP_LOCK_REQUIRED;
	LuaItemStack *o = checkObject<LuaItemStack>(L, 1);
	ItemStack &item = o->m_stack;

	log_deprecated(L, "ItemStack:set_metadata is deprecated", 1, true);

	size_t len = 0;
	const char *ptr = luaL_checklstring(L, 2, &len);
	item.metadata.setString("", std::string(ptr, len));

	lua_pushboolean(L, true);
	return 1;
}

// get_description(self)
int LuaItemStack::l_get_description(lua_State *L)
{
	NO_MAP_LOCK_REQUIRED;
	LuaItemStack *o = checkObject<LuaItemStack>(L, 1);
	std::string desc = o->m_stack.getDescription(getGameDef(L)->idef());
	lua_pushstring(L, desc.c_str());
	return 1;
}

// get_short_description(self)
int LuaItemStack::l_get_short_description(lua_State *L)
{
	NO_MAP_LOCK_REQUIRED;
	LuaItemStack *o = checkObject<LuaItemStack>(L, 1);
	std::string desc = o->m_stack.getShortDescription(getGameDef(L)->idef());
	lua_pushstring(L, desc.c_str());
	return 1;
}

// clear(self) -> true
int LuaItemStack::l_clear(lua_State *L)
{
	NO_MAP_LOCK_REQUIRED;
	LuaItemStack *o = checkObject<LuaItemStack>(L, 1);
	o->m_stack.clear();
	lua_pushboolean(L, true);
	return 1;
}

// replace(self, itemstack or itemstring or table or nil) -> true
int LuaItemStack::l_replace(lua_State *L)
{
	NO_MAP_LOCK_REQUIRED;
	LuaItemStack *o = checkObject<LuaItemStack>(L, 1);
	o->m_stack = read_item(L, 2, getGameDef(L)->idef());
	lua_pushboolean(L, true);
	return 1;
}

// to_string(self) -> string
int LuaItemStack::l_to_string(lua_State *L)
{
	NO_MAP_LOCK_REQUIRED;
	LuaItemStack *o = checkObject<LuaItemStack>(L, 1);
	std::string itemstring = o->m_stack.getItemString();
	lua_pushstring(L, itemstring.c_str());
	return 1;
}

// to_table(self) -> table or nil
int LuaItemStack::l_to_table(lua_State *L)
{
	NO_MAP_LOCK_REQUIRED;
	LuaItemStack *o = checkObject<LuaItemStack>(L, 1);
	const ItemStack &item = o->m_stack;
	if(item.empty())
	{
		lua_pushnil(L);
	}
	else
	{
		lua_newtable(L);
		lua_pushstring(L, item.name.c_str());
		lua_setfield(L, -2, "name");
		lua_pushinteger(L, item.count);
		lua_setfield(L, -2, "count");
		lua_pushinteger(L, item.wear);
		lua_setfield(L, -2, "wear");

		const std::string &metadata_str = item.metadata.getString("");
		lua_pushlstring(L, metadata_str.c_str(), metadata_str.size());
		lua_setfield(L, -2, "metadata");

		lua_newtable(L);
		const StringMap &fields = item.metadata.getStrings();
		for (const auto &field : fields) {
			const std::string &name = field.first;
			if (name.empty())
				continue;
			const std::string &value = field.second;
			lua_pushlstring(L, name.c_str(), name.size());
			lua_pushlstring(L, value.c_str(), value.size());
			lua_settable(L, -3);
		}
		lua_setfield(L, -2, "meta");
	}
	return 1;
}

// get_stack_max(self) -> number
int LuaItemStack::l_get_stack_max(lua_State *L)
{
	NO_MAP_LOCK_REQUIRED;
	LuaItemStack *o = checkObject<LuaItemStack>(L, 1);
	ItemStack &item = o->m_stack;
	lua_pushinteger(L, item.getStackMax(getGameDef(L)->idef()));
	return 1;
}

// get_free_space(self) -> number
int LuaItemStack::l_get_free_space(lua_State *L)
{
	NO_MAP_LOCK_REQUIRED;
	LuaItemStack *o = checkObject<LuaItemStack>(L, 1);
	ItemStack &item = o->m_stack;
	lua_pushinteger(L, item.freeSpace(getGameDef(L)->idef()));
	return 1;
}

// is_known(self) -> true/false
// Checks if the item is defined.
int LuaItemStack::l_is_known(lua_State *L)
{
	NO_MAP_LOCK_REQUIRED;
	LuaItemStack *o = checkObject<LuaItemStack>(L, 1);
	ItemStack &item = o->m_stack;
	bool is_known = item.isKnown(getGameDef(L)->idef());
	lua_pushboolean(L, is_known);
	return 1;
}

// get_definition(self) -> table
// Returns the item definition table from registered_items,
// or a fallback one (name="unknown")
int LuaItemStack::l_get_definition(lua_State *L)
{
	NO_MAP_LOCK_REQUIRED;
	LuaItemStack *o = checkObject<LuaItemStack>(L, 1);
	ItemStack &item = o->m_stack;

	// Get registered_items[name]
	lua_getglobal(L, "core");
	lua_getfield(L, -1, "registered_items");
	luaL_checktype(L, -1, LUA_TTABLE);
	lua_getfield(L, -1, item.name.c_str());
	if(lua_isnil(L, -1))
	{
		lua_pop(L, 1);
		lua_getfield(L, -1, "unknown");
	}
	return 1;
}

// get_tool_capabilities(self) -> table
// Returns the effective tool digging properties.
// Returns those of the hand ("") if this item has none associated.
int LuaItemStack::l_get_tool_capabilities(lua_State *L)
{
	NO_MAP_LOCK_REQUIRED;
	LuaItemStack *o = checkObject<LuaItemStack>(L, 1);
	ItemStack &item = o->m_stack;
	const ToolCapabilities &prop =
		item.getToolCapabilities(getGameDef(L)->idef());
	push_tool_capabilities(L, prop);
	return 1;
}

// add_wear(self, amount) -> true/false
// The range for "amount" is [0,65536]. Wear is only added if the item
// is a tool. Adding wear might destroy the item.
// Returns true if the item is (or was) a tool.
int LuaItemStack::l_add_wear(lua_State *L)
{
	NO_MAP_LOCK_REQUIRED;
	LuaItemStack *o = checkObject<LuaItemStack>(L, 1);
	ItemStack &item = o->m_stack;
	int amount = lua_tointeger(L, 2);
	bool result = item.addWear(amount, getGameDef(L)->idef());
	lua_pushboolean(L, result);
	return 1;
}

// add_wear_by_uses(self, max_uses) -> true/false
// The range for "max_uses" is [0,65536].
// Adds wear to the item in such a way that, if
// only this function is called to add wear, the item
// will be destroyed exactly after `max_uses` times of calling it.
// No-op if `max_uses` is 0 or item is not a tool.
// Returns true if the item is (or was) a tool.
int LuaItemStack::l_add_wear_by_uses(lua_State *L)
{
	NO_MAP_LOCK_REQUIRED;
	LuaItemStack *o = checkObject<LuaItemStack>(L, 1);
	ItemStack &item = o->m_stack;
	u32 max_uses = readParam<int>(L, 2);
	u32 add_wear = calculateResultWear(max_uses, item.wear);
	bool result = item.addWear(add_wear, getGameDef(L)->idef());
	lua_pushboolean(L, result);
	return 1;
}

// get_wear_bar_params(self) -> table
// Returns the effective wear bar parameters.
// Returns nil if this item has none associated.
int LuaItemStack::l_get_wear_bar_params(lua_State *L)
{
	NO_MAP_LOCK_REQUIRED;
	LuaItemStack *o = checkObject<LuaItemStack>(L, 1);
	ItemStack &item = o->m_stack;
	auto params = item.getWearBarParams(getGameDef(L)->idef());
	if (params.has_value()) {
		push_wear_bar_params(L, *params);
		return 1;
	}
	return 0;
}

// add_item(self, itemstack or itemstring or table or nil) -> itemstack
// Returns leftover item stack
int LuaItemStack::l_add_item(lua_State *L)
{
	NO_MAP_LOCK_REQUIRED;
	LuaItemStack *o = checkObject<LuaItemStack>(L, 1);
	ItemStack &item = o->m_stack;
	ItemStack newitem = read_item(L, -1, getGameDef(L)->idef());
	ItemStack leftover = item.addItem(newitem, getGameDef(L)->idef());
	create(L, leftover);
	return 1;
}

// item_fits(self, itemstack or itemstring or table or nil) -> true/false, itemstack
// First return value is true iff the new item fits fully into the stack
// Second return value is the would-be-left-over item stack
int LuaItemStack::l_item_fits(lua_State *L)
{
	NO_MAP_LOCK_REQUIRED;
	LuaItemStack *o = checkObject<LuaItemStack>(L, 1);
	ItemStack &item = o->m_stack;
	ItemStack newitem = read_item(L, 2, getGameDef(L)->idef());
	ItemStack restitem;
	bool fits = item.itemFits(newitem, &restitem, getGameDef(L)->idef());
	lua_pushboolean(L, fits);  // first return value
	create(L, restitem);       // second return value
	return 2;
}

// take_item(self, takecount=1) -> itemstack
int LuaItemStack::l_take_item(lua_State *L)
{
	NO_MAP_LOCK_REQUIRED;
	LuaItemStack *o = checkObject<LuaItemStack>(L, 1);
	ItemStack &item = o->m_stack;
	u32 takecount = 1;
	if(!lua_isnone(L, 2))
		takecount = luaL_checkinteger(L, 2);
	ItemStack taken = item.takeItem(takecount);
	create(L, taken);
	return 1;
}

// peek_item(self, peekcount=1) -> itemstack
int LuaItemStack::l_peek_item(lua_State *L)
{
	NO_MAP_LOCK_REQUIRED;
	LuaItemStack *o = checkObject<LuaItemStack>(L, 1);
	ItemStack &item = o->m_stack;
	u32 peekcount = 1;
	if(!lua_isnone(L, 2))
		peekcount = lua_tointeger(L, 2);
	ItemStack peekaboo = item.peekItem(peekcount);
	create(L, peekaboo);
	return 1;
}

// equals(self, other) -> bool
int LuaItemStack::l_equals(lua_State *L)
{
	NO_MAP_LOCK_REQUIRED;
	LuaItemStack *o1 = checkObject<LuaItemStack>(L, 1);

	// checks for non-userdata argument
	if (!lua_isuserdata(L, 2)) {
		lua_pushboolean(L, false);
		return 1;
	}

	// check that the argument is an ItemStack
	if (!lua_getmetatable(L, 2)) {
		lua_pushboolean(L, false);
		return 1;
	}
	lua_getfield(L, LUA_REGISTRYINDEX, className);
	if (!lua_rawequal(L, -1, -2)) {
		lua_pushboolean(L, false);
		return 1;
	}

	LuaItemStack *o2 = checkObject<LuaItemStack>(L, 2);

	ItemStack &item1 = o1->m_stack;
	ItemStack &item2 = o2->m_stack;

	lua_pushboolean(L, item1 == item2);
	return 1;
}

LuaItemStack::LuaItemStack(const ItemStack &item):
	m_stack(item)
{
}

// LuaItemStack(itemstack or itemstring or table or nil)
// Creates an LuaItemStack and leaves it on top of stack
int LuaItemStack::create_object(lua_State *L)
{
	NO_MAP_LOCK_REQUIRED;
	ItemStack item;
	if (!lua_isnone(L, 1))
		item = read_item(L, 1, getGameDef(L)->idef());
	LuaItemStack *o = new LuaItemStack(item);
	*(void **)(lua_newuserdata(L, sizeof(void *))) = o;
	luaL_getmetatable(L, className);
	lua_setmetatable(L, -2);
	return 1;
}

// Not callable from Lua
int LuaItemStack::create(lua_State *L, const ItemStack &item)
{
	NO_MAP_LOCK_REQUIRED;
	LuaItemStack *o = new LuaItemStack(item);
	*(void **)(lua_newuserdata(L, sizeof(void *))) = o;
	luaL_getmetatable(L, className);
	lua_setmetatable(L, -2);
	return 1;
}

void *LuaItemStack::packIn(lua_State *L, int idx)
{
	LuaItemStack *o = checkObject<LuaItemStack>(L, idx);
	return new ItemStack(o->getItem());
}

void LuaItemStack::packOut(lua_State *L, void *ptr)
{
	ItemStack *stack = reinterpret_cast<ItemStack*>(ptr);
	if (L)
		create(L, *stack);
	delete stack;
}

void LuaItemStack::Register(lua_State *L)
{
	static const luaL_Reg metamethods[] = {
		{"__tostring", mt_tostring},
		{"__gc", gc_object},
		{"__eq", l_equals},
		{0, 0}
	};
	registerClass<LuaItemStack>(L, methods, metamethods);

	// Can be created from Lua (ItemStack(itemstack or itemstring or table or nil))
	lua_register(L, className, create_object);

	script_register_packer(L, className, packIn, packOut);
}

const char LuaItemStack::className[] = "ItemStack";
const luaL_Reg LuaItemStack::methods[] = {
	luamethod(LuaItemStack, is_empty),
	luamethod(LuaItemStack, get_name),
	luamethod(LuaItemStack, set_name),
	luamethod(LuaItemStack, get_count),
	luamethod(LuaItemStack, set_count),
	luamethod(LuaItemStack, get_wear),
	luamethod(LuaItemStack, set_wear),
	luamethod(LuaItemStack, get_meta),
	luamethod(LuaItemStack, get_metadata),
	luamethod(LuaItemStack, set_metadata),
	luamethod(LuaItemStack, get_description),
	luamethod(LuaItemStack, get_short_description),
	luamethod(LuaItemStack, clear),
	luamethod(LuaItemStack, replace),
	luamethod(LuaItemStack, to_string),
	luamethod(LuaItemStack, to_table),
	luamethod(LuaItemStack, get_stack_max),
	luamethod(LuaItemStack, get_free_space),
	luamethod(LuaItemStack, is_known),
	luamethod(LuaItemStack, get_definition),
	luamethod(LuaItemStack, get_tool_capabilities),
	luamethod(LuaItemStack, add_wear),
	luamethod(LuaItemStack, add_wear_by_uses),
	luamethod(LuaItemStack, get_wear_bar_params),
	luamethod(LuaItemStack, add_item),
	luamethod(LuaItemStack, item_fits),
	luamethod(LuaItemStack, take_item),
	luamethod(LuaItemStack, peek_item),
	luamethod(LuaItemStack, equals),
	{0,0}
};

/*
	ItemDefinition
*/

// register_item_raw({lots of stuff})
int ModApiItem::l_register_item_raw(lua_State *L)
{
	NO_MAP_LOCK_REQUIRED;
	luaL_checktype(L, 1, LUA_TTABLE);
	int table = 1;

	// Get the writable item and node definition managers from the server
	IWritableItemDefManager *idef =
			getServer(L)->getWritableItemDefManager();
	NodeDefManager *ndef =
			getServer(L)->getWritableNodeDefManager();

	// Check if name is defined
	std::string name;
	lua_getfield(L, table, "name");
	if(lua_isstring(L, -1)){
		name = readParam<std::string>(L, -1);
	} else {
		throw LuaError("register_item_raw: name is not defined or not a string");
	}

	// Check if on_use is defined

	ItemDefinition def;
	// Set a distinctive default value to check if this is set
	def.node_placement_prediction = "__default";

	// Read the item definition
	read_item_definition(L, table, def, def);

	// Default to having client-side placement prediction for nodes
	// ("" in item definition sets it off)
	if(def.node_placement_prediction == "__default"){
		if(def.type == ITEM_NODE)
			def.node_placement_prediction = name;
		else
			def.node_placement_prediction.clear();
	}

	// Register item definition
	idef->registerItem(def);

	// Read the node definition (content features) and register it
	if (def.type == ITEM_NODE) {
		ContentFeatures f;
		read_content_features(L, f, table);
		// when a mod reregisters ignore, only texture changes and such should
		// be done
		if (f.name == "ignore")
			return 0;
		// This would break everything
		if (f.name.empty())
			throw LuaError("Cannot register node with empty name");

		content_t id = ndef->set(f.name, f);

		if (id > MAX_REGISTERED_CONTENT) {
			throw LuaError("Number of registerable nodes ("
					+ itos(MAX_REGISTERED_CONTENT+1)
					+ ") exceeded (" + name + ")");
		}
	}

	return 0; /* number of results */
}

// unregister_item(name)
int ModApiItem::l_unregister_item_raw(lua_State *L)
{
	NO_MAP_LOCK_REQUIRED;
	std::string name = luaL_checkstring(L, 1);

	IWritableItemDefManager *idef =
			getServer(L)->getWritableItemDefManager();

	// Unregister the node
	if (idef->get(name).type == ITEM_NODE) {
		NodeDefManager *ndef =
			getServer(L)->getWritableNodeDefManager();
		ndef->removeNode(name);
	}

	idef->unregisterItem(name);

	return 0; /* number of results */
}

// register_alias_raw(name, convert_to_name)
int ModApiItem::l_register_alias_raw(lua_State *L)
{
	NO_MAP_LOCK_REQUIRED;
	std::string name = luaL_checkstring(L, 1);
	std::string convert_to = luaL_checkstring(L, 2);

	// Get the writable item definition manager from the server
	IWritableItemDefManager *idef =
			getServer(L)->getWritableItemDefManager();

	idef->registerAlias(name, convert_to);

	return 0; /* number of results */
}

// get_content_id(name)
int ModApiItem::l_get_content_id(lua_State *L)
{
	NO_MAP_LOCK_REQUIRED;
	std::string name = luaL_checkstring(L, 1);

	const IItemDefManager *idef = getGameDef(L)->idef();
	const NodeDefManager *ndef = getGameDef(L)->ndef();

	// If this is called at mod load time, NodeDefManager isn't aware of
	// aliases yet, so we need to handle them manually
	const auto &alias_name = idef->getAlias(name);

	content_t content_id;
	if (alias_name != name) {
		if (!ndef->getId(alias_name, content_id))
			throw LuaError("Unknown node: " + alias_name +
					" (from alias " + name + ")");
	} else if (!ndef->getId(name, content_id)) {
		throw LuaError("Unknown node: " + name);
	}

	lua_pushinteger(L, content_id);
	return 1; /* number of results */
}

// get_name_from_content_id(name)
int ModApiItem::l_get_name_from_content_id(lua_State *L)
{
	NO_MAP_LOCK_REQUIRED;
	content_t c = luaL_checkint(L, 1);

	const NodeDefManager *ndef = getGameDef(L)->ndef();
	const char *name = ndef->get(c).name.c_str();

	lua_pushstring(L, name);
	return 1; /* number of results */
}

void ModApiItem::Initialize(lua_State *L, int top)
{
	API_FCT(register_item_raw);
	API_FCT(unregister_item_raw);
	API_FCT(register_alias_raw);
	API_FCT(get_content_id);
	API_FCT(get_name_from_content_id);
}

void ModApiItem::InitializeAsync(lua_State *L, int top)
{
	// all read-only functions
	API_FCT(get_content_id);
	API_FCT(get_name_from_content_id);
}

void ModApiItem::InitializeClient(lua_State *L, int top)
{
	// all read-only functions
	API_FCT(get_content_id);
	API_FCT(get_name_from_content_id);
}
