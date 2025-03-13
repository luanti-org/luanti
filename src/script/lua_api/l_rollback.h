// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2013 celeron55, Perttu Ahola <celeron55@gmail.com>

#pragma once

#include "lua_api/l_base.h"

class ModApiRollback : public ModApiBase
{
private:
	// rollback_get_node_actions(pos, range, seconds) -> {{actor, pos, time, oldnode, newnode}, ...}
	static int l_rollback_get_node_actions(lua_State *L);

	// rollback_revert_actions_by(actor, seconds) -> bool, log messages
	static int l_rollback_revert_actions_by(lua_State *L);

public:
	static void Initialize(lua_State *L, int top);
};
