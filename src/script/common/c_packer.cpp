// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2022 sfan5 <sfan5@live.de>

#include <cstdio>
#include <cstring>
#include <cmath>
#include <cassert>
#include <unordered_set>
#include <unordered_map>
#include "c_packer.h"
#include "c_internal.h"
#include "log.h"
#include "debug.h"
#include "threading/mutex_auto_lock.h"
#include "common/c_types.h" // LuaError

extern "C" {
#include <lauxlib.h>
}

//
// Helpers
//

// convert negative index to absolute position on Lua stack
static inline int absidx(lua_State *L, int idx)
{
	assert(idx < 0);
	return lua_gettop(L) + idx + 1;
}

// does the type put anything into PackedInstr::sdata?
static inline bool uses_sdata(int type)
{
	switch (type) {
		case LUA_TSTRING:
		case LUA_TFUNCTION:
		case LUA_TUSERDATA:
			return true;
		default:
			return false;
	}
}

// does the type put anything into PackedInstr::<union>?
static inline bool uses_union(int type)
{
	switch (type) {
		case LUA_TNIL:
		case LUA_TSTRING:
		case LUA_TFUNCTION:
			return false;
		default:
			return true;
	}
}

// can set_into be used with these key / value types in principle?
static inline bool can_set_into(int ktype, int vtype)
{
	switch (ktype) {
		case LUA_TNUMBER:
			return !uses_union(vtype);
		case LUA_TSTRING:
			return !uses_sdata(vtype);
		default:
			return false;
	}
}

// is the actual key suitable for use with set_into?
static inline bool suitable_key(lua_State *L, int idx)
{
	if (lua_type(L, idx) == LUA_TSTRING) {
		// strings may not have a NULL byte (-> lua_setfield)
		size_t len;
		const char *str = lua_tolstring(L, idx, &len);
		return strlen(str) == len;
	} else {
		assert(lua_type(L, idx) == LUA_TNUMBER);
		// numbers must fit into an s32 and be integers (-> lua_rawseti)
		lua_Number n = lua_tonumber(L, idx);
		return std::floor(n) == n && n >= S32_MIN && n <= S32_MAX;
	}
}

/**
 * Push core.known_metatables to the stack if it exists.
 * @param L Lua state
 * @return true if core.known_metatables exists, false otherwise.
*/
static inline bool get_known_lua_metatables(lua_State *L)
{
	lua_getglobal(L, "core");
	if (!lua_istable(L, -1)) {
		lua_pop(L, 1);
		return false;
	}
	lua_getfield(L, -1, "known_metatables");
	if (lua_istable(L, -1)) {
		lua_remove(L, -2);
		return true;
	}
	lua_pop(L, 2);
	return false;
}

namespace {
	// checks if you left any values on the stack, for debugging
	class StackChecker {
		lua_State *L;
		int top;
	public:
		StackChecker(lua_State *L) : L(L), top(lua_gettop(L)) {}
		~StackChecker() {
			assert(lua_gettop(L) >= top);
			if (lua_gettop(L) > top) {
				rawstream << "Lua stack not cleaned up: "
					<< lua_gettop(L) << " != " << top
					<< " (false-positive if exception thrown)" << std::endl;
			}
		}
	};

	// Since an std::vector may reallocate, this is the only safe way to keep
	// a reference to a particular element.
	template <typename T>
	class VectorRef {
		std::vector<T> *vec;
		size_t idx;
		VectorRef(std::vector<T> *vec, size_t idx) : vec(vec), idx(idx) {}
	public:
		constexpr VectorRef() : vec(nullptr), idx(0) {}
		static VectorRef<T> front(std::vector<T> &vec) {
			return VectorRef(&vec, 0);
		}
		static VectorRef<T> back(std::vector<T> &vec) {
			return VectorRef(&vec, vec.size() - 1);
		}
		T &operator*() { return (*vec)[idx]; }
		T *operator->() { return &(*vec)[idx]; }
		operator bool() const { return vec != nullptr; }
	};

	struct Packer {
		PackInFunc fin;
		PackOutFunc fout;
	};

	typedef std::pair<std::string, Packer> PackerTuple;
}

