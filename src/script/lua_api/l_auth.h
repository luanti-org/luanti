// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2018 bendeutsch, Ben Deutsch <ben@bendeutsch.de>

#pragma once

#include "lua_api/l_base.h"

class AuthDatabase;
struct AuthEntry;

class ModApiAuth : public ModApiBase
{
private:
	// auth_read(name)
	static int l_auth_read(lua_State *L);

	// auth_save(table)
	static int l_auth_save(lua_State *L);

	// auth_create(table)
	static int l_auth_create(lua_State *L);

	// auth_delete(name)
	static int l_auth_delete(lua_State *L);

	// auth_list_names()
	static int l_auth_list_names(lua_State *L);

	// auth_reload()
	static int l_auth_reload(lua_State *L);

	// helper for auth* methods
	static AuthDatabase *getAuthDb(lua_State *L);
	static void pushAuthEntry(lua_State *L, const AuthEntry &authEntry);

public:
	static void Initialize(lua_State *L, int top);
};
