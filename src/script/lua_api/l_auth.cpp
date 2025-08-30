// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2018 bendeutsch, Ben Deutsch <ben@bendeutsch.de>

#include "lua_api/l_auth.h"
#include "lua_api/l_internal.h"
#include "common/c_converter.h"
#include "common/c_content.h"
#include "cpp_api/s_base.h"
#include "server.h"
#include "environment.h"
#include "database/database.h"
#include <algorithm>

// common start: ensure auth db
AuthDatabase *ModApiAuth::getAuthDb(lua_State *L)
{
	GET_ENV_PTR_NO_MAP_LOCK;
	return env->getAuthDatabase();
}

void ModApiAuth::pushAuthEntry(lua_State *L, const AuthEntry &authEntry)
{
	lua_newtable(L);
	int table = lua_gettop(L);
	// id
	lua_pushnumber(L, authEntry.id);
	lua_setfield(L, table, "id");
	// name
	lua_pushstring(L, authEntry.name.c_str());
	lua_setfield(L, table, "name");
	// password
	lua_pushstring(L, authEntry.password.c_str());
	lua_setfield(L, table, "password");
	// privileges
	lua_newtable(L);
	int privtable = lua_gettop(L);
	for (const std::string &privs : authEntry.privileges) {
		lua_pushboolean(L, true);
		lua_setfield(L, privtable, privs.c_str());
	}
	lua_setfield(L, table, "privileges");
	// last_login
	lua_pushnumber(L, authEntry.last_login);
	lua_setfield(L, table, "last_login");

	lua_pushvalue(L, table);
}

// auth_read(name)
int ModApiAuth::l_auth_read(lua_State *L)
{
	NO_MAP_LOCK_REQUIRED;
	AuthDatabase *auth_db = getAuthDb(L);
	if (!auth_db)
		return 0;
	AuthEntry authEntry;
	const char *name = luaL_checkstring(L, 1);
	bool success = auth_db->getAuth(std::string(name), authEntry);
	if (!success)
		return 0;

	pushAuthEntry(L, authEntry);
	return 1;
}

// auth_save(table)
int ModApiAuth::l_auth_save(lua_State *L)
{
	NO_MAP_LOCK_REQUIRED;
	AuthDatabase *auth_db = getAuthDb(L);
	if (!auth_db)
		return 0;
	luaL_checktype(L, 1, LUA_TTABLE);
	int table = 1;
	AuthEntry authEntry;
	bool success;
	success = getintfield(L, table, "id", authEntry.id);
	success = success && getstringfield(L, table, "name", authEntry.name);
	success = success && getstringfield(L, table, "password", authEntry.password);
	lua_getfield(L, table, "privileges");
	if (lua_istable(L, -1)) {
		lua_pushnil(L);
		while (lua_next(L, -2)) {
			authEntry.privileges.emplace_back(
					lua_tostring(L, -2)); // the key, not the value
			lua_pop(L, 1);
		}
	} else {
		success = false;
	}
	lua_pop(L, 1); // the table
	success = success && getintfield(L, table, "last_login", authEntry.last_login);

	if (!success) {
		lua_pushboolean(L, false);
		return 1;
	}

	lua_pushboolean(L, auth_db->saveAuth(authEntry));
	return 1;
}

// auth_create(table)
int ModApiAuth::l_auth_create(lua_State *L)
{
	NO_MAP_LOCK_REQUIRED;
	AuthDatabase *auth_db = getAuthDb(L);
	if (!auth_db)
		return 0;
	luaL_checktype(L, 1, LUA_TTABLE);
	int table = 1;
	AuthEntry authEntry;
	bool success;
	// no meaningful id field, we assume
	success = getstringfield(L, table, "name", authEntry.name);
	success = success && getstringfield(L, table, "password", authEntry.password);
	lua_getfield(L, table, "privileges");
	if (lua_istable(L, -1)) {
		lua_pushnil(L);
		while (lua_next(L, -2)) {
			authEntry.privileges.emplace_back(
					lua_tostring(L, -2)); // the key, not the value
			lua_pop(L, 1);
		}
	} else {
		success = false;
	}
	lua_pop(L, 1); // the table
	success = success && getintfield(L, table, "last_login", authEntry.last_login);

	if (!success)
		return 0;

	if (auth_db->createAuth(authEntry)) {
		pushAuthEntry(L, authEntry);
		return 1;
	}

	return 0;
}

// auth_delete(name)
int ModApiAuth::l_auth_delete(lua_State *L)
{
	NO_MAP_LOCK_REQUIRED;
	AuthDatabase *auth_db = getAuthDb(L);
	if (!auth_db)
		return 0;
	std::string name(luaL_checkstring(L, 1));
	lua_pushboolean(L, auth_db->deleteAuth(name));
	return 1;
}

// auth_list_names()
int ModApiAuth::l_auth_list_names(lua_State *L)
{
	NO_MAP_LOCK_REQUIRED;
	AuthDatabase *auth_db = getAuthDb(L);
	if (!auth_db)
		return 0;
	std::vector<std::string> names;
	auth_db->listNames(names);
	lua_createtable(L, names.size(), 0);
	int table = lua_gettop(L);
	int i = 1;
	for (const std::string &name : names) {
		lua_pushstring(L, name.c_str());
		lua_rawseti(L, table, i++);
	}
	return 1;
}

// auth_reload()
int ModApiAuth::l_auth_reload(lua_State *L)
{
	NO_MAP_LOCK_REQUIRED;
	AuthDatabase *auth_db = getAuthDb(L);
	if (auth_db)
		auth_db->reload();
	return 0;
}

void ModApiAuth::Initialize(lua_State *L, int top)
{

	lua_newtable(L);
	int auth_top = lua_gettop(L);

	registerFunction(L, "read", l_auth_read, auth_top);
	registerFunction(L, "save", l_auth_save, auth_top);
	registerFunction(L, "create", l_auth_create, auth_top);
	registerFunction(L, "delete", l_auth_delete, auth_top);
	registerFunction(L, "list_names", l_auth_list_names, auth_top);
	registerFunction(L, "reload", l_auth_reload, auth_top);

	lua_setfield(L, top, "auth");
}