/**
 * Append instruction to end.
 *
 * @param pv target
 * @param type instruction type
 * @return reference to instruction
*/
static inline auto emplace(PackedValue &pv, s16 type)
{
	pv.i.emplace_back();
	auto ref = VectorRef<PackedInstr>::back(pv.i);
	ref->type = type;
	// Initialize fields that may be left untouched
	if (type == LUA_TTABLE) {
		ref->uidata1 = 0;
		ref->uidata2 = 0;
	} else if (type == LUA_TUSERDATA) {
		ref->ptrdata = nullptr;
	} else if (type == INSTR_POP) {
		ref->sidata2 = 0;
	}
	return ref;
}

//
// Management of registered packers
//

static std::unordered_map<std::string, Packer> g_packers;
static std::mutex g_packers_lock;

void script_register_packer(lua_State *L, const char *regname,
	PackInFunc fin, PackOutFunc fout)
{
	// Store away callbacks
	{
		MutexAutoLock autolock(g_packers_lock);
		auto it = g_packers.find(regname);
		if (it == g_packers.end()) {
			auto &ref = g_packers[regname];
			ref.fin = fin;
			ref.fout = fout;
		} else {
			FATAL_ERROR_IF(it->second.fin != fin || it->second.fout != fout,
				"Packer registered twice with mismatching callbacks");
		}
	}

	// Save metatable so we can identify instances later
	lua_rawgeti(L, LUA_REGISTRYINDEX, CUSTOM_RIDX_METATABLE_MAP);
	if (lua_isnil(L, -1)) {
		lua_newtable(L);
		lua_pushvalue(L, -1);
		lua_rawseti(L, LUA_REGISTRYINDEX, CUSTOM_RIDX_METATABLE_MAP);
	}

	luaL_getmetatable(L, regname);
	FATAL_ERROR_IF(lua_isnil(L, -1), "No metatable registered with that name");

	// CUSTOM_RIDX_METATABLE_MAP contains { [metatable] = "regname", ... }
	// check first
	lua_pushstring(L, regname);
	lua_rawget(L, -3);
	if (!lua_isnil(L, -1)) {
		FATAL_ERROR_IF(lua_topointer(L, -1) != lua_topointer(L, -2),
				"Packer registered twice with inconsistent metatable");
	}
	lua_pop(L, 1);
	// then set
	lua_pushstring(L, regname);
	lua_rawset(L, -3);

	lua_pop(L, 1);
}

/**
 * Find a packer for a metatable.
 *
 * @param regname metatable name
 * @param out packer will be placed here
 * @return success
*/
static bool find_packer(const char *regname, PackerTuple &out)
{
	MutexAutoLock autolock(g_packers_lock);
	auto it = g_packers.find(regname);
	if (it == g_packers.end())
		return false;
	// copy data for thread safety
	out.first = it->first;
	out.second = it->second;
	return true;
}

/**
 * Find a packer matching the metatable of the Lua value.
 *
 * @param L Lua state
 * @param idx Index on stack
 * @param out packer will be placed here
 * @return success
*/
static bool find_packer(lua_State *L, int idx, PackerTuple &out)
{
#ifndef NDEBUG
	StackChecker checker(L);
#endif

	// retrieve metatable of the object
	if (lua_getmetatable(L, idx) != 1)
		return false;

	// use our global table to map it to the registry name
	lua_rawgeti(L, LUA_REGISTRYINDEX, CUSTOM_RIDX_METATABLE_MAP);
	assert(lua_istable(L, -1));
	lua_pushvalue(L, -2);
	lua_rawget(L, -2);
	if (lua_isnil(L, -1)) {
		lua_pop(L, 3);
		return false;
	}

	// load the associated data
	bool found = find_packer(lua_tostring(L, -1), out);
	FATAL_ERROR_IF(!found, "Inconsistent internal state");
	lua_pop(L, 3);
	return true;
}

//
// Packing implementation
//

