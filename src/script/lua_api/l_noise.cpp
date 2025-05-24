// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2013 celeron55, Perttu Ahola <celeron55@gmail.com>

#include "lua_api/l_noise.h"
#include "lua_api/l_internal.h"
#include "common/c_converter.h"
#include "common/c_content.h"
#include "common/c_packer.h"
#include "log.h"
#include "porting.h"
#include "util/numeric.h"

///////////////////////////////////////
/*
  LuaValueNoise
*/

LuaValueNoise::LuaValueNoise(const NoiseParams *params) :
	np(*params)
{
}


int LuaValueNoise::l_get_2d(lua_State *L)
{
	NO_MAP_LOCK_REQUIRED;
	LuaValueNoise *o = checkObject<LuaValueNoise>(L, 1);
	v2f p = readParam<v2f>(L, 2);
	lua_Number val = NoiseFractal2D(&o->np, p.X, p.Y, 0);
	lua_pushnumber(L, val);
	return 1;
}


int LuaValueNoise::l_get_3d(lua_State *L)
{
	NO_MAP_LOCK_REQUIRED;
	LuaValueNoise *o = checkObject<LuaValueNoise>(L, 1);
	v3f p = check_v3f(L, 2);
	lua_Number val = NoiseFractal3D(&o->np, p.X, p.Y, p.Z, 0);
	lua_pushnumber(L, val);
	return 1;
}


int LuaValueNoise::create_object(lua_State *L)
{
	NO_MAP_LOCK_REQUIRED;

	NoiseParams params;

	if (lua_istable(L, 1)) {
		read_noiseparams(L, 1, &params);
	} else {
		params.seed    = luaL_checkint(L, 1);
		params.octaves = luaL_checkint(L, 2);
		params.persist = readParam<float>(L, 3);
		params.spread  = v3f(1, 1, 1) * readParam<float>(L, 4);
	}

	LuaValueNoise *o = new LuaValueNoise(&params);

	*(void **)(lua_newuserdata(L, sizeof(void *))) = o;
	luaL_getmetatable(L, className);
	lua_setmetatable(L, -2);
	return 1;
}


int LuaValueNoise::gc_object(lua_State *L)
{
	LuaValueNoise *o = *(LuaValueNoise **)(lua_touserdata(L, 1));
	delete o;
	return 0;
}


void *LuaValueNoise::packIn(lua_State *L, int idx)
{
	LuaValueNoise *o = checkObject<LuaValueNoise>(L, idx);
	return new NoiseParams(o->np);
}

void LuaValueNoise::packOut(lua_State *L, void *ptr)
{
	NoiseParams *np = reinterpret_cast<NoiseParams*>(ptr);
	if (L) {
		LuaValueNoise *o = new LuaValueNoise(np);
		*(void **)(lua_newuserdata(L, sizeof(void *))) = o;
		luaL_getmetatable(L, className);
		lua_setmetatable(L, -2);
	}
	delete np;
}


void LuaValueNoise::Register(lua_State *L)
{
	static const luaL_Reg metamethods[] = {
		{"__gc", gc_object},
		{0, 0}
	};
	registerClass<LuaValueNoise>(L, methods, metamethods);

	lua_register(L, className, create_object);

	script_register_packer(L, className, packIn, packOut);
}


const char LuaValueNoise::className[] = "ValueNoise";
luaL_Reg LuaValueNoise::methods[] = {
	luamethod_aliased(LuaValueNoise, get_2d, get2d),
	luamethod_aliased(LuaValueNoise, get_3d, get3d),
	{0,0}
};

///////////////////////////////////////
/*
  LuaValueNoiseMap
*/

LuaValueNoiseMap::LuaValueNoiseMap(const NoiseParams *np, s32 seed, v3s16 size)
{
	try {
		noise = new Noise(np, seed, size.X, size.Y, size.Z);
	} catch (InvalidNoiseParamsException &e) {
		throw LuaError(e.what());
	}
}


LuaValueNoiseMap::~LuaValueNoiseMap()
{
	delete noise;
}


