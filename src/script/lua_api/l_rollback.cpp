// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2013 celeron55, Perttu Ahola <celeron55@gmail.com>

#include "lua_api/l_rollback.h"
#include "lua_api/l_internal.h"
#include "common/c_converter.h"
#include "server.h"
#include "rollback_interface.h"


void push_RollbackNode(lua_State *L, RollbackNode &node)
{
	lua_createtable(L, 0, 3);
	lua_pushstring(L, node.name.c_str());
	lua_setfield(L, -2, "name");
	lua_pushnumber(L, node.param1);
	lua_setfield(L, -2, "param1");
	lua_pushnumber(L, node.param2);
	lua_setfield(L, -2, "param2");
}

// rollback_get_node_actions(pos, range, seconds, limit) -> {{actor, pos, time, oldnode, newnode}, ...}
int ModApiRollback::l_rollback_get_node_actions(lua_State *L)
{
	NO_MAP_LOCK_REQUIRED;

	v3s16 pos = read_v3s16(L, 1);
	int range = luaL_checknumber(L, 2);
	time_t seconds = (time_t) luaL_checknumber(L, 3);
	int limit = luaL_checknumber(L, 4);
	Server *server = getServer(L);
	IRollbackManager *rollback = server->getRollbackManager();
	if (rollback == NULL) {
		return 0;
	}

	std::list<RollbackAction> actions = rollback->getNodeActors(pos, range, seconds, limit);
	auto iter = actions.begin();

	lua_createtable(L, actions.size(), 0);
	for (unsigned int i = 1; iter != actions.end(); ++iter, ++i) {
		lua_createtable(L, 0, 5); // Make a table with enough space pre-allocated

		lua_pushstring(L, iter->actor.c_str());
		lua_setfield(L, -2, "actor");

		push_v3s16(L, iter->p);
		lua_setfield(L, -2, "pos");

		lua_pushnumber(L, iter->unix_time);
		lua_setfield(L, -2, "time");

		push_RollbackNode(L, iter->n_old);
		lua_setfield(L, -2, "oldnode");

		push_RollbackNode(L, iter->n_new);
		lua_setfield(L, -2, "newnode");

		lua_rawseti(L, -2, i); // Add action table to main table
	}

	return 1;
}

// rollback_revert_actions_by(actor, seconds) -> bool, log messages
int ModApiRollback::l_rollback_revert_actions_by(lua_State *L)
{
	MAP_LOCK_REQUIRED;

	std::string actor = luaL_checkstring(L, 1);
	int seconds = luaL_checknumber(L, 2);
	Server *server = getServer(L);
	IRollbackManager *rollback = server->getRollbackManager();

	// If rollback is disabled, tell it's not a success.
	if (rollback == NULL) {
		lua_pushboolean(L, false);
		lua_newtable(L);
		return 2;
	}
	std::list<RollbackAction> actions = rollback->getRevertActions(actor, seconds);
	std::list<std::string> log;
	bool success = server->rollbackRevertActions(actions, &log);
	// Push boolean result
	lua_pushboolean(L, success);
	lua_createtable(L, log.size(), 0);
	unsigned long i = 0;
	for(auto iter = log.begin(); iter != log.end(); ++i, ++iter) {
		lua_pushnumber(L, i);
		lua_pushstring(L, iter->c_str());
		lua_settable(L, -3);
	}
	return 2;
}

void ModApiRollback::Initialize(lua_State *L, int top)
{
	API_FCT(rollback_get_node_actions);
	API_FCT(rollback_revert_actions_by);
}
