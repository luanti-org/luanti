// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2018 nerzhul, Loic Blot <loic.blot@unix-experience.fr>

#pragma once

#include <string_view>

extern "C" {
#include <lua.h>
}

class LuaHelper
{
protected:
	/**
	 * Read a value using a template type T from Lua state L at index
	 *
	 * @tparam T type to read from Lua
	 * @param L Lua state
	 * @param index Lua index to read
	 * @return read value from Lua
	 */
	template <typename T>
	static T readParam(lua_State *L, int index);

	/**
	 * Read a value using a template type T from Lua state L at index
	 *
	 * @tparam T type to read from Lua
	 * @param L Lua state
	 * @param index Lua index to read
	 * @param default_value default value to apply if nil
	 * @return read value from Lua or default value if nil
	 */
	template <typename T>
	static inline T readParam(lua_State *L, int index, const T &default_value)
	{
		return lua_isnoneornil(L, index) ? default_value : readParam<T>(L, index);
	}
};

// (only declared for documentation purposes:)

/**
 * Read a string from Lua state L at index without copying it.
 *
 * Note that the returned string view is only valid as long as the value is on
 * the stack and has not been modified. Be careful.
 *
 * @param L Lua state
 * @param index Lua index to read
 * @return string view
 */
template <>
std::string_view LuaHelper::readParam(lua_State *L, int index);
