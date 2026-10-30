// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2013 celeron55, Perttu Ahola <celeron55@gmail.com>

#include "lua_api/l_nodemeta.h"
#include "lua_api/l_internal.h"
#include "gamedef.h"
#include "node_modifier.h"
#include "lua_api/l_node_modifier.h"
#include "itemstackmetadata.h"
#include "lua_api/l_inventory.h"
#include "common/c_content.h"
#include "common/helper.h"
#include "serverenvironment.h"
#include "map.h"
#include "mapblock.h"
#if CHECK_CLIENT_BUILD()
#include "client/client.h"
#include "client/clientenvironment.h"
#include "client/clientmap.h"
#endif

/*
	NodeMetaRef
*/

IMetadata* NodeMetaRef::getmeta(bool auto_create)
{
#if CHECK_CLIENT_BUILD()
	if (m_is_local)
		return m_client->getEnv().getMap().getNodeMetadata(m_p);
#endif

	NodeMetadata *meta = m_env->getMap().getNodeMetadata(m_p);
	if (meta == NULL && auto_create) {
		meta = new NodeMetadata(m_env->getGameDef()->idef());
		if (!m_env->getMap().setNodeMetadata(m_p, meta)) {
			delete meta;
			return NULL;
		}
	}
	return meta;
}

void NodeMetaRef::removeNodeMetadata()
{
	SANITY_CHECK(!m_is_local);
	m_env->getMap().removeNodeMetadata(m_p);
}

void NodeMetaRef::clearMeta()
{
	removeNodeMetadata();

	m_env->getMap().setNodeModifiers(m_p, {});
}

void NodeMetaRef::reportMetadataChange(const std::string *name)
{
	SANITY_CHECK(!m_is_local);
	// Inform other things that the metadata has changed
	NodeMetadata *meta = dynamic_cast<NodeMetadata*>(getmeta(false));

	bool is_private_change = meta && name && meta->isPrivate(*name);

	// If the metadata is now empty, get rid of it
	if (meta && meta->empty()) {
		removeNodeMetadata();
		meta = nullptr;
	}

	MapEditEvent event;
	event.type = MEET_BLOCK_NODE_METADATA_CHANGED;
	event.setPositionModified(m_p);
	event.is_private_change = is_private_change;
	m_env->getMap().dispatchEvent(event);
}

// Exported functions

// get_modifiers()
int NodeMetaRef::l_get_modifiers(lua_State *L)
{
	NO_MAP_LOCK_REQUIRED;

	auto *ref = checkObject<NodeMetaRef>(L, 1);
	NodeModifiersRef::create(L, ref->m_p, ref->m_env);
	return 1;
}

// get_inventory(self)
int NodeMetaRef::l_get_inventory(lua_State *L)
{
	MAP_LOCK_REQUIRED;

	NodeMetaRef *ref = checkObject<NodeMetaRef>(L, 1);
	ref->getmeta(true);  // try to ensure the metadata exists

	InventoryLocation loc;
	loc.setNodeMeta(ref->m_p);
	InvRef::create(L, loc);
	return 1;
}

// mark_as_private(self, <string> or {<string>, <string>, ...})
int NodeMetaRef::l_mark_as_private(lua_State *L)
{
	MAP_LOCK_REQUIRED;

	NodeMetaRef *ref = checkObject<NodeMetaRef>(L, 1);
	NodeMetadata *meta = dynamic_cast<NodeMetadata*>(ref->getmeta(true));
	if (!meta)
		return 0;

	bool modified = false;
	if (lua_istable(L, 2)) {
		LuaHelper::for_ipairs(L, 2, [&]() {
			luaL_checktype(L, -1, LUA_TSTRING);
			modified |= meta->markPrivate(readParam<std::string>(L, -1), true);
		});
	} else if (lua_isstring(L, 2)) {
		modified |= meta->markPrivate(readParam<std::string>(L, 2), true);
	}
	if (modified)
		ref->reportMetadataChange();

	return 0;
}