/**
 * Keeps track of seen objects, which is needed to make circular references work.
 * The first time an object is seen it remembers the instruction index.
 * The caller is expected to add instructions that produce the value immediately after.
 * For second, third, ... calls it pushes an instruction that references the already
 * created value.
 *
 * @param L Lua state
 * @param idx Index of value on Lua stack
 * @param pv target
 * @param seen Map of seen objects
 * @return empty reference (first time) or reference to instruction that
 *         reproduces the value (otherwise)
 *
*/
static VectorRef<PackedInstr> record_object(lua_State *L, int idx, PackedValue &pv,
		std::unordered_map<const void *, s32> &seen)
{
	const void *ptr = lua_topointer(L, idx);
	assert(ptr);
	auto found = seen.find(ptr);
	if (found == seen.end()) {
		// first time, record index
		assert(pv.i.size() <= S32_MAX);
		seen[ptr] = pv.i.size();
		return VectorRef<PackedInstr>();
	}

	s32 ref = found->second;
	assert(ref < (s32)pv.i.size());
	// reuse the value from first time
	auto r = emplace(pv, INSTR_PUSHREF);
	r->sidata1 = ref;
	pv.i[ref].keep_ref = true;
	return r;
}

/**
 * Pack a single Lua value and add it to the instruction stream.
 *
 * @param L Lua state
 * @param idx Index of value on Lua stack. Must be positive, use absidx if not!
 * @param vidx Next free index on the stack as it would look during unpacking. (v = virtual)
 * @param pv target
 * @param seen Map of seen objects (see record_object)
 * @return reference to the instruction that creates the value
*/
static VectorRef<PackedInstr> pack_inner(lua_State *L, int idx, int vidx, PackedValue &pv,
		std::unordered_map<const void *, s32> &seen)
{
#ifndef NDEBUG
	StackChecker checker(L);
	assert(idx > 0);
	assert(vidx > 0);
#endif

	switch (lua_type(L, idx)) {
		case LUA_TNONE:
		case LUA_TNIL:
			return emplace(pv, LUA_TNIL);
		case LUA_TBOOLEAN: {
			auto r = emplace(pv, LUA_TBOOLEAN);
			r->bdata = lua_toboolean(L, idx);
			return r;
		}
		case LUA_TNUMBER: {
			auto r = emplace(pv, LUA_TNUMBER);
			r->ndata = lua_tonumber(L, idx);
			return r;
		}
		case LUA_TSTRING: {
			auto r = emplace(pv, LUA_TSTRING);
			size_t len;
			const char *str = lua_tolstring(L, idx, &len);
			assert(str);
			r->sdata.assign(str, len);
			return r;
		}
		case LUA_TTABLE: {
			auto r = record_object(L, idx, pv, seen);
			if (r)
				return r;
			break; // execution continues
		}
		case LUA_TFUNCTION: {
			auto r = record_object(L, idx, pv, seen);
			if (r)
				return r;
			r = emplace(pv, LUA_TFUNCTION);
			call_string_dump(L, idx);
			size_t len;
			const char *str = lua_tolstring(L, -1, &len);
			assert(str);
			r->sdata.assign(str, len);
			lua_pop(L, 1);
			return r;
		}
		case LUA_TUSERDATA: {
			auto r = record_object(L, idx, pv, seen);
			if (r)
				return r;
			PackerTuple ser;
			if (!find_packer(L, idx, ser))
				throw LuaError("Cannot serialize unsupported userdata");
			// use packer callback to turn into a void*
			pv.contains_userdata = true;
			r = emplace(pv, LUA_TUSERDATA);
			r->sdata = ser.first;
			r->ptrdata = ser.second.fin(L, idx);
			return r;
		}
		default: {
			std::string err = "Cannot serialize type ";
			err += lua_typename(L, lua_type(L, idx));
			throw LuaError(err);
		}
	}

	// LUA_TTABLE
	lua_checkstack(L, 5);

	auto rtable = emplace(pv, LUA_TTABLE);
	const int vi_table = vidx++;

	lua_pushnil(L);
	while (lua_next(L, idx) != 0) {
		// key at -2, value at -1
		const int ktype = lua_type(L, -2), vtype = lua_type(L, -1);
		if (ktype == LUA_TNUMBER)
			rtable->uidata1++; // narr
		else
			rtable->uidata2++; // nrec

		// set_into is a shortcut that allows a pushed value
		// to be directly set into a table without separately pushing
		// the key and using SETTABLE.
		// only works in certain circumstances, hence the check:
		if (can_set_into(ktype, vtype) && suitable_key(L, -2)) {
			// push only the value
			auto rval = pack_inner(L, absidx(L, -1), vidx, pv, seen);
			vidx++;
			rval->pop = rval->type != LUA_TTABLE;
			// where to put it:
			rval->set_into = vi_table;
			if (ktype == LUA_TSTRING)
				rval->sdata = lua_tostring(L, -2);
			else
				rval->sidata1 = lua_tointeger(L, -2);
			// since tables take multiple instructions to populate we have to
			// pop them separately afterwards.
			if (!rval->pop) {
				auto ri1 = emplace(pv, INSTR_POP);
				ri1->sidata1 = vidx - 1;
			}
			vidx--;
		} else {
			// push the key and value
			pack_inner(L, absidx(L, -2), vidx, pv, seen);
			vidx++;
			pack_inner(L, absidx(L, -1), vidx, pv, seen);
			vidx++;
			// push an instruction to set them
			auto ri1 = emplace(pv, INSTR_SETTABLE);
			ri1->set_into = vi_table;
			ri1->sidata1 = vidx - 2;
			ri1->sidata2 = vidx - 1;
			ri1->pop = true;
			vidx -= 2;
		}

		lua_pop(L, 1);
	}

	// try to preserve metatable information
	if (lua_getmetatable(L, idx) && get_known_lua_metatables(L)) {
		lua_insert(L, -2);
		lua_gettable(L, -2);
		if (lua_isstring(L, -1)) {
			auto r = emplace(pv, INSTR_SETMETATABLE);
			r->sdata = std::string(lua_tostring(L, -1));
			r->set_into = vi_table;
		}
		lua_pop(L, 2);
	}

	// exactly the table should be left on stack
	assert(vidx == vi_table + 1);
	return rtable;
}

