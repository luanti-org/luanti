// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2013 celeron55, Perttu Ahola <celeron55@gmail.com>
// Copyright (C) 2017 nerzhul, Loic Blot <loic.blot@unix-experience.fr>

#include "lua_api/l_storage.h"
#include "l_internal.h"
#include "server.h"

int ModApiStorage::l_get_mod_storage(lua_State *L)
{
	// Note that this is wrapped in Lua, see builtin/common/mod_storage.lua
	std::string mod_name = readParam<std::string>(L, 1);

	if (IGameDef *gamedef = getGameDef(L)) {
		StorageRef::create(L, mod_name, gamedef->getModStorageDatabase());
	} else {
		assert(false); // this should not happen
		lua_pushnil(L);
	}
	return 1;
}

void ModApiStorage::Initialize(lua_State *L, int top)
{
	API_FCT(get_mod_storage);
}

void StorageRef::create(lua_State *L, const std::string &mod_name, ModStorageDatabase *db)
{
	StorageRef *o = new StorageRef(mod_name, db);
	*(void **)(lua_newuserdata(L, sizeof(void *))) = o;
	luaL_getmetatable(L, className);
	lua_setmetatable(L, -2);
}

void StorageRef::Register(lua_State *L)
{
	registerMetadataClass<StorageRef>(L, methods);
}

IMetadata* StorageRef::getmeta(bool auto_create)
{
	return &m_object;
}

void StorageRef::clearMeta()
{
	m_object.clear();
}

const char StorageRef::className[] = "StorageRef";
const luaL_Reg StorageRef::methods[] = {
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
	{0,0}
};
