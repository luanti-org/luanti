// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2025 Lars Müller

#include "Transform.h"
#include "irrTypes.h"
#include "irr_v3d.h"
#include "matrix4.h"
#include "quaternion.h"

#include "lua_api/l_matrix4.h"
#include "lua_api/l_rotation.h"
#include "lua_api/l_internal.h"
#include "common/c_packer.h"
#include "common/c_converter.h"

#include <cmath>
#include <lauxlib.h>
#include <lua.h>
#include <sstream>

/// Read an index from 1 to MAX, convert to 0-based
template<int MAX>
int LuaMatrix4::readIndex(lua_State *L, int index)
{
	f64 value = readParam<f64>(L, index);
	if (std::floor(value) != value)
		luaL_argerror(L, index, "index must be integer");
	if (value < 1 || value > MAX)
		luaL_argerror(L, index, "index out of range");
	return static_cast<int>(value) - 1;
}

core::matrix4 &LuaMatrix4::check(lua_State *L, int index)
{
	return static_cast<LuaMatrix4 *>(luaL_checkudata(L, index, LuaMatrix4::className))->matrix;
}

inline core::matrix4 &LuaMatrix4::create(lua_State *L)
{
	auto *mat = static_cast<LuaMatrix4 *>(lua_newuserdata(L, sizeof(LuaMatrix4)));
	luaL_getmetatable(L, LuaMatrix4::className);
	lua_setmetatable(L, -2);
	return mat->matrix;
}

int LuaMatrix4::l_identity(lua_State *L)
{
	create(L) = core::matrix4();
	return 1;
}

int LuaMatrix4::l_full(lua_State *L)
{
	f32	v = readParam<f32>(L, 1);
	create(L) = v;
	return 1;
}

int LuaMatrix4::l_new(lua_State *L)
{
	if (lua_gettop(L) != 16)
		luaL_error(L, "expected 16 arguments");
	core::matrix4 &matrix = create(L);
	for (int i = 0; i < 16; ++i)
		matrix[i] = readParam<f32>(L, 1 + i);
	matrix = matrix.getTransposed();
	return 1;
}

int LuaMatrix4::l_translation(lua_State *L)
{
	v3f translation = readParam<v3f>(L, 1);
	core::matrix4 &matrix = create(L);
	matrix = core::matrix4();
	matrix.setTranslation(translation);
	return 1;
}

int LuaMatrix4::l_rotation(lua_State *L)
{
	const core::quaternion &rotation = LuaRotation::check(L, 1);
	core::matrix4 &matrix = create(L);
	rotation.getMatrix(matrix);
	return 1;
}

int LuaMatrix4::l_scale(lua_State *L)
{
	v3f scale = lua_isnumber(L, 1)
			? v3f(readParam<f32>(L, 1))
			: readParam<v3f>(L, 1);
	core::matrix4 &matrix = create(L);
	matrix = core::matrix4();
	matrix.setScale(scale);
	return 1;
}

int LuaMatrix4::l_trs(lua_State *L)
{
	v3f translation = lua_isnoneornil(L, 1) ? v3f() : readParam<v3f>(L, 1);
	core::quaternion rotation = lua_isnoneornil(L, 2)
			? core::quaternion()
			: LuaRotation::check(L, 2);
	v3f scale = lua_isnoneornil(L, 3)
			? v3f(1)
			: lua_isnumber(L, 3) ? v3f(readParam<f32>(L, 3)) : readParam<v3f>(L, 3);
	// FIXME core::Transform should use left-handed conventions
	core::Transform trs{translation, rotation.makeInverse(), scale};
	core::matrix4 &matrix = create(L);
	matrix = trs.buildMatrix();
	return 1;
}

int LuaMatrix4::l_reflection(lua_State *L)
{
	v3f normal = readParam<v3f>(L, 1);
	normal.normalize();
	core::matrix4 &matrix = create(L);
	matrix = core::matrix4::reflection(normal);
	return 1;
}

// Container utils
// Because we transpose in Matrix4.new, the notions of row and column are swapped.

int LuaMatrix4::l_get(lua_State *L)
{
	const auto &matrix = check(L, 1);
	int row = readIndex(L, 2);
	int col = readIndex(L, 3);
	lua_pushnumber(L, matrix(col, row));
	return 1;
}

int LuaMatrix4::l_set(lua_State *L)
{
	auto &matrix = check(L, 1);
	int row = readIndex(L, 2);
	int col = readIndex(L, 3);
	matrix(col, row) = readParam<f32>(L, 4);
	return 0;
}