PackedValue *script_pack(lua_State *L, int idx)
{
	if (idx < 0)
		idx = absidx(L, idx);

	PackedValue pv;
	std::unordered_map<const void *, s32> seen;
	pack_inner(L, idx, 1, pv, seen);

	// allocate last for exception safety
	return new PackedValue(std::move(pv));
}

//
// Unpacking implementation
//

void script_unpack(lua_State *L, PackedValue *pv)
{
	assert(pv);
	// table that tracks objects for keep_ref / PUSHREF (key = instr index)
	lua_newtable(L);
	const int top = lua_gettop(L);
	int ctr = 0;

	for (size_t packed_idx = 0; packed_idx < pv->i.size(); packed_idx++) {
		auto &i = pv->i[packed_idx];

		// Make sure there's space on the stack (if applicable)
		if (!i.pop && (ctr++) >= 5) {
			lua_checkstack(L, 5);
			ctr = 0;
		}

		switch (i.type) {
			/* Instructions */
			case INSTR_SETTABLE:
				lua_pushvalue(L, top + i.sidata1); // key
				lua_pushvalue(L, top + i.sidata2); // value
				lua_rawset(L, top + i.set_into);
				if (i.pop) {
					if (i.sidata1 != i.sidata2) {
						// removing moves indices so pop higher index first
						lua_remove(L, top + std::max(i.sidata1, i.sidata2));
						lua_remove(L, top + std::min(i.sidata1, i.sidata2));
					} else {
						lua_remove(L, top + i.sidata1);
					}
				}
				continue;
			case INSTR_POP:
				lua_remove(L, top + i.sidata1);
				if (i.sidata2 > 0)
					lua_remove(L, top + i.sidata2);
				continue;
			case INSTR_PUSHREF:
				// retrieve from top table
				lua_pushinteger(L, i.sidata1);
				lua_rawget(L, top);
				break;
			case INSTR_SETMETATABLE:
				if (get_known_lua_metatables(L)) {
					lua_getfield(L, -1, i.sdata.c_str());
					lua_remove(L, -2);
					if (lua_istable(L, -1))
						lua_setmetatable(L, top + i.set_into);
					else
						lua_pop(L, 1);
				}
				continue;

			/* Lua types */
			case LUA_TNIL:
				lua_pushnil(L);
				break;
			case LUA_TBOOLEAN:
				lua_pushboolean(L, i.bdata);
				break;
			case LUA_TNUMBER:
				lua_pushnumber(L, i.ndata);
				break;
			case LUA_TSTRING:
				lua_pushlstring(L, i.sdata.data(), i.sdata.size());
				break;
			case LUA_TTABLE:
				lua_createtable(L, i.uidata1, i.uidata2);
				break;
			case LUA_TFUNCTION:
				luaL_loadbuffer(L, i.sdata.data(), i.sdata.size(), nullptr);
				break;
			case LUA_TUSERDATA: {
				PackerTuple ser;
				sanity_check(find_packer(i.sdata.c_str(), ser));
				ser.second.fout(L, i.ptrdata);
				i.ptrdata = nullptr; // ownership taken by packer callback
				break;
			}

			default:
				assert(0);
				break;
		}

		if (i.keep_ref) {
			// remember in top table
			lua_pushinteger(L, packed_idx);
			lua_pushvalue(L, -2);
			lua_rawset(L, top);
		}

		if (i.set_into) {
			if (!i.pop) // set will consume
				lua_pushvalue(L, -1);
			if (uses_sdata(i.type))
				lua_rawseti(L, top + i.set_into, i.sidata1);
			else
				lua_setfield(L, top + i.set_into, i.sdata.c_str());
		} else {
			if (i.pop)
				lua_pop(L, 1);
		}
	}

	// as part of the unpacking process all userdata is "used up"
	pv->contains_userdata = false;
	// leave exactly one value on the stack
	lua_settop(L, top+1);
	lua_remove(L, top);
}

