// SPDX-FileCopyrightText: 2025 Luanti authors
//
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include "lua_api/l_base.h"

class ModApiSSCSM : public ModApiBase
{
private:
	// get_node_or_nil(pos)
	static int l_get_node_or_nil(lua_State *L);

	// core.display_chat_message(message)
	static int l_display_chat_message(lua_State *L);

	// core.send_chat_message_raw(message)
	static int l_send_chat_message_raw(lua_State *L);

	// core.get_local_player_privs()
	static int l_get_local_player_privs(lua_State *L);

public:
	static void Initialize(lua_State *L, int top);
};
