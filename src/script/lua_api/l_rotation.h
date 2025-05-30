// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2025 Lars Müller

#pragma once

#include "irr_v3d.h"
#include "quaternion.h"

#include "lua_api/l_base.h"

class LuaRotation : public ModApiBase
{
private:
	core::quaternion quaternion;

	static const luaL_Reg methods[];

	// Conversions
	// Note: Matrix conversions are in l_matrix.h

	// self = identity()
	static int l_identity(lua_State *L);
	// self = quaternion(x, y, z, w)
	static int l_quaternion(lua_State *L);
	// self = axis_angle(axis, angle)
	static int l_axis_angle(lua_State *L);
	// self = x(angle); self = y(angle); self = z(angle)
	template<float v3f::* C>
	static int l_fixed_axis_angle(lua_State *L);
	// self = euler_angles(pitch, yaw, roll)
	static int l_euler_angles(lua_State *L);

	// x, y, z, w = to_quaternion(self)
	static int l_to_quaternion(lua_State *L);
	// axis, angle = to_axis_angle(self)
	static int l_to_axis_angle(lua_State *L);
	// pitch, yaw, roll = to_euler_angles(self)
	static int l_to_euler_angles(lua_State *L);

	// rotated_vector = apply(self, vector)
	static int l_apply(lua_State *L);
	// composition = compose(self, other)
	static int l_compose(lua_State *L);
	// inverse = invert(self)
	static int l_invert(lua_State *L);

	// slerped = slerp(from, to, time)
	static int l_slerp(lua_State *L);
	// angle = angle_to(to)
	static int l_angle_to(lua_State *L);

	// tostring(self)
	static int mt_tostring(lua_State *L);

	static void *packIn(lua_State *L, int idx);
	static void packOut(lua_State *L, void *ptr);


public:

	/// Constructor. Leaves the value on top of the stack.
	static void create(lua_State *L, const core::quaternion &quaternion);

	[[nodiscard]] static core::quaternion &check(lua_State *L, int index);

	static void Register(lua_State *L);

	static const char className[];
};