//
// PackedValue
//

PackedValue::~PackedValue()
{
	if (!contains_userdata)
		return;
	for (auto &i : this->i) {
		if (i.type == LUA_TUSERDATA && i.ptrdata) {
			PackerTuple ser;
			if (find_packer(i.sdata.c_str(), ser)) {
				// tell packer to deallocate object
				ser.second.fout(nullptr, i.ptrdata);
			} else {
				assert(false);
			}
		}
	}
}

//
// script_dump_packed
//

void script_dump_packed(const PackedValue *val)
{
	printf("instruction stream: [\n");
	for (const auto &i : val->i) {
		printf("\t(");
		switch (i.type) {
			case INSTR_SETTABLE:
				printf("SETTABLE(%d, %d)", i.sidata1, i.sidata2);
				break;
			case INSTR_POP:
				printf(i.sidata2 ? "POP(%d, %d)" : "POP(%d)", i.sidata1, i.sidata2);
				break;
			case INSTR_PUSHREF:
				printf("PUSHREF(%d)", i.sidata1);
				break;
			case INSTR_SETMETATABLE:
				printf("SETMETATABLE(%s)", i.sdata.c_str());
				break;
			case LUA_TNIL:
				printf("nil");
				break;
			case LUA_TBOOLEAN:
				printf(i.bdata ? "true" : "false");
				break;
			case LUA_TNUMBER:
				printf("%f", i.ndata);
				break;
			case LUA_TSTRING:
				printf("\"%s\"", i.sdata.c_str());
				break;
			case LUA_TTABLE:
				printf("table(%d, %d)", i.uidata1, i.uidata2);
				break;
			case LUA_TFUNCTION:
				printf("function(%d bytes)", (int)i.sdata.size());
				break;
			case LUA_TUSERDATA:
				printf("userdata %s %p", i.sdata.c_str(), i.ptrdata);
				break;
			default:
				FATAL_ERROR("unknown type");
				break;
		}
		if (i.set_into) {
			if (i.type >= 0 && uses_sdata(i.type))
				printf(", k=%d, into=%d", i.sidata1, i.set_into);
			else if (i.type >= 0)
				printf(", k=\"%s\", into=%d", i.sdata.c_str(), i.set_into);
			else
				printf(", into=%d", i.set_into);
		}
		if (i.keep_ref)
			printf(", keep_ref");
		if (i.pop)
			printf(", pop");
		printf(")\n");
	}
	printf("]\n");
}