int LuaMatrix4::l_get_row(lua_State *L)
{
	const auto &matrix = check(L, 1);
	int row = readIndex(L, 2);
	for (int col = 0; col < 4; ++col)
		lua_pushnumber(L, matrix(col, row));
	return 4;
}

int LuaMatrix4::l_set_row(lua_State *L)
{
	auto &matrix = check(L, 1);
	int row = readIndex(L, 2);
	f32 x = readParam<f32>(L, 3);
	f32 y = readParam<f32>(L, 4);
	f32 z = readParam<f32>(L, 5);
	f32 w = readParam<f32>(L, 6);
	matrix(0, row) = x;
	matrix(1, row) = y;
	matrix(2, row) = z;
	matrix(3, row) = w;
	return 0;
}

int LuaMatrix4::l_get_col(lua_State *L)
{
	const auto &matrix = check(L, 1);
	int col = readIndex(L, 2);
	for (int row = 0; row < 4; ++row)
		lua_pushnumber(L, matrix(col, row));
	return 4;
}

int LuaMatrix4::l_set_col(lua_State *L)
{
	auto &matrix = check(L, 1);
	int col = readIndex(L, 2);
	f32 x = readParam<f32>(L, 3);
	f32 y = readParam<f32>(L, 4);
	f32 z = readParam<f32>(L, 5);
	f32 w = readParam<f32>(L, 6);
	matrix(col, 0) = x;
	matrix(col, 1) = y;
	matrix(col, 2) = z;
	matrix(col, 3) = w;
	return 0;
}

int LuaMatrix4::l_unpack(lua_State *L)
{
	auto matrix = check(L, 1);
	matrix = matrix.getTransposed();
	lua_createtable(L, 16, 0);
	for (int i = 0; i < 16; ++i)
		lua_pushnumber(L, matrix[i]);
	return 16;
}

int LuaMatrix4::l_copy(lua_State *L)
{
	const auto &matrix = check(L, 1);
	create(L) = matrix;
	return 1;
}

// Linear algebra

int LuaMatrix4::l_transform_4d(lua_State *L)
{
	const auto &matrix = check(L, 1);
	f32 vec4[4];
	for (int i = 0; i < 4; ++i)
		vec4[i] = readParam<f32>(L, i + 2);
	f32 res[4];
	matrix.transformVec4(res, vec4);
	for (int i = 0; i < 4; ++i)
		lua_pushnumber(L, res[i]);
	return 4;
}

int LuaMatrix4::l_transform_pos(lua_State *L)
{
	const auto &matrix = check(L, 1);
	v3f vec = readParam<v3f>(L, 2);
	matrix.transformVect(vec);
	push_v3f(L, vec);
	return 1;
}

int LuaMatrix4::l_transform_dir(lua_State *L)
{
	const auto &matrix = check(L, 1);
	v3f vec = readParam<v3f>(L, 2);
	v3f res = matrix.rotateAndScaleVect(vec);
	push_v3f(L, res);
	return 1;
}

int LuaMatrix4::l_compose(lua_State *L)
{
	int n_args = lua_gettop(L);
	if (n_args == 0)
		return LuaMatrix4::l_identity(L);
	const auto &first = check(L, 1);
	auto &product = create(L);
	product = first;
	for (int i = 2; i <= n_args; ++i) {
		product *= check(L, i);
	}
	return 1;
}

int LuaMatrix4::l_transpose(lua_State *L)
{
	const auto &matrix = check(L, 1);
	create(L) = matrix.getTransposed();
	return 1;
}

int LuaMatrix4::l_determinant(lua_State *L)
{
	const auto &matrix = check(L, 1);
	lua_pushnumber(L, matrix.determinant());
	return 1;
}

int LuaMatrix4::l_invert(lua_State *L)
{
	const auto &matrix = check(L, 1);
	core::matrix4 inverse;
	if (!matrix.getInverse(inverse)) {
		lua_pushnil(L);
		return 1;
	}
	create(L) = inverse;
	return 1;
}

int LuaMatrix4::l_equals(lua_State *L)
{
	const auto &a = check(L, 1);
	const auto &b = check(L, 2);
	f32 tol = luaL_optnumber(L, 3, 0.0);
	lua_pushboolean(L, a.equals(b, tol));
	return 1;
}

int LuaMatrix4::l_is_affine_transform(lua_State *L)
{
	const auto &matrix = check(L, 1);
	f32 tol = luaL_optnumber(L, 3, 0.0);
	lua_pushboolean(L, matrix.isAffine(tol));
	return 1;
}

// Affine transform helpers

int LuaMatrix4::l_get_translation(lua_State *L)
{
	const auto &matrix = check(L, 1);
	push_v3f(L, matrix.getTranslation());
	return 1;
}

