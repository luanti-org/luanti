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

	// display_chat_message(text)
	static int l_display_chat_message(lua_State *L);

	// mod_channel_join(name) -> bool
	static int l_mod_channel_join(lua_State *L);
	// mod_channel_leave(name)
	static int l_mod_channel_leave(lua_State *L);
	// mod_channel_send_all(channel, message) -> bool
	static int l_mod_channel_send_all(lua_State *L);

public:
	static void Initialize(lua_State *L, int top);
};
