// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2025 Lars Müller

#pragma once

#include "lua_api/l_base.h"
#include "matrix4.h"
#include <lua.h>

class LuaMatrix4 : public ModApiBase
{
private:
	core::matrix4 matrix;

	static const luaL_Reg methods[];

	// Exported functions

	// Constructors

	// identity()
	static int l_identity(lua_State *L);
	// all(number)
	static int l_all(lua_State *L);
	// translation(vec)
	static int l_translation(lua_State *L);
	// rotation(rot)
	static int l_rotation(lua_State *L);
	// reflection(normal)
	static int l_reflection(lua_State *L);
	// scale(vec)
	static int l_scale(lua_State *L);
	// new(a11, a12, ..., a44)
	static int l_new(lua_State *L);

	// Misc. container utils

	// get(self, row, column)
	static int l_get(lua_State *L);
	// set(self, row, column, value)
	static int l_set(lua_State *L);
	// x, y, z, w = get_row(self, row)
	static int l_get_row(lua_State *L);
	// set_row(self, row, x, y, z, w)
	static int l_set_row(lua_State *L);
	// x, y, z, w = get_column(self, column)
	static int l_get_column(lua_State *L);
	// set_column(self, column, x, y, z, w)
	static int l_set_column(lua_State *L);
	// copy(self)
	static int l_copy(lua_State *L);
	// unpack(self)
	static int l_unpack(lua_State *L);

	// x, y, z, w = transform_4d(self, x, y, z, w)
	static int l_transform_4d(lua_State *L);
	// transform_position(self, vector)
	static int l_transform_position(lua_State *L);
	// transform_direction(self, vector)
	static int l_transform_direction(lua_State *L);
	// compose(self, other)
	static int l_compose(lua_State *L);
	// determinant(self)
	static int l_determinant(lua_State *L);
	// invert(self)
	static int l_invert(lua_State *L);
	// transpose(self)
	static int l_transpose(lua_State *L);
	// equals(self, other, [tolerance])
	static int l_equals(lua_State *L);
	// is_affine_transform(self)
	static int l_is_affine_transform(lua_State *L);

	// get_translation(self)
	static int l_get_translation(lua_State *L);
	// rotation, scale = get_rs(self)
	static int l_get_rs(lua_State *L);

	// set_translation(self, translation)
	static int l_set_translation(lua_State *L);

	// set_rotation and set_scale are deliberately omitted
	// to nudge users not to decompose and recompose matrices.
	// Instead they should store TRS transforms and construct matrices from them.

	// m1 + m2
	static int mt_add(lua_State *L);
	// m1 - m2
	static int mt_sub(lua_State *L);
	// -m
	static int mt_unm(lua_State *L);
	// scalar * m; m * scalar
	static int mt_mul(lua_State *L);
	// m1 == m2
	static int mt_eq(lua_State *L);
	// tostring(m)
	static int mt_tostring(lua_State *L);

	static void *packIn(lua_State *L, int idx);
	static void packOut(lua_State *L, void *ptr);

	template<int max = 4>
	static int readIndex(lua_State *L, int index);

public:

	// Constructor. Leaves the value on top of the stack.
	// Returns a reference that *must* be overwritten.
	[[nodiscard]] static inline core::matrix4 &create(lua_State *L);

	[[nodiscard]] static core::matrix4 &check(lua_State *L, int index);

	static void Register(lua_State *L);

	static const char className[];
};
