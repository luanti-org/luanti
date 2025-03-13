// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2021 velartrill, Lexi Hale <lexi@hale.su>

#pragma once
#include "lua_api/l_particles.h"
#include "lua_api/l_object.h"
#include "lua_api/l_internal.h"
#include "common/c_converter.h"
#include "common/c_content.h"
#include "server.h"
#include "particles.h"

namespace LuaParticleParams
{
	using namespace ParticleParamTypes;

	template<typename T>
	inline void readNumericLuaValue(lua_State* L, T& ret)
	{
		if (lua_isnil(L,-1))
			return;

		if (std::is_integral<T>())
			ret = lua_tointeger(L, -1);
		else
			ret = lua_tonumber(L, -1);
	}

	template <typename T, size_t N>
	inline void readNumericLuaValue(lua_State* L, Parameter<T,N>& ret)
	{
		readNumericLuaValue<T>(L, ret.val);
	}

	// these are unfortunately necessary as C++ intentionally disallows function template
	// specialization and there's no way to make template overloads reliably resolve correctly
	inline void readLuaValue(lua_State* L, f32Parameter& ret) { readNumericLuaValue(L, ret); }
	inline void readLuaValue(lua_State* L, f32& ret)          { readNumericLuaValue(L, ret); }
	inline void readLuaValue(lua_State* L, u16& ret)          { readNumericLuaValue(L, ret); }
	inline void readLuaValue(lua_State* L, u8& ret)           { readNumericLuaValue(L, ret); }

	inline void readLuaValue(lua_State* L, v3fParameter& ret)
	{
		if (lua_isnil(L, -1))
			return;

		if (lua_isnumber(L, -1)) { // shortcut for uniform vectors
			auto n = lua_tonumber(L, -1);
			ret = v3fParameter(n,n,n);
		} else {
			ret = (v3fParameter)check_v3f(L, -1);
		}
	}

	inline void readLuaValue(lua_State* L, v2fParameter& ret)
	{
		if (lua_isnil(L, -1))
			return;

		if (lua_isnumber(L, -1)) { // shortcut for uniform vectors
			auto n = lua_tonumber(L, -1);
			ret = v2fParameter(n,n);
		} else {
			ret = (v2fParameter)check_v2f(L, -1);
		}
	}

	inline void readLuaValue(lua_State* L, TweenStyle& ret)
	{
		if (lua_isnil(L, -1))
			return;

		static const EnumString opts[] = {
			{(int)TweenStyle::fwd,     "fwd"},
			{(int)TweenStyle::rev,     "rev"},
			{(int)TweenStyle::pulse,   "pulse"},
			{(int)TweenStyle::flicker, "flicker"},
			{0, nullptr},
		};

		luaL_checktype(L, -1, LUA_TSTRING);
		int v = (int)TweenStyle::fwd;
		if (!string_to_enum(opts, v, lua_tostring(L, -1))) {
			throw LuaError("tween style must be one of ('fwd', 'rev', 'pulse', 'flicker')");
		}
		ret = (TweenStyle)v;
	}

	inline void readLuaValue(lua_State* L, AttractorKind& ret)
	{
		if (lua_isnil(L, -1))
			return;

		static const EnumString opts[] = {
			{(int)AttractorKind::none,  "none"},
			{(int)AttractorKind::point, "point"},
			{(int)AttractorKind::line,  "line"},
			{(int)AttractorKind::plane, "plane"},
			{0, nullptr},
		};

		luaL_checktype(L, -1, LUA_TSTRING);
		int v = (int)AttractorKind::none;
		if (!string_to_enum(opts, v, lua_tostring(L, -1))) {
			throw LuaError("attractor kind must be one of ('none', 'point', 'line', 'plane')");
		}
		ret = (AttractorKind)v;
	}

	inline void readLuaValue(lua_State* L, BlendMode& ret)
	{
		if (lua_isnil(L, -1))
			return;

		static const EnumString opts[] = {
			{(int)BlendMode::alpha,  "alpha"},
			{(int)BlendMode::add,    "add"},
			{(int)BlendMode::sub,    "sub"},
			{(int)BlendMode::screen, "screen"},
			{(int)BlendMode::clip,   "clip"},
			{0, nullptr},
		};

		luaL_checktype(L, -1, LUA_TSTRING);
		int v = (int)BlendMode::alpha;
		if (!string_to_enum(opts, v, lua_tostring(L, -1))) {
			throw LuaError("blend mode must be one of ('alpha', 'clip', 'add', 'sub', 'screen')");
		}
		ret = (BlendMode)v;
	}

