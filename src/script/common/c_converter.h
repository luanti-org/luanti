// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2013 celeron55, Perttu Ahola <celeron55@gmail.com>


/******************************************************************************/
/******************************************************************************/
/* WARNING!!!! do NOT add this header in any include file or any code file    */
/*             not being a script/modapi file!!!!!!!!                         */
/******************************************************************************/
/******************************************************************************/
#pragma once

#include <vector>
#include <string>
#include <string_view>

#include "irrlichttypes_bloated.h"

extern "C" {
#include <lua.h>
}

std::string getstringfield_default(lua_State *L, int table,
		const char *fieldname, const std::string &default_);
bool getboolfield_default(lua_State *L, int table,
		const char *fieldname, bool default_);
float getfloatfield_default(lua_State *L, int table,
		const char *fieldname, float default_);
int getintfield_default(lua_State *L, int table,
		const char *fieldname, int default_);

bool check_field_or_nil(lua_State *L, int index, int type, const char *fieldname);

template<typename T>
bool getintfield(lua_State *L, int table, const char *fieldname, T &result)
{
	lua_getfield(L, table, fieldname);
	bool got = false;
	if (check_field_or_nil(L, -1, LUA_TNUMBER, fieldname)){
		result = lua_tointeger(L, -1);
		got = true;
	}
	lua_pop(L, 1);
	return got;
}

// Retrieve an v3s16 where all components are optional (falls back to default)
v3s16 getv3s16field_default(lua_State *L, int table,
		const char *fieldname, v3s16 default_);

bool getstringfield(lua_State *L, int table,
		const char *fieldname, std::string &result);
bool getstringfield(lua_State *L, int table,
		const char *fieldname, std::string_view &result);
size_t getstringlistfield(lua_State *L, int table,
		const char *fieldname, std::vector<std::string> *result);
bool getboolfield(lua_State *L, int table,
		const char *fieldname, bool &result);
bool getfloatfield(lua_State *L, int table,
		const char *fieldname, float &result);

void setstringfield(lua_State *L, int table,
		const char *fieldname, const std::string &value);
void setintfield(lua_State *L, int table,
		const char *fieldname, int value);
void setfloatfield(lua_State *L, int table,
		const char *fieldname, float value);
void setboolfield(lua_State *L, int table,
		const char *fieldname, bool value);

v3f checkFloatPos(lua_State *L, int index);
v2f check_v2f(lua_State *L, int index);
v3f check_v3f(lua_State *L, int index);
v3s16 check_v3s16(lua_State *L, int index);

// TODO: some day we should figure out the type-checking situation so it's done
// everywhere. (right now {x=true, y=false} as v2f is {0,0} with no warning)

/// @warning relaxed type-checking, prefer `check_v3f`.
v3f read_v3f(lua_State *L, int index);
/// @warning relaxed type-checking, prefer `check_v2f`.
v2f read_v2f(lua_State *L, int index);
/// @warning relaxed type-checking
v2s16 read_v2s16(lua_State *L, int index);
/// @warning relaxed type-checking
v2s32 read_v2s32(lua_State *L, int index);
/// @warning relaxed type-checking, prefer `check_v3s16`.
v3s16 read_v3s16(lua_State *L, int index);

video::SColor read_ARGB8(lua_State *L, int index);
bool read_color(lua_State *L, int index, video::SColor *color);
bool is_color_table(lua_State *L, int index);

/**
 * Read a floating-point axis-aligned box from Lua.
 *
 * @param  L the Lua state
 * @param  index the index of the Lua variable to read the box from. The
 *         variable must contain a table of the form
 *         {minx, miny, minz, maxx, maxy, maxz}.
 * @param  scale factor to scale the bounding box by
 *
 * @return the box corresponding to lua table
 */
aabb3f read_aabb3f(lua_State *L, int index, f32 scale);

std::vector<aabb3f> read_aabb3f_vector  (lua_State *L, int index, f32 scale);
size_t read_stringlist(lua_State *L, int index,
		std::vector<std::string> *result);

void push_v2s16(lua_State *L, v2s16 p);
void push_v2s32(lua_State *L, v2s32 p);
void push_v2u32(lua_State *L, v2u32 p);
void push_v3s16(lua_State *L, v3s16 p);
void push_aabb3f(lua_State *L, aabb3f box, f32 divisor = 1.0f);
void push_ARGB8(lua_State *L, video::SColor color);
void pushFloatPos(lua_State *L, v3f p);
void push_v3f(lua_State *L, v3f p);
void push_v2f(lua_State *L, v2f p);
void push_aabb3f_vector(lua_State *L, const std::vector<aabb3f> &boxes,
		f32 divisor = 1.0f);

size_t write_array_slice_float(lua_State *L, int table_index, float *data,
		v3u16 data_size, v3u16 slice_offset, v3u16 slice_size);

// This must match the implementation in builtin/game/misc_s.lua
// Note that this returns a floating point result as Lua integers are 32-bit
inline lua_Number hash_node_position(v3s16 pos)
{
	return (((s64)pos.Z + 0x8000L) << 32)
			| (((s64)pos.Y + 0x8000L) << 16)
			| ((s64)pos.X + 0x8000L);
}
