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

// hud_add(form)
int ModApiSSCSM::l_hud_add(lua_State *L)
{
	SSCSMRequestHudAdd request;
	read_hud_element(L, &request.elem, 1);

	auto answer = getSSCSMEnv(L)->doRequest(std::move(request));

	lua_pushnumber(L, answer.id);
	return 1;
}

// hud_remove(id)
int ModApiSSCSM::l_hud_remove(lua_State *L)
{
	SSCSMRequestHudRemove request;
	request.id = luaL_checkinteger(L, 1);

	auto answer = getSSCSMEnv(L)->doRequest(std::move(request));

	lua_pushboolean(L, answer.ok);
	return 1;
}

// hud_change(id, stat, data)
int ModApiSSCSM::l_hud_change(lua_State *L)
{
	u32 id = luaL_checkinteger(L, 1);

	HudElementStat stat;
	if (!read_hud_stat_name(L, stat, 2)) {
		lua_pushboolean(L, false);
		return 1;
	}
	std::string statstr = lua_tostring(L, 2);

	SSCSMRequestHudGet get_request;
	get_request.id = id;
	auto get_answer = getSSCSMEnv(L)->doRequest(std::move(get_request));
	if (!get_answer.found) {
		lua_pushboolean(L, false);
		return 1;
	}

	SSCSMRequestHudChange change_request;
	change_request.id = id;
	change_request.stat = stat;
	change_request.value = read_hud_stat_value(L, stat, statstr, get_answer.elem.type, 3);
	auto change_answer = getSSCSMEnv(L)->doRequest(std::move(change_request));

	lua_pushboolean(L, change_answer.ok);
	return 1;
}

// hud_get(id)
int ModApiSSCSM::l_hud_get(lua_State *L)
{
	SSCSMRequestHudGet request;
	request.id = luaL_checkinteger(L, 1);

	auto answer = getSSCSMEnv(L)->doRequest(std::move(request));

	if (!answer.found) {
		lua_pushnil(L);
		return 1;
	}
	push_hud_element(L, &answer.elem);
	return 1;
}

// hud_get_all()
int ModApiSSCSM::l_hud_get_all(lua_State *L)
{
	auto answer = getSSCSMEnv(L)->doRequest(SSCSMRequestHudGetAll{});

	lua_newtable(L);
	for (auto &[id, elem] : answer.elems) {
		push_hud_element(L, &elem);
		lua_rawseti(L, -2, id);
	}
	return 1;
}

void ModApiSSCSM::Initialize(lua_State *L, int top)
{
	API_FCT(get_node_or_nil);
	API_FCT(hud_add);
	API_FCT(hud_remove);
	API_FCT(hud_change);
	API_FCT(hud_get);
	API_FCT(hud_get_all);
}
