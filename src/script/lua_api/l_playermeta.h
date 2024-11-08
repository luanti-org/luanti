// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2013 celeron55, Perttu Ahola <celeron55@gmail.com>
// Copyright (C) 2017-8 rubenwardy <rw@rubenwardy.com>

#pragma once

#include "lua_api/l_base.h"
#include "lua_api/l_metadata.h"
#include "irrlichttypes_bloated.h"
#include "inventory.h"

class ServerEnvironment;

class PlayerMetaRef : public MetaDataRef
{
private:
	ServerEnvironment *m_env;
	std::string m_name;

	static const luaL_Reg methods[];

	virtual IMetadata *getmeta(bool auto_create);

	virtual void clearMeta();

	virtual void reportMetadataChange(const std::string *name = nullptr);

public:
	PlayerMetaRef(ServerEnvironment *env, std::string_view name) :
		m_env(env), m_name(name)
	{
		assert(m_env);
		assert(!m_name.empty());
	}
	~PlayerMetaRef() = default;

	// Creates an PlayerMetaRef and leaves it on top of stack
	// Not callable from Lua; all references are created on the C side.
	static void create(lua_State *L, ServerEnvironment *env, std::string_view name);

	static void Register(lua_State *L);

	static const char className[];
};
