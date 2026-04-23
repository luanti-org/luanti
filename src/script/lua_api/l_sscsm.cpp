// SPDX-FileCopyrightText: 2025 Luanti authors
//
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "l_sscsm.h"

#include "common/c_content.h"
#include "common/c_converter.h"
#include "l_internal.h"
#include "script/sscsm/sscsm_environment.h"
#include "script/sscsm/sscsm_requests.h"

// get_node_or_nil(pos)
// pos = {x=num, y=num, z=num}
int ModApiSSCSM::l_get_node_or_nil(lua_State *L)
{
	// pos
	v3s16 pos = read_v3s16(L, 1);

	// Do it
	auto request = SSCSMRequestGetNode{};
	request.pos = pos;
	auto answer = getSSCSMEnv(L)->doRequest(std::move(request));

	if (answer.is_pos_ok) {
		// Return node
		pushnode(L, answer.node);
	} else {
		lua_pushnil(L);
	}
	return 1;
}

// display_chat_message(text)
int ModApiSSCSM::l_display_chat_message(lua_State *L)
{
	if (!lua_isstring(L, 1))
		return 0;

	auto request = SSCSMRequestDisplayChatMessage{};
	request.text = luaL_checkstring(L, 1);
	getSSCSMEnv(L)->doRequest(std::move(request));

	lua_pushboolean(L, true);
	return 1;
}

// mod_channel_join(name) -> bool
int ModApiSSCSM::l_mod_channel_join(lua_State *L)
{
	auto request = SSCSMRequestJoinModChannel{};
	request.name = luaL_checkstring(L, 1);
	auto answer = getSSCSMEnv(L)->doRequest(std::move(request));
	lua_pushboolean(L, answer.ok);
	return 1;
}

// mod_channel_leave(name)
int ModApiSSCSM::l_mod_channel_leave(lua_State *L)
{
	auto request = SSCSMRequestLeaveModChannel{};
	request.name = luaL_checkstring(L, 1);
	getSSCSMEnv(L)->doRequest(std::move(request));
	return 0;
}

// mod_channel_send_all(channel, message) -> bool
int ModApiSSCSM::l_mod_channel_send_all(lua_State *L)
{
	auto request = SSCSMRequestSendModChannelMessage{};
	request.channel = luaL_checkstring(L, 1);
	request.message = luaL_checkstring(L, 2);
	auto answer = getSSCSMEnv(L)->doRequest(std::move(request));
	lua_pushboolean(L, answer.ok);
	return 1;
}

void ModApiSSCSM::Initialize(lua_State *L, int top)
{
	API_FCT(get_node_or_nil);
	API_FCT(display_chat_message);
	API_FCT(mod_channel_join);
	API_FCT(mod_channel_leave);
	API_FCT(mod_channel_send_all);
}
