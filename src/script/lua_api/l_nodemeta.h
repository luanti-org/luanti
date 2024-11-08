// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2013 celeron55, Perttu Ahola <celeron55@gmail.com>

#pragma once

#include "lua_api/l_base.h"
#include "lua_api/l_metadata.h"
#include "irrlichttypes_bloated.h"
#include "nodemetadata.h"

class ServerEnvironment;
class NodeMetadata;

/*
	NodeMetaRef
*/

class NodeMetaRef : public MetaDataRef {
private:
	bool m_is_local = false;
	// Set for server metadata
	v3s16 m_p;
	ServerEnvironment *m_env = nullptr;
	// Set for client metadata
	IMetadata *m_local_meta = nullptr;

	static const luaL_Reg methodsServer[];
	static const luaL_Reg methodsClient[];

	/**
	 * Retrieve metadata for a node.
	 * If @p auto_create is set and the specified node has no metadata information
	 * associated with it yet, the method attempts to attach a new metadata object
	 * to the node and returns a pointer to the metadata when successful.
	 *
	 * However, it is NOT guaranteed that the method will return a pointer,
	 * and @c NULL may be returned in case of an error regardless of @p auto_create.
	 *
	 * @param ref specifies the node for which the associated metadata is retrieved.
	 * @param auto_create when true, try to create metadata information for the node if it has none.
	 * @return pointer to a @c NodeMetadata object or @c NULL in case of error.
	 */
	virtual IMetadata* getmeta(bool auto_create);
	virtual void clearMeta();

	virtual void reportMetadataChange(const std::string *name = nullptr);

	virtual void handleToTable(lua_State *L, IMetadata *_meta);
	virtual bool handleFromTable(lua_State *L, int table, IMetadata *_meta);

	// Exported functions

	// get_inventory(self)
	static int l_get_inventory(lua_State *L);

	// mark_as_private(self, <string> or {<string>, <string>, ...})
	static int l_mark_as_private(lua_State *L);

public:
	NodeMetaRef(v3s16 p, ServerEnvironment *env);
	NodeMetaRef(IMetadata *meta);

	~NodeMetaRef() = default;

	// Creates an NodeMetaRef and leaves it on top of stack
	// Not callable from Lua; all references are created on the C side.
	static void create(lua_State *L, v3s16 p, ServerEnvironment *env);

	// Client-sided version of the above
	static void createClient(lua_State *L, IMetadata *meta);

	static void Register(lua_State *L);
	static void RegisterClient(lua_State *L);

	static const char className[];
};
