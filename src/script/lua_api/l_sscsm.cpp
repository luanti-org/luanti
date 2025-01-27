// SPDX-FileCopyrightText: 2025 Luanti authors
//
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "l_sscsm.h"

#include "common/c_content.h"
#include "common/c_converter.h"
#include "l_internal.h"
#include "log.h"
#include "script/sscsm/sscsm_environment.h"
#include "mapnode.h"

// print(text)
int ModApiSSCSM::l_print(lua_State *L)
{
	// TODO: send request to main process
	std::string text = luaL_checkstring(L, 1);
	rawstream << text << std::endl;
	return 0;
}

// get_node_or_nil(pos)
// pos = {x=num, y=num, z=num}
int ModApiSSCSM::l_get_node_or_nil(lua_State *L)
{
	// pos
	v3s16 pos = read_v3s16(L, 1);

	// Do it
	bool pos_ok = true;
	MapNode n = getSSCSMEnv(L)->requestGetNode(pos); //TODO: add pos_ok to request
	if (pos_ok) {
		// Return node
		pushnode(L, n);
	} else {
		lua_pushnil(L);
	}
	return 1;
}

void ModApiSSCSM::Initialize(lua_State *L, int top)
{
	API_FCT(print);
	API_FCT(get_node_or_nil);
}