int LuaValueNoiseMap::l_get_2d_map(lua_State *L)
{
	NO_MAP_LOCK_REQUIRED;
	size_t i = 0;

	LuaValueNoiseMap *o = checkObject<LuaValueNoiseMap>(L, 1);
	v2f p = readParam<v2f>(L, 2);

	Noise *n = o->noise;
	n->noiseMap2D(p.X, p.Y);

	lua_createtable(L, n->sy, 0);
	for (u32 y = 0; y != n->sy; y++) {
		lua_createtable(L, n->sx, 0);
		for (u32 x = 0; x != n->sx; x++) {
			lua_pushnumber(L, n->result[i++]);
			lua_rawseti(L, -2, x + 1);
		}
		lua_rawseti(L, -2, y + 1);
	}
	return 1;
}


int LuaValueNoiseMap::l_get_2d_map_flat(lua_State *L)
{
	NO_MAP_LOCK_REQUIRED;

	LuaValueNoiseMap *o = checkObject<LuaValueNoiseMap>(L, 1);
	v2f p = readParam<v2f>(L, 2);
	bool use_buffer = lua_istable(L, 3);

	Noise *n = o->noise;
	n->noiseMap2D(p.X, p.Y);

	size_t maplen = n->sx * n->sy;

	if (use_buffer)
		lua_pushvalue(L, 3);
	else
		lua_createtable(L, maplen, 0);

	for (size_t i = 0; i != maplen; i++) {
		lua_pushnumber(L, n->result[i]);
		lua_rawseti(L, -2, i + 1);
	}
	return 1;
}


int LuaValueNoiseMap::l_get_3d_map(lua_State *L)
{
	NO_MAP_LOCK_REQUIRED;
	size_t i = 0;

	LuaValueNoiseMap *o = checkObject<LuaValueNoiseMap>(L, 1);
	v3f p = check_v3f(L, 2);

	if (!o->is3D())
		return 0;

	Noise *n = o->noise;
	n->noiseMap3D(p.X, p.Y, p.Z);

	lua_createtable(L, n->sz, 0);
	for (u32 z = 0; z != n->sz; z++) {
		lua_createtable(L, n->sy, 0);
		for (u32 y = 0; y != n->sy; y++) {
			lua_createtable(L, n->sx, 0);
			for (u32 x = 0; x != n->sx; x++) {
				lua_pushnumber(L, n->result[i++]);
				lua_rawseti(L, -2, x + 1);
			}
			lua_rawseti(L, -2, y + 1);
		}
		lua_rawseti(L, -2, z + 1);
	}
	return 1;
}


int LuaValueNoiseMap::l_get_3d_map_flat(lua_State *L)
{
	NO_MAP_LOCK_REQUIRED;

	LuaValueNoiseMap *o = checkObject<LuaValueNoiseMap>(L, 1);
	v3f p                = check_v3f(L, 2);
	bool use_buffer      = lua_istable(L, 3);

	if (!o->is3D())
		return 0;

	Noise *n = o->noise;
	n->noiseMap3D(p.X, p.Y, p.Z);

	size_t maplen = n->sx * n->sy * n->sz;

	if (use_buffer)
		lua_pushvalue(L, 3);
	else
		lua_createtable(L, maplen, 0);

	for (size_t i = 0; i != maplen; i++) {
		lua_pushnumber(L, n->result[i]);
		lua_rawseti(L, -2, i + 1);
	}
	return 1;
}


int LuaValueNoiseMap::l_calc_2d_map(lua_State *L)
{
	NO_MAP_LOCK_REQUIRED;

	LuaValueNoiseMap *o = checkObject<LuaValueNoiseMap>(L, 1);
	v2f p = readParam<v2f>(L, 2);

	Noise *n = o->noise;
	n->noiseMap2D(p.X, p.Y);

	return 0;
}

int LuaValueNoiseMap::l_calc_3d_map(lua_State *L)
{
	NO_MAP_LOCK_REQUIRED;

	LuaValueNoiseMap *o = checkObject<LuaValueNoiseMap>(L, 1);
	v3f p                = check_v3f(L, 2);

	if (!o->is3D())
		return 0;

	Noise *n = o->noise;
	n->noiseMap3D(p.X, p.Y, p.Z);

	return 0;
}


