// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2013 celeron55, Perttu Ahola <celeron55@gmail.com>
// Copyright (C) 2017-8 rubenwardy <rw@rubenwardy.com>
// Copyright (C) 2017 raymoo

#pragma once

#include "lua_api/l_base.h"
#include "lua_api/l_metadata.h"
#include "lua_api/l_item.h"
#include "irrlichttypes_bloated.h"

class ItemStackMetaRef : public MetaDataRef
{
private:
	LuaItemStack *istack;

	static const luaL_Reg methods[];

	virtual IMetadata* getmeta(bool auto_create);

	virtual void clearMeta();

	virtual void reportMetadataChange(const std::string *name = nullptr);

	void setToolCapabilities(const ToolCapabilities &caps)
	{
		istack->getItem().metadata.setToolCapabilities(caps);
	}

	void clearToolCapabilities()
	{
		istack->getItem().metadata.clearToolCapabilities();
	}

	void setWearBarParams(const WearBarParams &params)
	{
		istack->getItem().metadata.setWearBarParams(params);
	}

	void clearWearBarParams()
	{
		istack->getItem().metadata.clearWearBarParams();
	}

	// Exported functions
	static int l_set_tool_capabilities(lua_State *L);
	static int l_set_wear_bar_params(lua_State *L);
public:
	// takes a reference
	ItemStackMetaRef(LuaItemStack *istack);
	~ItemStackMetaRef();

	DISABLE_CLASS_COPY(ItemStackMetaRef)

	// Creates an ItemStackMetaRef and leaves it on top of stack
	// Not callable from Lua; all references are created on the C side.
	static void create(lua_State *L, LuaItemStack *istack);

	static void Register(lua_State *L);

	static const char className[];
};