int LuaMatrix4::l_get_rs(lua_State *L)
{
	// TODO maybe check that it is, in fact, a rotation matrix;
	// not a fake rotation (axis flip) or a shear matrix
	auto matrix = check(L, 1);
	v3f scale = matrix.getScale();
	if (scale.X == 0.0f || scale.Y == 0.0f || scale.Z == 0.0f) {
		LuaRotation::create(L, core::quaternion());
		push_v3f(L, scale);
		return 2;
	}
	matrix.scaleAxes(v3f(1.0f) / scale);
	LuaRotation::create(L, core::quaternion(matrix));
	push_v3f(L, scale);
	return 2;
}

int LuaMatrix4::l_set_translation(lua_State *L)
{
	auto &matrix = check(L, 1);
	v3f translation = readParam<v3f>(L, 2);
	matrix.setTranslation(translation);
	return 0;
}

int LuaMatrix4::mt_add(lua_State *L)
{
	const auto &a = check(L, 1);
	const auto &b = check(L, 2);
	auto &res = create(L);
	res = a;
	res += b;
	return 1;
}

int LuaMatrix4::mt_sub(lua_State *L)
{
	const auto &a = check(L, 1);
	const auto &b = check(L, 2);
	auto &res = create(L);
	res = a;
	res -= b;
	return 1;
}

int LuaMatrix4::mt_unm(lua_State *L)
{
	const auto &matrix = check(L, 1);
	auto &res = create(L);
	res = matrix;
	res *= -1.0f;
	return 1;
}

int LuaMatrix4::mt_mul(lua_State *L)
{
	if (lua_isnumber(L, 1)) {
		f32 scalar = readParam<f32>(L, 1);
		const auto &matrix = check(L, 2);
		create(L) = scalar * matrix;
	} else {
		const auto &matrix = check(L, 1);
		f32 scalar = readParam<f32>(L, 2);
		create(L) = matrix * scalar;
	}
	return 1;
}

int LuaMatrix4::mt_eq(lua_State *L)
{
	const auto &a = check(L, 1);
	const auto &b = check(L, 2);
	lua_pushboolean(L, a == b);
	return 1;
}

int LuaMatrix4::mt_tostring(lua_State *L)
{
	const auto &matrix = check(L, 1);
	std::ostringstream ss;
	ss << matrix;
	std::string str = ss.str();
	lua_pushlstring(L, str.c_str(), str.size());
	return 1;
}

void *LuaMatrix4::packIn(lua_State *L, int idx)
{
	return new core::matrix4(check(L, idx));
}

void LuaMatrix4::packOut(lua_State *L, void *ptr)
{
	auto *matrix = static_cast<core::matrix4 *>(ptr);
	if (L)
		create(L) = *matrix;
	delete matrix;
}

void LuaMatrix4::Register(lua_State *L)
{
	static const luaL_Reg metamethods[] = {
		{"__tostring", mt_tostring},
		{"__add", mt_add},
		{"__sub", mt_sub},
		{"__unm", mt_unm},
		{"__mul", mt_mul},
		{"__eq", mt_eq},
		{0, 0}
	};
	registerClass<LuaMatrix4>(L, methods, metamethods);

	lua_createtable(L, 0, 0);
	int Matrix4 = lua_gettop(L);
#define CONSTRUCTOR(name) \
	lua_pushcfunction(L, l_##name); \
	lua_setfield(L, Matrix4, #name); \

	CONSTRUCTOR(new)
	CONSTRUCTOR(identity)
	CONSTRUCTOR(full)

	CONSTRUCTOR(translation)
	CONSTRUCTOR(rotation)
	CONSTRUCTOR(scale)
	CONSTRUCTOR(trs)
	CONSTRUCTOR(reflection)
	CONSTRUCTOR(compose)
#undef CONSTRUCTOR
	lua_setglobal(L, className);

	script_register_packer(L, className, packIn, packOut);
}

const char LuaMatrix4::className[] = "Matrix4";
#define METHOD(name) luamethod(LuaMatrix4, name)
const luaL_Reg LuaMatrix4::methods[] = {
	METHOD(get),
	METHOD(set),
	METHOD(get_row),
	METHOD(set_row),
	METHOD(get_column),
	METHOD(set_column),
	METHOD(copy),
	METHOD(unpack),
	METHOD(transform_4d),
	METHOD(transform_position),
	METHOD(transform_direction),
	METHOD(compose),
	METHOD(determinant),
	METHOD(invert),
	METHOD(transpose),
	METHOD(equals),
	METHOD(is_affine_transform),
	METHOD(get_translation),
	METHOD(get_rs),
	METHOD(set_translation),
	{0,0}
};
#undef METHOD
