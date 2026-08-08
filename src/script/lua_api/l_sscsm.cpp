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

// core.send_chat_message_raw(message)
int ModApiSSCSM::l_send_chat_message_raw(lua_State *L)
{
	std::string message = luaL_checkstring(L, 1);

	auto request = SSCSMRequestSendChatMessageRaw{};
	request.message = std::move(message);
	auto answer = getSSCSMEnv(L)->doRequest(std::move(request));

	return 0;
}

void ModApiSSCSM::Initialize(lua_State *L, int top)
{
	API_FCT(get_node_or_nil);
	API_FCT(send_chat_message_raw);
}