	template <typename T> void
	readLuaValue(lua_State* L, RangedParameter<T>& field)
	{
		if (lua_isnil(L,-1))
			return;
		if (!lua_istable(L,-1)) // is this is just a literal value?
			goto set_uniform;

		lua_getfield(L, -1, "min");
		// handle convenience syntax for non-range values
		if (lua_isnil(L,-1)) {
			lua_pop(L, 1);
			goto set_uniform;
		}
		readLuaValue(L,field.min);
		lua_pop(L, 1);

		lua_getfield(L, -1, "max");
		readLuaValue(L,field.max);
		lua_pop(L, 1);

		lua_getfield(L, -1, "bias");
		if (!lua_isnil(L,-1))
			readLuaValue(L,field.bias);
		lua_pop(L, 1);
		return;

		set_uniform:
		readLuaValue(L, field.min);
		readLuaValue(L, field.max);
	}

	template <typename T> void
	readLegacyValue(lua_State* L, const char* name, T& field) {}

	template <typename T> void
	readLegacyValue(lua_State* L, const char* name, RangedParameter<T>& field)
	{
		int tbl = lua_gettop(L);
		lua_pushliteral(L, "min");
		lua_pushstring(L, name);
		lua_concat(L, 2);
		lua_gettable(L, tbl);
		if (!lua_isnil(L, -1)) {
			readLuaValue(L, field.min);
		}
		lua_settop(L, tbl);

		lua_pushliteral(L, "max");
		lua_pushstring(L, name);
		lua_concat(L, 2);
		lua_gettable(L, tbl);
		if (!lua_isnil(L, -1)) {
			readLuaValue(L, field.max);
		}
		lua_settop(L, tbl);
	}

	template <typename T> void
	readTweenTable(lua_State* L, const char* name, TweenedParameter<T>& field)
	{
		int tbl = lua_gettop(L);

		lua_pushstring(L, name);
		lua_pushliteral(L, "_tween");
		lua_concat(L, 2);
		lua_gettable(L, tbl);
		if(lua_istable(L, -1)) {
			int tween = lua_gettop(L);
			// get the starting value
			lua_pushinteger(L, 1), lua_gettable(L, tween);
			readLuaValue(L, field.start);
			lua_pop(L, 1);

			// get the final value -- use len instead of 2 so that this
			// gracefully degrades if keyframe support is later added
			lua_pushinteger(L, (lua_Integer)lua_objlen(L, -1)), lua_gettable(L, tween);
			readLuaValue(L, field.end);
			lua_pop(L, 1);

			// get the effect settings
			lua_getfield(L, -1, "style");
			if (!lua_isnil(L,-1))
				readLuaValue(L, field.style);
			lua_pop(L, 1);

			lua_getfield(L, -1, "reps");
			if (!lua_isnil(L,-1))
				readLuaValue(L, field.reps);
			lua_pop(L, 1);

			lua_getfield(L, -1, "start");
			if (!lua_isnil(L,-1))
				readLuaValue(L, field.beginning);
			lua_pop(L, 1);

			goto done;
		} else {
			lua_pop(L,1);
		}
		// the table is not present; check for nonanimated values

		lua_getfield(L, tbl, name);
		if(!lua_isnil(L, -1)) {
			readLuaValue(L, field.start);
			lua_settop(L, tbl);
			goto set_uniform;
		} else {
			lua_pop(L,1);
		}

		// the goto did not trigger, so this table is not present either
		// check for pre-5.6.0 legacy values
		readLegacyValue(L, name, field.start);

		set_uniform:
		field.end = field.start;
		done:
		lua_settop(L, tbl); // clean up after ourselves
	}

	inline u16 readAttachmentID(lua_State* L, const char* name)
	{
		u16 id = 0;
		lua_getfield(L, -1, name);
		if (!lua_isnil(L, -1)) {
			ObjectRef *ref = ModApiBase::checkObject<ObjectRef>(L, -1);
			if (auto obj = ObjectRef::getobject(ref))
				id = obj->getId();
		}
		lua_pop(L, 1);
		return id;
	}

	void readTexValue(lua_State* L, ServerParticleTexture& tex);
}