int LuaValueNoiseMap::l_get_map_slice(lua_State *L)
{
	NO_MAP_LOCK_REQUIRED;

	LuaValueNoiseMap *o = checkObject<LuaValueNoiseMap>(L, 1);
	v3s16 slice_offset   = read_v3s16(L, 2);
	v3s16 slice_size     = read_v3s16(L, 3);
	bool use_buffer      = lua_istable(L, 4);

	Noise *n = o->noise;

	if (use_buffer)
		lua_pushvalue(L, 4);
	else
		lua_newtable(L);

	write_array_slice_float(L, lua_gettop(L), n->result,
		v3u16(n->sx, n->sy, n->sz),
		v3u16(slice_offset.X, slice_offset.Y, slice_offset.Z),
		v3u16(slice_size.X, slice_size.Y, slice_size.Z));

	return 1;
}


int LuaValueNoiseMap::create_object(lua_State *L)
{
	NoiseParams np;
	if (!read_noiseparams(L, 1, &np))
		return 0;
	v3s16 size = read_v3s16(L, 2);

	LuaValueNoiseMap *o = new LuaValueNoiseMap(&np, 0, size);
	*(void **)(lua_newuserdata(L, sizeof(void *))) = o;
	luaL_getmetatable(L, className);
	lua_setmetatable(L, -2);
	return 1;
}


int LuaValueNoiseMap::gc_object(lua_State *L)
{
	LuaValueNoiseMap *o = *(LuaValueNoiseMap **)(lua_touserdata(L, 1));
	delete o;
	return 0;
}


struct NoiseMapParams {
	NoiseParams np;
	s32 seed;
	v3s16 size;
};

void *LuaValueNoiseMap::packIn(lua_State *L, int idx)
{
	LuaValueNoiseMap *o = checkObject<LuaValueNoiseMap>(L, idx);
	NoiseMapParams *ret = new NoiseMapParams();
	ret->np = o->noise->np;
	ret->seed = o->noise->seed;
	ret->size = v3s16(o->noise->sx, o->noise->sy, o->noise->sz);
	return ret;
}

void LuaValueNoiseMap::packOut(lua_State *L, void *ptr)
{
	NoiseMapParams *p = reinterpret_cast<NoiseMapParams*>(ptr);
	if (L) {
		LuaValueNoiseMap *o = new LuaValueNoiseMap(&p->np, p->seed, p->size);
		*(void **)(lua_newuserdata(L, sizeof(void *))) = o;
		luaL_getmetatable(L, className);
		lua_setmetatable(L, -2);
	}
	delete p;
}


void LuaValueNoiseMap::Register(lua_State *L)
{
	static const luaL_Reg metamethods[] = {
		{"__gc", gc_object},
		{0, 0}
	};
	registerClass<LuaValueNoiseMap>(L, methods, metamethods);

	lua_register(L, className, create_object);

	script_register_packer(L, className, packIn, packOut);
}


const char LuaValueNoiseMap::className[] = "ValueNoiseMap";
luaL_Reg LuaValueNoiseMap::methods[] = {
	luamethod_aliased(LuaValueNoiseMap, get_2d_map,      get2dMap),
	luamethod_aliased(LuaValueNoiseMap, get_2d_map_flat, get2dMap_flat),
	luamethod_aliased(LuaValueNoiseMap, calc_2d_map,     calc2dMap),
	luamethod_aliased(LuaValueNoiseMap, get_3d_map,      get3dMap),
	luamethod_aliased(LuaValueNoiseMap, get_3d_map_flat, get3dMap_flat),
	luamethod_aliased(LuaValueNoiseMap, calc_3d_map,     calc3dMap),
	luamethod_aliased(LuaValueNoiseMap, get_map_slice,   getMapSlice),
	{0,0}
};

///////////////////////////////////////
/*
	LuaPseudoRandom
*/

int LuaPseudoRandom::l_next(lua_State *L)
{
	NO_MAP_LOCK_REQUIRED;

	LuaPseudoRandom *o = checkObject<LuaPseudoRandom>(L, 1);
	int min = 0, max = PseudoRandom::RANDOM_RANGE;
	if (lua_isnumber(L, 2))
		min = luaL_checkinteger(L, 2);
	if (lua_isnumber(L, 3))
		max = luaL_checkinteger(L, 3);

	int val;
	if (max - min == PseudoRandom::RANDOM_RANGE) {
		val = o->m_pseudo.next() + min;
	} else {
		try {
			val = o->m_pseudo.range(min, max);
		} catch (PrngException &e) {
			throw LuaError(e.what());
		}
	}
	lua_pushinteger(L, val);
	return 1;
}

