// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2013-8 celeron55, Perttu Ahola <celeron55@gmail.com>
// Copyright (C) 2017-8 rubenwardy <rw@rubenwardy.com>

#pragma once

#include "irrlichttypes_bloated.h"
#include "lua_api/l_base.h"

class IMetadata;

/*
	NodeMetaRef
*/

class MetaDataRef : public ModApiBase
{
public:
	virtual ~MetaDataRef() = default;

	static MetaDataRef *checkAnyMetadata(lua_State *L, int narg);

protected:
	virtual void reportMetadataChange(const std::string *name = nullptr) {}
	virtual IMetadata *getmeta(bool auto_create) = 0;
	virtual void clearMeta() = 0;

	virtual void handleToTable(lua_State *L, IMetadata *meta);
	virtual bool handleFromTable(lua_State *L, int table, IMetadata *meta);

	template<class T>
	static void registerMetadataClass(lua_State *L, const luaL_Reg *methods)
	{
		const luaL_Reg metamethods[] = {
			{"__eq", l_equals},
			{"__gc", gc_object},
			{0, 0}
		};
		registerClass<T>(L, methods, metamethods);

		// Set metadata_class in the metatable for MetaDataRef::checkAnyMetadata.
		luaL_getmetatable(L, T::className);
		lua_pushstring(L, T::className);
		lua_setfield(L, -2, "metadata_class");
		lua_pop(L, 1);
	}

	// Exported functions

	static int gc_object(lua_State *L);

	// contains(self, name)
	static int l_contains(lua_State *L);

	// get(self, name)
	static int l_get(lua_State *L);

	// get_string(self, name)
	static int l_get_string(lua_State *L);

	// set_string(self, name, var)
	static int l_set_string(lua_State *L);

	// get_int(self, name)
	static int l_get_int(lua_State *L);

	// set_int(self, name, var)
	static int l_set_int(lua_State *L);

	// get_float(self, name)
	static int l_get_float(lua_State *L);

	// set_float(self, name, var)
	static int l_set_float(lua_State *L);

	// get_keys(self)
	static int l_get_keys(lua_State *L);

	// to_table(self)
	static int l_to_table(lua_State *L);

	// from_table(self, table)
	static int l_from_table(lua_State *L);

	// equals(self, other)
	static int l_equals(lua_State *L);
};