void NodeMetaRef::handleToTable(lua_State *L, IMetadata *_meta)
{
	// fields
	MetaDataRef::handleToTable(L, _meta);

	NodeMetadata *meta = dynamic_cast<NodeMetadata*>(_meta);
	sanity_check(meta);

	// inventory
	Inventory *inv = meta->getInventory();
	if (inv) {
		push_inventory_lists(L, *inv);
	} else {
		lua_newtable(L);
	}
	lua_setfield(L, -2, "inventory");

#if CHECK_CLIENT_BUILD()
	if (m_is_local) {
		const auto ids = m_client->getEnv().getMap().getNodeModifiers(m_p);
		const auto *manager = m_client->getNodeModifierManager();
		lua_createtable(L, ids.size(), 0);
		int i = 0;
		for (u16 id : ids) {
			if (id >= manager->size())
				continue;
			const auto &name = manager->get(id).m_name;
			lua_pushlstring(L, name.data(), name.size());
			lua_rawseti(L, -2, ++i);
		}
	} else
#endif
		NodeModifiersRef(m_p, m_env).pushList(L);
	lua_setfield(L, -2, "modifiers");
}

// from_table(self, table)
bool NodeMetaRef::handleFromTable(lua_State *L, int table)
{
	SANITY_CHECK(!m_is_local);
	auto meta = std::make_unique<NodeMetadata>(m_env->getGameDef()->idef());
	readFields(L, table, meta.get());

	// inventory
	Inventory *inv = meta->getInventory();
	lua_getfield(L, table, "inventory");
	if (lua_istable(L, -1)) {
		auto *gamedef = getGameDef(L);
		int inventorytable = lua_gettop(L);
		lua_pushnil(L);
		while (lua_next(L, inventorytable) != 0) {
			// key at index -2 and value at index -1
			const char *name = luaL_checkstring(L, -2);
			read_inventory_list(L, -1, inv, name, gamedef);
			lua_pop(L, 1); // Remove value, keep key for next iteration
		}
	}
	lua_pop(L, 1);

	std::vector<std::string> modifiers;
	lua_getfield(L, table, "modifiers");
	if (!lua_isnil(L, -1))
		NodeModifiersRef::readList(L, -1, modifiers);
	lua_pop(L, 1);

	Map &map = m_env->getMap();
	if (!NodeModifiersRef(m_p, m_env).set(modifiers))
		return false;

	bool installed = map.setNodeMetadata(m_p, meta.get());
	// The block should exist, otherwise setting modifiers would fail
	SANITY_CHECK(installed);

	meta.release();
	return true;
}


NodeMetaRef::NodeMetaRef(v3s16 p, ServerEnvironment *env):
	m_p(p),
	m_env(env)
{
}

#if CHECK_CLIENT_BUILD()
NodeMetaRef::NodeMetaRef(v3s16 p, Client *client):
	m_is_local(true), m_p(p), m_client(client)
{
}
#endif

// Creates an NodeMetaRef and leaves it on top of stack
// Not callable from Lua; all references are created on the C side.
void NodeMetaRef::create(lua_State *L, v3s16 p, ServerEnvironment *env)
{
	NodeMetaRef *o = new NodeMetaRef(p, env);
	*(void **)(lua_newuserdata(L, sizeof(void *))) = o;
	luaL_getmetatable(L, className);
	lua_setmetatable(L, -2);
}

// Client-sided version of the above
#if CHECK_CLIENT_BUILD()
void NodeMetaRef::createClient(lua_State *L, v3s16 p, Client *client)
{
	NodeMetaRef *o = new NodeMetaRef(p, client);
	*(void **)(lua_newuserdata(L, sizeof(void *))) = o;
	luaL_getmetatable(L, className);
	lua_setmetatable(L, -2);
}
#endif

const char NodeMetaRef::className[] = "NodeMetaRef";

void NodeMetaRef::Register(lua_State *L)
{
	registerMetadataClass<NodeMetaRef>(L, methodsServer);
}


const luaL_Reg NodeMetaRef::methodsServer[] = {
	luamethod(NodeMetaRef, get_modifiers),
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
	luamethod(NodeMetaRef, get_inventory),
	luamethod(NodeMetaRef, mark_as_private),
	luamethod(MetaDataRef, equals),
	{0,0}
};


void NodeMetaRef::RegisterClient(lua_State *L)
{
	registerMetadataClass<NodeMetaRef>(L, methodsClient);
}


const luaL_Reg NodeMetaRef::methodsClient[] = {
	luamethod(MetaDataRef, contains),
	luamethod(MetaDataRef, get),
	luamethod(MetaDataRef, get_string),
	luamethod(MetaDataRef, get_int),
	luamethod(MetaDataRef, get_float),
	luamethod(MetaDataRef, get_keys),
	luamethod(MetaDataRef, to_table),
	{0,0}
};
