// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2013 celeron55, Perttu Ahola <celeron55@gmail.com>
// Copyright (C) 2017 nerzhul, Loic Blot <loic.blot@unix-experience.fr>

#pragma once

#include "l_metadata.h"
#include "lua_api/l_base.h"
#include "content/mods.h"

class ModApiStorage : public ModApiBase
{
protected:
	static int l_get_mod_storage(lua_State *L);

public:
	static void Initialize(lua_State *L, int top);
};

class StorageRef : public MetaDataRef
{
private:
	ModStorage m_object;

	static const luaL_Reg methods[];

	virtual IMetadata *getmeta(bool auto_create);
	virtual void clearMeta();

public:
	StorageRef(const std::string &mod_name, ModStorageDatabase *db): m_object(mod_name, db) {}
	~StorageRef() = default;

	static void Register(lua_State *L);
	static void create(lua_State *L, const std::string &mod_name, ModStorageDatabase *db);

	static const char className[];
};
