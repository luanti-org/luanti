// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2013 celeron55, Perttu Ahola <celeron55@gmail.com>

extern "C" {
#include <lua.h>
#include <lauxlib.h>
}

#include "util/numeric.h"
#include "util/serialize.h"
#include "util/string.h"
#include "log.h"
#include "common/c_converter.h"
#include "common/c_internal.h"
#include "constants.h"
#include <set>
#include <cmath>
#include "common/c_types.h"

static v3d read_v3d(lua_State *L, int index);
static v3d check_v3d(lua_State *L, int index);

#define CHECK_TYPE(index, name, type) do { \
		int t = lua_type(L, (index)); \
		if (t != (type)) { \
			throw LuaError(std::string("Invalid ") + (name) + \
				" (expected " + lua_typename(L, (type)) + \
				" got " + lua_typename(L, t) + ")."); \
		} \
	} while(0)

// TODO: this should be turned into an error in 2026.
// Just revert the commit that added this line.
#define CHECK_NOT_NIL(index, name) do { \
		if (lua_isnoneornil(L, (index))) { \
			auto msg = std::string("Invalid ") + (name) + \
				" (value is nil)."; \
			log_deprecated(L, msg, 1, true); \
		} \
	} while(0)

#define CHECK_FLOAT(value, name) do {\
		if (std::isnan(value) || std::isinf(value)) { \
			throw LuaError("Invalid float value for '" name \
				"' (NaN or infinity)"); \
		} \
	} while (0)

// strictly check type of coordinate
// (this won't permit string-to-int conversion, so maybe not the best idea?)
#define CHECK_POS_COORD(index, name) CHECK_TYPE(index, "vector coordinate " name, LUA_TNUMBER)
// loosely check type of coordinate
#define CHECK_POS_COORD2(index, name) CHECK_NOT_NIL(index, "vector coordinate " name)

// Note: not needed when using read_v3_aux
#define CHECK_POS_TAB(index) CHECK_TYPE(index, "vector", LUA_TTABLE)


/**
 * A helper which calls CUSTOM_RIDX_READ_VECTOR with the argument at the given index
 */
static void read_v3_aux(lua_State *L, int index)
{
	// TODO: someone find out if it's faster to have the type check in Lua too
	CHECK_POS_TAB(index);
	lua_pushvalue(L, index);
	lua_rawgeti(L, LUA_REGISTRYINDEX, CUSTOM_RIDX_READ_VECTOR);
	lua_insert(L, -2);
	lua_call(L, 1, 3);
}

// Retrieve an integer vector where all components are optional
template<class T>
static bool getv3intfield(lua_State *L, int index,
		const char *fieldname, T &result)
{
	lua_getfield(L, index, fieldname);
	bool got = false;
	if (lua_istable(L, -1)) {
		got |= getintfield(L, -1, "x", result.X);
		got |= getintfield(L, -1, "y", result.Y);
		got |= getintfield(L, -1, "z", result.Z);
	}
	lua_pop(L, 1);
	return got;
}

void push_v3f(lua_State *L, v3f p)
{
	lua_rawgeti(L, LUA_REGISTRYINDEX, CUSTOM_RIDX_PUSH_VECTOR);
	lua_pushnumber(L, p.X);
	lua_pushnumber(L, p.Y);
	lua_pushnumber(L, p.Z);
	lua_call(L, 3, 1);
}

void push_v2f(lua_State *L, v2f p)
{
	lua_createtable(L, 0, 2);
	lua_pushnumber(L, p.X);
	lua_setfield(L, -2, "x");
	lua_pushnumber(L, p.Y);
	lua_setfield(L, -2, "y");
}

v2s16 read_v2s16(lua_State *L, int index)
{
	return v2s16::from(read_v2f(L, index));
}

void push_v2s16(lua_State *L, v2s16 p)
{
	push_v2s32(L, v2s32::from(p));
}

void push_v2s32(lua_State *L, v2s32 p)
{
	lua_createtable(L, 0, 2);
	lua_pushinteger(L, p.X);
	lua_setfield(L, -2, "x");
	lua_pushinteger(L, p.Y);
	lua_setfield(L, -2, "y");
}

