// SPDX-FileCopyrightText: 2025 Luanti authors
//
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "s_sscsm.h"

#include "s_internal.h"
#include "script/sscsm/sscsm_environment.h"
#include "script/sscsm/sscsm_requests.h"
#include "script/common/c_content.h"
#include "itemdef.h"

void ScriptApiSSCSM::add_itemdefs()
{
	SCRIPTAPI_PRECHECKHEADER

	infostream << "Adding itemdefs..." << std::endl;

	auto answer = getSSCSMEnv()->doRequest(SSCSMRequestGetItemDefs{});

	lua_newtable(L);
	int idx_registered_items = lua_gettop(L);
	lua_newtable(L);
	int idx_registered_nodes = lua_gettop(L);
	lua_newtable(L);
	int idx_registered_tools = lua_gettop(L);
	lua_newtable(L);
	int idx_registered_craftitems = lua_gettop(L);
	lua_newtable(L);
	int idx_registered_aliases = lua_gettop(L);

	for (const ItemDefinition &item_def : answer.items) {
		lua_pushstring(L, item_def.name.c_str());
		int idx_name = lua_gettop(L);
		push_item_definition_for_sscsm(L, item_def);
		int idx_def = lua_gettop(L);

		// insert into core.registered_items
		lua_pushvalue(L, idx_name);
		lua_pushvalue(L, idx_def);
		lua_settable(L, idx_registered_items);

		// insert into core.registered_<itemtype>s
		int idx_defs = idx_registered_items;
		switch (item_def.type) {
		case ITEM_NONE:
			break;
		case ITEM_NODE:
			idx_defs = idx_registered_nodes;
			break;
		case ITEM_CRAFT:
			idx_defs = idx_registered_craftitems;
			break;
		case ITEM_TOOL:
			idx_defs = idx_registered_tools;
			break;
		default:
			assert(false);
			break;
		}
		if (idx_defs != idx_registered_items) {
			lua_pushvalue(L, idx_name);
			lua_pushvalue(L, idx_def);
			lua_settable(L, idx_defs);
		}

		lua_pop(L, 2); // def, name
	}

	for (const ContentFeatures &node_def : answer.nodes) {
		lua_pushstring(L, node_def.name.c_str());
		lua_gettable(L, idx_registered_nodes);
		if (lua_isnil(L, -1)) {
			// registered as a node but not as an item (e.g. CONTENT_UNKNOWN);
			// nothing to merge extra node fields into, skip it
			lua_pop(L, 1);
			continue;
		}
		int idx_existing = lua_gettop(L);

		push_content_features_for_sscsm(L, node_def);
		int idx_extra = lua_gettop(L);

		// merge node-only fields into the existing item def table
		lua_pushnil(L);
		while (lua_next(L, idx_extra) != 0) {
			lua_pushvalue(L, -2); // key
			lua_insert(L, -2); // key, value
			lua_settable(L, idx_existing);
		}

		lua_pop(L, 2); // idx_extra, idx_existing
	}

	//TODO: aliases, name-id mapping

	lua_getglobal(L, "core");
	lua_pushvalue(L, idx_registered_items);
	lua_setfield(L, -2, "registered_items");
	lua_pushvalue(L, idx_registered_nodes);
	lua_setfield(L, -2, "registered_nodes");
	lua_pushvalue(L, idx_registered_tools);
	lua_setfield(L, -2, "registered_tools");
	lua_pushvalue(L, idx_registered_craftitems);
	lua_setfield(L, -2, "registered_craftitems");
	lua_pushvalue(L, idx_registered_aliases);
	lua_setfield(L, -2, "registered_aliases");
	lua_pop(L, 1); // core

	// SSCSM client builtin will complete the defs
}

void ScriptApiSSCSM::load_mods(const std::vector<std::pair<std::string, std::string>> &mods)
{
	infostream << "Loading SSCSMs:" << std::endl;
	for (const auto &m : mods) {
		infostream << "Loading SSCSM " << m.first << std::endl;
		loadModFromMemory(m.first, m.second);
	}
}

void ScriptApiSSCSM::environment_step(float dtime)
{
	SCRIPTAPI_PRECHECKHEADER

	// Get core.registered_globalsteps
	lua_getglobal(L, "core");
	lua_getfield(L, -1, "registered_globalsteps");
	// Call callbacks
	lua_pushnumber(L, dtime);
	runCallbacks(1, RUN_CALLBACKS_MODE_FIRST);
}
