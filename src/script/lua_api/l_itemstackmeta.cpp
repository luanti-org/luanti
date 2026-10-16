// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2013 celeron55, Perttu Ahola <celeron55@gmail.com>
// Copyright (C) 2017-8 rubenwardy <rw@rubenwardy.com>
// Copyright (C) 2017 raymoo

#include "lua_api/l_itemstackmeta.h"
#include "lua_api/l_internal.h"
#include "gamedef.h"
#include "node_modifier.h"
#include "common/c_content.h"
#include "tool.h"

/*
	ItemStackMetaRef
*/

IMetadata* ItemStackMetaRef::getmeta(bool auto_create)
{
	return &istack->getItem().metadata;
}

void ItemStackMetaRef::clearMeta()
{
	istack->getItem().metadata.clear();
}

void ItemStackMetaRef::handleToTable(lua_State *L, IMetadata *meta)
{
	MetaDataRef::handleToTable(L, meta);
	l_get_modifiers_list(L);
	lua_setfield(L, -2, "modifiers");
}

bool ItemStackMetaRef::handleFromTable(lua_State *L, int table, IMetadata *meta)
{
	if (!MetaDataRef::handleFromTable(L, table, meta))
		return false;

	std::vector<std::string> modifiers;
	lua_getfield(L, table, "modifiers");
	bool result = lua_isnil(L, -1) || readModifiers(L, -1, modifiers);
	lua_pop(L, 1);
	if (!result)
		return false;

	return istack->getItem().metadata.setModifiers(std::move(modifiers));
}

void ItemStackMetaRef::reportMetadataChange(const std::string *name)
{
	// nothing to do
}

int ItemStackMetaRef::l_set_modifiers(lua_State *L)
{
	MAP_LOCK_REQUIRED;
	ItemStackMetaRef *ref = checkObject<ItemStackMetaRef>(L, 1);
	std::vector<std::string> names;
	bool success = readModifiers(L, 2, names) &&
			ref->istack->getItem().metadata.setModifiers(std::move(names));
	lua_pushboolean(L, success);
	return 1;
}

int ItemStackMetaRef::l_add_modifier(lua_State *L)
{
	MAP_LOCK_REQUIRED;

	ItemStackMetaRef *ref = checkObject<ItemStackMetaRef>(L, 1);
	luaL_checktype(L, 2, LUA_TSTRING);

	std::string name = readParam<std::string>(L, 2);
	const NodeModifierManager *manager = getGameDef(L)->getNodeModifierManager();
	u16 id = manager->getId(name);
	bool success = id != MODIFIER_IGNORE && !manager->get(id).m_is_dummy &&
			ref->istack->getItem().metadata.addModifier(name);

	lua_pushboolean(L, success);
	return 1;
}

int ItemStackMetaRef::l_remove_modifier(lua_State *L)
{
	ItemStackMetaRef *ref = checkObject<ItemStackMetaRef>(L, 1);
	luaL_checktype(L, 2, LUA_TSTRING);

	std::string name = readParam<std::string>(L, 2);
	ref->istack->getItem().metadata.removeModifier(name);

	return 0;
}

int ItemStackMetaRef::l_get_modifiers_list(lua_State *L)
{
	ItemStackMetaRef *ref = checkObject<ItemStackMetaRef>(L, 1);

	const std::vector<std::string> &names = ref->istack->getItem().metadata.getModifiersList();
	lua_createtable(L, names.size(), 0);

	int i = 0;
	for (const auto &name : names) {
		lua_pushlstring(L, name.data(), name.size());
		lua_rawseti(L, -2, ++i);
	}

	return 1;
}

// Exported functions
int ItemStackMetaRef::l_set_tool_capabilities(lua_State *L)
{
	ItemStackMetaRef *metaref = checkObject<ItemStackMetaRef>(L, 1);
	if (lua_isnoneornil(L, 2)) {
		metaref->clearToolCapabilities();
	} else if (lua_istable(L, 2)) {
		ToolCapabilities caps = read_tool_capabilities(L, 2);
		metaref->setToolCapabilities(caps);
	} else {
		luaL_typerror(L, 2, "table or nil");
	}

	return 0;
}

int ItemStackMetaRef::l_set_wear_bar_params(lua_State *L)
{
	ItemStackMetaRef *metaref = checkObject<ItemStackMetaRef>(L, 1);
	if (lua_isnoneornil(L, 2)) {
		metaref->clearWearBarParams();
	} else if (lua_istable(L, 2) || lua_isstring(L, 2)) {
		metaref->setWearBarParams(read_wear_bar_params(L, 2));
	} else {
		luaL_typerror(L, 2, "table, ColorString, or nil");
	}

	return 0;
}

ItemStackMetaRef::ItemStackMetaRef(LuaItemStack *istack): istack(istack)
{
	istack->grab();
}

ItemStackMetaRef::~ItemStackMetaRef()
{
	istack->drop();
}

// Creates an NodeMetaRef and leaves it on top of stack
// Not callable from Lua; all references are created on the C side.
void ItemStackMetaRef::create(lua_State *L, LuaItemStack *istack)
{
	ItemStackMetaRef *o = new ItemStackMetaRef(istack);
	*(void **)(lua_newuserdata(L, sizeof(void *))) = o;
	luaL_getmetatable(L, className);
	lua_setmetatable(L, -2);
}

void ItemStackMetaRef::Register(lua_State *L)
{
	registerMetadataClass<ItemStackMetaRef>(L, methods);
}

const char ItemStackMetaRef::className[] = "ItemStackMetaRef";
const luaL_Reg ItemStackMetaRef::methods[] = {
	luamethod(MetaDataRef, contains),
	luamethod(MetaDataRef, get),
	luamethod(MetaDataRef, get_string),
	luamethod(MetaDataRef, set_string),
	luamethod(MetaDataRef, get_int),
	luamethod(MetaDataRef, set_int),
	luamethod(MetaDataRef, get_float),
	luamethod(MetaDataRef, set_float),
	luamethod(MetaDataRef, get_keys),
	luamethod(MetaDataRef, to_table),
	luamethod(MetaDataRef, from_table),
	luamethod(MetaDataRef, equals),
	luamethod(ItemStackMetaRef, set_modifiers),
	luamethod(ItemStackMetaRef, add_modifier),
	luamethod(ItemStackMetaRef, remove_modifier),
	luamethod(ItemStackMetaRef, get_modifiers_list),
	luamethod(ItemStackMetaRef, set_tool_capabilities),
	luamethod(ItemStackMetaRef, set_wear_bar_params),
	{0,0}
};