void push_v2u32(lua_State *L, v2u32 p)
{
	lua_createtable(L, 0, 2);
	lua_pushinteger(L, p.X);
	lua_setfield(L, -2, "x");
	lua_pushinteger(L, p.Y);
	lua_setfield(L, -2, "y");
}

v2s32 read_v2s32(lua_State *L, int index)
{
	return v2s32::from(read_v2f(L, index));
}

v2f read_v2f(lua_State *L, int index)
{
	v2f p;
	CHECK_POS_TAB(index);
	lua_getfield(L, index, "x");
	CHECK_POS_COORD2(-1, "x");
	p.X = lua_tonumber(L, -1);
	lua_pop(L, 1);
	lua_getfield(L, index, "y");
	CHECK_POS_COORD2(-1, "y");
	p.Y = lua_tonumber(L, -1);
	lua_pop(L, 1);
	return p;
}

v2f check_v2f(lua_State *L, int index)
{
	v2f p;
	CHECK_POS_TAB(index);
	lua_getfield(L, index, "x");
	CHECK_POS_COORD(-1, "x");
	p.X = lua_tonumber(L, -1);
	CHECK_FLOAT(p.X, "x");
	lua_pop(L, 1);
	lua_getfield(L, index, "y");
	CHECK_POS_COORD(-1, "y");
	p.Y = lua_tonumber(L, -1);
	CHECK_FLOAT(p.Y, "y");
	lua_pop(L, 1);
	return p;
}

v3f read_v3f(lua_State *L, int index)
{
	return v3f::from(read_v3d(L, index));
}

v3f check_v3f(lua_State *L, int index)
{
	return v3f::from(check_v3d(L, index));
}

v3d read_v3d(lua_State *L, int index)
{
	read_v3_aux(L, index);
	CHECK_POS_COORD2(-3, "x");
	CHECK_POS_COORD2(-2, "y");
	CHECK_POS_COORD2(-1, "z");
	double x = lua_tonumber(L, -3);
	double y = lua_tonumber(L, -2);
	double z = lua_tonumber(L, -1);
	lua_pop(L, 3);
	return v3d(x, y, z);
}

v3d check_v3d(lua_State *L, int index)
{
	read_v3_aux(L, index);
	CHECK_POS_COORD(-3, "x");
	CHECK_POS_COORD(-2, "y");
	CHECK_POS_COORD(-1, "z");
	double x = lua_tonumber(L, -3);
	double y = lua_tonumber(L, -2);
	double z = lua_tonumber(L, -1);
	lua_pop(L, 3);
	CHECK_FLOAT(x, "x");
	CHECK_FLOAT(y, "y");
	CHECK_FLOAT(z, "z");
	return v3d(x, y, z);
}

void push_ARGB8(lua_State *L, video::SColor color)
{
	lua_createtable(L, 0, 4);
	lua_pushinteger(L, color.getAlpha());
	lua_setfield(L, -2, "a");
	lua_pushinteger(L, color.getRed());
	lua_setfield(L, -2, "r");
	lua_pushinteger(L, color.getGreen());
	lua_setfield(L, -2, "g");
	lua_pushinteger(L, color.getBlue());
	lua_setfield(L, -2, "b");
}

void pushFloatPos(lua_State *L, v3f p)
{
	p /= BS;
	push_v3f(L, p);
}

v3f checkFloatPos(lua_State *L, int index)
{
	return check_v3f(L, index) * BS;
}

void push_v3s16(lua_State *L, v3s16 p)
{
	lua_rawgeti(L, LUA_REGISTRYINDEX, CUSTOM_RIDX_PUSH_VECTOR);
	lua_pushinteger(L, p.X);
	lua_pushinteger(L, p.Y);
	lua_pushinteger(L, p.Z);
	lua_call(L, 3, 1);
}

v3s16 read_v3s16(lua_State *L, int index)
{
	// Correct rounding at <0
	v3d pf = read_v3d(L, index);
	return doubleToInt(pf, 1.0);
}

v3s16 check_v3s16(lua_State *L, int index)
{
	// Correct rounding at <0
	v3d pf = check_v3d(L, index);
	return doubleToInt(pf, 1.0);
}

