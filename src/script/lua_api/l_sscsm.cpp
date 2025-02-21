// SPDX-FileCopyrightText: 2025 Luanti authors
//
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "l_sscsm.h"

#include "common/c_content.h"
#include "common/c_converter.h"
#include "l_internal.h"
#include "log.h"
#include "script/sscsm/sscsm_environment.h"
#include "script/sscsm/sscsm_requests.h"
#include "mapnode.h"

// print(text)
int ModApiSSCSM::l_print(lua_State *L) //TODO: not core.print
{
	auto request = SSCSMRequestPrint{};
	request.text = luaL_checkstring(L, 1);
	getSSCSMEnv(L)->doRequest(std::move(request));

	return 0;
}

// log([level], text)
int ModApiSSCSM::l_log(lua_State *L)
{
	/*
	auto request = SSCSMRequestLog{};
	request.text = luaL_checkstring(L, 1);
	getSSCSMEnv(L)->doRequest(std::move(request));

	std::string_view text;
	LogLevel level = LL_NONE;
	if (lua_isnoneornil(L, 2)) {
		text = readParam<std::string_view>(L, 1);
	} else {
		auto name = readParam<std::string_view>(L, 1);
		text = readParam<std::string_view>(L, 2);
		// if (name == "deprecated") { //TODO
			// log_deprecated(L, text, 2);
			// return 0;
		// }
		level = Logger::stringToLevel(name);
		if (level == LL_MAX) {
			warningstream << "Tried to log at unknown level '" << name
				<< "'. Defaulting to \"none\"." << std::endl;
			level = LL_WARNING;
		}
	}
	g_logger.log(level, text);
	*/
	return 0;
}

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

void ModApiSSCSM::Initialize(lua_State *L, int top)
{
	API_FCT(print);
	API_FCT(log);
	API_FCT(get_node_or_nil);
}
