// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2013 celeron55, Perttu Ahola <celeron55@gmail.com>
// Copyright (C) 2017 nerzhul, Loic Blot <loic.blot@unix-experience.fr>

#pragma once

#include "lua_api/l_base.h"
#include "itemdef.h"
#include "tool.h"

class ModApiClient : public ModApiBase
{
private:
	// get_current_modname()
	static int l_get_current_modname(lua_State *L);

	// get_modpath(modname)
	static int l_get_modpath(lua_State *L);

	// print(text)
	static int l_print(lua_State *L);

	// display_chat_message(message)
	static int l_display_chat_message(lua_State *L);

	// send_chat_message(message)
	static int l_send_chat_message(lua_State *L);

	// clear_out_chat_queue()
	static int l_clear_out_chat_queue(lua_State *L);

	// get_player_names()
	static int l_get_player_names(lua_State *L);

	// disconnect()
	static int l_disconnect(lua_State *L);

	// gettext(text)
	static int l_gettext(lua_State *L);

	// get_node(pos)
	static int l_get_node_or_nil(lua_State *L);

	// get_language()
	static int l_get_language(lua_State *L);

	// get_wielded_item()
	static int l_get_wielded_item(lua_State *L);

	// get_meta(pos)
	static int l_get_meta(lua_State *L);

	// get_server_info()
	static int l_get_server_info(lua_State *L);

	// get_item_def(itemstring)
	static int l_get_item_def(lua_State *L);

	// get_node_def(nodename)
	static int l_get_node_def(lua_State *L);

	// get_privilege_list()
	static int l_get_privilege_list(lua_State *L);

	// get_builtin_path()
	static int l_get_builtin_path(lua_State *L);

	// get_csm_restrictions()
	static int l_get_csm_restrictions(lua_State *L);

public:
	static void Initialize(lua_State *L, int top);
};