bool read_color(lua_State *L, int index, video::SColor *color)
{
	if (lua_istable(L, index)) {
		*color = read_ARGB8(L, index);
	} else if (lua_isnumber(L, index)) {
		color->set(lua_tonumber(L, index));
	} else if (lua_isstring(L, index)) {
		video::SColor parsed_color;
		if (!parseColorString(lua_tostring(L, index), parsed_color, true))
			return false;

		*color = parsed_color;
	} else {
		return false;
	}

	return true;
}

video::SColor read_ARGB8(lua_State *L, int index)
{
	auto clamp_col = [](double c) -> u32 {
		return std::fmax(0.0, std::fmin(255.0, c));
	};

	// FIXME: maybe we should have strict type checks here. compare to is_color_table()

	video::SColor color(0);
	CHECK_TYPE(index, "ARGB color", LUA_TTABLE);
	lua_getfield(L, index, "a");
	color.setAlpha(lua_isnumber(L, -1) ? clamp_col(lua_tonumber(L, -1)) : 0xFF);
	lua_pop(L, 1);
	lua_getfield(L, index, "r");
	CHECK_NOT_NIL(-1, "color component R");
	color.setRed(clamp_col(lua_tonumber(L, -1)));
	lua_pop(L, 1);
	lua_getfield(L, index, "g");
	CHECK_NOT_NIL(-1, "color component G");
	color.setGreen(clamp_col(lua_tonumber(L, -1)));
	lua_pop(L, 1);
	lua_getfield(L, index, "b");
	CHECK_NOT_NIL(-1, "color component B");
	color.setBlue(clamp_col(lua_tonumber(L, -1)));
	lua_pop(L, 1);
	return color;
}

bool is_color_table(lua_State *L, int index)
{
	// Check whole table in case of missing ColorSpec keys:
	// This check does not remove the checked value from the stack.
	// Only update the value if we know we have a valid ColorSpec key pair.
	if (!lua_istable(L, index))
		return false;

	bool is_color_table = false;
	lua_getfield(L, index, "r");
	if (!is_color_table)
		is_color_table = lua_isnumber(L, -1);
	lua_getfield(L, index, "g");
	if (!is_color_table)
		is_color_table = lua_isnumber(L, -1);
	lua_getfield(L, index, "b");
	if (!is_color_table)
		is_color_table = lua_isnumber(L, -1);
	lua_pop(L, 3); // b, g, r values
	return is_color_table;
}

aabb3f read_aabb3f(lua_State *L, int index, f32 scale)
{
	// default value for accidental/historical reasons
	aabb3f box{-1.0f, -1.0f, -1.0f, 1.0f, 1.0f, 1.0f};
	if(lua_istable(L, index)){
		lua_rawgeti(L, index, 1);
		box.MinEdge.X = lua_tonumber(L, -1) * scale;
		lua_pop(L, 1);
		lua_rawgeti(L, index, 2);
		box.MinEdge.Y = lua_tonumber(L, -1) * scale;
		lua_pop(L, 1);
		lua_rawgeti(L, index, 3);
		box.MinEdge.Z = lua_tonumber(L, -1) * scale;
		lua_pop(L, 1);
		lua_rawgeti(L, index, 4);
		box.MaxEdge.X = lua_tonumber(L, -1) * scale;
		lua_pop(L, 1);
		lua_rawgeti(L, index, 5);
		box.MaxEdge.Y = lua_tonumber(L, -1) * scale;
		lua_pop(L, 1);
		lua_rawgeti(L, index, 6);
		box.MaxEdge.Z = lua_tonumber(L, -1) * scale;
		lua_pop(L, 1);
	}
	box.repair();
	return box;
}

void push_aabb3f(lua_State *L, aabb3f box, f32 divisor)
{
	lua_createtable(L, 6, 0);
	lua_pushnumber(L, box.MinEdge.X / divisor);
	lua_rawseti(L, -2, 1);
	lua_pushnumber(L, box.MinEdge.Y / divisor);
	lua_rawseti(L, -2, 2);
	lua_pushnumber(L, box.MinEdge.Z / divisor);
	lua_rawseti(L, -2, 3);
	lua_pushnumber(L, box.MaxEdge.X / divisor);
	lua_rawseti(L, -2, 4);
	lua_pushnumber(L, box.MaxEdge.Y / divisor);
	lua_rawseti(L, -2, 5);
	lua_pushnumber(L, box.MaxEdge.Z / divisor);
	lua_rawseti(L, -2, 6);
}