int LuaPseudoRandom::l_get_state(lua_State *L)
{
	NO_MAP_LOCK_REQUIRED;

	LuaPseudoRandom *o = checkObject<LuaPseudoRandom>(L, 1);
	PseudoRandom &pseudo = o->m_pseudo;
	int val = pseudo.getState();
	lua_pushinteger(L, val);
	return 1;
}


int LuaPseudoRandom::create_object(lua_State *L)
{
	NO_MAP_LOCK_REQUIRED;

	s32 seed = luaL_checkinteger(L, 1);
	LuaPseudoRandom *o = new LuaPseudoRandom(seed);
	*(void **)(lua_newuserdata(L, sizeof(void *))) = o;
	luaL_getmetatable(L, className);
	lua_setmetatable(L, -2);
	return 1;
}


int LuaPseudoRandom::gc_object(lua_State *L)
{
	LuaPseudoRandom *o = *(LuaPseudoRandom **)(lua_touserdata(L, 1));
	delete o;
	return 0;
}


void LuaPseudoRandom::Register(lua_State *L)
{
	static const luaL_Reg metamethods[] = {
		{"__gc", gc_object},
		{0, 0}
	};
	registerClass<LuaPseudoRandom>(L, methods, metamethods);

	lua_register(L, className, create_object);
}


const char LuaPseudoRandom::className[] = "PseudoRandom";
const luaL_Reg LuaPseudoRandom::methods[] = {
	luamethod(LuaPseudoRandom, next),
	luamethod(LuaPseudoRandom, get_state),
	{0,0}
};

///////////////////////////////////////
/*
	LuaPcgRandom
*/

int LuaPcgRandom::l_next(lua_State *L)
{
	NO_MAP_LOCK_REQUIRED;

	LuaPcgRandom *o = checkObject<LuaPcgRandom>(L, 1);
	u32 min = lua_isnumber(L, 2) ? lua_tointeger(L, 2) : o->m_rnd.RANDOM_MIN;
	u32 max = lua_isnumber(L, 3) ? lua_tointeger(L, 3) : o->m_rnd.RANDOM_MAX;

	lua_pushinteger(L, o->m_rnd.range(min, max));
	return 1;
}


int LuaPcgRandom::l_rand_normal_dist(lua_State *L)
{
	NO_MAP_LOCK_REQUIRED;

	LuaPcgRandom *o = checkObject<LuaPcgRandom>(L, 1);
	u32 min = lua_isnumber(L, 2) ? lua_tointeger(L, 2) : o->m_rnd.RANDOM_MIN;
	u32 max = lua_isnumber(L, 3) ? lua_tointeger(L, 3) : o->m_rnd.RANDOM_MAX;
	int num_trials = lua_isnumber(L, 4) ? lua_tointeger(L, 4) : 6;

	lua_pushinteger(L, o->m_rnd.randNormalDist(min, max, num_trials));
	return 1;
}

int LuaPcgRandom::l_get_state(lua_State *L)
{
	NO_MAP_LOCK_REQUIRED;

	LuaPcgRandom *o = checkObject<LuaPcgRandom>(L, 1);

	u64 state[2];
	o->m_rnd.getState(state);

	std::ostringstream oss;
	oss << std::hex << std::setw(16) << std::setfill('0')
		<< state[0] << state[1];

	lua_pushstring(L, oss.str().c_str());
	return 1;
}

int LuaPcgRandom::l_set_state(lua_State *L)
{
	NO_MAP_LOCK_REQUIRED;

	LuaPcgRandom *o = checkObject<LuaPcgRandom>(L, 1);

	std::string l_string = readParam<std::string>(L, 2);
	if (l_string.size() != 32) {
		throw LuaError("PcgRandom:set_state: Expected hex string of 32 characters");
	}

	std::istringstream s_state_0(l_string.substr(0, 16));
	std::istringstream s_state_1(l_string.substr(16, 16));

	u64 state[2];
	s_state_0 >> std::hex >> state[0];
	s_state_1 >> std::hex >> state[1];

	o->m_rnd.setState(state);

	return 0;
}

