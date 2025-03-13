// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2018 nerzhul, Loic Blot <loic.blot@unix-experience.fr>

extern "C" {
#include <lauxlib.h>
}

#include "helper.h"
#include <cmath>
#include <irr_v2d.h>
#include <irr_v3d.h>
#include <string_view>
#include "c_converter.h"
#include "c_types.h"

/*
 * Read template functions
 */
template <>
bool LuaHelper::readParam(lua_State *L, int index)
{
	return lua_toboolean(L, index) != 0;
}

template <>
s16 LuaHelper::readParam(lua_State *L, int index)
{
	return luaL_checkinteger(L, index);
}

template <>
int LuaHelper::readParam(lua_State *L, int index)
{
	return luaL_checkinteger(L, index);
}

template <>
float LuaHelper::readParam(lua_State *L, int index)
{
	lua_Number v = luaL_checknumber(L, index);
	if (std::isnan(v) && std::isinf(v))
		throw LuaError("Invalid float value (NaN or infinity)");

	return static_cast<float>(v);
}

template <>
v2s16 LuaHelper::readParam(lua_State *L, int index)
{
	return read_v2s16(L, index);
}

template <>
v2f LuaHelper::readParam(lua_State *L, int index)
{
	return check_v2f(L, index);
}

template <>
v3f LuaHelper::readParam(lua_State *L, int index)
{
	return check_v3f(L, index);
}

template <>
std::string_view LuaHelper::readParam(lua_State *L, int index)
{
	size_t length;
	const char *str = luaL_checklstring(L, index, &length);
	return std::string_view(str, length);
}

template <>
std::string LuaHelper::readParam(lua_State *L, int index)
{
	auto sv = readParam<std::string_view>(L, index);
	return std::string(sv); // a copy
}