std::vector<aabb3f> read_aabb3f_vector(lua_State *L, int index, f32 scale)
{
	std::vector<aabb3f> boxes;
	if(lua_istable(L, index)){
		int n = lua_objlen(L, index);
		// Check if it's a single box or a list of boxes
		bool possibly_single_box = (n == 6);
		for(int i = 1; i <= n && possibly_single_box; i++){
			lua_rawgeti(L, index, i);
			if(!lua_isnumber(L, -1))
				possibly_single_box = false;
			lua_pop(L, 1);
		}
		if(possibly_single_box){
			// Read a single box
			boxes.push_back(read_aabb3f(L, index, scale));
		} else {
			// Read a list of boxes
			for(int i = 1; i <= n; i++){
				lua_rawgeti(L, index, i);
				boxes.push_back(read_aabb3f(L, -1, scale));
				lua_pop(L, 1);
			}
		}
	}
	return boxes;
}

void push_aabb3f_vector(lua_State *L, const std::vector<aabb3f> &boxes, f32 divisor)
{
	lua_createtable(L, boxes.size(), 0);
	int i = 1;
	for (const aabb3f &box : boxes) {
		push_aabb3f(L, box, divisor);
		lua_rawseti(L, -2, i++);
	}
}

size_t read_stringlist(lua_State *L, int index, std::vector<std::string> *result)
{
	if (index < 0)
		index = lua_gettop(L) + 1 + index;

	size_t num_strings = 0;

	if (lua_istable(L, index)) {
		lua_pushnil(L);
		while (lua_next(L, index)) {
			if (lua_isstring(L, -1)) {
				result->push_back(lua_tostring(L, -1));
				num_strings++;
			}
			lua_pop(L, 1);
		}
	} else if (lua_isstring(L, index)) {
		result->push_back(lua_tostring(L, index));
		num_strings++;
	}

	return num_strings;
}

/*
	Table field getters
*/

bool check_field_or_nil(lua_State *L, int index, int type, const char *fieldname)
{
	thread_local std::set<u64> warned_msgs;

	int t = lua_type(L, index);
	if (t == LUA_TNIL)
		return false;

	if (t == type)
		return true;

	// Check coercion types
	if (type == LUA_TNUMBER) {
		if (lua_isnumber(L, index))
			return true;
	} else if (type == LUA_TSTRING) {
		if (lua_isstring(L, index))
			return true;
	}

	// Types mismatch. Log unique line.
	std::string backtrace = std::string("Invalid field ") + fieldname +
		" (expected " + lua_typename(L, type) +
		" got " + lua_typename(L, t) + ").\n" + script_get_backtrace(L);

	u64 hash = murmur_hash_64_ua(backtrace.data(), backtrace.length(), 0xBADBABE);
	if (warned_msgs.find(hash) == warned_msgs.end()) {
		errorstream << backtrace << std::endl;
		warned_msgs.insert(hash);
	}

	return false;
}

bool getstringfield(lua_State *L, int table,
		const char *fieldname, std::string &result)
{
	std::string_view sv;
	if (getstringfield(L, table, fieldname, sv)) {
		result = sv;
		return true;
	}
	return false;
}

bool getstringfield(lua_State *L, int table,
		const char *fieldname, std::string_view &result)
{
	lua_getfield(L, table, fieldname);
	bool got = false;

	if (check_field_or_nil(L, -1, LUA_TSTRING, fieldname)) {
		size_t len = 0;
		const char *ptr = lua_tolstring(L, -1, &len);
		if (ptr) {
			result = std::string_view(ptr, len);
			got = true;
		}
	}
	lua_pop(L, 1);
	return got;
}