int LuaPcgRandom::create_object(lua_State *L)
{
	NO_MAP_LOCK_REQUIRED;

	u64 seed = luaL_checknumber(L, 1);
	LuaPcgRandom *o = lua_isnumber(L, 2) ?
		new LuaPcgRandom(seed, lua_tointeger(L, 2)) :
		new LuaPcgRandom(seed);
	*(void **)(lua_newuserdata(L, sizeof(void *))) = o;
	luaL_getmetatable(L, className);
	lua_setmetatable(L, -2);
	return 1;
}


int LuaPcgRandom::gc_object(lua_State *L)
{
	LuaPcgRandom *o = *(LuaPcgRandom **)(lua_touserdata(L, 1));
	delete o;
	return 0;
}


void LuaPcgRandom::Register(lua_State *L)
{
	static const luaL_Reg metamethods[] = {
		{"__gc", gc_object},
		{0, 0}
	};
	registerClass<LuaPcgRandom>(L, methods, metamethods);

	lua_register(L, className, create_object);
}


const char LuaPcgRandom::className[] = "PcgRandom";
const luaL_Reg LuaPcgRandom::methods[] = {
	luamethod(LuaPcgRandom, next),
	luamethod(LuaPcgRandom, rand_normal_dist),
	luamethod(LuaPcgRandom, get_state),
	luamethod(LuaPcgRandom, set_state),
	{0,0}
};

///////////////////////////////////////
/*
	LuaSecureRandom
*/

bool LuaSecureRandom::fillRandBuf()
{
	return porting::secure_rand_fill_buf(m_rand_buf, RAND_BUF_SIZE);
}

int LuaSecureRandom::l_next_bytes(lua_State *L)
{
	NO_MAP_LOCK_REQUIRED;

	LuaSecureRandom *o = checkObject<LuaSecureRandom>(L, 1);
	u32 count = lua_isnumber(L, 2) ? lua_tointeger(L, 2) : 1;

	// Limit count
	count = MYMIN(RAND_BUF_SIZE, count);

	// Find out whether we can pass directly from our array, or have to do some gluing
	size_t count_remaining = RAND_BUF_SIZE - o->m_rand_idx;
	if (count_remaining >= count) {
		lua_pushlstring(L, o->m_rand_buf + o->m_rand_idx, count);
		o->m_rand_idx += count;
	} else {
		char output_buf[RAND_BUF_SIZE];

		// Copy over with what we have left from our current buffer
		memcpy(output_buf, o->m_rand_buf + o->m_rand_idx, count_remaining);

		// Refill buffer and copy over the remainder of what was requested
		o->fillRandBuf();
		memcpy(output_buf + count_remaining, o->m_rand_buf, count - count_remaining);

		// Update index
		o->m_rand_idx = count - count_remaining;

		lua_pushlstring(L, output_buf, count);
	}

	return 1;
}


int LuaSecureRandom::create_object(lua_State *L)
{
	LuaSecureRandom *o = new LuaSecureRandom();

	if (!o->fillRandBuf()) {
		delete o;
		throw LuaError("SecureRandom: Failed to find secure random device on system");
	}

	*(void **)(lua_newuserdata(L, sizeof(void *))) = o;
	luaL_getmetatable(L, className);
	lua_setmetatable(L, -2);
	return 1;
}


int LuaSecureRandom::gc_object(lua_State *L)
{
	LuaSecureRandom *o = *(LuaSecureRandom **)(lua_touserdata(L, 1));
	delete o;
	return 0;
}


void LuaSecureRandom::Register(lua_State *L)
{
	static const luaL_Reg metamethods[] = {
		{"__gc", gc_object},
		{0, 0}
	};
	registerClass<LuaSecureRandom>(L, methods, metamethods);

	lua_register(L, className, create_object);
}

const char LuaSecureRandom::className[] = "SecureRandom";
const luaL_Reg LuaSecureRandom::methods[] = {
	luamethod(LuaSecureRandom, next_bytes),
	{0,0}
};
