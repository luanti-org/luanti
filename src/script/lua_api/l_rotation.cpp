// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2025 Lars Müller

#include "lua_api/l_rotation.h"
#include "common/c_packer.h"
#include "lua_api/l_internal.h"

#include "common/c_converter.h"
#include "irr_v3d.h"
#include "quaternion.h"

#include <lauxlib.h>
#include <lua.h>
#include <sstream>

core::quaternion &LuaRotation::check(lua_State *L, int index)
{
	return static_cast<LuaRotation *>(luaL_checkudata(L, index, className))->quaternion;
}

void LuaRotation::create(lua_State *L, const core::quaternion &quaternion)
{
	auto *rot = static_cast<LuaRotation *>(lua_newuserdata(L, sizeof(LuaRotation)));
	rot->quaternion = quaternion;
	luaL_getmetatable(L, LuaRotation::className);
	lua_setmetatable(L, -2);
}

// Constructors

int LuaRotation::l_identity(lua_State *L)
{
	create(L, core::quaternion());
	return 1;
}

int LuaRotation::l_quaternion(lua_State *L)
{
	f32 x = readFiniteParam<f32>(L, 1);
	f32 y = readFiniteParam<f32>(L, 2);
	f32 z = readFiniteParam<f32>(L, 3);
	f32 w = readFiniteParam<f32>(L, 4);
	core::quaternion q(x, y, z, w);
	q.normalize();
	create(L, q);
	return 1;
}

int LuaRotation::l_axis_angle(lua_State *L)
{
	v3f axis = readParam<v3f>(L, 1);
	f32 angle = readFiniteParam<f32>(L, 2);
	core::quaternion quaternion;
	axis.normalize();
	quaternion.fromAngleAxis(angle, axis);
	create(L, quaternion);
	return 1;
}

template<float v3f::* C>
int LuaRotation::l_fixed_axis_angle(lua_State *L)
{
	f32 angle = readFiniteParam<f32>(L, 1);
	v3f euler_angles;
	euler_angles.*C = angle;
	create(L, core::quaternion(euler_angles));
	return 1;
}

int LuaRotation::l_euler_angles(lua_State *L)
{
	f32 pitch = readFiniteParam<f32>(L, 1);
	f32 yaw = readFiniteParam<f32>(L, 2);
	f32 roll = readFiniteParam<f32>(L, 3);
	core::quaternion quaternion;
	quaternion.set(pitch, yaw, roll);
	create(L, quaternion);
	return 1;
}

int LuaRotation::l_to_quaternion(lua_State *L)
{
	const auto &q = check(L, 1);
	lua_pushnumber(L, q.X);
	lua_pushnumber(L, q.Y);
	lua_pushnumber(L, q.Z);
	lua_pushnumber(L, q.W);
	return 4;
}

int LuaRotation::l_to_axis_angle(lua_State *L)
{
	const auto q = check(L, 1);
	core::vector3df axis;
	f32 angle;
	q.toAngleAxis(angle, axis);
	push_v3f(L, axis);
	lua_pushnumber(L, angle);
	return 2;
}

int LuaRotation::l_to_euler_angles(lua_State *L)
{
	const auto &q = check(L, 1);
	core::vector3df euler;
	q.toEuler(euler);
	lua_pushnumber(L, euler.X);
	lua_pushnumber(L, euler.Y);
	lua_pushnumber(L, euler.Z);
	return 3;
}

// Math

int LuaRotation::l_apply(lua_State *L)
{
	const auto &q = check(L, 1);
	v3f vec = readParam<v3f>(L, 2);
	push_v3f(L, q * vec);
	return 1;
}

int LuaRotation::l_compose(lua_State *L)
{
	int n_args = lua_gettop(L);
	if (n_args == 0)
		return LuaRotation::l_identity(L);
	auto product = check(L, 1);
	for (int i = 2; i <= n_args; ++i) {
		product *= check(L, i);
	}
	create(L, product);
	return 1;
}

int LuaRotation::l_invert(lua_State *L)
{
	create(L, check(L, 1).makeInverse());
	return 1;
}

int LuaRotation::l_slerp(lua_State *L)
{
	const auto &from = check(L, 1);
	const auto &to = check(L, 2);
	f32 time = readFiniteParam<f32>(L, 3);
	core::quaternion result;
	result.slerp(from, to, time);
	create(L, result);
	return 1;
}

int LuaRotation::l_angle_to(lua_State *L)
{
	const auto &from = check(L, 1);
	const auto &to = check(L, 2);
	f32 angle = from.angleTo(to);
	lua_pushnumber(L, angle);
	return 1;
}

// Serialization

int LuaRotation::mt_tostring(lua_State *L)
{
	const auto &q = check(L, 1);
	std::stringstream ss;
	ss << q;
	std::string str = ss.str();
	lua_pushlstring(L, str.c_str(), str.size());
	return 1;
}

void *LuaRotation::packIn(lua_State *L, int idx)
{
	return new core::quaternion(check(L, idx));
}

void LuaRotation::packOut(lua_State *L, void *ptr)
{
	auto *quat = static_cast<core::quaternion *>(ptr);
	if (L)
		create(L, *quat);
	delete quat;
}

void LuaRotation::Register(lua_State *L)
{
	static const luaL_Reg metamethods[] = {
		{"__tostring", mt_tostring},
		{0, 0}
	};
	registerClass<LuaRotation>(L, methods, metamethods);

	lua_createtable(L, 0, 0);
	int Matrix4 = lua_gettop(L);

#define SET_CONSTRUCTOR(name, method) \
	lua_pushcfunction(L, method); \
	lua_setfield(L, Matrix4, name); \

#define CONSTRUCTOR(name) SET_CONSTRUCTOR(#name, l_##name)

	CONSTRUCTOR(identity)
	CONSTRUCTOR(quaternion)
	CONSTRUCTOR(axis_angle)
	CONSTRUCTOR(euler_angles)
	CONSTRUCTOR(compose)

#undef CONSTRUCTOR

	SET_CONSTRUCTOR("x", l_fixed_axis_angle<&v3f::X>)
	SET_CONSTRUCTOR("y", l_fixed_axis_angle<&v3f::Y>)
	SET_CONSTRUCTOR("z", l_fixed_axis_angle<&v3f::Z>)

#undef SET_CONSTRUCTOR

	lua_setglobal(L, className);

	script_register_packer(L, className, packIn, packOut);
}

const char LuaRotation::className[] = "Rotation";

#define METHOD(name) luamethod(LuaRotation, name)
const luaL_Reg LuaRotation::methods[] = {
	METHOD(to_quaternion),
	METHOD(to_axis_angle),
	METHOD(to_euler_angles),
	METHOD(apply),
	METHOD(compose),
	METHOD(invert),
	METHOD(slerp),
	METHOD(angle_to),
	{0,0}
};
#undef METHOD