bool getfloatfield(lua_State *L, int table,
		const char *fieldname, float &result)
{
	lua_getfield(L, table, fieldname);
	bool got = false;

	if (check_field_or_nil(L, -1, LUA_TNUMBER, fieldname)) {
		result = lua_tonumber(L, -1);
		got = true;
	}
	lua_pop(L, 1);
	return got;
}

bool getboolfield(lua_State *L, int table,
		const char *fieldname, bool &result)
{
	lua_getfield(L, table, fieldname);
	bool got = false;

	if (check_field_or_nil(L, -1, LUA_TBOOLEAN, fieldname)){
		result = lua_toboolean(L, -1);
		got = true;
	}
	lua_pop(L, 1);
	return got;
}

size_t getstringlistfield(lua_State *L, int table, const char *fieldname,
		std::vector<std::string> *result)
{
	lua_getfield(L, table, fieldname);

	size_t num_strings_read = read_stringlist(L, -1, result);

	lua_pop(L, 1);
	return num_strings_read;
}

std::string getstringfield_default(lua_State *L, int table,
		const char *fieldname, const std::string &default_)
{
	std::string result = default_;
	getstringfield(L, table, fieldname, result);
	return result;
}

int getintfield_default(lua_State *L, int table,
		const char *fieldname, int default_)
{
	int result = default_;
	getintfield(L, table, fieldname, result);
	return result;
}

float getfloatfield_default(lua_State *L, int table,
		const char *fieldname, float default_)
{
	float result = default_;
	getfloatfield(L, table, fieldname, result);
	return result;
}

bool getboolfield_default(lua_State *L, int table,
		const char *fieldname, bool default_)
{
	bool result = default_;
	getboolfield(L, table, fieldname, result);
	return result;
}

v3s16 getv3s16field_default(lua_State *L, int table,
		const char *fieldname, v3s16 default_)
{
	getv3intfield(L, table, fieldname, default_);
	return default_;
}

void setstringfield(lua_State *L, int table,
		const char *fieldname, const std::string &value)
{
	lua_pushlstring(L, value.c_str(), value.length());
	if(table < 0)
		table -= 1;
	lua_setfield(L, table, fieldname);
}

void setintfield(lua_State *L, int table,
		const char *fieldname, int value)
{
	lua_pushinteger(L, value);
	if(table < 0)
		table -= 1;
	lua_setfield(L, table, fieldname);
}

void setfloatfield(lua_State *L, int table,
		const char *fieldname, float value)
{
	lua_pushnumber(L, value);
	if(table < 0)
		table -= 1;
	lua_setfield(L, table, fieldname);
}

void setboolfield(lua_State *L, int table,
		const char *fieldname, bool value)
{
	lua_pushboolean(L, value);
	if(table < 0)
		table -= 1;
	lua_setfield(L, table, fieldname);
}


////
//// Array table slices
////

size_t write_array_slice_float(
	lua_State *L,
	int table_index,
	float *data,
	v3u16 data_size,
	v3u16 slice_offset,
	v3u16 slice_size)
{
	v3u16 pmin, pmax(data_size);

	if (slice_offset.X > 0) {
		slice_offset.X--;
		pmin.X = slice_offset.X;
		pmax.X = MYMIN(slice_offset.X + slice_size.X, data_size.X);
	}

	if (slice_offset.Y > 0) {
		slice_offset.Y--;
		pmin.Y = slice_offset.Y;
		pmax.Y = MYMIN(slice_offset.Y + slice_size.Y, data_size.Y);
	}

	if (slice_offset.Z > 0) {
		slice_offset.Z--;
		pmin.Z = slice_offset.Z;
		pmax.Z = MYMIN(slice_offset.Z + slice_size.Z, data_size.Z);
	}

	const u32 ystride = data_size.X;
	const u32 zstride = data_size.X * data_size.Y;

	u32 elem_index = 1;
	for (u32 z = pmin.Z; z != pmax.Z; z++)
	for (u32 y = pmin.Y; y != pmax.Y; y++)
	for (u32 x = pmin.X; x != pmax.X; x++) {
		u32 i = z * zstride + y * ystride + x;
		lua_pushnumber(L, data[i]);
		lua_rawseti(L, table_index, elem_index);
		elem_index++;
	}

	return elem_index - 1;
}
