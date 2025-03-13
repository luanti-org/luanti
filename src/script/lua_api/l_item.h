// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2013 celeron55, Perttu Ahola <celeron55@gmail.com>

#pragma once

#include "lua_api/l_base.h"
#include "inventory.h"  // ItemStack
#include "util/pointer.h"

class LuaItemStack : public ModApiBase, public IntrusiveReferenceCounted {
private:
	ItemStack m_stack;

	LuaItemStack(const ItemStack &item);
	~LuaItemStack() = default;

	static const luaL_Reg methods[];

	// Exported functions

	// garbage collector
	static int gc_object(lua_State *L);

	// __tostring metamethod
	static int mt_tostring(lua_State *L);

	// is_empty(self) -> true/false
	static int l_is_empty(lua_State *L);

	// get_name(self) -> string
	static int l_get_name(lua_State *L);

	// set_name(self, name)
	static int l_set_name(lua_State *L);

	// get_count(self) -> number
	static int l_get_count(lua_State *L);

	// set_count(self, number)
	static int l_set_count(lua_State *L);

	// get_wear(self) -> number
	static int l_get_wear(lua_State *L);

	// set_wear(self, number)
	static int l_set_wear(lua_State *L);

	// get_meta(self) -> string
	static int l_get_meta(lua_State *L);

	// DEPRECATED
	// get_metadata(self) -> string
	static int l_get_metadata(lua_State *L);

	// DEPRECATED
	// set_metadata(self, string)
	static int l_set_metadata(lua_State *L);

	// get_description(self)
	static int l_get_description(lua_State *L);

	// get_short_description(self)
	static int l_get_short_description(lua_State *L);

	// clear(self) -> true
	static int l_clear(lua_State *L);

	// replace(self, itemstack or itemstring or table or nil) -> true
	static int l_replace(lua_State *L);

	// to_string(self) -> string
	static int l_to_string(lua_State *L);

	// to_table(self) -> table or nil
	static int l_to_table(lua_State *L);

	// get_stack_max(self) -> number
	static int l_get_stack_max(lua_State *L);

	// get_free_space(self) -> number
	static int l_get_free_space(lua_State *L);

	// is_known(self) -> true/false
	// Checks if the item is defined.
	static int l_is_known(lua_State *L);

	// get_definition(self) -> table
	// Returns the item definition table from core.registered_items,
	// or a fallback one (name="unknown")
	static int l_get_definition(lua_State *L);

	// get_tool_capabilities(self) -> table
	// Returns the effective tool digging properties.
	// Returns those of the hand ("") if this item has none associated.
	static int l_get_tool_capabilities(lua_State *L);

	// add_wear(self, amount) -> true/false
	// The range for "amount" is [0,65536]. Wear is only added if the item
	// is a tool. Adding wear might destroy the item.
	// Returns true if the item is (or was) a tool.
	static int l_add_wear(lua_State *L);

	// add_wear_by_uses(self, max_uses) -> true/false
	// The range for "max_uses" is [0,65536].
	// Adds wear to the item in such a way that, if
	// only this function is called to add wear, the item
	// will be destroyed exactly after `max_uses` times of calling it.
	// No-op if `max_uses` is 0 or item is not a tool.
	// Returns true if the item is (or was) a tool.
	static int l_add_wear_by_uses(lua_State *L);

	// get_wear_bar_params(self) -> table
	// Returns the effective wear bar parameters.
	// Returns nil if this item has none associated.
	static int l_get_wear_bar_params(lua_State *L);

	// add_item(self, itemstack or itemstring or table or nil) -> itemstack
	// Returns leftover item stack
	static int l_add_item(lua_State *L);

	// item_fits(self, itemstack or itemstring or table or nil) -> true/false, itemstack
	// First return value is true iff the new item fits fully into the stack
	// Second return value is the would-be-left-over item stack
	static int l_item_fits(lua_State *L);

	// take_item(self, takecount=1) -> itemstack
	static int l_take_item(lua_State *L);

	// peek_item(self, peekcount=1) -> itemstack
	static int l_peek_item(lua_State *L);

	// equals(self, other) -> bool
	static int l_equals(lua_State *L);

public:
	DISABLE_CLASS_COPY(LuaItemStack)

	inline const ItemStack& getItem() const { return m_stack; }
	inline ItemStack& getItem() { return m_stack; }

	// LuaItemStack(itemstack or itemstring or table or nil)
	// Creates an LuaItemStack and leaves it on top of stack
	static int create_object(lua_State *L);
	// Not callable from Lua
	static int create(lua_State *L, const ItemStack &item);

	static void *packIn(lua_State *L, int idx);
	static void packOut(lua_State *L, void *ptr);

	static void Register(lua_State *L);

	static const char className[];
};

class ModApiItem : public ModApiBase {
private:
	static int l_register_item_raw(lua_State *L);
	static int l_unregister_item_raw(lua_State *L);
	static int l_register_alias_raw(lua_State *L);
	static int l_get_content_id(lua_State *L);
	static int l_get_name_from_content_id(lua_State *L);

public:
	static void Initialize(lua_State *L, int top);
	static void InitializeAsync(lua_State *L, int top);
	static void InitializeClient(lua_State *L, int top);
};
